use std::cell::RefCell;
use std::rc::{Rc, Weak};

use log::{debug, trace};

use rpc_core::rpc::{Rpc, RpcProto};

use crate::config::RpcConfig;
use crate::detail::tcp_channel::TcpChannel;
use crate::tcp_server::TcpServer;

pub struct RpcSession {
    pub rpc: RefCell<Rc<Rpc>>,
    on_close: RefCell<Option<Box<dyn Fn()>>>,
    channel: Weak<TcpChannel>,
}

impl RpcSession {
    pub fn new(rpc: Rc<Rpc>, channel: Weak<TcpChannel>) -> Rc<Self> {
        Rc::new(Self {
            rpc: rpc.into(),
            on_close: None.into(),
            channel,
        })
    }

    pub fn on_close<F>(&self, callback: F)
        where F: Fn() + 'static,
    {
        *self.on_close.borrow_mut() = Some(Box::new(callback));
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
    on_session: RefCell<Option<Box<dyn Fn(Weak<RpcSession>)>>>,
    this: RefCell<Weak<Self>>,
}

impl RpcServer {
    pub fn new(port: u16, config: RpcConfig) -> Rc<Self> {
        let tcp_config = config.to_tcp_config();
        let config = Rc::new(RefCell::new(config));
        let r = Rc::new(Self {
            config,
            server: TcpServer::new(port, tcp_config),
            on_session: None.into(),
            this: Weak::new().into(),
        });
        let this_weak = Rc::downgrade(&r);
        *r.this.borrow_mut() = this_weak.clone().into();
        r.server.on_session(move |session| {
            let this = this_weak.upgrade().unwrap();
            let tcp_channel = session.upgrade().unwrap();
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
                rpc.get_connection().borrow_mut().set_send_package_impl(Box::new(move |package: Vec<u8>| {
                    if let Some(rs) = rs_weak.upgrade() {
                        rs.channel.upgrade().unwrap().send(package);
                    }
                }));
            }
            {
                let rs_weak = rs_weak.clone();
                tcp_channel.on_data(move |package| {
                    if let Some(rs) = rs_weak.upgrade() {
                        rs.rpc.borrow().get_connection().borrow().on_recv_package(package);
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
                    rs.rpc.borrow_mut().set_ready(false);
                    // *tc_weak.upgrade().unwrap().on_close.borrow_mut() = None;
                });
            }
            rpc_session.rpc.borrow_mut().set_ready(true);

            {
                let on_session = this.on_session.borrow();
                if let Some(on_session) = on_session.as_ref() {
                    on_session(rs_weak);
                }
            }
        });
        r
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
        where F: Fn(Weak<RpcSession>) + 'static,
    {
        *self.on_session.borrow_mut() = Some(Box::new(callback));
    }
}

