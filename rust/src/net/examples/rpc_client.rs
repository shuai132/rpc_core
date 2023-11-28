use std::rc::Rc;

use log::info;
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
        let rpc = Rpc::new(None);
        let mut client = rpc_client::RpcClient::new(RpcConfigBuilder::new().rpc(Some(rpc.clone())).build());
        client.on_open(|_: Rc<Rpc>| {
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

            info!("usage: callback...");
            rpc.cmd("cmd")
                .msg("hello")
                .rsp(|msg: String| {
                    info!("### rsp: {msg}");
                })
                .call();

            info!("usage: future...");
            let result = rpc.cmd("cmd").msg("hello").future::<String>().await;
            info!("### rsp: {result:?}");
        }
    });
}
