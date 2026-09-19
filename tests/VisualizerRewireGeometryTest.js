"use strict";

const assert = require("node:assert/strict");
const path = require("node:path");
const fs = require("node:fs");
const vm = require("node:vm");

const geometry = require(path.join(
  __dirname,
  "..",
  "visualizer-v2",
  "static",
  "rewire_geometry.js",
));

function mappings(source, width, offset, destination = "CONTROL") {
  return Array.from({ length: width }, (_, bit) => ({
    srcWire: source,
    srcBit: bit,
    dstWire: destination,
    dstBit: offset + bit,
  }));
}

function main() {
  const controlPack = {
    outputs: [{ name: "CONTROL", width: 32 }],
    mappings: [
      ...mappings("LEGAL", 1, 0),
      ...mappings("WRITEBACK_SEL", 2, 9),
      ...mappings("ALU_OP", 5, 22),
      ...mappings("ALU_A_SEL", 2, 27),
      ...mappings("ALU_B_SEL", 2, 29),
    ],
    unmappedDefault: "LOW",
  };
  const model = geometry.packedBusModel(controlPack);
  assert.ok(model);
  assert.equal(model.destination, "CONTROL");
  assert.equal(model.width, 32);
  assert.equal(model.groups.find((group) => group.srcWire === "LEGAL").range.label, "[0]");
  assert.equal(
    model.groups.find((group) => group.srcWire === "WRITEBACK_SEL").range.label,
    "[10:9]",
  );
  assert.equal(model.groups.find((group) => group.srcWire === "ALU_B_SEL").range.label, "[30:29]");
  assert.ok(model.unmappedBits.includes(31));
  assert.equal(model.unmappedDefault, "LOW");
  assert.equal(geometry.unpackedBusModel(controlPack), null);

  assert.equal(geometry.packedBusModel({
    outputs: [{ name: "LEFT", width: 8 }, { name: "RIGHT", width: 8 }],
    mappings: [
      ...mappings("A", 2, 0, "LEFT"),
      ...mappings("B", 2, 2, "RIGHT"),
      ...mappings("C", 2, 4, "LEFT"),
    ],
  }), null);
  assert.equal(geometry.packedBusModel({
    outputs: [{ name: "OUT", width: 8 }],
    mappings: [...mappings("A", 4, 0, "OUT"), ...mappings("B", 4, 4, "OUT")],
  }), null, "two-source rewires retain the detailed point-to-point renderer");
}

main();

function unpacker(width) {
  return {
    inputs: [{ name: "WORD", width }],
    outputs: Array.from({ length: width }, (_, bit) => ({ name: `BIT_${bit}`, width: 1 })),
    mappings: Array.from({ length: width }, (_, bit) => ({
      srcWire: "WORD", srcBit: bit, dstWire: `BIT_${bit}`, dstBit: 0,
    })),
  };
}
for (const width of [8, 16, 32]) {
  const model = geometry.unpackedBusModel(unpacker(width));
  assert.equal(model.source, "WORD");
  assert.equal(model.width, width);
  assert.deepEqual(model.taps.map(tap => tap.label), Array.from({ length: width }, (_, bit) => `[${bit}]`));
}
const reversed = unpacker(8);
reversed.mappings.forEach(mapping => { mapping.srcBit = 7 - mapping.srcBit; });
assert.equal(geometry.unpackedBusModel(reversed).taps[0].label, "[7]", "Label source bits, not output positions");
const fanout = unpacker(8);
fanout.mappings[1].srcBit = 0;
assert.equal(geometry.unpackedBusModel(fanout).taps[1].label, "[0]", "Repeated bit taps are valid fanout");
for (const mutate of [
  r => r.inputs.push({ name: "OTHER", width: 8 }),
  r => { r.inputs[0].width = 1; },
  r => { r.outputs[0].width = 2; },
  r => r.mappings.pop(),
  r => { r.mappings[0].srcWire = "MISSING"; },
  r => { r.mappings[0].srcBit = 8; },
  r => { r.mappings[0].srcBit = -1; },
  r => { r.mappings[0].dstBit = 1; },
  r => { r.mappings[0].dstWire = "MISSING"; },
  r => { r.mappings[0].dstWire = "BIT_1"; },
]) {
  const invalid = unpacker(8);
  mutate(invalid);
  assert.equal(geometry.unpackedBusModel(invalid), null, "Do not misrepresent a general mapping as an unpacker");
}
assert.equal(geometry.unpackedBusModel(null), null);

