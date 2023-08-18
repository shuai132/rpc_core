#pragma once

#include <map>
#include <memory>
#include <utility>

#include "../Connection.hpp"
#include "Coder.hpp"
#include "log.h"
#include "noncopyable.hpp"

namespace RpcCore {
namespace detail {

class MsgDispatcher : noncopyable {
 public:
  using CmdHandle = std::function<std::pair<bool, MsgWrapper>(MsgWrapper)>;
  using RspHandle = std::function<bool(MsgWrapper)>;

  using TimeoutCb = std::function<void()>;
  using TimerImpl = std::function<void(uint32_t ms, TimeoutCb)>;

 public:
  explicit MsgDispatcher(std::shared_ptr<Connection> conn) : conn_(std::move(conn)) {
    auto alive = std::weak_ptr<void>(isAlive_);
    conn_->onRecvPackage = ([this, RpcCore_MOVE_LAMBDA(alive)](const std::string& payload) {
      if (alive.expired()) {
        RpcCore_LOGD("onRecvPackage: MsgDispatcher destroyed!");
        return;
      }
      bool success;
      auto msg = Coder::deserialize(payload, success);
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
        // PING
        const bool isPing = msg.type & MsgWrapper::PING;
        if (isPing) {
          msg.type = static_cast<MsgWrapper::MsgType>(MsgWrapper::RESPONSE | MsgWrapper::PONG);
          conn_->sendPackageImpl(Coder::serialize(msg));
          return;
        }

        // COMMAND
        const auto& cmd = msg.cmd;
        RpcCore_LOGD("dispatch cmd:%s, seq:%d, conn:%p", cmd.c_str(), msg.seq, conn_.get());

        auto it = cmdHandleMap_.find(cmd);
        if (it == cmdHandleMap_.cend()) {
          RpcCore_LOGD("not register cmd for: %s", cmd.c_str());
          return;
        }
        const auto& fn = it->second;
        const bool needRsp = msg.type & MsgWrapper::NEED_RSP;
        auto resp = fn(std::move(msg));
        if (needRsp && resp.first) {
          conn_->sendPackageImpl(Coder::serialize(resp.second));
        }
      } break;

      case MsgWrapper::RESPONSE: {
        // PONG or RESPONSE
        const bool isPong = msg.type & MsgWrapper::PONG;
        const auto handleMap = isPong ? &pongHandleMap_ : &rspHandleMap_;

        RpcCore_LOGD("dispatch rsp: seq=%d, conn:%p", msg.seq, conn_.get());
        auto it = handleMap->find(msg.seq);
        if (it == handleMap->cend()) {
          RpcCore_LOGD("not register callback for response");
          break;
        }
        const auto& cb = it->second;
        if (not cb) {
          RpcCore_LOGE("rsp handle can not be null");
          return;
        }
        if (cb(std::move(msg))) {
          handleMap->erase(it);
          RpcCore_LOGV("handleMap->size=%zu", handleMap->size());
        } else {
          RpcCore_LOGE("may deserialize error");
        }
      } break;

      default:
        RpcCore_LOGE("unknown message type:%d, conn:%p", msg.type, conn_.get());
    }
  }

 public:
  inline void subscribeCmd(const CmdType& cmd, CmdHandle handle) {
    RpcCore_LOGD("subscribeCmd cmd:%s, conn:%p, handle:%p", cmd.c_str(), conn_.get(), &handle);
    cmdHandleMap_[cmd] = std::move(handle);
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

  void subscribeRsp(SeqType seq, RspHandle handle, RpcCore_MOVE_PARAM(TimeoutCb) timeoutCb, uint32_t timeoutMs, bool isPing) {
    RpcCore_LOGD("subscribeRsp seq:%d, handle:%p", seq, &handle);
    if (handle == nullptr) return;
    const auto handleMap = isPing ? &pongHandleMap_ : &rspHandleMap_;

    (*handleMap)[seq] = std::move(handle);

    if (timerImpl_ == nullptr) {
      RpcCore_LOGW("no timeout will cause memory leak!");
    }

    auto alive = std::weak_ptr<void>(isAlive_);
    timerImpl_(timeoutMs, [handleMap, seq, RpcCore_MOVE_LAMBDA(timeoutCb), RpcCore_MOVE_LAMBDA(alive)] {
      if (alive.expired()) {
        RpcCore_LOGD("timeout after destroy, ignore it!");
        return;
      }
      auto it = handleMap->find(seq);
      if (it != handleMap->cend()) {
        if (timeoutCb) {
          timeoutCb();
        }
        handleMap->erase(seq);
        RpcCore_LOGV("Timeout seq=%d, handleMap.size=%zu", seq, handleMap->size());
      }
    });
  }

  inline void setTimerImpl(TimerImpl timerImpl) {
    timerImpl_ = std::move(timerImpl);
  }

 private:
  std::shared_ptr<Connection> conn_;
  std::map<CmdType, CmdHandle> cmdHandleMap_;
  std::map<SeqType, RspHandle> rspHandleMap_;
  CmdHandle pingHandle_;
  std::map<SeqType, RspHandle> pongHandleMap_;
  TimerImpl timerImpl_;
  std::shared_ptr<void> isAlive_ = std::make_shared<uint8_t>();
};

}  // namespace detail
}  // namespace RpcCore
