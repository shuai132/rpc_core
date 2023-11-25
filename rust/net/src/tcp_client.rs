use std::collections::VecDeque;
use std::error::Error;
use std::net::ToSocketAddrs;

use log::debug;
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio::net::TcpStream;
use tokio::sync::Notify;

use crate::config::TcpConfig;

pub struct TcpClient {
    host: String,
    port: u16,
    socket: Option<TcpStream>,
    config: TcpConfig,
    on_open: Option<Box<dyn Fn()>>,
    on_open_failed: Option<Box<dyn Fn(&dyn Error)>>,
    on_close: Option<Box<dyn Fn()>>,
    on_data: Option<Box<dyn Fn(Vec<u8>)>>,
    is_open: bool,
    reconnect_ms: u32,
    reconnect_timer_running: bool,
    send_queue: VecDeque<Vec<u8>>,
    send_queue_notify: Notify,
}

// Be sure used on single thread
unsafe impl Send for TcpClient {}

unsafe impl Sync for TcpClient {}

// public
impl TcpClient {
    pub fn new(config: TcpConfig) -> Box<Self> {
        Box::new(Self {
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
            send_queue: VecDeque::new(),
            send_queue_notify: Notify::new(),
        })
    }

    pub fn open(&mut self, host: impl ToString, port: u16) {
        self.host = host.to_string();
        self.port = port;
        self.do_open();
    }

    pub fn close(&mut self) {
        self.cancel_reconnect();
        self.is_open = false;
        self.send_queue.clear();
        self.send_queue_notify.notify_one();
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

    pub fn stop(&mut self) {
        self.close();
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
        where F: Fn(Vec<u8>) + 'static,
    {
        self.on_data = Some(Box::new(callback));
    }

    pub fn on_close<F>(&mut self, callback: F)
        where F: Fn() + 'static,
    {
        self.on_close = Some(Box::new(callback));
    }

    pub fn send(&mut self, data: Vec<u8>) {
        if !self.is_open {
            debug!("tcp not connected");
            return;
        }
        if self.config.auto_pack {
            self.send_queue.push_back((data.len() as u32).to_le_bytes().to_vec());
        }
        self.send_queue.push_back(data);
        self.send_queue_notify.notify_one();
    }

    pub fn send_str(&mut self, data: impl ToString) {
        self.send(data.to_string().as_bytes().to_vec());
    }
}

// private
impl TcpClient {
    fn do_open(&mut self) {
        self.config.init();
        let host = self.host.clone();
        let port = self.port;

        let this_ptr = self as *const _ as *mut Self;
        let this = unsafe { &mut *this_ptr };
        let this2 = unsafe { &mut *this_ptr };
        tokio::spawn(async move {
            debug!("connect_tcp: {host} {port}");
            let result = TcpClient::connect_tcp(host, port).await;
            debug!("connect_tcp: {result:?}");
            match result {
                Ok(stream) => {
                    if let Some(on_open) = &this.on_open {
                        this.is_open = true;
                        this.socket = Some(stream);
                        on_open();
                        if this.config.auto_pack {
                            tokio::spawn(async move {
                                loop {
                                    this.do_read_header().await;
                                }
                            });
                        } else {
                            tokio::spawn(async move {
                                loop {
                                    this.do_read_data().await;
                                }
                            });
                        }

                        tokio::spawn(async {
                            let this = this2;
                            loop {
                                if !this.is_open {
                                    break;
                                }
                                if let Some(data) = this.send_queue.pop_front() {
                                    if let Some(stream) = this.socket.as_mut() {
                                        if stream.write_all(&data).await.is_err() {
                                            this.do_close();
                                            this.check_reconnect().await;
                                            break;
                                        }
                                    } else {
                                        this.do_close();
                                        this.check_reconnect().await;
                                        break;
                                    }
                                } else {
                                    this.send_queue_notify.notified().await;
                                }
                            }
                        });
                    }
                }
                Err(err) => {
                    if let Some(on_open_failed) = &this.on_open_failed {
                        on_open_failed(&*err);
                    }
                    this.is_open = false;
                    this.check_reconnect().await;
                }
            };
        });
    }

    fn do_close(&mut self) {
        if !self.is_open {
            return;
        }
        self.is_open = false;
        self.socket = None;
        if let Some(on_close) = &self.on_close {
            on_close();
        }
    }

    async fn do_read_data(&mut self) {
        let stream = self.socket.as_mut().unwrap();
        let mut buffer = vec![];
        if let Ok(_) = stream.read_buf(&mut buffer).await {
            if let Some(on_data) = &self.on_data {
                on_data(buffer);
            }
        } else {
            self.do_close();
            self.check_reconnect().await;
        }
    }

    async fn do_read_header(&mut self) {
        if let Some(stream) = self.socket.as_mut() {
            let mut buffer = [0u8; 4];
            if let Ok(_) = stream.read_exact(&mut buffer).await {
                let body_size = u32::from_le_bytes(buffer);
                self.do_read_body(body_size).await;
            } else {
                self.do_close();
                self.check_reconnect().await;
            }
        } else {
            self.do_close();
            self.check_reconnect().await;
        }
    }

    async fn do_read_body(&mut self, body_size: u32) {
        if let Some(stream) = self.socket.as_mut() {
            let mut buffer: Vec<u8> = vec![0; body_size as usize];
            if let Ok(_) = stream.read_exact(&mut buffer).await {
                if let Some(on_data) = &self.on_data {
                    on_data(buffer);
                }
            } else {
                self.do_close();
                self.check_reconnect().await;
            }
        } else {
            self.do_close();
            self.check_reconnect().await;
        }
    }

    async fn connect_tcp(
        host: String,
        port: u16,
    ) -> Result<TcpStream, Box<dyn Error + Send + Sync>> {
        let addr = (host, port).to_socket_addrs()?.next().unwrap();
        let stream = TcpStream::connect(addr).await?;
        Ok(stream)
    }

    async fn check_reconnect(&mut self) {
        if !self.is_open && !self.reconnect_timer_running && self.reconnect_ms > 0 {
            self.reconnect_timer_running = true;
            tokio::time::sleep(tokio::time::Duration::from_millis(self.reconnect_ms.into())).await;
            if self.reconnect_timer_running {
                self.reconnect_timer_running = false;
            } else {
                return;
            }
            if !self.is_open {
                self.do_open();
            }
        }
    }
}

