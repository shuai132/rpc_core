use std::cell::RefCell;
use std::collections::VecDeque;
use std::rc::Rc;

use log::{debug, trace};
use tokio::io::{AsyncReadExt, AsyncWriteExt, ReadHalf};
use tokio::net::TcpStream;
use tokio::select;
use tokio::sync::Notify;

use crate::net::config::TcpConfig;

pub struct TcpChannel {
    config: Rc<RefCell<TcpConfig>>,
    on_close: RefCell<Option<Box<dyn Fn()>>>,
    on_data: RefCell<Option<Box<dyn Fn(Vec<u8>)>>>,
    send_queue: RefCell<VecDeque<Vec<u8>>>,
    send_queue_notify: Notify,
    quit_notify: Notify,
    read_quit_finish_notify: Notify,
    write_quit_finish_notify: Notify,
    is_open: RefCell<bool>,
}

impl TcpChannel {
    pub fn new(config: Rc<RefCell<TcpConfig>>) -> Rc<Self> {
        Rc::new(Self {
            config,
            on_close: None.into(),
            on_data: None.into(),
            send_queue: VecDeque::new().into(),
            send_queue_notify: Notify::new(),
            quit_notify: Notify::new(),
            read_quit_finish_notify: Notify::new(),
            write_quit_finish_notify: Notify::new(),
            is_open: false.into(),
        })
    }

    pub fn is_open(&self) -> bool {
        *self.is_open.borrow()
    }

    pub fn on_data<F>(&self, callback: F)
    where
        F: Fn(Vec<u8>) + 'static,
    {
        *self.on_data.borrow_mut() = Some(Box::new(callback));
    }

    pub fn on_close<F>(&self, callback: F)
    where
        F: Fn() + 'static,
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
        self.do_close();
    }

    pub async fn wait_close_finish(&self) {
        tokio::join!(
            self.read_quit_finish_notify.notified(),
            self.write_quit_finish_notify.notified(),
        );
    }

    pub fn do_open(self: &Rc<Self>, stream: TcpStream) {
        if *self.is_open.borrow() {
            self.do_close();
        }
        *self.is_open.borrow_mut() = true;
        let this = self.clone();
        tokio::task::spawn_local(async move {
            let (read_half, write_half) = tokio::io::split(stream);

            // read loop task
            if this.config.borrow().auto_pack {
                let this = this.clone();
                tokio::task::spawn_local(async move {
                    let mut read_half = read_half;
                    loop {
                        if !*this.is_open.borrow() {
                            break;
                        }
                        select! {
                            goon = this.do_read_header(&mut read_half) => {
                                if !goon {
                                    break;
                                }
                            },
                            _ = this.quit_notify.notified() => {
                                break;
                            },
                        }
                    }
                    trace!("loop exit: read");
                    this.read_quit_finish_notify.notify_one();
                });
            } else {
                let this = this.clone();
                tokio::task::spawn_local(async move {
                    let mut read_half = read_half;
                    loop {
                        select! {
                            goon = this.do_read_data(&mut read_half) => {
                                if !goon {
                                    break;
                                }
                            },
                            _ = this.quit_notify.notified() => {
                                break;
                            },
                        }
                    }
                    trace!("loop exit: read");
                    this.read_quit_finish_notify.notify_one();
                });
            }

            // write loop task
            tokio::task::spawn_local(async move {
                let mut write_half = write_half;
                loop {
                    if !*this.is_open.borrow() {
                        break;
                    }

                    let send_data = this.send_queue.borrow_mut().pop_front();
                    if let Some(data) = send_data {
                        let mut result = None;
                        select! {
                            write_ret = write_half.write_all(&data) => {
                                result = write_ret.ok();
                            },
                            _ = this.quit_notify.notified() => {},
                        }
                        if result.is_none() {
                            this.do_close();
                            break;
                        }
                    } else {
                        this.send_queue_notify.notified().await;
                    }
                }
                trace!("loop exit: write");

                if let Some(on_close) = this.on_close.borrow().as_ref() {
                    on_close();
                }
                this.write_quit_finish_notify.notify_one();
            });
        });
    }

    fn do_close(&self) {
        if !*self.is_open.borrow() {
            return;
        }
        *self.is_open.borrow_mut() = false;
        self.quit_notify.notify_waiters();
        self.send_queue_notify.notify_one();
    }

    async fn do_read_data(&self, read_half: &mut ReadHalf<TcpStream>) -> bool {
        let mut buffer = vec![];
        let read_result = read_half.read_buf(&mut buffer).await.ok();

        if read_result.is_some() && !buffer.is_empty() {
            if let Some(on_data) = self.on_data.borrow().as_ref() {
                on_data(buffer);
            }
            true
        } else {
            self.do_close();
            false
        }
    }

    async fn do_read_header(&self, read_half: &mut ReadHalf<TcpStream>) -> bool {
        let mut buffer = [0u8; 4];
        let read_result = read_half.read_exact(&mut buffer).await.ok();

        if read_result.is_some() {
            let body_size = u32::from_le_bytes(buffer);
            self.do_read_body(read_half, body_size).await
        } else {
            self.do_close();
            false
        }
    }

    async fn do_read_body(&self, read_half: &mut ReadHalf<TcpStream>, body_size: u32) -> bool {
        let mut buffer: Vec<u8> = vec![0; body_size as usize];
        let read_result = read_half.read_exact(&mut buffer).await.ok();

        if read_result.is_some() {
            if let Some(on_data) = self.on_data.borrow().as_ref() {
                on_data(buffer);
            }
            true
        } else {
            self.do_close();
            false
        }
    }
}

impl Drop for TcpChannel {
    fn drop(&mut self) {
        trace!("~TcpChannel");
    }
}
