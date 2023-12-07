use std::cell::RefCell;
use std::error::Error;
use std::rc::{Rc, Weak};

use rpc_core::connection::Connection;
use rpc_core::rpc::Rpc;

use crate::config::RpcConfig;
use crate::tcp_client::TcpClient;

pub struct RpcClientImpl {
    tcp_client: Rc<TcpClient>,
    config: RpcConfig,
    on_open: Option<Box<dyn Fn(Rc<Rpc>)>>,
    on_open_failed: Option<Box<dyn Fn(&dyn Error)>>,
    on_close: Option<Box<dyn Fn()>>,
    connection: Rc<RefCell<dyn Connection>>,
    rpc: Option<Rc<Rpc>>,
    this: Weak<RpcClient>,
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
                connection: rpc_core::connection::DefaultConnection::new(),
                rpc: None,
                this: Weak::new(),
            })
        });
        r.inner.borrow_mut().this = Rc::downgrade(&r);
        let this_weak = r.inner.borrow_mut().this.clone();
        r.inner.borrow_mut().tcp_client.on_open(move || {
            let this = this_weak.upgrade().unwrap();
            {
                let mut this = this.inner.borrow_mut();
                if let Some(rpc) = this.config.rpc.clone() {
                    this.connection = rpc.get_connection();
                    this.rpc = Some(rpc);
                } else {
                    this.rpc = Some(Rpc::new(Some(this.connection.clone())));
                }
            }

            {
                let this_weak = this_weak.clone();
                this.inner.borrow().connection.borrow_mut().set_send_package_impl(Box::new(move |package: Vec<u8>| {
                    if let Some(this) = this_weak.upgrade() {
                        this.inner.borrow().tcp_client.send(package);
                    }
                }));
            }
            {
                let this_weak = this_weak.clone();
                this.inner.borrow().tcp_client.on_data(move |package| {
                    if let Some(this) = this_weak.upgrade() {
                        this.inner.borrow().connection.borrow_mut().on_recv_package(package);
                    }
                });
            }

            this.inner.borrow_mut().rpc.as_ref().unwrap().set_timer(|ms: u32, handle: Box<dyn Fn()>| {
                tokio::task::spawn_local(async move {
                    tokio::time::sleep(tokio::time::Duration::from_millis(ms as u64)).await;
                    handle();
                });
            });
            {
                let this_weak = this_weak.clone();
                this.inner.borrow().tcp_client.on_close(move || {
                    let this = this_weak.upgrade().unwrap();
                    this.inner.borrow().rpc.as_ref().unwrap().set_ready(false);
                });
            }
            this.inner.borrow_mut().rpc.as_ref().unwrap().set_ready(true);

            {
                let this = this.inner.borrow();
                if let Some(on_open) = &this.on_open {
                    on_open(this.rpc.clone().unwrap());
                }
            }
        });
        r
    }

    pub fn open(&self, host: impl ToString, port: u16) {
        self.inner.borrow_mut().tcp_client.open(host, port);
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

    pub fn stop(&mut self) {
        self.close();
    }

    pub fn on_open<F>(&self, callback: F)
        where F: Fn(Rc<Rpc>) + 'static,
    {
        self.inner.borrow_mut().on_open = Some(Box::new(callback));
    }

    pub fn on_open_failed<F>(&self, callback: F)
        where F: Fn(&dyn Error) + 'static,
    {
        self.inner.borrow_mut().on_open_failed = Some(Box::new(callback));
    }

    pub fn on_close<F>(&self, callback: F)
        where F: Fn() + 'static,
    {
        self.inner.borrow_mut().on_close = Some(Box::new(callback));
    }
}

