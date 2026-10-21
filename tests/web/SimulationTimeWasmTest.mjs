import assert from 'node:assert/strict';
import {execFileSync} from 'node:child_process';
import {readFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
const [js,wasm,probe,verifier]=process.argv.slice(2);
for(const test of ['SimulationTimeTest','SimulationTimePlaybackTest']){
  const output=execFileSync(process.execPath,[verifier,test],{encoding:'utf8',timeout:180000,maxBuffer:16*1024*1024});
  process.stdout.write(output);
}
const module=await (await import(pathToFileURL(resolve(js)))).default({wasmBinary:await readFile(wasm)});
const bridge=new module.WasmVisualizer();
try{
  const expected=JSON.parse(execFileSync(probe,['topology','SimulationTimePlaybackTest','simulation-wide-time'],{encoding:'utf8'}));
  const actual=JSON.parse(bridge.loadScenario('SimulationTimePlaybackTest','simulation-wide-time',0));
  for(const field of ['rootId','rootType','timestamps','checkpoints','pins','wires','state'])assert.deepEqual(actual[field],expected[field],field);
  const base=(1n<<54n)+3n;
  assert(actual.timestamps.includes(base.toString()),'Exact wide input timestamp missing');
  for(const time of actual.timestamps){assert(BigInt(time)>=0n);if(BigInt(time)>9007199254740991n)assert.equal(typeof time,'string');}
  assert.deepEqual(actual.checkpoints.map(c=>c.time),[(base+2n).toString(),(base+10n).toString()]);
  for(const index of [actual.timestamps.length-1,0,...actual.checkpoints.map(c=>c.index)]){
    const native=JSON.parse(execFileSync(probe,['state','SimulationTimePlaybackTest','simulation-wide-time',String(index)],{encoding:'utf8'}));
    assert.deepEqual(JSON.parse(bridge.stateJson(index)),native,`state ${index}`);
  }
  console.log('PASS: wasm32 wide-time execution, exact transport and native/WASM rewind');
}finally{bridge.delete();}
