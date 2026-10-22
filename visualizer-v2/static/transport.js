(function initializeCircuitSimTransport(global) {
  let wasmTransportPromise = null;
  let recordedTransportPromise = null;

  async function wasmTransport() {
    if (!wasmTransportPromise) {
      wasmTransportPromise = import("./wasm_transport.js?v=14")
        .then((module) => module.createWasmTransport());
    }
    return wasmTransportPromise;
  }

  global.visualizerRequest = async function visualizerRequest(url, options) {
    const recording = new URLSearchParams(global.location.search).get("recording");
    if (recording) {
      if (!recordedTransportPromise) {
        recordedTransportPromise = import("./recorded_circuit_transport.mjs")
          .then((module) => module.createRecordedCircuitTransport(recording));
      }
      return (await recordedTransportPromise).request(url, options);
    }
    const mode = (global.CIRCUITSIM_RUNTIME || {}).mode || "http";
    if (mode !== "wasm") return fetch(url, options);
    const transport = await wasmTransport();
    return transport.request(url, options);
  };
}(window));
