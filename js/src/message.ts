import { textDecoder, textEncoder, toUint8Array, type Bytes } from "./bytes.js";

export enum MsgType {
  Command = 1 << 0,
  Response = 1 << 1,
  NeedRsp = 1 << 2,
  Ping = 1 << 3,
  Pong = 1 << 4,
  NoSuchCmd = 1 << 5,
}

export interface RpcMessage {
  seq: number;
  type: number;
  cmd: string;
  data: Uint8Array;
  requestPayload?: Uint8Array;
}

const PAYLOAD_MIN_LEN = 4 + 2 + 1;

export function hasType(type: number, flag: MsgType): boolean {
  return (type & flag) !== 0;
}

export function serializeMessage(msg: RpcMessage): Uint8Array {
  const cmd = textEncoder.encode(msg.cmd);
  if (cmd.byteLength > 0xffff) {
    throw new RangeError("cmd length exceeds uint16");
  }

  const data = msg.requestPayload ?? msg.data;
  const payload = new Uint8Array(PAYLOAD_MIN_LEN + cmd.byteLength + data.byteLength);
  const view = new DataView(payload.buffer, payload.byteOffset, payload.byteLength);
  let offset = 0;

  view.setUint32(offset, msg.seq >>> 0, true);
  offset += 4;
  view.setUint16(offset, cmd.byteLength, true);
  offset += 2;
  payload.set(cmd, offset);
  offset += cmd.byteLength;
  view.setUint8(offset, msg.type & 0xff);
  offset += 1;
  payload.set(data, offset);

  return payload;
}

export function deserializeMessage(payload: Bytes): RpcMessage | undefined {
  const bytes = toUint8Array(payload);
  if (bytes.byteLength < PAYLOAD_MIN_LEN) {
    return undefined;
  }

  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  let offset = 0;
  const seq = view.getUint32(offset, true);
  offset += 4;
  const cmdLen = view.getUint16(offset, true);
  offset += 2;
  if (offset + cmdLen + 1 > bytes.byteLength) {
    return undefined;
  }

  const cmd = textDecoder.decode(bytes.subarray(offset, offset + cmdLen));
  offset += cmdLen;
  const type = view.getUint8(offset);
  offset += 1;
  const data = bytes.slice(offset);

  return { seq, type, cmd, data };
}
