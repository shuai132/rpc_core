import { EMPTY_BYTES, copyBytes, type Bytes } from "./bytes.js";
import { jsonCodec, type Codec } from "./codec.js";
import { DefaultConnection, type Connection } from "./connection.js";
import { MsgType, deserializeMessage, hasType, serializeMessage, type RpcMessage } from "./message.js";
import { FinallyType, Request, type ResponseCallback } from "./request.js";

export type TimerImpl = (ms: number, cb: () => void) => void;
export type ErrorHandler = (error: unknown, msg?: RpcMessage) => void;

export interface SubscribeOptions<P, R> {
  codec?: Codec<any>;
  requestCodec?: Codec<P>;
  responseCodec?: Codec<R>;
}

type CommandHandle = (msg: RpcMessage) => Promise<Uint8Array>;
type ResponseHandle = (msg: RpcMessage) => boolean;

export class Rpc {
  private readonly connection: Connection;
  private readonly cmdHandleMap = new Map<string, CommandHandle>();
  private readonly rspHandleMap = new Map<number, ResponseHandle>();
  private seqValue = 0;
  private ready = false;
  private timerImpl: TimerImpl | undefined = (ms, cb) => {
    setTimeout(cb, ms);
  };
  private errorHandler: ErrorHandler | undefined;

  constructor(connection: Connection = new DefaultConnection()) {
    this.connection = connection;
    this.connection.setRecvPackageImpl((pkg) => {
      this.onRecvPackage(pkg);
    });
  }

  static create(connection?: Connection): Rpc {
    return new Rpc(connection);
  }

  subscribe<P = unknown, R = unknown>(
    cmd: string,
    handle: (req: P, msg: RpcMessage) => R | Promise<R>,
    options: SubscribeOptions<P, R> = {},
  ): void {
    const requestCodec = (options.requestCodec ?? options.codec ?? jsonCodec) as Codec<P>;
    const responseCodec = (options.responseCodec ?? options.codec ?? jsonCodec) as Codec<R>;

    this.cmdHandleMap.set(cmd, async (msg) => {
      const req = requestCodec.decode(msg.data);
      const rsp = await handle(req, msg);
      return responseCodec.encode(rsp);
    });
  }

  subscribeRaw(cmd: string, handle: (req: Uint8Array, msg: RpcMessage) => Bytes | void | Promise<Bytes | void>): void {
    this.cmdHandleMap.set(cmd, async (msg) => {
      const rsp = await handle(copyBytes(msg.data), msg);
      return rsp === undefined ? EMPTY_BYTES : copyBytes(rsp);
    });
  }

  unsubscribe(cmd: string): void {
    this.cmdHandleMap.delete(cmd);
  }

  createRequest(): Request {
    return new Request(this);
  }

  cmd(cmd: string): Request {
    return this.createRequest().cmd(cmd);
  }

  ping<T = unknown>(payload?: T, codec?: Codec<T>): Request {
    const request = this.createRequest().ping();
    if (arguments.length > 0) {
      request.msg(payload as T, codec);
    }
    return request;
  }

  call<TReq>(cmd: string, message: TReq): Request;
  call<TReq, TRsp>(cmd: string, message: TReq, rsp: ResponseCallback<TRsp>): Request;
  call<TReq, TRsp>(cmd: string, message: TReq, rsp?: ResponseCallback<TRsp>): Request {
    const request = this.cmd(cmd).msg(message);
    if (rsp !== undefined) {
      request.rsp(rsp);
    }
    request.call();
    return request;
  }

  setTimer(timerImpl?: TimerImpl): void {
    this.timerImpl = timerImpl;
  }

  setReady(ready: boolean): void {
    this.ready = ready;
  }

  isReady(): boolean {
    return this.ready;
  }

  getConnection(): Connection {
    return this.connection;
  }

  onError(handle: ErrorHandler): void {
    this.errorHandler = handle;
  }

  makeSeq(): number {
    const seq = this.seqValue >>> 0;
    this.seqValue = (seq + 1) >>> 0;
    return seq;
  }

  sendRequest(request: Request): void {
    if (request._needRsp()) {
      this.subscribeResponse(
        request._seq(),
        (msg) => request._handleResponse(msg),
        () => request._onTimeout(),
        request._timeoutMs(),
      );
    }

    let type = MsgType.Command;
    if (request._isPing()) {
      type |= MsgType.Ping;
    }
    if (request._needRsp()) {
      type |= MsgType.NeedRsp;
    }

    this.sendMessage({
      seq: request._seq(),
      type,
      cmd: request._cmd(),
      data: request._payload(),
    });
  }

  unsubscribeResponse(seq: number): void {
    this.rspHandleMap.delete(seq >>> 0);
  }

  onRecvPackage(pkg: Bytes): void {
    const msg = deserializeMessage(pkg);
    if (msg === undefined) {
      this.emitError(new Error("payload deserialize error"));
      return;
    }

    void this.dispatch(msg).catch((error: unknown) => {
      this.emitError(error, msg);
    });
  }

  private subscribeResponse(seq: number, handle: ResponseHandle, timeoutCb: () => void, timeoutMs: number): void {
    const normalizedSeq = seq >>> 0;
    this.rspHandleMap.set(normalizedSeq, handle);
    this.timerImpl?.(timeoutMs, () => {
      if (this.rspHandleMap.delete(normalizedSeq)) {
        timeoutCb();
      }
    });
  }

  private async dispatch(msg: RpcMessage): Promise<void> {
    if (hasType(msg.type, MsgType.Command)) {
      if (hasType(msg.type, MsgType.Ping)) {
        this.sendMessage({
          ...msg,
          type: MsgType.Response | MsgType.Pong,
        });
        return;
      }

      const handle = this.cmdHandleMap.get(msg.cmd);
      const needRsp = hasType(msg.type, MsgType.NeedRsp);
      if (handle === undefined) {
        if (needRsp) {
          this.sendMessage({
            seq: msg.seq,
            type: MsgType.Response | MsgType.NoSuchCmd,
            cmd: "",
            data: EMPTY_BYTES,
          });
        }
        return;
      }

      if (!needRsp) {
        void handle(msg).catch((error: unknown) => {
          this.emitError(error, msg);
        });
        return;
      }

      const data = await handle(msg);
      this.sendMessage({
        seq: msg.seq,
        type: MsgType.Response,
        cmd: "",
        data,
      });
      return;
    }

    if (hasType(msg.type, MsgType.Response)) {
      const handle = this.rspHandleMap.get(msg.seq);
      if (handle === undefined) {
        return;
      }

      this.rspHandleMap.delete(msg.seq);
      if (!handle(msg)) {
        this.emitError(new Error("response deserialize error"), msg);
      }
      return;
    }

    this.emitError(new Error("unknown message type"), msg);
  }

  private sendMessage(msg: RpcMessage): void {
    try {
      const result = this.connection.sendPackage(serializeMessage(msg));
      if (result instanceof Promise) {
        result.catch((error: unknown) => {
          this.emitError(error, msg);
        });
      }
    } catch (error) {
      this.emitError(error, msg);
    }
  }

  private emitError(error: unknown, msg?: RpcMessage): void {
    this.errorHandler?.(error, msg);
  }
}

export { FinallyType, Request };
