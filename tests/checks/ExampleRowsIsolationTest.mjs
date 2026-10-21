import assert from 'node:assert/strict';
import {mkdtemp, mkdir, writeFile, readFile, rm} from 'node:fs/promises';
import {tmpdir} from 'node:os';
import {join, resolve} from 'node:path';
import {spawn} from 'node:child_process';

const temporary = await mkdtemp(join(tmpdir(), 'circuitsim-examples-'));
const generator = resolve('tools/generate-example-rows.mjs');
try {
  const docs = join(temporary, 'docs'), module = join(temporary, 'fake.mjs');
  await mkdir(docs);
  await writeFile(join(temporary, 'fake.wasm'), '');
  await writeFile(join(docs, 'examples.md'), [
    '<!-- simulator-examples: first TinyTest -->',
    '<!-- simulator-examples: second SecondTest -->',
    '<!-- simulator-examples: third TinyTest -->',
    '<!-- simulator-program: program RV32IProgramRootTest/program-01 -->',
  ].join('\n'));
  // Only the engine adapter is fake: both generator paths execute their real
  // discovery, filtering, row/state assertions, merging and atomic publication.
  await writeFile(module, [
    'export default async()=>({WasmVisualizer:class {',
    'setProfileOverride(){} delete(){}',
    'loadScenario(test){if(test==="FailTest")throw Error("forced failure");this.program=test.includes("/");return JSON.stringify(this.data());}',
    'data(){return {rootId:"DUT",profileFingerprint:"fake",components:[{id:"RV32I_PROGRAM_ROOT.CORE",fidelity:"behavioral"}],',
    'pins:(this.program?["PC","HALTED","TRAPPED"]:["P"]).map((name,stateIndex)=>({name,stateIndex,componentId:"DUT"})),',
    'checkpoints:[{rowIndex:0,label:this.program?"I0 done":"stable",time:3,index:0,detail:"pc_after=0x00000004 halted=true trapped=false"}]};}',
    'exampleRowsJson(){return JSON.stringify({schemaVersion:1,profileFingerprint:"fake",rows:[{rowIndex:0,checkpoint:"stable",time:3,timelineIndex:0,inputs:{},expectedOutputs:{P:"1"}}]});}',
    'stateJson(){return JSON.stringify({time:3,pins:this.program?["0".repeat(29)+"100","1","0"]:["1"]});}',
    '}});',
  ].join('\n'));
  async function run(output, isolated, filter, previous) {
    const args = [generator,module,join(temporary,'fake.wasm'),docs,output];
    if (filter) args.push(filter,previous);
    return await new Promise((accept,reject) => {
      const child=spawn(process.execPath,args,{env:{...process.env,CIRCUITSIM_EXAMPLES_ISOLATE:String(isolated)},stdio:['ignore','pipe','pipe']});
      let log='';
      child.stdout.on('data',data=>{log+=data;});child.stderr.on('data',data=>{log+=data;});
      child.once('error',reject);child.once('exit',code=>accept({code,log}));
    });
  }
  const flat=join(temporary,'flat.json'),isolated=join(temporary,'isolated.json');
  for (const [file,mode] of [[flat,0],[isolated,1]]) {
    const result=await run(file,mode);assert.equal(result.code,0,result.log);
  }
  assert.equal(await readFile(isolated,'utf8'),await readFile(flat,'utf8'));
  const seed=JSON.parse(await readFile(flat,'utf8'));seed.scenarios.historical={keep:true};
  const previous=join(temporary,'previous.json');await writeFile(previous,JSON.stringify(seed));
  for (const [file,mode] of [[flat,0],[isolated,1]]) {
    const result=await run(file,mode,'^TinyTest$',previous);assert.equal(result.code,0,result.log);
  }
  assert.equal(await readFile(isolated,'utf8'),await readFile(flat,'utf8'));
  assert.deepEqual(JSON.parse(await readFile(isolated,'utf8')).scenarios.historical,{keep:true});
  await writeFile(join(docs,'failure.md'),'<!-- simulator-examples: failure FailTest -->');
  for (const [file,mode] of [[flat,0],[isolated,1]]) {
    const before=await readFile(file,'utf8'),result=await run(file,mode);
    assert.notEqual(result.code,0);assert.match(result.log,/forced failure/);
    assert.equal(await readFile(file,'utf8'),before,'Failed generation replaced a working bundle');
  }
  console.log('PASS isolated/full/filtered exports are byte-identical; failures preserve the previous bundle');
} finally {
  await rm(temporary,{recursive:true,force:true});
}
