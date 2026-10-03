use std::cell::{Cell, RefCell};
use std::error::Error;
use std::rc::{Rc, Weak};

use log::debug;
use tokio::net::TcpStream;

use crate::net::config::TcpConfig;
use crate::net::detail::tcp_channel::TcpChannel;

pub struct TcpClient {
    host: RefCell<String>,
    port: RefCell<u16>,
    config: Rc<RefCell<TcpConfig>>,
    on_open: RefCell<Option<Rc<dyn Fn()>>>,
    on_open_failed: RefCell<Option<Rc<dyn Fn(&dyn Error)>>>,
    on_close: RefCell<Option<Rc<dyn Fn()>>>,
    reconnect_ms: RefCell<u32>,
    reconnect_timer_running: Cell<bool>,
    reconnect_generation: Cell<u64>,
    connect_generation: Cell<u64>,
    stopped: Cell<bool>,
    connecting: Cell<bool>,
    channel: Rc<TcpChannel>,
    connect_task: RefCell<Option<tokio::task::JoinHandle<()>>>,
    reconnect_task: RefCell<Option<tokio::task::JoinHandle<()>>>,
    this: RefCell<Weak<Self>>,
}

// public
impl TcpClient {
    pub fn new(config: TcpConfig) -> Rc<Self> {
        Rc::<Self>::new_cyclic(|this_weak| {
            let config = Rc::new(RefCell::new(config));
            let r = Self {
                host: "".to_string().into(),
                port: 0.into(),
                config: config.clone(),
                on_open: None.into(),
                on_open_failed: None.into(),
                on_close: None.into(),
                reconnect_ms: 0.into(),
                reconnect_timer_running: Cell::new(false),
                reconnect_generation: Cell::new(0),
                connect_generation: Cell::new(0),
                stopped: Cell::new(true),
                connecting: Cell::new(false),
                channel: TcpChannel::new(config),
                connect_task: RefCell::new(None),
                reconnect_task: RefCell::new(None),
                this: this_weak.clone().into(),
            };

            let this_weak = this_weak.clone();
            r.channel.on_close(move || {
                let Some(this) = this_weak.upgrade() else {
                    return;
                };
                let callback = this.on_close.borrow().clone();
                if let Some(on_close) = callback {
                    on_close();
                }
                this.schedule_reconnect();
            });
            r
        })
    }

    pub fn downgrade(&self) -> Weak<Self> {
        self.this.borrow().clone()
    }

    pub fn open(&self, host: impl ToString, port: u16) {
        self.stopped.set(false);
        self.connect_generation
            .set(self.connect_generation.get().wrapping_add(1));
        self.cancel_reconnect_timer();
        *self.host.borrow_mut() = host.to_string();
        *self.port.borrow_mut() = port;
        self.do_open();
    }

    pub fn close(&self) {
        if let Some(task) = self.connect_task.borrow_mut().take() {
            task.abort();
        }
        self.stopped.set(true);
        self.connect_generation
            .set(self.connect_generation.get().wrapping_add(1));
        self.connecting.set(false);
        self.cancel_reconnect_timer();
        self.channel.close();
    }

    pub fn set_reconnect(&self, ms: u32) {
        let reschedule = self.reconnect_timer_running.get();
        self.cancel_reconnect_timer();
        *self.reconnect_ms.borrow_mut() = ms;
        if reschedule && ms > 0 && !self.stopped.get() {
            self.schedule_reconnect();
        }
    }

    pub fn cancel_reconnect(&self) {
        *self.reconnect_ms.borrow_mut() = 0;
        self.cancel_reconnect_timer();
    }

    pub(crate) fn disconnect(&self) {
        self.channel.close();
    }

    pub(crate) fn is_open(&self) -> bool {
        self.channel.is_open()
    }

    pub fn stop(&self) {
        self.close();
    }

    pub fn on_open<F>(&self, callback: F)
    where
        F: Fn() + 'static,
    {
        let previous = self.on_open.borrow_mut().replace(Rc::new(callback));
        drop(previous);
    }

    pub fn on_open_failed<F>(&self, callback: F)
    where
        F: Fn(&dyn Error) + 'static,
    {
        let previous = self.on_open_failed.borrow_mut().replace(Rc::new(callback));
        drop(previous);
    }

