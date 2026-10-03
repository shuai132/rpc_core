#include <functional>
#include <stdexcept>
#include <vector>
#include "rpc_core.hpp"
#include "assert_def.h"

using namespace rpc_core;
using deferred = request_response<std::string, std::string>;

struct custom_reply {
  std::string value;
  std::function<void()> on_serialize;
  void operator>>(serialize_oarchive& ar) const {
    if (on_serialize) on_serialize();
    value >> ar;
  }
};

struct reply_fixture {
  std::shared_ptr<connection> conn = std::make_shared<connection>();
  rpc_s server = rpc::create(conn);
  request_response<std::string, custom_reply> pending;
  std::vector<std::string> sent;

  reply_fixture() {
    server->set_ready(true);
    server->subscribe("deferred", [&](request_response<std::string, custom_reply> rr) { pending = std::move(rr); });
    conn->send_package_impl = [&](std::string data) { sent.push_back(std::move(data)); return true; };
    detail::msg_wrapper command;
    command.cmd = "deferred";
    command.type = static_cast<detail::msg_wrapper::msg_type>(detail::msg_wrapper::command | detail::msg_wrapper::need_rsp);
    command.data = "request";
    conn->on_recv_package(detail::coder::serialize(command).second);
    ASSERT(pending);
  }
};

static void test_deferred_reply_blocks_serialization_reentry() {
  reply_fixture f;
  auto copy = f.pending->rsp;
  ASSERT(f.pending->rsp({"outer", [&] {
    ASSERT(copy({"nested", {}}).type == finally_t::busy);
  }}));
  ASSERT(f.pending->rsp_ready && f.sent.size() == 1);
  ASSERT(detail::coder::deserialize(f.sent.front()).second.data == "outer");
  ASSERT(copy({"duplicate", {}}).type == finally_t::busy);
}

#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
static void test_deferred_reply_can_retry_after_exception() {
  for (bool in_transport : {false, true}) {
    reply_fixture f;
    auto send = f.conn->send_package_impl;
    if (in_transport) {
      f.conn->send_package_impl = [](std::string) -> bool { throw std::runtime_error("transport failed"); };
    }
    bool caught = false;
    try {
      f.pending->rsp({"failed", [=] {
        if (!in_transport) throw std::runtime_error("serialization failed");
      }});
    } catch (const std::runtime_error&) {
      caught = true;
    }
    ASSERT(caught && !f.pending->rsp_ready && f.sent.empty());
    f.conn->send_package_impl = std::move(send);
    ASSERT(f.pending->rsp({"retry", {}}));
    ASSERT(f.pending->rsp_ready && f.sent.size() == 1);
    ASSERT(detail::coder::deserialize(f.sent.front()).second.data == "retry");
  }
}
#endif

static void test_reply_survives_reconnect_and_reports_offline() {
  auto pair = loopback_connection::create();
  auto server = rpc::create(pair.first);
  auto client = rpc::create(pair.second);
  server->set_ready(true);
  client->set_ready(true);
  deferred pending;
  server->subscribe("deferred", [&](deferred rr) { pending = std::move(rr); });
  int replies = 0;
  auto req = client->cmd("deferred")->rsp([&](std::string value) { ASSERT(value == "ok"); ++replies; });
  req->call();
  auto reply = pending->rsp;
  pending.reset();
  server->set_ready(false);
  client->set_ready(false);
  ASSERT(reply("offline").type == finally_t::rpc_not_ready);
  ASSERT(replies == 0);
  server->set_ready(true);
  client->set_ready(true);
  ASSERT(reply("ok"));
  ASSERT(replies == 1);
  ASSERT(reply("duplicate").type == finally_t::busy);
}

