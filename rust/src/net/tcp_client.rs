use std::cell::RefCell;
use std::error::Error;
use std::net::ToSocketAddrs;
use std::rc::{Rc, Weak};

use log::debug;
use tokio::net::TcpStream;

use crate::net::config::TcpConfig;
use crate::net::detail::tcp_channel::TcpChannel;

pub struct TcpClient {
    host: RefCell<String>,
    port: RefCell<u16>,
    config: Rc<RefCell<TcpConfig>>,
    on_open: RefCell<Option<Box<dyn Fn()>>>,
    on_open_failed: RefCell<Option<Box<dyn Fn(&dyn Error)>>>,
    on_close: RefCell<Option<Box<dyn Fn()>>>,
    reconnect_ms: RefCell<u32>,
    reconnect_timer_running: RefCell<bool>,
    channel: Rc<TcpChannel>,
    this: RefCell<Weak<Self>>,
}

// public
impl TcpClient {
    pub fn new(config: TcpConfig) -> Rc<Self> {
        let config = Rc::new(RefCell::new(config));
        let r = Rc::new(Self {
            host: "".to_string().into(),
            port: 0.into(),
            config: config.clone(),
            on_open: None.into(),
            on_open_failed: None.into(),
            on_close: None.into(),
            reconnect_ms: 0.into(),
            reconnect_timer_running: false.into(),
            channel: TcpChannel::new(config),
            this: Weak::new().into(),
        });
        let this_weak = Rc::downgrade(&r);
        *r.this.borrow_mut() = this_weak.clone().into();

        r.channel.on_close(move || {
            if let Some(this) = this_weak.upgrade() {
                if let Some(on_close) = this.on_close.borrow().as_ref() {
                    on_close();
                }
                tokio::task::spawn_local(async move {
                    this.check_reconnect().await;
                });
            }
        });
        r
    }

    pub fn downgrade(&self) -> Weak<Self> {
        self.this.borrow().clone()
    }

    pub fn open(&self, host: impl ToString, port: u16) {
        *self.host.borrow_mut() = host.to_string();
        *self.port.borrow_mut() = port;
        self.do_open();
    }

    pub fn close(&self) {
        self.cancel_reconnect();
        self.channel.close();
    }

    pub fn set_reconnect(&self, ms: u32) {
        *self.reconnect_ms.borrow_mut() = ms;
    }

    pub fn cancel_reconnect(&self) {
        *self.reconnect_timer_running.borrow_mut() = false;
    }

    pub fn stop(&self) {
        self.close();
    }

    pub fn on_open<F>(&self, callback: F)
        where F: Fn() + 'static,
    {
        *self.on_open.borrow_mut() = Some(Box::new(callback));
    }

    pub fn on_open_failed<F>(&self, callback: F)
        where F: Fn(&dyn Error) + 'static,
    {
        *self.on_open_failed.borrow_mut() = Some(Box::new(callback));
    }

    pub fn on_data<F>(&self, callback: F)
        where F: Fn(Vec<u8>) + 'static,
    {
        self.channel.on_data(callback);
    }

    pub fn on_close<F>(&self, callback: F)
        where F: Fn() + 'static,
    {
        *self.on_close.borrow_mut() = Some(Box::new(callback));
    }

    pub fn send(&self, data: Vec<u8>) {
        self.channel.send(data);
    }

    pub fn send_str(&self, data: impl ToString) {
        self.channel.send_str(data);
    }
}

// private
impl TcpClient {
    fn do_open(&self) {
        self.config.borrow_mut().init();
        let host = self.host.borrow().clone();
        let port = *self.port.borrow();

        let this_weak = self.this.borrow().clone();
        tokio::task::spawn_local(async move {
            let this = this_weak.upgrade().unwrap();
            if this.channel.is_open() {
                this.channel.close();
                this.channel.wait_close_finish().await;
            }
            debug!("connect_tcp: {host} {port}");
            let result = TcpClient::connect_tcp(host, port).await;
            debug!("connect_tcp: {result:?}");

            match result {
                Ok(stream) => {
                    this.channel.do_open(stream);
                    if let Some(on_open) = this.on_open.borrow_mut().as_ref() {
                        on_open();
                    }
                }
                Err(err) => {
                    if let Some(on_open_failed) = this.on_open_failed.borrow().as_ref() {
                        on_open_failed(&*err);
                    }
                    this.check_reconnect().await;
                }
            };
        });
    }

    async fn connect_tcp(
        host: String,
        port: u16,
    ) -> Result<TcpStream, Box<dyn Error + Send + Sync>> {
        let mut host = host;
        if host == "localhost" {
            host = "127.0.0.1".parse().unwrap();
        }
        let addr = (host, port).to_socket_addrs()?.next().unwrap();
        let stream = TcpStream::connect(addr).await?;
        Ok(stream)
    }

    async fn check_reconnect(&self) {
        if !self.channel.is_open() && !*self.reconnect_timer_running.borrow() && *self.reconnect_ms.borrow() > 0 {
            *self.reconnect_timer_running.borrow_mut() = true;
            tokio::time::sleep(tokio::time::Duration::from_millis((*self.reconnect_ms.borrow()).into())).await;
            if *self.reconnect_timer_running.borrow() {
                *self.reconnect_timer_running.borrow_mut() = false;
            } else {
                return;
            }
            if !self.channel.is_open() {
                self.do_open();
            }
        }
    }
}