    pub fn on_data<F>(&self, callback: F)
    where
        F: Fn(Vec<u8>) + 'static,
    {
        self.channel.on_data(callback);
    }

    pub fn on_close<F>(&self, callback: F)
    where
        F: Fn() + 'static,
    {
        let previous = self.on_close.borrow_mut().replace(Rc::new(callback));
        drop(previous);
    }

    pub fn send(&self, data: Vec<u8>) -> bool {
        self.channel.send(data)
    }

    pub fn send_str(&self, data: impl ToString) {
        self.channel.send_str(data);
    }
}

// private
impl TcpClient {
    fn cancel_reconnect_timer(&self) {
        if let Some(task) = self.reconnect_task.borrow_mut().take() {
            task.abort();
        }
        self.reconnect_generation
            .set(self.reconnect_generation.get().wrapping_add(1));
        self.reconnect_timer_running.set(false);
    }

    fn do_open(&self) {
        if self.stopped.get() {
            return;
        }
        self.connecting.set(true);
        let generation = self.connect_generation.get();
        self.config.borrow_mut().init();
        let host = self.host.borrow().clone();
        let port = *self.port.borrow();

        if let Some(task) = self.connect_task.borrow_mut().take() {
            task.abort();
        }
        let this_weak = self.downgrade();
        let channel = self.channel.clone();
        if channel.is_open() {
            channel.close();
        }
        let task = tokio::task::spawn_local(async move {
            channel.wait_close_finish().await;
            let valid = this_weak.upgrade().is_some_and(|this| {
                !this.stopped.get() && this.connect_generation.get() == generation
            });
            if !valid {
                return;
            }
            debug!("connect_tcp: {host} {port}");
            let result = TcpClient::connect_tcp(host, port).await;
            let Some(this) = this_weak.upgrade() else {
                return;
            };
            if this.stopped.get() || this.connect_generation.get() != generation {
                return;
            }
            this.connecting.set(false);
            match result {
                Ok(stream) => {
                    this.channel.do_open(stream);
                    let callback = this.on_open.borrow().clone();
                    if let Some(on_open) = callback {
                        on_open();
                    }
                }
                Err(err) => {
                    let callback = this.on_open_failed.borrow().clone();
                    if let Some(on_open_failed) = callback {
                        on_open_failed(&*err);
                    }
                    this.schedule_reconnect();
                }
            }
        });
        *self.connect_task.borrow_mut() = Some(task);
    }

    async fn connect_tcp(
        host: String,
        port: u16,
    ) -> Result<TcpStream, Box<dyn Error + Send + Sync>> {
        let host = if host == "localhost" {
            "127.0.0.1"
        } else {
            host.as_str()
        };
        Ok(TcpStream::connect((host, port)).await?)
    }