static void test_old_reply_cannot_reach_new_peer() {
  auto pair = loopback_connection::create();
  auto server_conn = pair.first;
  auto server = rpc::create(server_conn);
  auto old_client = rpc::create(pair.second);
  old_client->set_ready(true);
  server->set_ready(true);
  std::vector<deferred> pending;
  server->subscribe("deferred", [&](deferred rr) { pending.push_back(std::move(rr)); });
  old_client->cmd("deferred")->rsp([](std::string) { ASSERT(false); })->call();
  old_client.reset();
  server->set_ready(false);
  server->reset_session();

  auto new_conn = std::make_shared<connection>();
  new_conn->send_package_impl = [server_conn](std::string data) { server_conn->on_recv_package(std::move(data)); return true; };
  server_conn->send_package_impl = [weak = std::weak_ptr<connection>(new_conn)](std::string data) {
    if (auto peer = weak.lock()) {
      peer->on_recv_package(std::move(data));
      return true;
    }
    return false;
  };
  auto new_client = rpc::create(new_conn);
  new_client->set_ready(true);
  server->set_ready(true);
  int replies = 0;
  new_client->cmd("deferred")->rsp([&](std::string data) { ASSERT(data == "new"); ++replies; })->call();
  ASSERT(pending.size() == 2);
  ASSERT(pending[0]->rsp("old secret").type == finally_t::session_reset);
  ASSERT(replies == 0);
  ASSERT(pending[1]->rsp("new"));
  ASSERT(replies == 1);
}

static void test_reset_completes_requests_and_allows_reentry() {
  auto conn = std::make_shared<connection>();
  std::vector<std::string> sent;
  std::vector<rpc::timeout_cb> timers;
  conn->send_package_impl = [&](std::string data) { sent.push_back(std::move(data)); return true; };
  auto r = rpc::create(conn);
  r->set_ready(true);
  r->set_timer([&](uint32_t, rpc::timeout_cb cb) { timers.push_back(std::move(cb)); });
  int resets = 0, replies = 0, timeouts = 0;
  auto req = r->cmd("x")->rsp([&](std::string data) { ASSERT(data == "ok"); ++replies; });
  req->timeout([&] { ++timeouts; });
  req->finally([&](finally_t type) {
    if (type == finally_t::session_reset) { ++resets; ASSERT(req->call()); }
    else ASSERT(type == finally_t::normal);
  });
  req->call();
  auto first = detail::coder::deserialize(sent[0]).second;
  r->reset_session();
  ASSERT(resets == 1 && sent.size() == 2 && r->is_ready());
  auto second = detail::coder::deserialize(sent[1]).second;
  ASSERT(second.seq == first.seq + 1);
  timers[0]();
  ASSERT(timeouts == 0);
  std::string data = "old";
  auto old_response = detail::msg_wrapper::make_rsp(first.seq, &data);
  conn->on_recv_package(detail::coder::serialize(old_response).second);
  ASSERT(replies == 0);
  data = "ok";
  auto response = detail::msg_wrapper::make_rsp(second.seq, &data);
  conn->on_recv_package(detail::coder::serialize(response).second);
  ASSERT(replies == 1);
  timers[1]();
  ASSERT(timeouts == 0);
}

#ifdef RPC_CORE_FEATURE_FUTURE
static void test_reset_completes_future_and_releases_request() {
  for (bool ready : {false, true}) {
    auto r = rpc::create();
    r->get_connection()->send_package_impl = [](std::string) { return true; };
    r->set_ready(true);
    auto req = r->cmd("pending");
    request_w observer = req;
    auto future = req->future<std::string>();
    req.reset();
    r->set_ready(ready);
    r->reset_session();
    ASSERT(future.get().type == finally_t::session_reset);
    ASSERT(observer.expired());
    ASSERT(r->is_ready() == ready);
  }
}

#endif

