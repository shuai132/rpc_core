use log::info;
use rpc_core_net::config_builder::TcpConfigBuilder;
use rpc_core_net::tcp_client;

fn main() {
    std::env::set_var("RUST_LOG", "trace");
    env_logger::init();

    let runtime = tokio::runtime::Builder::new_current_thread()
        .enable_all()
        .build()
        .unwrap();

    runtime.block_on(async {
        let mut client = tcp_client::TcpClient::new(TcpConfigBuilder::new().auto_pack(true).build());
        let client_weak = client.downgrade();
        client.on_open(move || {
            info!("on_open");
            client_weak.unwrap().send_str("hello from client");
        });
        client.on_open_failed(|e| {
            info!("on_open_failed: {:?}", e);
        });
        client.on_data(|data| {
            info!("on_data: {}", String::from_utf8_lossy(data.as_slice()));
        });
        client.on_close(|| {
            info!("on_close");
        });
        client.set_reconnect(1000);
        client.open("localhost", 6666);
        info!("start...");

        loop {
            tokio::time::sleep(tokio::time::Duration::from_secs(1000)).await;
        }
    });
}

