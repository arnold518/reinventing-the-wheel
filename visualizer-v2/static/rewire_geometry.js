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

  // Recognize bus-to-bit wiring from the actual mapping, not instance names.
  // Each output must be fully driven by exactly one source bit. Partial,
  // multi-source, and bus-to-bus mappings keep the general renderer.
  function unpackedBusModel(rewire) {
    const inputs = rewire?.inputs || [];
    const outputs = rewire?.outputs || [];
    const mappings = rewire?.mappings || [];
    if (inputs.length !== 1 || outputs.length < 2) return null;
    const { name: source, width } = inputs[0];
    if (!source || !Number.isInteger(width) || width <= 1) return null;
    if (outputs.some(output => output.width !== 1) || mappings.length !== outputs.length) return null;
    const byOutput = new Map();
    for (const mapping of mappings) {
      if (mapping.srcWire !== source || mapping.dstBit !== 0
          || !Number.isInteger(mapping.srcBit) || mapping.srcBit < 0 || mapping.srcBit >= width
          || byOutput.has(mapping.dstWire)) return null;
      byOutput.set(mapping.dstWire, mapping.srcBit);
    }
    if (new Set(outputs.map(output => output.name)).size !== outputs.length
        || outputs.some(output => !byOutput.has(output.name))) return null;
    return {
      source,
      width,
      taps: outputs.map(output => ({
        destination: output.name,
        sourceBit: byOutput.get(output.name),
        label: `[${byOutput.get(output.name)}]`,
      })),
    };
  }

  function fillBit(unmappedDefault) {
    return ({ LOW: "0", HIGH: "1" })[unmappedDefault] || "X";
  }

  // Summarize in destination MSB-to-LSB order. Form ascending source slices
  // from the low end first, so a sign extension reads {20×[31], [31:20]}.
  function fieldLabel(sourceBits, unmappedDefault) {
    const parts = [];
    for (let low = 0; low < sourceBits.length;) {
      const first = sourceBits[low];
      let high = low;
      if (first === null) {
        while (sourceBits[high + 1] === null) high++;
      } else {
        const next = sourceBits[low + 1];
        const step = next === null || next === undefined ? null : next - first;
        if (step === 0 || step === 1 || step === -1) {
          while (high + 1 < sourceBits.length && sourceBits[high + 1] !== null
              && sourceBits[high + 1] - sourceBits[high] === step) high++;
        }
      }
      const count = high - low + 1;
      let part;
      if (first === null) part = count === 1 ? fillBit(unmappedDefault) : `${count}×${fillBit(unmappedDefault)}`;
      else if (count === 1) part = `[${first}]`;
      else if (sourceBits[high] === first) part = `${count}×[${first}]`;
      else part = `[${sourceBits[high]}:${first}]`;
      parts.push(part);
      low = high + 1;
    }
    return parts.length === 1 ? parts[0] : `{${parts.reverse().join(", ")}}`;
  }

  // One bus feeding arbitrary multi-bit fields. These are bit mappings, not
  // electrically joined output nets; repeated bits and default-filled holes
  // (including completely constant outputs) must remain explicit.
  function fieldExtractionModel(rewire) {
    const inputs = rewire?.inputs || [];
    const outputs = rewire?.outputs || [];
    if (inputs.length !== 1 || outputs.length < 2 || outputs.every(p => p.width === 1)) return null;
    const { name: source, width } = inputs[0];
    if (!source || !Number.isInteger(width) || width <= 1) return null;
    const byOutput = new Map();
    for (const output of outputs) {
      if (!output.name || !Number.isInteger(output.width) || output.width < 1 || byOutput.has(output.name)) return null;
      byOutput.set(output.name, Array(output.width).fill(null));
    }
    for (const mapping of rewire.mappings || []) {
      const bits = byOutput.get(mapping.dstWire);
      if (mapping.srcWire !== source || !Number.isInteger(mapping.srcBit)
          || mapping.srcBit < 0 || mapping.srcBit >= width || !bits
          || !Number.isInteger(mapping.dstBit) || mapping.dstBit < 0 || mapping.dstBit >= bits.length
          || bits[mapping.dstBit] !== null) return null;
      bits[mapping.dstBit] = mapping.srcBit;
    }
    const unmappedDefault = rewire.unmappedDefault || "UNKNOWN";
    return {
      source, width, unmappedDefault,
      taps: outputs.map(output => ({
        destination: output.name, width: output.width,
        sourceBits: byOutput.get(output.name),
        label: fieldLabel(byOutput.get(output.name), unmappedDefault),
      })),
    };
  }

  function fieldValue(tap, sourceValue, unmappedDefault) {
    return tap.sourceBits.slice().reverse().map(bit => bit === null
      ? fillBit(unmappedDefault)
      : sourceValue[sourceValue.length - 1 - bit] || "X").join("");
  }

  return {
    destinationRange,
    groupedMappings,
    packedBusModel,
    unpackedBusModel,
    fieldExtractionModel,
    fieldValue,
  };
}));
