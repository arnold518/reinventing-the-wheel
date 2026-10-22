// The original canvas consumes its usual topology/state contracts. A captured
// circuit supplies exact retained checkpoints without starting another CPU.
const MAX_BYTES=32*1024*1024;
const clone=value=>JSON.parse(JSON.stringify(value));
function require(condition,message){if(!condition)throw new Error(message);}
const exactTime=value=>{
  require((typeof value==='number'&&Number.isSafeInteger(value)&&value>=0)
    || (typeof value==='string'&&/^(0|[1-9][0-9]*)$/.test(value)),'Invalid checkpoint time');
  const result=BigInt(value);require(result<=18446744073709551615n,'Checkpoint time overflow');return result;
};

export function validateRecordedCircuit(record) {
  require(record?.schemaVersion===1&&record.kind==='circuitsim-circuit-recording'
    &&record.capturePolicy==='presentation-checkpoints','Unsupported circuit recording');
  const topology=record.topology,checkpoints=record.checkpoints;
  require(topology&&Array.isArray(topology.components)&&topology.components.length>0
    &&topology.components.length<=16384,'Invalid circuit topology');
  require(Array.isArray(checkpoints)&&checkpoints.length>0&&checkpoints.length<=64,'Invalid checkpoint count');
  require(Array.isArray(record.topologySlices)&&record.topologySlices.length<=32,'Invalid topology slices');
  const components=new Map();
  topology.components.forEach((component,index)=>{
    require(typeof component.id==='string'&&!components.has(component.id),'Duplicate component ID');
    require(component.parentIndex==null || (Number.isInteger(component.parentIndex)&&component.parentIndex>=0&&component.parentIndex<index),'Invalid component parent');
    components.set(component.id,component);
  });
  require(components.has(topology.rootId),'Missing circuit root');
  const collect=key=>{
    const entries=new Map();
    for(const slice of record.topologySlices){
      require(slice.sessionId===topology.sessionId&&Array.isArray(slice[key]),'Invalid topology slice');
      for(const entry of slice[key]){
        require(typeof entry.id==='string'&&Number.isInteger(entry.stateIndex)&&entry.stateIndex>=0&&entry.stateIndex<262144,'Invalid topology index');
        require(Number.isInteger(entry.width)&&entry.width>=1&&entry.width<=32,'Invalid signal width');
        require(components.has(key==='pins'?entry.componentId:entry.ownerId),'Unknown signal owner');
        if(entries.has(entry.id))require(JSON.stringify(entries.get(entry.id))===JSON.stringify(entry),'Conflicting topology entry');
        entries.set(entry.id,entry);
      }
    }
    const result=[...entries.values()].sort((a,b)=>a.stateIndex-b.stateIndex);
    result.forEach((entry,index)=>require(entry.stateIndex===index,'Non-contiguous signal indexes'));
    return result;
  };
  const pins=collect('pins'),wires=collect('wires'),pinIds=new Set(pins.map(pin=>pin.id));
  for(const wire of wires)require((wire.sourcePinId==null||pinIds.has(wire.sourcePinId))
    &&Array.isArray(wire.sinkPinIds)&&wire.sinkPinIds.every(id=>pinIds.has(id)),'Unknown wire pin');
  require(topology.stats?.pinCount===pins.length&&topology.stats?.wireCount===wires.length
    &&topology.stats?.componentCount===components.size,'Circuit counts differ');
  require(record.recording?.frames?.length===checkpoints.length&&record.recording.setupViolations===0,'Invalid guest evidence');
  let previous=-1n,pixelBytes=0;
  checkpoints.forEach((checkpoint,index)=>{
    const state=checkpoint.state,time=exactTime(state?.time);
    require(time>previous&&time===exactTime(record.recording.frames[index].time),'Checkpoint/frame times differ');previous=time;
    const frame=record.recording.frames[index];
    require(Number.isInteger(frame.width)&&Number.isInteger(frame.height)&&Number.isInteger(frame.stride)
      &&frame.width>=1&&frame.height>=1&&frame.width<=4096&&frame.height<=4096&&frame.stride>=frame.width
      &&frame.stride*frame.height<=262144&&frame.format==='rgb332','Invalid captured surface');
    const bytes=frame.stride*frame.height;pixelBytes+=bytes;require(pixelBytes<=4*1024*1024,'Captured pixel budget exceeded');
    for(const key of ['pixels','known'])require(typeof frame[key]==='string'&&frame[key].length===2*bytes&&/^[0-9a-f]+$/i.test(frame[key]),'Invalid captured pixels');
    for(const [name,entries] of [['pins',pins],['wires',wires]]){
      require(Array.isArray(state[name])&&state[name].length===entries.length,'Incomplete circuit state');
      state[name].forEach((bits,i)=>require(typeof bits==='string'&&bits.length===entries[i].width&&/^[01XZ]+$/.test(bits),'Invalid signal value'));
    }
    require(checkpoint.componentStates&&typeof checkpoint.componentStates==='object','Missing device states');
    for(const [id,value] of Object.entries(checkpoint.componentStates)){
      require(components.has(id)&&value.componentId===id&&exactTime(value.time)===time,'Invalid component checkpoint');
    }
    const displays=[...components.values()].filter(component=>component.deviceViewKind==='display');
    require(displays.length>0,'Missing display component');
    for(const display of displays){
      const value=checkpoint.componentStates[display.id],frame=record.recording.frames[index];
      require(value?.supported===true&&value.kind==='display'&&value.width===frame.width&&value.height===frame.height
        &&value.stride===frame.stride&&value.format===frame.format&&value.pixels===frame.pixels&&value.known===frame.known,'Display differs from checked guest frame');
    }
    for(const component of components.values())if(component.deviceViewKind==='keyboard'){
      const value=checkpoint.componentStates[component.id];
      require(value?.supported===true&&value.kind==='keyboard'&&frame.input,'Missing keyboard checkpoint');
      for(const key of ['levels','count','head','overflow'])require(value[key]===frame.input[key],'Keyboard differs from checked guest input');
    }
  });
  return {components,pins,wires};
}

