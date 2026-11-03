const assert=require('node:assert/strict');
const view=require('../../visualizer-v2/static/device_view.js');
const {spawnSync}=require('node:child_process');
const fs=require('node:fs'),vm=require('node:vm');
const {resolve}=require('node:path');
for(const file of ['app.js','device_view.js']){
  const check=spawnSync(process.execPath,['--check',resolve(__dirname,'../../visualizer-v2/static',file)],{encoding:'utf8'});
  assert.equal(check.status,0,`${file}: ${check.stderr}`);
}
for(const type of ['FramebufferDisplay','ButtonDevice','ConsoleDevice','TimerDevice','SynchronousFifo<8>','SynchronousFifo<32>'])assert(view.supports(type));
assert(!view.supports('Register<32>'));
assert.equal(view.number('1X01'),null);assert.equal(view.number('111'),7);
for(const value of ['0','1','X','Z']){
  const keys=view.keyboard({levels:value.repeat(32)});assert.equal(keys.length,32);assert(keys.every(k=>k.value===value));
}
assert.equal(view.keyboard({levels:'1'+'0'.repeat(31)})[31].value,'1');
assert.equal(view.keyboard({levels:'0'.repeat(31)+'1'})[0].value,'1');
const keyboardState={supported:true,kind:'keyboard',levels:'0'.repeat(32),count:'010',head:'0'.repeat(32),overflow:'1'};
assert(view.validKeyboardState(keyboardState));
for(const value of ['0','1','X','Z'])assert(view.validKeyboardState({...keyboardState,levels:value.repeat(32)}));
for(const bad of [null,{}, {...keyboardState,kind:'display',width:1000000,height:1000000},
  {...keyboardState,levels:'0'.repeat(33)},{...keyboardState,count:'1111'},
  {...keyboardState,head:'0'.repeat(31)+'?'},{...keyboardState,overflow:'00'}])assert(!view.validKeyboardState(bad));
for(const [width,height,stride] of [[128,96,128],[320,200,320],[97,65,100]]){
  const length=stride*height;
  const frame={width,height,stride,format:'rgb332',pixels:'e0'.repeat(length),known:'ff'.repeat(length)};
  const pixels=view.rgba(frame);assert.equal(pixels.length,width*height*4);
  assert.deepEqual([...pixels.slice(-4)],[255,0,0,255]);
  frame.known='00'+frame.known.slice(2);assert.deepEqual([...view.rgba(frame).slice(0,4)],[255,60,255,255]);
}
const ramp={width:256,height:1,stride:256,format:'rgb332',pixels:Array.from({length:256},(_,i)=>i.toString(16).padStart(2,'0')).join(''),known:'ff'.repeat(256)};
const colors=view.rgba(ramp);
for(let i=0;i<256;++i)assert.deepEqual([...colors.slice(i*4,i*4+4)],[Math.round((i>>5)*255/7),Math.round(((i>>2)&7)*255/7),(i&3)*85,255]);
for(const frame of [{width:4096,height:4096,stride:4096},{width:0,height:1,stride:1},{width:4,height:4,stride:3},
  {width:4097,height:1,stride:4097},{width:1,height:4097,stride:1}]){
  const length=frame.width*frame.height<=262144?frame.stride*frame.height:0;
  assert.throws(()=>view.rgba({...frame,format:'rgb332',pixels:'00'.repeat(length),known:'ff'.repeat(length)}));
}
assert.match(view.summary({supported:true,kind:'keyboard',levels:'0'.repeat(32),count:'010',head:'0001',overflow:'1'}),/queue 2.*head 1.*overflow 1/);
console.log('PASS: device types, 32 four-state keys, queues, RGB332, larger/padded geometry and invalid images');

