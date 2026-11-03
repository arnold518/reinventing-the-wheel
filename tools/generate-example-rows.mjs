// Export only scenarios requested by documentation, from the published WASM
// engine. Expectations come from C++ test actions, never from observed output.
import assert from 'node:assert/strict';
import { readFile, readdir, writeFile, rename, mkdtemp, rm } from 'node:fs/promises';
import { resolve, join, dirname } from 'node:path';
import { pathToFileURL, fileURLToPath } from 'node:url';
import { spawn } from 'node:child_process';

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
// Compact circuit programs and WASM linear memory retain their high-water
// allocation. A constrained build can regenerate every requested group in a
// fresh process, retaining the same row/state assertions in each child.
if (process.env.CIRCUITSIM_EXAMPLES_ISOLATE === '1') {
  const destination = resolve(outputPath);
  const temporaryDirectory = await mkdtemp(join(dirname(destination), '.example-rows-'));
  const staged = join(temporaryDirectory, 'rows.json');
  try {
    await writeFile(staged, JSON.stringify(filter ? previous : {schemaVersion:1,scenarios:{},programs:{}}));
    const selected = [...new Set([...requested, ...requestedPrograms]
      .filter(([, test]) => !filter || filter.test(test)).map(([, test]) => test))];
    for (const test of selected) {
      const pattern = '^' + test.replace(/[.*+?^{}$()|[\]\\]/g, '\\$&') + '$';
      await new Promise((accept, reject) => {
        const child = spawn(process.execPath, [fileURLToPath(import.meta.url),
          modulePath, wasmPath, docsPath, staged, pattern, staged], {
          stdio:'inherit', env:{...process.env,CIRCUITSIM_EXAMPLES_ISOLATE:'0'},
        });
        child.once('error', reject);
        child.once('exit', (code, signal) => code === 0 ? accept() :
          reject(new Error('Example group ' + test + ' failed: ' + (signal || code))));
      });
    }
    const completed = JSON.parse(await readFile(staged, 'utf8'));
    const scenarios = filter ? {...previous.scenarios} : {};
    const programs = filter ? {...previous.programs} : {};
    for (const [source, target, exported] of [
      [requested, scenarios, completed.scenarios], [requestedPrograms, programs, completed.programs],
    ]) {
      for (const [alias, test] of source) {
        if (filter && !filter.test(test)) continue;
        assert(Object.hasOwn(exported, alias), 'Missing isolated export: ' + alias);
        target[alias] = exported[alias];
      }
    }
    await writeFile(staged, JSON.stringify({schemaVersion:1,scenarios,programs}, null, 2) + '\n');
    await rename(staged, destination);
    console.log('Verified ' + selected.length + ' example groups in separate processes');
  } finally {
    await rm(temporaryDirectory, {recursive:true,force:true});
  }
  process.exit(0);
}
const imported = await import(pathToFileURL(resolve(modulePath)));
const module = await imported.default({ wasmBinary: await readFile(wasmPath) });
const scenarios = filter ? { ...previous.scenarios } : {};
// Rows are recorded under the profile the embedded simulator opens the
// scenario in (its catalog `fidelity`), so table and simulator agree.
const catalog = JSON.parse(await readFile(new URL('../visualizer-v2/static/scenarios.json', import.meta.url), 'utf8'));
for (const [scenario, test] of requested) {
  if (filter && !filter.test(test)) continue;
  const bridge = new module.WasmVisualizer();
  try {
    const fidelity = catalog.components[test]?.fidelity;
    if (fidelity) bridge.setFidelityProfile(fidelity);
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
    const recordedM = scenario.startsWith('rv32im-program');
    const rootName = recordedM ? 'RV32IM_PROGRAM_ROOT' : 'RV32I_PROGRAM_ROOT';
    bridge.setProfileOverride(`${rootName}.CORE`, 'behavioral');
    const topology = JSON.parse(bridge.loadScenario(test, scenario, 0));
    const core = topology.components.find(c => c.id === `${rootName}.CORE`);
    assert.equal(core?.fidelity, 'behavioral');
    assert(topology.checkpoints.length > 0);
    if (recordedM) {
      const image = JSON.parse(bridge.componentStateJson(topology.checkpoints.at(-1).index, `${rootName}.INSTRUCTION_MEMORY`));
      assert.equal(image.memoryRole, 'instruction');
      const names = ['mul','mulh','mulhsu','mulhu','div','divu','rem','remu'];
      let decodedM = 0;
      for (const row of image.windowWords) {
        const word = Number.parseInt(row.hex, 16);
        if ((word & 0xfe00007f) !== 0x02000033) continue;
        assert.equal(row.instructionText, `${names[word >>> 12 & 7]} x${word >>> 7 & 31}, x${word >>> 15 & 31}, x${word >>> 20 & 31}`);
        ++decodedM;
      }
      assert(decodedM > 0, `${scenario}: instruction-memory view has no decoded M instructions`);
    }
    const pins = new Map(topology.pins.filter(p => p.componentId === topology.rootId).map(p => [p.name, p]));
    for (const [index, checkpoint] of topology.checkpoints.entries()) {
      assert.equal(checkpoint.rowIndex, index);
      if (!recordedM) assert(checkpoint.label.startsWith(`I${index} `));
      else assert(checkpoint.label === 'reset' || checkpoint.label === 'arithmetic-wait' ||
        checkpoint.label === 'halted' || checkpoint.label.startsWith('commit-'));
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
    console.log(`${scenario}: ${topology.checkpoints.length} program checkpoints (PC/status verified)`);
  } finally { bridge.delete(); }
}
// An unsuccessful generation must not replace the last working preview data.
const destination = resolve(outputPath);
const temporary = `${destination}.tmp-${process.pid}`;
await writeFile(temporary, JSON.stringify({ schemaVersion: 1, scenarios, programs }, null, 2) + '\n');
await rename(temporary, destination);
