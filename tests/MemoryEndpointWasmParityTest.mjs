// Only new transaction endpoints: portable topology, history and inspector.
import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { readFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
const [modulePath, wasmPath, probePath] = process.argv.slice(2);
const module = await (await import(pathToFileURL(resolve(modulePath)))).default({wasmBinary: await readFile(wasmPath)});
const source = (await Promise.all(['3.2','3.3','3.1.6'].map(s =>
  readFile(new URL(`../site-docs/memory/${s}.md`, import.meta.url),'utf8')))).join('\n');
let count = 0;
for (const [, scenario, test] of source.matchAll(/<!-- simulator-examples: (\S+) (\S+) -->/g)) {
  const modes = scenario === 'scalable-memory-container' ? ['behavioral'] :
    scenario.startsWith('memory-endpoint-') && scenario !== 'memory-endpoint-relay' ? ['structural','behavioral','device'] : ['structural'];
  for (const mode of modes) {
    const overrides = [['MEMORY_ENDPOINT_ROOT', mode === 'behavioral' ? mode : 'structural']];
    if (mode === 'device') overrides.push(['MEMORY_ENDPOINT_ROOT.MEMORY','behavioral']);
    const native = (...args) => JSON.parse(execFileSync(probePath,
      [...args,...overrides.flatMap(([p,f])=>['--override',p,f])], {encoding:'utf8',maxBuffer:32*1024*1024}));
    const bridge = new module.WasmVisualizer();
    try {
      for (const [p,f] of overrides) bridge.setProfileOverride(p,f);
      const actual=JSON.parse(bridge.loadScenario(test,scenario,0));
      const expected=native('topology',test,scenario);
      for (const key of ['rootId','rootType','layoutKey','timestamps','checkpoints','stats','components','pins','wires','state'])
        assert.deepEqual(actual[key],expected[key],`${scenario}/${mode}/${key}`);
      assert.deepEqual(JSON.parse(bridge.exampleRowsJson()),native('examples',test,scenario));
      const inspector = mode === 'device' || scenario === 'memory-endpoint-relay' ? actual.rootId+'.MEMORY' : actual.rootId;
      for (const i of new Set([0,Math.floor(actual.timestamps.length/2),actual.timestamps.length-1])) {
        assert.deepEqual(JSON.parse(bridge.stateJson(i)),native('state',test,scenario,String(i)));
        const state=JSON.parse(bridge.componentStateJson(i,inspector));
        assert.deepEqual(state,native('component',test,scenario,String(i),inspector));
        if (mode !== 'structural' || scenario === 'memory-endpoint-relay') assert.equal(state.supported,true);
      }
      console.log(`${scenario}/${mode}: native/WASM parity passed`);++count;
    } finally {bridge.delete();}
  }
}
for (const test of ['MemoryEndpointContractTest','MemoryEndpointTimingTest']) {
  const bridge=new module.WasmVisualizer();
  try {bridge.loadScenario(test,test,0); console.log(`${test}: WASM passed`);}
  finally {bridge.delete();}
}
console.log(`${count} transaction endpoint parity comparisons passed`);
