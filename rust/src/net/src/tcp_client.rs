use std::cell::RefCell;
use std::collections::VecDeque;
use std::error::Error;
use std::net::ToSocketAddrs;
use std::rc::{Rc, Weak};

use log::debug;
use tokio::io::{AsyncReadExt, AsyncWriteExt, ReadHalf, WriteHalf};
use tokio::net::TcpStream;
use tokio::sync::Notify;

use crate::config::TcpConfig;

pub struct TcpClient {
    host: RefCell<String>,
    port: RefCell<u16>,
    stream_read: RefCell<Option<ReadHalf<TcpStream>>>,
    stream_write: RefCell<Option<WriteHalf<TcpStream>>>,
    config: RefCell<TcpConfig>,
    on_open: RefCell<Option<Box<dyn Fn()>>>,
    on_open_failed: RefCell<Option<Box<dyn Fn(&dyn Error)>>>,
    on_close: RefCell<Option<Box<dyn Fn()>>>,
    on_data: RefCell<Option<Box<dyn Fn(Vec<u8>)>>>,
    is_open: RefCell<bool>,
    reconnect_ms: RefCell<u32>,
    reconnect_timer_running: RefCell<bool>,
    send_queue: RefCell<VecDeque<Vec<u8>>>,
    send_queue_notify: RefCell<Notify>,
    this: RefCell<Weak<Self>>,
}

// public
impl TcpClient {
    pub fn new(config: TcpConfig) -> Rc<Self> {
        let r = Rc::new(Self {
            host: "".to_string().into(),
            port: 0.into(),
            stream_read: None.into(),
            stream_write: None.into(),
            config: config.into(),
            on_open: None.into(),
            on_open_failed: None.into(),
            on_close: None.into(),
            on_data: None.into(),
            is_open: false.into(),
            reconnect_ms: 0.into(),
            reconnect_timer_running: false.into(),
            send_queue: VecDeque::new().into(),
            send_queue_notify: Notify::new().into(),
            this: Weak::new().into(),
        });
        *r.this.borrow_mut() = Rc::downgrade(&r).into();
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
        *self.is_open.borrow_mut() = false;
        self.send_queue.borrow_mut().clear();
        self.send_queue_notify.borrow().notify_one();
        if let Some(on_close) = self.on_close.take() {
            on_close();
        }
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
        *self.on_data.borrow_mut() = Some(Box::new(callback));
    }

    pub fn on_close<F>(&self, callback: F)
        where F: Fn() + 'static,
    {
        *self.on_close.borrow_mut() = Some(Box::new(callback));
    }

    pub fn send(&self, data: Vec<u8>) {
        if !*self.is_open.borrow() {
            debug!("tcp not connected");
            return;
        }
        {
            let mut send_queue = self.send_queue.borrow_mut();
            if self.config.borrow().auto_pack {
                send_queue.push_back((data.len() as u32).to_le_bytes().to_vec());
            }
            send_queue.push_back(data);
        }
        self.send_queue_notify.borrow().notify_one();
    }

