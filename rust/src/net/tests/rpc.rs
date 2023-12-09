use std::rc::Rc;

use log::info;

use rpc_core::rpc::Rpc;
use rpc_core_net::{rpc_client, rpc_server};
use rpc_core_net::config_builder::RpcConfigBuilder;

#[test]
fn net_rpc() {
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
                let rpc = Rpc::new(None);
                rpc.subscribe("cmd", |msg: String| -> String {
                    info!("cmd: {msg}");
                    assert_eq!(msg, "hello");
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

                info!("server: start...");
                server.start();

                tokio::time::sleep(tokio::time::Duration::from_millis(1000)).await;
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
                let rpc = Rpc::new(None);
                let client = rpc_client::RpcClient::new(RpcConfigBuilder::new().rpc(Some(rpc.clone())).build());
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

                // wait server ready
                tokio::time::sleep(tokio::time::Duration::from_millis(200)).await;
                client.open("localhost", 6666);
                info!("client: start...");

                tokio::time::sleep(tokio::time::Duration::from_millis(200)).await;

                info!("usage: callback...");
                rpc.cmd("cmd")
                    .msg("hello")
                    .rsp(|msg: String| {
                        info!("### rsp: {msg}");
                        assert_eq!(msg, "world");
                    })
                    .call();

                info!("usage: future...");
                let result = rpc.cmd("cmd").msg("hello").future::<String>().await;
                info!("### rsp: {result:?}");
                assert_eq!(result.result.unwrap(), "world");
            }).await;
        });
    });

    thread_client.join().expect("thread_client: panic");
    thread_server.join().expect("thread_server: panic");
}
