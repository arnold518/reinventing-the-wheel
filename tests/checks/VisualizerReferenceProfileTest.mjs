// Exercise the actual worker's load function: URL profiles must not mutate
// persisted preferences and must not change normal scenario loading.
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import vm from 'node:vm';

const source = await readFile(new URL('../../visualizer-v2/static/wasm_worker.js', import.meta.url), 'utf8');
const start = source.indexOf('async function loadScenario(');
const end = source.indexOf('\nfunction parseBody', start);
assert(start >= 0 && end > start);
const saved = { schema_version: 1, scenarios: { 'rv32i-program9': {
  revision: 4, exact_overrides: { 'RV32I_PROGRAM_ROOT.CORE': 'structural' },
} } };
const before = JSON.stringify(saved);
let selected = {};
const sandbox = {
  normalizeScenario: key => ({ key, name: 'RV32ISingleCycleSystemTest/program-09' }),
  profileStore: async () => saved,
  bridge: {
    clearProfileOverrides: () => { selected = {}; },
    setProfileOverride: (path, fidelity) => { selected[path] = fidelity; },
    loadScenario: (name, key, revision) => JSON.stringify({ revision, overrides: selected }),
  },
  defaultLayout: {}, LAYOUT_KEY: 'layout', loadStored: async () => ({}),
  mergeLayoutStores: () => ({}), enrichPayload: payload => payload,
};
vm.createContext(sandbox);
vm.runInContext('let currentLayout, currentScenario, currentPayload;\n' + source.slice(start, end), sandbox);
let result = await sandbox.loadScenario('rv32i-program9', null, null, 'behavioral');
assert.equal(result.overrides['RV32I_PROGRAM_ROOT.CORE'], 'behavioral');
assert.equal(result.revision, 4);
assert.equal(JSON.stringify(saved), before);
result = await sandbox.loadScenario('rv32i-program9');
assert.equal(result.overrides['RV32I_PROGRAM_ROOT.CORE'], 'structural');
await assert.rejects(sandbox.loadScenario('adder8', null, null, 'behavioral'), /core_fidelity/);
await assert.rejects(sandbox.loadScenario('rv32i-program9', null, null, 'automatic'), /core_fidelity/);
console.log('PASS: temporary core fidelity, normal reload, validation, and saved profile preservation');

// A compact parent hides its descendants, but must not discard their saved
// fidelity choices. Exercise the actual worker apply route with a hidden word.
const handleStart = source.indexOf('async function handleRequest(');
const handleEnd = source.indexOf('\nself.addEventListener', handleStart);
const root = 'MEMORY_CONTAINER_ROOT';
const word = `${root}.STORAGE.WORD_0`;
const profiles = {schema_version: 1, scenarios: {'ff-memory-container': {
  revision: 4, exact_overrides: {[root]: 'behavioral', [word]: 'behavioral'},
}}};
const applySandbox = {
  URLSearchParams, initialize: async () => {},
  parseBody: message => JSON.parse(message.body),
  normalizeScenario: key => ({key}),
  profileStore: async () => profiles, localProfileStore: async () => structuredClone(profiles),
  currentPayload: {components: [{id: root, availableFidelities: ['structural', 'behavioral']}]},
  clone: structuredClone, PROFILE_KEY: 'profile',
  loadScenario: async (key, overrides, revision) => ({key, overrides, revision}),
  saveStored: async (_key, store) => { profiles.scenarios = store.scenarios; },
};
vm.createContext(applySandbox);
vm.runInContext(source.slice(handleStart, handleEnd), applySandbox);
const apply = (overrides, revision = 4) => applySandbox.handleRequest({
  method: 'POST', path: '/api/circuit/apply',
  body: JSON.stringify({scenario: 'ff-memory-container', revision, exactOverrides: overrides}),
});
await assert.rejects(apply({[root]: 'structural', [word]: 'structural'}), /unknown component/);
await assert.rejects(apply({[root]: 'automatic', [word]: 'behavioral'}), /does not provide/);
await assert.rejects(apply({[root]: 'structural', [`${word}.NEW`]: 'behavioral'}), /unknown component/);
const restored = JSON.parse(await apply({[root]: 'structural', [word]: 'behavioral'}));
assert.equal(restored.revision, 5);
assert.equal(restored.overrides[word], 'behavioral');
assert.equal(profiles.scenarios['ff-memory-container'].exact_overrides[word], 'behavioral');
await assert.rejects(apply({[root]: 'behavioral'}), /Profile changed/);
console.log('PASS: retained inactive descendants, invalid new overrides, and stale profile rejection');
