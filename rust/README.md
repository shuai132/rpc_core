# rpc-core

[![Build Status](https://github.com/shuai132/rpc_core/workflows/rust/badge.svg)](https://github.com/shuai132/rpc_core/actions?workflow=rust)
[![Latest version](https://img.shields.io/crates/v/rpc-core.svg)](https://crates.io/crates/rpc-core)
[![Documentation](https://docs.rs/rpc-core/badge.svg)](https://docs.rs/rpc-core)
![License](https://img.shields.io/crates/l/rpc-core.svg)

# Usage

Run the following Cargo command in your project directory:

```shell
cargo add rpc-core
```

Or add the following line to your Cargo.toml:

```toml
[dependencies]
rpc-core = { version = "0.3.2", features = ["net"] }
```

# Example

See [src/tests](src/tests) for details:

* receiver
    ```rust
    fn subscribe() {
        rpc_s.subscribe("cmd", |msg: String| -> String {
            assert_eq!(msg, "hello");
            "world".to_string()
        });
    }
    
    ```

* sender (callback)
    ```rust
    fn call() {
        rpc_c.cmd("cmd")
            .msg("hello")
            .rsp(|msg: String| {
                assert_eq!(msg, "world");
            })
            .call().expect("request rejected");
    }
    ```

* sender (future)
    ```rust
    async fn call() {
        let result = rpc_c.cmd("cmd").msg("hello").future::<String>().await;
        assert_eq!(result.result.unwrap(), "world");
    }
    ```

# Request reuse

Incoming messages must identify exactly one direction: command or response.
Messages with both or neither flag are ignored without invoking handlers,
sending replies or completing pending requests, matching the C++ dispatcher.

Omitting `.msg(...)` sends JSON `null`, equivalent to `.msg(())`. A command handler
can accept `()` for a no-argument call, and `rpc.ping().future::<()>()` completes
without requiring an explicit message. Explicit messages retain their own JSON encoding.

A request supports repeated calls after completion or cancellation, including
from its response or `finally` callback. Only one call may be active per request;
create separate requests for concurrent calls. `call()` and `call_with_rpc()`
return `Result<(), FinallyType>`: `Err(FinallyType::Busy)` leaves the active call
untouched, sends nothing and does not invoke `finally`.

An overlapping `future()` returns `FinallyType::Busy` without replacing the
original callbacks. Each accepted call snapshots its configuration. Builder
changes configure the next call; retries retain the original payload, RPC and
callbacks. A new call starts with the full configured retry budget, and retries
invoke `finally` only once for the logical call. After `cancel()`, call
`reset_cancel()` before reusing the request.
Canceling a request or resetting its session during custom response deserialization
discards that decoded result. If the request is reused, neither successful nor
failed decoding can finish the new call or invoke the old response callback.
With panic unwinding enabled, a panic in custom response decoding ends its still
active call with `RspSerializeError` and skips the response callback before
propagating. A call already canceled or reset is not completed again.
With panic unwinding enabled, a panic in a response callback still runs `finally`
once with that response's completion status before propagating. The request stays
alive through `finally`, and a new call started by the response callback remains
independent. A `finally` callback invoked during unwinding must not panic.
With panic unwinding enabled, a panic in a timeout callback ends its still-active
call with `Timeout` without retrying, and propagates. Its old timer registration
is removed; a call started after cancellation or session reset remains independent.

Replacing a response, timeout or `finally` callback releases its old captures
outside the request's internal borrow. Capture destructors may cancel or
reconfigure the request; for example, dropping a captured `Dispose` group cancels
its registered requests.
The same rule applies when replacing or removing command subscriptions and when
replacing the RPC timer implementation: released captures may cancel pending
requests, whose completion callbacks can reconfigure the RPC.
Command-name conversions for subscriptions and host conversions for client
connections also run without holding RPC state borrows, allowing custom
`ToString` implementations to reconfigure the same object.

Dropping a polled, pending `future()` cancels its logical call, including retries.
Use `reset_cancel()` before reusing that request. Dropping an unpolled future or
an old future after its call completed does not cancel a newer call.

A `Dispose` group cancels each distinct request at most once per `dismiss()`, in
registration order. Duplicate registrations do not cancel a new call started by
that request's cancellation callback. Requests may be registered again afterward.
With panic unwinding enabled, `dismiss()` cancels the entire detached batch even
if completion callbacks panic, then resumes the first panic.

# Logical sessions

An `Rpc` object represents a logical session. `set_ready(false)` marks a temporary
transport disconnect; `set_ready(true)` resumes the same session. Pending calls
and the sequence counter survive this transition, and existing deadlines continue
running. Keep both peers' RPC objects when resuming a session.

When a peer restarts or is replaced, call `reset_session()` before accepting its
packets. Old pending calls complete with `FinallyType::SessionReset`; subscriptions,
readiness and the sequence counter are retained. Completion callbacks may start
new calls, and old timeout registrations cannot finish those new calls. Responses
from a subscription handler that resets its own session are suppressed.
With panic unwinding enabled, reset completes every old call even if completion
callbacks panic, then resumes the first panic. Calls started by those callbacks
remain in the new session.

The adapter decides whether a connection resumes the old session. There is no
session handshake or automatic reply replay; the adapter must stop delivering
bytes from the old transport before attaching a different peer.
`Connection::send_package()` and its callback return `bool`: `true` means the
transport accepted the packet, and `false` means it could not send it. The RPC
layer maps rejection to `FinallyType::RpcNotReady`. Acceptance does not guarantee
delivery to the peer.

# Features

## net

See `examples` for details: [src/examples](src/examples)

* server
    ```rust
    fn server() {
        let rpc = Rpc::new(None);
        rpc.subscribe("cmd", |msg: String| -> String {
            assert_eq!(msg, "hello");
            "world".to_string()
        });
      
        let server = rpc_server::RpcServer::new(6666, RpcConfigBuilder::new().rpc(Some(rpc.clone())).build());
        server.start();
    }
    ```

* client
    ```rust
    async fn client() {
        let rpc = Rpc::new(None);
        let client = rpc_client::RpcClient::new(RpcConfigBuilder::new().rpc(Some(rpc.clone())).build());
        client.set_reconnect(1000);
        client.open("localhost", 6666);

        let result = rpc.cmd("cmd").msg("hello").future::<String>().await;
        assert_eq!(result.result.unwrap(), "world");
    }
    ```

## Network lifetime and heartbeat

Servers listen on IPv4 by default. Set `enable_ipv6(true)` in the configuration
builder to bind the IPv6 wildcard address (`::`); IPv4-mapped connections then
depend on the operating system's dual-stack policy. Clients can use an explicit
IPv6 address such as `::1` as the host.

TCP and RPC configurations default to a 16 MiB maximum frame body. Set
`max_body_size` explicitly to change this limit; zero disables it. Oversized
headers close the transport before reading the body. Receive buffers grow with
the bytes actually received, including when the limit is disabled.

Dropping a TCP or RPC client stops its connection/reconnect tasks and closes its
transport. Dropping a server stops accepting new connections; established server
sessions retain their own lifetime until their transport closes.
`RpcClient::stop()` is equivalent to `close()` and can be called through the shared
`Rc` returned by `new()`, including from callbacks. It cancels automatic reconnect;
an explicit `open()` can start the client again.
If replacing the previous RPC instance during reconnect finishes pending requests,
those completions run before `on_open`. If a completion closes or reopens the
client, that connection does not emit `on_open`; a subsequent connection can emit
its own event.
Network callback setters release replaced captures after releasing internal
borrows. Capture destructors may register new callbacks on the same client,
server, session or channel.
With panic unwinding enabled, a panic in a receive or close callback still closes
the channel and completes IO shutdown, allowing it to be reopened. Dropping IO
tasks before their first poll also releases the channel's active-task state.
Calling a server's `stop()` from its session callback also stops that listener
from accepting queued connections. A later `start()` creates a new listener.

Set both `ping_interval_ms` and `pong_timeout_ms` to enable heartbeat checks on a
client or server. A zero ping interval disables checks. Pings are sent one at a
time; the next interval starts after a successful pong. A missing or invalid
pong closes the transport, allowing a configured client reconnect to run.
Heartbeat requests are canceled when their transport closes or is replaced.

# License

This project is licensed under the [MIT license](LICENSE).
