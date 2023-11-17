use std::sync::Arc;

pub struct TcpConfig {
    pub auto_pack: bool,
    pub enable_ipv6: bool,
    pub max_body_size: u32,
    pub max_send_buffer_size: u32,
    pub socket_send_buffer_size: u32,
    pub socket_recv_buffer_size: u32,
}

impl TcpConfig {
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
}

impl TcpConfig {
    pub fn init(&mut self) {
        if !self.auto_pack && self.max_body_size == std::u32::MAX {
            self.max_body_size = 1024;
        }
    }
}

pub struct RpcConfig {
    pub rpc: Option<Arc<rpc_core::rpc::Rpc>>,
    pub ping_interval_ms: u32,
    pub pong_timeout_ms: u32,
    pub enable_ipv6: bool,
    pub max_body_size: u32,
    pub max_send_buffer_size: u32,
    pub socket_send_buffer_size: u32,
    pub socket_recv_buffer_size: u32,
}

impl RpcConfig {
    pub fn to_tcp_config(&self) -> TcpConfig {
        TcpConfig {
            auto_pack: true,
            enable_ipv6: self.enable_ipv6,
            max_body_size: self.max_body_size,
            max_send_buffer_size: self.max_send_buffer_size,
            socket_send_buffer_size: self.socket_send_buffer_size,
            socket_recv_buffer_size: self.socket_recv_buffer_size,
        }
    }
}