// Captures can introduce a new scenario key or geometry while retaining an
// existing circuit structure. Reuse only compatible child placements, and
// fill genuinely new structures with the same editable grid convention.
export function prepareRecordedLayout(topology,source) {
  const components=topology.components;
  const children=new Map();
  components.forEach(component=>{if(component.parentIndex!=null){
    const id=components[component.parentIndex].id;if(!children.has(id))children.set(id,[]);
    children.get(id).push(component);
  }});
  // The global layout store also contains large unrelated cache/memory
  // floorplans. A recording keeps only the types and child names it uses.
  const bases=new Set(components.map(component=>component.type));
  const fingerprints=new Set(components.map(component=>component.profileFingerprint).filter(Boolean));
  const childNames=new Map();
  for(const component of components){
    if(!childNames.has(component.type))childNames.set(component.type,new Set());
    for(const child of children.get(component.id)||[])childNames.get(component.type).add(child.name);
  }
  const pruneEntries=entries=>Object.fromEntries(Object.entries(entries||{}).filter(([key])=>
    bases.has(key)||[...bases].some(type=>key.startsWith(type+'['))).map(([key,value])=>{
      const type=bases.has(key)?key:[...bases].find(type=>key.startsWith(type+'['));
      const entry={...value};
      if(entry.children)entry.children=Object.fromEntries(Object.entries(entry.children).filter(([name])=>childNames.get(type)?.has(name)));
      return [key,entry];
    }));
  const layout=clone({schema_version:2,default_settings:{...source.default_settings,
      topology_detail_min_pixels:source.default_settings?.topology_detail_min_pixels??12},
    type_layouts:pruneEntries(source.type_layouts),
    profile_layouts:Object.fromEntries(Object.entries(source.profile_layouts||{}).filter(([key])=>fingerprints.has(key))
      .map(([key,value])=>[key,pruneEntries(value)])),
    root_layouts:Object.fromEntries(Object.entries(source.root_layouts||{}).filter(([key,entry])=>
      key===topology.layoutKey||(key.endsWith('@'+topology.profileFingerprint)
        &&Object.keys(entry.children||{}).length===(children.get(topology.rootId)||[]).length
        &&(children.get(topology.rootId)||[]).every(child=>entry.children?.[child.name]))))});
  const compatible=(entry,kids)=>entry?.children&&Object.keys(entry.children).length===kids.length
    &&kids.every(child=>Array.isArray(entry.children[child.name]?.rel_pos)
      &&Number.isFinite(entry.children[child.name]?.rel_width));
  const entryFor=component=>{
    const type=component.layoutType||component.type,base={...layout.type_layouts[component.type]};
    if(type.startsWith(component.type+'[config-'))delete base.children;
    return {...base,...layout.type_layouts[type],...layout.profile_layouts[component.profileFingerprint]?.[type]};
  };
  for(const component of components){
    const kids=children.get(component.id)||[];if(!kids.length)continue;
    const type=component.layoutType||component.type,profile=component.profileFingerprint;
    const typed=entryFor(component),root=component.id===topology.rootId;
    const existing=root?{...typed,...layout.root_layouts[topology.layoutKey]}:typed;
    if(compatible(existing,kids))continue;
    let inherited;
    if(root)inherited=Object.entries(layout.root_layouts).find(([key,value])=>
      key.endsWith('@'+topology.profileFingerprint)&&compatible(value,kids))?.[1];
    else inherited=Object.entries({...layout.type_layouts,...layout.profile_layouts[profile]}).find(([key,value])=>
      (key===component.type||key.startsWith(component.type+'[config-'))&&compatible(value,kids))?.[1];
    let placements;
    if(inherited)placements=clone(inherited.children);
    else{
      const cols=Math.ceil(Math.sqrt(kids.length)),rows=Math.ceil(kids.length/cols);
      const aspect=Number(existing.aspect_ratio||component.aspectRatio||1),cellW=.84/cols,cellH=Math.max(.1,aspect-.24)/rows;
      placements=Object.fromEntries(kids.map((child,index)=>{
        const childAspect=Number(entryFor(child).aspect_ratio||child.aspectRatio||1);
        const width=Math.min(cellW*.75,cellH*.75/childAspect);
        return [child.name,{rel_pos:[.08+(index%cols)*cellW+(cellW-width)/2,
          (.18+Math.floor(index/cols)*cellH+(cellH-width*childAspect)/2)/aspect],rel_width:width}];
      }));
    }
    if(root)layout.root_layouts[topology.layoutKey]={...layout.root_layouts[topology.layoutKey],children:placements};
    else if(profile&&profile!=='explicit'){
      layout.profile_layouts[profile]||={};
      layout.profile_layouts[profile][type]={...layout.profile_layouts[profile][type],children:placements};
    }else layout.type_layouts[type]={...layout.type_layouts[type],children:placements};
  }
  return layout;
}

