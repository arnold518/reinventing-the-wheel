import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {execFileSync} from 'node:child_process';
import vm from 'node:vm';
import {upgradeDefaultLayout} from '../../visualizer-v2/static/layout_migration.mjs';

const source = readFileSync(new URL('../../visualizer-v2/static/wasm_worker.js', import.meta.url), 'utf8');
const context = vm.createContext({});
vm.runInContext(source.slice(source.indexOf('function migrateProfileOverrides('), source.indexOf('function normalizeProfileStore(')), context);
const oldDevices = {
  'SYNCHRONOUS_FIFO_ROOT.HEAD': 'structural',
  'SYNCHRONOUS_FIFO_ROOT.SLOT_2.BIT_0': 'structural',
  'CONSOLE_DEVICE_ROOT.TX_FIFO.COUNT_STATE': 'structural',
  'CONSOLE_DEVICE_ROOT.PORT.OP': 'structural',
  'DEVICE_REGISTER_PORT_W32_ROOT.PENDING': 'structural',
  'ADDRESS_ROUTER_W32_ROOT.OWNER_2': 'structural',
  'RV32IM_DEVICE_SYSTEM_ROOT.FETCH_GUARD.HELD_ADDR': 'structural',
  'RV32IM_DEVICE_SYSTEM_ROOT.TIMER.COUNTER.DIVIDER': 'structural',
  'RV32IM_DEVICE_SYSTEM_ROOT.DISPLAY.FRAME_COUNTER': 'structural',
  'FRAMEBUFFER_DISPLAY_ROOT.FRAME_COUNTER': 'structural',
  'CLOCKED_COUNTER_ROOT.DIVIDER': 'structural',
  'SYNCHRONOUS_FIFO_ROOT.POINTERS.HEAD': 'behavioral',
  'UNRELATED.HEAD': 'structural',
};
const groupedDevices = {
  'SYNCHRONOUS_FIFO_ROOT.POINTERS.HEAD': 'behavioral',
  'SYNCHRONOUS_FIFO_ROOT.STORAGE.SLOT_2.BIT_0': 'structural',
  'CONSOLE_DEVICE_ROOT.TX_FIFO.CONTROL.COUNT_STATE': 'structural',
  'CONSOLE_DEVICE_ROOT.PORT.REQUEST.OP': 'structural',
  'DEVICE_REGISTER_PORT_W32_ROOT.CONTROL.PENDING': 'structural',
  'ADDRESS_ROUTER_W32_ROOT.REQUEST.OWNER_2': 'structural',
  'RV32IM_DEVICE_SYSTEM_ROOT.FETCH_GUARD.REQUEST.HELD_ADDR': 'structural',
  'RV32IM_DEVICE_SYSTEM_ROOT.TIMER.COUNTER.TICK_CONTROL.PHASE': 'structural',
  'RV32IM_DEVICE_SYSTEM_ROOT.DISPLAY.FRAME_CONTROL.FRAME_COUNTER': 'structural',
  'FRAMEBUFFER_DISPLAY_ROOT.FRAME_CONTROL.FRAME_COUNTER': 'structural',
  'CLOCKED_COUNTER_ROOT.TICK_CONTROL.PHASE': 'structural',
  'UNRELATED.HEAD': 'structural',
};
assert.deepEqual(JSON.parse(JSON.stringify(context.migrateProfileOverrides(oldDevices))),groupedDevices);
assert.deepEqual(JSON.parse(JSON.stringify(context.migrateProfileOverrides(groupedDevices))),groupedDevices);
assert.deepEqual(JSON.parse(execFileSync('/usr/bin/python3',['-c',
  'import sys,json;sys.path.insert(0,"visualizer-v2");from profile_store import migrate_component_paths;print(json.dumps(migrate_component_paths(json.loads(sys.argv[1]))))',JSON.stringify(oldDevices)],
  {cwd:new URL('../..',import.meta.url),timeout:10000})),groupedDevices);
