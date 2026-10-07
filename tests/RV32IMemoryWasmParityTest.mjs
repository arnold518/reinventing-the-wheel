// Focused portability check for both migrated program systems. Sequential only.
import assert from 'node:assert/strict';
import {execFileSync} from 'node:child_process';
import {readFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';

const [modulePath, wasmPath, probePath] = process.argv.slice(2);
const module = await (await import(pathToFileURL(resolve(modulePath)))).default({wasmBinary: await readFile(wasmPath)});
for (const stage of ['Fetch','Decode','Execute','Memory','Writeback','NextPC']) {
  const type = `RV32ISingleCycle${stage}Stage`, test = type+'Test', root = type+'_ROOT';
  for (const fidelity of ['structural','behavioral']) {
    const bridge = new module.WasmVisualizer();
    try {
      bridge.setProfileOverride(root,fidelity);
      const actual = JSON.parse(bridge.loadScenario(test,test,0));
      const expected = JSON.parse(execFileSync(probePath,['topology',test,test,'--override',root,fidelity],
        {encoding:'utf8',maxBuffer:64*1024*1024}));
      for (const key of ['rootId','rootType','timestamps','checkpoints','stats','components','pins','wires','state'])
        assert.deepEqual(actual[key],expected[key],`${stage}/${fidelity}/${key}`);
      if(stage==='Decode') for(const index of [0,actual.timestamps.length-1]) {
        const registers = JSON.parse(bridge.componentStateJson(index,root));
        const native = JSON.parse(execFileSync(probePath,['component',test,test,String(index),root,'--override',root,fidelity],
          {encoding:'utf8',maxBuffer:16*1024*1024}));
        assert.deepEqual(registers,native,`Decode register history/${fidelity}/${index}`);
      }
      console.log(`${stage}/${fidelity}: native/WASM stage contract and history match`);
    } finally { bridge.delete(); }
  }
}
for (const [family, prefix] of [['RV32ISingleCycleSystemTest', 'rv32i-program'],
                              ['RV32IFiveStageCoreProgramTest', 'rv32i-five-stage-program']]) {
  const direct = family === 'RV32ISingleCycleSystemTest';
  for (const fidelity of ['structural', 'behavioral']) {
    const scenario = prefix + '3', test = family + '/program-03';
    const native = (...args) => JSON.parse(execFileSync(probePath,
      [...args, '--override', 'RV32I_PROGRAM_ROOT.CORE', fidelity],
      {encoding: 'utf8', maxBuffer: 64 * 1024 * 1024}));
    const bridge = new module.WasmVisualizer();
    try {
      bridge.setProfileOverride('RV32I_PROGRAM_ROOT.CORE', fidelity);
      const topology = JSON.parse(bridge.loadScenario(test, scenario, 0));
      const expected = native('topology', test, scenario);
      for (const key of ['rootId', 'rootType', 'layoutKey', 'timestamps', 'checkpoints', 'stats', 'components', 'pins', 'wires', 'state'])
        assert.deepEqual(topology[key], expected[key], `${scenario}/${fidelity}/${key}`);
      for (const name of ['INSTRUCTION_MEMORY', 'DATA_MEMORY']) {
        const id = topology.rootId + '.' + name;
        assert.equal(topology.components.find(c => c.id === id).type, 'MemoryEndpoint<ff,32>');
        const pinNames = topology.pins.filter(p => p.componentId === id).map(p => p.name);
        for (const pin of ['REQ_VALID', 'REQ_READY', 'RSP_VALID', 'RSP_READY', 'RSP_STATUS'])
          assert(pinNames.includes(pin), `${id}/${pin}`);
        for (const index of new Set([0, topology.timestamps.length - 1])) {
          const state = JSON.parse(bridge.componentStateJson(index, id));
          assert.deepEqual(state, native('component', test, scenario, String(index), id));
          assert.equal(state.supported, true);
        }
      }

      assert.equal(topology.components.filter(c => c.id.startsWith(topology.rootId+'.') &&
        !c.id.slice(topology.rootId.length+1).includes('.')).length, 4);
      const core = topology.rootId + '.CORE';
      const corePins = topology.pins.filter(p => p.componentId === core).map(p => p.name);
      for(const prefix of ['IMEM_', 'DMEM_']) {
        for(const pin of ['REQ_VALID', 'REQ_READY', 'REQ_OP', 'REQ_ADDR', 'REQ_WRITE_DATA', 'REQ_WRITE_MASK',
                          'RSP_VALID', 'RSP_READY', 'RSP_STATUS', 'RSP_READ_DATA'])
          assert(corePins.includes(prefix+pin), prefix+pin);
        assert(!corePins.includes(prefix+'READY'));
      }
      assert(!topology.components.some(c => c.id.startsWith(core+'.') && c.type.startsWith('MemoryController<')));

      assert(topology.checkpoints.length > 0);
      console.log(`${scenario}/${fidelity}: native/WASM topology, timing, and memory inspector match`);
    } finally {bridge.delete();}
  }
}
