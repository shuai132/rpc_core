use bitflags::bitflags;
use log::error;
use serde_json::Error;

use crate::type_def::{CmdType, SeqType};

bitflags! {
pub(crate) struct MsgType: u8 {
    const Command = 1 << 0;
    const Response = 1 << 1;
    const NeedRsp = 1 << 2;
    const Ping = 1 << 3;
    const Pong = 1 << 4;
    const NoSuchCmd = 1 << 5;
}
}

pub(crate) struct MsgWrapper {
    pub(crate) seq: SeqType,
    pub(crate) type_: MsgType,
    pub(crate) cmd: CmdType,
    pub(crate) data: Vec<u8>,
    pub(crate) request_payload: Option<Vec<u8>>,
}

impl MsgWrapper {
    pub(crate) fn new() -> Self {
        Self {
            seq: 0,
            type_: MsgType::Command,
            cmd: "".to_string(),
            data: vec![],
            request_payload: None,
        }
    }

    pub(crate) fn dump(&self) -> String {
        format!("seq:{}, type:{}, cmd:{}", self.seq, self.type_.bits(), self.cmd)
    }

    pub(crate) fn unpack_as<'a, T>(&'a self) -> Result<T, Error>
        where T: serde::Deserialize<'a>
    {
        let r = serde_json::from_slice::<T>(self.data.as_slice());
        if r.is_err() {
            error!("deserialize error, msg info:{}", self.dump());
        }
        r
    }

    pub(crate) fn make_rsp<R>(seq: SeqType, rsp: R) -> MsgWrapper
        where R: serde::Serialize
    {
        let mut msg = MsgWrapper::new();
        msg.type_ = MsgType::Response;
        msg.seq = seq;
        msg.data = serde_json::to_string(&rsp).unwrap().into_bytes().into();
        msg
    }
}
