#pragma once

#include <memory>
#include <utility>

#include "Connection.hpp"
#include "Request.hpp"
#include "detail/MsgDispatcher.hpp"
#include "detail/callable/callable.hpp"
#include "detail/noncopyable.hpp"

namespace RpcCore {

using namespace internal;

class Rpc : noncopyable, public std::enable_shared_from_this<Rpc>, public Request::RpcProto {
 public:
  using TimeoutCb = internal::MsgDispatcher::TimeoutCb;

 public:
  template <typename... Args>
  static std::shared_ptr<Rpc> create(Args&&... args) {
    return std::shared_ptr<Rpc>(new Rpc(std::forward<Args>(args)...), [](Rpc* p) {
      delete p;
    });
  }

 private:
  explicit Rpc(std::shared_ptr<Connection> conn = std::make_shared<Connection>()) : conn_(conn), dispatcher_(std::move(conn)) {}

  ~Rpc() override {
    RpcCore_LOGD("~Rpc");
  };

 public:
  inline std::shared_ptr<Connection> getConn() const {
    return conn_;
  }

  inline void setTimer(internal::MsgDispatcher::TimerImpl timerImpl) {
    dispatcher_.setTimerImpl(std::move(timerImpl));
  }

 public:
  template <typename F>
  void subscribe(const CmdType& cmd, RpcCore_MOVE_PARAM(F) handle) {
    constexpr bool F_ReturnIsEmpty = std::is_void<typename callable_traits<F>::return_type>::value;
    constexpr bool F_ParamIsEmpty = callable_traits<F>::argc == 0;
    subscribe_help<F, F_ReturnIsEmpty, F_ParamIsEmpty>()(cmd, std::move(handle), &dispatcher_);
  }

  inline void unsubscribe(const CmdType& cmd) {
    dispatcher_.unsubscribeCmd(cmd);
  }

 public:
  inline SRequest createRequest() {
    return Request::create(shared_from_this());
  }

  inline SRequest cmd(CmdType cmd) {
    return createRequest()->cmd(std::move(cmd));
  }

  inline SRequest ping(std::string payload = "") {
    return createRequest()->ping()->msg(String(std::move(payload)));
  }

 public:
  SeqType makeSeq() override {
    return seq_++;
  }

  void sendRequest(const SRequest& request) override {
    const bool needRsp = request->needRsp();
    if (needRsp) {
      dispatcher_.subscribeRsp(request->seq(), request->rspHandle(), request->timeoutCb_, request->timeoutMs(), request->isPing_);
    }
    auto msg = MsgWrapper::MakeCmd(request->cmd(), request->seq(), request->isPing_, needRsp, request->payload());
    conn_->sendPackageImpl(Coder::serialize(msg));
  }

 private:
  template <typename F, bool F_ReturnIsEmpty, bool F_ParamIsEmpty>
  struct subscribe_help;

  template <typename F>
  struct subscribe_help<F, false, false> {
    void operator()(const CmdType& cmd, RpcCore_MOVE_PARAM(F) handle, MsgDispatcher* dispatcher) {
      dispatcher->subscribeCmd(cmd, [RpcCore_MOVE_LAMBDA(handle)](const MsgWrapper& msg) {
        using F_Param = detail::remove_cvref_t<typename callable_traits<F>::template argument_type<0>>;
        static_assert(std::is_base_of<Message, F_Param>::value, "function param type should be base of `Message`");

        using F_Return = detail::remove_cvref_t<typename callable_traits<F>::return_type>;
        static_assert(std::is_base_of<Message, F_Return>::value, "function return type should be base of `Message`");

        auto r = msg.unpackAs<F_Param>();
        F_Return ret;
        if (r.first) {
          ret = handle(std::move(r.second));
        }
        return MsgWrapper::MakeRsp(msg.seq, ret, r.first);
      });
    }
  };

  template <typename F>
  struct subscribe_help<F, true, false> {
    void operator()(const CmdType& cmd, RpcCore_MOVE_PARAM(F) handle, MsgDispatcher* dispatcher) {
      dispatcher->subscribeCmd(cmd, [RpcCore_MOVE_LAMBDA(handle)](const MsgWrapper& msg) {
        using F_Param = detail::remove_cvref_t<typename callable_traits<F>::template argument_type<0>>;
        static_assert(std::is_base_of<Message, F_Param>::value, "function param type should be base of `Message`");

        auto r = msg.unpackAs<F_Param>();
        if (r.first) {
          handle(std::move(r.second));
        }
        return MsgWrapper::MakeRsp(msg.seq, VOID, r.first);
      });
    }
  };

  template <typename F>
  struct subscribe_help<F, false, true> {
    void operator()(const CmdType& cmd, RpcCore_MOVE_PARAM(F) handle, MsgDispatcher* dispatcher) {
      dispatcher->subscribeCmd(cmd, [RpcCore_MOVE_LAMBDA(handle)](const MsgWrapper& msg) {
        using F_Return = typename callable_traits<F>::return_type;
        static_assert(std::is_base_of<Message, F_Return>::value, "function return type should be base of `Message`");

        F_Return ret = handle();
        return MsgWrapper::MakeRsp(msg.seq, ret, true);
      });
    }
  };

  template <typename F>
  struct subscribe_help<F, true, true> {
    void operator()(const CmdType& cmd, RpcCore_MOVE_PARAM(F) handle, MsgDispatcher* dispatcher) {
      dispatcher->subscribeCmd(cmd, [RpcCore_MOVE_LAMBDA(handle)](const MsgWrapper& msg) {
        handle();
        return MsgWrapper::MakeRsp(msg.seq, VOID, true);
      });
    }
  };

 private:
  std::shared_ptr<Connection> conn_;
  internal::MsgDispatcher dispatcher_;
  SeqType seq_{0};
};

}  // namespace RpcCore
