// Exact portable-session topology/timestamp parity and initial/final recorded states.
import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { readFile } from 'node:fs/promises';
import { pathToFileURL } from 'node:url';
import { performance } from 'node:perf_hooks';

const [modulePath, wasmPath, probePath] = process.argv.slice(2);
if (!probePath) throw new Error('Usage: node ALUWasmParityTest.mjs <module.js> <module.wasm> <native-probe>');
const imported = await import(pathToFileURL(modulePath));
const module = await imported.default({ wasmBinary: await readFile(wasmPath) });
const scenarios = [
  ['ALUScenarioTest', 'alu'], ['ALUWidth8ScenarioTest', 'alu-width8'],
  ['ALUWidth32ScenarioTest', 'alu-width32'],
  ['RippleCarryAdderWidth16Test', 'ripple-carry-adder-w16'], ['ParallelPrefixAdderWidth16Test', 'parallel-prefix-adder-w16'],
  ['RippleCarryAdderWidth8Test', 'ripple-carry-adder-w8'], ['ZeroDetectWidth8Test', 'zero-detect-w8'],
  ['AddSubWidth8Test', 'add-sub-w8'],
  ['ArithmeticUnitWidth8Test', 'arithmetic-unit-w8'], ['LogicUnitWidth8Test', 'logic-unit-w8'],
  ['BarrelShifterWidth8Test', 'barrel-shifter-w8'], ['CompareSelectWidth8Test', 'compare-select-w8'],
  ['ALUFlagUnitWidth8Test', 'alu-flag-unit-w8'],
  ['ALUControlDecoderTest', 'alu-control-decoder'],
  ['ALUResultSelectorWidth8Test', 'alu-result-selector-w8'],
  ['RV32IALUControlTest', 'rv32i-alu-control'],
  ['RV32IALUCompareTest', 'rv32i-alu-compare'],
  ['RV32IALUResultSelectTest', 'rv32i-alu-result-select'],
  ['RV32IALUTest', 'rv32i-alu'],
];
for (const [test, scenario] of scenarios) {
  const native = (...args) => JSON.parse(execFileSync(probePath, args, { encoding: 'utf8', maxBuffer: 256 * 1024 * 1024 }));
  const bridge = new module.WasmVisualizer();
  try {
    const start = performance.now();
    const actual = JSON.parse(bridge.loadScenario(test, scenario, 0));
    const wasmMs = performance.now() - start;
    const nativeStart = performance.now();
    const expected = native('topology', test, scenario);
    const nativeProcessMs = performance.now() - nativeStart;
    for (const key of ['rootId', 'rootType', 'layoutKey', 'timestamps', 'checkpoints', 'stats', 'components', 'pins', 'wires', 'state']) {
      assert.deepEqual(actual[key], expected[key], `${scenario}: ${key}`);
    }
    assert.deepEqual(JSON.parse(bridge.exampleRowsJson()), native('examples', test, scenario),
      `${scenario}: declared inputs, expected outputs, and settled checkpoints`);
    const last = actual.timestamps.length - 1;
    assert.deepEqual(JSON.parse(bridge.stateJson(last)), native('state', test, scenario, String(last)), `${scenario}: final state`);
    if (scenario === 'alu-width8') {
      const rootPins = actual.pins.filter(pin => pin.componentId === actual.rootId);
      assert(rootPins.some(pin => pin.name === 'OP' && pin.width === 5));
      assert(rootPins.some(pin => pin.name === 'C_IN' && pin.width === 1));
      assert(rootPins.some(pin => pin.name === 'INVALID_OP' && pin.width === 1));
      const rootIndex = actual.components.findIndex(component => component.id === actual.rootId);
      const units = actual.components.filter(component => component.parentIndex === rootIndex);
      assert.equal(units.length, 7, 'extended ALU: seven functional blocks');
      assert(units.some(component => component.type === 'ArithmeticUnit<8>'));
      const owners = [actual.rootId, ...units.map(component => component.id)];
      const { sessionId: wasmSession, ...wasmSlice } = JSON.parse(bridge.topologySliceJson(owners));
      const { sessionId: nativeSession, ...nativeSlice } = native('slice', test, scenario, ...owners);
      // Each native probe is a fresh process; WASM session IDs advance across scenarios.
      assert.equal(wasmSession, actual.sessionId);
      assert.equal(nativeSession, expected.sessionId);
      assert.deepEqual(wasmSlice, nativeSlice, 'extended ALU: expanded topology');
    }
    if (scenario === 'rv32i-alu') {
      const rootIndex = actual.components.findIndex(component => component.id === actual.rootId);
      assert.equal(actual.components.filter(component => component.parentIndex === rootIndex).length, 6,
        'RV32IALU: six functional blocks');
      assert.equal(actual.components.filter(component => component.type === 'RV32IALUAddSub').length, 1,
        'integer ALU: one shared arithmetic unit');
      assert.equal(actual.components.filter(component => component.type === 'ParallelPrefixAdder<32>').length, 1,
        'integer ALU: one shared prefix adder');
      assert.equal(actual.components.filter(component => component.type === 'HalfAdder').length, 32,
        'integer ALU: one sum bit-slice per bit');
      assert.equal(actual.components.filter(component => component.type.startsWith('CarryPrefixLevel<')).length, 5,
        'integer ALU: five prefix-carry levels');
    }
    console.log(JSON.stringify({ scenario, wasmMs, nativeProcessMs, components: actual.stats.componentCount, timestamps: actual.timestamps.length }));
    const root = actual.components.find(component => component.id === actual.rootId);
    if (root.availableFidelities?.includes('behavioral')) {
      assert(root.profileSelectable, `${scenario}: selectable root`);
      const variants = [[actual.rootId]];
      if (scenario === 'alu-width8') {
        const rootIndex = actual.components.findIndex(component => component.id === actual.rootId);
        const children = actual.components.filter(component => component.parentIndex === rootIndex);
        for (const child of children) assert(child.profileSelectable, `${child.id}: selectable nested unit`);
        variants.push(children.map(component => component.id));
        variants.push([`${actual.rootId}.ARITHMETIC.ADD_SUB.ADD`, `${actual.rootId}.FLAGS.ZERO_DETECT`]);
      }
      for (const paths of variants) {
        bridge.clearProfileOverrides();
        for (const path of paths) bridge.setProfileOverride(path, 'behavioral');
        const options = paths.flatMap(path => ['--override', path, 'behavioral']);
        const started = performance.now();
        const selected = JSON.parse(bridge.loadScenario(test, scenario, 1));
        const selectedMs = performance.now() - started;
        const nativeSelected = native('topology', test, scenario, '--revision', '1', ...options);
        for (const key of ['rootId', 'rootType', 'layoutKey', 'timestamps', 'checkpoints', 'stats', 'components', 'pins', 'wires', 'state']) {
          assert.deepEqual(selected[key], nativeSelected[key], `${scenario}: behavioral ${key}`);
        }
        assert.deepEqual(JSON.parse(bridge.exampleRowsJson()), native('examples', test, scenario, '--revision', '1', ...options));
        assert(selected.stats.componentCount < actual.stats.componentCount);
        if (paths[0] === actual.rootId) assert.equal(selected.stats.componentCount, 1);
        if (paths.length === 7) assert.equal(selected.stats.componentCount, 8);
        for (const path of paths) assert.equal(selected.components.find(c => c.id === path).fidelity, 'behavioral');
        // Both exports validate every actual output against the same declared
        // expected rows; only profile-specific time/timeline fields may differ.
        const selectedRows = JSON.parse(bridge.exampleRowsJson()).rows;
        const structuralRows = native('examples', test, scenario).rows;
        const values = rows => rows.map(({ checkpoint, inputs, expectedOutputs }) => ({ checkpoint, inputs, expectedOutputs }));
        assert.deepEqual(values(selectedRows), values(structuralRows));
        console.log(JSON.stringify({ scenario, behavioralPaths: paths, wasmMs: selectedMs, components: selected.stats.componentCount }));
      }
    }
  } finally { bridge.delete(); }
}