const deviceLayouts=JSON.parse(readFileSync(new URL('../../visualizer-v2/layout.json',import.meta.url)));
let deviceStyles=0;
for(const store of [deviceLayouts.type_layouts,deviceLayouts.root_layouts,...Object.values(deviceLayouts.profile_layouts)]) {
  for(const entry of Object.values(store)) {
    if(entry.layout_revision!=='device-io-groups-v1'||!entry.previous_default)continue;
    const saved={...structuredClone(entry.previous_default),color:[12,34,56,190]};
    const migrated=upgradeDefaultLayout(entry,saved);
    for(const key of ['aspect_ratio','min_aspect_ratio','title_bar_ratio','pin_size_ratio','boundary_area_ratio'])
      if(entry[key]!==undefined)assert.equal(migrated[key],entry[key]);
    assert.deepEqual(migrated.color,saved.color);
    assert.equal(upgradeDefaultLayout(entry,{...saved,aspect_ratio:9.876}).aspect_ratio,9.876);
    assert.equal(upgradeDefaultLayout(entry,{...saved,layout_mode:'manual'}).layout_mode,'manual');
    deviceStyles++;
  }
}
assert(deviceStyles>20,deviceStyles);
const legacy = {
  'RV32I_SINGLE_CYCLE_SYSTEM_ROOT.CORE.REGISTER_FILE.X7': 'behavioral',
  'RV32I_PROGRAM_ROOT.CORE.REGISTER_FILE.X7.BIT_31.STATE': 'behavioral',
  'REGISTER_FILE_W32_N32_ROOT.X1': 'behavioral',
  'REGISTER_FILE_W32_N32_ROOT.X1.BIT_0': 'behavioral',
  'REGISTER_FILE_W32_N32_ROOT.STORAGE.WORD_0': 'structural',
  'UNRELATED.X7': 'behavioral',
  'REGISTER32_ROOT.BIT_31.STATE': 'behavioral',
  'REGISTER32_ROOT.BIT_0': 'behavioral',
  'REGISTER32_ROOT.STORAGE': 'behavioral',
  'REGISTER32_ROOT.STORAGE.CELL_0': 'structural',
  'REGISTER32_ROOT.STORAGE.CELL_1': 'structural',
  'REGISTER32_ROOT.STORAGE.OUTPUT': 'structural',
  'REGISTER32_ROOT.D_SPLIT': 'structural',
  'RV32I_SINGLE_CYCLE_SYSTEM_ROOT.CORE.CONTROL_FLOW.PC_STATE.BIT_7': 'behavioral',
  'RV32I_PROGRAM_ROOT.CORE.IF_ID.INSTRUCTION_STATE.BIT_5.STATE': 'behavioral',
  'RV32I_PROGRAM_ROOT.CORE.FETCH.REQUEST.REQUEST_DATA.BIT_1': 'behavioral',
  'UNRELATED.BIT_0': 'behavioral',
};
const expected = {
  'RV32I_PROGRAM_ROOT.CORE.DECODE.REGISTER_FILE.STORAGE.WORD_6': 'behavioral',
  'RV32I_PROGRAM_ROOT.CORE.DECODE.REGISTER_FILE.STORAGE.WORD_6.BIT_31.STATE': 'behavioral',
  'REGISTER_FILE_W32_N32_ROOT.STORAGE.WORD_0': 'structural',
  'REGISTER_FILE_W32_N32_ROOT.STORAGE.WORD_0.BIT_0': 'behavioral',
  'UNRELATED.X7': 'behavioral',
  'REGISTER32_ROOT.BIT_31.STATE': 'behavioral',
  'REGISTER32_ROOT': 'behavioral',
  'REGISTER32_ROOT.BIT_0': 'behavioral',
  'REGISTER32_ROOT.BIT_1': 'structural',
  'REGISTER32_ROOT.INPUT_BITS': 'structural',
  'REGISTER32_ROOT.OUTPUT_BITS': 'structural',
  'RV32I_PROGRAM_ROOT.CORE.FETCH.PC_STATE.BIT_7': 'behavioral',
  'RV32I_PROGRAM_ROOT.CORE.IF_ID.INSTRUCTION_STATE.BIT_5.STATE': 'behavioral',
  'RV32I_PROGRAM_ROOT.CORE.FETCH.REQUEST.REQUEST_DATA.BIT_1': 'behavioral',
  'UNRELATED.BIT_0': 'behavioral',
};
// Stage and register refactoring must preserve descendant overrides, migrate
// the former wrapper children, and leave current/pipeline paths alone.
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
  {cwd:new URL('../..',import.meta.url),timeout:10000}));
assert.deepEqual(native, expected);
const oldArithmetic = {
  'MULTIPLIER_W32_ROOT.BUSY_STATE': 'structural',
  'MULTIPLIER_W32_ROOT.MULTIPLICAND.BIT_2': 'structural',
  'MULTIPLIER_W32_ROOT.STEP': 'structural',
  'RV32IM_FIVE_STAGE_CORE_ROOT.EXECUTE.MULDIV.DIVIDE.DIVISOR': 'structural',
  'RV32IM_PROGRAM_ROOT.CORE.EXECUTE.MULDIV.MULTIPLY.LOW_OR_QUOTIENT': 'structural',
  'DIVIDER_W32_ROOT.DONE_STATE': 'structural',
  'DIVIDER_W32_ROOT.CONTROL.DONE_STATE': 'behavioral',
  'UNRELATED.DIVISOR': 'structural',
};
const groupedArithmetic = {
  'MULTIPLIER_W32_ROOT.CONTROL.BUSY_STATE': 'structural',
  'MULTIPLIER_W32_ROOT.OPERANDS.MULTIPLICAND.BIT_2': 'structural',
  'MULTIPLIER_W32_ROOT.DATAPATH.STEP': 'structural',
  'RV32IM_FIVE_STAGE_CORE_ROOT.EXECUTE.MULDIV.DIVIDE.OPERANDS.DIVISOR': 'structural',
  'RV32IM_PROGRAM_ROOT.CORE.EXECUTE.MULDIV.MULTIPLY.DATAPATH.LOW_OR_QUOTIENT': 'structural',
  'DIVIDER_W32_ROOT.CONTROL.DONE_STATE': 'behavioral',
  'UNRELATED.DIVISOR': 'structural',
};
assert.deepEqual(JSON.parse(JSON.stringify(context.migrateProfileOverrides(oldArithmetic))), groupedArithmetic);
assert.deepEqual(JSON.parse(execFileSync('/usr/bin/python3', ['-c',
  'import sys,json;sys.path.insert(0,"visualizer-v2");from profile_store import migrate_component_paths;print(json.dumps(migrate_component_paths(json.loads(sys.argv[1]))))', JSON.stringify(oldArithmetic)],
  {cwd:new URL('../..',import.meta.url),timeout:10000})), groupedArithmetic);
