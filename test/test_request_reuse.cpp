#include <chrono>
#include <vector>

#include "rpc_core.hpp"
#include "assert_def.h"

using namespace rpc_core;

struct fixture {
  std::shared_ptr<connection> conn = std::make_shared<connection>();
  rpc_s r = rpc::create(conn);
  std::vector<std::string> sent;
  std::vector<rpc::timeout_cb> timers;

  fixture() {
    conn->send_package_impl = [&](std::string packet) { sent.push_back(std::move(packet)); return true; };
    r->set_ready(true);
    r->set_timer([&](uint32_t, rpc::timeout_cb cb) { timers.push_back(std::move(cb)); });
  }

  void reply(size_t index, std::string data) {
    auto command = detail::coder::deserialize(sent[index]);
    ASSERT(command.first);
    auto response = detail::msg_wrapper::make_rsp(command.second.seq, &data);
    conn->on_recv_package(detail::coder::serialize(response).second);
  }

  void expire(size_t index) {
    auto callback = timers[index];
    callback();
  }
};

static void test_busy_keeps_original_call() {
  fixture f, other;
  int replies = 0, finished = 0, next_finished = 0;
  auto req = f.r->cmd("original")->msg(std::string("first"))
      ->rsp([&](std::string data) { ASSERT(data == "ok"); ++replies; })
      ->finally([&](finally_t t) { ASSERT(t == finally_t::normal); ++finished; });
  ASSERT(req->call());
  req->finally([&](finally_t t) { ASSERT(t == finally_t::normal); ++next_finished; });
  ASSERT(req->call().type == finally_t::busy);
  ASSERT(req->call(other.r).type == finally_t::busy);
  ASSERT(req->rpc().lock() == f.r);
  ASSERT(f.sent.size() == 1 && f.timers.size() == 1 && other.sent.empty());
  ASSERT(replies == 0 && finished == 0 && next_finished == 0);
  f.reply(0, "ok");
  ASSERT(replies == 1 && finished == 1 && next_finished == 0);
  ASSERT(req->call());
  f.reply(1, "ok");
  ASSERT(replies == 2 && finished == 1 && next_finished == 1);
}

static void test_cancel_and_stale_callbacks() {
  fixture f, other;
  int replies = 0;
  std::vector<finally_t> finished;
  auto req = f.r->cmd("x")->rsp([&](std::string data) { ASSERT(data == "new"); ++replies; })
      ->finally([&](finally_t t) { finished.push_back(t); });
  ASSERT(req->call());
  // Changing next-call settings must not change how the active call is canceled.
  req->disable_rsp()->rpc(other.r);
  req->cancel();
  ASSERT(finished == std::vector<finally_t>{finally_t::canceled});
  req->reset_cancel()->enable_rsp();
  ASSERT(req->call(f.r));
  f.expire(0);
  f.reply(0, "old");
  ASSERT(replies == 0 && finished.size() == 1);
  f.reply(1, "new");
  ASSERT(replies == 1 && finished.size() == 2 && finished.back() == finally_t::normal);
  f.expire(1);
  ASSERT(finished.size() == 2);
}

static void test_retries_keep_call_and_configuration() {
  fixture f, other;
  std::vector<finally_t> finished;
  int next_finished = 0, timeouts = 0;
  auto req = f.r->cmd("original")->msg(std::string("payload"))->rsp([](std::string) {})
      ->retry(2)->finally([&](finally_t t) { finished.push_back(t); });
  req->timeout([&] {
    ++timeouts;
    ASSERT(req->call().type == finally_t::busy);
  });
  ASSERT(req->call());
  req->cmd("next")->msg(std::string("changed"))->disable_rsp()->rpc(other.r)
      ->finally([&](finally_t) { ++next_finished; });
  f.expire(0);
  f.expire(0);  // The previous retry's timer is stale.
  ASSERT(f.sent.size() == 2 && finished.empty());
  f.reply(0, "late response");
  ASSERT(finished.empty());
  f.expire(1);
  ASSERT(f.sent.size() == 3 && finished.empty());
  f.expire(2);
  ASSERT(finished == std::vector<finally_t>{finally_t::timeout});
  ASSERT(timeouts == 3 && next_finished == 0 && other.sent.empty());
  for (const auto& packet : f.sent) {
    auto msg = detail::coder::deserialize(packet);
    ASSERT(msg.first && msg.second.cmd == "original" && msg.second.data == "payload");
  }
  // A new logical call starts with the full configured retry budget.
  req->cmd("original")->msg(std::string("payload"))->enable_rsp()->rpc(f.r)
      ->finally([&](finally_t t) { finished.push_back(t); });
  ASSERT(req->call());
  f.expire(3);
  f.expire(4);
  ASSERT(f.sent.size() == 6 && finished.size() == 1);
  f.expire(5);
  ASSERT(finished.size() == 2 && finished.back() == finally_t::timeout);
}

