from __future__ import annotations

import inspect
from typing import Awaitable, Callable, Protocol

SendPackageImpl = Callable[[bytes], None | Awaitable[None]]
RecvPackageImpl = Callable[[bytes], None]


class Connection(Protocol):
    def set_send_package_impl(self, handle: SendPackageImpl) -> None:
        ...

    def send_package(self, package: bytes | bytearray | memoryview) -> None | Awaitable[None]:
        ...

    def set_recv_package_impl(self, handle: RecvPackageImpl) -> None:
        ...

    def on_recv_package(self, package: bytes | bytearray | memoryview) -> None:
        ...


class DefaultConnection:
    def __init__(self) -> None:
        self._send_package_impl: SendPackageImpl | None = None
        self._recv_package_impl: RecvPackageImpl | None = None

    def set_send_package_impl(self, handle: SendPackageImpl) -> None:
        self._send_package_impl = handle

    def send_package(self, package: bytes | bytearray | memoryview) -> None | Awaitable[None]:
        if self._send_package_impl is None:
            return None
        result = self._send_package_impl(bytes(package))
        return result if inspect.isawaitable(result) else None

    def set_recv_package_impl(self, handle: RecvPackageImpl) -> None:
        self._recv_package_impl = handle

    def on_recv_package(self, package: bytes | bytearray | memoryview) -> None:
        if self._recv_package_impl is not None:
            self._recv_package_impl(bytes(package))


class LoopbackConnection:
    @staticmethod
    def create() -> tuple[DefaultConnection, DefaultConnection]:
        c1 = DefaultConnection()
        c2 = DefaultConnection()
        c1.set_send_package_impl(lambda package: c2.on_recv_package(package))
        c2.set_send_package_impl(lambda package: c1.on_recv_package(package))
        return c1, c2
