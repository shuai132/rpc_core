#pragma once

#include <string>
#include <functional>
#include <cassert>
#include <utility>

#include "base/noncopyable.hpp"

namespace RpcCore {

/**
 * 消息连接
 * 约定消息发送和接收的接口
 * 用法
 * 1. 收发都要保证是一包完整的数据
 * 2. 当实际收到一包数据时 请调用onRecvPackage
 * 3. 提供发送数据的实现 重载sendPackage或者设置发送的实现回调setSendPackageHandle
 */
class Connection : noncopyable {
private:
    using SendPackageHandle = std::function<void(std::string)>;
    using RecvPackageHandle = std::function<void(std::string)>;
    SendPackageHandle sendPackageHandle_;
    RecvPackageHandle recvPackageHandle_;

public:
    explicit Connection(SendPackageHandle handle = nullptr) : sendPackageHandle_(std::move(handle)) {}
    virtual ~Connection() = default;

public:
    virtual void sendPackage(std::string payload) {
        if (sendPackageHandle_) sendPackageHandle_(std::move(payload));
    }

public:
    inline void setSendPackageHandle(SendPackageHandle cb) { sendPackageHandle_ = std::move(cb); }
    inline void setRecvPackageHandle(RecvPackageHandle cb) { recvPackageHandle_ = std::move(cb); }
    inline void onRecvPackage(std::string package) {
        if (recvPackageHandle_) recvPackageHandle_(std::move(package));
    }
};


/**
 * 回环消息连接 用于测试
 */
class LoopbackConnection : public Connection {
public:
    void sendPackage(std::string payload) override {
        onRecvPackage(std::move(payload));
    }
};

}
