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
rpc-core = { version = "0.3.0", features = ["net"] }
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
            .call();
    }
    ```

* sender (future)
    ```rust
    async fn call() {
        let result = rpc_c.cmd("cmd").msg("hello").future::<String>().await;
        assert_eq!(result.result.unwrap(), "world");
    }
    ```

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
    fn client() {
        let rpc = Rpc::new(None);
        let client = rpc_client::RpcClient::new(RpcConfigBuilder::new().rpc(Some(rpc.clone())).build());
        client.set_reconnect(1000);
        client.open("localhost", 6666);

        let result = rpc_c.cmd("cmd").msg("hello").future::<String>().await;
        assert_eq!(result.result.unwrap(), "world");
    }
    ```

# License

This project is licensed under the [MIT license](LICENSE).
