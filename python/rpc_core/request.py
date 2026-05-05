from __future__ import annotations

import asyncio
import inspect
from dataclasses import dataclass
from enum import Enum
from typing import TYPE_CHECKING, Any, Callable, Generic, TypeVar

from .codec import Codec, json_codec
from .message import MsgType, RpcMessage, has_type

if TYPE_CHECKING:
    from .dispose import Dispose
    from .rpc import Rpc

T = TypeVar("T")
_MISSING = object()


class FinallyType(Enum):
    NORMAL = "normal"
    NO_NEED_RSP = "no_need_rsp"
    TIMEOUT = "timeout"
    CANCELED = "canceled"
    RPC_EXPIRED = "rpc_expired"
    RPC_NOT_READY = "rpc_not_ready"
    RSP_SERIALIZE_ERROR = "rsp_serialize_error"
    NO_SUCH_CMD = "no_such_cmd"


@dataclass
class RequestResult(Generic[T]):
    type: FinallyType
    data: T | None = None
    error: BaseException | None = None


ResponseCallback = Callable[..., None]
FinallyCallback = Callable[[FinallyType], None]


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


def _call_response_callback(cb: ResponseCallback, value: Any, type_value: FinallyType) -> None:
    count = _callback_positional_count(cb)
    if count == 0:
        cb()
    elif count == 1:
        cb(value)
    else:
        cb(value, type_value)


