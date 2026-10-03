#![cfg(feature = "net")]

use rpc_core::net::{
    config::TcpConfig, config_builder::RpcConfigBuilder, rpc_client::RpcClient,
    rpc_server::RpcServer, tcp_client::TcpClient, tcp_server::TcpServer,
};
use std::rc::Rc;
use std::time::Duration;
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio::net::{TcpListener, TcpStream};
use tokio::time::timeout;

fn run(future: impl std::future::Future<Output = ()>) {
    let runtime = tokio::runtime::Builder::new_current_thread()
        .enable_all()
        .build()
        .unwrap();
    runtime.block_on(tokio::task::LocalSet::new().run_until(async {
        timeout(Duration::from_secs(3), future).await.unwrap();
    }));
}

#[test]
fn dropping_connected_client_closes_socket() {
    run(async {
        let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
        let client = TcpClient::new(TcpConfig::new());
        let weak = client.downgrade();
        let (tx, mut rx) = tokio::sync::mpsc::unbounded_channel();
        client.on_open(move || {
            tx.send(()).unwrap();
        });
        client.open("127.0.0.1", listener.local_addr().unwrap().port());
        let (mut peer, _) = listener.accept().await.unwrap();
        rx.recv().await.unwrap();
        drop(client);
        assert!(weak.upgrade().is_none());
        assert_eq!(peer.read(&mut [0]).await.unwrap(), 0);
    });
}

#[test]
fn dropping_client_cancels_connection_and_reconnect_tasks() {
    run(async {
        let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
        let port = listener.local_addr().unwrap().port();
        let client = TcpClient::new(TcpConfig::new());
        let weak = client.downgrade();
        client.open("127.0.0.1", port);
        drop(client);
        assert!(weak.upgrade().is_none());
        assert!(timeout(Duration::from_millis(30), listener.accept())
            .await
            .is_err());

        let client = TcpClient::new(TcpConfig::new());
        let weak = client.downgrade();
        let (tx, mut rx) = tokio::sync::mpsc::unbounded_channel();
        client.on_close(move || {
            tx.send(()).unwrap();
        });
        client.set_reconnect(30);
        client.open("127.0.0.1", port);
        let (peer, _) = listener.accept().await.unwrap();
        drop(peer);
        rx.recv().await.unwrap();
        drop(client);
        assert!(weak.upgrade().is_none());
        assert!(timeout(Duration::from_millis(60), listener.accept())
            .await
            .is_err());
    });
}

async fn unused_port() -> u16 {
    TcpListener::bind("127.0.0.1:0")
        .await
        .unwrap()
        .local_addr()
        .unwrap()
        .port()
}

#[test]
fn ipv6_enabled_servers_accept_ipv6_clients() {
    run(async {
        let probe = match TcpListener::bind("[::1]:0").await {
            Ok(probe) => probe,
            Err(error) => {
                eprintln!("IPv6 loopback is unavailable: {error}");
                return;
            }
        };
        let port = probe.local_addr().unwrap().port();
        drop(probe);
        let mut config = TcpConfig::new();
        config.enable_ipv6 = true;
        let server = TcpServer::new(port, config);
        server.start();
        tokio::task::yield_now().await;
        let peer = TcpStream::connect(("::1", port)).await.unwrap();
        drop(peer);
        server.stop();
        tokio::task::yield_now().await;

        let server = RpcServer::new(port, RpcConfigBuilder::new().enable_ipv6(true).build());
        server.on_session(|session| {
            session
                .upgrade()
                .unwrap()
                .rpc
                .borrow()
                .subscribe("echo", |value: String| value);
        });
        server.start();
        tokio::task::yield_now().await;
        let client = RpcClient::new(RpcConfigBuilder::new().build());
        let (tx, mut rx) = tokio::sync::mpsc::unbounded_channel();
        client.on_open(move |rpc| {
            tx.send(rpc).unwrap();
        });
        client.open("::1", port);
        let rpc = rx.recv().await.unwrap();
        let result = rpc.cmd("echo").msg("ipv6").future::<String>().await;
        assert_eq!(result.unwrap(), "ipv6");
        client.close();
        server.stop();
    });
}

