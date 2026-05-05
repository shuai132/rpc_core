import asyncio
import unittest

from rpc_core import (
    FinallyType,
    LoopbackConnection,
    MsgType,
    Rpc,
    RpcMessage,
    StreamPackageParser,
    create_asyncio_tcp_rpc,
    deserialize_message,
    json_codec,
    pack_stream_package,
    serialize_message,
)


class RpcCoreTest(unittest.IsolatedAsyncioTestCase):
    def test_serializes_the_rpc_core_wire_format(self) -> None:
        wire = serialize_message(
            RpcMessage(
                seq=0x01020304,
                type=int(MsgType.COMMAND | MsgType.NEED_RSP),
                cmd="cmd",
                data=json_codec.encode("hello"),
            )
        )

        self.assertEqual(list(wire[:10]), [4, 3, 2, 1, 3, 0, 99, 109, 100, 5])

        msg = deserialize_message(wire)
        self.assertIsNotNone(msg)
        assert msg is not None
        self.assertEqual(msg.seq, 0x01020304)
        self.assertEqual(msg.cmd, "cmd")
        self.assertEqual(msg.type, int(MsgType.COMMAND | MsgType.NEED_RSP))
        self.assertEqual(json_codec.decode(msg.data), "hello")

    async def test_supports_loopback_request_and_response(self) -> None:
        server_connection, client_connection = LoopbackConnection.create()
        server = Rpc(server_connection)
        client = Rpc(client_connection)
        server.set_ready(True)
        client.set_ready(True)

        def on_cmd(msg: str) -> str:
            self.assertEqual(msg, "hello")
            return "world"

        server.subscribe("cmd", on_cmd)

        result = await client.cmd("cmd").msg("hello").promise()
        self.assertEqual(result, type(result)(type=FinallyType.NORMAL, data="world"))

    async def test_supports_async_command_handlers(self) -> None:
        server_connection, client_connection = LoopbackConnection.create()
        server = Rpc(server_connection)
        client = Rpc(client_connection)
        server.set_ready(True)
        client.set_ready(True)

        async def on_slow(msg: str) -> str:
            await asyncio.sleep(0.001)
            return f"{msg}!"

        server.subscribe("slow", on_slow)

        result = await client.cmd("slow").msg("hello").promise()
        self.assertEqual(result.type, FinallyType.NORMAL)
        self.assertEqual(result.data, "hello!")

    async def test_supports_ping_and_no_such_command_responses(self) -> None:
        server_connection, client_connection = LoopbackConnection.create()
        server = Rpc(server_connection)
        client = Rpc(client_connection)
        server.set_ready(True)
        client.set_ready(True)

        pong = await client.ping("hello").promise()
        self.assertEqual(pong.type, FinallyType.NORMAL)
        self.assertEqual(pong.data, "hello")

        missing = await client.cmd("missing").msg("hello").promise()
        self.assertEqual(missing.type, FinallyType.NO_SUCH_CMD)

    async def test_times_out_when_no_response_arrives(self) -> None:
        client = Rpc()
        client.set_ready(True)

        result = await client.cmd("missing").msg("hello").timeout_ms(1).promise()
        self.assertEqual(result.type, FinallyType.TIMEOUT)

    def test_parses_stream_packed_packages_across_chunks(self) -> None:
        source = bytes([1, 2, 3, 4, 5])
        packed = pack_stream_package(source)
        received: list[list[int]] = []
        parser = StreamPackageParser(on_package=lambda package: received.append(list(package)))

        parser.feed(packed[:2])
        parser.feed(packed[2:6])
        parser.feed(packed[6:])

        self.assertEqual(received, [[1, 2, 3, 4, 5]])

    async def test_supports_the_asyncio_tcp_adapter(self) -> None:
        sessions = []

        async def on_client(reader: asyncio.StreamReader, writer: asyncio.StreamWriter) -> None:
            session = await create_asyncio_tcp_rpc(reader, writer)
            sessions.append(session)
            session.rpc.subscribe("cmd", lambda msg: f"{msg}-python")

        server = await asyncio.start_server(on_client, "127.0.0.1", 0)
        try:
            sockets = server.sockets
            self.assertTrue(sockets)
            assert sockets is not None
            port = sockets[0].getsockname()[1]

            reader, writer = await asyncio.open_connection("127.0.0.1", port)
            client = await create_asyncio_tcp_rpc(reader, writer, ready=True)
            result = await client.rpc.cmd("cmd").msg("hello").promise()
            self.assertEqual(result.type, FinallyType.NORMAL)
            self.assertEqual(result.data, "hello-python")
            await client.close()
        finally:
            for session in sessions:
                await session.close()
            server.close()
            await server.wait_closed()


if __name__ == "__main__":
    unittest.main()
