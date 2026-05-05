import assert from "node:assert/strict";
import { connect, createServer } from "node:net";

import { FinallyType, createNodeTcpRpc } from "../src/node.js";

const server = createServer((socket) => {
  const { rpc } = createNodeTcpRpc(socket);
  console.log("server on_session");

  socket.on("close", () => {
    console.log("server session on_close");
  });

  rpc.subscribe<string, string>("cmd", (data) => {
    console.log(`server on cmd: ${data}`);
    assert.equal(data, "hello");
    return "world";
  });
});

await new Promise<void>((resolve, reject) => {
  server.once("error", reject);
  server.listen(0, "127.0.0.1", () => {
    server.off("error", reject);
    resolve();
  });
});

try {
  const address = server.address();
  assert.ok(address && typeof address === "object");

  const socket = connect({ host: "127.0.0.1", port: address.port });
  const client = createNodeTcpRpc(socket, { ready: false });

  await new Promise<void>((resolve, reject) => {
    socket.once("connect", resolve);
    socket.once("error", reject);
  });
  console.log("client on_open");

  const result = await client.rpc.cmd("cmd").msg("hello").timeoutMs(1000).promise<string>();
  console.log(`cmd rsp: ${result.data}`);
  assert.deepEqual(result, { type: FinallyType.Normal, data: "world" });

  client.close();
  await new Promise<void>((resolve) => {
    socket.once("close", resolve);
  });
  console.log("client on_close");
} finally {
  await new Promise<void>((resolve) => {
    server.close(() => resolve());
  });
}

console.log("rpc ok");
