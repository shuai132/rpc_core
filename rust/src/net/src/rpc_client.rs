use std::cell::RefCell;
use std::error::Error;
use std::rc::Rc;
use std::sync::atomic::{AtomicPtr, Ordering};

use rpc_core::base::this::{SharedPtr, WeakPtr};
use rpc_core::connection::Connection;
use rpc_core::rpc::Rpc;

use crate::config::RpcConfig;
use crate::tcp_client::TcpClient;

pub struct RpcClient {
    tcp_client: Box<TcpClient>,
    config: RpcConfig,
    on_open: Option<Box<dyn Fn(Rc<Rpc>)>>,
    on_open_failed: Option<Box<dyn Fn(&dyn Error)>>,
    on_close: Option<Box<dyn Fn()>>,
    connection: Rc<RefCell<dyn Connection>>,
    rpc: Option<Rc<Rpc>>,
    this: SharedPtr<Self>,
}

impl RpcClient {
    pub fn new(config: RpcConfig) -> Box<Self> {
        let mut r = Box::new(Self {
            tcp_client: TcpClient::new(config.to_tcp_config()),
            config,
            on_open: None,
            on_open_failed: None,
            on_close: None,
            connection: rpc_core::connection::DefaultConnection::new(),
            rpc: None,
            this: SharedPtr::new(),
        });
        r.this = SharedPtr::from_box(&r);
        let this_weak = r.this.downgrade();
        r.tcp_client.on_open(move || {
            let this = this_weak.unwrap();
            if let Some(rpc) = &this.config.rpc {
                this.rpc = Some(rpc.clone());
                this.connection = rpc.get_connection().unwrap();
            } else {
                this.rpc = Some(Rpc::new(Some(this.connection.clone())));
            }

            {
                let this_weak = this_weak.clone();
                this.connection.borrow_mut().set_send_package_impl(Box::new(move |package: Vec<u8>| {
                    let this = this_weak.unwrap();
                    this.tcp_client.send(package);
                }));
            }
            {
                let this_weak = this_weak.clone();
                this.tcp_client.on_data(move |package| {
                    let this = this_weak.unwrap();
                    this.connection.borrow_mut().on_recv_package(package);
                });
            }

            this.rpc.as_mut().unwrap().set_timer(|ms: u32, handle: Box<dyn Fn()>| {
                let handle_ptr = AtomicPtr::new(Box::into_raw(Box::new(handle)));
                tokio::task::spawn_local(async move {
                    tokio::time::sleep(tokio::time::Duration::from_millis(ms as u64)).await;
                    let handle = unsafe { Box::from_raw(handle_ptr.load(Ordering::SeqCst)) };
                    handle();
                });
            });
            {
                let this_weak = this_weak.clone();
                this.tcp_client.on_close(move || {
                    let this = this_weak.unwrap();
                    this.rpc.as_mut().unwrap().set_ready(false);
                });
            }
            this.rpc.as_mut().unwrap().set_ready(true);

            if let Some(on_open) = &this.on_open {
                on_open(this.rpc.clone().unwrap());
            }
        });
        r
    }

    pub fn downgrade(&self) -> WeakPtr<Self> {
        self.this.downgrade()
    }

    pub fn open(&mut self, host: impl ToString, port: u16) {
        self.tcp_client.open(host, port);
    }

    pub fn close(&mut self) {
        self.tcp_client.close();
    }

    pub fn set_reconnect(&mut self, ms: u32) {
        self.tcp_client.set_reconnect(ms);
    }

    pub fn cancel_reconnect(&mut self) {
        self.tcp_client.cancel_reconnect();
    }

    pub fn stop(&mut self) {
        self.close();
    }

    pub fn on_open<F>(&mut self, callback: F)
        where F: Fn(Rc<Rpc>) + 'static,
    {
        self.on_open = Some(Box::new(callback));
    }

    pub fn on_open_failed<F>(&mut self, callback: F)
        where F: Fn(&dyn Error) + 'static,
    {
        self.on_open_failed = Some(Box::new(callback));
    }

    pub fn on_close<F>(&mut self, callback: F)
        where F: Fn() + 'static,
    {
        self.on_close = Some(Box::new(callback));
    }
}

