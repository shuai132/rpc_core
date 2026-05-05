#include <cstdint>
#include <cstdlib>
#include <string>

#include "assert_def.h"
#include "rpc_core/detail/log.h"
#include "rpc_core/net/asio_tcp.hpp"

int main(int argc, char** argv) {
  using rpc_core::net::asio_tcp_rpc;

  const std::string host = argc > 1 ? argv[1] : "127.0.0.1";
  const uint16_t port = argc > 2 ? static_cast<uint16_t>(std::atoi(argv[2])) : 6666;

  asio::io_context io_context;
  auto client = asio_tcp_rpc::create(io_context);

  client->on_open = [&](rpc_core::rpc_s rpc) {
    RPC_CORE_LOG("client on_open");
    rpc->cmd("cmd")
        ->msg(std::string("hello"))
        ->rsp([&](const std::string& rsp) {
          RPC_CORE_LOG("cmd rsp: %s", rsp.c_str());
          ASSERT(rsp == "world");
          client->close();
          io_context.stop();
        })
        ->timeout_ms(3000)
        ->call();
  };
  client->on_open_failed = [](const std::error_code& ec) {
    RPC_CORE_LOG("client on_open_failed: %d, %s", ec.value(), ec.message().c_str());
    ASSERT(false);
  };
  client->on_close = [](const std::error_code&) {
    RPC_CORE_LOG("client on_close");
  };

  client->open(host, port);
  io_context.run();
  return 0;
}
