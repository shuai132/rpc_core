import { createServer } from "node:net";

import { createNodeTcpRpc } from "../src/node.js";

const port = Number.parseInt(process.env.RPC_PORT ?? "6666", 10);
const host = process.env.RPC_HOST ?? "127.0.0.1";

const server = createServer((socket) => {
  const { rpc } = createNodeTcpRpc(socket);
  console.log(`on_session: ${socket.remoteAddress}:${socket.remotePort}`);

  socket.on("close", () => {
    console.log(`session on_close: ${socket.remoteAddress}:${socket.remotePort}`);
  });

  rpc.onError((error) => {
    console.error("rpc error:", error);
  });

  rpc.subscribe<string, string>("cmd", (data) => {
    console.log(`session on cmd: ${data}`);
    return "world";
  });
});

await new Promise<void>((resolve, reject) => {
  server.once("error", reject);
  server.listen(port, host, () => {
    server.off("error", reject);
    resolve();
  });
});

console.log(`rpc_s listening on ${host}:${port}`);

process.once("SIGINT", () => {
  server.close(() => {
    process.exit(0);
  });
});
