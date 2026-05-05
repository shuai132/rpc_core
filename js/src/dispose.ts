import type { Request } from "./request.js";

export class Dispose {
  private readonly requests = new Set<Request>();

  add(request: Request): void {
    this.requests.add(request);
  }

  remove(request: Request): void {
    this.requests.delete(request);
  }

  dispose(): void {
    for (const request of this.requests) {
      request.cancel();
    }
    this.requests.clear();
  }
}
