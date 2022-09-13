#pragma once

#include <map>
#include <memory>
#include <utility>

#include "Coder.hpp"
#include "Connection.hpp"
#include "Message.hpp"
#include "base/noncopyable.hpp"
#include "log.h"

namespace RpcCore {
namespace internal {

/**
 * 消息分发器
 * 注册消息到指定命令
 */
class MsgDispatcher : noncopyable {
 public:
  // 命令处理者 当接收到命令时回调：参数为命令携带的信息，返回值为序列化是否成功和返回的信息
  using CmdHandle = std::function<std::pair<bool, MsgWrapper>(MsgWrapper)>;
  // 回复处理者 当接收到响应时回调：参数为响应携带的信息，返回值为序列化是否成功
  using RspHandle = std::function<bool(MsgWrapper)>;

  // 超时回调 当期待时间内未收到正确响应时回调
  using TimeoutCb = std::function<void()>;
  // 定时器具体实现的接口
  using TimerImpl = std::function<void(uint32_t ms, TimeoutCb)>;

 public:
  explicit MsgDispatcher(std::shared_ptr<Connection> conn) : conn_(std::move(conn)) {
    conn_->onRecvPackage = ([this](const std::string& payload) {
      bool success;
      auto msg = Coder::unserialize(payload, success);
      if (success) {
        this->dispatch(std::move(msg));
      } else {
        RpcCore_LOGE("payload can not be parsed, msg info");
      }
    });
  }

 private:
  void dispatch(MsgWrapper msg) {
    switch (msg.type & (MsgWrapper::COMMAND | MsgWrapper::RESPONSE)) {
      case MsgWrapper::COMMAND: {
        // COMMAND
        const auto& cmd = msg.cmd;
        RpcCore_LOGD("dispatch cmd:%s, seq:%d, conn:%p", cmd.c_str(), msg.seq, conn_.get());

        auto it = cmdHandleMap_.find(cmd);
        if (it == cmdHandleMap_.cend()) {
          RpcCore_LOGD("not register cmd for: %s", cmd.c_str());
          return;
        }
        const auto& fn = (*it).second;
        const bool needRsp = msg.type & MsgWrapper::NEED_RSP;
        auto resp = fn(std::move(msg));
        if (needRsp && resp.first) {
          conn_->sendPackageImpl(Coder::serialize(resp.second));
        }
      } break;

      case MsgWrapper::RESPONSE: {
        RpcCore_LOGD("dispatch rsp: seq=%d, conn:%p", msg.seq, conn_.get());
        auto it = rspHandleMap_.find(msg.seq);
        if (it == rspHandleMap_.cend()) {
          RpcCore_LOGD("not register callback for response");
          break;
        }
        const auto& cb = (*it).second;
        if (not cb) {
          RpcCore_LOGE("rsp handle can not be null");
          return;
        }
        if (cb(std::move(msg))) {
          rspHandleMap_.erase(it);
          RpcCore_LOGV("rspHandleMap_.size=%zu", rspHandleMap_.size());
        } else {
          RpcCore_LOGE("may unserialize error");
        }
      } break;

      default:
        RpcCore_LOGE("unknown message type:%d, conn:%p", msg.type, conn_.get());
    }
  }

 public:
  inline void subscribeCmd(CmdType cmd, CmdHandle handle) {
    RpcCore_LOGD("subscribeCmd cmd:%s, conn:%p, handle:%p", cmd.c_str(), conn_.get(), &handle);
    cmdHandleMap_[std::move(cmd)] = std::move(handle);
  }

  void unsubscribeCmd(const CmdType& cmd) {
    auto it = cmdHandleMap_.find(cmd);
    if (it != cmdHandleMap_.cend()) {
      RpcCore_LOGD("erase cmd: %s", cmd.c_str());
      cmdHandleMap_.erase(it);
    } else {
      RpcCore_LOGD("not register cmd for: %s", cmd.c_str());
    }
  }

  void subscribeRsp(SeqType seq, RspHandle handle, TimeoutCb timeoutCb, uint32_t timeoutMs) {
    RpcCore_LOGD("subscribeRsp seq:%d, handle:%p", seq, &handle);
    if (handle == nullptr) return;
    rspHandleMap_[seq] = std::move(handle);

    if (timerImpl_ == nullptr) {
      RpcCore_LOGW("no timeout will cause memory leak!");
    }

    timerImpl_(timeoutMs, [this, seq, RpcCore_MOVE(timeoutCb)] {
      auto it = rspHandleMap_.find(seq);
      if (it != rspHandleMap_.cend()) {
        if (timeoutCb) {
          timeoutCb();
        }
        rspHandleMap_.erase(seq);
        RpcCore_LOGV("Timeout seq=%d, rspHandleMap_.size=%zu", seq, rspHandleMap_.size());
      }
    });
  }

  inline std::shared_ptr<Connection> getConn() const {
    return conn_;
  }

  inline void setTimerImpl(TimerImpl timerImpl) {
    timerImpl_ = std::move(timerImpl);
  }

 private:
  std::shared_ptr<Connection> conn_;
  std::map<CmdType, CmdHandle> cmdHandleMap_;
  std::map<SeqType, RspHandle> rspHandleMap_;
  TimerImpl timerImpl_;
};

}  // namespace internal
}  // namespace RpcCore
