"use strict";

const assert = require("node:assert/strict");
const path = require("node:path");

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