    fn schedule_reconnect(&self) {
        let delay_ms = *self.reconnect_ms.borrow();
        if self.stopped.get()
            || self.channel.is_open()
            || self.connecting.get()
            || self.reconnect_timer_running.get()
            || delay_ms == 0
        {
            return;
        }
        let generation = self.reconnect_generation.get();
        self.reconnect_timer_running.set(true);
        let weak = self.downgrade();
        let task = tokio::task::spawn_local(async move {
            tokio::time::sleep(tokio::time::Duration::from_millis(delay_ms.into())).await;
            let Some(this) = weak.upgrade() else {
                return;
            };
            if this.reconnect_generation.get() != generation {
                return;
            }
            this.reconnect_timer_running.set(false);
            if !this.stopped.get() && !this.channel.is_open() && !this.connecting.get() {
                this.do_open();
            }
        });
        *self.reconnect_task.borrow_mut() = Some(task);
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use tokio::net::TcpListener;
    use tokio::time::{timeout, Duration};

    fn run(future: impl std::future::Future<Output = ()>) {
        let runtime = tokio::runtime::Builder::new_current_thread()
            .enable_all()
            .build()
            .unwrap();
        runtime.block_on(tokio::task::LocalSet::new().run_until(async {
            timeout(Duration::from_secs(3), future).await.unwrap();
        }));
    }

    #[test]
    fn immediate_close_and_open_finishes_old_io() {
        run(async {
            let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
            let client = TcpClient::new(TcpConfig::new());
            let weak = client.downgrade();
            let port = listener.local_addr().unwrap().port();
            let opens = Rc::new(Cell::new(0));
            let copy = opens.clone();
            client.on_open(move || {
                let client = weak.upgrade().unwrap();
                copy.set(copy.get() + 1);
                if copy.get() == 1 {
                    client.close();
                    client.open("127.0.0.1", port);
                }
            });
            client.open("127.0.0.1", port);
            let (mut old_peer, _) = listener.accept().await.unwrap();
            let (_new_peer, _) = listener.accept().await.unwrap();
            while opens.get() < 2 {
                tokio::task::yield_now().await;
            }
            use tokio::io::AsyncReadExt;
            let mut byte = [0];
            assert_eq!(old_peer.read(&mut byte).await.unwrap(), 0);
            client.close();
        });
    }

    #[test]
    fn explicit_close_stops_reconnect_but_allows_manual_open() {
        run(async {
            let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
            let client = TcpClient::new(TcpConfig::new());
            let (opened, mut opens) = tokio::sync::mpsc::unbounded_channel();
            client.on_open(move || {
                let _ = opened.send(());
            });
            let (closed, mut closes) = tokio::sync::mpsc::unbounded_channel();
            client.on_close(move || {
                let _ = closed.send(());
            });
            client.set_reconnect(10);
            client.open("127.0.0.1", listener.local_addr().unwrap().port());
            let (_peer, _) = listener.accept().await.unwrap();
            opens.recv().await.unwrap();
            client.close();
            closes.recv().await.unwrap();
            assert!(timeout(Duration::from_millis(100), listener.accept())
                .await
                .is_err());
            client.open("127.0.0.1", listener.local_addr().unwrap().port());
            let (_peer2, _) = listener.accept().await.unwrap();
            opens.recv().await.unwrap();
            client.close();
        });
    }

    #[test]
    fn unexpected_disconnect_still_reconnects() {
        run(async {
            let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
            let client = TcpClient::new(TcpConfig::new());
            let (opened, mut opens) = tokio::sync::mpsc::unbounded_channel();
            client.on_open(move || {
                let _ = opened.send(());
            });
            client.set_reconnect(1);
            client.open("127.0.0.1", listener.local_addr().unwrap().port());
            let (peer, _) = listener.accept().await.unwrap();
            opens.recv().await.unwrap();
            drop(peer);
            let (_new_peer, _) = listener.accept().await.unwrap();
            opens.recv().await.unwrap();
            client.close();
        });
    }

    #[test]
    fn reconnect_delay_can_change_while_sleeping() {
        run(async {
            let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
            let client = TcpClient::new(TcpConfig::new());
            *client.host.borrow_mut() = "127.0.0.1".into();
            *client.port.borrow_mut() = listener.local_addr().unwrap().port();
            client.stopped.set(false);
            client.set_reconnect(1000);
            client.schedule_reconnect();
            tokio::task::yield_now().await;
            assert!(client.reconnect_timer_running.get());
            client.set_reconnect(1);
            let (_peer, _) = timeout(Duration::from_millis(300), listener.accept())
                .await
                .unwrap()
                .unwrap();
            client.close();
        });
    }

    #[test]
    fn canceled_reconnect_timer_cannot_restart() {
        run(async {
            let client = TcpClient::new(TcpConfig::new());
            client.stopped.set(false);
            client.set_reconnect(1);
            client.schedule_reconnect();
            tokio::task::yield_now().await;
            assert!(client.reconnect_timer_running.get());
            client.cancel_reconnect();
            tokio::task::yield_now().await;
            assert!(!client.reconnect_timer_running.get());
            assert!(!client.connecting.get());
        });
    }

    #[test]
    fn close_before_connect_task_runs_prevents_open() {
        run(async {
            let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
            let client = TcpClient::new(TcpConfig::new());
            client.on_open(|| panic!("opened after close"));
            client.open("127.0.0.1", listener.local_addr().unwrap().port());
            client.close();
            assert!(timeout(Duration::from_millis(100), listener.accept())
                .await
                .is_err());
        });
    }
}

impl Drop for TcpClient {
    fn drop(&mut self) {
        if let Some(task) = self.connect_task.get_mut().take() {
            task.abort();
        }
        if let Some(task) = self.reconnect_task.get_mut().take() {
            task.abort();
        }
        self.channel.close();
    }
}
