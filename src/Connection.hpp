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
 */
class Connection : noncopyable {
    MAKE_EVENT(SendPayload, std::string);
    MAKE_EVENT(RecvPayload, std::string);

public:
    virtual ~Connection() = default;

public:
    virtual void sendPayload(const std::string& payload)
    {
        onSendPayload(payload);
    }

    void onPayload(const std::string& payload)
    {
        onRecvPayload(payload);
    }
};


/**
 * 回环消息连接 用于测试
 */
class LoopbackConnection : public Connection {
public:
    void sendPayload(const std::string& payload) override {
        onPayload(payload);
    }
};

}
