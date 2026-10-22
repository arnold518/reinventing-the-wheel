import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
import {validateRecordedCircuit,prepareRecordedLayout,createRecordedCircuitTransport} from '../../visualizer-v2/static/recorded_circuit_transport.mjs';
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
assert.equal((await request('circuit?timing=embedded')).status,400);
assert.equal((await request('component-state?componentId=missing')).status,400);
assert.equal((await (await request('component-state?componentId=display')).json()).pixels,'e0');
assert.equal((await (await request('component-state?componentId=root')).json()).supported,false);
assert.equal((await request('circuit/apply',{method:'POST',body:'{}'})).status,400);
const scope=await (await request('topology',{method:'POST',body:JSON.stringify({session:'capture',ownerIds:['root']})})).json();
assert.equal(scope.pins.length,1);assert.equal(scope.wires.length,1);
assert.equal((await request('topology',{method:'POST',body:JSON.stringify({session:'capture',ownerIds:['missing']})})).status,400);
await assert.rejects(()=>createRecordedCircuitTransport('https://outside.test/demo.json'));
// Adding captured mode leaves the original native HTTP transport untouched.
let legacy;
const context={URLSearchParams,fetch:async (...args)=>{legacy=args;return 'original-response';},
  window:{location:{search:''},CIRCUITSIM_RUNTIME:{mode:'http'}}};
vm.runInNewContext(readFileSync(new URL('../../visualizer-v2/static/transport.js',import.meta.url),'utf8'),context);
assert.equal(await context.window.visualizerRequest('/api/state',{method:'GET'}),'original-response');
assert.equal(legacy[0],'/api/state');
console.log('PASS recorded circuit contracts, bounds, wide times, invalid input, exact device frames and no simulation worker');
