use std::rc::Rc;
use crate::config::{RpcConfig, TcpConfig};

pub struct TcpConfigBuilder {
    auto_pack: bool,
    enable_ipv6: bool,
    max_body_size: u32,
    max_send_buffer_size: u32,
    socket_send_buffer_size: u32,
    socket_recv_buffer_size: u32,
}

impl TcpConfigBuilder {
    pub fn new() -> Self {
        Self {
            auto_pack: false,
            enable_ipv6: false,
            max_body_size: 0,
            max_send_buffer_size: 0,
            socket_send_buffer_size: 0,
            socket_recv_buffer_size: 0,
        }
    }

    pub fn auto_pack(&mut self, auto_pack: bool) -> &mut Self {
        self.auto_pack = auto_pack;
        self
    }

    pub fn enable_ipv6(&mut self, enable_ipv6: bool) -> &mut Self {
        self.enable_ipv6 = enable_ipv6;
        self
    }

    pub fn max_body_size(&mut self, max_body_size: u32) -> &mut Self {
        self.max_body_size = max_body_size;
        self
    }

    pub fn max_send_buffer_size(&mut self, max_send_buffer_size: u32) -> &mut Self {
        self.max_send_buffer_size = max_send_buffer_size;
        self
    }

    pub fn socket_send_buffer_size(&mut self, socket_send_buffer_size: u32) -> &mut Self {
        self.socket_send_buffer_size = socket_send_buffer_size;
        self
    }

    pub fn socket_recv_buffer_size(&mut self, socket_recv_buffer_size: u32) -> &mut Self {
        self.socket_recv_buffer_size = socket_recv_buffer_size;
        self
    }

    pub fn build(&self) -> TcpConfig {
        TcpConfig {
            auto_pack: self.auto_pack,
            enable_ipv6: self.enable_ipv6,
            max_body_size: self.max_body_size,
            max_send_buffer_size: self.max_send_buffer_size,
            socket_send_buffer_size: self.socket_send_buffer_size,
            socket_recv_buffer_size: self.socket_recv_buffer_size,
        }
    }
}

pub struct RpcConfigBuilder {
    rpc: Option<Rc<rpc_core::rpc::Rpc>>,
    ping_interval_ms: u32,
    pong_timeout_ms: u32,
    enable_ipv6: bool,
    max_body_size: u32,
    max_send_buffer_size: u32,
    socket_send_buffer_size: u32,
    socket_recv_buffer_size: u32,
}

impl RpcConfigBuilder {
    pub fn new() -> Self {
        RpcConfigBuilder {
            rpc: None,
            ping_interval_ms: 0,
            pong_timeout_ms: 0,
            enable_ipv6: false,
            max_body_size: 0,
            max_send_buffer_size: 0,
            socket_send_buffer_size: 0,
            socket_recv_buffer_size: 0,
        }
    }

    pub fn rpc(mut self, rpc: Option<Rc<rpc_core::rpc::Rpc>>) -> Self {
        self.rpc = rpc;
        self
    }

    pub fn ping_interval_ms(mut self, ping_interval_ms: u32) -> Self {
        self.ping_interval_ms = ping_interval_ms;
        self
    }

    pub fn pong_timeout_ms(mut self, pong_timeout_ms: u32) -> Self {
        self.pong_timeout_ms = pong_timeout_ms;
        self
    }

    pub fn enable_ipv6(mut self, enable_ipv6: bool) -> Self {
        self.enable_ipv6 = enable_ipv6;
        self
    }

    pub fn max_body_size(mut self, max_body_size: u32) -> Self {
        self.max_body_size = max_body_size;
        self
    }

    pub fn max_send_buffer_size(mut self, max_send_buffer_size: u32) -> Self {
        self.max_send_buffer_size = max_send_buffer_size;
        self
    }

    pub fn socket_send_buffer_size(mut self, socket_send_buffer_size: u32) -> Self {
        self.socket_send_buffer_size = socket_send_buffer_size;
        self
    }

    pub fn socket_recv_buffer_size(mut self, socket_recv_buffer_size: u32) -> Self {
        self.socket_recv_buffer_size = socket_recv_buffer_size;
        self
    }

    pub fn build(self) -> RpcConfig {
        RpcConfig {
            rpc: self.rpc,
            ping_interval_ms: self.ping_interval_ms,
            pong_timeout_ms: self.pong_timeout_ms,
            enable_ipv6: self.enable_ipv6,
            max_body_size: self.max_body_size,
            max_send_buffer_size: self.max_send_buffer_size,
            socket_send_buffer_size: self.socket_send_buffer_size,
            socket_recv_buffer_size: self.socket_recv_buffer_size,
        }
    }
}
