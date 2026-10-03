#include <chrono>
#include <stdexcept>
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

struct reentrant_response {
  static std::function<void()> on_decode;
  static bool fail;

  void operator<<(serialize_iarchive& ar) {
    auto callback = std::move(on_decode);
    callback();
    ar.error = fail;
  }
};

std::function<void()> reentrant_response::on_decode;
bool reentrant_response::fail = false;

#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
static void test_send_unwind_finishes_only_its_attempt() {
  for (bool need_rsp : {false, true}) {
    for (bool timer_failure : {false, true}) {
      for (int reentry : {0, 1, 2, 3}) {
        if (!need_rsp && (timer_failure || reentry)) continue;
        if (timer_failure && reentry == 3) continue;
        fixture f;
        std::vector<finally_t> finished;
        auto req = f.r->cmd("pending")->finally([&](finally_t status) { finished.push_back(status); });
        if (need_rsp) req->mark_need_rsp();
        if (reentry == 2) req->retry(1);
        request_w weak = req;
        bool fail = true;
        auto callback = [&] {
          if (!fail) return;
          fail = false;
          if (reentry == 1) {
            req->cancel()->reset_cancel();
            ASSERT(req->call());
          } else if (reentry == 2) {
            f.expire(0);
          } else if (reentry == 3) {
            f.reply(f.sent.size() - 1, "completed");
          }
          throw std::runtime_error("send setup failed");
        };
        if (timer_failure) {
          f.r->set_timer([&](uint32_t, rpc::timeout_cb cb) {
            f.timers.push_back(std::move(cb));
            callback();
          });
        } else {
          f.conn->send_package_impl = [&](std::string packet) {
            f.sent.push_back(std::move(packet));
            callback();
            return true;
          };
        }
        bool caught = false;
        try {
          req->call();
        } catch (const std::runtime_error& error) {
          caught = std::string(error.what()) == "send setup failed";
        }
        ASSERT(caught);
        if (reentry == 0) {
          ASSERT(finished == std::vector<finally_t>{finally_t::rpc_not_ready});
          ASSERT(req->call());
        } else if (reentry == 3) {
          ASSERT(finished == std::vector<finally_t>{finally_t::normal});
          ASSERT(req->call());
        } else if (reentry == 1) {
          ASSERT(finished == std::vector<finally_t>{finally_t::canceled});
        } else {
          ASSERT(finished.empty());
        }
        if (need_rsp) {
          f.expire(0);
          ASSERT(req->call().type == finally_t::busy);
          f.reply(f.sent.size() - 1, "ok");
          ASSERT(finished.back() == finally_t::normal);
        } else {
          ASSERT(finished.back() == finally_t::no_need_rsp);
        }
        ASSERT(finished.size() == (reentry == 2 ? 1 : 2));
        for (size_t i = 0; i < f.timers.size(); ++i) f.expire(i);
        ASSERT(finished.size() == (reentry == 2 ? 1 : 2));
        req.reset();
        ASSERT(weak.expired());
      }
    }
  }
}

static void test_timeout_unwind_finishes_only_its_call() {
  for (int retries : {-1, 0, 2}) {
    for (auto status : {finally_t::timeout, finally_t::canceled, finally_t::session_reset}) {
      fixture f;
      std::vector<finally_t> finished;
      auto req = f.r->cmd("pending")->mark_need_rsp()->retry(retries)
          ->finally([&](finally_t result) { finished.push_back(result); });
      request_w weak = req;
      req->timeout([&] {
        if (status != finally_t::timeout) {
          if (status == finally_t::session_reset) f.r->reset_session();
          else req->cancel()->reset_cancel();
          req->timeout(nullptr)->retry(0);
          ASSERT(req->call());
        }
        req.reset();
        throw std::runtime_error("timeout callback failed");
      });
      ASSERT(req->call());
      bool caught = false;
      try {
        f.expire(0);
      } catch (const std::runtime_error& error) {
        caught = std::string(error.what()) == "timeout callback failed";
      }
      ASSERT(caught);
      ASSERT(finished == std::vector<finally_t>{status});
      f.expire(0);
      f.reply(0, "late");
      ASSERT(finished.size() == 1);
      ASSERT(f.sent.size() == (status == finally_t::timeout ? 1 : 2));
      if (status != finally_t::timeout) {
        ASSERT(weak.lock()->call().type == finally_t::busy);
        f.reply(1, "new");
        ASSERT(finished.size() == 2 && finished.back() == finally_t::normal);
      }
      ASSERT(weak.expired());
    }
  }
}

