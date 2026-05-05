import asyncio
import contextlib
import os
import signal
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from rpc_core import create_asyncio_tcp_rpc


async def main() -> None:
    port = int(os.environ.get("RPC_PORT", "6666"))
    host = os.environ.get("RPC_HOST", "127.0.0.1")
    run_once = os.environ.get("RPC_ONCE") == "1"
    sessions = []

    async def on_session(reader: asyncio.StreamReader, writer: asyncio.StreamWriter) -> None:
        session = await create_asyncio_tcp_rpc(reader, writer)
        sessions.append(session)
        peer = writer.get_extra_info("peername")
        print(f"on_session: {peer}")

        def on_cmd(data: str) -> str:
            print(f"session on cmd: {data}")
            return "world"

        session.rpc.on_error(lambda error, _msg: print(f"rpc error: {error}", file=sys.stderr))
        session.rpc.subscribe("cmd", on_cmd)

        def on_close(_task: asyncio.Task[None]) -> None:
            print(f"session on_close: {peer}")
            if run_once:
                stop.set()

        session.task.add_done_callback(on_close)

    server = await asyncio.start_server(on_session, host, port)
    stop = asyncio.Event()
    loop = asyncio.get_running_loop()
    for sig in (signal.SIGINT, signal.SIGTERM):
        try:
            loop.add_signal_handler(sig, stop.set)
        except NotImplementedError:
            pass

    print(f"rpc_s listening on {host}:{port}")
    try:
        await stop.wait()
    finally:
        server.close()
        with contextlib.suppress(asyncio.TimeoutError):
            await asyncio.wait_for(server.wait_closed(), timeout=1.0)
        for session in list(sessions):
            with contextlib.suppress(asyncio.TimeoutError):
                await asyncio.wait_for(session.close(), timeout=1.0)


if __name__ == "__main__":
    asyncio.run(main())
