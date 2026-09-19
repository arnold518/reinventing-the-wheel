"use strict";
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");

// Exercise the actual browser merge, including repeated lazy scope records.
const source = fs.readFileSync(path.join(__dirname, "../visualizer-v2/static/app.js"), "utf8");
const mergeSource = source.slice(source.indexOf("function mergeTopologyRecords("),
  source.indexOf("function visibleMissingScopeIds("));
function fixture() {
  const app = { pins: new Map(), pinOrder: [], wires: new Map(), wireOrder: [],
    wiresByOwner: new Map(), components: new Map([["ROOT", { inputPins: [], outputPins: [] }]]) };
  const context = vm.createContext({ app });
  vm.runInContext(mergeSource, context);
  return { app, merge: context.mergeTopologyRecords };
}
const input = { id: "ROOT.input.LOAD", componentId: "ROOT", name: "LOAD", type: "input", width: 32, stateIndex: 0 };
const output = { ...input, id: "ROOT.output.LOAD", type: "output", stateIndex: 1 };
const wire = { id: "ROOT.net", name: "net", ownerId: "ROOT", width: 32,
  stateIndex: 0, sourcePinId: output.id, sinkPinIds: [input.id] };
const payload = { pins: [input, output], wires: [wire] };
const { app, merge } = fixture();
merge(payload);
merge(payload);
merge({ pins: [output, input, output], wires: [wire, wire] });
assert.equal(app.pins.size, 2);
assert.equal(app.pinOrder.length, 2);
assert.equal(app.wireOrder.length, 1);
assert.equal(app.components.get("ROOT").inputPins.length, 1);
assert.equal(app.components.get("ROOT").outputPins.length, 1);
assert.equal(app.wires.get(wire.id).sourcePin, app.pins.get(output.id));
assert.equal(app.wires.get(wire.id).sinkPins[0], app.pins.get(input.id));
assert.equal(app.pins.get(output.id).sourceWireIds.length, 1);
assert.equal(app.pins.get(input.id).sinkWireIds.length, 1);
for (const patch of [{ type: "output" }, { width: 8 }, { name: "STORE" },
  { componentId: "OTHER" }, { stateIndex: 3 }]) {
  assert.throws(() => merge({ pins: [{ ...input, ...patch }] }), /Conflicting topology pin ID/);
}
for (const patch of [{ sourcePinId: input.id }, { sinkPinIds: [output.id] },
  { width: 8 }, { ownerId: "OTHER" }, { name: "other" }, { stateIndex: 3 }]) {
  assert.throws(() => merge({ wires: [{ ...wire, ...patch }] }), /Conflicting topology wire ID/);
}
// Within-batch collisions must be rejected too, without a half-applied batch.
for (const invalid of [
  { pins: [input, { ...output, id: input.id }], wires: [] },
  { ...payload, wires: [wire, { ...wire, sourcePinId: input.id }] },
  { ...payload, wires: [{ ...wire, sourcePinId: "missing" }] },
  { ...payload, wires: [{ ...wire, sinkPinIds: ["missing"] }] },
  { pins: [{ ...input, type: "sideways" }] },
  { pins: [{ ...input, componentId: "missing" }] },
]) {
  const fresh = fixture();
  assert.throws(() => fresh.merge(invalid), /topology/);
  assert.equal(fresh.app.pins.size, 0);
  assert.equal(fresh.app.wires.size, 0);
}
// Subsequent scopes may introduce only wires between already-known boundary pins.
merge({ wires: [{ ...wire, id: "ROOT.net2", stateIndex: 1 }] });
assert.equal(app.wires.size, 2);
console.log("Direction-aware pin identity, endpoint binding and atomic lazy-topology validation passed");
