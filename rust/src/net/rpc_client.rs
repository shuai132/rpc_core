use std::cell::RefCell;
use std::error::Error;
use std::rc::Rc;

use crate::connection::Connection;
use crate::net::config::RpcConfig;
use crate::net::detail::heartbeat::Heartbeat;
use crate::net::tcp_client::TcpClient;
use crate::rpc::Rpc;

pub struct RpcClientImpl {
    tcp_client: Rc<TcpClient>,
    config: RpcConfig,
    on_open: Option<Rc<dyn Fn(Rc<Rpc>)>>,
    on_open_failed: Option<Rc<dyn Fn(&dyn Error)>>,
    on_close: Option<Rc<dyn Fn()>>,
    connection: Rc<RefCell<dyn Connection>>,
    rpc: Option<Rc<Rpc>>,
    heartbeat: Option<Heartbeat>,
}

pub struct RpcClient {
    inner: RefCell<RpcClientImpl>,
}

impl RpcClient {
    pub fn new(config: RpcConfig) -> Rc<Self> {
        let r = Rc::new(Self {
            inner: RefCell::new(RpcClientImpl {
                tcp_client: TcpClient::new(config.to_tcp_config()),
                config,
                on_open: None,
                on_open_failed: None,
                on_close: None,
                connection: crate::connection::DefaultConnection::new(),
                rpc: None,
                heartbeat: None,
            }),
        });

        let this_weak = Rc::downgrade(&r);
        r.inner.borrow_mut().tcp_client.on_open(move || {
            let Some(this) = this_weak.upgrade() else {
                return;
            };
            let previous_heartbeat = this.inner.borrow_mut().heartbeat.take();
            drop(previous_heartbeat);
            // Callback installation may release captures that reenter the client.
            // This also applies when Rpc::new replaces a connection's receiver.
            let (configured_rpc, previous_connection, tcp_client) = {
                let inner = this.inner.borrow();
                (
                    inner.config.rpc.clone(),
                    inner.connection.clone(),
                    inner.tcp_client.clone(),
                )
            };
            let rpc = configured_rpc.unwrap_or_else(|| Rpc::new(Some(previous_connection.clone())));
            let connection = rpc.get_connection();
            let previous_rpc = {
                let mut inner = this.inner.borrow_mut();
                inner.connection = connection.clone();
                inner.rpc.replace(rpc.clone())
            };

            {
                let this_weak = this_weak.clone();
                connection
                    .borrow_mut()
                    .set_send_package_impl(Box::new(move |package: Vec<u8>| {
                        if let Some(this) = this_weak.upgrade() {
                            return this.inner.borrow().tcp_client.send(package);
                        }
                        false
                    }));
            }
            {
                let this_weak = this_weak.clone();
                tcp_client.on_data(move |package| {
                    if let Some(this) = this_weak.upgrade() {
                        let connection = this.inner.borrow().connection.clone();
                        connection.borrow().on_recv_package(package);
                    }
                });
            }

            rpc.set_timer(|ms: u32, handle: Box<dyn Fn()>| {
                tokio::task::spawn_local(async move {
                    tokio::time::sleep(tokio::time::Duration::from_millis(ms as u64)).await;
                    handle();
                });
            });
            {
                let this_weak = this_weak.clone();
                tcp_client.on_close(move || {
                    let Some(this) = this_weak.upgrade() else {
                        return;
                    };
                    let heartbeat = this.inner.borrow_mut().heartbeat.take();
                    drop(heartbeat);
                    this.inner.borrow().rpc.as_ref().unwrap().set_ready(false);
                    let callback = this.inner.borrow().on_close.clone();
                    if let Some(on_close) = callback {
                        on_close();
                    }
                });
            }
            rpc.set_ready(true);

            let (rpc, interval, timeout, tcp) = {
                let inner = this.inner.borrow();
                (
                    inner.rpc.clone().unwrap(),
                    inner.config.ping_interval_ms,
                    inner.config.pong_timeout_ms,
                    Rc::downgrade(&inner.tcp_client),
                )
            };
            let heartbeat = Heartbeat::start(&rpc, interval, timeout, move || {
                if let Some(tcp) = tcp.upgrade() {
                    tcp.disconnect();
                }
            });
            this.inner.borrow_mut().heartbeat = heartbeat;

            // Old pending completions may reconfigure the client. Finish binding
            // the new connection and release all borrows before running them.
            drop(previous_rpc);
            let (callback, rpc) = {
                let inner = this.inner.borrow();
                // A completion may close or reopen the client. Reopening closes
                // the old channel synchronously and connects asynchronously.
                if !inner.tcp_client.is_open() {
                    return;
                }
                (inner.on_open.clone(), inner.rpc.clone().unwrap())
            };
            if let Some(on_open) = callback {
                on_open(rpc);
            }
        });

        let this_weak = Rc::downgrade(&r);
        r.inner.borrow_mut().tcp_client.on_open_failed(move |e| {
            let Some(this) = this_weak.upgrade() else {
                return;
            };
            let callback = this.inner.borrow().on_open_failed.clone();
            if let Some(on_open_failed) = callback {
                on_open_failed(e);
            }
        });

        r
    }