// Renamed component scenarios retain saved root and descendant choices.
const catalog = JSON.parse(readFileSync(new URL('../../visualizer-v2/static/scenarios.json',import.meta.url)));
for (const [test, entry] of Object.entries(catalog.components)) {
  if (!entry.root) continue;
  const alias = catalog.aliases[test][0];
  const saved = {DUT: 'behavioral', 'DUT.CHILD': 'structural', [entry.root]: 'structural',
    'UNRELATED.CHILD': 'behavioral'};
  const expectedRoot = {[entry.root]: 'structural', [entry.root + '.CHILD']: 'structural',
    'UNRELATED.CHILD': 'behavioral'};
  assert.deepEqual(JSON.parse(JSON.stringify(context.migrateProfileOverrides(saved, entry.root))), expectedRoot);
  const migrated = JSON.parse(execFileSync('/usr/bin/python3', ['-c',
    'import sys,json;sys.path.insert(0,"visualizer-v2");from profile_store import migrate_component_paths;print(json.dumps(migrate_component_paths(json.loads(sys.argv[1]),sys.argv[2])))', JSON.stringify(saved), alias],
    {cwd:new URL('../..',import.meta.url),timeout:10000}));
  assert.deepEqual(migrated, expectedRoot, alias);
}
const layout = JSON.parse(readFileSync(new URL('../../visualizer-v2/layout.json',import.meta.url)));
const mCore = layout.type_layouts.RV32IMFiveStageCore;
const formerCore = {children:structuredClone(mCore.automatic_layout_history.at(-1).children)};
assert.deepEqual(upgradeDefaultLayout(mCore, formerCore).children.EXECUTE, mCore.children.EXECUTE);
for (const [key, overlay] of Object.entries(layout.root_layouts)) {
  if (key.split('@')[0] !== 'rv32im-core' || !overlay.automatic_layout_revision) continue;
  const base = {...mCore,...overlay,children:{...mCore.children,...overlay.children}};
  const former = {children:structuredClone(mCore.automatic_layout_history.at(-1).children)};
  assert.deepEqual(upgradeDefaultLayout(base, former).children.EXECUTE, base.children.EXECUTE, key);
}
formerCore.children.EXECUTE = {rel_pos:[.42,.21],rel_width:.12};
assert.deepEqual(upgradeDefaultLayout(mCore, formerCore).children.EXECUTE, formerCore.children.EXECUTE);
const muldiv = layout.type_layouts['MulDivUnit<32>'];
assert.equal(muldiv.layout_revision, 'rv32im-groups-v1');
const formerMuldiv = structuredClone(muldiv.previous_default);
const upgradedMuldiv = upgradeDefaultLayout(muldiv, formerMuldiv);
assert.equal(upgradedMuldiv.aspect_ratio, muldiv.aspect_ratio);
assert.deepEqual(upgradedMuldiv.children.MULTIPLY, muldiv.children.MULTIPLY);
formerMuldiv.aspect_ratio = 3.1415;
formerMuldiv.children.MULTIPLY = {rel_pos:[.12,.34],rel_width:.2};
const customMuldiv = upgradeDefaultLayout(muldiv, formerMuldiv);
assert.equal(customMuldiv.aspect_ratio, 3.1415);
assert.deepEqual(customMuldiv.children.MULTIPLY, formerMuldiv.children.MULTIPLY);
formerMuldiv.layout_mode = 'manual';
assert.deepEqual(upgradeDefaultLayout(muldiv, formerMuldiv), formerMuldiv);
const executeStage = layout.type_layouts.RV32IMExecuteStage;
const formerExecute = structuredClone(executeStage.previous_default);
const upgradedExecute = upgradeDefaultLayout(executeStage, formerExecute);
assert.equal(upgradedExecute.aspect_ratio, .95);
assert.equal(upgradedExecute.pin_size_ratio, .018);
assert.deepEqual(upgradedExecute.children.MULDIV, executeStage.children.MULDIV);
formerExecute.children.MULDIV = {rel_pos:[.1,.2],rel_width:.1};
assert.deepEqual(upgradeDefaultLayout(executeStage, formerExecute).children.MULDIV, formerExecute.children.MULDIV);
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
