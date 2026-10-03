#pragma once

#include <exception>

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
  // Timer registration and transport callbacks may unwind before a send completes.
  struct send_scope {
    request& owner;
    rpc_core::rpc& peer;
    uint32_t call_id;
    seq_type seq;
    bool completed = false;
    ~send_scope() noexcept(false) {
      if (!completed && owner.matches_attempt(call_id, seq)) {
        peer.unsubscribe_rsp(seq);
        if (owner.matches_attempt(call_id, seq)) owner.on_finish(finally_t::rpc_not_ready);
      }
    }
  } sending{*self, *r, call_id, seq};
  auto sent = r->send_request(this);
  sending.completed = true;
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
  attempt_callback_scope completion{*self, call_id, seq, finally_t::timeout};
  if (options->timeout_cb) options->timeout_cb();
  completion.completed = true;
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
  auto owner = shared_from_this();
  // Capture destructors may reenter call(); release them only after setup and send.
  auto previous_callbacks = std::make_pair(rsp_handle_, finally_);
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
  auto owner = shared_from_this();
  auto previous_callbacks = std::make_pair(rsp_handle_, finally_);
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
namespace detail {

// Request startup must finish before a synchronous completion resumes the coroutine.
// If it throws after completing the request, deliver only the exception.
template <typename Complete>
struct co_call_completion {
  explicit co_call_completion(Complete complete) : complete(std::move(complete)) {}

  void finish(finally_t status) {
    if (completed) return;
    completed = true;
    type = status;
    if (!initiating) complete(nullptr, status);
  }

  void start(request& pending) {
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
    try {
      pending.call();
    } catch (...) {
      initiating = false;
      completed = true;
      complete(std::current_exception(), type);
      return;
    }
#else
    pending.call();
#endif
    initiating = false;
    if (completed) complete(nullptr, type);
  }

  Complete complete;
  bool initiating = true;
  bool completed = false;
  finally_t type = finally_t::rpc_not_ready;
};

}  // namespace detail

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
  co_return co_await asio::async_compose<decltype(asio::use_awaitable), void(std::exception_ptr, result<R>)>(
      [pending = owner.get(), &executor](auto& self) mutable {
        auto keeper = pending->shared_from_this();
        auto previous_callbacks = std::make_pair(pending->rsp_handle_, pending->finally_);
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
        auto complete = [executor, response, slot, self = std::move(self_sp)](std::exception_ptr error, finally_t type) mutable {
          if (!self) return;
          slot.clear();
          // The executor may resume later, after this request has been reused.
          asio::dispatch(executor, [self = std::move(self), response = std::move(*response), error, type]() mutable {
            self->complete(error, {type, response ? std::move(*response) : R{}});
          });
        };
        auto completion = std::make_shared<detail::co_call_completion<decltype(complete)>>(std::move(complete));
        pending->rsp([response, completion](R data, finally_t) {
          if (!completion->completed) response->reset(new R(std::move(data)));
        });
        pending->finally([completion](finally_t type) { completion->finish(type); });
        completion->start(*pending);
      },
      asio::use_awaitable);
}

template <typename R, typename std::enable_if<std::is_same<R, void>::value, int>::type>
asio::awaitable<result<R>> request::co_call_impl(request_s owner) {
  if (owner->active_) co_return result<R>{finally_t::busy};
  auto executor = co_await asio::this_coro::executor;
  co_return co_await asio::async_compose<decltype(asio::use_awaitable), void(std::exception_ptr, result<R>)>(
      [pending = owner.get(), &executor](auto& self) mutable {
        auto keeper = pending->shared_from_this();
        auto previous_callbacks = std::make_pair(pending->rsp_handle_, pending->finally_);
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
        auto complete = [executor, slot, self = std::move(self_sp)](std::exception_ptr error, finally_t type) mutable {
          if (!self) return;
          slot.clear();
          asio::dispatch(executor, [self = std::move(self), error, type] {
            self->complete(error, {type});
          });
        };
        auto completion = std::make_shared<detail::co_call_completion<decltype(complete)>>(std::move(complete));
        pending->finally([completion](finally_t type) { completion->finish(type); });
        completion->start(*pending);
      },
      asio::use_awaitable);
}
#endif

}  // namespace rpc_core
