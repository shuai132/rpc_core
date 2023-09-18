#pragma once

#include <functional>
#include <string>
#include <utility>

// config
#include "config.hpp"

// include
#include "detail/data_packer.hpp"
#include "detail/noncopyable.hpp"

namespace rpc_core {

/**
 * Defines interfaces for sending and receiving messages
 * Usage:
 * 1. Both sending and receiving should ensure that a complete package of data is sent/received.
 * 2. Call on_recv_package when a package of data is actually received.
 * 3. Provide the implementation of sending data, send_package_impl.
 */
struct connection : detail::noncopyable {
  std::function<void(std::string)> send_package_impl;
  std::function<void(std::string)> on_recv_package;
};

/**
 * Default connection avoid crash
 */
struct default_connection : connection {
  default_connection() {
    send_package_impl = [](const std::string &payload) {
      RPC_CORE_LOGE("need send_package_impl: %zu", payload.size());
    };
    on_recv_package = [](const std::string &payload) {
      RPC_CORE_LOGE("need on_recv_package: %zu", payload.size());
    };
  }
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

/**
 * Stream connection
 * for bytes stream: tcp socket, serial port, etc.
 */
struct stream_connection : public connection {
  explicit stream_connection(uint32_t max_body_size = UINT32_MAX) : data_packer_(max_body_size) {
    send_package_impl = [this](const std::string &package) {
      auto payload = data_packer_.pack(package);
      send_bytes_impl(std::move(payload));
    };
    data_packer_.on_data = [this](std::string payload) {
      on_recv_package(std::move(payload));
    };
    on_recv_bytes = [this](const void *data, size_t size) {
      data_packer_.feed(data, size);
    };
  }

  /**
   * should call on connected or disconnected
   */
  void reset() {
    data_packer_.reset();
  }

 public:
  std::function<void(std::string)> send_bytes_impl;
  std::function<void(const void *data, size_t size)> on_recv_bytes;

 private:
  detail::data_packer data_packer_;
};

}  // namespace rpc_core
