import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {execFileSync} from 'node:child_process';
import vm from 'node:vm';
import {upgradeDefaultLayout} from '../visualizer-v2/static/layout_migration.mjs';

const source = readFileSync(new URL('../visualizer-v2/static/wasm_worker.js', import.meta.url), 'utf8');
const context = vm.createContext({});
vm.runInContext(source.slice(source.indexOf('function migrateProfileOverrides('), source.indexOf('function normalizeProfileStore(')), context);
const legacy = {
  'RV32I_SINGLE_CYCLE_SYSTEM_ROOT.CORE.REGISTER_FILE.X7': 'behavioral',
  'RV32I_PROGRAM_ROOT.CORE.REGISTER_FILE.X7.BIT_31.STATE': 'behavioral',
  'REGISTER_FILE32X32_ROOT.X1': 'behavioral',
  'REGISTER_FILE32X32_ROOT.X1.BIT_0': 'behavioral',
  'REGISTER_FILE32X32_ROOT.STORAGE.WORD_0': 'structural',
  'UNRELATED.X7': 'behavioral',
  'REGISTER32_ROOT.BIT_31.STATE': 'behavioral',
  'REGISTER32_ROOT.BIT_0': 'behavioral',
  'REGISTER32_ROOT.STORAGE.CELL_0': 'structural',
  'REGISTER32_ROOT.D_SPLIT': 'structural',
  'RV32I_SINGLE_CYCLE_SYSTEM_ROOT.CORE.CONTROL_FLOW.PC_STATE.BIT_7': 'behavioral',
  'RV32I_PROGRAM_ROOT.CORE.IF_ID.INSTRUCTION_STATE.BIT_5.STATE': 'behavioral',
  'RV32I_PROGRAM_ROOT.CORE.FETCH.REQUEST.REQUEST_DATA.BIT_1': 'behavioral',
  'UNRELATED.BIT_0': 'behavioral',
};
const expected = {
  'RV32I_PROGRAM_ROOT.CORE.DECODE.REGISTER_FILE.STORAGE.WORD_6': 'behavioral',
  'RV32I_PROGRAM_ROOT.CORE.DECODE.REGISTER_FILE.STORAGE.WORD_6.CELL_31.STATE': 'behavioral',
  'REGISTER_FILE32X32_ROOT.STORAGE.WORD_0': 'structural',
  'REGISTER_FILE32X32_ROOT.STORAGE.WORD_0.CELL_0': 'behavioral',
  'UNRELATED.X7': 'behavioral',
  'REGISTER32_ROOT.STORAGE.CELL_31.STATE': 'behavioral',
  'REGISTER32_ROOT.STORAGE.CELL_0': 'structural',
  'REGISTER32_ROOT.STORAGE.DATA': 'structural',
  'RV32I_PROGRAM_ROOT.CORE.FETCH.PC_STATE.STORAGE.CELL_7': 'behavioral',
  'RV32I_PROGRAM_ROOT.CORE.IF_ID.INSTRUCTION_STATE.STORAGE.CELL_5.STATE': 'behavioral',
  'RV32I_PROGRAM_ROOT.CORE.FETCH.REQUEST.REQUEST_DATA.BIT_1': 'behavioral',
  'UNRELATED.BIT_0': 'behavioral',
};
// Stage refactoring must migrate both program and standalone-core selections,
// preserve descendant overrides, and leave already current/pipeline paths alone.
for (const prefix of ['RV32I_PROGRAM_ROOT.CORE.', 'RV32I_SINGLE_CYCLE_CORE_ROOT.']) {
  for (const [old, current] of [
    ['DECODE_CONTROL', 'DECODE.CONTROL'], ['ALU', 'EXECUTE.ALU'],
    ['ALU_A_MUX', 'EXECUTE.OPERAND_A'], ['ALU_B_MUX', 'EXECUTE.OPERAND_B'],
    ['WRITEBACK_MUX', 'WRITEBACK.RESULT_SELECT'], ['FETCH_REQUEST', 'FETCH.REQUEST'],
    ['DATA_REQUEST', 'MEMORY.REQUEST'], ['LOAD_STORE', 'MEMORY.LANES'],
    ['CONTROL_FLOW', 'FETCH'], ['CONTROL_FLOW.PC_IMM_ADD', 'EXECUTE.BRANCH_TARGET_ADD'],
    ['CONTROL_FLOW.RS1_IMM_ADD', 'EXECUTE.ALU.ARITHMETIC'], ['CONTROL_FLOW.JALR_MASK', 'EXECUTE.JALR_MASK'],
  ]) {
    legacy[prefix + old] = 'behavioral';
    expected[prefix + current] = 'behavioral';
  }
  legacy[prefix + 'ALU.ARITHMETIC.SUB'] = 'structural';
  expected[prefix + 'EXECUTE.ALU.ARITHMETIC.SUB'] = 'structural';
  // Current path wins over the old ALU selection regardless of insertion order.
  legacy[prefix + 'EXECUTE.ALU'] = 'structural';
  expected[prefix + 'EXECUTE.ALU'] = 'structural';
  legacy[prefix + 'EXECUTE.CONTROL_FLOW'] = 'structural';
  expected[prefix + 'EXECUTE.CONTROL_FLOW'] = 'structural';
}
assert.deepEqual(JSON.parse(JSON.stringify(context.migrateProfileOverrides(legacy))), expected);
const native = JSON.parse(execFileSync('/usr/bin/python3', ['-c',
  'import sys,json;sys.path.insert(0,"visualizer-v2");from profile_store import migrate_component_paths;print(json.dumps(migrate_component_paths(json.loads(sys.argv[1]))))', JSON.stringify(legacy)],
  {cwd:new URL('..',import.meta.url),timeout:10000}));
assert.deepEqual(native, expected);
const layout = JSON.parse(readFileSync(new URL('../visualizer-v2/layout.json',import.meta.url)));
for(const store of [layout.type_layouts,layout.root_layouts,...Object.values(layout.profile_layouts)]) {
  for(const base of Object.values(store)) {
    if(base.layout_revision !== 'register-file-ff-v1') continue;
    const old = structuredClone(base.previous_default);
    assert.equal(upgradeDefaultLayout(base,old).aspect_ratio,base.aspect_ratio);
    old.aspect_ratio=7.654;
    old.children={...old.children,STORAGE:{rel_pos:[.1,.2],rel_width:.3}};
    const kept=upgradeDefaultLayout(base,old);
    assert.equal(kept.aspect_ratio,7.654);
    assert.deepEqual(kept.children.STORAGE,old.children.STORAGE);
  }
}
console.log('Saved register/cell fidelity migration agrees in native/WASM; canonical choices and custom layouts retained.');
