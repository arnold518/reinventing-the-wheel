const assert=require('node:assert/strict');
const view=require('../../visualizer-v2/static/device_view.js');
const {spawnSync}=require('node:child_process');
const fs=require('node:fs'),vm=require('node:vm');
const {resolve}=require('node:path');
for(const file of ['app.js','device_view.js','games.mjs','games_worker.mjs']){
  const check=spawnSync(process.execPath,['--check',resolve(__dirname,'../../visualizer-v2/static',file)],{encoding:'utf8'});
  assert.equal(check.status,0,`${file}: ${check.stderr}`);
}
for(const type of ['FramebufferDisplay32','ButtonDevice32','ConsoleDevice32','TimerDevice32','SynchronousFifo<8>','SynchronousFifo<32>'])assert(view.supports(type));
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
function extract(source,name){const start=source.indexOf(`function ${name}(`);assert(start>=0,name);const end=source.indexOf('\nfunction ',start+1);return source.slice(start,end<0?source.length:end);}
for(const worker of [false,true]){
  const source=fs.readFileSync(resolve(__dirname,'../../visualizer-v2/static',worker?'wasm_worker.js':'app.js'),'utf8');
  const type='SynchronousFifo<8>',key=type+'[config-depth=7]';
  const layout={type_layouts:{[type]:{color:[1,2,3,190],children:{SLOT_0:{rel_width:1}}},[key]:{children:{SLOT_6:{rel_width:.5}}}}};
  const context=vm.createContext({app:{layout}});
  for(const name of worker?['mergeLayout','componentLayout']:['mergeLayout','componentLayoutKey','structureKeyed','getTypeLayoutFor'])vm.runInContext(extract(source,name),context);
  const component={type,layoutType:key,depth:1,profileFingerprint:'explicit'};
  const result=worker?context.componentLayout(component,layout,'test'):context.getTypeLayoutFor(component);
  assert(!Object.hasOwn(result.children,'SLOT_0'));assert(Object.hasOwn(result.children,'SLOT_6'));assert.deepEqual(result.color,layout.type_layouts[type].color);
  assert(Object.hasOwn(layout.type_layouts[type].children,'SLOT_0'));
}
console.log('PASS: configured floorplans retain styles and avoid default child placements');
