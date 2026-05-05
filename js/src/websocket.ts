import { textEncoder, toUint8Array, type Bytes } from "./bytes.js";
import { DefaultConnection } from "./connection.js";

export interface WebSocketLike {
  binaryType?: BinaryType;
  send(data: string | ArrayBufferLike | Blob | ArrayBufferView): void;
  addEventListener?: (type: "message", listener: (event: { data: unknown }) => void) => void;
  onmessage?: ((event: { data: unknown }) => void) | null;
}

export function createWebSocketConnection(socket: WebSocketLike): DefaultConnection {
  const connection = new DefaultConnection();

  if (socket.binaryType !== undefined) {
    socket.binaryType = "arraybuffer";
  }

  connection.setSendPackageImpl((pkg) => {
    socket.send(pkg);
  });

  const onMessage = (event: { data: unknown }) => {
    void webSocketDataToBytes(event.data).then((pkg) => {
      connection.onRecvPackage(pkg);
    });
  };

  if (socket.addEventListener !== undefined) {
    socket.addEventListener("message", onMessage);
  } else {
    socket.onmessage = onMessage;
  }

  return connection;
}

export async function webSocketDataToBytes(data: unknown): Promise<Uint8Array> {
  if (typeof data === "string") {
    return textEncoder.encode(data);
  }
  if (data instanceof ArrayBuffer || ArrayBuffer.isView(data)) {
    return toUint8Array(data as Bytes).slice();
  }
  if (typeof Blob !== "undefined" && data instanceof Blob) {
    return new Uint8Array(await data.arrayBuffer());
  }
  throw new TypeError("unsupported websocket message data");
}
