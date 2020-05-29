#pragma once

#include <memory>
#include <utility>

#include "base/noncopyable.hpp"
#include "Connection.hpp"
#include "MsgDispatcher.hpp"
#include "coder/JsonCoder.hpp"
#include "Request.hpp"
#include "Dispose.hpp"

namespace RpcCore {
/**
 * 1. 封装操作细节以方便发送接收消息
 * 2. 持有一个连接 收发均基于此连接
 * 为了方便使用，消息注册和发送重载了多种形式。
 */
class Rpc : noncopyable, public std::enable_shared_from_this<Rpc>, public Request::RpcProto {
    using CmdHandle = MsgDispatcher::CmdHandle;
    using RspHandle = MsgDispatcher::RspHandle;
    using TimeoutCb = MsgDispatcher::TimeoutCb;

public:
    explicit Rpc(
            std::shared_ptr<Connection> conn,
            std::shared_ptr<coder::Coder> coder = std::make_shared<coder::JsonCoder>())
            : conn_(conn), coder_(std::move(coder)), dispatcher_(std::move(conn), coder_)
    {
        // 注册一个PING消息，以便有PING到来时，给发送者回复PONG，PING/PONG可携带payload，会原样返回。
        dispatcher_.subscribeCmd(InnerCmd::PING, [](MsgWrapper msg) {
            msg.cmd = InnerCmd::PONG;
            auto ret = msg.unpackAs<String>();
            return MsgWrapper::MakeRsp(msg.seq, ret.second, ret.first);
        });
    }
    ~Rpc() override {
        LOGD("~Rpc");
    };

public:
    inline std::shared_ptr<Connection> getConn() const {
        return conn_;
    }

    inline void setTimerImpl(MsgDispatcher::TimerImpl timerImpl) {
        dispatcher_.setTimerImpl(std::move(timerImpl));
    }

    /// 注册消息
public:
    /**
     * 注册命令 接收消息 返回消息
     * @tparam T 接收消息的类型 这将决定解析行为 与发送时"发送参数类型"一致
     * @tparam R 返回消息结果的类型 与发送时"回调参数类型"一致
     * @param cmd
     * @param handle 接收T类型消息 返回T类型消息作为回复 可使用R(Message, bool)构造 也可直接返回Message或bool
     */
    template <typename T, typename R, RpcCore_ENSURE_TYPE_IS_MESSAGE(T), RpcCore_ENSURE_TYPE_IS_MESSAGE(R)>
    void subscribe(const CmdType& cmd, const std::function<R(T&&)>& handle) {
        dispatcher_.subscribeCmd(cmd, [handle](const MsgWrapper& msg) {
            auto r = msg.unpackAs<T>();
            if (r.first) {
                handle(std::move(r.second));
            }
            return MsgWrapper::MakeRsp(
                    msg.seq,
                    r.second,
                    r.first
            );
        });
    }

    /**
     * 注册命令 接收 无回复
     * @tparam T 接收消息的类型
     * @param cmd
     * @param handle 接收数据 返回操作状态
     */
    template <typename T, RpcCore_ENSURE_TYPE_IS_MESSAGE(T)>
    void subscribe(const CmdType& cmd, const std::function<void(T&&)>& handle) {
        dispatcher_.subscribeCmd(cmd, [handle](const MsgWrapper& msg) {
            auto r = msg.unpackAs<T>();
            if (r.first) {
                handle(std::move(r.second));
            }
            return MsgWrapper::MakeRsp(
                    msg.seq,
                    r.second,
                    r.first
            );
        });
    }

    /**
     * 注册命令 不接收 无回复
     * @param cmd
     * @param handle 不接收参数 返回操作状态
     */
    inline void subscribe(CmdType cmd, const std::function<void()>& handle) {
        dispatcher_.subscribeCmd(std::move(cmd), [handle](const MsgWrapper& msg) {
            handle();
            return MsgWrapper::MakeRsp(msg.seq);
        });
    }

    /**
     * 注册命令 不接收 有回复
     * @tparam R 返回给对方的数据类型
     * @param cmd
     * @param handle 不接收参数 返回R(msg, true)形式
     */
    template <typename R, RpcCore_ENSURE_TYPE_IS_MESSAGE(R)>
    inline void subscribe(const CmdType& cmd, const std::function<R()>& handle) {
        dispatcher_.subscribeCmd(cmd, [handle](const MsgWrapper& msg) {
            R rsp = handle();
            return MsgWrapper::MakeRsp(
                    msg.seq,
                    rsp
            );
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

    /**
     * 可作为连通性的测试 会原样返回payload
     */
    inline SRequest ping(std::string payload = "")
    {
        auto request = createRequest();
        request->init();
        request->cmd(InnerCmd::PING);
        request->setMessage(String(std::move(payload)));
        return request;
    }

public:
    SeqType makeSeq() override {
        return seq_++;
    }

    void sendRequest(const SRequest& request) override {
        dispose->addRequest(request);
        dispatcher_.subscribeRsp(request->seq(), request->rspHandle(), request->timeoutCb_, request->timeoutMs());
        auto msg = MsgWrapper::MakeCmd(request->cmd(), request->seq(), request->payload());
        conn_->sendPacket(coder_->serialize(msg));
    }

public:
    std::unique_ptr<Dispose> dispose = std::unique_ptr<Dispose>(new Dispose());

private:
    std::shared_ptr<Connection> conn_;
    std::shared_ptr<coder::Coder> coder_;
    MsgDispatcher dispatcher_;
    SeqType seq_{0};
};

}
