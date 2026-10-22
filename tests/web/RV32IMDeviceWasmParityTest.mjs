// Recorded component contracts, including the disabled system scenario.
import assert from 'node:assert/strict';
import {execFileSync} from 'node:child_process';
import {readdir, readFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';

const [js, wasm, probe] = process.argv.slice(2);
const mod = await (await import(pathToFileURL(resolve(js)))).default({wasmBinary: await readFile(wasm)});
const cases = [];
for (const directory of ['rv32im', 'device-io']) {
  const base = new URL(`../../site-docs/${directory}/`, import.meta.url);
  for (const file of (await readdir(base)).filter(f => f.endsWith('.md')).sort()) {
    const source = await readFile(new URL(file, base), 'utf8');
    for (const [, alias, test] of source.matchAll(/<!-- simulator-examples: (\S+) (\S+) -->/g))
      cases.push({alias, test});
  }
}
assert.equal(cases.length, 22);
const pattern = process.env.CIRCUITSIM_DEVICE_SCENARIO_FILTER;
const selected = cases.filter(({alias}) => !pattern || new RegExp(pattern).test(alias));
assert(selected.length, 'No device scenarios match the selected filter');
for (const {alias, test} of selected) {
  const native = (...args) => JSON.parse(execFileSync(probe, args, {
    encoding: 'utf8', maxBuffer: 64 * 1024 * 1024,
  }));
  const expected = native('topology', test, alias);
  const bridge = new mod.WasmVisualizer();
  try {
    const actual = JSON.parse(bridge.loadScenario(test, alias, 0));
    for (const field of ['rootId', 'rootType', 'layoutKey', 'timestamps', 'checkpoints',
      'stats', 'components', 'pins', 'wires', 'state'])
      assert.deepEqual(actual[field], expected[field], `${alias}: ${field}`);
    const rows = JSON.parse(bridge.exampleRowsJson());
    assert.deepEqual(rows, native('examples', test, alias), `${alias}: examples`);
    for (const index of new Set([0, Math.floor(actual.timestamps.length / 2), actual.timestamps.length - 1])) {
      assert.deepEqual(JSON.parse(bridge.stateJson(index)),
        native('state', test, alias, String(index)), `${alias}: state ${index}`);
    }
    if (['FramebufferDisplay','ButtonDevice','ConsoleDevice','TimerDevice','SynchronousFifo<8>'].includes(actual.rootType)) {
      for (const index of [0, actual.timestamps.length-1, 0]) {
        const view=JSON.parse(bridge.componentStateJson(index,actual.rootId));
        assert.deepEqual(view,native('component',test,alias,String(index),actual.rootId),`${alias}: device observer ${index}`);
        assert.equal(view.supported,true);
        if(view.kind==='display')assert.equal(view.pixels.length,view.width*view.height*2);
        if(view.kind==='keyboard')assert.equal(view.levels.length,32);
      }
    }
    // Explicitly scrub backwards after visiting the end of the recording.
    assert.deepEqual(JSON.parse(bridge.stateJson(0)), expected.state, `${alias}: rewind`);
    console.log(`${alias}: topology, examples and recorded state match native`);
  } finally {
    bridge.delete();
  }
}
console.log(`${selected.length} of ${cases.length} RV32IM/device recorded scenarios match native`);
