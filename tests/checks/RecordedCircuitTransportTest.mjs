import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
import {validateRecordedCircuit,validateRecordedTopology,validateRecordedPlaylist,prepareRecordedLayout,createRecordedCircuitTransport} from '../../visualizer-v2/static/recorded_circuit_transport.mjs';
const frame={time:'9223372036854775931',cycle:1,width:1,height:1,stride:1,format:'rgb332',pixels:'e0',known:'ff'};
const sample={schemaVersion:1,kind:'circuitsim-circuit-recording',capturePolicy:'presentation-checkpoints',sourceRuntime:'native',
  presentation:{layout:'demo.layout.json'},
  topology:{sessionId:'capture',scenario:'recorded-game-demo',rootId:'root',layoutKey:'demo',profile:{editable:false},
    pins:[{id:'pin',componentId:'display',stateIndex:0,width:1}],wires:[{id:'wire',ownerId:'root',stateIndex:0,width:1,sourcePinId:'pin',sinkPinIds:[]}],loadedScopeIds:['root'],
    components:[{id:'root',name:'MACHINE',type:'System',parentIndex:null,depth:0},{id:'display',name:'DISPLAY',type:'FramebufferDisplay',deviceViewKind:'display',parentIndex:0,depth:1}],
    stats:{componentCount:2,pinCount:1,wireCount:1}},
  topologySlices:[{sessionId:'capture',pins:[{id:'pin',componentId:'display',stateIndex:0,width:1}],
    wires:[{id:'wire',ownerId:'root',stateIndex:0,width:1,sourcePinId:'pin',sinkPinIds:[]}]}],
  checkpoints:[{state:{time:frame.time,pins:['1'],wires:['1']},componentStates:{display:{componentId:'display',time:frame.time,supported:true,kind:'display',...frame}}}],
  recording:{guest:'demo',setupViolations:0,frames:[frame]}};
assert.equal(validateRecordedCircuit(sample).pins.length,1);
const defaults={schema_version:2,type_layouts:{},profile_layouts:{},root_layouts:{'other@fingerprint':{children:{DISPLAY:{rel_pos:[.2,.2],rel_width:.3}}}}};
const topology=structuredClone(sample.topology);topology.profileFingerprint='fingerprint';
const prepared=prepareRecordedLayout(topology,defaults);
assert.deepEqual(prepared.root_layouts.demo.children.DISPLAY,defaults.root_layouts['other@fingerprint'].children.DISPLAY);
assert.equal(defaults.root_layouts.demo,undefined,'Original layouts were mutated');
const grid=prepareRecordedLayout(topology,{schema_version:2});
assert(grid.root_layouts.demo.children.DISPLAY.rel_width>0);
assert.equal(grid.default_settings.topology_detail_min_pixels,12);
const bad=mutate=>{const value=structuredClone(sample);mutate(value);assert.throws(()=>validateRecordedCircuit(value));};
bad(r=>r.schemaVersion=2);bad(r=>r.checkpoints=[]);bad(r=>r.topology.components[1].id='root');
bad(r=>r.topology.components[1].parentIndex=1);bad(r=>r.topologySlices[0].pins[0].stateIndex=5);
bad(r=>r.checkpoints[0].state.pins=[]);bad(r=>r.checkpoints[0].state.pins=['?']);
bad(r=>r.checkpoints[0].state.time=Number(frame.time));bad(r=>r.recording.frames[0].time='18446744073709551616');
bad(r=>r.checkpoints[0].componentStates.display.pixels='00');bad(r=>r.topologySlices[0].wires[0].sourcePinId='missing');
bad(r=>r.recording.setupViolations=1);
const store=new Map();
globalThis.document={baseURI:'http://example.test/simulator/'};globalThis.location=new URL(document.baseURI);
globalThis.window=globalThis;globalThis.localStorage={getItem:key=>store.get(key)||null,setItem:(key,value)=>store.set(key,value)};
globalThis.Worker=class {constructor(){throw new Error('Captured playback must not create a simulation worker');}};
globalThis.fetch=async url=>new Response(JSON.stringify(String(url).endsWith('layout.json')
  ?{schema_version:2,type_layouts:{},root_layouts:{},profile_layouts:{}}
  :String(url).endsWith('palette.json')?{default_color:[1,2,3,255]}:sample));
