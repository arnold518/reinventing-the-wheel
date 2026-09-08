"use strict";

(function exportRewireGeometry(root, factory) {
  const api = factory();
  if (typeof module === "object" && module.exports) module.exports = api;
  else root.CircuitRewireGeometry = api;
}(typeof globalThis !== "undefined" ? globalThis : this, () => {
  function groupedMappings(rewire) {
    const mappings = rewire && Array.isArray(rewire.mappings) ? rewire.mappings : [];
    const groups = new Map();
    for (const mapping of mappings) {
      const key = `${mapping.srcWire}\u0000${mapping.dstWire}`;
      if (!groups.has(key)) {
        groups.set(key, {
          srcWire: mapping.srcWire,
          dstWire: mapping.dstWire,
          mappings: [],
        });
      }
      groups.get(key).mappings.push(mapping);
    }
    return [...groups.values()].sort((left, right) => {
      if (left.srcWire !== right.srcWire) return left.srcWire.localeCompare(right.srcWire);
      return left.dstWire.localeCompare(right.dstWire);
    });
  }

  function destinationRange(mappings) {
    const bits = [...new Set((mappings || [])
      .map((mapping) => Number(mapping.dstBit))
      .filter(Number.isInteger))]
      .sort((left, right) => left - right);
    if (!bits.length) return null;
    const contiguous = bits.every((bit, index) => index === 0 || bit === bits[index - 1] + 1);
    const low = bits[0];
    const high = bits[bits.length - 1];
    return {
      bits,
      low,
      high,
      contiguous,
      label: contiguous
        ? (low === high ? `[${low}]` : `[${high}:${low}]`)
        : `{${bits.slice().reverse().join(",")}}`,
    };
  }

  function packedBusModel(rewire, minimumSources = 3) {
    const groups = groupedMappings(rewire);
    if (groups.length < minimumSources) return null;
    const destination = groups[0].dstWire;
    if (!destination || groups.some((group) => group.dstWire !== destination)) return null;

    const outputs = rewire && Array.isArray(rewire.outputs) ? rewire.outputs : [];
    const output = outputs.find((candidate) => candidate.name === destination);
    const width = Number(output && output.width);
    if (!Number.isInteger(width) || width <= 1) return null;

    const mappedBits = new Set();
    const modeledGroups = groups.map((group) => {
      const range = destinationRange(group.mappings);
      for (const bit of range ? range.bits : []) mappedBits.add(bit);
      return { ...group, range };
    });
    const unmappedBits = [];
    for (let bit = 0; bit < width; bit += 1) {
      if (!mappedBits.has(bit)) unmappedBits.push(bit);
    }
    return {
      destination,
      width,
      groups: modeledGroups,
      unmappedBits,
      unmappedDefault: String((rewire && rewire.unmappedDefault) || "UNKNOWN"),
    };
  }

  return {
    destinationRange,
    groupedMappings,
    packedBusModel,
  };
}));
