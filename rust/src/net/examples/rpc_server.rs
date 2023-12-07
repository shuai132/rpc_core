use log::info;

use rpc_core::rpc::Rpc;
use rpc_core_net::config_builder::RpcConfigBuilder;
use rpc_core_net::rpc_server;

fn main() {
    std::env::set_var("RUST_LOG", "trace");
    env_logger::init();

    let runtime = tokio::runtime::Builder::new_current_thread()
        .enable_all()
        .build()
        .unwrap();

    runtime.block_on(async {
        let local = tokio::task::LocalSet::new();
        local.run_until(async move {
            let rpc = Rpc::new(None);
            rpc.subscribe("cmd", |msg: String| -> String {
                info!("cmd: {msg}");
                "world".to_string()
            });

            let server = rpc_server::RpcServer::new(6666, RpcConfigBuilder::new().rpc(Some(rpc.clone())).build());
            server.on_session(move |session| {
                info!("on_open");
                let session = session.upgrade().unwrap();
                session.on_close(|| {
                    info!("on_close");
                });
            });

            info!("start...");
            server.start();
            loop {
                tokio::time::sleep(tokio::time::Duration::from_secs(1000)).await;
            }
        }).await;
    });
}

