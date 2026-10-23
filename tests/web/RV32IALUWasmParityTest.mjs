// Native/WASM contract check for the integer ALU references and Execute.
import assert from 'node:assert/strict';
import {execFileSync} from 'node:child_process';
import {readFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';

const [modulePath, wasmPath, probePath] = process.argv.slice(2);
const module = await (await import(pathToFileURL(resolve(modulePath)))).default({wasmBinary: await readFile(wasmPath)});
const cases = [
  ['RV32IALUAddSubTest','rv32i-alu-add-sub'], ['RV32IALULogicTest','rv32i-alu-logic'], ['RV32IALUShifterTest','rv32i-alu-shifter'],
  ['RV32IALUControlTest','rv32i-alu-control'], ['RV32IALUCompareTest','rv32i-alu-compare'],
  ['RV32IALUResultSelectTest','rv32i-alu-result-select'],
  ['RV32IALUTest','rv32i-alu'], ['RV32ISingleCycleExecuteStageTest','rv32i-single-cycle-execute'],
];
for (const [test, scenario] of cases) {
  const bridge = new module.WasmVisualizer();
  const native = (...args) => JSON.parse(execFileSync(probePath, [...args.slice(0,1),test,scenario,...args.slice(1)],
    {encoding:'utf8',maxBuffer:64*1024*1024}));
  try {
    const actual = JSON.parse(bridge.loadScenario(test,scenario,0));
    const expected = native('topology');
    for (const key of ['rootId','rootType','timestamps','checkpoints','stats','components','pins','wires','state'])
      assert.deepEqual(actual[key],expected[key],`${scenario}/${key}`);
    assert.deepEqual(JSON.parse(bridge.exampleRowsJson()),native('examples'),`${scenario}/examples`);
    assert.deepEqual(JSON.parse(bridge.stateJson(actual.timestamps.length-1)),
      native('state',String(actual.timestamps.length-1)),`${scenario}/final state`);
    console.log(`${scenario}: native/WASM topology, checkpoints, examples and final state match`);
  } finally {bridge.delete();}
}