#[test]
fn stop_inside_session_callback_discards_queued_connections() {
    run(async {
        let port = unused_port().await;
        let server = TcpServer::new(port, TcpConfig::new());
        let weak = server.downgrade();
        let accepted = Rc::new(std::cell::Cell::new(0));
        let count = accepted.clone();
        let (tx, mut rx) = tokio::sync::mpsc::unbounded_channel();
        server.on_session(move |session| {
            count.set(count.get() + 1);
            weak.upgrade().unwrap().stop();
            session.upgrade().unwrap().close();
            tx.send(()).unwrap();
        });
        server.start();
        tokio::task::yield_now().await;
        // Queue both handshakes before allowing the local accept task to run.
        let address = std::net::SocketAddr::from(([127, 0, 0, 1], port));
        let _first =
            std::net::TcpStream::connect_timeout(&address, Duration::from_secs(1)).unwrap();
        let _second =
            std::net::TcpStream::connect_timeout(&address, Duration::from_secs(1)).unwrap();
        rx.recv().await.unwrap();
        tokio::task::yield_now().await;
        assert_eq!(accepted.get(), 1);
        server.start();
        tokio::task::yield_now().await;
        let _fresh = TcpStream::connect(("127.0.0.1", port)).await.unwrap();
        rx.recv().await.unwrap();
        assert_eq!(accepted.get(), 2);
    });
}

#[test]
fn dropping_servers_stops_listening() {
    run(async {
        let port = unused_port().await;
        let server = TcpServer::new(port, TcpConfig::new());
        let weak = server.downgrade();
        server.start();
        tokio::task::yield_now().await;
        let peer = TcpStream::connect(("127.0.0.1", port)).await.unwrap();
        drop(peer);
        drop(server);
        assert!(weak.upgrade().is_none());
        tokio::task::yield_now().await;
        assert!(TcpStream::connect(("127.0.0.1", port)).await.is_err());

        let server = RpcServer::new(port, RpcConfigBuilder::new().build());
        server.start();
        tokio::task::yield_now().await;
        drop(server);
        tokio::task::yield_now().await;
        assert!(TcpStream::connect(("127.0.0.1", port)).await.is_err());
    });
}

#[test]
fn immediate_server_restart_releases_old_listener() {
    run(async {
        let port = unused_port().await;
        let server = TcpServer::new(port, TcpConfig::new());
        server.start();
        tokio::task::yield_now().await;
        server.stop();
        server.start();
        tokio::task::yield_now().await;
        let _peer = TcpStream::connect(("127.0.0.1", port)).await.unwrap();
        server.stop();
    });
}

async fn read_ping(peer: &mut TcpStream) -> Vec<u8> {
    let size = peer.read_u32_le().await.unwrap();
    let mut payload = vec![0; size as usize];
    peer.read_exact(&mut payload).await.unwrap();
    assert!(payload.len() >= 7);
    let cmd_len = u16::from_le_bytes([payload[4], payload[5]]) as usize;
    assert_eq!(payload[6 + cmd_len] & 0x0d, 0x0d);
    payload
}

async fn pong(peer: &mut TcpStream, mut payload: Vec<u8>) {
    let cmd_len = u16::from_le_bytes([payload[4], payload[5]]) as usize;
    payload[6 + cmd_len] = 2 | 16;
    peer.write_all(&(payload.len() as u32).to_le_bytes())
        .await
        .unwrap();
    peer.write_all(&payload).await.unwrap();
}

