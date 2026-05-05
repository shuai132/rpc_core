export type Bytes = Uint8Array | ArrayBuffer | ArrayBufferView;

export const EMPTY_BYTES: Uint8Array = new Uint8Array(0);

export const textEncoder = new TextEncoder();
export const textDecoder = new TextDecoder();

export function toUint8Array(data: Bytes): Uint8Array {
  if (data instanceof Uint8Array) {
    return data;
  }
  if (data instanceof ArrayBuffer) {
    return new Uint8Array(data);
  }
  if (ArrayBuffer.isView(data)) {
    return new Uint8Array(data.buffer, data.byteOffset, data.byteLength);
  }
  throw new TypeError("unsupported byte container");
}

export function copyBytes(data: Bytes): Uint8Array {
  return toUint8Array(data).slice();
}

export function concatBytes(chunks: readonly Bytes[]): Uint8Array {
  let size = 0;
  for (const chunk of chunks) {
    size += toUint8Array(chunk).byteLength;
  }

  const result = new Uint8Array(size);
  let offset = 0;
  for (const chunk of chunks) {
    const bytes = toUint8Array(chunk);
    result.set(bytes, offset);
    offset += bytes.byteLength;
  }
  return result;
}