const fields = {
  inputs: [{ name: "WORD", width: 8 }],
  outputs: [{ name: "SLICE", width: 3 }, { name: "SIGN", width: 8 },
    { name: "MIX", width: 8 }, { name: "CONSTANT", width: 4 }],
  mappings: [
    ...[3, 4, 5].map((bit, i) => ({ srcWire: "WORD", srcBit: bit, dstWire: "SLICE", dstBit: i })),
    ...[0, 1, 2, 3, 3, 3, 3, 3].map((bit, i) => ({ srcWire: "WORD", srcBit: bit, dstWire: "SIGN", dstBit: i })),
    ...[7, 0, 5, 5].map((bit, i) => ({ srcWire: "WORD", srcBit: bit, dstWire: "MIX", dstBit: i + 1 })),
  ],
  unmappedDefault: "LOW",
};
const fieldModel = geometry.fieldExtractionModel(fields);
assert.deepEqual(fieldModel.taps.map(t => t.label), ["[5:3]", "{4×[3], [3:0]}", "{3×0, 2×[5], [0], [7], 0}", "4×0"]);
for (const fill of ["LOW", "HIGH", "UNKNOWN"]) {
  const model = geometry.fieldExtractionModel({ ...fields, unmappedDefault: fill });
  for (const word of ["10100110", "XZ1010Z1", "0000X001", "0000Z001", "00000000", "11111111"]) {
    for (const tap of model.taps) {
      const expected = Array(tap.width).fill({ LOW: "0", HIGH: "1", UNKNOWN: "X" }[fill]);
      for (const map of fields.mappings.filter(m => m.dstWire === tap.destination))
        expected[tap.width - 1 - map.dstBit] = word[7 - map.srcBit];
      assert.equal(geometry.fieldValue(tap, word, model.unmappedDefault), expected.join(""));
    }
  }
}
assert.equal(geometry.fieldExtractionModel(null), null);
assert.equal(geometry.fieldExtractionModel(unpacker(8)), null, "Keep the one-bit unpacker");
const reversedFields = geometry.fieldExtractionModel({
  inputs: [{name:"A",width:32}], outputs: [{name:"COPY",width:32},{name:"REVERSE",width:32}],
  mappings: Array.from({length:32},(_,i)=>[
    {srcWire:"A",srcBit:i,dstWire:"COPY",dstBit:i},
    {srcWire:"A",srcBit:31-i,dstWire:"REVERSE",dstBit:i},
  ]).flat(),
});
assert.equal(reversedFields.taps[1].label, "[0:31]");
const fourStateWord = "10XZ".repeat(8);
assert.equal(geometry.fieldValue(reversedFields.taps[0], fourStateWord), fourStateWord);
assert.equal(geometry.fieldValue(reversedFields.taps[1], fourStateWord), [...fourStateWord].reverse().join(""));
for (const mutate of [
  r => r.inputs.push({ name: "OTHER", width: 8 }),
  r => r.outputs.push({ ...r.outputs[0] }),
  r => { r.outputs[0].width = 0; },
  r => { r.mappings[0].srcWire = "MISSING"; },
  r => { r.mappings[0].srcBit = 8; },
  r => { r.mappings[0].srcBit = -1; },
  r => { r.mappings[0].dstBit = 3; },
  r => { r.mappings[0].dstBit = 0.5; },
  r => { r.mappings[0].dstWire = "MISSING"; },
  r => r.mappings.push({ ...r.mappings[0] }),
]) {
  const invalid = structuredClone(fields); mutate(invalid);
  assert.equal(geometry.fieldExtractionModel(invalid), null);
}

