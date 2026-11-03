// Collect native states before allocating the portable heap.
import assert from 'node:assert/strict';
import {execFileSync} from 'node:child_process';
import {readFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
const [js,wasm,probe]=process.argv.slice(2),cases=[];
for(const [test,scenario,presets] of [
  ['RegisterTest','register32',['gate','logic','visual','compact']],
  ...['ConsoleDevice','TimerDevice','ButtonDevice','FramebufferDisplay'].map(name=>[
    name+'ScenarioTest',name.replace(/([a-z])([A-Z])/g,'$1-$2').toLowerCase(),['visual']])]){
  for(const preset of presets){
    const native=(command,...args)=>JSON.parse(execFileSync(probe,[command,test,scenario,...args,'--fidelity',preset],{encoding:'utf8',maxBuffer:32*1024*1024}));
    const topology=native('topology'),indexes=[...new Set([0,Math.floor(topology.timestamps.length/2),topology.timestamps.length-1])];
    cases.push({test,scenario,preset,topology,states:indexes.map(index=>[index,native('state',String(index))])});
  }
}
const module=await (await import(pathToFileURL(resolve(js)))).default({wasmBinary:await readFile(wasm)});
for(const {test,scenario,preset,topology,states} of cases){
  const bridge=new module.WasmVisualizer();
  try{
    bridge.setFidelityProfile(preset);
    const actual=JSON.parse(bridge.loadScenario(test,scenario,0));
    assert.equal(actual.profile.fidelityPreset,preset);
    assert.equal(actual.profileFingerprint,topology.profileFingerprint);
    assert.deepEqual(actual.components,topology.components);
    assert.deepEqual(actual.timestamps,topology.timestamps);
    for(const [index,state] of states)assert.deepEqual(JSON.parse(bridge.stateJson(index)),state);
    console.log(`${scenario}/${preset}: actual selections and three native/WASM states match`);
  }finally{bridge.delete();}
}
console.log(`${cases.length} fidelity preset parity cases pass`);
