// Focused native/WASM contract check for the eight 4.2.3 references.
import assert from 'node:assert/strict';
import {execFileSync} from 'node:child_process';
import {readFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';

const [modulePath, wasmPath, probePath] = process.argv.slice(2);
const module = await (await import(pathToFileURL(resolve(modulePath)))).default({wasmBinary: await readFile(wasmPath)});
const cases = [
  ['AddSub32Test','addsub32'], ['Logic32Test','logic32'], ['Shifter32Test','shifter32'],
  ['ALU32ControlDecoderTest','alu32-control'], ['ALU32CompareUnitTest','alu32-compare'],
  ['ALU32ResultSelectorTest','alu32-result-selector'], ['ALU32FlagUnitTest','alu32-flags'],
  ['ALU32Test','alu32'], ['RV32ISingleCycleExecuteStageTest','rv32i-single-cycle-execute'],
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
    if (scenario==='alu32-flags') {
      const id=actual.rootId+'.SIGN_BIT';
      assert.equal(actual.components.find(c=>c.id===id)?.type,'Rewire');
      assert(!actual.components.some(c=>c.id===actual.rootId+'.RESULT_SPLIT'));
      assert.deepEqual(actual.pins.filter(p=>p.componentId===id).map(p=>[p.name,p.width]).sort(),
        [['RESULT',32],['SIGN',1]]);
      assert.equal(actual.checkpoints.length,42);
    }
    console.log(`${scenario}: native/WASM topology, checkpoints, examples and final state match`);
  } finally {bridge.delete();}
}
