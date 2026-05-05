#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

#include "assert_def.h"
#include "rpc_core/net/asio_tcp.hpp"

int main() {
  using rpc_core::net::asio_tcp_rpc;
  using tcp = asio::ip::tcp;

  asio::io_context io_context;
  tcp::acceptor acceptor(io_context, tcp::endpoint(asio::ip::address_v4::loopback(), 0));
  const uint16_t port = acceptor.local_endpoint().port();

  bool opened = false;
  bool got_response = false;
  std::shared_ptr<asio_tcp_rpc> server_session;

  acceptor.async_accept([&](const std::error_code& ec, tcp::socket socket) {
    ASSERT(!ec);
    auto socket_ptr = std::make_shared<tcp::socket>(std::move(socket));
    server_session = asio_tcp_rpc::bind(socket_ptr);
    server_session->rpc()->subscribe("cmd", [](const std::string& msg) -> std::string {
      ASSERT(msg == "hello");
      return "world";
    });
  });

  auto timeout = std::make_shared<asio::steady_timer>(io_context);
  timeout->expires_after(std::chrono::seconds(2));
  timeout->async_wait([&](const std::error_code& ec) {
    if (!ec) {
      ASSERT(false);
    }
  });

  auto client = asio_tcp_rpc::create(io_context);
  client->on_open = [&](rpc_core::rpc_s rpc) {
    opened = true;
    rpc->cmd("cmd")
        ->msg(std::string("hello"))
        ->rsp([&](const std::string& rsp) {
          ASSERT(rsp == "world");
          got_response = true;
          timeout->cancel();
          client->close();
          if (server_session) {
            server_session->close();
          }
          acceptor.close();
          io_context.stop();
        })
        ->timeout_ms(1000)
        ->call();
  };
  client->on_open_failed = [](const std::error_code&) {
    ASSERT(false);
  };
  client->open("127.0.0.1", port);

  io_context.run();

  ASSERT(opened);
  ASSERT(got_response);
  return 0;
}
