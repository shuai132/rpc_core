#pragma once

#include <memory>
#include <utility>

#ifdef RPC_CORE_FEATURE_FUTURE
#include <future>
#endif

// config
#include "config.hpp"

// include
#include "detail/callable/callable.hpp"
#include "detail/msg_wrapper.hpp"
#include "detail/noncopyable.hpp"
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
    this->payload_ = serialize(std::forward<T>(message));
    return shared_from_this();
  }

  template <typename F, typename std::enable_if<callable_traits<F>::argc == 2, int>::type = 0>
  request_s rsp(F cb) {
    using T = detail::remove_cvref_t<typename callable_traits<F>::template argument_type<0>>;

    need_rsp_ = true;
    auto self = shared_from_this();
    this->rsp_handle_ = [this, cb = std::move(cb)](detail::msg_wrapper msg) mutable {
      if (canceled_) {
        on_finish(finally_t::canceled);
        return true;
      }

      if (msg.type & detail::msg_wrapper::msg_type::no_such_cmd) {
        on_finish(finally_t::no_such_cmd);
        return true;
      }

      auto rsp = msg.unpack_as<T>();
      if (rsp.first) {
        cb(std::move(rsp.second), finally_t::normal);
        on_finish(finally_t::normal);
        return true;
      } else {
        cb({}, finally_t::rsp_serialize_error);
        on_finish(finally_t::rsp_serialize_error);
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
    this->rsp_handle_ = [this, cb = std::move(cb)](detail::msg_wrapper msg) mutable {
      if (canceled_) {
        on_finish(finally_t::canceled);
        return true;
      }

      if (msg.type & detail::msg_wrapper::msg_type::no_such_cmd) {
        on_finish(finally_t::no_such_cmd);
        return true;
      }

      auto rsp = msg.unpack_as<T>();
      if (rsp.first) {
        cb(std::move(rsp.second));
        on_finish(finally_t::normal);
        return true;
      } else {
        on_finish(finally_t::rsp_serialize_error);
        return false;
      }
    };
    return self;
  }

  template <typename F, typename std::enable_if<callable_traits<F>::argc == 0, int>::type = 0>
  request_s rsp(F cb) {
    need_rsp_ = true;
    auto self = shared_from_this();
    this->rsp_handle_ = [this, cb = std::move(cb)](const detail::msg_wrapper& msg) mutable {
      RPC_CORE_UNUSED(msg);
      if (canceled_) {
        on_finish(finally_t::canceled);
        return true;
      }

      if (msg.type & detail::msg_wrapper::msg_type::no_such_cmd) {
        on_finish(finally_t::no_such_cmd);
        return true;
      }

      cb();
      on_finish(finally_t::normal);
      return true;
    };
    return self;
  }

  /**
   * one call, one finally
   * @param finally
   * @return
   */
  request_s finally(std::function<void(finally_t)> finally) {
    finally_ = std::move(finally);
    return shared_from_this();
  }

  request_s finally(std::function<void()> finally) {
    finally_ = [finally = std::move(finally)](finally_t t) mutable {
      RPC_CORE_UNUSED(t);
      finally();
    };
    return shared_from_this();
  }

  inline void call(const rpc_s& rpc = nullptr);

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
    timeout_cb_ = [this, timeout_cb = std::move(timeout_cb)]() mutable {
      if (timeout_cb) {
        timeout_cb();
      }
      if (retry_count_ == -1) {
        call();
      } else if (retry_count_ > 0) {
        retry_count_--;
        call();
      } else {
        on_finish(finally_t::timeout);
      }
    };
    return shared_from_this();
  }

  inline request_s add_to(dispose& dispose);

  request_s cancel() {
    canceled(true);
    on_finish(finally_t::canceled);
    return shared_from_this();
  }

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
    rsp([] {});
    return shared_from_this();
  }

  request_s rpc(rpc_w rpc) {
    rpc_ = std::move(rpc);
    return shared_from_this();
  }

  rpc_w rpc() {
    return rpc_;
  }

  bool is_canceled() const {
    return canceled_;
  }

  request_s canceled(bool canceled) {
    canceled_ = canceled;
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
  explicit request(const rpc_s& rpc = nullptr) : rpc_(rpc) {
    RPC_CORE_LOGD("request: %p", this);
  }
  ~request() {
    RPC_CORE_LOGD("~request: %p", this);
  }

 private:
  void on_finish(finally_t type) {
    if (!waiting_rsp_) return;
    waiting_rsp_ = false;
    RPC_CORE_LOGD("on_finish: cmd:%s type:%s", cmd_.c_str(), finally_t_str(type));
    finally_type_ = type;
    if (finally_) {
      finally_(finally_type_);
    }
    self_keeper_ = nullptr;
  }

 private:
  rpc_w rpc_;
  request_s self_keeper_;
  seq_type seq_{};
  cmd_type cmd_;
  std::string payload_;
  bool need_rsp_ = false;
  bool canceled_ = false;
  std::function<bool(detail::msg_wrapper)> rsp_handle_;
  uint32_t timeout_ms_ = 3000;
  std::function<void()> timeout_cb_;
  finally_t finally_type_ = finally_t::no_need_rsp;
  std::function<void(finally_t)> finally_;
  int retry_count_ = 0;
  bool waiting_rsp_ = false;
  bool is_ping_ = false;
};

using request_s = request::request_s;
using request_w = request::request_w;

}  // namespace rpc_core
