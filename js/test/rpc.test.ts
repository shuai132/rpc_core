import assert from "node:assert/strict";
import { createServer, connect } from "node:net";
import { test } from "node:test";

import {
  FinallyType,
  LoopbackConnection,
  MsgType,
  Rpc,
  StreamPackageParser,
  deserializeMessage,
  jsonCodec,
  packStreamPackage,
  serializeMessage,
} from "../src/index.js";
import { createNodeTcpRpc } from "../src/node.js";

test("serializes the rpc_core wire format", () => {
  const wire = serializeMessage({
    seq: 0x01020304,
    type: MsgType.Command | MsgType.NeedRsp,
    cmd: "cmd",
    data: jsonCodec.encode("hello"),
  });

  assert.deepEqual([...wire.slice(0, 10)], [4, 3, 2, 1, 3, 0, 99, 109, 100, 5]);

  const msg = deserializeMessage(wire);
  assert.ok(msg);
  assert.equal(msg.seq, 0x01020304);
  assert.equal(msg.cmd, "cmd");
  assert.equal(msg.type, MsgType.Command | MsgType.NeedRsp);
  assert.equal(jsonCodec.decode(msg.data), "hello");
});

test("supports loopback request and response", async () => {
  const [serverConnection, clientConnection] = LoopbackConnection.create();
  const server = new Rpc(serverConnection);
  const client = new Rpc(clientConnection);
  server.setReady(true);
  client.setReady(true);

  server.subscribe<string, string>("cmd", (msg) => {
    assert.equal(msg, "hello");
    return "world";
  });

  const result = await client.cmd("cmd").msg("hello").promise<string>();
  assert.deepEqual(result, { type: FinallyType.Normal, data: "world" });
});

test("supports async command handlers", async () => {
  const [serverConnection, clientConnection] = LoopbackConnection.create();
  const server = new Rpc(serverConnection);
  const client = new Rpc(clientConnection);
  server.setReady(true);
  client.setReady(true);

  server.subscribe<string, string>("slow", async (msg) => {
    await new Promise((resolve) => setTimeout(resolve, 1));
    return `${msg}!`;
  });

  const result = await client.cmd("slow").msg("hello").promise<string>();
  assert.deepEqual(result, { type: FinallyType.Normal, data: "hello!" });
});

test("supports ping and no-such-command responses", async () => {
  const [serverConnection, clientConnection] = LoopbackConnection.create();
  const server = new Rpc(serverConnection);
  const client = new Rpc(clientConnection);
  server.setReady(true);
  client.setReady(true);

  const pong = await client.ping("hello").promise<string>();
  assert.deepEqual(pong, { type: FinallyType.Normal, data: "hello" });

  const missing = await client.cmd("missing").msg("hello").promise<string>();
  assert.equal(missing.type, FinallyType.NoSuchCmd);
});

test("times out when no response arrives", async () => {
  const client = new Rpc();
  client.setReady(true);

  const result = await client.cmd("missing").msg("hello").timeoutMs(1).promise<string>();
  assert.equal(result.type, FinallyType.Timeout);
});

test("parses stream-packed packages across chunks", () => {
  const source = new Uint8Array([1, 2, 3, 4, 5]);
  const packed = packStreamPackage(source);
  const received: number[][] = [];
  const parser = new StreamPackageParser({
    onPackage(pkg) {
      received.push([...pkg]);
    },
  });

  parser.feed(packed.slice(0, 2));
  parser.feed(packed.slice(2, 6));
  parser.feed(packed.slice(6));

  assert.deepEqual(received, [[1, 2, 3, 4, 5]]);
});

test("supports the Node TCP adapter", async () => {
  const server = createServer((socket) => {
    const { rpc } = createNodeTcpRpc(socket);
    rpc.subscribe<string, string>("cmd", (msg) => `${msg}-node`);
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

    const result = await client.rpc.cmd("cmd").msg("hello").promise<string>();
    assert.deepEqual(result, { type: FinallyType.Normal, data: "hello-node" });
    client.close();
  } finally {
    await new Promise<void>((resolve) => {
      server.close(() => resolve());
    });
  }
});