const transport=await createRecordedCircuitTransport('../captures/demo.json');
const request=(path,options)=>transport.request(new URL('api/'+path,document.baseURI),options);
const payload=await (await request('circuit')).json();
assert.equal(payload.topologyEncoding,'progressive-v1');assert.equal(payload.timestamps[0],frame.time);
assert.equal(payload.recording.checkpoints,1);assert.equal(payload.profile.editable,false);
assert.equal((await (await request('state?session=capture&index=99')).json()).index,0);
assert.equal((await request('state?session=expired')).status,400);
assert.equal((await request('state?index=-1')).status,400);
// The capture's timing is fixed: another requested profile gets the recorded one.
{const fixed=await request('circuit?timing=embedded');assert.equal(fixed.status,200);
 assert.equal((await fixed.json()).timing.name,'unit');}
assert.equal((await request('component-state?componentId=missing')).status,400);
assert.equal((await (await request('component-state?componentId=display')).json()).pixels,'e0');
assert.equal((await (await request('component-state?componentId=root')).json()).supported,false);
assert.equal((await request('circuit/apply',{method:'POST',body:'{}'})).status,400);
const scope=await (await request('topology',{method:'POST',body:JSON.stringify({session:'capture',ownerIds:['root']})})).json();
assert.equal(scope.pins.length,1);assert.equal(scope.wires.length,1);
assert.equal((await request('topology',{method:'POST',body:JSON.stringify({session:'capture',ownerIds:['missing']})})).status,400);
await assert.rejects(()=>createRecordedCircuitTransport('https://outside.test/demo.json'));
// Whole-session timelines load a bounded part on demand, including rewind and
// concurrent state/inspector requests. The original single-file path above stays.
const second=structuredClone(sample),later=(BigInt(frame.time)+1n).toString();
second.checkpoints[0].state.time=later;second.checkpoints[0].componentStates.display.time=later;
second.recording.frames[0].time=later;second.recording.frames[0].cycle=2;
const playlist={schemaVersion:1,kind:'circuitsim-circuit-playlist',guest:'demo',
  parts:[{file:'first.json',start:0,count:1},{file:'second.json',start:1,count:1}],
  checkpoints:[{time:frame.time,cycle:1},{time:later,cycle:2}]};
validateRecordedPlaylist(playlist);
for(const mutate of [p=>p.parts[1].start=2,p=>p.parts[0].count=65,p=>p.checkpoints[1].time=frame.time,
  p=>p.checkpoints[0].cycle=-1,p=>p.parts=[]]){
  const invalid=structuredClone(playlist);mutate(invalid);assert.throws(()=>validateRecordedPlaylist(invalid));
}
const loads=[];
globalThis.fetch=async url=>{
  const path=new URL(url).pathname;loads.push(path);
  const value=path.endsWith('playlist.json')?playlist:path.endsWith('first.json')?sample:path.endsWith('second.json')?second:
    path.endsWith('layout.json')?{schema_version:2,type_layouts:{},root_layouts:{},profile_layouts:{}}:{default_color:[1,2,3,255]};
  return new Response(JSON.stringify(value));
};
const long=await createRecordedCircuitTransport('../captures/playlist.json');
const longRequest=path=>long.request(new URL('api/'+path,document.baseURI));
assert.deepEqual((await (await longRequest('circuit')).json()).timestamps,[frame.time,later]);
const pair=await Promise.all([longRequest('state?index=1'),longRequest('component-state?index=1&componentId=display')]);
assert.equal((await pair[0].json()).time,later);assert.equal((await pair[1].json()).time,later);
assert.equal(loads.filter(path=>path.endsWith('second.json')).length,1,'Concurrent requests loaded duplicate parts');
assert.equal((await (await longRequest('state?index=0')).json()).time,frame.time);
assert.equal(loads.filter(path=>path.endsWith('first.json')).length,2,'Rewind did not reload the released part');
assert.equal((await (await longRequest('circuit')).json()).state.time,frame.time,'Initial state was lost after releasing part zero');
const invalidTopology=structuredClone(second);invalidTopology.topology.layoutKey='different';
globalThis.fetch=async url=>new Response(JSON.stringify(String(url).endsWith('second.json')?invalidTopology:sample));
assert.equal((await longRequest('state?index=1')).status,400,'Mixed topologies were accepted');
// External graph files retain every signal while releasing scope objects.
const slice={...structuredClone(sample.topologySlices[0]),scopeIds:['root','display']};
const graphIndex={schemaVersion:1,kind:'circuitsim-recorded-topology',topology:sample.topology,
  scopes:[{file:'scope.json',ownerIds:slice.scopeIds}]};
