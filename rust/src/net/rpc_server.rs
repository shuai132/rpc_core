use std::cell::RefCell;
use std::rc::{Rc, Weak};

use log::{debug, trace};

use crate::net::config::RpcConfig;
use crate::net::detail::heartbeat::Heartbeat;
use crate::net::detail::tcp_channel::TcpChannel;
use crate::net::tcp_server::TcpServer;
use crate::rpc::Rpc;

pub struct RpcSession {
    pub rpc: RefCell<Rc<Rpc>>,
    on_close: RefCell<Option<Box<dyn Fn()>>>,
    channel: Weak<TcpChannel>,
    heartbeat: RefCell<Option<Heartbeat>>,
}

impl RpcSession {
    pub fn new(rpc: Rc<Rpc>, channel: Weak<TcpChannel>) -> Rc<Self> {
        Rc::new(Self {
            rpc: rpc.into(),
            on_close: None.into(),
            channel,
            heartbeat: RefCell::new(None),
        })
    }

    fn close(&self) {
        let heartbeat = self.heartbeat.borrow_mut().take();
        drop(heartbeat);
        self.rpc.borrow().set_ready(false);
        let callback = self.on_close.borrow_mut().take();
        if let Some(callback) = callback {
            callback();
        }
    }

    pub fn on_close<F>(&self, callback: F)
    where
        F: Fn() + 'static,
    {
        let previous = self.on_close.borrow_mut().replace(Box::new(callback));
        drop(previous);
    }
}

impl Drop for RpcSession {
    fn drop(&mut self) {
        trace!("~RpcSession");
    }
}

pub struct RpcServer {
    config: Rc<RefCell<RpcConfig>>,
    server: Rc<TcpServer>,
    on_session: RefCell<Option<Rc<dyn Fn(Weak<RpcSession>)>>>,
    this: RefCell<Weak<Self>>,
}

impl RpcServer {
    pub fn new(port: u16, config: RpcConfig) -> Rc<Self> {
        Rc::new_cyclic(|this_weak| {
            let tcp_config = config.to_tcp_config();
            let config = Rc::new(RefCell::new(config));

            let r = Self {
                config,
                server: TcpServer::new(port, tcp_config),
                on_session: None.into(),
                this: this_weak.clone().into(),
            };

            let this_weak = this_weak.clone();
            r.server.on_session(move |session| {
                let Some(this) = this_weak.upgrade() else {
                    return;
                };
                let Some(tcp_channel) = session.upgrade() else {
                    return;
                };
                let rpc = if let Some(rpc) = this.config.borrow().rpc.clone() {
                    if rpc.is_ready() {
                        debug!("rpc already connected");
                        tcp_channel.close();
                        return;
                    }
                    rpc
                } else {
                    Rpc::new(None)
                };

                let rpc_session = RpcSession::new(rpc.clone(), Rc::downgrade(&tcp_channel));
                let rs_weak = Rc::downgrade(&rpc_session);

                {
                    let rs_weak = rs_weak.clone();
                    rpc.get_connection()
                        .borrow_mut()
                        .set_send_package_impl(Box::new(move |package: Vec<u8>| {
                            if let Some(rs) = rs_weak.upgrade() {
                                if let Some(channel) = rs.channel.upgrade() {
                                    return channel.send(package);
                                }
                            }
                            false
                        }));
                }
                {
                    let rs_weak = rs_weak.clone();
                    tcp_channel.on_data(move |package| {
                        if let Some(rs) = rs_weak.upgrade() {
                            rs.rpc
                                .borrow()
                                .get_connection()
                                .borrow()
                                .on_recv_package(package);
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
                    // bind rpc_session lifecycle to tcp_session and end with on_close
                    let rs = rpc_session.clone();
                    // let tc_weak = Rc::downgrade(&tcp_channel);
                    tcp_channel.on_close(move || {
                        rs.close();
                        // *tc_weak.upgrade().unwrap().on_close.borrow_mut() = None;
                    });
                }
                rpc_session.rpc.borrow_mut().set_ready(true);
                let channel = Rc::downgrade(&tcp_channel);
                let config = this.config.borrow();
                *rpc_session.heartbeat.borrow_mut() = Heartbeat::start(
                    &rpc,
                    config.ping_interval_ms,
                    config.pong_timeout_ms,
                    move || {
                        if let Some(channel) = channel.upgrade() {
                            channel.close();
                        }
                    },
                );
                drop(config);

                let callback = this.on_session.borrow().clone();
                if let Some(on_session) = callback {
                    on_session(rs_weak);
                }
            });
            r
        })
    }

    pub fn downgrade(&self) -> Weak<Self> {
        self.this.borrow().clone()
    }

    pub fn start(&self) {
        self.server.start();
    }

    pub fn stop(&self) {
        self.server.stop();
    }

    pub fn on_session<F>(&self, callback: F)
    where
        F: Fn(Weak<RpcSession>) + 'static,
    {
        let previous = self.on_session.borrow_mut().replace(Rc::new(callback));
        drop(previous);
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn session_close_notifies_user_once() {
        let rpc = Rpc::new(None);
        rpc.set_ready(true);
        let session = RpcSession::new(rpc.clone(), Weak::new());
        let count = Rc::new(RefCell::new(0));
        let count_copy = count.clone();
        session.on_close(move || *count_copy.borrow_mut() += 1);
        session.close();
        session.close();
        assert!(!rpc.is_ready());
        assert_eq!(*count.borrow(), 1);
    }
}
