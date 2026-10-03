#pragma once

#include <memory>
#include <utility>

#ifdef RPC_CORE_FEATURE_FUTURE
#include <future>
#endif

// config
#include "config.hpp"

#ifdef RPC_CORE_FEATURE_CO_ASIO
#include <asio.hpp>
#endif

// include
#include "detail/callable/callable.hpp"
#include "detail/msg_wrapper.hpp"
#include "detail/noncopyable.hpp"
#include "detail/shared_function.hpp"
#include "result.hpp"
#include "serialize.hpp"

namespace rpc_core {

class rpc;
using rpc_s = std::shared_ptr<rpc>;
using rpc_w = std::weak_ptr<rpc>;
class dispose;

class request : detail::noncopyable, public std::enable_shared_from_this<request> {
  friend class rpc;

 public:
  using request_s = std::shared_ptr<request>;
  using request_w = std::weak_ptr<request>;

 public:
  template <typename... Args>
  static request_s create(Args&&... args) {
    struct helper : public request {
      explicit helper(Args&&... a) : request(std::forward<Args>(a)...) {}
    };
    auto r = std::make_shared<helper>(std::forward<Args>(args)...);
    r->timeout(nullptr);
    return r;
  }

 public:
  request_s cmd(cmd_type cmd) {
    cmd_ = std::move(cmd);
    return shared_from_this();
  }

  template <typename T>
  request_s msg(T&& message) {
    auto self = shared_from_this();
    this->payload_ = serialize(std::forward<T>(message));
    return self;
  }

  template <typename F, typename std::enable_if<callable_traits<F>::argc == 2, int>::type = 0>
  request_s rsp(F cb) {
    using T = detail::remove_cvref_t<typename callable_traits<F>::template argument_type<0>>;

    need_rsp_ = true;
    auto self = shared_from_this();
    request_w weak = self;
    this->rsp_handle_ = [weak, cb = std::move(cb)](detail::msg_wrapper msg) mutable {
      auto self = weak.lock();
      if (!self) return true;

      if (self->canceled_) {
        self->on_finish(finally_t::canceled);
        return true;
      }

      if (msg.type & detail::msg_wrapper::msg_type::no_such_cmd) {
        self->on_finish(finally_t::no_such_cmd);
        return true;
      }

      const auto call_id = self->call_id_;
      const auto seq = self->seq_;
      attempt_callback_scope decoding{*self, call_id, seq, finally_t::rsp_serialize_error};
      auto rsp = msg.unpack_as<T>();
      decoding.completed = true;
      // Custom deserialization can cancel and reuse this request.
      if (!self->matches_attempt(call_id, seq)) return true;
      if (rsp.first) {
        self->finish_response(finally_t::normal, [&] { cb(std::move(rsp.second), finally_t::normal); });
        return true;
      } else {
        self->finish_response(finally_t::rsp_serialize_error, [&] { cb({}, finally_t::rsp_serialize_error); });
        return false;
      }
    };
    return self;
  }

  template <typename F, typename std::enable_if<callable_traits<F>::argc == 1, int>::type = 0>
  request_s rsp(F cb) {
    using T = detail::remove_cvref_t<typename callable_traits<F>::template argument_type<0>>;

    need_rsp_ = true;
    auto self = shared_from_this();
    request_w weak = self;
    this->rsp_handle_ = [weak, cb = std::move(cb)](detail::msg_wrapper msg) mutable {
      auto self = weak.lock();
      if (!self) return true;

      if (self->canceled_) {
        self->on_finish(finally_t::canceled);
        return true;
      }

      if (msg.type & detail::msg_wrapper::msg_type::no_such_cmd) {
        self->on_finish(finally_t::no_such_cmd);
        return true;
      }

      const auto call_id = self->call_id_;
      const auto seq = self->seq_;
      attempt_callback_scope decoding{*self, call_id, seq, finally_t::rsp_serialize_error};
      auto rsp = msg.unpack_as<T>();
      decoding.completed = true;
      // Custom deserialization can cancel and reuse this request.
      if (!self->matches_attempt(call_id, seq)) return true;
      if (rsp.first) {
        self->finish_response(finally_t::normal, [&] { cb(std::move(rsp.second)); });
        return true;
      } else {
        self->on_finish(finally_t::rsp_serialize_error);
        return false;
      }
    };
    return self;
  }

