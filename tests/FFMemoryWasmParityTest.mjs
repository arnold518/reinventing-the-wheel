// Run after tools/build-pages.sh: native and WASM share all memory contracts.
import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { readFile } from 'node:fs/promises';
import { pathToFileURL } from 'node:url';
import { performance } from 'node:perf_hooks';

const [modulePath, wasmPath, probePath] = process.argv.slice(2);
if (!probePath) throw new Error('Usage: node FFMemoryWasmParityTest.mjs <module.js> <module.wasm> <native-probe>');
const imported = await import(pathToFileURL(modulePath));
const module = await imported.default({ wasmBinary: await readFile(wasmPath) });
for (const [name, scenario] of [['FFMemoryTest', 'ff-memory-container'],
  ['FFWordTest', 'ff-word'],
  ['FFStorageBankTest', 'ff-storage-bank']]) {
  const rootId = name === 'FFWordTest' ? 'FF_WORD_ROOT'
    : name === 'FFStorageBankTest' ? 'FF_STORAGE_BANK_ROOT' : 'MEMORY_CONTAINER_ROOT';
  const modes = name === 'FFMemoryTest'
    ? ['structural', 'behavioral', 'mixed', 'bank'] : ['structural', 'behavioral'];
  for (const fidelity of modes) {
    const bridge = new module.WasmVisualizer();
    try {
      const overrides = [[rootId, ['mixed', 'bank'].includes(fidelity) ? 'structural' : fidelity]];
      if (fidelity === 'bank') overrides.push([`${rootId}.STORAGE`, 'behavioral']);
      if (fidelity === 'mixed') overrides.push([`${rootId}.STORAGE.WORD_0`, 'behavioral'],
        [`${rootId}.STORAGE.WORD_1.CELL_0`, 'behavioral']);
      for (const [path, choice] of overrides) bridge.setProfileOverride(path, choice);
      const native = (...args) => JSON.parse(execFileSync(probePath,
        [...args, ...overrides.flatMap(([path, choice]) => ['--override', path, choice])],
        { encoding: 'utf8', maxBuffer: 128 * 1024 * 1024 }));
      const start = performance.now();
      const actual = JSON.parse(bridge.loadScenario(name, scenario, 0));
      const elapsedMs = performance.now() - start;
      const expected = native('topology', name, scenario);
      for (const key of ['rootId', 'rootType', 'layoutKey', 'timestamps', 'checkpoints', 'stats', 'components', 'pins', 'wires', 'state']) {
        assert.deepEqual(actual[key], expected[key], `${scenario}/${fidelity}: ${key}`);
      }
      assert.deepEqual(JSON.parse(bridge.exampleRowsJson()), native('examples', name, scenario));
      for (const index of new Set([0, Math.floor(actual.timestamps.length / 2), actual.timestamps.length - 1])) {
        assert.deepEqual(JSON.parse(bridge.stateJson(index)), native('state', name, scenario, String(index)));
        assert.deepEqual(JSON.parse(bridge.componentStateJson(index, actual.rootId)),
          native('component', name, scenario, String(index), actual.rootId));
      }
      console.log(JSON.stringify({scenario, fidelity, elapsedMs, components: actual.stats.componentCount,
        timestamps: actual.timestamps.length}));
    } finally { bridge.delete(); }
  }
}
