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
  const pinsById = new Map(topology.pins.map(pin => [pin.id, pin]));
  assert.equal(pinsById.size, topology.pins.length, "Pin IDs must be unique across directions");
  for (const pin of topology.pins)
    assert.equal(pin.id, `${pin.componentId}.${pin.type}.${pin.name}`);
  for (const wire of topology.wires) {
    if (wire.sourcePinId != null) assert(pinsById.has(wire.sourcePinId));
    for (const id of wire.sinkPinIds) assert(pinsById.has(id));
  }

  if (payload.rootType === "RV32IMemoryLanes") {
    for (let lane = 0; lane < 4; ++lane) {
      const id = `${payload.rootId}.LANE_${lane}`;
      assert.equal(topology.pins.filter(p => p.componentId === id).length, 4);
      for (const name of ["LOAD", "STORE"]) {
        assert(topology.wires.some(w => w.sourcePinId === `${id}.output.${name}`));
        assert(topology.wires.some(w => w.sinkPinIds.includes(`${id}.input.${name}`)));
      }
    }
    for (const row of payload.checkpoints) {
      const values = JSON.parse(bridge.stateJson(row.index)).pins;
      for (let lane = 0; lane < 4; ++lane) {
        const id = `${payload.rootId}.LANE_${lane}`;
        for (const [name, input, output] of [
          ["LOAD", 0x80ff3412, 0x80ff3412 >>> (8 * lane)],
          ["STORE", 0xdeadbeef, (0xdeadbeef << (8 * lane)) >>> 0],
        ]) for (const [direction, expected] of [["input", input], ["output", output]]) {
          const pin = pinsById.get(`${id}.${direction}.${name}`);
          assert.equal(values[pin.stateIndex], expected.toString(2).padStart(32, "0"));
        }
      }
    }
  }

  if (payload.rootType.startsWith("MemoryContainer<") || payload.rootType === "RegisterFile32x32") {
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
