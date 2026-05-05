from __future__ import annotations

from typing import Callable


def pack_stream_package(package: bytes | bytearray | memoryview) -> bytes:
    body = bytes(package)
    if len(body) > 0xFFFFFFFF:
        raise ValueError("package length exceeds uint32")
    return len(body).to_bytes(4, "little") + body


class StreamPackageParser:
    def __init__(
        self,
        *,
        max_body_size: int = 0xFFFFFFFF,
        on_package: Callable[[bytes], None] | None = None,
    ) -> None:
        self.max_body_size = max_body_size
        self.on_package = on_package
        self._buffer = b""
        self._body_size: int | None = None

    def reset(self) -> None:
        self._buffer = b""
        self._body_size = None

    def feed(self, chunk: bytes | bytearray | memoryview) -> None:
        data = bytes(chunk)
        if not data:
            return

        self._buffer += data
        while True:
            if self._body_size is None:
                if len(self._buffer) < 4:
                    return
                self._body_size = int.from_bytes(self._buffer[:4], "little")
                if self._body_size > self.max_body_size:
                    size = self._body_size
                    self.reset()
                    raise ValueError(f"body_size > max_body_size: {size} > {self.max_body_size}")
                self._buffer = self._buffer[4:]

            if len(self._buffer) < self._body_size:
                return

            package = self._buffer[: self._body_size]
            self._buffer = self._buffer[self._body_size :]
            self._body_size = None
            if self.on_package is not None:
                self.on_package(package)
