use crate::detail::msg_wrapper::{MsgType, MsgWrapper};

const PAYLOAD_MIN_LEN: usize = 4 /*seq*/ + 2 /*cmdLen*/ + 1 /*type*/;

pub fn serialize(msg: &MsgWrapper) -> Result<Vec<u8>, ()> {
    let cmd_len = u16::try_from(msg.cmd.len()).map_err(|_| ())?;
    let data = msg.request_payload.as_deref().unwrap_or(&msg.data);
    let mut payload = Vec::with_capacity(PAYLOAD_MIN_LEN + msg.cmd.len() + data.len());
    payload.extend_from_slice(&msg.seq.to_le_bytes());
    payload.extend_from_slice(&cmd_len.to_le_bytes());
    payload.extend_from_slice(msg.cmd.as_bytes());
    payload.push(msg.type_.bits());
    payload.extend_from_slice(data);
    Ok(payload)
}

pub fn deserialize(payload: &[u8]) -> Option<MsgWrapper> {
    if payload.len() < PAYLOAD_MIN_LEN {
        return None;
    }

    let seq = u32::from_le_bytes(payload[..4].try_into().ok()?);
    let cmd_len = u16::from_le_bytes(payload[4..6].try_into().ok()?) as usize;
    let type_offset = 6usize.checked_add(cmd_len)?;
    let type_ = MsgType::from_bits(*payload.get(type_offset)?)?;
    let cmd = std::str::from_utf8(payload.get(6..type_offset)?)
        .ok()?
        .to_owned();
    let data = payload.get(type_offset + 1..)?.to_vec();

    Some(MsgWrapper {
        seq,
        type_,
        cmd,
        data,
        request_payload: None,
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn rejects_truncated_and_invalid_headers() {
        for size in 0..PAYLOAD_MIN_LEN {
            assert!(deserialize(&vec![0; size]).is_none());
        }
        assert!(deserialize(&[0, 0, 0, 0, 2, 0, b'a', 1]).is_none());
        assert!(deserialize(&[0, 0, 0, 0, 1, 0, 0xff, 1]).is_none());
    }

    #[test]
    fn rejects_oversized_command_without_truncating_it() {
        let mut msg = MsgWrapper::new();
        msg.cmd = "a".repeat(u16::MAX as usize + 1);
        assert!(serialize(&msg).is_err());
    }

    #[test]
    fn round_trip() {
        let mut msg = MsgWrapper::new();
        msg.seq = 42;
        msg.cmd = "命令".to_owned();
        msg.data = vec![0, 255, 1];
        let decoded = deserialize(&serialize(&msg).unwrap()).unwrap();
        assert_eq!(decoded.seq, msg.seq);
        assert_eq!(decoded.cmd, msg.cmd);
        assert_eq!(decoded.type_.bits(), msg.type_.bits());
        assert_eq!(decoded.data, msg.data);
    }
}