static void test_old_timer_cannot_remove_reused_sequence() {
  auto conn = std::make_shared<connection>();
  auto dispatcher = std::make_shared<detail::msg_dispatcher>(conn);
  std::vector<rpc::timeout_cb> timers;
  dispatcher->set_timer_impl([&](uint32_t, rpc::timeout_cb cb) { timers.push_back(std::move(cb)); });
  int resets = 0, old_timeouts = 0, new_timeouts = 0;
  dispatcher->subscribe_rsp(7, [](detail::msg_wrapper) { return true; }, [&] { ++old_timeouts; }, 10, nullptr, [&] { ++resets; });
  dispatcher->reset_session();
  dispatcher->subscribe_rsp(7, [](detail::msg_wrapper) { return true; }, [&] { ++new_timeouts; }, 10);
  timers[0]();
  ASSERT(resets == 1 && old_timeouts == 0 && new_timeouts == 0);
  timers[1]();
  ASSERT(new_timeouts == 1);
}

static void test_reset_inside_handler_suppresses_response_and_keeps_subscription() {
  auto pair = loopback_connection::create();
  auto server = rpc::create(pair.first);
  auto client = rpc::create(pair.second);
  server->set_ready(true);
  client->set_ready(true);
  int calls = 0, replies = 0;
  server->subscribe("x", [&](std::string value) {
    if (++calls == 1) server->reset_session();
    return value;
  });
  auto req = client->cmd("x")->msg(std::string("ok"))->rsp([&](std::string) { ++replies; });
  req->call();
  ASSERT(calls == 1 && replies == 0);
  req->cancel()->reset_cancel()->call();
  ASSERT(calls == 2 && replies == 1);
  server->subscribe("async", [&](deferred rr) {
    server->reset_session();
    ASSERT(rr->rsp("old").type == finally_t::session_reset);
  });
  auto pending = client->cmd("async")->rsp([&](std::string) { ASSERT(false); });
  pending->call();
  pending->cancel();
}

static void test_transport_rejection_and_expired_reply() {
  auto conn = std::make_shared<connection>();
  int sent = 0;
  auto r = rpc::create(conn);
  r->set_ready(true);
  ASSERT(r->cmd("unbound")->mark_need_rsp()->call().type == finally_t::rpc_not_ready);
  conn->send_package_impl = [&](std::string) { ++sent; return false; };
  auto req = r->cmd("x");
  request_w observer = req;
  ASSERT(req->mark_need_rsp()->call().type == finally_t::rpc_not_ready);
  req.reset();
  ASSERT(sent == 1 && observer.expired());

  auto pair = loopback_connection::create();
  auto server = rpc::create(pair.first);
  auto client = rpc::create(pair.second);
  client->set_ready(true);
  deferred pending;
  server->subscribe("x", [&](deferred rr) { pending = std::move(rr); });
  auto waiting = client->cmd("x")->rsp([](std::string) { ASSERT(false); });
  waiting->call();
  auto reply = pending->rsp;
  pending.reset();
  server.reset();
  ASSERT(reply("gone").type == finally_t::rpc_expired);
  waiting->cancel();
}

static void test_inline_reply_reports_transport_rejection_and_can_retry() {
  auto pair = loopback_connection::create();
  auto server = rpc::create(pair.first);
  auto client = rpc::create(pair.second);
  server->set_ready(true);
  client->set_ready(true);
  auto send = pair.first->send_package_impl;
  pair.first->send_package_impl = [](std::string) { return false; };
  deferred pending;
  server->subscribe("x", [&](deferred rr) {
    pending = rr;
    ASSERT(rr->rsp("rejected").type == finally_t::rpc_not_ready);
    ASSERT(!rr->rsp_ready);
  });
  int replies = 0;
  client->cmd("x")->rsp([&](std::string value) { ASSERT(value == "ok"); ++replies; })->call();
  ASSERT(replies == 0);
  pair.first->send_package_impl = std::move(send);
  ASSERT(pending->rsp("ok"));
  ASSERT(pending->rsp_ready && replies == 1);
}

