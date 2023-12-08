use std::rc::Rc;

use log::info;

use rpc_core_net::{tcp_client, tcp_server};
use rpc_core_net::config_builder::TcpConfigBuilder;

#[test]
fn net_tcp() {
    std::env::set_var("RUST_LOG", "trace");
    env_logger::init();

    let thread_server = std::thread::spawn(|| {
        let runtime = tokio::runtime::Builder::new_current_thread()
            .enable_all()
            .build()
            .unwrap();

        runtime.block_on(async {
            let local = tokio::task::LocalSet::new();
            local.run_until(async move {
                let server = tcp_server::TcpServer::new(6666, TcpConfigBuilder::new().auto_pack(true).build());
                server.on_session(move |session| {
                    info!("session: on_open");
                    let session = session.upgrade().unwrap();
                    let sw = Rc::downgrade(&session);
                    session.on_data(move |data| {
                        let msg = String::from_utf8_lossy(data.as_slice());
                        info!("session: on_data: {}", msg);
                        assert_eq!(msg, "hello");
                        sw.upgrade().unwrap().send_str("world");
                    });
                    session.on_close(|| {
                        info!("session: on_close");
                    });
                });
                info!("server: start...");
                server.start();
                tokio::time::sleep(tokio::time::Duration::from_millis(200)).await;
            }).await;
        });
    });

    let thread_client = std::thread::spawn(|| {
        let runtime = tokio::runtime::Builder::new_current_thread()
            .enable_all()
            .build()
            .unwrap();

        runtime.block_on(async {
            let local = tokio::task::LocalSet::new();
            local.run_until(async move {
                let client = tcp_client::TcpClient::new(TcpConfigBuilder::new().auto_pack(true).build());
                let client_weak = client.downgrade();
                client.on_open(move || {
                    info!("client: on_open");
                    client_weak.upgrade().unwrap().send_str("hello");
                });
                client.on_open_failed(|e| {
                    info!("client: on_open_failed: {:?}", e);
                });
                client.on_data(|data| {
                    let msg = String::from_utf8_lossy(data.as_slice());
                    info!("client: on_data: {}", msg);
                    assert_eq!(msg, "world");
                });
                client.on_close(|| {
                    info!("client: on_close");
                });
                client.set_reconnect(1000);
                client.open("localhost", 6666);
                info!("client: start...");
                tokio::time::sleep(tokio::time::Duration::from_millis(100)).await;
            }).await;
        });
    });

    thread_client.join().expect("thread_client: panic");
    thread_server.join().expect("thread_server: panic");
}
