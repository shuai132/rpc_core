from __future__ import annotations

import asyncio
import contextlib
from dataclasses import dataclass

from .connection import Connection, DefaultConnection
from .rpc import Rpc
from .stream import StreamPackageParser, pack_stream_package


@dataclass
class AsyncioTcpRpc:
    reader: asyncio.StreamReader
    writer: asyncio.StreamWriter
    connection: Connection
    rpc: Rpc
    task: asyncio.Task[None]

    async def close(self) -> None:
        self.rpc.set_ready(False)
        if not self.task.done():
            self.task.cancel()
            with contextlib.suppress(asyncio.CancelledError):
                await self.task
        if not self.writer.is_closing():
            self.writer.close()
        with contextlib.suppress(ConnectionError, RuntimeError):
            await asyncio.wait_for(self.writer.wait_closed(), timeout=1.0)


async def bind_asyncio_tcp_connection(
    reader: asyncio.StreamReader,
    writer: asyncio.StreamWriter,
    connection: Connection,
    *,
    max_body_size: int = 0xFFFFFFFF,
) -> asyncio.Task[None]:
    parser = StreamPackageParser(max_body_size=max_body_size, on_package=connection.on_recv_package)

    async def send_package(package: bytes) -> None:
        writer.write(pack_stream_package(package))
        await writer.drain()

    async def read_loop() -> None:
        try:
            while True:
                chunk = await reader.read(65536)
                if not chunk:
                    break
                parser.feed(chunk)
        finally:
            parser.reset()

    connection.set_send_package_impl(send_package)
    return asyncio.create_task(read_loop())


async def create_asyncio_tcp_rpc(
    reader: asyncio.StreamReader,
    writer: asyncio.StreamWriter,
    *,
    rpc: Rpc | None = None,
    ready: bool = True,
    max_body_size: int = 0xFFFFFFFF,
) -> AsyncioTcpRpc:
    selected_rpc = rpc or Rpc()
    connection = selected_rpc.get_connection()
    task = await bind_asyncio_tcp_connection(reader, writer, connection, max_body_size=max_body_size)
    selected_rpc.set_ready(ready)
    task.add_done_callback(lambda _done: selected_rpc.set_ready(False))
    return AsyncioTcpRpc(reader=reader, writer=writer, connection=connection, rpc=selected_rpc, task=task)


async def open_asyncio_tcp_rpc(
    host: str = "127.0.0.1",
    port: int = 6666,
    *,
    rpc: Rpc | None = None,
    max_body_size: int = 0xFFFFFFFF,
) -> AsyncioTcpRpc:
    reader, writer = await asyncio.open_connection(host, port)
    return await create_asyncio_tcp_rpc(reader, writer, rpc=rpc, ready=True, max_body_size=max_body_size)
