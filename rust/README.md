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

Dropping a polled, pending `future()` cancels its logical call, including retries.
Use `reset_cancel()` before reusing that request. Dropping an unpolled future or
an old future after its call completed does not cancel a newer call.

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

# License

This project is licensed under the [MIT license](LICENSE).
