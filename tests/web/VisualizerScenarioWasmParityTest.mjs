// Catalog parity is cheap: enumerating scenarios does not execute their programs.
import assert from 'node:assert/strict';
import {execFileSync} from 'node:child_process';
import {readFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
import {scenarioOptions, menuOptions} from '../../visualizer-v2/static/scenario_catalog.mjs';
const [js, wasm, probe] = process.argv.slice(2);
const catalog = JSON.parse(await readFile(new URL('../../visualizer-v2/static/scenarios.json', import.meta.url)));
const native = JSON.parse(execFileSync(probe, ['--list'], {encoding: 'utf8'}));
const mod = await (await import(pathToFileURL(resolve(js)))).default({wasmBinary: await readFile(wasm)});
const bridge = new mod.WasmVisualizer();
try {
  const descriptors = JSON.parse(bridge.visualizableTestsJson());
  assert.deepEqual(descriptors, native);
  const options = scenarioOptions(descriptors, catalog);
  assert.equal(menuOptions(options, catalog.groups).flatMap(g => g.options).filter(o => o.workload).length, 22);
  // Formerly missing prediction configuration, including temporary fidelity.
  bridge.setProfileOverride('RV32I_PROGRAM_ROOT.CORE', 'behavioral');
  const name = 'RV32IPredictionProgramTest/two-bit/program-04', key = 'rv32i-predict-two-bit-program4';
  const actual = JSON.parse(bridge.loadScenario(name, key, 0));
  const expected = JSON.parse(execFileSync(probe, ['topology', name, key, '--override',
    'RV32I_PROGRAM_ROOT.CORE', 'behavioral'], {encoding: 'utf8', maxBuffer: 32 * 1024 * 1024}));
  for (const field of ['rootId', 'rootType', 'timestamps', 'checkpoints', 'performanceMetrics', 'state', 'stats'])
    assert.deepEqual(actual[field], expected[field], field);
  console.log(`${descriptors.length} descriptors match; all 22 programs configurable; new prediction scenario matches`);
} finally { bridge.delete(); }
