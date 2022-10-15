#pragma once

#include <memory>
#include <utility>

#include "Connection.hpp"
#include "MsgDispatcher.hpp"
#include "Request.hpp"
#include "detail/noncopyable.hpp"

namespace RpcCore {

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
  explicit Rpc(std::shared_ptr<Connection> conn = std::make_shared<Connection>()) : conn_(conn), dispatcher_(std::move(conn)) {
    // 注册PING消息，可携带payload，会原样返回。
    dispatcher_.subscribeCmd(InnerCmd::PING, [](MsgWrapper msg) {
      msg.cmd = InnerCmd::PONG;
      auto ret = msg.unpackAs<String>();
      return MsgWrapper::MakeRsp(msg.seq, ret.second, ret.first);
    });
  }
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

  /// 注册消息
 public:
  /**
   * 注册命令 接收消息 回复消息
   * @tparam T 接收消息的类型
   * @tparam R 返回消息的类型
   * @param cmd
   * @param handle 接收T类型消息 返回R类型消息
   */
  template <typename T, typename R, RpcCore_ENSURE_TYPE_IS_MESSAGE(T), RpcCore_ENSURE_TYPE_IS_MESSAGE(R)>
  void subscribe(const CmdType& cmd, std::function<R(T&&)> handle) {
    dispatcher_.subscribeCmd(cmd, [handle](const MsgWrapper& msg) {
      auto r = msg.unpackAs<T>();
      R ret;
      if (r.first) {
        ret = handle(std::move(r.second));
      }
      return MsgWrapper::MakeRsp(msg.seq, ret, r.first);
    });
  }

  /**
   * 注册命令 接收 无回复
   * @tparam T 接收消息的类型
   * @param cmd
   * @param handle 接收T类型消息
   */
  template <typename T, RpcCore_ENSURE_TYPE_IS_MESSAGE(T)>
  void subscribe(const CmdType& cmd, RpcCore_MOVE_PARAM(std::function<void(T&&)>) handle) {
    dispatcher_.subscribeCmd(cmd, [RpcCore_MOVE_LAMBDA(handle)](const MsgWrapper& msg) {
      auto r = msg.unpackAs<T>();
      if (r.first) {
        handle(std::move(r.second));
      }
      return MsgWrapper::MakeRsp(msg.seq, VOID, r.first);
    });
  }

  /**
   * 注册命令 无接收 无回复
   * @param cmd
   * @param handle
   */
  inline void subscribe(CmdType cmd, RpcCore_MOVE_PARAM(std::function<void()>) handle) {
    dispatcher_.subscribeCmd(std::move(cmd), [RpcCore_MOVE_LAMBDA(handle)](const MsgWrapper& msg) {
      handle();
      return MsgWrapper::MakeRsp(msg.seq);
    });
  }

  /**
   * 取消注册的命令
   * @param cmd
   */
  inline void unsubscribe(const CmdType& cmd) {
    dispatcher_.unsubscribeCmd(cmd);
  }

  /// 发送消息
 public:
  inline SRequest createRequest() {
    return Request::create(shared_from_this());
  }

  inline SRequest cmd(CmdType cmd) {
    return createRequest()->cmd(std::move(cmd));
  }

  /**
   * 可作为连通性的测试 会原样返回payload
   */
  inline SRequest ping(std::string payload = "") {
    return createRequest()->cmd(InnerCmd::PING)->msg(String(std::move(payload)));
  }

 public:
  SeqType makeSeq() override {
    return seq_++;
  }

  void sendRequest(const SRequest& request) override {
    const bool needRsp = request->needRsp();
    if (needRsp) {
      dispatcher_.subscribeRsp(request->seq(), request->rspHandle(), request->timeoutCb_, request->timeoutMs());
    }
    auto msg = MsgWrapper::MakeCmd(request->cmd(), request->seq(), needRsp, request->payload());
    conn_->sendPackageImpl(Coder::serialize(msg));
  }

 private:
  std::shared_ptr<Connection> conn_;
  internal::MsgDispatcher dispatcher_;
  SeqType seq_{0};
};

}  // namespace RpcCore
