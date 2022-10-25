#pragma once

#include <memory>
#include <utility>

#include "detail/MsgWrapper.hpp"
#include "detail/callable/callable.hpp"
#include "detail/noncopyable.hpp"

namespace RpcCore {

#define RpcCore_Request_MAKE_PROP_PUBLIC(type, name) \
 public:                                             \
  inline std::shared_ptr<Request> name(type name) {  \
    name##_ = std::move(name);                       \
    return shared_from_this();                       \
  }                                                  \
                                                     \
 private:                                            \
  type name##_;                                      \
  inline type name() { return name##_; }

#define RpcCore_Request_MAKE_PROP_PRIVATE(type, name) \
 private:                                             \
  inline type name() { return name##_; }              \
  inline std::shared_ptr<Request> name(type name) {   \
    name##_ = std::move(name);                        \
    return shared_from_this();                        \
  }                                                   \
                                                      \
 private:                                             \
  type name##_;

struct Request : detail::noncopyable, public std::enable_shared_from_this<Request> {
  using SRequest = std::shared_ptr<Request>;
  using WRequest = std::weak_ptr<Request>;

  struct RpcProto {
    virtual ~RpcProto() = default;
    virtual SeqType makeSeq() = 0;
    virtual void sendRequest(const SRequest&) = 0;
  };
  using SSendProto = std::shared_ptr<RpcProto>;
  using WSendProto = std::weak_ptr<RpcProto>;

  struct DisposeProto {
    virtual ~DisposeProto() = default;
    virtual void add(const SRequest& request) = 0;
    virtual void remove(const SRequest& request) = 0;
  };

  enum class FinallyType {
    NORMAL,
    TIMEOUT,
    CANCELED,
    RPC_EXPIRED,
    NO_NEED_RSP,
  };

 public:
  template <typename... Args>
  static SRequest create(Args&&... args) {
    auto request = std::shared_ptr<Request>(new Request(std::forward<Args>(args)...), [](Request* p) {
      delete p;
    });
    request->init();
    return request;
  }

  RpcCore_Request_MAKE_PROP_PUBLIC(CmdType, cmd);
  RpcCore_Request_MAKE_PROP_PUBLIC(uint32_t, timeoutMs);
  RpcCore_Request_MAKE_PROP_PUBLIC(std::function<void(FinallyType)>, finally);

 public:
  SRequest ping() {
    isPing_ = true;
    return shared_from_this();
  }

  SRequest msg(const Message& message) {
    this->payload(message.serialize());
    return shared_from_this();
  }

  template <typename F>
  SRequest rsp(RpcCore_MOVE_PARAM(F) cb) {
    using T = detail::remove_cvref_t<typename callable_traits<F>::template argument_type<0>>;
    static_assert(std::is_base_of<Message, T>::value, "function param should be base of `Message`");

    needRsp_ = true;
    auto self = shared_from_this();
    this->rspHandle_ = [this, RpcCore_MOVE_LAMBDA(cb), self](detail::MsgWrapper msg) {
      if (canceled()) {
        onFinish(FinallyType::CANCELED);
        return true;
      }

      auto rsp = msg.unpackAs<T>();
      if (rsp.first) {
        cb(std::move(rsp.second));
        onFinish(FinallyType::NORMAL);
        return true;
      }
      return false;
    };
    return self;
  }

  void call(const SSendProto& rpc = nullptr) {
    if (canceled()) return;

    auto self = shared_from_this();
    if (rpc) {
      rpc_ = rpc;
    } else if (rpc_.expired()) {
      onFinish(FinallyType::RPC_EXPIRED);
      return;
    }
    waitingRsp_ = true;
    auto r = rpc_.lock();
    seq_ = r->makeSeq();
    r->sendRequest(self);
    if (!needRsp_) {
      onFinish(FinallyType::NO_NEED_RSP);
    }
  }

  std::shared_ptr<Request> timeout(RpcCore_MOVE_PARAM(std::function<void()>) timeoutCb) {
    timeoutCb_ = [this, RpcCore_MOVE_LAMBDA(timeoutCb)] {
      if (timeoutCb) {
        timeoutCb();
      }
      if (retryCount_ == -1) {
        call();
      } else if (retryCount_ > 0) {
        retryCount_--;
        call();
      } else {
        onFinish(FinallyType::TIMEOUT);
      }
    };
    return shared_from_this();
  }

  SRequest addTo(DisposeProto& dispose) {
    auto self = shared_from_this();
    dispose.add(self);
    return self;
  }

  SRequest cancel() {
    canceled(true);
    if (waitingRsp_) {
      onFinish(FinallyType::CANCELED);
    }
    return shared_from_this();
  }

  SRequest unCancel() {
    canceled(false);
    return shared_from_this();
  }

  /**
   * 超时后自动重试次数
   * -1为一直重试 0为不重试 >0为重试次数
   */
  SRequest retry(int count) {
    retryCount_ = count;
    return shared_from_this();
  }

  /**
   * 强制忽略rsp回调
   */
  SRequest disableRsp() {
    needRsp_ = false;
    return shared_from_this();
  }

 private:
  explicit Request(const SSendProto& rpc = nullptr) : rpc_(rpc) {}  // NOLINT(cppcoreguidelines-pro-type-member-init)
  ~Request() {
    RpcCore_LOGD("~Request: cmd:%s, %p", cmd_.c_str(), this);
  }

 private:
  void init() {
    needRsp(false);
    canceled(false);
    timeoutMs(3000);
    timeout(nullptr);
  }

  void onFinish(FinallyType type) {
    assert(waitingRsp_);
    waitingRsp_ = false;
    RpcCore_LOGD("onFinish: cmd:%s, type: %d, %p", cmd().c_str(), (int)type, this);
    finallyType_ = type;
    if (finally_) {
      finally_(finallyType_);
    }
  }

  friend class Dispose;
  friend class Rpc;
  RpcCore_Request_MAKE_PROP_PRIVATE(WSendProto, rpc);
  RpcCore_Request_MAKE_PROP_PRIVATE(SeqType, seq);
  RpcCore_Request_MAKE_PROP_PRIVATE(std::function<bool(detail::MsgWrapper)>, rspHandle);
  RpcCore_Request_MAKE_PROP_PRIVATE(std::string, payload);
  RpcCore_Request_MAKE_PROP_PRIVATE(bool, needRsp);
  RpcCore_Request_MAKE_PROP_PRIVATE(bool, canceled);

 private:
  FinallyType finallyType_;
  std::function<void()> timeoutCb_;

 private:
  int retryCount_ = 0;
  bool waitingRsp_ = false;
  bool isPing_ = false;
};

using SRequest = Request::SRequest;
using WRequest = Request::WRequest;
using FinishType = Request::FinallyType;

}  // namespace RpcCore