    pub fn open(&self, host: impl ToString, port: u16) {
        let tcp_client = self.inner.borrow().tcp_client.clone();
        tcp_client.open(host, port);
    }

    pub fn close(&self) {
        self.inner.borrow().tcp_client.close();
    }

    pub fn set_reconnect(&self, ms: u32) {
        self.inner.borrow_mut().tcp_client.set_reconnect(ms);
    }

    pub fn cancel_reconnect(&self) {
        self.inner.borrow_mut().tcp_client.cancel_reconnect();
    }

    pub fn stop(&self) {
        self.close();
    }

    pub fn on_open<F>(&self, callback: F)
    where
        F: Fn(Rc<Rpc>) + 'static,
    {
        let previous = self.inner.borrow_mut().on_open.replace(Rc::new(callback));
        drop(previous);
    }

    pub fn on_open_failed<F>(&self, callback: F)
    where
        F: Fn(&dyn Error) + 'static,
    {
        let previous = self
            .inner
            .borrow_mut()
            .on_open_failed
            .replace(Rc::new(callback));
        drop(previous);
    }

    pub fn on_close<F>(&self, callback: F)
    where
        F: Fn() + 'static,
    {
        let previous = self.inner.borrow_mut().on_close.replace(Rc::new(callback));
        drop(previous);
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::detail::{
        coder,
        msg_wrapper::{MsgType, MsgWrapper},
    };
    use crate::net::config_builder::RpcConfigBuilder;
    use tokio::io::{AsyncReadExt, AsyncWriteExt};

    #[test]
    fn replaced_transport_captures_can_reconfigure_or_close_client() {
        struct OnDrop(Box<dyn Fn()>);
        impl Drop for OnDrop {
            fn drop(&mut self) {
                (self.0)();
            }
        }
        for callback in ["sender", "timer", "receiver"] {
            for close_during_setup in [false, true] {
                let runtime = tokio::runtime::Builder::new_current_thread()
                    .enable_all()
                    .build()
                    .unwrap();
                runtime.block_on(tokio::task::LocalSet::new().run_until(async {
                    tokio::time::timeout(std::time::Duration::from_secs(3), async {
                        let listener = tokio::net::TcpListener::bind("127.0.0.1:0").await.unwrap();
                        let rpc = Rpc::new(None);
                        let configured_rpc = (callback != "receiver").then(|| rpc.clone());
                        let client =
                            RpcClient::new(RpcConfigBuilder::new().rpc(configured_rpc).build());
                        let (tx, mut events) = tokio::sync::mpsc::unbounded_channel();
                        let opened = tx.clone();
                        client.on_open(move |_| {
                            opened.send("open").unwrap();
                        });
                        let weak = Rc::downgrade(&client);
                        let capture = OnDrop(Box::new(move || {
                            let client = weak.upgrade().unwrap();
                            let closed = tx.clone();
                            client.on_close(move || {
                                closed.send("close").unwrap();
                            });
                            client.cancel_reconnect();
                            if close_during_setup {
                                client.close();
                            }
                            tx.send("released").unwrap();
                        }));
                        if callback == "timer" {
                            rpc.set_timer(move |_, _| {
                                let _keep = &capture;
                            });
                        } else if callback == "receiver" {
                            let connection = client.inner.borrow().connection.clone();
                            connection
                                .borrow_mut()
                                .set_recv_package_impl(Box::new(move |_| {
                                    let _keep = &capture;
                                }));
                        } else {
                            rpc.get_connection()
                                .borrow_mut()
                                .set_send_package_impl(Box::new(move |_| {
                                    let _keep = &capture;
                                    true
                                }));
                        }
                        client.open("127.0.0.1", listener.local_addr().unwrap().port());
                        let (mut peer, _) = listener.accept().await.unwrap();
                        assert_eq!(events.recv().await, Some("released"));
                        if !close_during_setup {
                            assert_eq!(events.recv().await, Some("open"));
                            assert!(client.inner.borrow().rpc.as_ref().unwrap().is_ready());
                            client.close();
                        }
                        assert_eq!(events.recv().await, Some("close"));
                        assert_eq!(peer.read(&mut [0]).await.unwrap(), 0);
                        assert!(!client.inner.borrow().rpc.as_ref().unwrap().is_ready());
                        assert!(events.try_recv().is_err());
                    })
                    .await
                    .expect("capture reentry interrupted connection setup");
                }));
            }
        }
    }

    #[test]
    fn reconnect_completion_can_close_or_replace_the_connection() {
        for reopen in [false, true] {
            let runtime = tokio::runtime::Builder::new_current_thread()
                .enable_all()
                .build()
                .unwrap();
            runtime.block_on(tokio::task::LocalSet::new().run_until(async {
                tokio::time::timeout(std::time::Duration::from_secs(3), async {
                    let listener = tokio::net::TcpListener::bind("127.0.0.1:0").await.unwrap();
                    let port = listener.local_addr().unwrap().port();
                    let client = RpcClient::new(RpcConfigBuilder::new().build());
                    let (tx, mut events) = tokio::sync::mpsc::unbounded_channel();
                    let closed = tx.clone();
                    client.on_close(move || {
                        let _ = closed.send("close");
                    });
                    let opens = std::cell::Cell::new(0);
                    let weak = Rc::downgrade(&client);
                    client.on_open(move |rpc| {
                        opens.set(opens.get() + 1);
                        if opens.get() == 1 {
                            let weak = weak.clone();
                            let tx = tx.clone();
                            rpc.cmd("pending")
                                .msg(1)
                                .rsp(|_: i32| {})
                                .finally(move |status| {
                                    assert_eq!(status, crate::request::FinallyType::RpcExpired);
                                    let client = weak.upgrade().unwrap();
                                    client.close();
                                    if reopen {
                                        client.open("127.0.0.1", port);
                                    }
                                    tx.send("expired").unwrap();
                                })
                                .call()
                                .unwrap();
                        }
                        tx.send("open").unwrap();
                    });
                    client.set_reconnect(1);
                    client.open("127.0.0.1", port);
                    let (peer, _) = listener.accept().await.unwrap();
                    assert_eq!(events.recv().await, Some("open"));
                    drop(peer);
                    assert_eq!(events.recv().await, Some("close"));
                    let (_replaced_peer, _) = listener.accept().await.unwrap();
                    assert_eq!(events.recv().await, Some("expired"));
                    // The completion closed this connection before on_open could run.
                    assert_eq!(events.recv().await, Some("close"));
                    if reopen {
                        let (_new_peer, _) = listener.accept().await.unwrap();
                        assert_eq!(events.recv().await, Some("open"));
                        client.close();
                        assert_eq!(events.recv().await, Some("close"));
                    }
                    assert!(events.try_recv().is_err());
                })
                .await
                .expect("reentrant close interrupted connection lifecycle");
            }));
        }
    }

    #[test]
    fn reconnect_completions_can_reconfigure_client() {
        let runtime = tokio::runtime::Builder::new_current_thread()
            .enable_all()
            .build()
            .unwrap();
        runtime.block_on(tokio::task::LocalSet::new().run_until(async {
            tokio::time::timeout(std::time::Duration::from_secs(3), async {
                let listener = tokio::net::TcpListener::bind("127.0.0.1:0").await.unwrap();
                let client = RpcClient::new(RpcConfigBuilder::new().build());
                let (tx, mut events) = tokio::sync::mpsc::unbounded_channel();
                let opens = std::cell::Cell::new(0);
                let weak = Rc::downgrade(&client);
                client.on_open(move |rpc| {
                    opens.set(opens.get() + 1);
                    if opens.get() == 1 {
                        let weak = weak.clone();
                        let tx = tx.clone();
                        rpc.cmd("pending")
                            .msg(1)
                            .rsp(|_: i32| {})
                            .timeout_ms(10000)
                            .finally(move |status| {
                                assert_eq!(status, crate::request::FinallyType::RpcExpired);
                                let client = weak.upgrade().unwrap();
                                client.on_close(|| {});
                                client.cancel_reconnect();
                                tx.send("expired").unwrap();
                            })
                            .call()
                            .unwrap();
                    }
                    tx.send("open").unwrap();
                });
                client.set_reconnect(1);
                client.open("127.0.0.1", listener.local_addr().unwrap().port());
                let (peer, _) = listener.accept().await.unwrap();
                assert_eq!(events.recv().await, Some("open"));
                drop(peer);
                let (_peer, _) = listener.accept().await.unwrap();
                assert_eq!(events.recv().await, Some("expired"));
                assert_eq!(events.recv().await, Some("open"));
                client.close();
            })
            .await
            .expect("reconnect callback interrupted connection setup");
        }));
    }

    #[test]
    fn callbacks_can_reconfigure_the_client() {
        let runtime = tokio::runtime::Builder::new_current_thread()
            .enable_all()
            .build()
            .unwrap();
        runtime.block_on(tokio::task::LocalSet::new().run_until(async {
            tokio::time::timeout(std::time::Duration::from_secs(3), async {
                let listener = tokio::net::TcpListener::bind("127.0.0.1:0").await.unwrap();
                let port = listener.local_addr().unwrap().port();
                let unavailable = tokio::net::TcpListener::bind("127.0.0.1:0").await.unwrap();
                let closed_port = unavailable.local_addr().unwrap().port();
                drop(unavailable);
                let client = RpcClient::new(RpcConfigBuilder::new().build());
                let (tx, mut events) = tokio::sync::mpsc::unbounded_channel();
                let weak = Rc::downgrade(&client);
                let failed_tx = tx.clone();
                client.on_open_failed(move |_| {
                    let client = weak.upgrade().unwrap();
                    client.on_open_failed(|_| panic!("unexpected connection failure"));
                    client.open("127.0.0.1", port);
                    failed_tx.send("failed").unwrap();
                });
                let weak = Rc::downgrade(&client);
                let open_tx = tx.clone();
                client.on_open(move |_| {
                    let client = weak.upgrade().unwrap();
                    client.on_open(|_| {});
                    client.set_reconnect(1000);
                    client.cancel_reconnect();
                    client.close();
                    open_tx.send("open").unwrap();
                });
                let weak = Rc::downgrade(&client);
                client.on_close(move || {
                    weak.upgrade().unwrap().on_close(|| {});
                    tx.send("close").unwrap();
                });
                client.open("127.0.0.1", closed_port);
                let (_peer, _) = listener.accept().await.unwrap();
                for expected in ["failed", "open", "close"] {
                    assert_eq!(events.recv().await, Some(expected));
                }
            })
            .await
            .unwrap();
        }));
    }

    #[test]
    fn client_can_reply_to_server_commands() {
        let runtime = tokio::runtime::Builder::new_current_thread()
            .enable_all()
            .build()
            .unwrap();
        runtime.block_on(tokio::task::LocalSet::new().run_until(async {
            tokio::time::timeout(std::time::Duration::from_secs(3), async {
                let listener = tokio::net::TcpListener::bind("127.0.0.1:0").await.unwrap();
                let rpc = Rpc::new(None);
                rpc.subscribe("reverse", |value: String| value);
                let client = RpcClient::new(RpcConfigBuilder::new().rpc(Some(rpc)).build());
                let (ready_tx, ready_rx) = tokio::sync::oneshot::channel();
                let ready_tx = RefCell::new(Some(ready_tx));
                client.on_open(move |_| {
                    ready_tx.borrow_mut().take().unwrap().send(()).unwrap();
                });
                client.open("127.0.0.1", listener.local_addr().unwrap().port());
                let (mut peer, _) = listener.accept().await.unwrap();
                ready_rx.await.unwrap();
                let mut command = MsgWrapper::new();
                command.seq = 7;
                command.cmd = "reverse".into();
                command.type_ = MsgType::Command | MsgType::NeedRsp;
                command.data = serde_json::to_vec("hello").unwrap();
                let payload = coder::serialize(&command).unwrap();
                peer.write_all(&(payload.len() as u32).to_le_bytes())
                    .await
                    .unwrap();
                peer.write_all(&payload).await.unwrap();
                let size = peer.read_u32_le().await.unwrap();
                let mut response = vec![0; size as usize];
                peer.read_exact(&mut response).await.unwrap();
                let response = coder::deserialize(&response).unwrap();
                assert_eq!(response.seq, 7);
                assert_eq!(response.unpack_as::<String>().unwrap(), "hello");
                client.close();
            })
            .await
            .expect("client failed to reply");
        }));
    }
}

impl Drop for RpcClient {
    fn drop(&mut self) {
        let inner = self.inner.get_mut();
        inner.heartbeat.take();
        inner.tcp_client.close();
        if let Some(rpc) = &inner.rpc {
            rpc.set_ready(false);
        }
    }
}
