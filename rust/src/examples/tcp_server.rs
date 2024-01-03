use log::info;

use rpc_core::net::config_builder::TcpConfigBuilder;
use rpc_core::net::tcp_server;

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
            let server = tcp_server::TcpServer::new(6666, TcpConfigBuilder::new().auto_pack(false).build());
            server.on_session(move |session| {
                info!("on_open");
                let session = session.upgrade().unwrap();
                session.on_data(|data| {
                    info!("on_data: {}", String::from_utf8_lossy(data.as_slice()));
                });
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