  template <typename F, typename std::enable_if<callable_traits<F>::argc == 0, int>::type = 0>
  request_s rsp(F cb) {
    need_rsp_ = true;
    auto self = shared_from_this();
    request_w weak = self;
    this->rsp_handle_ = [weak, cb = std::move(cb)](const detail::msg_wrapper& msg) mutable {
      RPC_CORE_UNUSED(msg);
      auto self = weak.lock();
      if (!self) return true;

      if (self->canceled_) {
        self->on_finish(finally_t::canceled);
        return true;
      }

      if (msg.type & detail::msg_wrapper::msg_type::no_such_cmd) {
        self->on_finish(finally_t::no_such_cmd);
        return true;
      }

      self->finish_response(finally_t::normal, [&] { cb(); });
      return true;
    };
    return self;
  }

  /**
   * One accepted call, one finally. A busy call does not invoke finally.
   * @param finally
   * @return
   */
  request_s finally(std::function<void(finally_t)> finally) {
    auto self = shared_from_this();
    finally_ = std::make_shared<std::function<void(finally_t)>>(std::move(finally));
    return self;
  }

  request_s finally(std::function<void()> finally) {
    auto self = shared_from_this();
    if (!finally) {
      finally_.reset();
      return self;
    }
    finally_ = std::make_shared<std::function<void(finally_t)>>([finally = std::move(finally)](finally_t t) mutable {
      RPC_CORE_UNUSED(t);
      finally();
    });
    return self;
  }

  /**
   * Reusable after completion or cancellation; only one call may be active.
   * Returns busy without changing the active call. Other immediate failures
   * also complete the accepted call through finally.
   */
  inline result<void> call(const rpc_s& rpc = nullptr);

  request_s ping() {
    is_ping_ = true;
    return shared_from_this();
  }

  request_s timeout_ms(uint32_t timeout_ms) {
    timeout_ms_ = timeout_ms;
    return shared_from_this();
  }

  /**
   * timeout callback for wait `rsp`
   */
  request_s timeout(std::function<void()> timeout_cb) {
    auto self = shared_from_this();
    timeout_cb_ = std::move(timeout_cb);
    return self;
  }

  inline request_s add_to(dispose& dispose);

  inline request_s cancel();

  request_s reset_cancel() {
    canceled(false);
    return shared_from_this();
  }

  /**
   * Automatic retry times after timeout
   * -1 means retry indefinitely, 0 means no retry, >0 means the number of retries.
   */
  request_s retry(int count) {
    retry_count_ = count;
    return shared_from_this();
  }

  /**
   * Force ignoring `rsp` callback.
   */
  request_s disable_rsp() {
    need_rsp_ = false;
    return shared_from_this();
  }

  request_s enable_rsp() {
    if (!rsp_handle_) return mark_need_rsp();
    need_rsp_ = true;
    return shared_from_this();
  }

  /**
   * Mark wait peer's response for finally
   * if no rsp handle, rpc call will finish immediately with FinallyType::NoNeedRsp
   * template is used for suppress warnings on some old compilers(rsp callable_traits)
   */
  template <typename _ = void>
  request_s mark_need_rsp() {
    return rsp([] {});
  }

  request_s rpc(rpc_w rpc) {
    auto self = shared_from_this();
    rpc_ = std::move(rpc);
    return self;
  }

  rpc_w rpc() {
    return rpc_;
  }

  bool is_canceled() const {
    return canceled_;
  }

  request_s canceled(bool canceled) {
    if (canceled) return cancel();
    canceled_ = false;
    return shared_from_this();
  }

#ifdef RPC_CORE_FEATURE_FUTURE
  /**
   * Future pattern
   * It is not recommended to use blocking interfaces unless you are very clear about what you are doing, as it is easy to cause deadlock.
   */
  template <typename R, typename std::enable_if<!std::is_same<R, void>::value, int>::type = 0>
  std::future<result<R>> future(const rpc_s& rpc = nullptr);

