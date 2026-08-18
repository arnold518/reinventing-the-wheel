import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import { pathToFileURL } from "node:url";
import { performance } from "node:perf_hooks";

if (process.argv.length < 4) {
  throw new Error("Usage: node VisualizerWasmRuntimeTest.mjs <module.js> <module.wasm> [test] [scenario]");
}

const [modulePath, wasmPath] = process.argv.slice(2, 4);
const registeredTest = process.argv[4] || "Adder8Test";
const scenario = process.argv[5] || "adder8";
const wasmBinary = await readFile(wasmPath);
const imported = await import(pathToFileURL(modulePath));

const instantiateStarted = performance.now();
const module = await imported.default({ wasmBinary });
const instantiateMs = performance.now() - instantiateStarted;
const bridge = new module.WasmVisualizer();

try {
  const descriptors = JSON.parse(bridge.visualizableTestsJson());
  assert(descriptors.some((item) => item.name === "Adder8Test" && item.visualizable));

  const buildStarted = performance.now();
  const payload = JSON.parse(bridge.loadScenario(registeredTest, scenario, 0));
  const buildMs = performance.now() - buildStarted;
  assert.equal(payload.scenario, scenario);
  assert.equal(payload.topologyEncoding, "progressive-v1");
  assert(payload.components.length >= 1);
  assert.equal(payload.stats.componentCount, payload.components.length);
  assert.equal(payload.state.encoding, "indexed-v1");

  const state = JSON.parse(bridge.stateJson(payload.timestamps.length - 1));
  assert.equal(state.index, payload.timestamps.length - 1);
  assert.equal(state.pins.length, payload.stats.pinCount);
  assert.equal(state.wires.length, payload.stats.wireCount);

  const topology = JSON.parse(bridge.topologySliceJson([payload.rootId]));
  assert.deepEqual(topology.scopeIds, [payload.rootId]);
  assert(topology.pins.length > 0);

  if (payload.rootType === "Memory64Kx32" || payload.rootType === "RegisterFile32x32") {
    const componentState = JSON.parse(bridge.componentStateJson(0, payload.rootId));
    assert.equal(componentState.supported, true);
    assert.equal(componentState.type, payload.rootType);
  }

  process.stdout.write(`${JSON.stringify({
    scenario,
    instantiateMs,
    buildMs,
    components: payload.stats.componentCount,
    pins: payload.stats.pinCount,
    wires: payload.stats.wireCount,
    timestamps: payload.stats.timestampCount,
  })}\n`);
} finally {
  bridge.delete();
}