const external=structuredClone(sample);external.schemaVersion=2;external.topologyRef='graph.json';
delete external.topology;delete external.topologySlices;
const externalGraph=await validateRecordedTopology(graphIndex,async()=>slice);
assert(externalGraph.pins instanceof Uint8Array);validateRecordedCircuit(external,externalGraph);
for(const mutate of [g=>g.scopes[0].ownerIds=['root'],g=>g.scopes.push(g.scopes[0]),
  g=>g.topology.stats.pinCount=2,g=>g.topology.components[1].parentIndex=1]){
  const invalid=structuredClone(graphIndex);mutate(invalid);
  await assert.rejects(()=>validateRecordedTopology(invalid,async()=>slice));
}
const badWire=structuredClone(slice);badWire.wires[0].sinkPinIds=['missing'];
await assert.rejects(()=>validateRecordedTopology(graphIndex,async()=>badWire));
const externalSecond=structuredClone(second);externalSecond.schemaVersion=2;externalSecond.topologyRef='graph.json';
delete externalSecond.topology;delete externalSecond.topologySlices;
const externalLoads=[];
globalThis.fetch=async url=>{
  const path=new URL(url).pathname;externalLoads.push(path);
  return new Response(JSON.stringify(path.endsWith('playlist.json')?playlist:path.endsWith('first.json')?external:
    path.endsWith('second.json')?externalSecond:path.endsWith('graph.json')?graphIndex:path.endsWith('scope.json')?slice:
    path.endsWith('layout.json')?{schema_version:2}:{default_color:[1,2,3,255]}));
};
const chunked=await createRecordedCircuitTransport('../captures/playlist.json');
const chunkRequest=(path,options)=>chunked.request(new URL('api/'+path,document.baseURI),options);
assert.equal((await (await chunkRequest('state?index=1')).json()).time,later);
assert.equal((await (await chunkRequest('state?index=0')).json()).time,frame.time);
const chunkScope=()=>chunkRequest('topology',{method:'POST',body:JSON.stringify({session:'capture',ownerIds:['root']})});
assert.equal((await (await chunkScope()).json()).wires[0].id,'wire');
assert.equal((await (await chunkScope()).json()).pins[0].id,'pin');
assert.equal(externalLoads.filter(path=>path.endsWith('scope.json')).length,2,'Scope cache failed');
assert.equal((await (await chunkRequest('circuit')).json()).state.time,frame.time);
// Adding captured mode leaves the original native HTTP transport untouched.
let legacy;
const context={URLSearchParams,fetch:async (...args)=>{legacy=args;return 'original-response';},
  window:{location:{search:''},CIRCUITSIM_RUNTIME:{mode:'http'}}};
vm.runInNewContext(readFileSync(new URL('../../visualizer-v2/static/transport.js',import.meta.url),'utf8'),context);
assert.equal(await context.window.visualizerRequest('/api/state',{method:'GET'}),'original-response');
assert.equal(legacy[0],'/api/state');
console.log('PASS recorded circuit contracts, bounds, wide times, invalid input, exact device frames and no simulation worker');
