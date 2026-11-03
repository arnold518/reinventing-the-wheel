// Every § 5 example scenario: WebAssembly topology, rows and recorded state match native.
import assert from 'node:assert/strict';
import {execFileSync} from 'node:child_process';
import {readdir, readFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';

const [js, wasm, probe] = process.argv.slice(2);
const mod = await (await import(pathToFileURL(resolve(js)))).default({wasmBinary: await readFile(wasm)});
const cases = [];
// Every example table of the § 5 chapter (the computer system).
const base = new URL('../../site-docs/system/', import.meta.url);
for (const file of (await readdir(base, {recursive: true})).filter(f => f.endsWith('.md')).sort()) {
  const source = await readFile(new URL(file, base), 'utf8');
  for (const [, alias, test] of source.matchAll(/<!-- simulator-examples: (\S+) (\S+) -->/g))
    cases.push({alias, test});
}
assert.equal(cases.length, 24);
// Two scenarios that run a guest program (no example table): the bring-up
// program on the one-bus machine and the Pong short version.
cases.push({alias: 'computer-system', test: 'ComputerSystemScenarioTest', guest: true},
           {alias: 'game-pong', test: 'GamePongScenarioTest', guest: true});
const pattern = process.env.CIRCUITSIM_DEVICE_SCENARIO_FILTER;
const selected = cases.filter(({alias}) => !pattern || new RegExp(pattern).test(alias));
assert(selected.length, 'No device scenarios match the selected filter');
for (const {alias, test, guest} of selected) {
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
    if (!guest) {
      const rows = JSON.parse(bridge.exampleRowsJson());
      assert.deepEqual(rows, native('examples', test, alias), `${alias}: examples`);
    }
    for (const index of new Set([0, Math.floor(actual.timestamps.length / 2), actual.timestamps.length - 1])) {
      assert.deepEqual(JSON.parse(bridge.stateJson(index)),
        native('state', test, alias, String(index)), `${alias}: state ${index}`);
    }
    if (guest) {
      // The machine's display at the last frame, read through its observer.
      const display = actual.components.find(c => c.name === 'DISPLAY');
      const last = actual.timestamps.length - 1;
      assert.deepEqual(JSON.parse(bridge.componentStateJson(last, display.id)),
        native('component', test, alias, String(last), display.id), `${alias}: display`);
    }
    if (['FramebufferDisplay','ButtonDevice','ConsoleDevice','TimerDevice','LedPanel','SynchronousFifo<8>'].includes(actual.rootType)) {
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
    console.log(`${alias}: topology, ${guest ? 'guest run' : 'examples'} and recorded state match native`);
  } finally {
    bridge.delete();
  }
}
console.log(`${selected.length} of ${cases.length} § 5 recorded scenarios match native`);