const appSource = fs.readFileSync(path.join(__dirname, "../visualizer-v2/static/app.js"), "utf8");
function extract(source, name) {
  const start = source.indexOf(`function ${name}(`);
  const end = source.indexOf("\n}\n", start);
  assert(start >= 0 && end > start, `Missing function ${name}`);
  return source.slice(start, end + 3);
}
const lines = [], labels = [];
const context = vm.createContext({
  window: { CircuitRewireGeometry: geometry }, app: { camera: { zoom: 2 } },
  pinByName: (pins, name) => pins.find(pin => pin.name === name),
  pinMapByName: pins => new Map(pins.map(pin => [pin.name, pin])),
  pinInnerContactPoint: pin => pin.pos,
  adapterPinSignal: pin => pin.value,
  withComponentBodyClip: (parts, w, h, draw) => draw(),
  drawAdapterPanel() {},
  drawAdapterPolyline: (c, points, value, pins, width, annotate = true) => lines.push({ points, value, width, annotate }),
  drawAdapterJunction() { assert.fail("Unpacking must not draw bit-joining dots"); },
  worldToScreen: point => point, ellipsizeText: text => text,
  ctx: { save() {}, restore() {}, fillText(text) { labels.push(text); }, measureText(text) { return { width: text.length * 6 }; } },
});
for (const name of ["signalBitAt", "drawUnpackedRewireContents", "drawRewireContents"])
  vm.runInContext(extract(appSource, name), context);
for (const width of [8, 32]) {
  lines.length = labels.length = 0;
  const rewire = unpacker(width);
  const component = { rewire,
    inputPins: [{ name: "WORD", pos: { x: 0, y: width * 10 }, value: "1" + "0".repeat(width - 2) + "1" }],
    outputPins: rewire.outputs.map((pin, i) => ({ ...pin, pos: { x: 400, y: 20 + i * 20 } })),
  };
  context.drawRewireContents(component, { bodyRect: { x: 0, y: 0, w: 400, h: width * 20 + 40 } });
  assert.equal(lines.length, width + 2, "One bus input, one spine, one independent tap per output");
  assert.equal(lines[0].width, width);
  assert.equal(lines[1].width, width);
  const taps = lines.slice(2);
  assert.equal(new Set(taps.map(line => line.points[0].y)).size, width, "No reused vertical bit routes");
  assert(taps.every(line => line.width === 1 && line.points[0].y < line.points[1].y));
  assert.equal(taps[0].value, "1");
  assert.equal(taps[1].value, "0");
  assert.equal(taps[width - 1].value, "1");
  assert.deepEqual(labels, rewire.outputs.map((_, bit) => `[${bit}]`));
}

for (const zoom of [1, 2, 8]) {
  lines.length = labels.length = 0;
  context.app.camera.zoom = zoom;
  context.drawRewireContents({ rewire: fields,
    inputPins: [{ name: "WORD", pos: { x: 0, y: 225 }, value: "XZ1010Z1" }],
    outputPins: fields.outputs.map((pin, i) => ({ ...pin, pos: { x: 400, y: 90 + i * 90 } })),
  }, { bodyRect: { x: 0, y: 0, w: 400, h: 450 } });
  assert.equal(lines.length, 6, "One input, one spine, four fields (including constants)");
  const verticals = lines.flatMap(line => line.points.slice(1).map((point, i) => [line.points[i], point]))
    .filter(([a, b]) => a.x === b.x && a.y !== b.y);
  assert.equal(verticals.length, 1, "Do not redraw a vertical route per field");
  assert.deepEqual(lines.map(line => line.width), [8, 8, 3, 8, 8, 4]);
  assert.equal(lines.filter(line => line.annotate).length, 1, "Bus slashes belong only to the source lead");
  assert.equal(lines.at(-1).points.length, 2, "A constant field is not connected to the source spine");
  assert.deepEqual(lines.slice(2).map(line => line.value), fieldModel.taps.map(tap => geometry.fieldValue(tap, "XZ1010Z1", "LOW")));
  assert.deepEqual(labels.filter(label => !label.includes("[")), ["4×0"]);
  for (const tap of fieldModel.taps) assert(labels.includes(`${tap.destination}[${tap.width}]`));
}

