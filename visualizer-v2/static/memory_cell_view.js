"use strict";

(function (root, factory) {
  const api = factory();
  if (typeof module === "object" && module.exports) module.exports = api;
  else root.CircuitMemoryCellView = api;
}(typeof globalThis !== "undefined" ? globalThis : this, () => {
  // Values always come from recorded pins at the selected timeline position.
  // In particular, do not invent a DRAM decay timer or a flash busy state.
  const definitions = {
    FlipFlopCell: { fields: [["Q", "STORED BIT"]], controls: [] },
    SRAMCell: { fields: [["Q", "STORED BIT"], ["Q_BAR", "COMPLEMENT"]], controls: ["WL", "DRIVE"] },
    DRAMCell: { fields: [["Q", "CHARGE (DIGITAL)"], ["SAMPLE", "READ SAMPLE"]], controls: ["WL", "RESTORE"] },
    ROMCell: { fields: [["Q", "PROGRAMMED BIT"]], controls: [] },
    NORFlashCell: { fields: [["Q", "STORED BIT"]], controls: ["WL", "PROGRAM", "ERASE"] },
  };
  function supports(type, fidelity) {
    // DRAM/NOR cells are terminal digital primitives registered as structural;
    // unlike expanded SRAM/FF/ROM cells they have no child circuit to obscure.
    const terminal = type === "DRAMCell" || type === "NORFlashCell";
    return Object.hasOwn(definitions, type) && (fidelity === "behavioral" || (terminal && fidelity === "structural"));
  }
  function model(type, fidelity, pins) {
    if (!supports(type, fidelity)) return null;
    const definition = definitions[type];
    const value = name => /^[01XZ]$/.test(pins[name]) ? pins[name] : "X";
    return {
      fields: definition.fields.map(([pin, label]) => ({ pin, label, value: value(pin) })),
      controls: definition.controls.map(pin => ({ pin, value: value(pin) })),
    };
  }
  return { model, supports };
}));
