use std::cell::RefCell;
use std::collections::VecDeque;
use std::rc::Rc;

use log::{debug, trace};
use tokio::io::{AsyncReadExt, AsyncWriteExt, ReadHalf, WriteHalf};
use tokio::net::TcpStream;
use tokio::select;
use tokio::sync::Notify;

use crate::config::TcpConfig;

pub struct TcpChannel {
    stream_read: RefCell<Option<ReadHalf<TcpStream>>>,
    stream_write: RefCell<Option<WriteHalf<TcpStream>>>,
    config: Rc<RefCell<TcpConfig>>,
    on_close: RefCell<Option<Box<dyn Fn()>>>,
    on_data: RefCell<Option<Box<dyn Fn(Vec<u8>)>>>,
    send_queue: RefCell<VecDeque<Vec<u8>>>,
    send_queue_notify: Notify,
    quit_notify: Notify,
    pub is_open: RefCell<bool>,
}

impl TcpChannel {
    pub fn new(config: Rc<RefCell<TcpConfig>>) -> Rc<Self> {
        Rc::new(Self {
            stream_read: None.into(),
            stream_write: None.into(),
            config,
            on_close: None.into(),
            on_data: None.into(),
            send_queue: VecDeque::new().into(),
            send_queue_notify: Notify::new(),
            quit_notify: Notify::new(),
            is_open: false.into(),
        })
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
        self.send_queue_notify.notify_one();
    }

    pub fn send_str(&self, data: impl ToString) {
        self.send(data.to_string().as_bytes().to_vec());
    }

    pub fn close(&self) {
        *self.is_open.borrow_mut() = false;
        self.send_queue.borrow_mut().clear();
        self.send_queue_notify.notify_one();
        if let Some(on_close) = self.on_close.take() {
            on_close();
        }
    }

    pub fn do_open(self: &Rc<Self>, stream: TcpStream) {
        if *self.is_open.borrow() {
            self.do_close();
        }
        *self.is_open.borrow_mut() = true;
        let this = self.clone();
        tokio::task::spawn_local(async move {
            let (read_half, write_half) = tokio::io::split(stream);
            *this.stream_read.borrow_mut() = Some(read_half);
            *this.stream_write.borrow_mut() = Some(write_half);

            // read loop task
            if this.config.borrow().auto_pack {
                let this = this.clone();
                tokio::task::spawn_local(async move {
                    loop {
                        if !*this.is_open.borrow() {
                            break;
                        }
                        select! {
                            goon = this.do_read_header() => {
                                if !goon {
                                    break;
                                }
                            },
                            _ = this.quit_notify.notified() => {
                                break;
                            },
                        }
                    }
                    *this.stream_read.borrow_mut() = None;
                    trace!("loop exit: read");
                });
            } else {
                let this = this.clone();
                tokio::task::spawn_local(async move {
                    loop {
                        select! {
                            goon = this.do_read_data() => {
                                if !goon {
                                    break;
                                }
                            },
                            _ = this.quit_notify.notified() => {
                                break;
                            },
                        }
                        *this.stream_read.borrow_mut() = None;
                    }
                    trace!("loop exit: read");
                });
            }

            // write loop task
            tokio::task::spawn_local(async move {
                loop {
                    if !*this.is_open.borrow() {
                        break;
                    }

                    let send_data = this.send_queue.borrow_mut().pop_front();
                    if let Some(data) = send_data {
                        let mut result = None;
                        if let Some(stream) = this.stream_write.borrow_mut().as_mut() {
                            select! {
                                write_ret = stream.write_all(&data) => {
                                    result = write_ret.ok();
                                },
                                _ = this.quit_notify.notified() => {},
                            }
                        }

                        if result.is_none() {
                            this.do_close();
                            break;
                        }
                    } else {
                        this.send_queue_notify.notified().await;
                    }
                }
                *this.stream_write.borrow_mut() = None;
                trace!("loop exit: write");
            });
        });
    }

    pub fn do_close(&self) {
        if !*self.is_open.borrow() {
            return;
        }
        *self.is_open.borrow_mut() = false;
        self.quit_notify.notify_waiters();
        self.send_queue_notify.notify_one();
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

impl Drop for TcpChannel {
    fn drop(&mut self) {
        trace!("~TcpChannel");
    }
}