// The multi-source fallback must not cycle eight routes through three lanes.
for (const name of ["groupedRewireMappings", "rewireRouteValue"])
  vm.runInContext(extract(appSource, name), context);
context.drawAdapterJunction = () => {};
lines.length = 0;
context.drawRewireContents({
  rewire: { inputs: [{name:"A",width:8},{name:"B",width:8}],
    outputs: fields.outputs,
    mappings: fields.outputs.flatMap(o => ["A","B"].map((srcWire,i) => ({srcWire,srcBit:0,dstWire:o.name,dstBit:i}))) },
  inputPins: ["A","B"].map((name,i) => ({name,width:8,pos:{x:0,y:100+i*30},value:"00000001"})),
  outputPins: fields.outputs.map((p,i) => ({...p,pos:{x:400,y:60+i*70},value:"00"})),
}, {bodyRect:{x:0,y:0,w:400,h:400}});
const routes = lines.filter(line => line.points.length === 4);
assert.equal(routes.length, 8);
assert.equal(new Set(routes.map(line => line.points[1].x)).size, 8, "Give each general route a separate lane");

// Both rendering paths must ignore legacy shared proportions but retain colors
// and explicit overrides for the new variant. Never mutate saved layout data.
const workerSource = fs.readFileSync(path.join(__dirname, "../visualizer-v2/static/wasm_worker.js"), "utf8");
for (const source of [appSource, workerSource]) {
  const layout = { type_layouts: { Rewire: { aspect_ratio: 9, min_aspect_ratio: 9, color: [1, 2, 3, 190] } } };
  const c = { type: "Rewire", layoutType: "Rewire<in=1,out=8>", depth: 1, profileFingerprint: "explicit" };
  const sandbox = vm.createContext({ app: { layout } });
  const isApp = source === appSource;
  for (const name of isApp ? ["mergeLayout", "componentLayoutKey", "getTypeLayoutFor"] : ["mergeLayout", "componentLayout"])
    vm.runInContext(extract(source, name), sandbox);
  const get = () => isApp ? sandbox.getTypeLayoutFor(c) : sandbox.componentLayout(c, layout, "test");
  assert.equal(get().aspect_ratio, undefined);
  assert.equal(get().min_aspect_ratio, undefined);
  assert.deepEqual(get().color, layout.type_layouts.Rewire.color);
  layout.type_layouts[c.layoutType] = { aspect_ratio: 1.5, min_aspect_ratio: 1.2 };
  assert.equal(get().aspect_ratio, 1.5);
  assert.equal(get().min_aspect_ratio, 1.2);
  assert.equal(layout.type_layouts.Rewire.aspect_ratio, 9);
  c.profileFingerprint = "saved";
  layout.profile_layouts = { saved: { Rewire: { aspect_ratio: 10, min_aspect_ratio: 10, color: [9, 8, 7, 190] } } };
  assert.equal(get().aspect_ratio, 1.5);
  assert.deepEqual(get().color, layout.profile_layouts.saved.Rewire.color);
  layout.profile_layouts.saved[c.layoutType] = { aspect_ratio: 2, color: [6, 5, 4, 190] };
  assert.equal(get().aspect_ratio, 2);
  assert.deepEqual(get().color, layout.profile_layouts.saved[c.layoutType].color);
}
console.log("Rewire geometry: packing, bit/field taps, fills, X/Z values, fallback lanes and layout variants passed");
