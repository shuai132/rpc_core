#pragma once

#include <cassert>
#include <memory>
#include <utility>

#ifndef RPC_CORE_FEATURE_DISABLE_FUTURE
#include <future>
#endif

// config
#include "config.hpp"

// include
#include "detail/callable/callable.hpp"
#include "detail/msg_wrapper.hpp"
#include "detail/noncopyable.hpp"
#include "serialize.hpp"

namespace rpc_core {

class request : detail::noncopyable, public std::enable_shared_from_this<request> {
  friend class rpc;

 public:
  using request_s = std::shared_ptr<request>;
  using request_w = std::weak_ptr<request>;

  struct rpc_proto {
    virtual ~rpc_proto() = default;
    virtual seq_type make_seq() = 0;
    virtual void send_request(const request_s&) = 0;
    virtual bool is_ready() const = 0;
  };
  using send_proto_s = std::shared_ptr<rpc_proto>;
  using send_proto_w = std::weak_ptr<rpc_proto>;

  struct dispose_proto {
    virtual ~dispose_proto() = default;
    virtual void add(const request_s& request) = 0;
    virtual void remove(const request_s& request) = 0;
  };

  enum class finally_t : int {
    normal = 0,
    no_need_rsp = 1,
    timeout = 2,
    canceled = 3,
    rpc_expired = 4,
    rpc_not_ready = 5,
    rsp_serialize_error = 6,
    no_such_cmd = 7,
  };
  static inline const char* finally_t_str(finally_t t) {
    switch (t) {
      case finally_t::normal:
        return "normal";
      case finally_t::no_need_rsp:
        return "no_need_rsp";
      case finally_t::timeout:
        return "timeout";
      case finally_t::canceled:
        return "canceled";
      case finally_t::rpc_expired:
        return "rpc_expired";
      case finally_t::rpc_not_ready:
        return "rpc_not_ready";
      case finally_t::rsp_serialize_error:
        return "rsp_serialize_error";
      case finally_t::no_such_cmd:
        return "no_such_cmd";
      default:
        return "unknown";
    }
  }

 public:
  template <typename... Args>
  static request_s create(Args&&... args) {
    auto r = std::shared_ptr<request>(new request(std::forward<Args>(args)...), [](request* p) {
      delete p;
    });
    r->init();
    return r;
  }

 public:
  std::shared_ptr<request> cmd(cmd_type cmd) {
    cmd_ = std::move(cmd);
    return shared_from_this();
  }

  template <typename T>
  request_s msg(T&& message) {
    this->payload_ = serialize(std::forward<T>(message));
    return shared_from_this();
  }

  template <typename F, typename std::enable_if<callable_traits<F>::argc, int>::type = 0>
  request_s rsp(RPC_CORE_MOVE_PARAM(F) cb) {
    using T = detail::remove_cvref_t<typename callable_traits<F>::template argument_type<0>>;

    need_rsp_ = true;
    auto self = shared_from_this();
    this->rsp_handle_ = [this, RPC_CORE_MOVE_LAMBDA(cb)](detail::msg_wrapper msg) {
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

  template <typename F, typename std::enable_if<!callable_traits<F>::argc, int>::type = 0>
  request_s rsp(RPC_CORE_MOVE_PARAM(F) cb) {
    need_rsp_ = true;
    auto self = shared_from_this();
    this->rsp_handle_ = [this, RPC_CORE_MOVE_LAMBDA(cb)](const detail::msg_wrapper& msg) {
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
  std::shared_ptr<request> finally(std::function<void(finally_t)> finally) {
    finally_ = std::move(finally);
    return shared_from_this();
  }

  std::shared_ptr<request> finally(RPC_CORE_MOVE_PARAM(std::function<void()>) finally) {
    finally_ = [RPC_CORE_MOVE_LAMBDA(finally)](finally_t t) {
      RPC_CORE_UNUSED(t);
      finally();
    };
    return shared_from_this();
  }

  void call(const send_proto_s& rpc = nullptr) {
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
    r->send_request(self_keeper_);
    if (!need_rsp_) {
      on_finish(finally_t::no_need_rsp);
    }
  }

  request_s ping() {
    is_ping_ = true;
    return shared_from_this();
  }

  std::shared_ptr<request> timeout_ms(uint32_t timeout_ms) {
    timeout_ms_ = timeout_ms;
    return shared_from_this();
  }

  /**
   * timeout callback for wait `rsp`
   */
  std::shared_ptr<request> timeout(RPC_CORE_MOVE_PARAM(std::function<void()>) timeout_cb) {
    timeout_cb_ = [this, RPC_CORE_MOVE_LAMBDA(timeout_cb)] {
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

  request_s add_to(dispose_proto& dispose) {
    auto self = shared_from_this();
    dispose.add(self);
    return self;
  }

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
   * Force to ignore `rsp` callback.
   */
  request_s disable_rsp() {
    need_rsp_ = false;
    return shared_from_this();
  }

  std::shared_ptr<request> rpc(send_proto_w rpc) {
    rpc_ = std::move(rpc);
    return shared_from_this();
  }

  send_proto_w rpc() {
    return rpc_;
  }

  bool canceled() const {
    return canceled_;
  }

  std::shared_ptr<request> canceled(bool canceled) {
    canceled_ = canceled;
    return shared_from_this();
  }

#ifndef RPC_CORE_FEATURE_DISABLE_FUTURE
  /**
   * Future pattern
   * It is not recommended to use blocking interfaces unless you are very clear about what you are doing, as it is easy to cause deadlock.
   */
  template <typename T>
  struct future_ret;

  template <typename R, typename std::enable_if<!std::is_same<R, void>::value, int>::type = 0>
  std::future<future_ret<R>> future(const send_proto_s& rpc = nullptr);

  template <typename R, typename std::enable_if<std::is_same<R, void>::value, int>::type = 0>
  std::future<future_ret<void>> future(const send_proto_s& rpc = nullptr);
#endif

 private:
  explicit request(const send_proto_s& rpc = nullptr) : rpc_(rpc) {
    RPC_CORE_LOGD("request: %p", this);
  }
  ~request() {
    RPC_CORE_LOGD("~request: %p", this);
  }

 private:
  void init() {
    timeout(nullptr);
  }

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
  send_proto_w rpc_;
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

#ifndef RPC_CORE_FEATURE_DISABLE_FUTURE
template <typename T>
struct request::future_ret {
  finally_t type;
  T data;
};

template <>
struct request::future_ret<void> {
  finally_t type;
};

template <typename R, typename std::enable_if<!std::is_same<R, void>::value, int>::type>
std::future<request::future_ret<R>> request::future(const request::send_proto_s& rpc) {
  auto promise = std::make_shared<std::promise<future_ret<R>>>();
  rsp([promise](R r) {
    promise->set_value({finally_t::normal, std::move(r)});
  });
  finally([promise](finally_t type) {
    if (type != finally_t::normal) {
      promise->set_value({type, {}});
    }
  });
  call(rpc);
  return promise->get_future();
}

template <typename R, typename std::enable_if<std::is_same<R, void>::value, int>::type>
std::future<request::future_ret<void>> request::future(const request::send_proto_s& rpc) {
  auto promise = std::make_shared<std::promise<request::future_ret<void>>>();
  rsp([promise] {
    promise->set_value({request::finally_t::normal});
  });
  finally([promise](request::finally_t type) {
    if (type != request::finally_t::normal) {
      promise->set_value({type});
    }
  });
  call(rpc);
  return promise->get_future();
}
#endif

using request_s = request::request_s;
using request_w = request::request_w;
using finally_t = request::finally_t;

}  // namespace rpc_core
