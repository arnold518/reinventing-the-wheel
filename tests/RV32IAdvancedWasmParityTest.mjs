// One session at a time: compare real native and WASM output, not only final PCs.
import assert from 'node:assert/strict';
import {execFileSync} from 'node:child_process';
import {readFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
const [js,wasm,probe]=process.argv.slice(2);
const module=await (await import(pathToFileURL(resolve(js)))).default({wasmBinary:await readFile(wasm)});
const cases=['PredictionCounter','BranchPredecode','BranchPredictor','BranchRecovery','PredictionPath']
  .map(s=>[`RV32I${s}Test`,'RV32I_PREDICTION_ROOT']);
cases.push(['RV32IExecuteStageTest','RV32I_EXECUTE_STAGE_ROOT']);
cases.push(['RV32IPipelineCoordinatorTest','RV32I_PIPELINE_COORDINATOR_ROOT']);
for(const policy of ['not-taken','taken','backward-taken','one-bit','two-bit'])
  cases.push([`RV32IPredictionProgramTest/${policy}/program-20`,'RV32I_PROGRAM_ROOT.CORE']);
for(const [test,root] of cases) for(const fidelity of ['structural','behavioral']) {
  const bridge=new module.WasmVisualizer();
  const native=(...args)=>JSON.parse(execFileSync(probe,[...args,'--override',root,fidelity],{encoding:'utf8',maxBuffer:80*1024*1024}));
  try {
    bridge.setProfileOverride(root,fidelity);
    const actual=JSON.parse(bridge.loadScenario(test,test,0));
    const expected=native('topology',test,test);
    for(const key of ['rootId','rootType','timestamps','checkpoints','stats','performanceMetrics','components','pins','wires','state'])
      assert.deepEqual(actual[key],expected[key],`${test}/${fidelity}/${key}`);
    if(!test.includes('ProgramTest')) assert.deepEqual(JSON.parse(bridge.exampleRowsJson()),native('examples',test,test));
    else {
      const index=actual.timestamps.length-1;
      for(const name of ['INSTRUCTION_MEMORY','DATA_MEMORY']) {
        const id='RV32I_PROGRAM_ROOT.'+name;
        assert.deepEqual(JSON.parse(bridge.componentStateJson(index,id)),native('component',test,test,String(index),id));
      }
    }
    console.log(`${test}/${fidelity}: hierarchy, time, checkpoints, metrics and state match`);
  } finally {bridge.delete();}
}
