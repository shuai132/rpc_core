import { copyBytes, type Bytes } from "./bytes.js";

export type SendPackageImpl = (pkg: Uint8Array) => void | Promise<void>;
export type RecvPackageImpl = (pkg: Uint8Array) => void;

export interface Connection {
  setSendPackageImpl(handle: SendPackageImpl): void;
  sendPackage(pkg: Bytes): void | Promise<void>;
  setRecvPackageImpl(handle: RecvPackageImpl): void;
  onRecvPackage(pkg: Bytes): void;
}

export class DefaultConnection implements Connection {
  private sendPackageImpl?: SendPackageImpl;
  private recvPackageImpl?: RecvPackageImpl;

  setSendPackageImpl(handle: SendPackageImpl): void {
    this.sendPackageImpl = handle;
  }

  sendPackage(pkg: Bytes): void | Promise<void> {
    if (this.sendPackageImpl === undefined) {
      return;
    }
    return this.sendPackageImpl(copyBytes(pkg));
  }

  setRecvPackageImpl(handle: RecvPackageImpl): void {
    this.recvPackageImpl = handle;
  }

  onRecvPackage(pkg: Bytes): void {
    if (this.recvPackageImpl === undefined) {
      return;
    }
    this.recvPackageImpl(copyBytes(pkg));
  }
}

export class LoopbackConnection {
  static create(): [DefaultConnection, DefaultConnection] {
    const c1 = new DefaultConnection();
    const c2 = new DefaultConnection();
    c1.setSendPackageImpl((pkg) => {
      c2.onRecvPackage(pkg);
    });
    c2.setSendPackageImpl((pkg) => {
      c1.onRecvPackage(pkg);
    });
    return [c1, c2];
  }
}
