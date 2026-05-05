from __future__ import annotations

import asyncio
import inspect
from typing import Any, Awaitable, Callable

from .codec import Codec, json_codec
from .connection import Connection, DefaultConnection
from .message import MsgType, RpcMessage, deserialize_message, has_type, serialize_message
from .request import Request

TimerImpl = Callable[[int, Callable[[], None]], None]
ErrorHandler = Callable[[BaseException, RpcMessage | None], None]
CommandHandle = Callable[[RpcMessage], Awaitable[tuple[bytes, bool]]]
ResponseHandle = Callable[[RpcMessage], bool]

_MISSING = object()


def _callback_positional_count(cb: Callable[..., Any]) -> int | None:
    try:
        signature = inspect.signature(cb)
    except (TypeError, ValueError):
        return None
    count = 0
    for parameter in signature.parameters.values():
        if parameter.kind == inspect.Parameter.VAR_POSITIONAL:
            return None
        if parameter.kind in (inspect.Parameter.POSITIONAL_ONLY, inspect.Parameter.POSITIONAL_OR_KEYWORD):
            count += 1
    return count


async def _maybe_await(value: Any) -> Any:
    if inspect.isawaitable(value):
        return await value
    return value


class Rpc:
    def __init__(self, connection: Connection | None = None) -> None:
        self._connection = connection or DefaultConnection()
        self._cmd_handle_map: dict[str, CommandHandle] = {}
        self._rsp_handle_map: dict[int, ResponseHandle] = {}
        self._seq = 0
        self._ready = False
        self._timer_impl: TimerImpl | None = None
        self._error_handler: ErrorHandler | None = None
        self._connection.set_recv_package_impl(self.on_recv_package)

    @staticmethod
    def create(connection: Connection | None = None) -> "Rpc":
        return Rpc(connection)

    def subscribe(
        self,
        cmd: str,
        handle: Callable[..., Any],
        *,
        codec: Codec[Any] | None = None,
        request_codec: Codec[Any] | None = None,
        response_codec: Codec[Any] | None = None,
    ) -> None:
        selected_request_codec = request_codec or codec or json_codec
        selected_response_codec = response_codec or codec or json_codec
        positional_count = _callback_positional_count(handle)

        async def command_handle(msg: RpcMessage) -> tuple[bytes, bool]:
            req = selected_request_codec.decode(msg.data)
            if positional_count == 0:
                rsp = await _maybe_await(handle())
            elif positional_count == 1:
                rsp = await _maybe_await(handle(req))
            else:
                rsp = await _maybe_await(handle(req, msg))
            return selected_response_codec.encode(rsp), selected_response_codec is json_codec

        self._cmd_handle_map[cmd] = command_handle

    def subscribe_raw(
        self,
        cmd: str,
        handle: Callable[..., bytes | bytearray | memoryview | None | Awaitable[bytes | bytearray | memoryview | None]],
    ) -> None:
        positional_count = _callback_positional_count(handle)

        async def command_handle(msg: RpcMessage) -> tuple[bytes, bool]:
            if positional_count == 0:
                rsp = await _maybe_await(handle())
            elif positional_count == 1:
                rsp = await _maybe_await(handle(bytes(msg.data)))
            else:
                rsp = await _maybe_await(handle(bytes(msg.data), msg))
            return (b"" if rsp is None else bytes(rsp)), False

        self._cmd_handle_map[cmd] = command_handle

    def unsubscribe(self, cmd: str) -> None:
        self._cmd_handle_map.pop(cmd, None)

    def create_request(self) -> Request:
        return Request(self)

    def cmd(self, cmd: str) -> Request:
        return self.create_request().cmd(cmd)

    def ping(self, payload: Any = _MISSING, codec: Codec[Any] | None = None) -> Request:
        request = self.create_request().ping()
        if payload is not _MISSING:
            request.msg(payload, codec)
        return request

    def call(self, cmd: str, message: Any, rsp: Callable[..., None] | None = None) -> Request:
        request = self.cmd(cmd).msg(message)
        if rsp is not None:
            request.rsp(rsp)
        request.call()
        return request

    def set_timer(self, timer_impl: TimerImpl | None) -> None:
        self._timer_impl = timer_impl

    def set_ready(self, ready: bool) -> None:
        self._ready = ready

    def is_ready(self) -> bool:
        return self._ready

    def get_connection(self) -> Connection:
        return self._connection

    def on_error(self, handle: ErrorHandler) -> None:
        self._error_handler = handle

    def make_seq(self) -> int:
        seq = self._seq & 0xFFFFFFFF
        self._seq = (seq + 1) & 0xFFFFFFFF
        return seq

    def send_request(self, request: Request) -> None:
        if request._need_rsp:
            self._subscribe_response(
                request._seq,
                request._handle_response,
                request._on_timeout,
                request._timeout_ms,
            )

        type_value = int(MsgType.COMMAND)
        if request._is_ping:
            type_value |= int(MsgType.PING)
        if request._need_rsp:
            type_value |= int(MsgType.NEED_RSP)
        if request._payload_json:
            type_value |= int(MsgType.PAYLOAD_JSON)

        self._send_message(
            RpcMessage(
                seq=request._seq,
                type=type_value,
                cmd=request._cmd,
                data=request._payload or b"",
            )
        )

    def unsubscribe_response(self, seq: int) -> None:
        self._rsp_handle_map.pop(seq & 0xFFFFFFFF, None)

    def on_recv_package(self, package: bytes | bytearray | memoryview) -> None:
        msg = deserialize_message(package)
        if msg is None:
            self._emit_error(ValueError("payload deserialize error"), None)
            return

        try:
            loop = asyncio.get_running_loop()
        except RuntimeError:
            asyncio.run(self._dispatch(msg))
            return

        task = loop.create_task(self._dispatch(msg))
        task.add_done_callback(lambda done: self._task_done(done, msg))

    def _subscribe_response(
        self,
        seq: int,
        handle: ResponseHandle,
        timeout_cb: Callable[[], None],
        timeout_ms: int,
    ) -> None:
        normalized_seq = seq & 0xFFFFFFFF
        self._rsp_handle_map[normalized_seq] = handle

        def on_timeout() -> None:
            if self._rsp_handle_map.pop(normalized_seq, None) is not None:
                timeout_cb()

        if self._timer_impl is not None:
            self._timer_impl(timeout_ms, on_timeout)
            return

        loop = asyncio.get_running_loop()
        loop.call_later(timeout_ms / 1000.0, on_timeout)

    async def _dispatch(self, msg: RpcMessage) -> None:
        if has_type(msg.type, MsgType.COMMAND):
            if has_type(msg.type, MsgType.PING):
                self._send_message(
                    RpcMessage(
                        seq=msg.seq,
                        type=int(
                            MsgType.RESPONSE
                            | MsgType.PONG
                            | (msg.type & int(MsgType.PAYLOAD_JSON))
                        ),
                        cmd=msg.cmd,
                        data=msg.data,
                    )
                )
                return

            handle = self._cmd_handle_map.get(msg.cmd)
            need_rsp = has_type(msg.type, MsgType.NEED_RSP)
            if handle is None:
                if need_rsp:
                    self._send_message(
                        RpcMessage(
                            seq=msg.seq,
                            type=int(MsgType.RESPONSE | MsgType.NO_SUCH_CMD),
                            cmd="",
                            data=b"",
                        )
                    )
                return

            if not need_rsp:
                task = asyncio.create_task(handle(msg))
                task.add_done_callback(lambda done: self._task_done(done, msg))
                return

            data, payload_json = await handle(msg)
            type_value = int(MsgType.RESPONSE)
            if payload_json:
                type_value |= int(MsgType.PAYLOAD_JSON)
            self._send_message(RpcMessage(seq=msg.seq, type=type_value, cmd="", data=data))
            return

        if has_type(msg.type, MsgType.RESPONSE):
            handle = self._rsp_handle_map.pop(msg.seq, None)
            if handle is None:
                return
            if not handle(msg):
                self._emit_error(ValueError("response deserialize error"), msg)
            return

        self._emit_error(ValueError("unknown message type"), msg)

    def _send_message(self, msg: RpcMessage) -> None:
        try:
            result = self._connection.send_package(serialize_message(msg))
        except BaseException as error:
            self._emit_error(error, msg)
            return

        if inspect.isawaitable(result):
            task = asyncio.create_task(result)
            task.add_done_callback(lambda done: self._task_done(done, msg))

    def _task_done(self, task: asyncio.Future[Any], msg: RpcMessage | None) -> None:
        try:
            task.result()
        except asyncio.CancelledError:
            raise
        except BaseException as error:
            self._emit_error(error, msg)

    def _emit_error(self, error: BaseException, msg: RpcMessage | None) -> None:
        if self._error_handler is not None:
            self._error_handler(error, msg)
