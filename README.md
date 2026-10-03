# rpc_core

[![Build Status](https://github.com/shuai132/rpc_core/workflows/cpp/badge.svg)](https://github.com/shuai132/rpc_core/actions?workflow=cpp)
[![Build Status](https://github.com/shuai132/rpc_core/workflows/rust/badge.svg)](https://github.com/shuai132/rpc_core/actions?workflow=rust)
[![Release](https://img.shields.io/github/release/shuai132/rpc_core.svg)](https://github.com/shuai132/rpc_core/releases)
[![License](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

a tiny C++14 rpc library, supports all platforms (macOS, Linux, Windows, iOS, Android, etc.) and most microchips (
Arduino, STM32, ESP32/ESP8266, etc.)

**Recommend TCP-based implementation: [asio_net](https://github.com/shuai132/asio_net)**

## Introduction

Full-feature rpc frameworks (e.g. `gRPC` and `bRPC`) are very complex and not suitable for use on embedded platforms.

This project offers a lightweight and user-friend rpc library that is better suited for one-to-one rpc calls.
It supports all platforms and a wide range of microchips, including Arduino, STM32, ESP32/ESP8266, and more.

Note:
This library only offers the protocol layer and API, it **does not** include the implementation of the transport layer.
For TCP-based implementation: [asio_net](https://github.com/shuai132/asio_net)

## Features

* Header-Only
* No-Schema
* No exceptions in the core RPC protocol; errors are returned as `result` values
* Support performance-limited platforms including microchips
* Support any connection type (`tcp socket`, `serial port`, etc.)
* High Performance Serialization, support most STL containers and user type
* Serialization plugins implementations for `flatbuffers` and `nlohmann::json`
* Support `co_await`, depend on `C++20` and `asio`, or custom implementation
* Support subscribe async callback, async coroutine, and custom scheduler
* RAII-based `dispose` for automatic cancel request
* Support timeout, retry, cancel api
* Comprehensive unittests

## TCP-based implementations

* C++
    - [asio_net](https://github.com/shuai132/asio_net): based on [asio](https://think-async.com/Asio/#)  
      Support macOS, Linux, Windows, iOS, Android, etc. and can be used on MCUs that support asio, such as ESP32.

* Rust
    - [./rust](./rust): based on [tokio](https://github.com/tokio-rs/tokio)  
      Support callback and async/await, details: [README.md](./rust/README.md)

## Requirements

* C++14
* Provide your connection implementation: [connection](include/rpc_core/connection.hpp)  
  NOTICE: complete data packets are required for data transmission, such as `websocket`.  
  If using `tcp socket`, `serial port`, etc., message pack and unpack need to be implemented.
  Or you can use [stream_connection](include/rpc_core/connection.hpp).
* Optional: C++20 (for coroutine api, co_await co_call)

With CMake, `RPC_CORE_FEATURE_CO_ASIO=ON` enables the Asio coroutine API and
propagates C++20, Asio include paths and thread linkage through the `rpc_core`
target. Set `ASIO_PATH` or `RPC_CORE_ASIO_INCLUDE_DIR` to locate Asio headers.
The RPC headers include Asio when this feature is enabled. `RPC_CORE_TEST_CO_ASIO`
only enables the coroutine regression target, keeping the core's C++14 default.

## Usage

Include `rpc_core.hpp` to obtain the inline RPC implementations. Declaration headers
such as `rpc_core/rpc.hpp`, `rpc_core/request.hpp` and `rpc_core/dispose.hpp` may be
included before it without changing serialization lookup.

* basic usage:  
  notice: both sender and receiver can use `subscribe` and `call` apis

```c++
// receiver
rpc->subscribe("cmd", [](const std::string& msg) -> std::string {
    assert(msg == "hello");
    return "world";
});

// sender
rpc->cmd("cmd")
    ->msg(std::string("hello"))
    ->rsp([](const std::string& rsp) {
      assert(rsp == "world");
    })
    ->call();
// or: rpc->call("cmd", std::string("hello"), [](const std::string& rsp) {});
```

* async response:  
  just use `request_response<>` type, with `scheduler` support

```c++
rpc->subscribe("cmd", [&](request_response<std::string, std::string> rr) {
    assert(rr->req == "hello");
    rr->rsp("world"); // call rr->rsp() when data is ready
}, scheduler);
```

Scheduled calls share the subscription's handler instance, including mutable
captures. Unsubscribing or replacing a subscription does not cancel already
scheduled calls; their tasks retain the original handler. A coroutine scheduler
must retain the task callable until the returned coroutine completes (see the
scheduler example in [test/test_rpc.cpp](test/test_rpc.cpp)).

* async call and response using c++20 coroutine:  
  here is an example using asio, custom async/coroutine implementation is supported

```c++
// receiver
rpc->subscribe("cmd", [&](request_response<std::string, std::string> rr) -> asio::awaitable<void> {
    assert(rr->req == "hello");
    asio::steady_timer timer(context);
    timer.expires_after(std::chrono::seconds(1));
    co_await timer.async_wait();
    rr->rsp("world");
}, scheduler_asio_coroutine);

// sender
// use C++20 co_await with asio, or you can use custom async implementation, and co_await it!
auto rsp = co_await rpc->cmd("cmd")->msg(std::string("hello"))->co_call<std::string>();
// or: auto rsp = co_await rpc->co_call<std::string>("cmd", std::string("hello"));
assert(rsp.data == "world");
```

Inspect the code for more
details: [rpc_s_coroutine.cpp](https://github.com/shuai132/asio_net/blob/main/test/rpc_s_coroutine.cpp)
and [rpc_c_coroutine.cpp](https://github.com/shuai132/asio_net/blob/main/test/rpc_c_coroutine.cpp)

* Addition:

1. `msg` and `rsp` support any serializable type, refer to [Serialization](#Serialization).
2. Detailed usages and unittests can be found here: [rpc_test.cpp](test/test_rpc.cpp)
3. There is an example shows custom async
   impl: [rpc_c_coroutine.hpp](https://github.com/shuai132/asio_net/blob/main/test/rpc_c_coroutine.hpp)

## Request reuse

A `request` can be called again after completion or cancellation, including from
its response or `finally` callback. Each request allows only one active call;
use separate requests for concurrent calls. `call()` returns `result<void>`:
`normal` means the request was sent, while `busy` rejects an overlapping call
without sending or invoking `finally`. Other immediate failures also complete
the accepted call through `finally`.
If the transport sender or timer registration throws, the still-active attempt
is unregistered and completed with `rpc_not_ready` before the exception propagates.
A response already completed, a synchronous retry, or a new call started during
that callback keeps its own outcome and registration.

`future()` and `co_call()` return a `busy` result for overlapping calls without
replacing the active call's callbacks. They retain replaced callback captures
until their own call has started, so capture destructors cannot start a call
with partially installed async callbacks. Every accepted call snapshots its
configuration. Builder changes configure the next call; retries retain the
original payload, RPC and callbacks. Retries belong to the same logical call,
consume a fresh retry budget for each new call, and invoke `finally` only once.
Reused callbacks retain their mutable captures across calls. Replacing a callback
configures a new instance for future calls; the active call keeps its old instance.
Passing an empty `std::function<void()>` or `std::function<void(finally_t)>` to
`finally()` clears the callback for future calls without affecting an active call.
After `cancel()`, use `reset_cancel()` before calling again. Asio `co_call()` also
responds to terminal coroutine cancellation; cancellation affects only that call,
including when a request is reused.
Canceling a request or resetting its session during custom response deserialization
discards that decoded result. If the request is reused, neither successful nor
failed decoding can finish the new call or invoke the old response callback.
If custom response decoding throws while its call is still active, the call ends
with `rsp_serialize_error` and skips the response callback; the exception still
propagates. A call already canceled or reset is not completed again.
If a response callback throws, `finally` still runs once with that response's
completion status before the exception propagates. The request stays alive through
`finally`, and a new call started by the response callback remains independent.
A `finally` callback invoked during stack unwinding must not throw.
If a timeout callback throws, its still-active call ends with `timeout` without
retrying, and the exception propagates. Its old timer registration is removed;
a call started after cancellation or session reset remains independent.

A `dispose` group cancels each distinct request at most once per `dismiss()`, in
registration order. Duplicate registrations do not cancel a new call started by
that request's cancellation callback. Requests may be registered again afterward.
Explicit `dismiss()` cancels the entire detached batch even if completion callbacks
throw, then rethrows the first exception. Callbacks used during `dispose`
destruction must not throw.

```c++
auto req = rpc->cmd("cmd")->msg(std::string("hello"))->rsp([](std::string) {});
auto started = req->call();
auto overlapping = req->call(); // busy if the first call is still active
req->cancel()->reset_cancel();
auto restarted = req->call();
```

## Logical sessions and reconnects

An RPC object represents a logical session. A transport disconnect only calls
`set_ready(false)`; reconnecting the same peer calls `set_ready(true)` and keeps
pending requests, deferred replies and the sequence counter. Existing deadlines
continue running while disconnected. Keep both peers' RPC objects for a resumed
session.

When a peer restarts or is replaced, call `reset_session()` on the retained RPC
object. It completes old pending calls with `finally_t::session_reset`, invalidates
old deferred replies, and retains subscriptions, readiness and the sequence
counter. Completion callbacks can immediately start new calls. Reset while
disconnected if those callbacks must not send until the new transport is bound.
If completion callbacks throw, reset still completes every old call, then rethrows
the first exception. Calls started by those callbacks remain in the new session.

The transport adapter determines whether a connection resumes the same logical
session; the protocol does not perform a session handshake. Adapters must stop
delivering bytes from the old transport before attaching a different peer.

```c++
rpc->set_ready(false);             // Transport disconnected.
// Same peer reconnects: rebind the transport, then set_ready(true).
// New logical peer: also call reset_session() before accepting its packets.
rpc->reset_session();
rpc->set_ready(true);              // New transport is bound and ready.
```

`request_response::rsp(value)` returns `result<void>`; existing calls may ignore
the return value. `normal` means the transport accepted the reply, not that the
peer acknowledged it. A reply made inside the subscription handler is sent
immediately, so the sender's callback may run before that handler returns.
`rpc_not_ready` indicates a disconnect or transport rejection, `session_reset`
indicates an obsolete session, and `rpc_expired` indicates a destroyed RPC.
Successful replies are sent once; subsequent attempts return `busy`. Failed
attempts are not cached. A `rpc_not_ready` attempt can be retried by the caller
with its own data after the same session reconnects. Notifications without a requested reply return
`no_need_rsp`.
Reentrant attempts during reply serialization or sending also return `busy`.
Exceptions from user serialization or transport code propagate to the caller and
release the in-flight state, allowing another attempt.

Connections provide `send_package_impl`, returning `bool`: `true` means the
transport accepted the packet, and `false` means it could not send it. The RPC
layer maps rejection to `rpc_not_ready`. Stream connections use the same contract
for `send_bytes_impl`. `on_recv_bytes()` returns `false` on invalid framing;
stop reading that stream and call `reset()` before feeding a new stream. Frames
received reentrantly are processed after the current input chunk. No automatic
reply caching, replay, delivery acknowledgement or session negotiation is provided.

`on_recv_bytes(nullptr, size)` with nonzero `size` returns `false` and requires
`reset()` before receiving again. A null buffer with zero length is a valid no-op
on a healthy stream. `data_packer::pack()` also rejects nonempty null buffers
before emitting any bytes, while `(nullptr, 0)` produces an empty frame.

A receive callback may destroy its stream connection. Any remaining frames in
that input chunk are discarded, and saved `on_recv_bytes` callbacks return `false`
after the connection is destroyed. Saved `send_package_impl` callbacks also return
`false` after their stream connection is destroyed.

Connection callbacks and `data_packer::on_data` retain the running callable when
it is replaced, cleared, or its owner is destroyed. They remain assignable from
lambdas and `std::function`; copied callbacks share the callable's mutable state.

## Serialization

High-performance and memory-saving binary serialization.

* api is very simple to use: [include/rpc_core/serialize_api.hpp](include/rpc_core/serialize_api.hpp)
* usage and comprehensive unittest: [test/test_serialize.cpp](test/test_serialize.cpp)
* the design balance cpu and memory usage, and zero-copy if possible.
* std::string is used as inner data container, it's serialize/deserialize is zero-overhead. so, it is recommended to use
  std::string whenever possible, using it to store binary data is also a good choice.

String decoding supports input views into the destination's own storage, including
subranges and wide-character data. Overlapping input is copied before the destination
changes; non-overlapping input continues to reuse the destination's capacity.

Container elements with length prefixes are validated before invoking their
decoders. Truncated element headers or bodies stop decoding without calling that
element's custom decoder. Earlier elements may already have been written.

Decoding `binary_wrap` validates the length and copies the payload before replacing
its storage. Invalid or truncated input leaves the destination unchanged. The input
may also refer to the destination's existing storage.

Enum decoding rejects values outside the underlying integer type's range and
leaves the destination unchanged on failure. The existing unsigned wire encoding,
including its representation of negative signed enum values, is unchanged.

Pair decoding supports writable reference members without changing their bindings.
Both elements are decoded before assigning them to the destination, so malformed
input leaves the pair and any referenced values unchanged. Const members cannot
be deserialization destinations.

The struct macros support packed scalar fields by copying their bytes through
aligned temporary values. Integer, floating-point, enum and pointer fields retain
their existing wire format. Non-scalar fields still require their normal object
alignment; packing does not make misaligned standard-library objects safe.

The binary format assumes little-endian hosts and fixes the floating-point formats:

| C++ type | Wire format | Size |
| --- | --- | --- |
| `float` | IEEE 754 binary32 | 4 bytes |
| `double` | IEEE 754 binary64 | 8 bytes |
| `long double` | IEEE 754 binary64 | 8 bytes |

The native type must match the specified format exactly. Serialization and
deserialization check the object size, IEEE 754 conformance, radix, precision
and exponent range at compile time. Unsupported types produce a static assertion
when used; merely including the library does not reject other supported types.
For example, an x87 extended80 or binary128 `long double` is unsupported. There
are no representation tags, padding or implicit conversions. Users must explicitly
convert unsupported types or provide custom serialization. An explicit narrowing
conversion can lose precision or exceed the destination range; the serializer
itself copies supported floating-point values without reducing their precision.

The `long double` format replaces the legacy 16-byte encoding. Both peers must
be upgraded together, and stored data using that format must be migrated; legacy
data is not auto-detected. The `float` and `double` wire formats are unchanged.

Integer `std::chrono::duration` counts are range-checked on decode. Unsigned
counts use unsigned encoding, so values above `INTMAX_MAX` no longer pass through
a signed intermediate. Older versions encoded those values as negative counts;
applications using that range must upgrade both peers and migrate stored values.
The encoding of counts from zero through `INTMAX_MAX` is unchanged.

### Why design a new serialization

Fist of all, I want to keep `rpc_core` library standalone, without any dependencies, except for STL.

Moreover, these serialization libraries do not align with my design goals:

* protobuf library is too large for some platforms, and it's Varint, Zigzag, and GZIP will use a lot of cpu.
* msgpack has similarity reason to protobuf.
* flatbuffers serialized data is too large.

Of course, when communicating across languages, it is recommended to use the above serialization libraries!

Finally, it also provides a way to use thirdparty serialization libraries directly, refer
to [Serialization Plugins](#Serialization-Plugins).

### Usage

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
| msgpack     |  20   |
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

JSON decoding failures log only a generic failure message; parser and conversion
exception text is omitted because it may contain message payloads. Rust response
serialization failures follow the same logging rule.
JSON backends and plugins reject null or empty input before parsing. JSON plugins
also skip already-failed archives, preserving the destination and avoiding calls to
application conversion functions.

* [flatbuffers.hpp](include/rpc_core/plugin/flatbuffers.hpp)  
  Supports using types generated by `flatbuffers` directly as message  
  (add the option `--gen-object-api` when using `flatc`)
  Decoding replaces the entire destination, including fields omitted from the new
  message. Invalid input or an already-failed archive leaves the destination unchanged.


* [json_msg.hpp](include/rpc_core/plugin/json_msg.hpp)  
  Supports using types supported by [nlohmann/json](https://github.com/nlohmann/json) directly as message  
  (the `to_json/from_json` rules in `nlohmann/json` need to be satisfied, and use `DEFINE_JSON_CLASS`).


* [json.hpp](include/rpc_core/plugin/json.hpp)  
  A flexible way to use `nlohmann/json`

# License

This project is licensed under the [MIT license](LICENSE).

# Links

* Implementation based on asio: [asio_net](https://github.com/shuai132/asio_net)


* Implementation suitable for ESP8266: [esp_rpc](https://github.com/shuai132/esp_rpc)
