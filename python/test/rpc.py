import asyncio
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from rpc_core import FinallyType, LoopbackConnection, Rpc, create_asyncio_tcp_rpc


async def run_loopback() -> None:
    server_connection, client_connection = LoopbackConnection.create()
    server = Rpc(server_connection)
    client = Rpc(client_connection)
    server.set_ready(True)
    client.set_ready(True)

    def on_cmd(data: str) -> str:
        print(f"server on cmd: {data}")
        assert data == "hello"
        return "world"

    server.subscribe("cmd", on_cmd)

    result = await client.cmd("cmd").msg("hello").timeout_ms(1000).promise()
    print(f"loopback cmd rsp: {result.data}")
    assert result.type == FinallyType.NORMAL
    assert result.data == "world"

    pong = await client.ping("hello").timeout_ms(1000).promise()
    print(f"loopback ping rsp: {pong.data}")
    assert pong.type == FinallyType.NORMAL
    assert pong.data == "hello"

    missing = await client.cmd("missing").msg("hello").timeout_ms(1000).promise()
    print(f"loopback missing rsp: {missing.type.value}")
    assert missing.type == FinallyType.NO_SUCH_CMD


async def run_tcp() -> None:
    sessions = []

    async def on_session(reader: asyncio.StreamReader, writer: asyncio.StreamWriter) -> None:
        session = await create_asyncio_tcp_rpc(reader, writer)
        sessions.append(session)
        peer = writer.get_extra_info("peername")
        print(f"server on_session: {peer}")

        def on_cmd(data: str) -> str:
            print(f"server session on cmd: {data}")
            assert data == "hello"
            return "world"

        session.rpc.subscribe("cmd", on_cmd)

    server = await asyncio.start_server(on_session, "127.0.0.1", 0)
    try:
        sockets = server.sockets
        assert sockets
        port = sockets[0].getsockname()[1]

        reader, writer = await asyncio.open_connection("127.0.0.1", port)
        client = await create_asyncio_tcp_rpc(reader, writer, ready=True)
        print("client on_open")

        result = await client.rpc.cmd("cmd").msg("hello").timeout_ms(1000).promise()
        print(f"tcp cmd rsp: {result.data}")
        assert result.type == FinallyType.NORMAL
        assert result.data == "world"

        await client.close()
        print("client on_close")
    finally:
        for session in sessions:
            await session.close()
            print("server session on_close")
        server.close()
        await server.wait_closed()


async def main() -> None:
    await run_loopback()
    await run_tcp()
    print("rpc ok")


if __name__ == "__main__":
    asyncio.run(main())
