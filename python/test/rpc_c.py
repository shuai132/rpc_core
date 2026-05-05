import asyncio
import os
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from rpc_core import FinallyType, open_asyncio_tcp_rpc


async def main() -> None:
    port = int(os.environ.get("RPC_PORT", "6666"))
    host = os.environ.get("RPC_HOST", "127.0.0.1")

    client = await open_asyncio_tcp_rpc(host, port)
    client.rpc.on_error(lambda error, _msg: print(f"rpc error: {error}", file=sys.stderr))
    print("client on_open")

    try:
        result = await client.rpc.cmd("cmd").msg("hello").timeout_ms(1000).promise()
        if result.type != FinallyType.NORMAL:
            raise RuntimeError(f"cmd failed: {result.type.value}")
        print(f"cmd rsp: {result.data}")
    finally:
        await client.close()
        print("client on_close")


if __name__ == "__main__":
    asyncio.run(main())
