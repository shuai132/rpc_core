# rpc_core

[![Build Status](https://github.com/shuai132/rpc_core/workflows/cpp/badge.svg)](https://github.com/shuai132/rpc_core/actions?workflow=cpp)
[![Build Status](https://github.com/shuai132/rpc_core/workflows/rust/badge.svg)](https://github.com/shuai132/rpc_core/actions?workflow=rust)

a tiny c++11 rpc library, supports all platforms (macOS, Linux, Windows, iOS, Android, etc.) and most microchips (
Arduino, STM32, ESP32/ESP8266, etc.)

## Introduction

The complete rpc frameworks (such as `gRPC` and `brpc`) have complex functions
and are not practical on embedded platforms.

This project offers a lightweight and user-friendly rpc library that is better suited for one-to-one rpc calls.
It supports all platforms and a wide range of microchips, including Arduino, STM32, ESP32/ESP8266, and more.

Note:
This project solely offers the protocol layer and API,
and `does not include the implementation of the transport layer`.
For TCP-based implementations: [asio_net](https://github.com/shuai132/asio_net)

## Features

* Header-Only
* No-Schema
* Support performance-limited platforms including microchips
* Support any connection type (`tcp socket`, `serial port`, etc.)
* High Performance Serialization, support most STL containers and user type
* Serialization plugins implementations for `flatbuffers` and `nlohmann::json`
* RAII-based `dispose` for automatic cancel request
* Timeout and Retry API
* Support `std::future` interface
* Language support list: [C++](./include) [Rust](./rust)

## Requirements

* C++11
* Provide your connection implementation: [connection](include/rpc_core/connection.hpp)  
  NOTICE: complete data packets are required for data transmission, such as `websocket`.  
  If using `tcp socket`, `serial port`, etc., message pack and unpack need to be implemented.
  Or you can use [stream_connection](include/rpc_core/connection.hpp).

## Usage

```c++
// The Receiver
rpc->subscribe("cmd", [](const std::string& msg) -> std::string {
    assert(msg == "hello");
    return "world";
});

// The Sender
rpc->cmd("cmd")
    ->msg(std::string("hello"))
    ->rsp([](const std::string& rsp) {
      assert(rsp == "world");
    })
    ->call();
```

`msg` and `rsp` support any serializable type, see [Serialization](#Serialization).

Detailed usages and unittests can be found here: [rpc_test.cpp](test/test_rpc.cpp)

## Serialization

High-performance and memory-saving binary serialization.

For example, user data:

```c++
struct Type {
  uint8_t id = 1;
  uint8_t age = 18;
  std::string name = "test";
};
```

json: `{"id":1,"age":18,"name":"test"}`

| library     | bytes |
|-------------|:-----:|
| json        |  31   |
| flatbuffers |  44   |
| protobuf    |  10   |
| msgpack     |   8   |
| rpc_core    |   8   |

- [x] [std::string](https://en.cppreference.com/w/cpp/string/basic_string)
- [x] [std::wstring](https://en.cppreference.com/w/cpp/string/basic_string)
- [x] [std::array](https://en.cppreference.com/w/cpp/container/array)
- [x] [std::vector](https://en.cppreference.com/w/cpp/container/vector)
- [x] [std::list](https://en.cppreference.com/w/cpp/container/list)
- [x] [std::forward_list](https://en.cppreference.com/w/cpp/container/forward_list)
- [x] [std::deque](https://en.cppreference.com/w/cpp/container/deque)
- [x] [std::pair](https://en.cppreference.com/w/cpp/utility/pair)
- [x] [std::tuple](https://en.cppreference.com/w/cpp/utility/tuple)
- [x] [std::map](https://en.cppreference.com/w/cpp/container/map)
- [x] [std::unordered_map](https://en.cppreference.com/w/cpp/container/unordered_map)
- [x] [std::multimap](https://en.cppreference.com/w/cpp/container/multimap)
- [x] [std::unordered_multimap](https://en.cppreference.com/w/cpp/container/unordered_multimap)
- [x] [std::set](https://en.cppreference.com/w/cpp/container/set)
- [x] [std::unordered_set](https://en.cppreference.com/w/cpp/container/unordered_set)
- [x] [std::multiset](https://en.cppreference.com/w/cpp/container/multiset)
- [x] [std::unordered_multiset](https://en.cppreference.com/w/cpp/container/unordered_multiset)
- [x] [std::stack](https://en.cppreference.com/w/cpp/container/stack)
- [x] [std::queue](https://en.cppreference.com/w/cpp/container/queue)
- [x] [std::priority_queue](https://en.cppreference.com/w/cpp/container/priority_queue)
- [x] [std::bitset](https://en.cppreference.com/w/cpp/utility/bitset)
- [x] [std::complex](https://en.cppreference.com/w/cpp/numeric/complex)
- [x] [std::chrono::duration](https://en.cppreference.com/w/cpp/chrono/duration)
- [x] [std::chrono::time_point](https://en.cppreference.com/w/cpp/chrono/time_point)
- [x] [std::unique_ptr](https://en.cppreference.com/w/cpp/memory/unique_ptr)
- [x] [std::shared_ptr](https://en.cppreference.com/w/cpp/memory/shared_ptr)
- [x] [rpc_core::binary_wrap](include/rpc_core/serialize/binary_wrap.hpp)
- [x] [custom struct/class](test/serialize/CustomType.h)
  ```c++
  #include "rpc_core/serialize.hpp"
  struct TestStruct {
    uint8_t a;
    std::string b;
    OtherType c
    // RPC_CORE_DEFINE_TYPE_INNER(a, b, c);
  };
  RPC_CORE_DEFINE_TYPE(TestStruct, a, b, c);
  ```
  choose `RPC_CORE_DEFINE_TYPE` or `RPC_CORE_DEFINE_TYPE_INNER` for private member variable.

## Serialization Plugins

* [flatbuffers.hpp](include/rpc_core/plugin/flatbuffers.hpp)  
  Supports using types generated by `flatbuffers` directly as message  
  (add the option `--gen-object-api` when using `flatc`)


* [json_msg.hpp](include/rpc_core/plugin/json_msg.hpp)  
  Supports using types supported by [nlohmann/json](https://github.com/nlohmann/json) directly as message  
  (the `to_json/from_json` rules in `nlohmann/json` need to be satisfied, and use `DEFINE_JSON_CLASS`).


* [json.hpp](include/rpc_core/plugin/json.hpp)  
  A flexible way to use `nlohmann/json`

## Links

* Implementation based on asio: [asio_net](https://github.com/shuai132/asio_net)  
  support macOS, Linux, Windows, iOS, Android, etc. and can be used on MCUs that support asio, such as ESP32.


* Implementation suitable for ESP8266: [esp_rpc](https://github.com/shuai132/esp_rpc)
