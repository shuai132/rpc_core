#pragma once

#include "request.hpp"
#include "rpc.hpp"

namespace rpc_core {

void request::call(const rpc_s& rpc) {
  waiting_rsp_ = true;

  if (canceled_) {
    on_finish(finally_t::canceled);
    return;
  }

  self_keeper_ = shared_from_this();
  if (rpc) {
    rpc_ = rpc;
  } else if (rpc_.expired()) {
    on_finish(finally_t::rpc_expired);
    return;
  }

  auto r = rpc_.lock();
  if (!r->is_ready()) {
    on_finish(finally_t::rpc_not_ready);
    return;
  }
  seq_ = r->make_seq();
  r->send_request(this);
  if (!need_rsp_) {
    on_finish(finally_t::no_need_rsp);
  }
}

request_s request::add_to(dispose& dispose) {
  auto self = shared_from_this();
  dispose.add(self);
  return self;
}

#ifdef RPC_CORE_FEATURE_FUTURE
template <typename R, typename std::enable_if<!std::is_same<R, void>::value, int>::type>
std::future<result<R>> request::future(const rpc_s& rpc) {
  auto promise = std::make_shared<std::promise<result<R>>>();
  rsp([promise](R r, finally_t type) {
    promise->set_value({type, std::move(r)});
  });
  call(rpc);
  return promise->get_future();
}

template <typename R, typename std::enable_if<std::is_same<R, void>::value, int>::type>
std::future<result<void>> request::future(const rpc_s& rpc) {
  auto promise = std::make_shared<std::promise<result<void>>>();
  mark_need_rsp();
  finally([promise](finally_t type) {
    promise->set_value({type});
  });
  call(rpc);
  return promise->get_future();
}
#endif

#ifdef RPC_CORE_FEATURE_ASIO_COROUTINE
template <typename R, typename std::enable_if<!std::is_same<R, void>::value, int>::type>
asio::awaitable<result<R>> request::async_call() {
  auto executor = co_await asio::this_coro::executor;
  co_return co_await asio::async_compose<decltype(asio::use_awaitable), void(result<R>)>(
      [this, &executor](auto& self) mutable {
        using ST = std::remove_reference<decltype(self)>::type;
        auto self_sp = std::make_shared<ST>(std::forward<ST>(self));
        rsp([&executor, self = std::move(self_sp)](R data, finally_t type) mutable {
          asio::dispatch(executor, [self = std::move(self), data = std::move(data), type]() {
            self->complete({type, data});
          });
        });
        call();
      },
      asio::use_awaitable);
}

template <typename R, typename std::enable_if<std::is_same<R, void>::value, int>::type>
asio::awaitable<result<R>> request::async_call() {
  auto executor = co_await asio::this_coro::executor;
  co_return co_await asio::async_compose<decltype(asio::use_awaitable), void(result<R>)>(
      [this, &executor](auto& self) mutable {
        using ST = std::remove_reference<decltype(self)>::type;
        auto self_sp = std::make_shared<ST>(std::forward<ST>(self));
        mark_need_rsp();
        finally([&executor, self = std::move(self_sp)](finally_t type) mutable {
          asio::dispatch(executor, [self = std::move(self), type] {
            self->complete({type});
          });
        });
        call();
      },
      asio::use_awaitable);
}
#endif

}  // namespace rpc_core
