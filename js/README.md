# @rpc-core/js

JavaScript and TypeScript SDK for `rpc_core`.

The default payload codec is JSON, which is compatible with the Rust crate and with C++ builds that use the JSON serializer path. The wire header matches `rpc_core`: `seq(u32 LE) + cmd_len(u16 LE) + cmd + type(u8) + payload`. The protocol marks JSON payloads with `type` bit 6 (`0x40`); unmarked JSON packets are still accepted.

## Install

```shell
npm install
npm run build
```

## Browser or WebSocket usage

```ts
import { Rpc, createWebSocketConnection } from "@rpc-core/js";

const socket = new WebSocket("ws://localhost:6666");
const rpc = new Rpc(createWebSocketConnection(socket));

socket.addEventListener("open", () => {
  rpc.setReady(true);
});
socket.addEventListener("close", () => {
  rpc.setReady(false);
});

const result = await rpc.cmd("cmd").msg("hello").promise<string>();
console.log(result.data);
```

## Node TCP usage

```ts
import { connectNodeTcpRpc } from "@rpc-core/js/node";

const client = connectNodeTcpRpc(6666, "127.0.0.1");

const result = await client.rpc.cmd("cmd").msg("hello").promise<string>();
console.log(result.data);
```

## Commands

```shell
npm run build
npm test
```
