use std::cell::RefCell;
use std::collections::VecDeque;
use std::rc::{Rc, Weak};

use log::{debug, trace};
use tokio::io::{AsyncReadExt, AsyncWriteExt, ReadHalf, WriteHalf};
use tokio::net::TcpStream;
use tokio::sync::Notify;

use crate::config::TcpConfig;

pub struct TcpChannel {
    pub stream_read: RefCell<Option<ReadHalf<TcpStream>>>,
    pub stream_write: RefCell<Option<WriteHalf<TcpStream>>>,
    pub config: Rc<RefCell<TcpConfig>>,
    pub on_close: RefCell<Option<Box<dyn Fn()>>>,
    pub on_data: RefCell<Option<Box<dyn Fn(Vec<u8>)>>>,
    pub send_queue: RefCell<VecDeque<Vec<u8>>>,
    pub send_queue_notify: RefCell<Notify>,
    pub is_open: RefCell<bool>,
    this: RefCell<Weak<Self>>,
}

// public
impl TcpChannel {
    pub fn new(config: Rc<RefCell<TcpConfig>>) -> Rc<Self> {
        let r = Rc::new(Self {
            stream_read: None.into(),
            stream_write: None.into(),
            config,
            on_close: None.into(),
            on_data: None.into(),
            send_queue: VecDeque::new().into(),
            send_queue_notify: Notify::new().into(),
            is_open: false.into(),
            this: Weak::new().into(),
        });
        *r.this.borrow_mut() = Rc::downgrade(&r).into();
        r
    }

    pub fn on_close<F>(&self, callback: F)
        where F: Fn() + 'static,
    {
        *self.on_close.borrow_mut() = Some(Box::new(callback));
    }

    pub fn send(&self, data: Vec<u8>) {
        if !*self.is_open.borrow() {
            debug!("channel not connected");
            return;
        }
        if data.is_empty() {
            debug!("send empty ignore");
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

    pub fn close(&self) {
        *self.is_open.borrow_mut() = false;
        self.send_queue.borrow_mut().clear();
        self.send_queue_notify.borrow().notify_one();
        if let Some(on_close) = self.on_close.take() {
            on_close();
        }
    }
}

// private
impl TcpChannel {
    pub fn do_open(&self, stream: TcpStream) {
        *self.is_open.borrow_mut() = true;
        let this_weak = self.this.borrow().clone();
        tokio::task::spawn_local(async move {
            let this = this_weak.upgrade().unwrap();
            let (read_half, write_half) = tokio::io::split(stream);
            *this.stream_read.borrow_mut() = Some(read_half);
            *this.stream_write.borrow_mut() = Some(write_half);
            if this.config.borrow().auto_pack {
                let this_weak = this_weak.clone();
                tokio::task::spawn_local(async move {
                    loop {
                        if !this_weak.upgrade().unwrap().do_read_header().await {
                            break;
                        }
                    }
                    trace!("loop exit: read");
                });
            } else {
                let this_weak = this_weak.clone();
                tokio::task::spawn_local(async move {
                    loop {
                        if !this_weak.upgrade().unwrap().do_read_data().await {
                            break;
                        }
                    }
                    trace!("loop exit: read");
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
                            break;
                        }
                    } else {
                        this.send_queue_notify.borrow().notified().await;
                    }
                }
                trace!("loop exit: send");
            });
        });
    }

    pub fn do_close(&self) {
        if !*self.is_open.borrow() {
            return;
        }
        *self.is_open.borrow_mut() = false;
        self.send_queue_notify.borrow().notify_one();
        *self.stream_read.borrow_mut() = None;
        *self.stream_write.borrow_mut() = None;
        if let Some(on_close) = self.on_close.borrow().as_ref() {
            on_close();
        }
    }

    pub async fn do_read_data(&self) -> bool {
        let mut buffer = vec![];
        let mut read_result = None;
        if let Some(stream) = self.stream_read.borrow_mut().as_mut() {
            read_result = stream.read_buf(&mut buffer).await.ok();
        }

        return if read_result.is_some() && !buffer.is_empty() {
            if let Some(on_data) = self.on_data.borrow().as_ref() {
                on_data(buffer);
            }
            true
        } else {
            self.do_close();
            false
        };
    }

    pub async fn do_read_header(&self) -> bool {
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
            false
        };
    }
}

