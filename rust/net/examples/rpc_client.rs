use std::rc::Rc;
use log::info;
use rpc_core::connection::DefaultConnection;
use rpc_core::rpc::Rpc;
use rpc_core_net::config_builder::RpcConfigBuilder;

use rpc_core_net::rpc_client;

fn main() {
    std::env::set_var("RUST_LOG", "trace");
    env_logger::init();

    let runtime = tokio::runtime::Builder::new_current_thread()
        .enable_all()
        .build()
        .unwrap();

    runtime.block_on(async {
        let rpc = rpc_core::rpc::create(Some(DefaultConnection::create()));
        let mut client = rpc_client::RpcClient::new(RpcConfigBuilder::new().rpc(Some(rpc.clone())).build());
        client.on_open(move |_: Rc<Rpc>| {
            info!("on_open");
        });
        client.on_open_failed(|e| {
            info!("on_open_failed: {:?}", e);
        });
        client.on_close(|| {
            info!("on_close");
        });
        client.set_reconnect(1000);
        client.open("localhost", 6666);
        info!("start...");

        loop {
            tokio::time::sleep(tokio::time::Duration::from_secs(1)).await;

            info!("rpc...");
            rpc.cmd("cmd")
                .msg("hello")
                .rsp(move |msg: String| {
                    info!("rsp: {msg}");
                })
                .call();
        }
    });
}
