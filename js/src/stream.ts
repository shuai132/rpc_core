import { EMPTY_BYTES, concatBytes, toUint8Array, type Bytes } from "./bytes.js";

export interface StreamPackageParserOptions {
  maxBodySize?: number | undefined;
  onPackage?: ((pkg: Uint8Array) => void) | undefined;
}

export function packStreamPackage(pkg: Bytes): Uint8Array {
  const body = toUint8Array(pkg);
  if (body.byteLength > 0xffffffff) {
    throw new RangeError("package length exceeds uint32");
  }

  const packed = new Uint8Array(4 + body.byteLength);
  new DataView(packed.buffer, packed.byteOffset, packed.byteLength).setUint32(0, body.byteLength, true);
  packed.set(body, 4);
  return packed;
}

export class StreamPackageParser {
  private readonly maxBodySize: number;
  private buffer: Uint8Array = EMPTY_BYTES;
  private bodySize: number | undefined;

  onPackage: ((pkg: Uint8Array) => void) | undefined;

  constructor(options: StreamPackageParserOptions = {}) {
    this.maxBodySize = options.maxBodySize ?? 0xffffffff;
    this.onPackage = options.onPackage;
  }

  reset(): void {
    this.buffer = EMPTY_BYTES;
    this.bodySize = undefined;
  }

  feed(chunk: Bytes): void {
    const bytes = toUint8Array(chunk);
    if (bytes.byteLength === 0) {
      return;
    }

    this.buffer = this.buffer.byteLength === 0 ? bytes.slice() : concatBytes([this.buffer, bytes]);

    for (;;) {
      if (this.bodySize === undefined) {
        if (this.buffer.byteLength < 4) {
          return;
        }
        this.bodySize = new DataView(this.buffer.buffer, this.buffer.byteOffset, this.buffer.byteLength).getUint32(0, true);
        if (this.bodySize > this.maxBodySize) {
          this.reset();
          throw new RangeError(`body_size > max_body_size: ${this.bodySize} > ${this.maxBodySize}`);
        }
        this.buffer = this.buffer.slice(4);
      }

      if (this.buffer.byteLength < this.bodySize) {
        return;
      }

      const pkg = this.buffer.slice(0, this.bodySize);
      this.buffer = this.buffer.slice(this.bodySize);
      this.bodySize = undefined;
      this.onPackage?.(pkg);
    }
  }
}
