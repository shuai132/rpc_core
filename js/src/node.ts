import { connect, type Socket } from "node:net";

import { DefaultConnection, type Connection } from "./connection.js";
import { packStreamPackage, StreamPackageParser, type StreamPackageParserOptions } from "./stream.js";
import { Rpc } from "./rpc.js";

export * from "./index.js";

export interface NodeTcpConnectionOptions {
  maxBodySize?: number;
}

export interface NodeTcpRpcOptions extends NodeTcpConnectionOptions {
  rpc?: Rpc;
  ready?: boolean;
}

export interface NodeTcpRpc {
  socket: Socket;
  connection: Connection;
  rpc: Rpc;
  close(): void;
}

export function bindNodeTcpConnection(socket: Socket, connection: Connection, options: NodeTcpConnectionOptions = {}): Connection {
  const parserOptions: StreamPackageParserOptions = {
    onPackage: (pkg) => {
      connection.onRecvPackage(pkg);
    },
  };
  if (options.maxBodySize !== undefined) {
    parserOptions.maxBodySize = options.maxBodySize;
  }
  const parser = new StreamPackageParser(parserOptions);

  connection.setSendPackageImpl((pkg) => {
    socket.write(packStreamPackage(pkg));
  });

  socket.on("data", (chunk: Uint8Array) => {
    parser.feed(chunk);
  });

  return connection;
}

export function createNodeTcpConnection(socket: Socket, options: NodeTcpConnectionOptions = {}): DefaultConnection {
  const connection = new DefaultConnection();
  bindNodeTcpConnection(socket, connection, options);
  return connection;
}

export function createNodeTcpRpc(socket: Socket, options: NodeTcpRpcOptions = {}): NodeTcpRpc {
  const rpc = options.rpc ?? new Rpc();
  const connection = bindNodeTcpConnection(socket, rpc.getConnection(), options);

  rpc.setReady(options.ready ?? socket.readyState === "open");
  socket.on("connect", () => {
    rpc.setReady(true);
  });
  socket.on("close", () => {
    rpc.setReady(false);
  });
  socket.on("error", () => {
    rpc.setReady(false);
  });

  return {
    socket,
    connection,
    rpc,
    close() {
      socket.end();
    },
  };
}

export function connectNodeTcpRpc(port: number, host = "127.0.0.1", options: NodeTcpRpcOptions = {}): NodeTcpRpc {
  const socket = connect({ port, host });
  return createNodeTcpRpc(socket, { ...options, ready: false });
}
