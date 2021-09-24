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
 * 3. 提供发送数据的实现 sendPackageImpl
 */
struct Connection : noncopyable {
    std::function<void(std::string)> sendPackageImpl;
    std::function<void(std::string)> onRecvPackage;
};

/**
 * 回环消息连接 用于测试
 */
struct LoopbackConnection : public Connection {
    LoopbackConnection() {
        sendPackageImpl = [this](std::string payload) {
            onRecvPackage(std::move(payload));
        };
    }
};

}
