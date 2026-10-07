// Exact native/WASM parity plus mixed-fidelity and history inspection.
import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { readFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
const [modulePath, wasmPath, probePath] = process.argv.slice(2);
const factory = (await import(pathToFileURL(resolve(modulePath)))).default;
const module = await factory({wasmBinary: await readFile(wasmPath)});
const docs = await readFile(new URL('../../site-docs/memory/3.1.2.md', import.meta.url), 'utf8');
let comparisons = 0;
for (const [, scenario, test] of docs.matchAll(/<!-- simulator-examples: (\S+) (\S+) -->/g)) {
  const modes = ['structural'];
  if (['sram-cell', 'sram-row', 'sram-storage-bank', 'sram'].includes(scenario)) modes.push('behavioral');
  if (scenario === 'sram') modes.push('bank', 'cells', 'rows');
  for (const mode of modes) {
    const overrides = [['SRAM_ROOT', mode === 'behavioral' ? mode : 'structural']];
    if (mode === 'bank') overrides.push(['SRAM_ROOT.STORAGE', 'behavioral']);
    if (mode === 'cells') overrides.push(['SRAM_ROOT.STORAGE.ROW_0.CELL_0', 'behavioral']);
    if (mode === 'rows') overrides.push(['SRAM_ROOT.STORAGE.ROW_0', 'behavioral']);
    const native = (...args) => JSON.parse(execFileSync(probePath,
      [...args, ...overrides.flatMap(([p, f]) => ['--override', p, f])], {encoding:'utf8', maxBuffer:128*1024*1024}));
    const bridge = new module.WasmVisualizer();
    try {
      for (const [p,f] of overrides) bridge.setProfileOverride(p,f);
      const actual = JSON.parse(bridge.loadScenario(test,scenario,0));
      const expected = native('topology',test,scenario);
      for (const key of ['rootId','rootType','layoutKey','timestamps','checkpoints','stats','components','pins','wires','state'])
        assert.deepEqual(actual[key],expected[key],`${scenario}/${mode}: ${key}`);
      assert.deepEqual(JSON.parse(bridge.exampleRowsJson()),native('examples',test,scenario));
      for (const index of new Set([0,Math.floor(actual.timestamps.length/2),actual.timestamps.length-1])) {
        assert.deepEqual(JSON.parse(bridge.stateJson(index)),native('state',test,scenario,String(index)));
        assert.deepEqual(JSON.parse(bridge.componentStateJson(index,actual.rootId)),native('component',test,scenario,String(index),actual.rootId));
      }
      ++comparisons;
      console.log(`${scenario}/${mode}: native/WASM parity passed`);
    } finally {bridge.delete();}
  }
}
console.log(`${comparisons} SRAM parity comparisons passed`);
