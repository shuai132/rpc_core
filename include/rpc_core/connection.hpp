#pragma once

#include <functional>
#include <memory>
#include <string>
#include <utility>

// config
#include "config.hpp"

// include
#include "detail/data_packer.hpp"
#include "detail/noncopyable.hpp"
#include "detail/shared_function.hpp"

namespace rpc_core {

/**
 * Defines interfaces for sending and receiving messages
 * Usage:
 * 1. Both sending and receiving should ensure that a complete package of data is sent/received.
 * 2. Call on_recv_package when a package of data is actually received.
 * 3. Provide the implementation of sending data, send_package_impl.
 */
struct connection : detail::noncopyable {
  detail::shared_function<bool(std::string)> send_package_impl;
  detail::shared_function<void(std::string)> on_recv_package;

  // true means accepted by the transport, not acknowledged by the peer.
  bool send_package(std::string package) {
    return send_package_impl && send_package_impl(std::move(package));
  }
};

/**
 * Default connection avoid crash
 */
struct default_connection : connection {
  default_connection() {
    send_package_impl = [](const std::string &payload) {
      RPC_CORE_LOGE("need send_package_impl: %zu", payload.size());
      return false;
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
  static std::pair<std::shared_ptr<connection>, std::shared_ptr<connection>> create() {
    auto c1 = std::make_shared<connection>();
    auto c1_weak = std::weak_ptr<connection>(c1);
    auto c2 = std::make_shared<connection>();
    auto c2_weak = std::weak_ptr<connection>(c2);
    c1->send_package_impl = [c2_weak](std::string package) {
      if (auto peer = c2_weak.lock()) {
        peer->on_recv_package(std::move(package));
        return true;
      }
      return false;
    };
    c2->send_package_impl = [c1_weak](std::string package) {
      if (auto peer = c1_weak.lock()) {
        peer->on_recv_package(std::move(package));
        return true;
      }
      return false;
    };
    return std::make_pair(c1, c2);
  }
};

/**
 * Stream connection
 * for bytes stream: tcp socket, serial port, etc.
 */
struct stream_connection : public connection {
 private:
  struct stream_state {
    explicit stream_state(uint32_t max_body_size) : packer(max_body_size) {}
    detail::data_packer packer;
    stream_connection* owner = nullptr;
  };

 public:
  explicit stream_connection(uint32_t max_body_size = UINT32_MAX) : state_(std::make_shared<stream_state>(max_body_size)) {
    state_->owner = this;
    send_package_impl = [state = state_](const std::string &package) {
      if (!state->owner) return false;
      auto payload = state->packer.pack(package);
      auto& send = state->owner->send_bytes_impl;
      return !payload.empty() && send && send(std::move(payload));
    };
    state_->packer.on_data = [state = state_.get()](std::string payload) {
      if (state->owner) state->owner->on_recv_package(std::move(payload));
    };
    on_recv_bytes = [state = state_](const void *data, size_t size) {
      // User callbacks may destroy the connection and this callable while feeding.
      auto keeper = state;
      return keeper->owner && keeper->packer.feed(data, size);
    };
  }

  ~stream_connection() {
    state_->owner = nullptr;
    state_->packer.reset();
  }

  /**
   * should call on connected or disconnected
   */
  void reset() {
    state_->packer.reset();
  }

 public:
  detail::shared_function<bool(std::string)> send_bytes_impl;
  // false means invalid framing; stop receiving until reset() for a new stream.
  detail::shared_function<bool(const void *data, size_t size)> on_recv_bytes;

 private:
  std::shared_ptr<stream_state> state_;
};

}  // namespace rpc_core
