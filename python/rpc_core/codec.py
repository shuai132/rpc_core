from __future__ import annotations

import json
from typing import Any, Generic, Protocol, TypeVar

T = TypeVar("T")


class Codec(Protocol[T]):
    def encode(self, value: T) -> bytes:
        ...

    def decode(self, data: bytes | bytearray | memoryview) -> T:
        ...


class JsonCodec(Generic[T]):
    def encode(self, value: T) -> bytes:
        return json.dumps(value, ensure_ascii=False, separators=(",", ":")).encode("utf-8")

    def decode(self, data: bytes | bytearray | memoryview) -> Any:
        payload = bytes(data)
        if not payload:
            return None
        return json.loads(payload.decode("utf-8"))


class StringCodec:
    def encode(self, value: str) -> bytes:
        return value.encode("utf-8")

    def decode(self, data: bytes | bytearray | memoryview) -> str:
        return bytes(data).decode("utf-8")


class BytesCodec:
    def encode(self, value: bytes | bytearray | memoryview) -> bytes:
        return bytes(value)

    def decode(self, data: bytes | bytearray | memoryview) -> bytes:
        return bytes(data)


json_codec: Codec[Any] = JsonCodec()
string_codec = StringCodec()
bytes_codec = BytesCodec()
