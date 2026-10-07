// Fail packaging if a registered CPU program cannot open without the server.
// Program images differ; their four-child testbench topology is shared.
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
const [js,wasm,layoutPath]=process.argv.slice(2);
const module=await (await import(pathToFileURL(resolve(js)))).default({wasmBinary:await readFile(wasm)});
const bridge=new module.WasmVisualizer();
try {
  const layout=JSON.parse(await readFile(layoutPath,'utf8'));
  const names=JSON.parse(bridge.visualizableTestsJson()).map(row=>row.name);
  let count=0;
  for(const name of names) {
    const standard=name.match(/^(RV32ISingleCycleSystemTest|RV32IFiveStageCoreProgramTest|RV32IBaselinePipelineProgramTest)\/program-(\d+)$/);
    const predicted=name.match(/^RV32IPredictionProgramTest\/(not-taken|taken|backward-taken|one-bit|two-bit)\/program-(\d+)$/);
    if(!standard&&!predicted)continue;
    const prefix=predicted?`rv32i-predict-${predicted[1]}-program`:
      standard[1]==='RV32ISingleCycleSystemTest'?'rv32i-program':
      standard[1]==='RV32IFiveStageCoreProgramTest'?'rv32i-five-stage-program':'rv32i-baseline-program';
    const key=prefix+Number((standard||predicted)[2]),entry=layout.root_layouts[key];
    for(const child of ['CLOCK','CORE','INSTRUCTION_MEMORY','DATA_MEMORY']) {
      const placement=entry?.children?.[child];
      assert(placement&&Array.isArray(placement.rel_pos)&&placement.rel_pos.length===2
        &&placement.rel_pos.every(Number.isFinite)&&Number.isFinite(placement.rel_width)&&placement.rel_width>0,
        `Missing packaged layout: ${key}.${child}. Refresh the matching pipeline program roots.`);
    }
    count++;
  }
  assert(count>0,'No CPU program scenarios found');
  console.log(`${count} CPU program root layouts are packaged`);
} finally {bridge.delete();}
