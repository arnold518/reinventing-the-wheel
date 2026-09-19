// Register32 and representative CPU users, with bounded sequential sessions.
import assert from 'node:assert/strict';
import {execFileSync} from 'node:child_process';
import {readFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';

const [modulePath, wasmPath, probePath] = process.argv.slice(2);
const module = await (await import(pathToFileURL(resolve(modulePath)))).default({wasmBinary: await readFile(wasmPath)});
const cases = [
  ['Register32Test','register32',[]],
  ['Register32Test','register32',[['REGISTER32_ROOT','behavioral']]],
  ['Register32Test','register32',[['REGISTER32_ROOT.STORAGE','behavioral']]],
  ['RV32ISingleCycleFetchStageTest','rv32i-single-cycle-fetch',[]],
  ['RV32IIFIDPipelineRegisterTest','rv32i-ifid-pipeline-register',[]],
];
for (const [test, alias, overrides] of cases) {
  const bridge = new module.WasmVisualizer();
  const args = overrides.flatMap(([path,mode])=>['--override',path,mode]);
  const native = (...action) => JSON.parse(execFileSync(probePath,[...action,...args],
    {encoding:'utf8',maxBuffer:32*1024*1024,timeout:120000}));
  try {
    for (const [path,mode] of overrides) bridge.setProfileOverride(path,mode);
    const actual=JSON.parse(bridge.loadScenario(test,alias,0));
    const expected=native('topology',test,alias);
    for (const key of ['rootId','rootType','timestamps','checkpoints','stats','components','pins','wires','state'])
      assert.deepEqual(actual[key],expected[key],`${alias}/${key}`);
    const examples=JSON.parse(bridge.exampleRowsJson());
    assert.deepEqual(examples,native('examples',test,alias));
    const pins=new Map(actual.pins.filter(p=>p.componentId===actual.rootId).map(p=>[p.name,p]));
    for (const row of examples.rows) {
      const state=JSON.parse(bridge.stateJson(row.timelineIndex));
      for (const [name,value] of Object.entries(row.expectedOutputs))
        assert.equal(state.pins[pins.get(name).stateIndex],value,`${alias}/${row.checkpoint}/${name}`);
    }
    assert.deepEqual(JSON.parse(bridge.stateJson(actual.timestamps.length-1)),
      native('state',test,alias,String(actual.timestamps.length-1)));
    for (const register of actual.components.filter(c=>c.type==='Register32' && c.fidelity==='structural'))
      assert.equal(actual.components.find(c=>c.id===register.id+'.STORAGE')?.type,'FFWord<32>');
    console.log(`${alias} ${JSON.stringify(overrides)}: topology, ${actual.checkpoints.length} checkpoints, examples and final state match`);
  } finally { bridge.delete(); }
}
