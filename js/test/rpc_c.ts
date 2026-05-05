import { connectNodeTcpRpc, FinallyType } from "../src/node.js";

const port = Number.parseInt(process.env.RPC_PORT ?? "6666", 10);
const host = process.env.RPC_HOST ?? "127.0.0.1";

const client = connectNodeTcpRpc(port, host);

client.socket.on("close", () => {
  console.log("client on_close");
});

client.rpc.onError((error) => {
  console.error("rpc error:", error);
});

await new Promise<void>((resolve, reject) => {
  client.socket.once("connect", resolve);
  client.socket.once("error", reject);
});

console.log("client on_open");

const result = await client.rpc.cmd("cmd").msg("hello").timeoutMs(1000).promise<string>();
if (result.type !== FinallyType.Normal) {
  throw new Error(`cmd failed: ${result.type}`);
}
console.log(`cmd rsp: ${result.data}`);

client.close();
