#pragma once

#include <map>
#include <memory>
#include <utility>

#include "base/noncopyable.hpp"
#include "Connection.hpp"
#include "coder/Coder.hpp"
#include "Message.hpp"
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
    explicit MsgDispatcher(std::shared_ptr<Connection> conn, std::shared_ptr<Coder> coder)
            : conn_(std::move(conn)), coder_(std::move(coder)) {
        conn_->setRecvPacketCb([this](const std::string& payload){
            bool success;
            auto msg = coder_->unserialize(payload, success);
            if (success) {
                this->dispatch(msg);
            } else {
                LOGE("payload can not be parsed, msg info");
            }
        });
    }

private:
    void dispatch(const MsgWrapper& msg)
    {
        switch (msg.type) {
            case MsgWrapper::COMMAND:
            {
                // COMMAND
                const auto& cmd = msg.cmd;
                LOGD("dispatch cmd:%s, seq:%d, conn:%p", CmdToStr(cmd).c_str(), msg.seq, conn_.get());

                auto it = cmdHandleMap_.find(cmd);
                if (it == cmdHandleMap_.cend()) {
                    LOGD("not register cmd for: %s", CmdToStr(cmd).c_str());
                    return;
                }
                const auto& fn = (*it).second;
                auto resp = fn(msg);
                if (resp.first) {
                    conn_->sendPacket(coder_->serialize(resp.second));
                }
            } break;

            case MsgWrapper::RESPONSE:
            {
                LOGD("dispatch rsp: seq=%d, conn:%p", msg.seq, conn_.get());
                auto it = rspHandleMap_.find(msg.seq);
                if (it == rspHandleMap_.cend()) {
                    LOGD("not register callback for response");
                    break;
                }
                const auto& cb = (*it).second;
                if(not cb) {
                    LOGE("rsp handle can not be null");
                    return;
                }
                if (cb(msg)) {
                    rspHandleMap_.erase(it);
                    LOGD("rspHandleMap_.size=%zu", rspHandleMap_.size());
                } else {
                    LOGE("may unserialize error");
                }
            } break;

            default:
                LOGE("unknown message type, conn:%p", conn_.get());
        }
    }

public:
    inline void subscribeCmd(CmdType cmd, CmdHandle handle)
    {
        LOGD("subscribeCmd cmd:%s, conn:%p, handle:%p", CmdToStr(cmd).c_str(), conn_.get(), &handle);
        cmdHandleMap_[std::move(cmd)] = std::move(handle);
    }

    void unsubscribeCmd(const CmdType& cmd)
    {
        auto it = cmdHandleMap_.find(cmd);
        if (it != cmdHandleMap_.cend()) {
            LOGD("erase cmd: %s", CmdToStr(cmd).c_str());
            cmdHandleMap_.erase(it);
        } else {
            LOGD("not register cmd for: %s", CmdToStr(cmd).c_str());
        }
    }

    void subscribeRsp(SeqType seq, RspHandle handle, TimeoutCb timeoutCb, uint32_t timeoutMs)
    {
        LOGD("subscribeRsp seq:%d, handle:%p", seq, &handle);
        if (handle == nullptr) return;
        rspHandleMap_[seq] = std::move(handle);

        if(timerImpl_ == nullptr) {
            LOGW("no timeout will cause memory leak!");
        }

        timerImpl_(timeoutMs, [this, seq, RpcCore_MOVE(timeoutCb)] {
            auto it = rspHandleMap_.find(seq);
            if (it != rspHandleMap_.cend()) {
                if (timeoutCb) {
                    timeoutCb();
                }
                rspHandleMap_.erase(seq);
                LOGD("Timeout seq=%d, rspHandleMap_.size=%zu", seq, rspHandleMap_.size());
            }
        });
    }

    inline std::shared_ptr<Connection> getConn() const
    {
        return conn_;
    }

    inline void setTimerImpl(TimerImpl timerImpl)
    {
        timerImpl_ = std::move(timerImpl);
    }

private:
    std::shared_ptr<Connection> conn_;
    std::shared_ptr<Coder> coder_;
    std::map<CmdType, CmdHandle> cmdHandleMap_;
    std::map<SeqType, RspHandle> rspHandleMap_;
    TimerImpl timerImpl_;
};

}
}
