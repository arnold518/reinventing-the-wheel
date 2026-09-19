// Export only scenarios requested by documentation, from the published WASM
// engine. Expectations come from C++ test actions, never from observed output.
import assert from 'node:assert/strict';
import { readFile, readdir, writeFile, rename } from 'node:fs/promises';
import { resolve, join } from 'node:path';
import { pathToFileURL } from 'node:url';

const [modulePath, wasmPath, docsPath, outputPath, filterText, previousPath] = process.argv.slice(2);
// Focused regeneration preserves older verified exports without rerunning their
// scenarios. A missing previous bundle is an error, never a partial replacement.
const filter = filterText ? new RegExp(filterText) : null;
const previous = filter ? JSON.parse(await readFile(previousPath || outputPath, 'utf8')) : {};
if (!outputPath) throw new Error('Usage: node generate-example-rows.mjs <module.js> <module.wasm> <docs-dir> <output.json>');
const requested = new Map();
const requestedPrograms = new Map();
async function discover(directory) {
  for (const entry of (await readdir(directory, { withFileTypes: true })).sort((a, b) => a.name.localeCompare(b.name))) {
    const path = join(directory, entry.name);
    if (entry.isDirectory()) await discover(path);
    else if (entry.name.endsWith('.md')) {
      const text = await readFile(path, 'utf8');
      for (const [, scenario, test] of text.matchAll(/<!-- simulator-examples: ([a-z0-9-]+) ([A-Za-z0-9_]+) -->/g)) {
        assert(!requested.has(scenario) || requested.get(scenario) === test, `Conflicting test for ${scenario}`);
        requested.set(scenario, test);
      }
      for (const [, scenario, test] of text.matchAll(/<!-- simulator-program: ([a-z0-9-]+) ([A-Za-z0-9_/-]+) -->/g)) {
        assert(!requestedPrograms.has(scenario) || requestedPrograms.get(scenario) === test);
        requestedPrograms.set(scenario, test);
      }
    }
  }
}
await discover(resolve(docsPath));
assert(requested.size > 0, 'No documentation example scenarios found');
const imported = await import(pathToFileURL(resolve(modulePath)));
const module = await imported.default({ wasmBinary: await readFile(wasmPath) });
const scenarios = filter ? { ...previous.scenarios } : {};
for (const [scenario, test] of requested) {
  if (filter && !filter.test(test)) continue;
  const bridge = new module.WasmVisualizer();
  try {
    const topology = JSON.parse(bridge.loadScenario(test, scenario, 0));
    const examples = JSON.parse(bridge.exampleRowsJson());
    assert.equal(examples.schemaVersion, 1);
    assert.equal(examples.profileFingerprint, topology.profileFingerprint);
    assert.equal(examples.rows.length, topology.checkpoints.length);
    const pins = new Map(topology.pins.filter(pin => pin.componentId === topology.rootId).map(pin => [pin.name, pin]));
    // Independently verify every exported value/time against the same recorded
    // timeline that the browser will display, not only the final checkpoint.
    for (const [index, row] of examples.rows.entries()) {
      const checkpoint = topology.checkpoints[index];
      assert.deepEqual([row.rowIndex, row.checkpoint, row.time, row.timelineIndex],
        [checkpoint.rowIndex, checkpoint.label, checkpoint.time, checkpoint.index]);
      const state = JSON.parse(bridge.stateJson(row.timelineIndex));
      assert.equal(state.time, row.time);
      for (const [name, value] of Object.entries({ ...row.inputs, ...row.expectedOutputs })) {
        const pin = pins.get(name);
        assert(pin, `${scenario}/${row.checkpoint}: missing ${name}`);
        assert.equal(state.pins[pin.stateIndex], value, `${scenario}/${row.checkpoint}/${name}`);
      }
    }
    scenarios[scenario] = examples;
    console.log(`${scenario}: ${examples.rows.length} verified rows (${examples.profileFingerprint})`);
  } finally { bridge.delete(); }
}
const programs = filter ? { ...previous.programs } : {};
for (const [scenario, test] of requestedPrograms) {
  if (filter && !filter.test(test)) continue;
  const bridge = new module.WasmVisualizer();
  try {
    bridge.setProfileOverride('RV32I_PROGRAM_ROOT.CORE', 'behavioral');
    const topology = JSON.parse(bridge.loadScenario(test, scenario, 0));
    const core = topology.components.find(c => c.id === 'RV32I_PROGRAM_ROOT.CORE');
    assert.equal(core?.fidelity, 'behavioral');
    assert(topology.checkpoints.length > 0);
    const pins = new Map(topology.pins.filter(p => p.componentId === topology.rootId).map(p => [p.name, p]));
    for (const [index, checkpoint] of topology.checkpoints.entries()) {
      assert.equal(checkpoint.rowIndex, index);
      assert(checkpoint.label.startsWith(`I${index} `));
      const state = JSON.parse(bridge.stateJson(checkpoint.index));
      assert.equal(state.time, checkpoint.time);
      const pc = checkpoint.detail.match(/pc_after=0x([0-9a-f]{8})/i);
      assert(pc, `${scenario}: missing expected PC`);
      const expected = { PC: parseInt(pc[1], 16).toString(2).padStart(32, '0'),
        HALTED: checkpoint.detail.includes('halted=true') ? '1' : '0',
        TRAPPED: checkpoint.detail.includes('trapped=true') ? '1' : '0' };
      for (const [name, value] of Object.entries(expected)) {
        assert.equal(state.pins[pins.get(name).stateIndex], value, `${scenario}/${checkpoint.label}/${name}`);
      }
    }
    programs[scenario] = { scenario, registeredTest: test, coreFidelity: core.fidelity,
      checkpoints: topology.checkpoints };
    console.log(`${scenario}: ${topology.checkpoints.length} reference checkpoints (PC/status verified)`);
  } finally { bridge.delete(); }
}
// An unsuccessful generation must not replace the last working preview data.
const destination = resolve(outputPath);
const temporary = `${destination}.tmp-${process.pid}`;
await writeFile(temporary, JSON.stringify({ schemaVersion: 1, scenarios, programs }, null, 2) + '\n');
await rename(temporary, destination);
