#include RPC_CORE_FIRST_HEADER
#include "rpc_core.hpp"

#include "assert_def.h"

int main() {
  auto connections = rpc_core::loopback_connection::create();
  auto server = rpc_core::rpc::create(connections.first);
  auto client = rpc_core::rpc::create(connections.second);
  server->set_ready(true);
  client->set_ready(true);
  client->set_timer([](uint32_t, rpc_core::rpc::timeout_cb) {});
  server->subscribe("integer", [](int value) { return value + 1; });
  server->subscribe("string", [](std::string value) { return value + "!"; });
  int responses = 0;
  ASSERT(client->cmd("integer")->msg(41)->rsp([&](int value) {
    ASSERT(value == 42);
    ++responses;
  })->call());
  ASSERT(client->cmd("string")->msg(std::string("hello"))->rsp([&](std::string value) {
    ASSERT(value == "hello!");
    ++responses;
  })->call());
  ASSERT(responses == 2);
}
