# rpc-core Python

Python SDK for `rpc_core`.

The default payload codec is JSON. The wire header matches `rpc_core` and the JavaScript SDK:
`seq(u32 LE) + cmd_len(u16 LE) + cmd + type(u8) + payload`.

## Usage

```python
from rpc_core import LoopbackConnection, Rpc

server_connection, client_connection = LoopbackConnection.create()
server = Rpc(server_connection)
client = Rpc(client_connection)
server.set_ready(True)
client.set_ready(True)

server.subscribe("cmd", lambda msg: f"{msg}-python")

result = await client.cmd("cmd").msg("hello").promise()
print(result.data)
```

## Test

```shell
python -m unittest discover -s test
python test/rpc.py
python test/rpc_s.py
python test/rpc_c.py
RPC_ONCE=1 python test/rpc_s.py
```