static void test_response_decode_unwind_finishes_only_its_call() {
  for (int arity : {1, 2}) {
    for (auto status : {finally_t::rsp_serialize_error, finally_t::canceled, finally_t::session_reset}) {
      fixture f;
      std::vector<finally_t> finished;
      auto req = f.r->cmd("pending")->finally([&](finally_t result) { finished.push_back(result); });
      request_w weak = req;
      if (arity == 1) req->rsp([](reentrant_response) { ASSERT(false); });
      else req->rsp([](reentrant_response, finally_t) { ASSERT(false); });
      reentrant_response::on_decode = [&] {
        if (status != finally_t::rsp_serialize_error) {
          if (status == finally_t::session_reset) f.r->reset_session();
          else req->cancel()->reset_cancel();
          req->rsp([](std::string data) { ASSERT(data == "new"); });
          ASSERT(req->call());
        }
        req.reset();
        throw std::runtime_error("response decoder failed");
      };
      ASSERT(req->call());
      bool caught = false;
      try {
        f.reply(0, "old");
      } catch (const std::runtime_error& error) {
        caught = std::string(error.what()) == "response decoder failed";
      }
      ASSERT(caught);
      ASSERT(finished == std::vector<finally_t>{status});
      f.expire(0);
      ASSERT(finished.size() == 1);
      if (status != finally_t::rsp_serialize_error) {
        ASSERT(weak.lock()->call().type == finally_t::busy);
        f.reply(1, "new");
        ASSERT(finished.size() == 2 && finished.back() == finally_t::normal);
      }
      ASSERT(weak.expired());
    }
  }
}

static void test_response_unwind_runs_finally() {
  for (int arity : {0, 1, 2}) {
    for (bool reuse : {false, true}) {
      fixture f;
      std::vector<finally_t> finished, next_finished;
      auto req = f.r->cmd("pending");
      request_w weak = req;
      req->finally([&](finally_t status) {
        ASSERT(!weak.expired());
        finished.push_back(status);
      });
      auto response = [&] {
        if (reuse) {
          req->rsp([] {})->finally([&](finally_t status) { next_finished.push_back(status); });
          ASSERT(req->call());
        } else {
          req.reset();
        }
        throw std::runtime_error("response callback failed");
      };
      if (arity == 0) req->rsp(response);
      else if (arity == 1) req->rsp([&](std::string) { response(); });
      else req->rsp([&](std::string, finally_t) { response(); });
      ASSERT(req->call());
      bool caught = false;
      try {
        f.reply(0, "old");
      } catch (const std::runtime_error& error) {
        caught = std::string(error.what()) == "response callback failed";
      }
      ASSERT(caught);
      ASSERT(finished == std::vector<finally_t>{finally_t::normal});
      f.expire(0);
      ASSERT(finished.size() == 1);
      if (reuse) {
        ASSERT(req->call().type == finally_t::busy);
        ASSERT(next_finished.empty());
        f.reply(1, "new");
        ASSERT(next_finished == std::vector<finally_t>{finally_t::normal});
        req.reset();
      }
      ASSERT(weak.expired());
    }
  }
}

static void test_finally_unwind_releases_request() {
  fixture f;
  auto req = f.r->cmd("pending");
  request_w weak = req;
  int responses = 0, completions = 0;
  req->rsp([&] { ++responses; })->finally([&](finally_t status) {
    ASSERT(status == finally_t::normal && responses == 1);
    ++completions;
    req.reset();
    throw std::runtime_error("finally failed");
  });
  ASSERT(req->call());
  bool caught = false;
  try {
    f.reply(0, "reply");
  } catch (const std::runtime_error& error) {
    caught = std::string(error.what()) == "finally failed";
  }
  ASSERT(caught && completions == 1 && weak.expired());
  f.expire(0);
  ASSERT(completions == 1);
}

static void test_callback_unwind_cleans_only_its_registration(bool timeout) {
  for (bool replace : {false, true}) {
    auto conn = std::make_shared<connection>();
    auto dispatcher = std::make_shared<detail::msg_dispatcher>(conn);
    dispatcher->init();
    detail::msg_dispatcher::timeout_cb timer;
    dispatcher->set_timer_impl([&](uint32_t, detail::msg_dispatcher::timeout_cb cb) { timer = std::move(cb); });
    int resets = 0;
    auto callback = [&] {
      if (replace) {
        dispatcher->subscribe_rsp(7, [](detail::msg_wrapper) { return true; }, nullptr, 1,
                                  nullptr, [&] { ++resets; });
      }
      throw std::runtime_error("callback failed");
    };
    dispatcher->subscribe_rsp(7, [&](detail::msg_wrapper) { callback(); return true; }, callback,
                              1, nullptr, [&] { resets += 10; });
    auto response = detail::msg_wrapper::make_rsp<std::string>(7);
    bool caught = false;
    try {
      if (timeout) {
        auto fire = timer;
        fire();
      } else {
        conn->on_recv_package(detail::coder::serialize(response).second);
      }
    } catch (const std::runtime_error&) {
      caught = true;
    }
    ASSERT(caught);
    dispatcher->reset_session();
    ASSERT(resets == (replace ? 1 : 0));
  }
}
#endif

