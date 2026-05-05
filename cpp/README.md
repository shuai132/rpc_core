# rpc_core C++

The C++ code in this repository is the protocol and API layer. It intentionally does not bind to a specific communication library.
That keeps the core usable on embedded and performance-constrained platforms where the transport may be TCP, serial, CAN, BLE,
WebSocket, or a custom packet channel.

For a ready-to-use C++ TCP implementation, use [asio_net](https://github.com/shuai132/asio_net). It is based on
[asio](https://think-async.com/Asio/#) and provides the network integration around this RPC core.

## Cross-language payloads

When C++ needs to talk to the JavaScript, Python, or Rust SDKs, use JSON for request and response payload serialization:

```shell
cmake -S . -B build -DRPC_CORE_SERIALIZE_USE_NLOHMANN_JSON=ON
```

This defines `RPC_CORE_SERIALIZE_USE_NLOHMANN_JSON`, which selects the `nlohmann::json` serializer path. The transport framing is
still the compact binary RPC protocol; only the payload body is JSON. This gives cross-language compatibility without giving up
the efficient binary message header and stream framing.