async function boundedJson(url) {
  const response=await fetch(url);require(response.ok,`Cannot load ${url.pathname}: ${response.status}`);
  require(Number(response.headers.get('content-length')||0)<=MAX_BYTES,'Recording exceeds byte budget');
  const reader=response.body.getReader(),chunks=[];let size=0;
  try{for(;;){const {value,done}=await reader.read();if(done)break;size+=value.byteLength;
    require(size<=MAX_BYTES,'Recording exceeds byte budget');chunks.push(value);}}
  catch(error){await reader.cancel();throw error;}
  const bytes=new Uint8Array(size);let offset=0;
  for(const chunk of chunks){bytes.set(chunk,offset);offset+=chunk.byteLength;}
  return JSON.parse(new TextDecoder().decode(bytes));
}

export async function createRecordedCircuitTransport(source) {
  const url=new URL(source,document.baseURI);
  require(url.origin===location.origin&&['http:','https:'].includes(url.protocol),'Recording must use this site');
  const record=await boundedJson(url),graph=validateRecordedCircuit(record);
  const layoutUrl=record.presentation?.layout?new URL(record.presentation.layout,url):new URL('./layout.json',import.meta.url);
  require(layoutUrl.origin===location.origin&&['http:','https:'].includes(layoutUrl.protocol),'Layout must use this site');
  const defaults=prepareRecordedLayout(record.topology,await boundedJson(layoutUrl));
  const palette=await boundedJson(new URL('./palette.json',import.meta.url));
  const storageKey='circuitsim-recorded-layout-v1:'+url.pathname;
  let layout=defaults;
  try{const saved=JSON.parse(localStorage.getItem(storageKey));if(saved?.schema_version===2)layout=prepareRecordedLayout(record.topology,saved);}catch{}
  const scenario=record.topology.scenario,name='RecordedCircuit/'+record.recording.guest;
  const catalog={schema:1,groups:[{id:'recorded-games',title:'Scripted games · circuit checkpoints'}],
    aliases:{[name]:[scenario]},components:{[name]:{title:record.recording.guest+' · recorded circuit',group:'recorded-games'}},programs:[]};
  const descriptor={key:scenario,className:name,...catalog.components[name]};
  function layoutEntry(component,store){
    const type=component.layoutType||component.type;
    return {...store.type_layouts?.[component.type],...store.type_layouts?.[type],
      ...store.profile_layouts?.[component.profileFingerprint]?.[type],
      ...(component.depth===0?store.root_layouts?.[record.topology.layoutKey]:{})};
  }
  function payload(){
    const result=clone(record.topology);
    // Preserve the normal progressive scopes: eagerly drawing every nested
    // gate wire defeats the original canvas's memory and geometry budget.
    result.topologyEncoding='progressive-v1';
    result.timestamps=record.checkpoints.map(item=>item.state.time);
    result.checkpoints=record.checkpoints.map((item,index)=>({index,time:item.state.time,
      label:`PRESENT ${index+1} · cycle ${record.recording.frames[index].cycle.toLocaleString()}`,
      detail:'Settled presentation checkpoint; intermediate gate transitions are not recorded.'}));
    result.stats.timestampCount=record.checkpoints.length;result.state={...record.checkpoints[0].state,index:0};
    result.layout=clone(layout);result.profile.editable=false;
    result.timing={name:'unit',tickPicoseconds:0,available:['unit'],description:'Recorded guest clock; fixed for this capture.'};
    result.recording={guest:record.recording.guest,capturePolicy:record.capturePolicy,sourceRuntime:record.sourceRuntime,
      checkpoints:record.checkpoints.length};
    for(const component of result.components){
      const entry=layoutEntry(component,layout),type=component.layoutType||component.type;
      const prefix=Object.entries(palette.prefix_colors||{}).find(([key])=>type.startsWith(key));
      component.color=clone(entry.color||prefix?.[1]||palette.type_colors?.[type]||palette.default_color||component.color);
    }
    return result;
  }
  function indexAt(query){const value=Number(query.get('index')||0);
    require(Number.isInteger(value)&&value>=0,'Invalid checkpoint index');return Math.min(value,record.checkpoints.length-1);}
  window.CIRCUITSIM_RUNTIME={...(window.CIRCUITSIM_RUNTIME||{}),mode:'recorded'};
  return {async request(address,options={}){
    try{
      const request=new URL(address,document.baseURI),query=request.searchParams;
      const path=request.pathname.slice(request.pathname.indexOf('/api/')),method=(options.method||'GET').toUpperCase();
      const body=options.body?JSON.parse(options.body):{};let result;
      if(query.has('session'))require(query.get('session')===record.topology.sessionId,'Expired circuit session');
      if(path==='/api/scenarios'&&method==='GET')result={default:scenario,scenarios:[descriptor],groups:catalog.groups,catalog,runtime:'recorded'};
      else if(path==='/api/circuit'&&method==='GET'){
        require(!query.get('scenario')||query.get('scenario')===scenario,'Unknown recorded circuit');
        require(!query.get('timing')||query.get('timing')==='unit','Recorded timing is fixed');result=payload();
      }else if(path==='/api/state'&&method==='GET'){
        const index=indexAt(query);result={...record.checkpoints[index].state,index};
      }else if(path==='/api/component-state'&&method==='GET'){
        const index=indexAt(query),id=query.get('componentId');require(graph.components.has(id),'Unknown component');
        result={...(record.checkpoints[index].componentStates[id]||{componentId:id,supported:false,
          reason:'This component has no retained inspector data; its pin values are captured.'}),index,time:record.checkpoints[index].state.time};
      }else if(path==='/api/topology'&&method==='POST'){
        require(body.session===record.topology.sessionId,'Expired circuit session');
        require(Array.isArray(body.ownerIds)&&body.ownerIds.length>0&&body.ownerIds.length<=512,'Invalid topology scopes');
        const owners=new Set(body.ownerIds),pinOwners=new Set(owners);
        for(const id of owners)require(graph.components.has(id),'Unknown topology owner');
        for(const component of graph.components.values())if(component.parentIndex!=null
          &&owners.has(record.topology.components[component.parentIndex].id))pinOwners.add(component.id);
        result={sessionId:record.topology.sessionId,scopeIds:[...owners],
          pins:graph.pins.filter(pin=>pinOwners.has(pin.componentId)),
          wires:graph.wires.filter(wire=>owners.has(wire.ownerId))};
      }else if(path==='/api/layout'&&method==='POST'){
        require(body.layout?.schema_version===2,'Invalid layout');layout=clone(body.layout);
        localStorage.setItem(storageKey,JSON.stringify(layout));result={ok:true,storage:'recording-local'};
      }else if(path==='/api/layout/defaults'&&method==='POST'){
        const parent=graph.components.get(body.parentId);require(parent,'Unknown layout parent');
        const placements=layoutEntry(parent,defaults).children||{};
        result=body.childName?{[body.childName]:placements[body.childName]}:placements;
      }else throw new Error('This action is unavailable for a captured circuit');
      return new Response(JSON.stringify(result),{status:200,headers:{'Content-Type':'application/json'}});
    }catch(error){return new Response(String(error.message||error),{status:400});}
  }};
}
