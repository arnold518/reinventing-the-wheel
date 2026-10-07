// Native/WASM equivalence for the baseline. One WASM session at a time.
import assert from 'node:assert/strict';
import {execFileSync} from 'node:child_process';
import {readFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
const [js,wasm,probe]=process.argv.slice(2);
const module=await (await import(pathToFileURL(resolve(js)))).default({wasmBinary:await readFile(wasm)});
const cases=['FetchStage','DecodeStage','ExecuteStage','MemoryStage','DependencyUnit','Control','PipelineCore']
  .map(s=>[`RV32IBaseline${s}Test`,`RV32IBaseline${s}_ROOT`]);
cases.push(['RV32IWritebackStageTest','RV32I_WRITEBACK_STAGE_ROOT']);
cases.push(['RV32IPipelineExecutionStatusTest','RV32I_PIPELINE_EXECUTION_STATUS_ROOT']);
cases.push(['RV32IBaselinePipelineProgramTest/program-03','RV32I_PROGRAM_ROOT.CORE']);
for(const [test,root] of cases) for(const fidelity of ['structural','behavioral']) {
  const bridge=new module.WasmVisualizer();
  const native=(...args)=>JSON.parse(execFileSync(probe,[...args,'--override',root,fidelity],{encoding:'utf8',maxBuffer:80*1024*1024}));
  try {
    bridge.setProfileOverride(root,fidelity);
    const actual=JSON.parse(bridge.loadScenario(test,test,0));
    const expected=native('topology',test,test);
    for(const key of ['rootId','rootType','timestamps','checkpoints','stats','components','pins','wires','state'])
      assert.deepEqual(actual[key],expected[key],`${test}/${fidelity}/${key}`);
    if(test.includes('ProgramTest')) {
      for(const name of ['INSTRUCTION_MEMORY','DATA_MEMORY']) {
        const id='RV32I_PROGRAM_ROOT.'+name;
        assert.equal(actual.components.find(c=>c.id===id).type,'MemoryEndpoint<ff,32>');
        const index=actual.timestamps.length-1;
        assert.deepEqual(JSON.parse(bridge.componentStateJson(index,id)),native('component',test,test,String(index),id));
      }
    } else {
      const rows=JSON.parse(bridge.exampleRowsJson());
      assert.deepEqual(rows,native('examples',test,test));
      if(!test.includes('PipelineCore')) {
        const index=actual.timestamps.length-1;
        const inspector=JSON.parse(bridge.componentStateJson(index,root));
        assert.deepEqual(inspector,native('component',test,test,String(index),root));
        if(!test.includes('DecodeStage')) assert.equal(inspector.supported,false,`${test}: no register-file inspector`);
      }
    }
    console.log(`${test}/${fidelity}: topology, timing, state and examples match`);
  } finally {bridge.delete();}
}
