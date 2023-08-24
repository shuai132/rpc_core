#pragma once

#include <functional>
#include <string>
#include <utility>

// config
#include "config.hpp"

// include
#include "detail/noncopyable.hpp"

namespace RPC_CORE_NAMESPACE {

/**
 * Defines interfaces for sending and receiving messages
 * Usage:
 * 1. Both sending and receiving should ensure that a complete package of data is sent/received.
 * 2. Call on_recv_package when a package of data is actually received.
 * 3. Provides the implementation of sending data, send_package_impl.
 */
struct connection : detail::noncopyable {
  std::function<void(std::string)> send_package_impl;
  std::function<void(std::string)> on_recv_package;
};

/**
 * Loopback connection for testing
 */
struct loopback_connection : public connection {
  loopback_connection() {
    send_package_impl = [this](std::string payload) {
      on_recv_package(std::move(payload));
    };
  }
};

}  // namespace RPC_CORE_NAMESPACE
