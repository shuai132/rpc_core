#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <system_error>
#include <utility>

#include "asio.hpp"

#if !defined(RPC_CORE_SERIALIZE_USE_CUSTOM) && !defined(RPC_CORE_SERIALIZE_USE_NLOHMANN_JSON)
#define RPC_CORE_SERIALIZE_USE_NLOHMANN_JSON
#endif

#include "rpc_core.hpp"
#include "rpc_core/detail/data_packer.hpp"

namespace rpc_core {
namespace net {

class asio_tcp_rpc : public std::enable_shared_from_this<asio_tcp_rpc> {
 public:
  using socket_type = asio::ip::tcp::socket;

  static std::shared_ptr<asio_tcp_rpc> create(asio::io_context& io_context, rpc_s rpc = nullptr, uint32_t max_body_size = UINT32_MAX) {
    auto result = std::shared_ptr<asio_tcp_rpc>(new asio_tcp_rpc(io_context, max_body_size));
    result->init(std::move(rpc));
    return result;
  }

  static std::shared_ptr<asio_tcp_rpc> bind(std::shared_ptr<socket_type> socket, rpc_s rpc = nullptr, bool ready = true,
                                            uint32_t max_body_size = UINT32_MAX) {
    auto result = std::shared_ptr<asio_tcp_rpc>(new asio_tcp_rpc(std::move(socket), max_body_size));
    result->init(std::move(rpc));
    result->start(ready);
    return result;
  }

  static std::shared_ptr<asio_tcp_rpc> open(asio::io_context& io_context, const std::string& host, uint16_t port, rpc_s rpc = nullptr,
                                            uint32_t max_body_size = UINT32_MAX) {
    auto result = create(io_context, std::move(rpc), max_body_size);
    result->open(host, port);
    return result;
  }

  void open(const std::string& host, uint16_t port) {
    auto self = shared_from_this();
    auto resolver = std::make_shared<asio::ip::tcp::resolver>(io_context_);
    resolver->async_resolve(host, std::to_string(port), [self, resolver](const std::error_code& ec, asio::ip::tcp::resolver::results_type endpoints) {
      if (ec) {
        self->fail_open(ec);
        return;
      }
      asio::async_connect(*self->socket_, endpoints, [self](const std::error_code& connect_ec, const asio::ip::tcp::endpoint&) {
        if (connect_ec) {
          self->fail_open(connect_ec);
          return;
        }
        self->start(true);
        if (self->on_open) {
          self->on_open(self->rpc_);
        }
      });
    });
  }

  void close() {
    auto self = shared_from_this();
    asio::post(io_context_, [self] {
      self->handle_close(std::error_code());
    });
  }

  socket_type& socket() {
    return *socket_;
  }

  rpc_s rpc() const {
    return rpc_;
  }

  std::shared_ptr<connection> get_connection() const {
    return connection_;
  }

 private:
  asio_tcp_rpc(asio::io_context& io_context, uint32_t max_body_size)
      : io_context_(io_context), socket_(std::make_shared<socket_type>(io_context)), data_packer_(max_body_size) {}

  asio_tcp_rpc(std::shared_ptr<socket_type> socket, uint32_t max_body_size)
      : io_context_(static_cast<asio::io_context&>(socket->get_executor().context())), socket_(std::move(socket)), data_packer_(max_body_size) {}

  void init(rpc_s rpc) {
    if (rpc) {
      rpc_ = std::move(rpc);
      connection_ = rpc_->get_connection();
    } else {
      connection_ = std::make_shared<connection>();
      rpc_ = rpc_core::rpc::create(connection_);
    }

    auto self = std::weak_ptr<asio_tcp_rpc>(shared_from_this());
    connection_->send_package_impl = [self](std::string package) {
      if (auto owner = self.lock()) {
        owner->send_package(std::move(package));
      }
    };
    data_packer_.on_data = [self](std::string package) {
      if (auto owner = self.lock()) {
        owner->connection_->on_recv_package(std::move(package));
      }
    };
    rpc_->set_timer([self](uint32_t ms, rpc::timeout_cb cb) {
      auto owner = self.lock();
      if (!owner) return;
      auto timer = std::make_shared<asio::steady_timer>(owner->io_context_);
      timer->expires_after(std::chrono::milliseconds(ms));
      timer->async_wait([timer, cb = std::move(cb)](const std::error_code& ec) mutable {
        if (!ec) {
          cb();
        }
      });
    });
  }

  void start(bool ready) {
    if (started_) return;
    started_ = true;
    closed_ = false;
    data_packer_.reset();
    rpc_->set_ready(ready);
    do_read();
  }

  void do_read() {
    auto self = shared_from_this();
    socket_->async_read_some(asio::buffer(read_buffer_), [self](const std::error_code& ec, size_t size) {
      if (ec) {
        self->handle_close(ec);
        return;
      }
      if (!self->data_packer_.feed(self->read_buffer_.data(), size)) {
        self->handle_close(std::error_code());
        return;
      }
      self->do_read();
    });
  }

  void send_package(std::string package) {
    auto payload = data_packer_.pack(package);
    if (payload.empty() && !package.empty()) return;

    auto self = shared_from_this();
    asio::post(io_context_, [self, payload = std::move(payload)]() mutable {
      if (self->closed_) return;
      const bool writing = !self->write_queue_.empty();
      self->write_queue_.emplace_back(std::move(payload));
      if (!writing) {
        self->do_write();
      }
    });
  }

  void do_write() {
    auto self = shared_from_this();
    asio::async_write(*socket_, asio::buffer(write_queue_.front()), [self](const std::error_code& ec, size_t) {
      if (ec) {
        self->handle_close(ec);
        return;
      }
      self->write_queue_.pop_front();
      if (!self->write_queue_.empty()) {
        self->do_write();
      }
    });
  }

  void fail_open(const std::error_code& ec) {
    rpc_->set_ready(false);
    if (on_open_failed) {
      on_open_failed(ec);
    }
  }

  void handle_close(const std::error_code& ec) {
    if (closed_) return;
    closed_ = true;
    started_ = false;
    rpc_->set_ready(false);
    data_packer_.reset();
    write_queue_.clear();

    std::error_code ignored;
    socket_->shutdown(asio::ip::tcp::socket::shutdown_both, ignored);
    socket_->close(ignored);

    if (on_close) {
      on_close(ec);
    }
  }

 public:
  std::function<void(rpc_s)> on_open;
  std::function<void(std::error_code)> on_open_failed;
  std::function<void(std::error_code)> on_close;

 private:
  asio::io_context& io_context_;
  std::shared_ptr<socket_type> socket_;
  rpc_s rpc_;
  std::shared_ptr<connection> connection_;
  detail::data_packer data_packer_;
  std::array<char, 65536> read_buffer_{};
  std::deque<std::string> write_queue_;
  bool started_ = false;
  bool closed_ = true;
};

}  // namespace net
}  // namespace rpc_core
