use crate::detail::msg_wrapper::{MsgType, MsgWrapper};
use crate::type_def::SeqType;

const PAYLOAD_MIN_LEN: usize = 4 /*seq*/ + 2 /*cmdLen*/ + 1 /*type*/;

pub(crate) fn serialize(msg: &MsgWrapper) -> Vec<u8> {
    let mut payload: Vec<u8> = vec![];
    payload.reserve(PAYLOAD_MIN_LEN);
    payload.extend_from_slice(&msg.seq.to_le_bytes());
    let cmd_len: u16 = msg.cmd.len() as u16;
    payload.extend_from_slice(&cmd_len.to_le_bytes());
    payload.extend(msg.cmd.bytes());
    let type_: u8 = msg.type_.bits();
    payload.extend_from_slice(&type_.to_le_bytes());
    if let Some(request_payload) = &msg.request_payload {
        payload.extend(request_payload);
    } else {
        payload.extend(&msg.data);
    }
    payload
}

pub(crate) fn deserialize(payload: &Vec<u8>) -> Option<MsgWrapper> {
    if payload.len() < PAYLOAD_MIN_LEN {
        return None;
    }

    let mut msg = MsgWrapper::new();
    unsafe {
        let mut p = payload.as_ptr();
        let pend = p.add(payload.len());

        msg.seq = *(p as *const SeqType);
        p = p.add(4);

        let cmd_len: u16;
        cmd_len = *(p as *const u16);
        p = p.add(2);

        msg.cmd = String::from_utf8_unchecked(Vec::from(std::slice::from_raw_parts(p as *mut u8, cmd_len as usize)));
        p = p.add(cmd_len as usize);

        msg.type_ = MsgType::from_bits(*p).unwrap();
        p = p.add(1);

        msg.data = Vec::from(std::slice::from_raw_parts(p, pend.offset_from(p) as usize));
    }

    Some(msg)
}
