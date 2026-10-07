import assert from 'node:assert/strict';
import {execFileSync} from 'node:child_process';
import {readFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
const [js,wasm,probe]=process.argv.slice(2);
const module=await (await import(pathToFileURL(resolve(js)))).default({wasmBinary:await readFile(wasm)});
for(const [test,alias] of [['RV32ISingleCycleSystemTest/program-03','rv32i-program3'],
    ['RV32IFiveStageCoreProgramTest/program-03','rv32i-five-stage-program3'],
    ['RV32IBaselinePipelineProgramTest/program-03','rv32i-baseline-program3']]) {
  const bridge=new module.WasmVisualizer();
  try {
    bridge.setProfileOverride('RV32I_PROGRAM_ROOT.CORE','behavioral');
    const topology=JSON.parse(bridge.loadScenario(test,alias,0));
    const selected=new Map();
    for(let i=0;i<topology.timestamps.length&&selected.size<3;i++)
      for(const name of ['INSTRUCTION_MEMORY','DATA_MEMORY']) {
        const id=topology.rootId+'.'+name,state=JSON.parse(bridge.componentStateJson(i,id));
        const key=name+':'+state.transaction?.operation;
        if(state.windowActive&&state.activeAddress>0&&['READ','WRITE'].includes(state.transaction?.operation)&&!selected.has(key))
          selected.set(key,{i,id,state});
      }
    assert.equal(selected.size,3);
    for(const {i,id,state} of selected.values()) {
      const native=(...extra)=>JSON.parse(execFileSync(probe,['component',test,alias,String(i),id,
        '--override','RV32I_PROGRAM_ROOT.CORE','behavioral',...extra],{encoding:'utf8',maxBuffer:8*1024*1024}));
      assert.deepEqual(state,native());
      assert(state.windowWords.some(row=>row.address===state.activeBaseAddress));
      assert.deepEqual(JSON.parse(bridge.memoryPageJson(i,id,1600,8)),native('--address','1600','--count','8'));
      if(id.endsWith('INSTRUCTION_MEMORY')) assert(state.touchedWords.every(row=>row.instructionText));
    }
    const {i,id,state}=selected.values().next().value;
    bridge.componentStateJson(topology.timestamps.length-1,id);
    assert.deepEqual(JSON.parse(bridge.componentStateJson(i,id)),state);
    console.log(`${alias}: active fetch/read/write, paged snapshots and rewind match native`);
  } finally {bridge.delete();}
}