static void test_response_callback_can_replace_in_flight_deferred_reply() {
  auto pair = loopback_connection::create();
  auto server = rpc::create(pair.first);
  auto client = rpc::create(pair.second);
  server->set_ready(true);
  client->set_ready(true);
  deferred pending;
  server->subscribe("x", [&](deferred rr) { pending = std::move(rr); });
  int replies = 0;
  auto req = client->cmd("x");
  req->rsp([&](std::string value) {
    ASSERT(value == (replies == 0 ? "first" : "second"));
    if (++replies == 1) ASSERT(req->call());
  });
  req->call();
  auto old_reply = std::weak_ptr<deferred::element_type>(pending);
  ASSERT(pending->rsp("first"));
  ASSERT(old_reply.expired() && pending && replies == 1);
  ASSERT(pending->rsp("second"));
  ASSERT(replies == 2);
}

static void test_reset_during_timeout_can_reuse_request() {
  for (int retries : {0, 1, -1}) {
    auto conn = std::make_shared<connection>();
    auto r = rpc::create(conn);
    int sent = 0, resets = 0, timeouts = 0;
    conn->send_package_impl = [&](std::string) { ++sent; return true; };
    r->set_ready(true);
    std::vector<rpc::timeout_cb> timers;
    r->set_timer([&](uint32_t, rpc::timeout_cb cb) { timers.push_back(std::move(cb)); });
    auto req = r->cmd("x")->rsp([] { ASSERT(false); })->retry(retries);
    req->finally([&](finally_t type) {
      if (type == finally_t::session_reset) ++resets;
      else { ASSERT(type == finally_t::timeout); ++timeouts; }
    });
    req->timeout([&] {
      timers[0](); // Reentering the same expired timer must do nothing.
      auto late = detail::msg_wrapper::make_rsp<uint8_t>(0);
      conn->on_recv_package(detail::coder::serialize(late).second);
      r->reset_session();
      ASSERT(resets == 1);
      req->timeout(nullptr)->retry(0);
      ASSERT(req->call());
    });
    ASSERT(req->call());
    auto first = timers[0];
    first();
    ASSERT(sent == 2 && timers.size() == 2 && resets == 1 && timeouts == 0);
    first();
    request_w observer = req;
    req.reset();
    timers[1]();
    ASSERT(timeouts == 1 && observer.expired());
  }
}

static void test_wide_string_replies() {
  auto pair = loopback_connection::create();
  auto server = rpc::create(pair.first);
  auto client = rpc::create(pair.second);
  server->set_ready(true);
  client->set_ready(true);
  server->subscribe("wide", [] { return std::wstring(L"wide"); });
  server->subscribe("utf16", [](int) { return std::u16string(u"utf16"); });
  int replies = 0;
  client->cmd("wide")->rsp([&](std::wstring value) { ASSERT(value == L"wide"); ++replies; })->call();
  client->cmd("utf16")->msg(1)->rsp([&](std::u16string value) { ASSERT(value == u"utf16"); ++replies; })->call();
  ASSERT(replies == 2);
}

static void test_stream_rejects_oversized_sends() {
  auto conn = std::make_shared<stream_connection>(8);
  int writes = 0, completed = 0;
  conn->send_bytes_impl = [&](std::string) { ++writes; return true; };
  auto r = rpc::create(conn);
  r->set_ready(true);
  auto req = r->cmd("oversized")->mark_need_rsp();
  req->finally([&](finally_t type) { ASSERT(type == finally_t::rpc_not_ready); ++completed; });
  ASSERT(req->call().type == finally_t::rpc_not_ready);
  ASSERT(writes == 0 && completed == 1);
  request_w observer = req;
  req.reset();
  ASSERT(observer.expired());
  // Even an empty package has a four-byte frame and must still be sent.
  ASSERT(conn->send_package({}) && writes == 1);
}

