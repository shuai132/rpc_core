#pragma once

#include <string>
#include <functional>
#include <cassert>

#include "base/noncopyable.hpp"

namespace RpcCore {

/**
 * 消息连接
 * 约定消息发送和接收的接口
 */
class Connection : noncopyable {
    using PayloadHandle = std::function<void(const std::string& payload)>;

public:
    virtual ~Connection() = default;

public:
    virtual void sendPayload(const std::string& payload)
    {
        if (sendPayloadFunc_) {
            sendPayloadFunc_(payload);
        }
    }

    void onPayload(const std::string& payload)
    {
        assert(onPayloadHandle_);
        onPayloadHandle_(payload);
    }

    void setOnPayloadHandle(const PayloadHandle& handle)
    {
        onPayloadHandle_ = handle;
    }

    void setSendPayloadFunc(const PayloadHandle& handle)
    {
        sendPayloadFunc_ = handle;
    }

private:
    PayloadHandle onPayloadHandle_;
    PayloadHandle sendPayloadFunc_;
};


/**
 * 回环消息连接
 * 多用于测试
 */
class LoopbackConnection : public Connection {
public:
    void sendPayload(const std::string& payload) override {
        onPayload(payload);
    }
};

}
