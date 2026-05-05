#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>

#include "assert_def.h"
#include "rpc_core/detail/log.h"
#include "rpc_core/net/asio_tcp.hpp"

int main(int argc, char** argv) {
  using rpc_core::net::asio_tcp_rpc;
  using tcp = asio::ip::tcp;

  const uint16_t port = argc > 1 ? static_cast<uint16_t>(std::atoi(argv[1])) : 6666;

  asio::io_context io_context;
  tcp::acceptor acceptor(io_context, tcp::endpoint(asio::ip::tcp::v4(), port));
  std::shared_ptr<asio_tcp_rpc> session;

  acceptor.async_accept([&](const std::error_code& ec, tcp::socket socket) {
    ASSERT(!ec);
    RPC_CORE_LOG("server on_session");

    session = asio_tcp_rpc::bind(std::make_shared<tcp::socket>(std::move(socket)));
    session->rpc()->subscribe("cmd", [](const std::string& msg) -> std::string {
      RPC_CORE_LOG("server on cmd: %s", msg.c_str());
      return "world";
    });
    session->on_close = [&](const std::error_code&) {
      RPC_CORE_LOG("server session on_close");
      acceptor.close();
      io_context.stop();
    };
  });

  RPC_CORE_LOG("rpc_s listening on 0.0.0.0:%u", port);
  io_context.run();
  return 0;
}