// Configured structures inherit colors, while their placements use the
// configuration's own key in both native-page and WASM worker rendering.
function extract(source,name){let start=source.indexOf(`function ${name}(`);assert(start>=0,name);if(source.slice(start-6,start)==='async ')start-=6;const next=/\n(?:export )?(?:async )?function /.exec(source.slice(start+1));return source.slice(start,next?start+1+next.index:source.length);}
for(const worker of [false,true]){
  const source=fs.readFileSync(resolve(__dirname,'../../visualizer-v2/static',worker?'wasm_worker.js':'app.js'),'utf8');
  const type='SynchronousFifo<8>',key=type+'[config-depth=7]';
  const layout={type_layouts:{[type]:{color:[1,2,3,190],children:{SLOT_0:{rel_width:1}}},[key]:{children:{SLOT_6:{rel_width:.5}}}}};
  const context=vm.createContext({app:{layout}});
  if(worker)vm.runInContext(extract(fs.readFileSync(resolve(__dirname,'../../visualizer-v2/static/layout_changes.mjs'),'utf8'),'mergeLayout'),context);
  for(const name of worker?['componentLayout']:['mergeLayout','componentLayoutKey','structureKeyed','getTypeLayoutFor'])vm.runInContext(extract(source,name),context);
  const component={type,layoutType:key,depth:1,profileFingerprint:'explicit'};
  const result=worker?context.componentLayout(component,layout,'test'):context.getTypeLayoutFor(component);
  assert(!Object.hasOwn(result.children,'SLOT_0'));assert(Object.hasOwn(result.children,'SLOT_6'));assert.deepEqual(result.color,layout.type_layouts[type].color);
  assert(Object.hasOwn(layout.type_layouts[type].children,'SLOT_0'));
}
console.log('PASS: configured floorplans retain styles and avoid default child placements');

assert(view.supports({type:'FutureDisplay<16>',deviceViewKind:'display'}));
assert(view.supports('SynchronousFifo<16>'));
assert(!view.supports({type:'FutureDisplay<16>',deviceViewKind:'unknown'}));
const smallKeys={supported:true,kind:'keyboard',buttonCount:4,levels:'1001',count:'001',head:'0'.repeat(32),overflow:'0'};
assert(view.validKeyboardState(smallKeys));
assert.deepEqual(view.keyboard(smallKeys).filter(k=>k.value==='1').map(k=>k.id),[0,3]);
assert(!view.validKeyboardState({...smallKeys,buttonCount:33}));
console.log('PASS: device capability metadata and variable keyboard dimensions');

// A visual abstraction hides every nested abstraction, regardless of fidelity.
// Independent branches still show their own outermost abstraction.
const source=fs.readFileSync(resolve(__dirname,'../../visualizer-v2/static/app.js'),'utf8');
const cellView=require('../../visualizer-v2/static/memory_cell_view.js');
const components=[
  {id:'root',type:'ComputerSystem'},
  {id:'display',parentId:'root',type:'FramebufferDisplay'},
  {id:'pixels',parentId:'display',type:'MemoryEndpoint<ff,32>'},
  {id:'cell',parentId:'pixels',type:'FlipFlopCell'},
  {id:'bus',parentId:'root',type:'FutureBus<32>',deviceViewKind:'bus'},
  {id:'bus-register',parentId:'bus',type:'Register<32>'},
  {id:'console',parentId:'root',type:'ConsoleDevice',fidelity:'behavioral'},
  {id:'fifo',parentId:'console',type:'SynchronousFifo<8>'},
  {id:'word-gate',parentId:'root',type:'XORGate<32>'},
  {id:'scalar-gate',parentId:'word-gate',type:'XORGate'},
  {id:'register',parentId:'root',type:'Register<8>'},
  {id:'rf',parentId:'root',type:'FutureRegisterBank',registerView:{width:16}},
  {id:'memory',parentId:'root',type:'SRAM<16>'},
  {id:'standalone-cell',parentId:'root',type:'SRAMCell'},
  {id:'plain',parentId:'root',type:'FullAdder'},
].map(c=>({fidelity:'structural',childIds:[],...c}));
const model={components:new Map(components.map(c=>[c.id,c])),componentOrder:components,
  componentStates:new Map(),memoryPages:new Map(),selectionPath:new Set(),
  pins:new Map([['boundary',{componentId:'display'}],['hidden',{componentId:'cell'}]]),
  wires:new Map([['inside',{ownerId:'display'}]]),sessionId:'unchanged',stateRequestId:1,componentStateRequestId:0};
