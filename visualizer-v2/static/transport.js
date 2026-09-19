(function initializeCircuitSimTransport(global) {
  let wasmTransportPromise = null;

  async function wasmTransport() {
    if (!wasmTransportPromise) {
      wasmTransportPromise = import("./wasm_transport.js?v=11")
        .then((module) => module.createWasmTransport());
    }
    return wasmTransportPromise;
  }

  global.visualizerRequest = async function visualizerRequest(url, options) {
    const mode = (global.CIRCUITSIM_RUNTIME || {}).mode || "http";
    if (mode !== "wasm") return fetch(url, options);
    const transport = await wasmTransport();
    return transport.request(url, options);
  };
}(window));
