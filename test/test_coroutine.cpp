#include <asio.hpp>
#include <algorithm>
#include <vector>
#include "rpc_core.hpp"
#include "assert_def.h"

using namespace rpc_core;

template <typename T>
static void test_asio_cancellation() {
  asio::io_context io;
  auto r = rpc::create();
  r->set_ready(true);
  r->get_connection()->send_package_impl = [](std::string) { return true; };
  r->set_timer([](uint32_t, rpc::timeout_cb) {});
  auto req = r->cmd("pending");
  asio::cancellation_signal signal;
  bool done = false;
  asio::co_spawn(io, req->co_call<T>(), asio::bind_cancellation_slot(signal.slot(),
      [&](std::exception_ptr error, result<T> response) {
        ASSERT(!error && response.type == finally_t::canceled);
        done = true;
      }));
  io.poll();
  signal.emit(asio::cancellation_type::terminal);
  io.restart();
  io.poll();
  ASSERT(done && req->is_canceled());
  req->reset_cancel()->finally(std::function<void(finally_t)>{});
  ASSERT(req->call());
  signal.emit(asio::cancellation_type::terminal);
  io.restart();
  io.poll();
  ASSERT(!req->is_canceled());
  req->cancel();
}

static void test_scheduled_coroutine_keeps_handler_and_borrowed_request() {
  asio::io_context io;
  asio::steady_timer gate(io, asio::steady_timer::time_point::max());
  auto pair = loopback_connection::create();
  auto server = rpc::create(pair.first);
  auto client = rpc::create(pair.second);
  client->set_ready(true);
  client->set_timer([](uint32_t, rpc::timeout_cb) {});
  auto token = std::make_shared<int>(42);
  std::weak_ptr<int> observer = token;
  int started = 0, completed = 0;
  std::vector<int> replies;
  server->subscribe("next", [&, token, count = 0](const request_response<int, int>& rr) mutable -> asio::awaitable<void> {
    ++started;
    asio::error_code error;
    co_await gate.async_wait(asio::redirect_error(asio::use_awaitable, error));
    ASSERT(error == asio::error::operation_aborted);
    ASSERT(*token == rr->req);
    ASSERT(rr->rsp(++count));
  }, [&](std::function<asio::awaitable<void>()> task) {
    // A coroutine scheduler owns the callable until its awaitable completes.
    asio::co_spawn(io, [task = std::move(task)]() -> asio::awaitable<void> {
      co_await task();
    }, [&](std::exception_ptr error) { ASSERT(!error); ++completed; });
  });
  token.reset();
  for (int i = 0; i < 3; ++i) {
    ASSERT(client->cmd("next")->msg(42)->rsp([&](int value) { replies.push_back(value); })->call());
  }
  io.poll();
  ASSERT(started == 3 && completed == 0 && replies.empty());
  server->unsubscribe("next");
  ASSERT(!observer.expired());
  gate.cancel();
  io.restart();
  io.run();
  ASSERT(completed == 3 && observer.expired());
  std::sort(replies.begin(), replies.end());
  ASSERT((replies == std::vector<int>{1, 2, 3}));
}

int main() {
  test_scheduled_coroutine_keeps_handler_and_borrowed_request();
  test_asio_cancellation<void>();
  test_asio_cancellation<std::string>();
  asio::io_context io;
  bool done = false;
  asio::co_spawn(io, [&]() -> asio::awaitable<void> {
    auto r = rpc::create();
    auto delayed = r->cmd("x")->co_call<std::string>();
    co_await asio::post(io, asio::use_awaitable);
    ASSERT((co_await std::move(delayed)).type == finally_t::rpc_not_ready);
    auto delayed_void = r->cmd("x")->co_call<>();
    co_await asio::post(io, asio::use_awaitable);
    ASSERT((co_await std::move(delayed_void)).type == finally_t::rpc_not_ready);
    request_w abandoned;
    {
      auto req = r->cmd("x");
      abandoned = req;
      auto op = req->co_call<>();
      req.reset();
      ASSERT(!abandoned.expired());
    }
    ASSERT(abandoned.expired());
    auto not_ready = co_await r->cmd("x")->co_call<std::string>();
    ASSERT(not_ready.type == finally_t::rpc_not_ready);
    auto conn = loopback_connection::create();
    auto server = rpc::create(conn.first);
    auto client = rpc::create(conn.second);
    client->set_ready(true);
    auto missing = co_await client->cmd("missing")->co_call<std::string>();
    ASSERT(missing.type == finally_t::no_such_cmd);
    server->subscribe("echo", [](std::string data) { return data; });
    auto ok = co_await client->cmd("echo")->msg(std::string("hello"))->co_call<std::string>();
    ASSERT(ok && ok.data == "hello");
    auto saved = client->co_call<std::string>("echo", std::string("temporary"));
    co_await asio::post(io, asio::use_awaitable);
    ASSERT((co_await std::move(saved)).data == "temporary");
    auto reusable = client->cmd("echo")->msg(std::string("again"));
    ASSERT((co_await reusable->co_call<std::string>()).data == "again");
    ASSERT(reusable->call());
    ASSERT((co_await reusable->co_call<>()).type == finally_t::normal);
    ASSERT(reusable->call());
    r->get_connection()->send_package_impl = [](std::string) { return true; };
    r->set_ready(true);
    r->set_timer([&](uint32_t, rpc::timeout_cb cb) { asio::post(io, std::move(cb)); });
    auto timed = co_await r->cmd("x")->co_call<std::string>();
    ASSERT(timed.type == finally_t::timeout);
    auto req = r->cmd("x");
    asio::post(io, [req] { req->cancel(); });
    auto canceled = co_await req->co_call<std::string>();
    ASSERT(canceled.type == finally_t::canceled);
    auto dead = request::create();
    auto expired = co_await dead->co_call<std::string>();
    ASSERT(expired.type == finally_t::rpc_expired);
    r->set_timer([](uint32_t, rpc::timeout_cb) {});
    auto pending = r->cmd("x");
    int busy_finished = 0;
    pending->rsp([](std::string) {})->finally([&](finally_t type) {
      ASSERT(type == finally_t::canceled);
      ++busy_finished;
    });
    ASSERT(pending->call());
    ASSERT((co_await pending->co_call<std::string>()).type == finally_t::busy);
    ASSERT((co_await pending->co_call<>()).type == finally_t::busy);
    ASSERT(pending->call().type == finally_t::busy);
    ASSERT(busy_finished == 0);
    pending->cancel()->reset_cancel();
    ASSERT(busy_finished == 1);
    r->set_timer([](uint32_t, rpc::timeout_cb) {});
    asio::post(io, [&r] { r.reset(); });
    auto destroyed = co_await pending->co_call<std::string>();
    ASSERT(destroyed.type == finally_t::rpc_expired);
    done = true;
  }, [](std::exception_ptr error) { if (error) std::rethrow_exception(error); });
  io.run();
  ASSERT(done);
}