static void test_response_deserialization_cannot_finish_reused_request(bool reset_session) {
  for (bool fail : {false, true}) {
    for (int arity : {1, 2}) {
      fixture f;
      std::vector<finally_t> finished;
      int replies = 0;
      auto req = f.r->cmd("pending")->finally([&](finally_t status) { finished.push_back(status); });
      if (arity == 1) req->rsp([](reentrant_response) { ASSERT(false); });
      else req->rsp([](reentrant_response, finally_t) { ASSERT(false); });
      reentrant_response::fail = fail;
      reentrant_response::on_decode = [&] {
        if (reset_session) f.r->reset_session();
        else req->cancel()->reset_cancel();
        req->rsp([&](std::string value) {
          ASSERT(value == "new");
          ++replies;
        });
        ASSERT(req->call());
      };
      ASSERT(req->call());
      f.reply(0, "old");
      ASSERT(finished == std::vector<finally_t>{reset_session ? finally_t::session_reset : finally_t::canceled});
      ASSERT(replies == 0 && req->call().type == finally_t::busy);
      f.expire(0);
      f.reply(1, "new");
      ASSERT(replies == 1 && finished.size() == 2 && finished.back() == finally_t::normal);
      f.expire(1);
      ASSERT(finished.size() == 2);
    }
  }
}

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

static void test_reentrant_retry_survives_old_send_failure() {
  fixture f;
  std::vector<finally_t> finished;
  f.conn->send_package_impl = [&](std::string packet) {
    f.sent.push_back(std::move(packet));
    if (f.sent.size() == 1) {
      f.expire(0);  // A nested event loop can expire this attempt while sending.
      return false;
    }
    return true;
  };
  auto req = f.r->cmd("retry")->retry(1)->rsp([](std::string value) { ASSERT(value == "ok"); })
      ->finally([&](finally_t status) { finished.push_back(status); });
  ASSERT(req->call().type == finally_t::rpc_not_ready);
  ASSERT(f.sent.size() == 2 && finished.empty());
  ASSERT(req->call().type == finally_t::busy);
  f.reply(0, "stale");
  f.expire(0);
  ASSERT(finished.empty());
  f.reply(1, "ok");
  ASSERT(finished == std::vector<finally_t>{finally_t::normal});
  f.expire(1);
  ASSERT(finished.size() == 1);
}

static void test_timer_registration_can_disconnect_rpc() {
  fixture f;
  std::vector<finally_t> finished;
  f.r->set_timer([&](uint32_t, rpc::timeout_cb cb) {
    f.timers.push_back(std::move(cb));
    if (f.timers.size() == 1) f.r->set_ready(false);
  });
  auto req = f.r->cmd("x")->rsp([](std::string value) { ASSERT(value == "ok"); })
      ->finally([&](finally_t status) { finished.push_back(status); });
  ASSERT(req->call().type == finally_t::rpc_not_ready);
  ASSERT(f.sent.empty() && finished == std::vector<finally_t>{finally_t::rpc_not_ready});
  f.expire(0);
  ASSERT(finished.size() == 1);
  f.r->set_ready(true);
  ASSERT(req->call());
  f.expire(0);
  ASSERT(f.sent.size() == 1 && finished.size() == 1);
  f.reply(0, "ok");
  ASSERT(finished.size() == 2 && finished.back() == finally_t::normal);
}

static void test_clearing_finally_callback(bool no_arguments) {
  auto r = rpc::create();
  r->get_connection()->send_package_impl = [](std::string) { return true; };
  r->set_ready(true);
  rpc::timeout_cb timer;
  r->set_timer([&](uint32_t, rpc::timeout_cb cb) { timer = std::move(cb); });
  int finished = 0;
  auto req = r->cmd("pending")->mark_need_rsp()->finally([&] { ++finished; });
  auto clear = [&] {
    if (no_arguments) req->finally(std::function<void()>{});
    else req->finally(std::function<void(finally_t)>{});
  };
  ASSERT(req->call());
  clear();
  auto fire = std::move(timer);
  fire();
  ASSERT(finished == 1);

  // The next accepted call uses the cleared callback, including immediate failures.
  ASSERT(req->call());
  fire = std::move(timer);
  fire();
  ASSERT(finished == 1);
  r->set_ready(false);
  ASSERT(req->call().type == finally_t::rpc_not_ready);
  ASSERT(finished == 1);

  req->finally([&] { ++finished; });
  ASSERT(req->call().type == finally_t::rpc_not_ready);
  ASSERT(finished == 2);
}

int main() {
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
  test_send_unwind_finishes_only_its_attempt();
  test_timeout_unwind_finishes_only_its_call();
  test_response_decode_unwind_finishes_only_its_call();
  test_response_unwind_runs_finally();
  test_finally_unwind_releases_request();
  test_callback_unwind_cleans_only_its_registration(false);
  test_callback_unwind_cleans_only_its_registration(true);
#endif
  test_response_deserialization_cannot_finish_reused_request(false);
  test_response_deserialization_cannot_finish_reused_request(true);
  test_clearing_finally_callback(false);
  test_clearing_finally_callback(true);
  test_timer_registration_can_disconnect_rpc();
  test_reentrant_retry_survives_old_send_failure();
  test_reused_callbacks_keep_mutable_state();
  test_busy_keeps_original_call();
  test_cancel_and_stale_callbacks();
  test_retries_keep_call_and_configuration();
  test_timeout_callback_can_cancel_and_reuse();
#ifdef RPC_CORE_FEATURE_FUTURE
  test_future_busy_and_reuse();
#endif
}