  template <typename R = void, typename std::enable_if<std::is_same<R, void>::value, int>::type = 0>
  std::future<result<void>> future(const rpc_s& rpc = nullptr);
#endif

#ifdef RPC_CORE_FEATURE_CO_ASIO
  template <typename R, typename std::enable_if<!std::is_same<R, void>::value, int>::type = 0>
  asio::awaitable<result<R>> co_call();

  template <typename R = void, typename std::enable_if<std::is_same<R, void>::value, int>::type = 0>
  asio::awaitable<result<R>> co_call();
#endif

#ifdef RPC_CORE_FEATURE_CO_CUSTOM
  template <typename R, typename std::enable_if<!std::is_same<R, void>::value, int>::type = 0>
  RPC_CORE_FEATURE_CO_CUSTOM_R RPC_CORE_FEATURE_CO_CUSTOM();

  template <typename R = void, typename std::enable_if<std::is_same<R, void>::value, int>::type = 0>
  RPC_CORE_FEATURE_CO_CUSTOM_R RPC_CORE_FEATURE_CO_CUSTOM();
#endif

 private:
#ifdef RPC_CORE_FEATURE_CO_ASIO
  template <typename R, typename std::enable_if<!std::is_same<R, void>::value, int>::type = 0>
  static asio::awaitable<result<R>> co_call_impl(request_s owner);

  template <typename R, typename std::enable_if<std::is_same<R, void>::value, int>::type = 0>
  static asio::awaitable<result<R>> co_call_impl(request_s owner);
#endif

 private:
  explicit request(const rpc_s& rpc = nullptr) : rpc_(rpc) {
    RPC_CORE_LOGD("request: %p", this);
  }
  ~request() {
    RPC_CORE_LOGD("~request: %p", this);
  }

 private:
  inline result<void> send_attempt();
  inline void on_timeout();

  // User code may unwind after this attempt stops accepting responses.
  struct attempt_callback_scope {
    request& owner;
    uint32_t call_id;
    seq_type seq;
    finally_t failure;
    bool completed = false;
    ~attempt_callback_scope() noexcept(false) {
      if (!completed && owner.matches_attempt(call_id, seq)) {
        owner.on_finish(failure);
      }
    }
  };

  bool matches_attempt(uint32_t call_id, seq_type seq) const {
    return active_ && call_id_ == call_id && seq_ == seq;
  }

  void on_finish(finally_t type) {
    finish_response(type, [] {});
  }

  template <typename F>
  void finish_response(finally_t type, F&& response) {
    if (!active_) return;
    auto completed = std::move(active_);
    completed->completion = type;
    RPC_CORE_LOGD("on_finish: cmd:%s type:%s", completed->cmd.c_str(), finally_t_str(type));
    auto keeper = std::move(self_keeper_);
    // Run finally before releasing this call, including during response unwinding.
    struct finally_scope {
      std::shared_ptr<std::function<void(finally_t)>> callback;
      finally_t type;
      ~finally_scope() noexcept(false) {
        if (callback && *callback) (*callback)(type);
      }
    } completion{completed->finally, type};
    // Detach this completion before user code can start another call.
    std::forward<F>(response)();
  }

 private:
  // Builder changes configure the next call; active calls retain their options.
  struct call_options {
    rpc_w rpc;
    cmd_type cmd;
    std::string payload;
    bool need_rsp;
    detail::shared_function<bool(detail::msg_wrapper)> rsp_handle;
    uint32_t timeout_ms;
    detail::shared_function<void()> timeout_cb;
    std::shared_ptr<std::function<void(finally_t)>> finally;
    bool is_ping;
    finally_t completion = finally_t::normal;
  };
  std::shared_ptr<call_options> active_;
  int retries_remaining_ = 0;
  rpc_w rpc_;
  request_s self_keeper_;
  seq_type seq_{};
  uint32_t call_id_ = 0;
  cmd_type cmd_;
  std::string payload_;
  bool need_rsp_ = false;
  bool canceled_ = false;
  detail::shared_function<bool(detail::msg_wrapper)> rsp_handle_;
  uint32_t timeout_ms_ = 3000;
  detail::shared_function<void()> timeout_cb_;
  std::shared_ptr<std::function<void(finally_t)>> finally_;
  int retry_count_ = 0;
  bool is_ping_ = false;
};

using request_s = request::request_s;
using request_w = request::request_w;

}  // namespace rpc_core
