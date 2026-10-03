use std::cell::{Cell, RefCell};
use std::rc::{Rc, Weak};

use super::config::TcpConfig;
use super::config_builder::RpcConfigBuilder;
use super::detail::tcp_channel::TcpChannel;
use super::rpc_client::RpcClient;
use super::rpc_server::{RpcServer, RpcSession};
use super::tcp_client::TcpClient;
use super::tcp_server::TcpServer;
use crate::rpc::Rpc;

struct OnDrop(Box<dyn Fn()>);

impl Drop for OnDrop {
    fn drop(&mut self) {
        (self.0)();
    }
}

fn check_callback_replacement(install: impl Fn(Box<dyn Fn()>) + 'static) {
    let install = Rc::new(install);
    let weak = Rc::downgrade(&install);
    let released = Rc::new(Cell::new(0));
    let copy = released.clone();
    let guard = OnDrop(Box::new(move || {
        // Capture destruction may install another callback in the same slot.
        weak.upgrade().unwrap()(Box::new(|| {}));
        copy.set(copy.get() + 1);
    }));
    install(Box::new(move || {
        let _keep_guard = &guard;
    }));
    assert_eq!(released.get(), 0);
    install(Box::new(|| {}));
    assert_eq!(released.get(), 1);
}

#[test]
fn tcp_client_callback_captures_can_reenter_setters() {
    let client = TcpClient::new(TcpConfig::new());
    let owner = client.clone();
    check_callback_replacement(move |cb| owner.on_open(cb));
    let owner = client.clone();
    check_callback_replacement(move |cb| owner.on_open_failed(move |_| cb()));
    let owner = client.clone();
    check_callback_replacement(move |cb| owner.on_close(cb));
    check_callback_replacement(move |cb| client.on_data(move |_| cb()));
}

#[test]
fn rpc_client_callback_captures_can_reenter_setters() {
    let client = RpcClient::new(RpcConfigBuilder::new().build());
    let owner = client.clone();
    check_callback_replacement(move |cb| owner.on_open(move |_| cb()));
    let owner = client.clone();
    check_callback_replacement(move |cb| owner.on_open_failed(move |_| cb()));
    check_callback_replacement(move |cb| client.on_close(cb));
}

#[test]
fn server_callback_captures_can_reenter_setters() {
    let server = TcpServer::new(0, TcpConfig::new());
    check_callback_replacement(move |cb| server.on_session(move |_| cb()));
    let server = RpcServer::new(0, RpcConfigBuilder::new().build());
    check_callback_replacement(move |cb| server.on_session(move |_| cb()));
}

#[test]
fn session_callback_captures_can_reenter_setters() {
    let session = RpcSession::new(Rpc::new(None), Weak::new());
    check_callback_replacement(move |cb| session.on_close(cb));
}

#[test]
fn channel_callback_captures_can_reenter_setters() {
    let channel = TcpChannel::new(Rc::new(RefCell::new(TcpConfig::new())));
    let owner = channel.clone();
    check_callback_replacement(move |cb| owner.on_data(move |_| cb()));
    check_callback_replacement(move |cb| channel.on_close(cb));
}
