#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "assert_def.h"
#include "rpc_core/detail/log.h"
#include "rpc_core/net/asio_tcp.hpp"

int main(int argc, char** argv) {
  using rpc_core::net::asio_tcp_rpc;
  using tcp = asio::ip::tcp;

  const uint16_t port = argc > 1 ? static_cast<uint16_t>(std::atoi(argv[1])) : 6666;

  asio::io_context io_context;
  tcp::acceptor acceptor(io_context, tcp::endpoint(asio::ip::tcp::v4(), port));
  std::vector<std::shared_ptr<asio_tcp_rpc>> sessions;
  std::function<void()> do_accept;

  do_accept = [&] {
    acceptor.async_accept([&](const std::error_code& ec, tcp::socket socket) {
      if (ec) {
        return;
      }
      RPC_CORE_LOG("server on_session");

      auto session = asio_tcp_rpc::bind(std::make_shared<tcp::socket>(std::move(socket)));
      sessions.emplace_back(session);
      session->rpc()->subscribe("cmd", [](const std::string& msg) -> std::string {
        RPC_CORE_LOG("server on cmd: %s", msg.c_str());
        return "world";
      });
      std::weak_ptr<asio_tcp_rpc> weak_session = session;
      session->on_close = [&, weak_session](const std::error_code&) {
        RPC_CORE_LOG("server session on_close");
        auto session = weak_session.lock();
        if (!session) {
          return;
        }
        for (auto it = sessions.begin(); it != sessions.end(); ++it) {
          if (*it == session) {
            sessions.erase(it);
            break;
          }
        }
      };

      do_accept();
    });
  };
  do_accept();

  asio::signal_set signals(io_context, SIGINT, SIGTERM);
  signals.async_wait([&](const std::error_code&, int) {
    acceptor.close();
    for (auto& session : sessions) {
      session->close();
    }
    sessions.clear();
    io_context.stop();
  });

  RPC_CORE_LOG("rpc_s listening on 0.0.0.0:%u", port);
  io_context.run();
  return 0;
}
