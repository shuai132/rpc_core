use std::cell::RefCell;
use std::net::SocketAddr;
use std::rc::{Rc, Weak};

use log::{debug, trace};
use tokio::net::TcpListener;
use tokio::select;
use tokio::sync::Notify;

use crate::config::TcpConfig;
use crate::detail::tcp_channel::TcpChannel;

pub struct TcpServer {
    port: RefCell<u16>,
    config: Rc<RefCell<TcpConfig>>,
    on_session: RefCell<Option<Box<dyn Fn(Weak<TcpChannel>)>>>,
    quit_notify: Notify,
    this: RefCell<Weak<Self>>,
}

// public
impl TcpServer {
    pub fn new(port: u16, config: TcpConfig) -> Rc<Self> {
        let config = Rc::new(RefCell::new(config));
        let r = Rc::new(Self {
            port: port.into(),
            config: config.clone(),
            on_session: None.into(),
            quit_notify: Notify::new(),
            this: Weak::new().into(),
        });
        let this_weak = Rc::downgrade(&r);
        *r.this.borrow_mut() = this_weak.clone().into();
        r
    }

    pub fn downgrade(&self) -> Weak<Self> {
        self.this.borrow().clone()
    }

    pub fn start(&self) {
        self.config.borrow_mut().init();
        let port = *self.port.borrow();

        let this_weak = self.this.borrow().clone();

        tokio::task::spawn_local(async move {
            debug!("listen: {port}");
            let listener = TcpListener::bind(SocketAddr::new("0.0.0.0".parse().unwrap(), port)).await.unwrap();
            loop {
                let this = this_weak.upgrade().unwrap();
                select! {
                    res = listener.accept() => {
                        match res {
                            Ok((stream, addr))=> {
                                debug!("accept addr: {addr}");
                                tokio::task::spawn_local(async move {
                                    let session = TcpChannel::new(this.config.clone());
                                    if let Some(on_session) = this.on_session.borrow_mut().as_ref() {
                                        session.do_open(stream);
                                        on_session(Rc::downgrade(&session));
                                    }
                                });
                            },
                            Err(e) => {
                                println!("Error accepting connection: {}", e);
                            }
                        }
                    }
                    _ = this.quit_notify.notified() => {
                        trace!("server: stop");
                        break;
                    }
                }
            }
        });
    }

    pub fn stop(&self) {
        self.quit_notify.notify_one();
    }

    pub fn on_session<F>(&self, callback: F)
        where F: Fn(Weak<TcpChannel>) + 'static,
    {
        *self.on_session.borrow_mut() = Some(Box::new(callback));
    }
}