for(const c of components)model.components.get(c.parentId)?.childIds.push(c.id);
let enabled=false;
const context=vm.createContext({app:model,CircuitDeviceView:view,CircuitMemoryCellView:cellView,
  ui:{deviceViews:{getAttribute:()=>String(enabled)},inspectorBody:{},inspectorPath:{}},
  componentCanDescend:()=>true,plannedFidelity:c=>c.fidelity,
  hasChildren:c=>Boolean(c?.childIds.length),commitLayoutTransaction(){},updateInspector(){},requestRender(){},
  apiUrl:path=>path});
vm.runInContext(source.slice(source.indexOf('const BASIC_LOGIC_GATE_VISUALS ='),source.indexOf('\nfunction wordGateWidth(')),context);
for(const name of ['deviceViewsEnabled','canvasAbstractionKind','outermostAbstraction','deviceViewComponent',
  'selectionInside','componentClosedByView','componentOpen','componentDrawn','isMemoryArrayType',
  'statefulOverlayComponents','setSelection','rebuildSelectionPath','refreshComponentStates'])vm.runInContext(extract(source,name),context);
const get=id=>model.components.get(id);
assert.equal(context.deviceViewComponent(get('display')),false);
assert.equal(context.componentOpen(get('display')),true);
enabled=true;
assert.deepEqual(components.filter(context.deviceViewComponent).map(c=>c.id),
  ['display','bus','console','word-gate','register','rf','memory','standalone-cell']);
assert.deepEqual(Array.from(context.statefulOverlayComponents(),c=>c.id),['display','bus','console','rf','memory']);
for(const id of ['pixels','cell','bus-register','fifo','scalar-gate']){
  assert.equal(context.deviceViewComponent(get(id)),false,id);
  assert.equal(context.componentDrawn(get(id)),false,id);
  context.setSelection('component',id);
  assert.equal(model.selection.id,context.outermostAbstraction(get(id)).id);
  assert.equal(context.componentDrawn(get(id)),false,'selection cannot reveal hidden children');
}
context.setSelection('pin','boundary');assert.equal(model.selection.type,'pin');
context.setSelection('pin','hidden');assert.equal(model.selection.id,'display');
context.setSelection('wire','inside');assert.equal(model.selection.id,'display');
enabled=false;
assert.equal(context.componentOpen(get('display')),true);
assert.equal(context.componentDrawn(get('cell')),true);
assert.equal(context.componentOpen(get('console')),false,'behavioral fidelity still closes parts');
assert(components.every(c=>c.fidelity===(c.id==='console'?'behavioral':'structural')));
console.log('PASS: outermost device/gate/storage views, independent siblings, hidden selection and fidelity preservation');

// Slow state requests from a previous view must not overwrite newer pictures.
(async()=>{
  const pending=[];
  context.visualizerRequest=()=>new Promise(resolve=>pending.push(resolve));
  enabled=true;
  const old=context.refreshComponentStates(0,1),oldResponses=pending.splice(0);
  enabled=false;
  const latest=context.refreshComponentStates(1,1),newResponses=pending.splice(0);
  for(const resolve of newResponses)resolve({ok:true,json:async()=>({checkpoint:1})});
  await latest;
  for(const resolve of oldResponses)resolve({ok:true,json:async()=>({checkpoint:0})});
  await old;
  assert(model.componentStates.size>0);
  assert([...model.componentStates.values()].every(state=>state.checkpoint===1));
  console.log('PASS: stale overlay responses after a view toggle cannot replace current state');
})().catch(error=>{console.error(error);process.exitCode=1;});
