// New memory components only. Native/WASM parity includes mixed fidelity and
// time-scrubbed memory inspection; each bridge is destroyed before the next.
import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { readFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
const [modulePath, wasmPath, probePath] = process.argv.slice(2);
const factory = (await import(pathToFileURL(resolve(modulePath)))).default;
const module = await factory({ wasmBinary: await readFile(wasmPath) });
const documents = await Promise.all(['3.1.0', '3.1.3', '3.1.4', '3.1.5'].map(section =>
  readFile(new URL(`../../site-docs/memory/${section}.md`, import.meta.url), 'utf8')));
const behavioral = new Set(['rom-cell', 'rom-row', 'rom-storage-bank', 'rom',
  'dram-row', 'dram-storage-bank', 'dram', 'nor-flash-row', 'nor-flash-storage-bank', 'nor-flash']);
let count = 0;
for (const [, scenario, test] of documents.join('\n').matchAll(/<!-- simulator-examples: (\S+) (\S+) -->/g)) {
  const modes = ['structural'];
  if (behavioral.has(scenario)) modes.push('behavioral');
  if (['dram', 'rom', 'nor-flash'].includes(scenario)) modes.push('bank', 'row');
  for (const mode of modes) {
    const overrides = [['MEMORY_ROOT', mode === 'behavioral' ? mode : 'structural']];
    if (mode === 'bank') overrides.push(['MEMORY_ROOT.STORAGE', 'behavioral']);
    if (mode === 'row') overrides.push(['MEMORY_ROOT.STORAGE.ROW_0', 'behavioral']);
    const native = (...args) => JSON.parse(execFileSync(probePath,
      [...args, ...overrides.flatMap(([p, f]) => ['--override', p, f])],
      { encoding: 'utf8', maxBuffer: 32 * 1024 * 1024 }));
    const bridge = new module.WasmVisualizer();
    try {
      for (const [p, f] of overrides) bridge.setProfileOverride(p, f);
      const actual = JSON.parse(bridge.loadScenario(test, scenario, 0));
      const expected = native('topology', test, scenario);
      for (const key of ['rootId', 'rootType', 'layoutKey', 'timestamps', 'checkpoints', 'stats', 'components', 'pins', 'wires', 'state'])
        assert.deepEqual(actual[key], expected[key], `${scenario}/${mode}: ${key}`);
      assert.deepEqual(JSON.parse(bridge.exampleRowsJson()), native('examples', test, scenario));
      for (const index of new Set([0, Math.floor(actual.timestamps.length / 2), actual.timestamps.length - 1])) {
        assert.deepEqual(JSON.parse(bridge.stateJson(index)), native('state', test, scenario, String(index)));
        const state = JSON.parse(bridge.componentStateJson(index, actual.rootId));
        assert.deepEqual(state, native('component', test, scenario, String(index), actual.rootId));
        if (mode === 'behavioral' && ['dram', 'rom', 'nor-flash'].includes(scenario))
          assert.equal(state.supported, true, 'Memory inspector unavailable');
      }
      ++count;
      console.log(`${scenario}/${mode}: native/WASM parity passed`);
    } finally { bridge.delete(); }
  }
}
for (const test of ['MemoryHelperContractTest', 'MemoryDeviceContractTest', 'MemoryDeviceTimingTest']) {
  const bridge = new module.WasmVisualizer();
  try { bridge.loadScenario(test, test, 0); console.log(`${test}: WASM passed`); }
  finally { bridge.delete(); }
}
console.log(`${count} new-memory parity comparisons passed`);