static void test_scheduled_subscription_keeps_state_and_lifetime() {
  using rr_type = request_response<int, int>;
  // Direct, immediately scheduled, and queued calls must share the same state.
  for (int mode = 0; mode < 3; ++mode) {
    auto pair = loopback_connection::create();
    auto server = rpc::create(pair.first);
    auto client = rpc::create(pair.second);
    client->set_ready(true);
    client->set_timer([](uint32_t, rpc::timeout_cb) {});
    std::vector<std::function<void()>> tasks;
    std::vector<int> replies;
    auto token = std::make_shared<int>(42);
    std::weak_ptr<int> observer = token;
    auto handle = [count = 0, token](rr_type rr) mutable {
      ASSERT(*token == 42);
      ASSERT(rr->rsp(++count));
    };
    token.reset();
    if (mode == 0) server->subscribe("next", std::move(handle));
    else server->subscribe("next", std::move(handle), [&](std::function<void()> task) {
      if (mode == 1) task();
      else tasks.push_back(std::move(task));
    });
    auto call = [&] {
      ASSERT(client->cmd("next")->msg(0)->rsp([&](int value) { replies.push_back(value); })->call());
    };
    call();
    call();
    call();
    if (mode == 2) {
      ASSERT(replies.empty());
      // Replacement gets fresh state; queued calls retain the old instance.
      server->subscribe("next", [count = 100](rr_type rr) mutable { ASSERT(rr->rsp(++count)); },
                        [&](std::function<void()> task) { tasks.push_back(std::move(task)); });
      call();
    }
    server->unsubscribe("next");
    if (mode == 2) {
      ASSERT(!observer.expired());
      for (auto& task : tasks) task();
      tasks.clear();
      ASSERT((replies == std::vector<int>{1, 2, 3, 101}));
    } else {
      ASSERT((replies == std::vector<int>{1, 2, 3}));
    }
    ASSERT(observer.expired());
  }
}

static void test_sequence_wrap_keeps_pending_calls() {
  auto conn = std::make_shared<connection>();
  auto dispatcher = std::make_shared<detail::msg_dispatcher>(conn);
  std::vector<rpc::timeout_cb> timers;
  dispatcher->set_timer_impl([&](uint32_t, rpc::timeout_cb cb) { timers.push_back(std::move(cb)); });
  int completed = 0;
  auto pending = [&](seq_type seq) {
    dispatcher->subscribe_rsp(seq, [](detail::msg_wrapper) { return true; }, [&] { ++completed; }, 10);
  };
  seq_type next = 0;
  pending(dispatcher->make_seq(next));
  next = UINT32_MAX;
  ASSERT(dispatcher->make_seq(next) == UINT32_MAX);
  pending(UINT32_MAX);
  const auto after_wrap = dispatcher->make_seq(next);
  ASSERT(after_wrap == 1);
  pending(after_wrap);
  next = UINT32_MAX;
  ASSERT(dispatcher->make_seq(next) == 2);
  for (auto& timer : timers) timer();
  ASSERT(completed == 3);
  next = 0;
  ASSERT(dispatcher->make_seq(next) == 0);
}

int main() {
  test_deferred_reply_blocks_serialization_reentry();
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
  test_deferred_reply_can_retry_after_exception();
#endif
  test_sequence_wrap_keeps_pending_calls();
  test_scheduled_subscription_keeps_state_and_lifetime();
  test_wide_string_replies();
  test_stream_rejects_oversized_sends();
  test_reset_during_timeout_can_reuse_request();
  test_reply_survives_reconnect_and_reports_offline();
  test_old_reply_cannot_reach_new_peer();
  test_reset_completes_requests_and_allows_reentry();
#ifdef RPC_CORE_FEATURE_FUTURE
  test_reset_completes_future_and_releases_request();
#endif
  test_old_timer_cannot_remove_reused_sequence();
  test_reset_inside_handler_suppresses_response_and_keeps_subscription();
  test_transport_rejection_and_expired_reply();
  test_inline_reply_reports_transport_rejection_and_can_retry();
  test_response_callback_can_replace_in_flight_deferred_reply();
}
