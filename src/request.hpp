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

namespace RPC_CORE_NAMESPACE {

#define RPC_CORE_DETAIL_REQUEST_MAKE_PROP_PUBLIC(type, name) \
 public:                                                     \
  inline std::shared_ptr<request> name(type name) {          \
    name##_ = std::move(name);                               \
    return shared_from_this();                               \
  }                                                          \
                                                             \
 private:                                                    \
  type name##_;                                              \
  inline type name() {                                       \
    return name##_;                                          \
  }

#define RPC_CORE_DETAIL_REQUEST_MAKE_PROP_PRIVATE(type, name) \
 private:                                                     \
  inline type name() {                                        \
    return name##_;                                           \
  }                                                           \
  inline std::shared_ptr<request> name(type name) {           \
    name##_ = std::move(name);                                \
    return shared_from_this();                                \
  }                                                           \
                                                              \
 private:                                                     \
  type name##_;

class request : detail::noncopyable, public std::enable_shared_from_this<request> {
  friend class rpc;

 public:
  using request_s = std::shared_ptr<request>;
  using request_w = std::weak_ptr<request>;

  struct rpc_proto {
    virtual ~rpc_proto() = default;
    virtual seq_type make_seq() = 0;
    virtual void send_request(const request_s&) = 0;
  };
  using send_proto_s = std::shared_ptr<rpc_proto>;
  using send_proto_w = std::weak_ptr<rpc_proto>;

  struct dispose_proto {
    virtual ~dispose_proto() = default;
    virtual void add(const request_s& request) = 0;
    virtual void remove(const request_s& request) = 0;
  };

  enum class finally_t {
    normal,
    timeout,
    canceled,
    rpc_expired,
    no_need_rsp,
  };

 public:
  template <typename... Args>
  static request_s create(Args&&... args) {
    auto request = std::shared_ptr<RPC_CORE_NAMESPACE::request>(new RPC_CORE_NAMESPACE::request(std::forward<Args>(args)...),
                                                                [](RPC_CORE_NAMESPACE::request* p) {
                                                                  delete p;
                                                                });
    request->init();
    return request;
  }

  RPC_CORE_DETAIL_REQUEST_MAKE_PROP_PUBLIC(cmd_type, cmd);
  RPC_CORE_DETAIL_REQUEST_MAKE_PROP_PUBLIC(uint32_t, timeout_ms);
  RPC_CORE_DETAIL_REQUEST_MAKE_PROP_PUBLIC(std::function<void(finally_t)>, finally);

 public:
  request_s ping() {
    is_ping_ = true;
    return shared_from_this();
  }

  template <class T>
  request_s msg(const T& message) {
    this->payload(serialize(message));
    return shared_from_this();
  }

  template <class T>
  request_s msg(T&& message) {
    this->payload(serialize(std::forward<T>(message)));
    return shared_from_this();
  }

  template <typename F, typename std::enable_if<callable_traits<F>::argc, int>::type = 0>
  request_s rsp(RPC_CORE_MOVE_PARAM(F) cb) {
    using T = detail::remove_cvref_t<typename callable_traits<F>::template argument_type<0>>;

    need_rsp_ = true;
    auto self = shared_from_this();
    this->rsp_handle_ = [this, RPC_CORE_MOVE_LAMBDA(cb), self](detail::msg_wrapper msg) {
      if (canceled()) {
        on_finish(finally_t::canceled);
        return true;
      }

      auto rsp = msg.unpack_as<T>();
      if (rsp.first) {
        cb(std::move(rsp.second));
        on_finish(finally_t::normal);
        return true;
      }
      return false;
    };
    return self;
  }

  template <typename F, typename std::enable_if<!callable_traits<F>::argc, int>::type = 0>
  request_s rsp(RPC_CORE_MOVE_PARAM(F) cb) {
    need_rsp_ = true;
    auto self = shared_from_this();
    this->rsp_handle_ = [this, RPC_CORE_MOVE_LAMBDA(cb), self](const detail::msg_wrapper& msg) {
      if (canceled()) {
        on_finish(finally_t::canceled);
        return true;
      }
      cb();
      on_finish(finally_t::normal);
      return true;
    };
    return self;
  }

  void call(const send_proto_s& rpc = nullptr) {
    if (canceled()) return;

    auto self = shared_from_this();
    if (rpc) {
      rpc_ = rpc;
    } else if (rpc_.expired()) {
      on_finish(finally_t::rpc_expired);
      return;
    }
    waiting_rsp_ = true;
    auto r = rpc_.lock();
    seq_ = r->make_seq();
    r->send_request(self);
    if (!need_rsp_) {
      on_finish(finally_t::no_need_rsp);
    }
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

  std::shared_ptr<request> timeout(RPC_CORE_MOVE_PARAM(std::function<void()>) timeoutCb) {
    timeoutCb_ = [this, RPC_CORE_MOVE_LAMBDA(timeoutCb)] {
      if (timeoutCb) {
        timeoutCb();
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
    if (waiting_rsp_) {
      on_finish(finally_t::canceled);
    }
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
  request_s disableRsp() {
    need_rsp_ = false;
    return shared_from_this();
  }

 private:
  explicit request(const send_proto_s& rpc = nullptr) : rpc_(rpc) {}  // NOLINT(cppcoreguidelines-pro-type-member-init)
  ~request() {
    RPC_CORE_LOGD("~request: cmd:%s, %p", cmd_.c_str(), this);
  }

 private:
  void init() {
    need_rsp(false);
    canceled(false);
    timeout_ms(3000);
    timeout(nullptr);
  }

  void on_finish(finally_t type) {
    assert(waiting_rsp_);
    waiting_rsp_ = false;
    RPC_CORE_LOGD("on_finish: cmd:%s, type: %d, %p", cmd().c_str(), (int)type, this);
    finallyType_ = type;
    if (finally_) {
      finally_(finallyType_);
    }
  }

  RPC_CORE_DETAIL_REQUEST_MAKE_PROP_PRIVATE(send_proto_w, rpc);
  RPC_CORE_DETAIL_REQUEST_MAKE_PROP_PRIVATE(seq_type, seq);
  RPC_CORE_DETAIL_REQUEST_MAKE_PROP_PRIVATE(std::function<bool(detail::msg_wrapper)>, rsp_handle);
  RPC_CORE_DETAIL_REQUEST_MAKE_PROP_PRIVATE(std::string, payload);
  RPC_CORE_DETAIL_REQUEST_MAKE_PROP_PRIVATE(bool, need_rsp);
  RPC_CORE_DETAIL_REQUEST_MAKE_PROP_PRIVATE(bool, canceled);

 private:
  finally_t finallyType_;
  std::function<void()> timeoutCb_;

 private:
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

}  // namespace RPC_CORE_NAMESPACE
