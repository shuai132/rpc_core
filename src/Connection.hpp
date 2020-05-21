#pragma once

#include <string>
#include <functional>
#include <cassert>

#include "base/noncopyable.hpp"
#include "MakeEvent.hpp"

namespace RpcCore {

/**
 * 消息连接
 * 约定消息发送和接收的接口
 * 用法
 * 1. 收发都要保证是一包完整的数据
 * 2. 当实际收到一包数据时 请调用onRecvPacket
 * 3. 提供发送数据的实现 重载sendPacket或者设置发送的实现回调setSendPacketCb
 */
class Connection : noncopyable {
    MAKE_EVENT(SendPacketImpl, std::string);
    MAKE_EVENT(RecvPacket, std::string);

public:
    explicit Connection(SendPacketImplCb_t sendImpl = nullptr) : _cbSendPacketImpl(std::move(sendImpl)) {}
    virtual ~Connection() = default;

public:
    virtual void sendPacket(const std::string& payload) {
        onSendPacketImpl(payload);
    }
};


/**
 * 回环消息连接 用于测试
 */
class LoopbackConnection : public Connection {
public:
    void sendPacket(const std::string& payload) override {
        onRecvPacket(payload);
    }
};

}
