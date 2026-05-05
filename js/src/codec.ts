import { copyBytes, textDecoder, textEncoder, toUint8Array, type Bytes } from "./bytes.js";

export interface Codec<T = unknown> {
  encode(value: T): Uint8Array;
  decode(bytes: Bytes): T;
}

export const jsonCodec: Codec = {
  encode(value: unknown): Uint8Array {
    const json = JSON.stringify(value);
    return textEncoder.encode(json === undefined ? "null" : json);
  },

  decode<T = unknown>(bytes: Bytes): T {
    const data = toUint8Array(bytes);
    if (data.byteLength === 0) {
      return undefined as T;
    }
    return JSON.parse(textDecoder.decode(data)) as T;
  },
};

export const stringCodec: Codec<string> = {
  encode(value: string): Uint8Array {
    return textEncoder.encode(value);
  },

  decode(bytes: Bytes): string {
    return textDecoder.decode(toUint8Array(bytes));
  },
};

export const bytesCodec: Codec<Bytes> = {
  encode(value: Bytes): Uint8Array {
    return copyBytes(value);
  },

  decode(bytes: Bytes): Uint8Array {
    return copyBytes(bytes);
  },
};
