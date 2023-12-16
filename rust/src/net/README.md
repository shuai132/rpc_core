# rpc-core-net

[![Build Status](https://github.com/shuai132/rpc_core/workflows/rust/badge.svg)](https://github.com/shuai132/rpc_core/actions?workflow=rust)
[![Latest version](https://img.shields.io/crates/v/rpc-core-net.svg)](https://crates.io/crates/rpc-core-net)
[![Documentation](https://docs.rs/rpc-core-net/badge.svg)](https://docs.rs/rpc-core-net)
![License](https://img.shields.io/crates/l/rpc-core-net.svg)

# Usage

Run the following Cargo command in your project directory:

```shell
cargo add rpc-core-net
```

Or add the following line to your Cargo.toml:

```toml
[dependencies]
rpc-core-net = "0.2.2"
```

# Example

See `examples` for details: [examples](examples)

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