#[test]
fn client_heartbeat_accepts_pong_and_closes_on_timeout() {
    run(async {
        let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
        let client = RpcClient::new(
            RpcConfigBuilder::new()
                .ping_interval_ms(5)
                .pong_timeout_ms(50)
                .build(),
        );
        let (tx, mut rx) = tokio::sync::mpsc::unbounded_channel();
        client.on_open(move |rpc| {
            tx.send(rpc).unwrap();
        });
        client.open("127.0.0.1", listener.local_addr().unwrap().port());
        let (mut peer, _) = listener.accept().await.unwrap();
        let rpc = rx.recv().await.unwrap();
        for _ in 0..2 {
            let ping = read_ping(&mut peer).await;
            pong(&mut peer, ping).await;
        }
        let _ping = read_ping(&mut peer).await;
        assert_eq!(peer.read(&mut [0]).await.unwrap(), 0);
        tokio::task::yield_now().await;
        assert!(!rpc.is_ready());
        client.close();
    });
}

#[test]
fn server_heartbeat_and_client_drop_release_pending_ping() {
    run(async {
        let port = unused_port().await;
        let server = RpcServer::new(
            port,
            RpcConfigBuilder::new()
                .ping_interval_ms(5)
                .pong_timeout_ms(50)
                .build(),
        );
        server.start();
        tokio::task::yield_now().await;
        let mut peer = TcpStream::connect(("127.0.0.1", port)).await.unwrap();
        let ping = read_ping(&mut peer).await;
        pong(&mut peer, ping).await;
        let _ping = read_ping(&mut peer).await;
        assert_eq!(peer.read(&mut [0]).await.unwrap(), 0);
        server.stop();

        let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
        let client = RpcClient::new(
            RpcConfigBuilder::new()
                .ping_interval_ms(5)
                .pong_timeout_ms(10000)
                .build(),
        );
        let weak = Rc::downgrade(&client);
        client.open("127.0.0.1", listener.local_addr().unwrap().port());
        let (mut peer, _) = listener.accept().await.unwrap();
        let _ping = read_ping(&mut peer).await;
        drop(client);
        assert!(weak.upgrade().is_none());
        assert_eq!(peer.read(&mut [0]).await.unwrap(), 0);
    });
}

#[test]
fn old_heartbeat_cannot_close_reconnected_or_reset_session() {
    run(async {
        let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
        let rpc = rpc_core::rpc::Rpc::new(None);
        let client = RpcClient::new(
            RpcConfigBuilder::new()
                .rpc(Some(rpc.clone()))
                .ping_interval_ms(5)
                .pong_timeout_ms(40)
                .build(),
        );
        client.set_reconnect(1);
        client.open("127.0.0.1", listener.local_addr().unwrap().port());
        let (mut old_peer, _) = listener.accept().await.unwrap();
        let _old_ping = read_ping(&mut old_peer).await;
        drop(old_peer);
        let (mut peer, _) = listener.accept().await.unwrap();
        for _ in 0..10 {
            let ping = read_ping(&mut peer).await;
            pong(&mut peer, ping).await;
        }
        assert!(rpc.is_ready());
        let obsolete_ping = read_ping(&mut peer).await;
        rpc.reset_session();
        pong(&mut peer, obsolete_ping).await;
        let ping = read_ping(&mut peer).await;
        pong(&mut peer, ping).await;
        assert!(rpc.is_ready());
        client.close();
    });
}

#[test]
fn default_rpc_frame_limit_rejects_header_without_waiting_for_body() {
    run(async {
        let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
        let client = RpcClient::new(RpcConfigBuilder::new().build());
        let (tx, mut closed) = tokio::sync::mpsc::unbounded_channel();
        client.on_close(move || {
            tx.send(()).unwrap();
        });
        client.open("127.0.0.1", listener.local_addr().unwrap().port());
        let (mut peer, _) = listener.accept().await.unwrap();
        peer.write_all(&u32::MAX.to_le_bytes()).await.unwrap();
        closed.recv().await.unwrap();
        assert_eq!(peer.read(&mut [0]).await.unwrap(), 0);
        client.close();
    });
}