static void test_timeout_callback_can_cancel_and_reuse() {
  fixture f;
  std::vector<finally_t> finished;
  auto req = f.r->cmd("x")->rsp([](std::string data) { ASSERT(data == "new"); })->retry(-1)
      ->finally([&](finally_t t) { finished.push_back(t); });
  req->timeout([&] {
    req->cancel()->reset_cancel()->timeout(nullptr);
    ASSERT(req->call());
  });
  ASSERT(req->call());
  f.expire(0);
  ASSERT(f.sent.size() == 2 && finished == std::vector<finally_t>{finally_t::canceled});
  f.reply(0, "old");
  f.expire(0);
  ASSERT(finished.size() == 1 && f.sent.size() == 2);
  f.reply(1, "new");
  ASSERT(finished.size() == 2 && finished.back() == finally_t::normal);
}

#ifdef RPC_CORE_FEATURE_FUTURE
static void test_future_busy_and_reuse() {
  fixture f;
  auto req = f.r->cmd("x");
  auto first = req->future<std::string>();
  auto busy = req->future<std::string>();
  auto busy_void = req->future<>();
  ASSERT(busy.wait_for(std::chrono::seconds(0)) == std::future_status::ready);
  ASSERT(busy.get().type == finally_t::busy && busy_void.get().type == finally_t::busy);
  ASSERT(f.sent.size() == 1 && first.wait_for(std::chrono::seconds(0)) == std::future_status::timeout);
  f.reply(0, "first");
  auto response = first.get();
  ASSERT(response && response.data == "first");
  auto second = req->future<std::string>();
  f.expire(0);
  f.reply(0, "old");
  ASSERT(second.wait_for(std::chrono::seconds(0)) == std::future_status::timeout);
  f.reply(1, "second");
  response = second.get();
  ASSERT(response && response.data == "second");
  // Reusing an adapter-configured request must not fulfill its old promise again.
  ASSERT(req->call());
  f.reply(2, "third");
}
#endif

static void test_reused_callbacks_keep_mutable_state() {
  for (int arity = 0; arity < 3; ++arity) {
    fixture f;
    std::vector<int> counts;
    auto req = f.r->cmd("x");
    if (arity == 0) req->rsp([&, count = 0]() mutable { counts.push_back(++count); });
    if (arity == 1) req->rsp([&, count = 0](std::string) mutable { counts.push_back(++count); });
    if (arity == 2) req->rsp([&, count = 0](std::string, finally_t) mutable { counts.push_back(++count); });
    for (int i = 0; i < 3; ++i) {
      ASSERT(req->call());
      f.reply(i, "ok");
    }
    ASSERT((counts == std::vector<int>{1, 2, 3}));
    ASSERT(req->call());
    // A replacement configures the next call without replacing the active callback.
    req->rsp([&, count = 100](std::string) mutable { counts.push_back(++count); });
    f.reply(3, "old");
    ASSERT(req->call());
    f.reply(4, "new");
    ASSERT((counts == std::vector<int>{1, 2, 3, 4, 101}));
  }
  fixture f;
  std::vector<int> counts;
  auto req = f.r->cmd("timeout")->mark_need_rsp()->retry(1)
      ->timeout([&, count = 0]() mutable { counts.push_back(++count); });
  for (int i = 0; i < 2; ++i) {
    ASSERT(req->call());
    f.expire(i * 2);
    f.expire(i * 2 + 1);
  }
  ASSERT((counts == std::vector<int>{1, 2, 3, 4}));
}

int main() {
  test_reused_callbacks_keep_mutable_state();
  test_busy_keeps_original_call();
  test_cancel_and_stale_callbacks();
  test_retries_keep_call_and_configuration();
  test_timeout_callback_can_cancel_and_reuse();
#ifdef RPC_CORE_FEATURE_FUTURE
  test_future_busy_and_reuse();
#endif
}