class Request:
    def __init__(self, rpc: Rpc | None = None) -> None:
        self._rpc = rpc
        self._seq = 0
        self._cmd = ""
        self._payload: bytes | None = None
        self._payload_json = False
        self._need_rsp = False
        self._canceled = False
        self._rsp_handle: Callable[[RpcMessage], bool] | None = None
        self._timeout_ms = 3000
        self._timeout_cb: Callable[[], None] = lambda: None
        self._finally_type = FinallyType.NO_NEED_RSP
        self._finally_cb: FinallyCallback | None = None
        self._retry_count = 0
        self._waiting_rsp = False
        self._is_ping = False
        self._response_error: BaseException | None = None
        self.timeout(lambda: None)

    @staticmethod
    def create(rpc: Rpc | None = None) -> "Request":
        return Request(rpc)

    def cmd(self, cmd: str) -> "Request":
        self._cmd = cmd
        return self

    def msg(self, message: Any, codec: Codec[Any] | None = None) -> "Request":
        selected_codec = codec or json_codec
        self._payload = selected_codec.encode(message)
        self._payload_json = codec is None or codec is json_codec
        return self

    def raw(self, data: bytes | bytearray | memoryview) -> "Request":
        self._payload = bytes(data)
        self._payload_json = False
        return self

    def rsp(self, cb: ResponseCallback, codec: Codec[Any] | None = None) -> "Request":
        selected_codec = codec or json_codec
        self._need_rsp = True

        def handle(msg: RpcMessage) -> bool:
            if self._canceled:
                self._on_finish(FinallyType.CANCELED)
                return True

            if has_type(msg.type, MsgType.NO_SUCH_CMD):
                self._on_finish(FinallyType.NO_SUCH_CMD)
                return True

            try:
                rsp = selected_codec.decode(msg.data)
            except BaseException as error:
                self._response_error = error
                self._on_finish(FinallyType.RSP_SERIALIZE_ERROR)
                return False

            _call_response_callback(cb, rsp, FinallyType.NORMAL)
            self._on_finish(FinallyType.NORMAL)
            return True

        self._rsp_handle = handle
        return self

    def raw_rsp(self, cb: ResponseCallback) -> "Request":
        self._need_rsp = True

        def handle(msg: RpcMessage) -> bool:
            if self._canceled:
                self._on_finish(FinallyType.CANCELED)
            elif has_type(msg.type, MsgType.NO_SUCH_CMD):
                self._on_finish(FinallyType.NO_SUCH_CMD)
            else:
                _call_response_callback(cb, bytes(msg.data), FinallyType.NORMAL)
                self._on_finish(FinallyType.NORMAL)
            return True

        self._rsp_handle = handle
        return self

    async def promise(self, codec: Codec[Any] | None = None, rpc: Rpc | None = None) -> RequestResult[Any]:
        loop = asyncio.get_running_loop()
        future: asyncio.Future[RequestResult[Any]] = loop.create_future()
        previous_finally = self._finally_cb
        resolved = False

        def resolve_once(result: RequestResult[Any]) -> None:
            nonlocal resolved
            if not resolved:
                resolved = True
                future.set_result(result)

        def on_rsp(rsp: Any) -> None:
            resolve_once(RequestResult(type=FinallyType.NORMAL, data=rsp))

        def on_finally(type_value: FinallyType) -> None:
            if previous_finally is not None:
                previous_finally(type_value)
            if not resolved:
                resolve_once(RequestResult(type=type_value, error=self._response_error))

        self.rsp(on_rsp, codec)
        self.finally_(on_finally)
        self.call(rpc)
        return await future

    async def future(self, codec: Codec[Any] | None = None, rpc: Rpc | None = None) -> RequestResult[Any]:
        return await self.promise(codec, rpc)

    def finally_(self, cb: FinallyCallback) -> "Request":
        self._finally_cb = cb
        return self

    def call(self, rpc: Rpc | None = None) -> "Request":
        if rpc is not None:
            self._rpc = rpc

        self._waiting_rsp = True
        if self._canceled:
            self._on_finish(FinallyType.CANCELED)
            return self

        if self._rpc is None:
            self._on_finish(FinallyType.RPC_EXPIRED)
            return self

        if not self._rpc.is_ready():
            self._on_finish(FinallyType.RPC_NOT_READY)
            return self

        self._seq = self._rpc.make_seq()
        self._rpc.send_request(self)
        if not self._need_rsp:
            self._on_finish(FinallyType.NO_NEED_RSP)
        return self

    def ping(self) -> "Request":
        self._is_ping = True
        return self

    def timeout_ms(self, timeout_ms: int) -> "Request":
        self._timeout_ms = max(0, int(timeout_ms)) & 0xFFFFFFFF
        return self

    def timeout(self, cb: Callable[[], None]) -> "Request":
        def handle_timeout() -> None:
            cb()
            if self._retry_count == -1:
                self.call()
            elif self._retry_count > 0:
                self._retry_count -= 1
                self.call()
            else:
                self._on_finish(FinallyType.TIMEOUT)

        self._timeout_cb = handle_timeout
        return self

    def add_to(self, dispose: Dispose) -> "Request":
        dispose.add(self)
        return self

    def cancel(self) -> "Request":
        self.canceled(True)
        if self._need_rsp and self._waiting_rsp and self._rpc is not None:
            self._rpc.unsubscribe_response(self._seq)
        self._on_finish(FinallyType.CANCELED)
        return self

    def reset_cancel(self) -> "Request":
        return self.canceled(False)

    def retry(self, count: int) -> "Request":
        self._retry_count = int(count)
        return self

    def disable_rsp(self) -> "Request":
        self._need_rsp = False
        return self

    def enable_rsp(self) -> "Request":
        self._need_rsp = True
        return self

    def mark_need_rsp(self) -> "Request":
        self._need_rsp = True

        def handle(msg: RpcMessage) -> bool:
            if self._canceled:
                self._on_finish(FinallyType.CANCELED)
            elif has_type(msg.type, MsgType.NO_SUCH_CMD):
                self._on_finish(FinallyType.NO_SUCH_CMD)
            else:
                self._on_finish(FinallyType.NORMAL)
            return True

        self._rsp_handle = handle
        return self

    def rpc(self, rpc: Rpc) -> "Request":
        self._rpc = rpc
        return self

    def get_rpc(self) -> Rpc | None:
        return self._rpc

    def is_canceled(self) -> bool:
        return self._canceled

    def canceled(self, canceled: bool) -> "Request":
        self._canceled = canceled
        return self

    @property
    def finally_type(self) -> FinallyType:
        return self._finally_type

    def _handle_response(self, msg: RpcMessage) -> bool:
        if self._rsp_handle is None:
            return True
        return self._rsp_handle(msg)

    def _on_timeout(self) -> None:
        self._timeout_cb()

    def _on_finish(self, type_value: FinallyType) -> None:
        if not self._waiting_rsp:
            return
        self._waiting_rsp = False
        self._finally_type = type_value
        if self._finally_cb is not None:
            self._finally_cb(type_value)
