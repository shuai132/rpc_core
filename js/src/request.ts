import { EMPTY_BYTES, copyBytes, type Bytes } from "./bytes.js";
import { jsonCodec, type Codec } from "./codec.js";
import { MsgType, hasType, type RpcMessage } from "./message.js";
import type { Dispose } from "./dispose.js";
import type { Rpc } from "./rpc.js";

export enum FinallyType {
  Normal = "normal",
  NoNeedRsp = "no_need_rsp",
  Timeout = "timeout",
  Canceled = "canceled",
  RpcExpired = "rpc_expired",
  RpcNotReady = "rpc_not_ready",
  RspSerializeError = "rsp_serialize_error",
  NoSuchCmd = "no_such_cmd",
}

export interface RequestResult<T> {
  type: FinallyType;
  data?: T;
  error?: unknown;
}

export interface CodecOption<T> {
  codec?: Codec<T>;
}

export type ResponseCallback<T> = (rsp: T, type: FinallyType) => void;
export type FinallyCallback = (type: FinallyType) => void;

export class Request {
  private rpcRef: Rpc | undefined;
  private seqValue = 0;
  private cmdValue = "";
  private payloadValue: Uint8Array | undefined;
  private needRspValue = false;
  private canceledValue = false;
  private rspHandleValue: ((msg: RpcMessage) => boolean) | undefined;
  private timeoutMsValue = 3000;
  private timeoutCbValue: () => void = () => {};
  private finallyTypeValue = FinallyType.NoNeedRsp;
  private finallyCbValue: FinallyCallback | undefined;
  private retryCountValue = 0;
  private waitingRspValue = false;
  private isPingValue = false;
  private responseErrorValue: unknown;

  constructor(rpc?: Rpc) {
    this.rpcRef = rpc;
    this.timeout(() => {});
  }

  static create(rpc?: Rpc): Request {
    return new Request(rpc);
  }

  cmd(cmd: string): this {
    this.cmdValue = cmd;
    return this;
  }

  msg<T>(message: T, codec?: Codec<T>): this {
    const selectedCodec = codec ?? (jsonCodec as Codec<T>);
    this.payloadValue = selectedCodec.encode(message);
    return this;
  }

  raw(bytes: Bytes): this {
    this.payloadValue = copyBytes(bytes);
    return this;
  }

  rsp<T>(cb: ResponseCallback<T>, options: CodecOption<T> = {}): this {
    const selectedCodec = options.codec ?? (jsonCodec as Codec<T>);
    this.needRspValue = true;
    this.rspHandleValue = (msg) => {
      if (this.canceledValue) {
        this.onFinish(FinallyType.Canceled);
        return true;
      }

      if (hasType(msg.type, MsgType.NoSuchCmd)) {
        this.onFinish(FinallyType.NoSuchCmd);
        return true;
      }

      try {
        const rsp = selectedCodec.decode(msg.data);
        cb(rsp, FinallyType.Normal);
        this.onFinish(FinallyType.Normal);
        return true;
      } catch (error) {
        this.responseErrorValue = error;
        this.onFinish(FinallyType.RspSerializeError);
        return false;
      }
    };
    return this;
  }

  rawRsp(cb: ResponseCallback<Uint8Array>): this {
    this.needRspValue = true;
    this.rspHandleValue = (msg) => {
      if (this.canceledValue) {
        this.onFinish(FinallyType.Canceled);
        return true;
      }

      if (hasType(msg.type, MsgType.NoSuchCmd)) {
        this.onFinish(FinallyType.NoSuchCmd);
        return true;
      }

      cb(copyBytes(msg.data), FinallyType.Normal);
      this.onFinish(FinallyType.Normal);
      return true;
    };
    return this;
  }

  promise<T>(options?: CodecOption<T>, rpc?: Rpc): Promise<RequestResult<T>> {
    return new Promise((resolve) => {
      let resolved = false;
      const previousFinally = this.finallyCbValue;

      this.rsp<T>((rsp) => {
        resolved = true;
        resolve({ type: FinallyType.Normal, data: rsp });
      }, options);

      this.finally((type) => {
        previousFinally?.(type);
        if (!resolved) {
          resolved = true;
          const result: RequestResult<T> = { type };
          if (this.responseErrorValue !== undefined) {
            result.error = this.responseErrorValue;
          }
          resolve(result);
        }
      });

      this.call(rpc);
    });
  }

