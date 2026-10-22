import assert from 'node:assert/strict';
import {execFileSync} from 'node:child_process';
import {createHash} from 'node:crypto';
import {mkdir,readFile,writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {fileURLToPath,pathToFileURL} from 'node:url';
const [js,wasm,probe]=process.argv.slice(2);
const guests=['bringup','2048','sokoban','snake','pong','tetris','2048-merge','tetris-line-clear','pong-rally','snake-wall'];
// An acceptance campaign already runs native repetitions, fidelity and quantum
// comparisons. Reuse its immutable baseline only when its complete provenance
// matches this checkout and binary; an invalid cache is a failure, not fallback.
const recordingDirectory=process.env.CIRCUITSIM_NATIVE_GUEST_RECORDINGS;
let provenance;
const sha=value=>createHash('sha256').update(value).digest('hex');
if(recordingDirectory){
  const root=fileURLToPath(new URL('../../',import.meta.url));
  const files=[...new Set(execFileSync('git',['ls-files','-co','--exclude-standard'],{cwd:root,encoding:'utf8'}).trim().split('\n'))]
    .filter(name=>name==='CMakeLists.txt'||['include/','src/','tools/rv32im/','tools/build/'].some(prefix=>name.startsWith(prefix))).sort();
  const sourceSha256={};
  for(const name of files)sourceSha256[name]=sha(await readFile(resolve(root,name)));
  const manifestBytes=await readFile(resolve(root,'tests/fixtures/rv32im/games/manifest.json'));
  provenance={sourceSha256,runnerSha256:sha(await readFile(probe)),fixtureManifestSha256:sha(manifestBytes),manifest:JSON.parse(manifestBytes),fixtureRoot:resolve(root,'tests/fixtures/rv32im/games')};
}
async function cachedNativeRecord(guest){
  const base=resolve(recordingDirectory,guest,'baseline');
  const [bytes,evidenceBytes]=await Promise.all([readFile(resolve(base,'recording.json')),readFile(resolve(base,'evidence.json'))]);
  const evidence=JSON.parse(evidenceBytes),record=JSON.parse(bytes);
  assert.equal(evidence.version,1);assert.equal(evidence.guest,guest);assert.equal(record.guest,guest);
  assert.equal(evidence.runnerSha256,provenance.runnerSha256,'Native recording binary differs');
  assert.equal(evidence.fixtureManifestSha256,provenance.fixtureManifestSha256,'Native recording fixtures differ');
  assert.deepEqual(evidence.sourceSha256,provenance.sourceSha256,'Native recording source differs');
  assert.equal(evidence.recordingSha256,sha(bytes),'Native recording bytes differ');
  assert.equal(evidence.machine,'ff');assert.equal(evidence.logicProfile,'compact');
  assert.equal(evidence.quantumEvents,100000);assert.equal(evidence.limits.cycles,500000);
  assert.equal(record.configuration.caches,false);assert.equal(record.configuration.dram,false);
  assert.equal(record.configuration.quantumEvents,100000);
  const fixture=provenance.manifest.guests.find(item=>item.name===guest);
  assert.equal(sha(await readFile(resolve(provenance.fixtureRoot,fixture.elf))),fixture.elfSha256,'Current ELF fixture bytes differ');
  assert.equal(sha(await readFile(resolve(provenance.fixtureRoot,fixture.script))),fixture.scriptSha256,'Current input fixture bytes differ');
  assert.equal(record.elfSha256,fixture.elfSha256);assert.equal(record.scriptSha256,fixture.scriptSha256);
  assert.equal(record.setupViolations,0);
  assert.deepEqual(evidence.frameSha256,record.frames.map(frame=>sha(Buffer.from(frame.pixels,'hex'))));
  return record;
}
// WASM linear memory retains its high-water allocation. Finish native runs
// before creating that heap so later native children cannot overlap it.
const expectedRecords=new Map();
for(const guest of guests){
  const record=recordingDirectory?await cachedNativeRecord(guest):JSON.parse(execFileSync(probe,[guest],{encoding:'utf8',timeout:3600000,maxBuffer:64*1024*1024,stdio:['ignore','pipe','inherit']}));
  expectedRecords.set(guest,record);
  console.log(`${guest}: native reference ${recordingDirectory?'reused after full provenance checks':'captured'} (${record.cycles} cycles)`);
}
let largeExpected;
if(process.env.CIRCUITSIM_LARGE_NATIVE_RECORDING){
    const directory=resolve(process.env.CIRCUITSIM_LARGE_NATIVE_RECORDING);
    const bytes=await readFile(resolve(directory,'recording.json'));
    const evidence=JSON.parse(await readFile(resolve(directory,'evidence.json')));
    assert.equal(evidence.runnerSha256,sha(await readFile(probe)));
    assert.equal(evidence.recordingSha256,sha(bytes));
    if(provenance){assert.deepEqual(evidence.sourceSha256,provenance.sourceSha256);assert.equal(evidence.fixtureManifestSha256,provenance.fixtureManifestSha256);}
    largeExpected=JSON.parse(bytes);
    assert.equal(largeExpected.configuration.width,97);assert.equal(largeExpected.configuration.height,65);
}else largeExpected=JSON.parse(execFileSync(probe,['bringup','ff','500000','100000','compact','97','65'],{encoding:'utf8',timeout:3600000,maxBuffer:64*1024*1024,stdio:['ignore','pipe','inherit']}));
const exportDirectory=process.env.CIRCUITSIM_WASM_GUEST_RECORDINGS;
let wasmBinary=await readFile(wasm);
const exportIdentity=exportDirectory?{
  wasmSha256:sha(wasmBinary),driverSha256:sha(await readFile(fileURLToPath(import.meta.url))),
  nativeRunnerSha256:sha(await readFile(probe)),
  sourceManifestSha256:provenance?sha(JSON.stringify(provenance.sourceSha256)):null,
}:null;
const module=await (await import(pathToFileURL(resolve(js)))).default({wasmBinary});
wasmBinary=undefined;
function deterministic(record){
  record=structuredClone(record);
  for(const key of ['setupSeconds','executionSeconds','captureSeconds'])delete record[key];
  for(const f of record.frames)delete f.hostSeconds;
  return record;
}
async function exportComparedRecord(name,actual,expected){
  if(!exportDirectory)return;
  await mkdir(exportDirectory,{recursive:true});
  const bytes=JSON.stringify(actual)+'\n';
  await writeFile(resolve(exportDirectory,name+'.json'),bytes,{flag:'wx'});
  await writeFile(resolve(exportDirectory,name+'.evidence.json'),JSON.stringify({
    version:1,case:name,...exportIdentity,exactNativeParity:true,
    recordingSha256:sha(bytes),nativeDeterministicSha256:sha(JSON.stringify(deterministic(expected))),
  },null,2)+'\n',{flag:'wx'});
}
for(const guest of guests){
  const expected=expectedRecords.get(guest);
  const bridge=new module.WasmVisualizer();
  try{
    const actual=JSON.parse(bridge.runRecordedGuestJson(guest,false,false,500000));
    assert.deepEqual(deterministic(actual),deterministic(expected),guest);
    assert.equal(actual.setupViolations,0);
    assert(actual.presentations>=2&&actual.changedFrames>=1);
    await exportComparedRecord(guest,actual,expected);
    console.log(`${guest}: exact native/WASM frames, reports, device samples, cycles and event counts; ${actual.executionSeconds}s wasm`);
  }finally{bridge.delete();}
}

{
    const bridge=new module.WasmVisualizer();
    try{const actual=JSON.parse(bridge.runRecordedGuestSurfaceJson('bringup',97,65,500000));
        assert.deepEqual(deterministic(actual),deterministic(largeExpected));
        assert(actual.frames.every(f=>f.width===97&&f.height===65&&f.pixels.length===97*65*2&&/^f+$/.test(f.known)));
        assert.equal(actual.setupViolations,0);
        await exportComparedRecord('bringup-97x65',actual,largeExpected);
        console.log('97x65 compiled bringup: exact full frames, odd row alignment, device reads and native/WASM parity');
    }finally{bridge.delete();}
}
