#pragma once

#include "request.hpp"
#include "rpc.hpp"

namespace rpc_core {

result<void> request::call(const rpc_s& rpc) {
  if (active_) return {finally_t::busy};
  // Synchronous completion may release the caller's last reference.
  auto self = shared_from_this();
  ++call_id_;
  if (rpc) rpc_ = rpc;
  active_ = std::make_shared<call_options>(call_options{
      rpc_, cmd_, payload_, need_rsp_, rsp_handle_, timeout_ms_, timeout_cb_, finally_, is_ping_});
  retries_remaining_ = retry_count_;
  self_keeper_ = self;
  return send_attempt();
}

result<void> request::send_attempt() {
  auto self = shared_from_this();
  auto options = active_;
  const auto call_id = call_id_;
  if (canceled_) {
    on_finish(finally_t::canceled);
    return {finally_t::canceled};
  }
  auto r = options->rpc.lock();
  if (!r) {
    on_finish(finally_t::rpc_expired);
    return {finally_t::rpc_expired};
  }
  if (!r->is_ready()) {
    on_finish(finally_t::rpc_not_ready);
    return {finally_t::rpc_not_ready};
  }
  seq_ = r->make_seq();
  const auto seq = seq_;
  auto sent = r->send_request(this);
  // Sending may reenter a timeout and start another attempt of this same call.
  if (!matches_attempt(call_id, seq)) return sent;
  if (!sent) {
    on_finish(sent.type);
    return sent;
  }
  if (!options->need_rsp) {
    on_finish(finally_t::no_need_rsp);
  }
  return sent;
}

void request::on_timeout() {
  auto self = shared_from_this();
  auto options = active_;
  const auto call_id = call_id_;
  const auto seq = seq_;
  if (options->timeout_cb) options->timeout_cb();
  if (!matches_attempt(call_id, seq) || canceled_) return;
  if (retries_remaining_ == -1 || retries_remaining_ > 0) {
    if (retries_remaining_ > 0) --retries_remaining_;
    send_attempt();
  } else {
    on_finish(finally_t::timeout);
  }
}

request_s request::cancel() {
  auto self = shared_from_this();
  canceled_ = true;
  if (active_ && active_->need_rsp) {
    auto r = active_->rpc.lock();
    if (r) {
      r->unsubscribe_rsp(seq_);
    }
  }
  on_finish(finally_t::canceled);
  return self;
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
  if (active_) {
    promise->set_value({finally_t::busy, R{}});
    return promise->get_future();
  }
  auto response = std::make_shared<std::unique_ptr<R>>();
  rsp([response](R r, finally_t) {
    response->reset(new R(std::move(r)));
  });
  finally([promise, response](finally_t type) mutable {
    if (!promise) return;
    promise->set_value({type, *response ? std::move(**response) : R{}});
    promise.reset();
  });
  call(rpc);
  return promise->get_future();
}

template <typename R, typename std::enable_if<std::is_same<R, void>::value, int>::type>
std::future<result<void>> request::future(const rpc_s& rpc) {
  auto promise = std::make_shared<std::promise<result<void>>>();
  if (active_) {
    promise->set_value({finally_t::busy});
    return promise->get_future();
  }
  mark_need_rsp();
  finally([promise](finally_t type) mutable {
    if (!promise) return;
    promise->set_value({type});
    promise.reset();
  });
  call(rpc);
  return promise->get_future();
}
#endif

#ifdef RPC_CORE_FEATURE_CO_ASIO
template <typename R, typename std::enable_if<!std::is_same<R, void>::value, int>::type>
asio::awaitable<result<R>> request::co_call() {
  return co_call_impl<R>(shared_from_this());
}

template <typename R, typename std::enable_if<std::is_same<R, void>::value, int>::type>
asio::awaitable<result<R>> request::co_call() {
  return co_call_impl<R>(shared_from_this());
}

template <typename R, typename std::enable_if<!std::is_same<R, void>::value, int>::type>
asio::awaitable<result<R>> request::co_call_impl(request_s owner) {
  if (owner->active_) co_return result<R>{finally_t::busy, R{}};
  auto executor = co_await asio::this_coro::executor;
  co_return co_await asio::async_compose<decltype(asio::use_awaitable), void(result<R>)>(
      [pending = owner.get(), &executor](auto& self) mutable {
        using ST = std::remove_reference<decltype(self)>::type;
        auto self_sp = std::make_shared<ST>(std::forward<ST>(self));
        auto slot = self_sp->get_cancellation_state().slot();
        if (slot.is_connected()) {
          slot.assign([executor, weak = request_w(pending->shared_from_this()), id = pending->call_id_ + uint32_t(1)](asio::cancellation_type_t type) {
            if (type == asio::cancellation_type::none) return;
            // Complete outside signal emission, where clearing the slot is safe.
            asio::post(executor, [weak, id] {
              auto request = weak.lock();
              if (request && request->active_ && request->call_id_ == id) request->cancel();
            });
          });
        }
        auto response = std::make_shared<std::unique_ptr<R>>();
        pending->rsp([response](R data, finally_t) {
          response->reset(new R(std::move(data)));
        });
        pending->finally([executor, response, slot, self = std::move(self_sp)](finally_t type) mutable {
          if (!self) return;
          slot.clear();
          asio::dispatch(executor, [self = std::move(self), response, type]() mutable {
            self->complete({type, *response ? std::move(**response) : R{}});
          });
        });
        pending->call();
      },
      asio::use_awaitable);
}

template <typename R, typename std::enable_if<std::is_same<R, void>::value, int>::type>
asio::awaitable<result<R>> request::co_call_impl(request_s owner) {
  if (owner->active_) co_return result<R>{finally_t::busy};
  auto executor = co_await asio::this_coro::executor;
  co_return co_await asio::async_compose<decltype(asio::use_awaitable), void(result<R>)>(
      [pending = owner.get(), &executor](auto& self) mutable {
        using ST = std::remove_reference<decltype(self)>::type;
        auto self_sp = std::make_shared<ST>(std::forward<ST>(self));
        auto slot = self_sp->get_cancellation_state().slot();
        if (slot.is_connected()) {
          slot.assign([executor, weak = request_w(pending->shared_from_this()), id = pending->call_id_ + uint32_t(1)](asio::cancellation_type_t type) {
            if (type == asio::cancellation_type::none) return;
            // Complete outside signal emission, where clearing the slot is safe.
            asio::post(executor, [weak, id] {
              auto request = weak.lock();
              if (request && request->active_ && request->call_id_ == id) request->cancel();
            });
          });
        }
        pending->mark_need_rsp();
        pending->finally([executor, slot, self = std::move(self_sp)](finally_t type) mutable {
          if (!self) return;
          slot.clear();
          asio::dispatch(executor, [self = std::move(self), type] {
            self->complete({type});
          });
        });
        pending->call();
      },
      asio::use_awaitable);
}
#endif

}  // namespace rpc_core
