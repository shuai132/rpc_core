#include <limits>

#include "rpc_core.hpp"
#include "assert_def.h"

using namespace rpc_core;

static void test_response_reentry(int arity) {
  auto conn = std::make_shared<connection>();
  std::vector<std::string> sent;
  conn->send_package_impl = [&](std::string data) { sent.push_back(std::move(data)); };
  auto r = rpc::create(conn);
  r->set_ready(true);
  int responses = 0, first_finished = 0, second_finished = 0;
  auto req = r->cmd("x");
  request_w observer = req;
  auto received = [&] {
    if (++responses == 1) {
      req->finally([&](finally_t type) { ASSERT(type == finally_t::normal); ++second_finished; });
      req->call();
    }
  };
  if (arity == 0) req->rsp([&] { received(); });
  if (arity == 1) req->rsp([&](std::string data) { ASSERT(data == "ok"); received(); });
  if (arity == 2) req->rsp([&](std::string data, finally_t type) {
    ASSERT(data == "ok" && type == finally_t::normal); received();
  });
  if (arity == 3) req->rsp([&](int data, finally_t type) {
    ASSERT(type == (responses == 0 ? finally_t::rsp_serialize_error : finally_t::normal));
    if (responses != 0) ASSERT(data == 42);
    received();
  });
  req->finally([&](finally_t type) {
    ASSERT(type == (arity == 3 ? finally_t::rsp_serialize_error : finally_t::normal));
    ++first_finished;
  });
  req->call();
  auto reply = [&](size_t index) {
    auto msg = detail::coder::deserialize(sent[index]);
    ASSERT(msg.first);
    std::string data = "ok";
    auto response = detail::msg_wrapper::make_rsp(msg.second.seq, &data);
    if (arity == 3) response.second.data = index == 0 ? std::string{} : serialize(42);
    conn->on_recv_package(detail::coder::serialize(response.second).second);
  };
  reply(0);
  ASSERT(sent.size() == 2 && first_finished == 1 && second_finished == 0);
  req.reset();
  ASSERT(!observer.expired());
  reply(1);
  ASSERT(responses == 2 && second_finished == 1);
  ASSERT(observer.expired());
}

static void test_old_rpc_destruction_does_not_finish_new_call() {
  auto old_conn = std::make_shared<connection>();
  old_conn->send_package_impl = [](std::string) {};
  auto old_rpc = rpc::create(old_conn);
  auto new_conn = std::make_shared<connection>();
  std::string sent;
  new_conn->send_package_impl = [&](std::string data) { sent = std::move(data); };
  auto new_rpc = rpc::create(new_conn);
  old_rpc->set_ready(true);
  new_rpc->set_ready(true);
  int responses = 0, finished = 0, canceled = 0;
  auto req = old_rpc->cmd("pending")->rsp([&](std::string data) {
    ASSERT(data == "ok");
    ++responses;
  })->finally([&](finally_t type) {
    if (type == finally_t::canceled) { ++canceled; return; }
    ASSERT(type == finally_t::normal);
    ++finished;
  });
  request_w observer = req;
  req->call();
  ASSERT(req->call(new_rpc).type == finally_t::busy);
  req->cancel()->reset_cancel();
  ASSERT(req->call(new_rpc));
  req.reset();
  old_rpc.reset();
  ASSERT(canceled == 1 && finished == 0 && !observer.expired());
  auto command = detail::coder::deserialize(sent);
  ASSERT(command.first);
  std::string data = "ok";
  auto response = detail::msg_wrapper::make_rsp(command.second.seq, &data);
  new_conn->on_recv_package(detail::coder::serialize(response.second).second);
  ASSERT(responses == 1 && finished == 1 && observer.expired());
}

static void test_dispose_reentry(bool destroy) {
  auto r = rpc::create();
  r->get_connection()->send_package_impl = [](std::string) { return true; };
  r->set_ready(true);
  r->set_timer([](uint32_t, rpc::timeout_cb) {});
  auto group = dispose::create();
  auto scope = group.get();
  std::vector<request_s> requests;
  int finished = 0;
  for (int i = 0; i < 3; ++i) {
    auto req = r->cmd("pending")->mark_need_rsp();
    request_w weak = req;
    req->finally([&, weak](finally_t type) {
      ASSERT(type == finally_t::canceled);
      ++finished;
      scope->remove(weak.lock());
      scope->dismiss();
    });
    req->add_to(*scope)->call();
    requests.push_back(req);
  }
  if (destroy) group.reset();
  else group->dismiss();
  ASSERT(finished == 3);
  for (const auto& req : requests) ASSERT(req->is_canceled());
}

static void test_dispose_keeps_new_requests() {
  auto r = rpc::create();
  r->get_connection()->send_package_impl = [](std::string) { return true; };
  r->set_ready(true);
  r->set_timer([](uint32_t, rpc::timeout_cb) {});
  dispose group;
  auto next = r->cmd("next")->mark_need_rsp();
  auto first = r->cmd("first")->mark_need_rsp()->finally([&] {
    next->add_to(group)->call();
  });
  first->add_to(group)->call();
  group.dismiss();
  ASSERT(first->is_canceled() && !next->is_canceled());
  group.dismiss();
  ASSERT(next->is_canceled());
}

