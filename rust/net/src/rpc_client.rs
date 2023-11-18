use std::cell::RefCell;
use std::error::Error;
use std::rc::Rc;
use rpc_core::connection::Connection;
use rpc_core::rpc::Rpc;
use crate::config::RpcConfig;
use crate::tcp_client::TcpClient;

pub struct RpcClient {
    tcp_client: TcpClient,
    config: RpcConfig,
    on_open: Option<Box<dyn Fn(Rc<Rpc>)>>,
    on_open_failed: Option<Box<dyn Fn(&dyn Error)>>,
    on_close: Option<Box<dyn Fn()>>,
    connection: Rc<RefCell<dyn Connection>>,
    rpc: Option<Rc<Rpc>>,
}

impl RpcClient {
    pub fn new(config: RpcConfig) -> Self {
        let mut r = Self {
            tcp_client: TcpClient::new(config.to_tcp_config()),
            config,
            on_open: None,
            on_open_failed: None,
            on_close: None,
            rpc: None,
            connection: rpc_core::connection::DefaultConnection::create(),
        };

        let this_ptr = &r as *const _ as *mut Self;
        let this_ptr_box = Box::new(this_ptr);
        r.tcp_client.on_open(move || {
            let this = unsafe { &mut **this_ptr_box };
            if let Some(rpc) = &this.config.rpc {
                this.rpc = Some(rpc.clone());
                this.connection = rpc.get_connection().unwrap();
            } else {
                this.rpc = Some(rpc_core::rpc::create(Some(this.connection.clone())));
            }

            let this_ptr = &this as *const _ as *mut Self;
            this.connection.borrow_mut().set_send_package_impl(Box::new(move |package: Vec<u8>| {
                let this = unsafe { &mut *this_ptr };
                this.tcp_client.send(package);
            }));

            this.tcp_client.on_data(|package| {
                this.connection.borrow_mut().on_recv_package(package);
            });

            this.rpc.as_mut().unwrap().set_timer(|ms: u32, _handle: Box<dyn Fn()>| {
                tokio::spawn(async move {
                    tokio::time::sleep(tokio::time::Duration::from_millis(ms as u64)).await;
                    // handle();
                });
            });
            this.rpc.as_mut().unwrap().set_ready(true);

            if let Some(on_open) = &this.on_open {
                on_open(this.rpc.clone().unwrap());
            }
        });
        r
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
        where F: Fn(Rc<rpc_core::rpc::Rpc>) + 'static,
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

