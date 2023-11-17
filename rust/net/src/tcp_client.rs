use std::error::Error;
use std::net::ToSocketAddrs;

use log::debug;
use tokio::io::AsyncReadExt;
use tokio::net::TcpStream;

use crate::config::TcpConfig;

pub struct TcpClient {
    host: String,
    port: u16,
    socket: Option<TcpStream>,
    config: TcpConfig,
    on_open: Option<Box<dyn Fn()>>,
    on_open_failed: Option<Box<dyn Fn(&dyn Error)>>,
    on_close: Option<Box<dyn Fn() + Send>>,
    on_data: Option<Box<dyn Fn(Vec<u8>) + Send>>,
    is_open: bool,
    reconnect_ms: u32,
    reconnect_timer_running: bool,
}

// Be sure used on single thread
unsafe impl Send for TcpClient {}

unsafe impl Sync for TcpClient {}

impl TcpClient {
    pub fn new(config: TcpConfig) -> Self {
        TcpClient {
            host: "".to_string(),
            port: 0,
            socket: None,
            config,
            on_open: None,
            on_open_failed: None,
            on_close: None,
            on_data: None,
            is_open: false,
            reconnect_ms: 0,
            reconnect_timer_running: false,
        }
    }

    pub fn open(&mut self, host: impl ToString, port: u16) {
        self.host = host.to_string();
        self.port = port;
        self.do_open();
    }

    pub fn close(&mut self) {
        self.cancel_reconnect();
        self.is_open = false;
        if let Some(on_close) = self.on_close.take() {
            on_close();
        }
    }

    pub fn set_reconnect(&mut self, ms: u32) {
        self.reconnect_ms = ms;
    }

    pub fn cancel_reconnect(&mut self) {
        self.reconnect_timer_running = false;
    }

    fn check_reconnect(&mut self) {
        if !self.is_open && !self.reconnect_timer_running && self.reconnect_ms > 0 {
            self.reconnect_timer_running = true;
            let this_ptr = self as *const _ as *mut Self;
            let this = unsafe { &mut *this_ptr };
            tokio::spawn(async move {
                tokio::time::sleep(tokio::time::Duration::from_millis(this.reconnect_ms.into())).await;
                if this.reconnect_timer_running {
                    this.reconnect_timer_running = false;
                    this.do_open();
                }
            });
        }
    }

    pub fn stop(&mut self) {
        self.close();
    }

    fn do_open(&mut self) {
        self.config.init();
        let host = self.host.clone();
        let port = self.port;

        let this_ptr = self as *const _ as *mut Self;
        let this = unsafe { &mut *this_ptr };
        tokio::spawn(async move {
            debug!("connect_tcp: {host} {port}");
            let result = TcpClient::connect_tcp(host, port).await;
            debug!("connect_tcp: {result:?}");
            match result {
                Ok(stream) => {
                    if let Some(on_open) = &this.on_open {
                        this.is_open = true;
                        this.socket = Some(stream);
                        this.do_read_header();
                        on_open();
                    }
                }
                Err(err) => {
                    if let Some(on_open_failed) = &this.on_open_failed {
                        on_open_failed(&*err);
                        this.check_reconnect();
                    }
                }
            };
        });
    }

    fn do_read_header(&self) {
        let this_ptr = self as *const _ as *mut Self;
        let this = unsafe { &mut *this_ptr };
        tokio::spawn(async move {
            let stream = this.socket.as_mut().unwrap();
            let mut buffer = [0u8; 4];
            stream.read_exact(&mut buffer).await.unwrap();
            let body_size = u32::from_le_bytes(buffer);
            this.do_read_body(body_size);
        });
    }

    fn do_read_body(&mut self, body_size: u32) {
        let this_ptr = self as *const _ as *mut Self;
        let this = unsafe { &mut *this_ptr };
        tokio::spawn(async move {
            let stream = this.socket.as_mut().unwrap();
            let mut buffer: Vec<u8> = vec![0; body_size as usize];
            stream.read_exact(&mut buffer).await.unwrap();
            if let Some(on_data) = &this.on_data {
                on_data(buffer);
            }
            this.do_read_header();
        });
    }

    async fn connect_tcp(
        host: String,
        port: u16,
    ) -> Result<TcpStream, Box<dyn Error>> {
        let addr = (host, port).to_socket_addrs()?.next().unwrap();
        let stream = TcpStream::connect(addr).await?;
        Ok(stream)
    }

    pub fn on_open<F>(&mut self, callback: F)
        where F: Fn() + 'static,
    {
        self.on_open = Some(Box::new(callback));
    }

    pub fn on_open_failed<F>(&mut self, callback: F)
        where F: Fn(&dyn Error) + 'static,
    {
        self.on_open_failed = Some(Box::new(callback));
    }

    pub fn on_data<F>(&mut self, callback: F)
        where F: Fn(Vec<u8>) + Send + 'static,
    {
        self.on_data = Some(Box::new(callback));
    }

    pub fn on_close<F>(&mut self, callback: F)
        where F: Fn() + Send + 'static,
    {
        self.on_close = Some(Box::new(callback));
    }
}

