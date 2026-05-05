use crate::detail::msg_wrapper::{MsgType, MsgWrapper};
use crate::type_def::SeqType;
use log::error;

const PAYLOAD_MIN_LEN: usize = 4 /*seq*/ + 2 /*cmdLen*/ + 1 /*type*/;

pub fn serialize(msg: &MsgWrapper) -> Option<Vec<u8>> {
    if msg.cmd.len() > u16::MAX as usize {
        error!("cmd length exceeds uint16: {}", msg.cmd.len());
        return None;
    }

    let mut payload: Vec<u8> = Vec::with_capacity(PAYLOAD_MIN_LEN + msg.cmd.len() + msg.data.len());
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
    Some(payload)
}

pub fn deserialize(payload: &[u8]) -> Option<MsgWrapper> {
    if payload.len() < PAYLOAD_MIN_LEN {
        return None;
    }

    let mut msg = MsgWrapper::new();
    let seq = payload.get(0..4)?;
    msg.seq = SeqType::from_le_bytes(seq.try_into().ok()?);

    let cmd_len = payload.get(4..6)?;
    let cmd_len = u16::from_le_bytes(cmd_len.try_into().ok()?) as usize;
    let type_offset = 6 + cmd_len;
    if type_offset + 1 > payload.len() {
        return None;
    }

    msg.cmd = String::from_utf8(payload[6..type_offset].to_vec()).ok()?;
    msg.type_ = MsgType::from_bits(payload[type_offset])?;
    msg.data = payload[type_offset + 1..].to_vec();

    Some(msg)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn message_header_uses_little_endian_and_rejects_long_cmd() {
        let mut msg = MsgWrapper::new();
        msg.seq = 0x01020304;
        msg.cmd = "cmd".to_string();
        msg.type_ = MsgType::Command | MsgType::PayloadJson;
        msg.data = b"payload".to_vec();

        let payload = serialize(&msg).unwrap();
        assert_eq!(&payload[0..6], &[0x04, 0x03, 0x02, 0x01, 0x03, 0x00]);

        let decoded = deserialize(&payload).unwrap();
        assert_eq!(decoded.seq, msg.seq);
        assert_eq!(decoded.cmd, msg.cmd);
        assert_eq!(decoded.type_.bits(), msg.type_.bits());
        assert_eq!(decoded.data, msg.data);

        msg.cmd = "x".repeat(u16::MAX as usize + 1);
        assert!(serialize(&msg).is_none());
    }

    #[test]
    fn malformed_cmd_length_is_rejected() {
        let payload = [0, 0, 0, 0, 3, 0, b'a', b'b'];
        assert!(deserialize(&payload).is_none());
    }
}
