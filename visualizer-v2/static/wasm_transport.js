class WasmResponse {
  constructor(status, body) {
    this.status = status;
    this.ok = status >= 200 && status < 300;
    this.body = typeof body === "string" ? body : JSON.stringify(body);
  }

  async json() {
    return JSON.parse(this.body || "null");
  }

  async text() {
    return this.body;
  }
}

export function createWasmTransport() {
  const workerUrl = new URL("./wasm_worker.js?v=14", import.meta.url);
  const worker = new Worker(workerUrl, { type: "module", name: "circuitsim-wasm" });
  const pending = new Map();
  let nextRequestId = 1;

  worker.addEventListener("message", (event) => {
    const message = event.data || {};
    const request = pending.get(message.id);
    if (!request) return;
    pending.delete(message.id);
    request.resolve(new WasmResponse(message.status || 500, message.body || ""));
  });
  worker.addEventListener("error", (event) => {
    const error = new Error(event.message || "CircuitSim WebAssembly worker failed");
    for (const request of pending.values()) request.reject(error);
    pending.clear();
  });

  return {
    request(url, options = {}) {
      const id = nextRequestId++;
      const parsed = new URL(url, document.baseURI);
      const method = String(options.method || "GET").toUpperCase();
      const body = options.body == null ? "" : String(options.body);
      return new Promise((resolve, reject) => {
        pending.set(id, { resolve, reject });
        worker.postMessage({
          id,
          method,
          path: parsed.pathname,
          query: parsed.search,
          body,
        });
      });
    },
    close() {
      worker.terminate();
    },
  };
}