  future<T>(options?: CodecOption<T>, rpc?: Rpc): Promise<RequestResult<T>> {
    return this.promise(options, rpc);
  }

  finally(cb: FinallyCallback): this {
    this.finallyCbValue = cb;
    return this;
  }

  call(rpc?: Rpc): this {
    if (rpc !== undefined) {
      this.rpcRef = rpc;
    }

    this.waitingRspValue = true;

    if (this.canceledValue) {
      this.onFinish(FinallyType.Canceled);
      return this;
    }

    if (this.rpcRef === undefined) {
      this.onFinish(FinallyType.RpcExpired);
      return this;
    }

    if (!this.rpcRef.isReady()) {
      this.onFinish(FinallyType.RpcNotReady);
      return this;
    }

    this.seqValue = this.rpcRef.makeSeq();
    this.rpcRef.sendRequest(this);

    if (!this.needRspValue) {
      this.onFinish(FinallyType.NoNeedRsp);
    }
    return this;
  }

  ping(): this {
    this.isPingValue = true;
    return this;
  }

  timeoutMs(timeoutMs: number): this {
    this.timeoutMsValue = Math.max(0, timeoutMs >>> 0);
    return this;
  }

  timeout(cb: () => void): this {
    this.timeoutCbValue = () => {
      cb();
      if (this.retryCountValue === -1) {
        this.call();
      } else if (this.retryCountValue > 0) {
        this.retryCountValue -= 1;
        this.call();
      } else {
        this.onFinish(FinallyType.Timeout);
      }
    };
    return this;
  }

  addTo(dispose: Dispose): this {
    dispose.add(this);
    return this;
  }

  cancel(): this {
    this.canceled(true);
    if (this.needRspValue && this.waitingRspValue && this.rpcRef !== undefined) {
      this.rpcRef.unsubscribeResponse(this.seqValue);
    }
    this.onFinish(FinallyType.Canceled);
    return this;
  }

  resetCancel(): this {
    return this.canceled(false);
  }

  retry(count: number): this {
    this.retryCountValue = Math.trunc(count);
    return this;
  }

  disableRsp(): this {
    this.needRspValue = false;
    return this;
  }

  enableRsp(): this {
    this.needRspValue = true;
    return this;
  }

  markNeedRsp(): this {
    this.needRspValue = true;
    this.rspHandleValue = (msg) => {
      if (this.canceledValue) {
        this.onFinish(FinallyType.Canceled);
      } else if (hasType(msg.type, MsgType.NoSuchCmd)) {
        this.onFinish(FinallyType.NoSuchCmd);
      } else {
        this.onFinish(FinallyType.Normal);
      }
      return true;
    };
    return this;
  }

  rpc(rpc: Rpc): this {
    this.rpcRef = rpc;
    return this;
  }

  getRpc(): Rpc | undefined {
    return this.rpcRef;
  }

  isCanceled(): boolean {
    return this.canceledValue;
  }

  canceled(canceled: boolean): this {
    this.canceledValue = canceled;
    return this;
  }

  get finallyType(): FinallyType {
    return this.finallyTypeValue;
  }

  _seq(): number {
    return this.seqValue;
  }

  _cmd(): string {
    return this.cmdValue;
  }

  _payload(): Uint8Array {
    return this.payloadValue ?? EMPTY_BYTES;
  }

  _needRsp(): boolean {
    return this.needRspValue;
  }

  _isPing(): boolean {
    return this.isPingValue;
  }

  _timeoutMs(): number {
    return this.timeoutMsValue;
  }

  _handleResponse(msg: RpcMessage): boolean {
    return this.rspHandleValue?.(msg) ?? true;
  }

  _onTimeout(): void {
    this.timeoutCbValue();
  }

  private onFinish(type: FinallyType): void {
    if (!this.waitingRspValue) {
      return;
    }

    this.waitingRspValue = false;
    this.finallyTypeValue = type;
    this.finallyCbValue?.(type);
  }
}
