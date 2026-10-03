use std::cell::{Cell, RefCell};
use std::rc::{Rc, Weak};

use log::{debug, error};
use tokio::net::TcpListener;
use tokio::task::JoinHandle;

use crate::net::config::TcpConfig;
use crate::net::detail::tcp_channel::TcpChannel;

pub struct TcpServer {
    port: RefCell<u16>,
    config: Rc<RefCell<TcpConfig>>,
    on_session: RefCell<Option<Rc<dyn Fn(Weak<TcpChannel>)>>>,
    accept_task: RefCell<Option<JoinHandle<()>>>,
    accept_generation: Cell<u64>,
    this: RefCell<Weak<Self>>,
}

// public
impl TcpServer {
    pub fn new(port: u16, config: TcpConfig) -> Rc<Self> {
        Rc::new_cyclic(|this_weak| Self {
            port: port.into(),
            config: Rc::new(RefCell::new(config)),
            on_session: None.into(),
            accept_task: RefCell::new(None),
            accept_generation: Cell::new(0),
            this: this_weak.clone().into(),
        })
    }

    pub fn downgrade(&self) -> Weak<Self> {
        self.this.borrow().clone()
    }

    pub fn start(&self) {
        let generation = self.accept_generation.get().wrapping_add(1);
        self.accept_generation.set(generation);
        self.config.borrow_mut().init();
        let port = *self.port.borrow();
        let host = if self.config.borrow().enable_ipv6 {
            "::"
        } else {
            "0.0.0.0"
        };

        let this_weak = self.this.borrow().clone();

        let previous = self.accept_task.borrow_mut().take();
        if let Some(task) = &previous {
            task.abort();
        }
        let task = tokio::task::spawn_local(async move {
            if let Some(previous) = previous {
                let _ = previous.await;
            }
            debug!("listen: [{host}]:{port}");
            let listener = match TcpListener::bind((host, port)).await {
                Ok(listener) => listener,
                Err(err) => {
                    error!("Failed to listen on [{host}]:{port}: {err}");
                    return;
                }
            };
            loop {
                let result = listener.accept().await;
                let Some(this) = this_weak.upgrade() else {
                    break;
                };
                if this.accept_generation.get() != generation {
                    break;
                }
                match result {
                    Ok((stream, addr)) => {
                        debug!("accept addr: {addr}");
                        let callback = this.on_session.borrow().clone();
                        if let Some(on_session) = callback {
                            let session = TcpChannel::new(this.config.clone());
                            if let Err(err) = session.do_open(stream) {
                                error!("Failed to configure accepted connection: {err}");
                                continue;
                            }
                            on_session(Rc::downgrade(&session));
                        }
                    }
                    Err(err) => error!("Error accepting connection: {err}"),
                }
                // Abort takes effect at a task poll boundary. A callback may
                // stop/restart us while more accepts are immediately ready.
                if this.accept_generation.get() != generation {
                    break;
                }
            }
        });
        *self.accept_task.borrow_mut() = Some(task);
    }

    pub fn stop(&self) {
        self.accept_generation
            .set(self.accept_generation.get().wrapping_add(1));
        if let Some(task) = self.accept_task.borrow().as_ref() {
            task.abort();
        }
    }

    pub fn on_session<F>(&self, callback: F)
    where
        F: Fn(Weak<TcpChannel>) + 'static,
    {
        let previous = self.on_session.borrow_mut().replace(Rc::new(callback));
        drop(previous);
    }
}

impl Drop for TcpServer {
    fn drop(&mut self) {
        if let Some(task) = self.accept_task.get_mut().take() {
            task.abort();
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use tokio::io::AsyncReadExt;
    use tokio::net::TcpStream;
    use tokio::time::{sleep, timeout, Duration};

    #[test]
    fn socket_configuration_failure_keeps_accepting() {
        let runtime = tokio::runtime::Builder::new_current_thread()
            .enable_all()
            .build()
            .unwrap();
        runtime.block_on(tokio::task::LocalSet::new().run_until(async {
            timeout(Duration::from_secs(3), async {
                let reserved = TcpListener::bind("127.0.0.1:0").await.unwrap();
                let address = reserved.local_addr().unwrap();
                let server = TcpServer::new(
                    address.port(),
                    TcpConfig {
                        socket_recv_buffer_size: u32::MAX,
                        ..TcpConfig::new()
                    },
                );
                let (tx, mut sessions) = tokio::sync::mpsc::unbounded_channel();
                server.on_session(move |session| {
                    tx.send(session).unwrap();
                });
                drop(reserved);
                server.start();
                let mut rejected = loop {
                    if let Ok(stream) = TcpStream::connect(address).await {
                        break stream;
                    }
                    sleep(Duration::from_millis(1)).await;
                };
                assert_eq!(rejected.read(&mut [0]).await.unwrap(), 0);
                assert!(sessions.try_recv().is_err());
                server.config.borrow_mut().socket_recv_buffer_size = 32768;
                let mut peer = TcpStream::connect(address).await.unwrap();
                let session = sessions.recv().await.unwrap().upgrade().unwrap();
                assert!(session.is_open());
                session.close();
                session.wait_close_finish().await;
                assert_eq!(peer.read(&mut [0]).await.unwrap(), 0);
                server.stop();
            })
            .await
            .unwrap();
        }));
    }
}
