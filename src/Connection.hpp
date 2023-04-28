#pragma once

#include <functional>
#include <string>
#include <utility>

#include "detail/noncopyable.hpp"

namespace RpcCore {

/**
 * Defines interfaces for sending and receiving messages
 * Usage:
 * 1. Both sending and receiving should ensure that a complete package of data is sent/received.
 * 2. Call onRecvPackage when a package of data is actually received.
 * 3. Provides the implementation of sending data, sendPackageImpl.
 */
struct Connection : detail::noncopyable {
  std::function<void(std::string)> sendPackageImpl;
  std::function<void(std::string)> onRecvPackage;
};

/**
 * Loopback connection for testing
 */
struct LoopbackConnection : public Connection {
  LoopbackConnection() {
    sendPackageImpl = [this](std::string payload) {
      onRecvPackage(std::move(payload));
    };
  }
};

}  // namespace RpcCore
