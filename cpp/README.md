# rpc_core C++

The C++ code in this repository is primarily the protocol and API layer. The default include path still does not bind to a
specific communication library, which keeps the core usable on embedded and performance-constrained platforms where the
transport may be TCP, serial, CAN, BLE, WebSocket, or a custom packet channel.

For the same lightweight TCP shape as the JavaScript and Python SDKs, include the optional standalone Asio adapter:

```c++
#include "rpc_core/net/asio_tcp.hpp"

asio::io_context io;
auto client = rpc_core::net::asio_tcp_rpc::open(io, "127.0.0.1", 6666);
client->on_open = [](rpc_core::rpc_s rpc) {
  rpc->cmd("cmd")->msg(std::string("hello"))->rsp([](const std::string& rsp) {
    // ...
  })->call();
};
io.run();
```

This adapter is not included by `rpc_core.hpp`; users who need it must provide standalone
[asio](https://think-async.com/Asio/#) in their include path. It uses the native C++ serializer by default. For a fuller C++
networking layer with reconnect, SSL, UDP, serial, DDS, and discovery support, use [asio_net](https://github.com/shuai132/asio_net).

The adapter tests mirror the `asio_net` RPC examples:

```shell
cmake -S . -B build-asio -DRPC_CORE_TEST_ASIO=ON
cmake --build build-asio --target rpc_core_test_asio_tcp_rpc
./build-asio/rpc_core_test_asio_tcp_rpc
cmake --build build-asio --target rpc_core_test_asio_tcp_rpc_json
./build-asio/rpc_core_test_asio_tcp_rpc_json

cmake --build build-asio --target rpc_core_test_asio_tcp_rpc_s rpc_core_test_asio_tcp_rpc_c
./build-asio/rpc_core_test_asio_tcp_rpc_s 6666 &
sleep 0.1
./build-asio/rpc_core_test_asio_tcp_rpc_c 127.0.0.1 6666

cmake --build build-asio --target rpc_core_test_asio_tcp_rpc_s_json rpc_core_test_asio_tcp_rpc_c_json
```

## Cross-language payloads

When C++ needs to talk to the JavaScript, Python, or Rust SDKs, use JSON for request and response payload serialization:

```shell
cmake -S . -B build -DRPC_CORE_SERIALIZE_USE_NLOHMANN_JSON=ON
```

This defines `RPC_CORE_SERIALIZE_USE_NLOHMANN_JSON`, which selects the `nlohmann::json` serializer path. The transport framing
is still the compact binary RPC protocol; only the payload body is JSON. This gives cross-language compatibility without giving
up the efficient binary message header and stream framing.