int main() {
  test_dispose_reentry(false);
  test_dispose_reentry(true);
  test_dispose_keeps_new_requests();
  // Waiting for a response must register completion even without a user handler.
  {
    auto r = rpc::create();
    r->get_connection()->send_package_impl = [](std::string) { return true; };
    r->set_ready(true);
    rpc::timeout_cb timer;
    r->set_timer([&](uint32_t, rpc::timeout_cb cb) { timer = std::move(cb); });
    int finished = 0;
    auto req = r->cmd("x")->enable_rsp()->finally([&](finally_t type) {
      ASSERT(type == finally_t::timeout);
      ++finished;
    });
    request_w observer = req;
    req->call();
    req.reset();
    ASSERT(timer);
    timer();
    ASSERT(finished == 1 && observer.expired());
  }
  test_old_rpc_destruction_does_not_finish_new_call();
  for (int arity : {0, 1, 2, 3}) test_response_reentry(arity);
  // Coder failure becomes a request completion without sending or registering a timeout.
  for (bool need_rsp : {false, true}) {
    auto conn = std::make_shared<connection>();
    int sent = 0, timers = 0, finished = 0;
    conn->send_package_impl = [&](std::string) { ++sent; };
    auto r = rpc::create(conn);
    r->set_ready(true);
    r->set_timer([&](uint32_t, rpc::timeout_cb) { ++timers; });
    auto req = r->cmd(std::string(static_cast<size_t>((std::numeric_limits<uint16_t>::max)()) + 1, 'x'));
    if (need_rsp) req->rsp([](std::string) { ASSERT(false); });
    req->finally([&](finally_t type) {
      ASSERT(type == finally_t::req_serialize_error);
      ++finished;
    });
    std::weak_ptr<request> observer = req;
    req->call();
    ASSERT(sent == 0 && timers == 0 && finished == 1);
    req.reset();
    ASSERT(observer.expired());
    r.reset();
    ASSERT(finished == 1);
  }
  // Destruction completes requests even when no timer is installed.
  for (bool with_timer : {false, true}) {
    auto r = rpc::create();
    r->set_ready(true);
    rpc::timeout_cb timer;
    if (with_timer) r->set_timer([&](uint32_t, rpc::timeout_cb cb) { timer = std::move(cb); });
    int finished = 0;
    auto req = r->cmd("pending")->rsp([](std::string) {})->finally([&](finally_t type) {
      ASSERT(type == finally_t::rpc_expired);
      ++finished;
    });
    std::weak_ptr<request> observer = req;
    req->call();
    req.reset();
    r.reset();
    ASSERT(observer.expired());
    ASSERT(finished == 1);
    if (timer) timer();
    ASSERT(finished == 1);
  }

  for (int retry : {0, 1, -1}) {
    auto r = rpc::create();
    r->set_ready(true);
    rpc::timeout_cb timer;
    r->set_timer([&](uint32_t, rpc::timeout_cb cb) { timer = std::move(cb); });
    int finished = 0;
    auto req = r->cmd("pending")->rsp([](std::string) {})->retry(retry);
    req->timeout([&] { req->canceled(true); });
    req->finally([&](finally_t type) { ASSERT(type == finally_t::canceled); ++finished; });
    req->call();
    auto fire = std::move(timer);
    fire();
    ASSERT(finished == 1);
    ASSERT(!timer);
    req->reset_cancel()->call();
    ASSERT(timer);
    req->cancel();
    ASSERT(finished == 2);
  }

  {
    auto r = rpc::create();
    r->set_ready(true);
    rpc::timeout_cb timer;
    r->set_timer([&](uint32_t, rpc::timeout_cb cb) { timer = std::move(cb); });
    int finished = 0;
    auto req = r->cmd("pending")->rsp([](std::string) {});
    std::weak_ptr<request> observer = req;
    req->finally([&](finally_t type) {
      ASSERT(type == finally_t::timeout);
      if (++finished == 1) req->call();
    });
    req->call();
    auto first = std::move(timer);
    first();
    req.reset();
    ASSERT(!observer.expired());
    timer();
    ASSERT(finished == 2);
    ASSERT(observer.expired());
  }

  {
    auto r = rpc::create();
    int count = 0;
    auto req = r->cmd("x")->finally([state = 0, &count](finally_t) mutable { count = ++state; });
    req->call();
    req->call();
    ASSERT(count == 2);
  }
  {
    auto pair = loopback_connection::create();
    pair.second.reset();
    pair.first->send_package_impl("gone");
  }

  {
    auto r = rpc::create();
    r->set_ready(true);
    auto req = r->cmd("x")->rsp([](std::string) {});
    std::weak_ptr<request> observer = req;
    req->finally([&] { req.reset(); });
    req->call();
    auto canceled = req->cancel();
    ASSERT(!req && canceled->is_canceled());
    canceled.reset();
    ASSERT(observer.expired());
  }

  // Failure must propagate regardless of where a stream read is split.
  for (size_t split = 0; split < 9; ++split) {
    detail::data_packer p(8);
    int frames = 0;
    p.on_data = [&](std::string data) { ASSERT(data == "a"); ++frames; };
    auto data = p.pack("a");
    uint32_t oversized = 9;
    data.append(reinterpret_cast<const char*>(&oversized), sizeof(oversized));
    ASSERT(p.feed(data.data(), split));
    ASSERT(!p.feed(data.data() + split, data.size() - split));
    ASSERT(frames == 1);
  }
}