    pub fn send_str(&self, data: impl ToString) {
        self.send(data.to_string().as_bytes().to_vec());
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
            debug!("connect_tcp: {host} {port}");
            let result = TcpClient::connect_tcp(host, port).await;
            debug!("connect_tcp: {result:?}");
            let this = this_weak.upgrade().unwrap();

            match result {
                Ok(stream) => {
                    if let Some(on_open) = this.on_open.borrow_mut().as_ref() {
                        *this.is_open.borrow_mut() = true;
                        let (read_half, write_half) = tokio::io::split(stream);
                        *this.stream_read.borrow_mut() = Some(read_half);
                        *this.stream_write.borrow_mut() = Some(write_half);
                        on_open();
                        if this.config.borrow().auto_pack {
                            let this_weak = this_weak.clone();
                            tokio::task::spawn_local(async move {
                                loop {
                                    if !this_weak.upgrade().unwrap().do_read_header().await {
                                        break;
                                    }
                                }
                            });
                        } else {
                            let this_weak = this_weak.clone();
                            tokio::task::spawn_local(async move {
                                loop {
                                    if !this_weak.upgrade().unwrap().do_read_data().await {
                                        break;
                                    }
                                }
                            });
                        }

                        let this_weak = this_weak.clone();
                        tokio::task::spawn_local(async move {
                            let this = this_weak.upgrade().unwrap();
                            loop {
                                if !*this.is_open.borrow() {
                                    break;
                                }
                                let send_data = this.send_queue.borrow_mut().pop_front();
                                if let Some(data) = send_data {
                                    let mut result = None;
                                    if let Some(stream) = this.stream_write.borrow_mut().as_mut() {
                                        result = stream.write_all(&data).await.ok();
                                    }

                                    if result.is_none() {
                                        this.do_close();
                                        this.check_reconnect().await;
                                        break;
                                    }
                                } else {
                                    this.send_queue_notify.borrow().notified().await;
                                }
                            }
                        });
                    }
                }
                Err(err) => {
                    if let Some(on_open_failed) = this.on_open_failed.borrow().as_ref() {
                        on_open_failed(&*err);
                    }
                    *this.is_open.borrow_mut() = false;
                    this.check_reconnect().await;
                }
            };
        });
    }

    fn do_close(&self) {
        if !*self.is_open.borrow() {
            return;
        }
        *self.is_open.borrow_mut() = false;
        *self.stream_read.borrow_mut() = None;
        *self.stream_write.borrow_mut() = None;
        if let Some(on_close) = self.on_close.borrow().as_ref() {
            on_close();
        }
    }

    async fn do_read_data(&self) -> bool {
        let mut buffer = vec![];
        let mut read_result = None;
        if let Some(stream) = self.stream_read.borrow_mut().as_mut() {
            read_result = stream.read_buf(&mut buffer).await.ok();
        }

        return if read_result.is_some() {
            if let Some(on_data) = self.on_data.borrow().as_ref() {
                on_data(buffer);
            }
            true
        } else {
            self.do_close();
            self.check_reconnect().await;
            false
        };
    }

    async fn do_read_header(&self) -> bool {
        let mut buffer = [0u8; 4];
        let mut read_result = None;
        if let Some(stream) = self.stream_read.borrow_mut().as_mut() {
            read_result = stream.read_exact(&mut buffer).await.ok();
        }

        return if read_result.is_some() {
            let body_size = u32::from_le_bytes(buffer);
            self.do_read_body(body_size).await
        } else {
            self.do_close();
            self.check_reconnect().await;
            false
        };
    }

    async fn do_read_body(&self, body_size: u32) -> bool {
        let mut buffer: Vec<u8> = vec![0; body_size as usize];
        let mut read_result = None;
        if let Some(stream) = self.stream_read.borrow_mut().as_mut() {
            read_result = stream.read_exact(&mut buffer).await.ok();
        }

        return if read_result.is_some() {
            if let Some(on_data) = self.on_data.borrow().as_ref() {
                on_data(buffer);
            }
            true
        } else {
            self.do_close();
            self.check_reconnect().await;
            false
        };
    }

    async fn connect_tcp(
        host: String,
        port: u16,
    ) -> Result<TcpStream, Box<dyn Error + Send + Sync>> {
        let addr = (host, port).to_socket_addrs()?.next().unwrap();
        let stream = TcpStream::connect(addr).await?;
        Ok(stream)
    }

    async fn check_reconnect(&self) {
        if !*self.is_open.borrow() && !*self.reconnect_timer_running.borrow() && *self.reconnect_ms.borrow() > 0 {
            *self.reconnect_timer_running.borrow_mut() = true;
            tokio::time::sleep(tokio::time::Duration::from_millis((*self.reconnect_ms.borrow()).into())).await;
            if *self.reconnect_timer_running.borrow() {
                *self.reconnect_timer_running.borrow_mut() = false;
            } else {
                return;
            }
            if !*self.is_open.borrow() {
                self.do_open();
            }
        }
    }
}

