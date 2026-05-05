from __future__ import annotations

from dataclasses import dataclass
from enum import IntFlag


class MsgType(IntFlag):
    COMMAND = 1 << 0
    RESPONSE = 1 << 1
    NEED_RSP = 1 << 2
    PING = 1 << 3
    PONG = 1 << 4
    NO_SUCH_CMD = 1 << 5


@dataclass
class RpcMessage:
    seq: int
    type: int
    cmd: str
    data: bytes
    request_payload: bytes | None = None


PAYLOAD_MIN_LEN = 4 + 2 + 1


def has_type(type_value: int, flag: MsgType) -> bool:
    return (type_value & int(flag)) != 0


def serialize_message(msg: RpcMessage) -> bytes:
    cmd = msg.cmd.encode("utf-8")
    if len(cmd) > 0xFFFF:
        raise ValueError("cmd length exceeds uint16")

    data = msg.request_payload if msg.request_payload is not None else msg.data
    payload = bytearray(PAYLOAD_MIN_LEN + len(cmd) + len(data))
    offset = 0
    payload[offset : offset + 4] = (msg.seq & 0xFFFFFFFF).to_bytes(4, "little")
    offset += 4
    payload[offset : offset + 2] = len(cmd).to_bytes(2, "little")
    offset += 2
    payload[offset : offset + len(cmd)] = cmd
    offset += len(cmd)
    payload[offset] = msg.type & 0xFF
    offset += 1
    payload[offset:] = data
    return bytes(payload)


def deserialize_message(payload: bytes | bytearray | memoryview) -> RpcMessage | None:
    data = bytes(payload)
    if len(data) < PAYLOAD_MIN_LEN:
        return None

    offset = 0
    seq = int.from_bytes(data[offset : offset + 4], "little")
    offset += 4
    cmd_len = int.from_bytes(data[offset : offset + 2], "little")
    offset += 2
    if offset + cmd_len + 1 > len(data):
        return None

    cmd = data[offset : offset + cmd_len].decode("utf-8")
    offset += cmd_len
    type_value = data[offset]
    offset += 1
    return RpcMessage(seq=seq, type=type_value, cmd=cmd, data=data[offset:])
