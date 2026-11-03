// The original canvas consumes its usual topology/state contracts. A captured
// circuit supplies exact retained checkpoints without starting another CPU.
import {createTickPlayback,validateTickRecording} from './tick_recording.mjs';
const MAX_BYTES=32*1024*1024,MAX_INFLATED_BYTES=64*1024*1024;
const clone=value=>JSON.parse(JSON.stringify(value));
function require(condition,message){if(!condition)throw new Error(message);}
const exactTime=value=>{
  require((typeof value==='number'&&Number.isSafeInteger(value)&&value>=0)
    || (typeof value==='string'&&/^(0|[1-9][0-9]*)$/.test(value)),'Invalid checkpoint time');
  const result=BigInt(value);require(result<=18446744073709551615n,'Checkpoint time overflow');return result;
};

export function validateRecordedCircuit(record,externalGraph=null) {
  require([1,2].includes(record?.schemaVersion)&&record.kind==='circuitsim-circuit-recording'
    &&record.capturePolicy==='presentation-checkpoints','Unsupported circuit recording');
  if(record.schemaVersion===2){
    require(externalGraph&&typeof record.topologyRef==='string','Missing external topology');
    return validateCheckpoints(record,externalGraph);
  }
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
  return validateCheckpoints(record,{components,pins,wires});
}
function validateCheckpoints(record,graph){
  const {components,pins,wires}=graph,checkpoints=record.checkpoints;
  require(Array.isArray(checkpoints)&&checkpoints.length>0&&checkpoints.length<=64,'Invalid checkpoint count');
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
      state[name].forEach((bits,i)=>require(typeof bits==='string'&&bits.length===(typeof entries[i]==='number'?entries[i]:entries[i].width)&&/^[01XZ]+$/.test(bits),'Invalid signal value'));
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
  return graph;
}

function signature(value){
  const text=JSON.stringify(value);let a=2166136261,b=5381;
  for(let i=0;i<text.length;i++){const c=text.charCodeAt(i);a=Math.imul(a^c,16777619);b=Math.imul(b,33)^c;}
  return [text.length,a>>>0,b>>>0].join(':');
}
export async function validateRecordedTopology(index,readSlice){
  require(index?.schemaVersion===1&&index.kind==='circuitsim-recorded-topology','Unsupported external topology');
  const topology=index.topology;
  require(Array.isArray(topology?.components)&&topology.components.length>0&&topology.components.length<=65536,'Invalid circuit topology');
  const components=new Map();
  topology.components.forEach((component,i)=>{
    require(typeof component.id==='string'&&!components.has(component.id),'Duplicate component ID');
    require(component.parentIndex==null||(Number.isInteger(component.parentIndex)&&component.parentIndex>=0&&component.parentIndex<i),'Invalid component parent');
    components.set(component.id,component);
  });
  require(components.has(topology.rootId)&&topology.stats?.componentCount===components.size,'Circuit counts differ');
  require(Array.isArray(index.scopes)&&index.scopes.length>0&&index.scopes.length<=128,'Invalid scope files');
  const ownerToScope=new Map(),scopeSignatures=[],observationPins=new Map();
  const observationRoots=new Set(topology.components.filter(c=>c.deviceViewKind||c.registerView).map(c=>c.id));
  const observationOwners=new Set(observationRoots);
  for(const component of topology.components)if(component.parentIndex!=null
    &&observationRoots.has(topology.components[component.parentIndex].id))observationOwners.add(component.id);
  const catalogs={};
  for(const key of ['pins','wires']){
    const count=topology.stats[key==='pins'?'pinCount':'wireCount'];
    require(Number.isInteger(count)&&count>=0&&count<=262144,'Invalid signal count');
    catalogs[key]={widths:new Uint8Array(count),ids:new Map(),signatures:new Array(count)};
  }
  function collect(key,entry){
    const catalog=catalogs[key],i=entry.stateIndex;
    require(typeof entry.id==='string'&&Number.isInteger(i)&&i>=0&&i<catalog.widths.length,'Invalid topology index');
    require(Number.isInteger(entry.width)&&entry.width>=1&&entry.width<=32,'Invalid signal width');
    require(components.has(key==='pins'?entry.componentId:entry.ownerId),'Unknown signal owner');
    const hash=signature(entry);
    if(key==='pins'&&observationOwners.has(entry.componentId)){
      if(!observationPins.has(entry.componentId))observationPins.set(entry.componentId,new Map());
      observationPins.get(entry.componentId).set(entry.name,entry.stateIndex);
    }
    if(catalog.widths[i])require(catalog.ids.get(entry.id)===i&&catalog.signatures[i]===hash,'Conflicting topology entry');
    else{require(!catalog.ids.has(entry.id),'Duplicate topology index');catalog.ids.set(entry.id,i);catalog.widths[i]=entry.width;catalog.signatures[i]=hash;}
  }
  for(let n=0;n<index.scopes.length;n++){
    const descriptor=index.scopes[n];
    require(typeof descriptor.file==='string'&&descriptor.file.length<=512&&Array.isArray(descriptor.ownerIds)
      &&descriptor.ownerIds.length>0&&descriptor.ownerIds.length<=512,'Invalid topology scope');
    for(const id of descriptor.ownerIds){require(components.has(id)&&!ownerToScope.has(id),'Duplicate or unknown scope owner');ownerToScope.set(id,n);}
    const slice=await readSlice(descriptor.file);
    require(slice?.sessionId===topology.sessionId&&JSON.stringify(slice.scopeIds)===JSON.stringify(descriptor.ownerIds),'Invalid topology slice');
    for(const key of ['pins','wires']){require(Array.isArray(slice[key]),'Invalid topology slice');for(const entry of slice[key])collect(key,entry);}
    const localPins=new Set(slice.pins.map(pin=>pin.id)),owners=new Set(descriptor.ownerIds);
    for(const wire of slice.wires)require(owners.has(wire.ownerId)&&(wire.sourcePinId==null||localPins.has(wire.sourcePinId))
      &&Array.isArray(wire.sinkPinIds)&&wire.sinkPinIds.every(id=>localPins.has(id)),'Unknown wire pin');
    scopeSignatures.push(signature(slice));
  }
  require(ownerToScope.size===components.size,'Incomplete topology scopes');
  for(const key of ['pins','wires']){
    const catalog=catalogs[key];require(catalog.ids.size===catalog.widths.length,'Non-contiguous signal indexes');
    require(Array.isArray(topology[key]),'Invalid topology header');
    for(const entry of topology[key]){require(catalog.ids.get(entry.id)===entry.stateIndex&&catalog.signatures[entry.stateIndex]===signature(entry),'Topology header differs');}
  }
  return {components,pins:catalogs.pins.widths,wires:catalogs.wires.widths,ownerToScope,scopeSignatures,observationPins};
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
  let bytes=new Uint8Array(size);let offset=0;
  for(const chunk of chunks){bytes.set(chunk,offset);offset+=chunk.byteLength;}
  // Recordings may be stored gzip-compressed (`.json.gz`). A server that
  // already decoded them hands over plain JSON, so look at the bytes.
  if(bytes.length>=2&&bytes[0]===0x1f&&bytes[1]===0x8b)bytes=await gunzip(bytes);
  return JSON.parse(new TextDecoder().decode(bytes));
}
async function gunzip(compressed){
  const reader=new Blob([compressed]).stream().pipeThrough(new DecompressionStream('gzip')).getReader(),chunks=[];let size=0;
  try{for(;;){const {value,done}=await reader.read();if(done)break;size+=value.byteLength;
    require(size<=MAX_INFLATED_BYTES,'Recording exceeds byte budget');chunks.push(value);}}
  catch(error){await reader.cancel();throw error;}
  const bytes=new Uint8Array(size);let offset=0;
  for(const chunk of chunks){bytes.set(chunk,offset);offset+=chunk.byteLength;}
  return bytes;
}

export function validateRecordedPlaylist(value) {
  require(value?.schemaVersion===1&&value.kind==='circuitsim-circuit-playlist','Unsupported circuit playlist');
  require(typeof value.guest==='string'&&value.guest.length>0,'Missing playlist guest');
  require(Array.isArray(value.parts)&&value.parts.length>0&&value.parts.length<=64,'Invalid playlist parts');
  require(Array.isArray(value.checkpoints)&&value.checkpoints.length>0&&value.checkpoints.length<=1024,'Invalid playlist checkpoints');
  let next=0,previous=-1n;
  for(const part of value.parts){
    require(part.start===next&&Number.isInteger(part.count)&&part.count>0&&part.count<=64
      &&typeof part.file==='string'&&part.file.length>0&&part.file.length<=512,'Invalid playlist part range');
    next+=part.count;
  }
  require(next===value.checkpoints.length,'Playlist count differs');
  for(const checkpoint of value.checkpoints){
    const time=exactTime(checkpoint.time);require(time>previous,'Playlist times must increase');previous=time;
    require(Number.isSafeInteger(checkpoint.cycle)&&checkpoint.cycle>=0,'Invalid playlist cycle');
  }
  return value;
}

export async function createRecordedCircuitTransport(source) {
  const url=new URL(source,document.baseURI);
  require(url.origin===location.origin&&['http:','https:'].includes(url.protocol),'Recording must use this site');
  const initial=await boundedJson(url);
  const tickRecord=initial.kind==='circuitsim-tick-recording'?validateTickRecording(initial):null;
  const presentationUrl=tickRecord?new URL(tickRecord.presentations,url):url;
  require(presentationUrl.origin===location.origin&&['http:','https:'].includes(presentationUrl.protocol),'Presentations must use this site');
  const entry=tickRecord?await boundedJson(presentationUrl):initial;
  const playlist=entry.kind==='circuitsim-circuit-playlist'?validateRecordedPlaylist(entry):null;
  const partUrls=playlist?.parts.map(part=>{
    const target=new URL(part.file,presentationUrl);
    require(target.origin===location.origin&&['http:','https:'].includes(target.protocol),'Playlist part must use this site');return target;
  });
  const record=playlist?await boundedJson(partUrls[0]):entry;
  const base=playlist?partUrls[0]:presentationUrl;
  const sameSite=(path,relative)=>{const target=new URL(path,relative);
    require(target.origin===location.origin&&['http:','https:'].includes(target.protocol),'Topology must use this site');return target;};
  const graphUrl=record.schemaVersion===2?sameSite(record.topologyRef,base):null;
  const graphIndex=graphUrl?await boundedJson(graphUrl):null;
  const graph=graphIndex?await validateRecordedTopology(graphIndex,file=>boundedJson(sameSite(file,graphUrl))):null;
  if(graphIndex)record.topology=graphIndex.topology;
  const checkedGraph=validateRecordedCircuit(record,graph);
  require(!tickRecord||graph,'Tick recordings require a shared topology index');
  const scopeCache=new Map();let scopeTail=Promise.resolve();
  async function readScope(number){
    const previous=scopeTail;let release;scopeTail=new Promise(resolve=>{release=resolve;});await previous;
    try{
      if(scopeCache.has(number)){const slice=scopeCache.get(number);scopeCache.delete(number);scopeCache.set(number,slice);return slice;}
      const slice=await boundedJson(sameSite(graphIndex.scopes[number].file,graphUrl));
      require(signature(slice)===graph.scopeSignatures[number],'Recorded topology changed');
      scopeCache.set(number,slice);if(scopeCache.size>2)scopeCache.delete(scopeCache.keys().next().value);return slice;
    }finally{release();}
  }
  const initialCheckpoint=record.checkpoints[0];
  const timeline=playlist?playlist.checkpoints:record.checkpoints.map((item,index)=>({time:item.state.time,cycle:record.recording.frames[index].cycle}));
  const ticks=tickRecord?await createTickPlayback(tickRecord,checkedGraph,boundedJson,path=>sameSite(path,url),timeline,record.recording.frames[0]):null;
  const initialTickState=ticks?await ticks.stateAt(0):null;
  const topologySignature=playlist&&!graphIndex?JSON.stringify(record.topology)+JSON.stringify(record.topologySlices):null;
  let loadedPart=0,loadedRecord=record,loadTail=Promise.resolve();
  function checkPart(part,index){
    const range=playlist.parts[index];require(part.checkpoints.length===range.count,'Playlist part count differs');
    require(part.recording.guest===playlist.guest,'Playlist guest differs');
    for(let i=0;i<range.count;i++){
      require(exactTime(part.checkpoints[i].state.time)===exactTime(timeline[range.start+i].time),'Playlist checkpoint time differs');
      require(part.recording.frames[i].cycle===timeline[range.start+i].cycle,'Playlist cycle differs');
    }
  }
  if(playlist)checkPart(record,0);
  async function checkpointAt(index){
    if(!playlist)return record.checkpoints[index];
    const number=playlist.parts.findIndex(part=>index>=part.start&&index<part.start+part.count);
    require(number>=0,'Missing playlist checkpoint');
    if(loadedPart!==number||!loadedRecord){
      const previous=loadTail;let release;loadTail=new Promise(resolve=>{release=resolve;});await previous;
      try{
        if(loadedPart!==number||!loadedRecord){
          // Release the prior window before fetching another. Keep topology,
          // the initial state and small timeline metadata, not every frame.
          loadedRecord=null;record.checkpoints=[];record.recording.frames=[];
          const candidate=await boundedJson(partUrls[number]);validateRecordedCircuit(candidate,graph);
          if(graphIndex)require(candidate.schemaVersion===2&&sameSite(candidate.topologyRef,partUrls[number]).href===graphUrl.href,'Playlist topology differs');
          else require(JSON.stringify(candidate.topology)+JSON.stringify(candidate.topologySlices)===topologySignature,'Playlist topology differs');
          checkPart(candidate,number);loadedRecord=candidate;loadedPart=number;
        }
      }finally{release();}
    }
    return loadedRecord.checkpoints[index-playlist.parts[number].start];
  }
  const layoutBase=playlist?partUrls[0]:presentationUrl;
  const layoutUrl=record.presentation?.layout?new URL(record.presentation.layout,layoutBase):new URL('./layout.json',import.meta.url);
  require(layoutUrl.origin===location.origin&&['http:','https:'].includes(layoutUrl.protocol),'Layout must use this site');
  const defaults=prepareRecordedLayout(record.topology,await boundedJson(layoutUrl));
  const palette=await boundedJson(new URL('./palette.json',import.meta.url));
  const storageKey='circuitsim-recorded-layout-v1:'+url.pathname;
  let layout=defaults;
  try{const saved=JSON.parse(localStorage.getItem(storageKey));if(saved?.schema_version===2)layout=prepareRecordedLayout(record.topology,saved);}catch{}
  const scenario=record.topology.scenario,name='RecordedCircuit/'+record.recording.guest;
  const own={schema:1,groups:[{id:'recorded-games',title:'Recorded games'}],
    aliases:{[name]:[scenario]},components:{[name]:{title:record.recording.guest+' · recorded circuit',group:'recorded-games'}},programs:[]};
  const descriptor={key:scenario,className:name,...own.components[name]};
  // The live visualizer's menu, from its catalog: choosing one of its
  // scenarios leaves the recording. Program rows use their single-cycle entry.
  let catalog=own,menuEntries=[descriptor];
  try{
    const live=await boundedJson(new URL('./scenarios.json',import.meta.url));
    const {scenarioOptions}=await import('./scenario_catalog.mjs');
    const descriptors=[...Object.keys(live.components).map(name=>({name,visualizable:true})),
      ...live.programs.map(program=>({name:`RV32ISingleCycleSystemTest/${program.id}`,visualizable:true}))];
    menuEntries=[...scenarioOptions(descriptors,live),descriptor];
    catalog={...live,groups:[...live.groups,...own.groups],aliases:{...live.aliases,...own.aliases},
      components:{...live.components,...own.components}};
  }catch{}
  function layoutEntry(component,store){
    const type=component.layoutType||component.type;
    return {...store.type_layouts?.[component.type],...store.type_layouts?.[type],
      ...store.profile_layouts?.[component.profileFingerprint]?.[type],
      ...(component.depth===0?store.root_layouts?.[record.topology.layoutKey]:{})};
  }
  // Older captures did not name their profile; they all used unit timing.
  const recordedTiming=typeof record.recording.timingProfile==='string'?record.recording.timingProfile:'unit';
  function payload(){
    const result=clone(record.topology);
    // Preserve the normal progressive scopes: eagerly drawing every nested
    // gate wire defeats the original canvas's memory and geometry budget.
    result.topologyEncoding='progressive-v1';
    result.timestamps=ticks?[initialTickState.time]:timeline.map(item=>item.time);
    result.timestampCount=ticks?.count||timeline.length;
    result.checkpoints=timeline.map((item,index)=>({index:ticks?ticks.markers[index].index:index,time:item.time,
      label:`PRESENT ${index+1} · cycle ${item.cycle.toLocaleString()}`,
      detail:ticks?'Presentation marker in the complete signal-change timeline.':'Settled presentation checkpoint; intermediate gate transitions are not recorded.'}));
    result.stats.timestampCount=result.timestampCount;result.state=initialTickState||{...initialCheckpoint.state,index:0};
    result.layout=clone(layout);result.profile.editable=false;
    result.profile.fidelityPreset=record.recording.fidelityPreset || 'recorded';
    result.timing={name:recordedTiming,tickPicoseconds:recordedTiming==='unit'?0:1,available:[recordedTiming],
      description:`Recorded with the ${recordedTiming} timing profile`+(record.recording.configuration?.period?`, clock period ${record.recording.configuration.period} ticks`:'')+'; fixed for this capture.'};
    result.recording={guest:record.recording.guest,capturePolicy:tickRecord?.capturePolicy||record.capturePolicy,sourceRuntime:record.sourceRuntime,
      checkpoints:timeline.length,timestamps:result.timestampCount,fidelityProfile:record.recording.profileName || 'Recorded fidelity'};
    for(const component of result.components){
      const entry=layoutEntry(component,layout),type=component.layoutType||component.type;
      const prefix=Object.entries(palette.prefix_colors||{}).find(([key])=>type.startsWith(key));
      component.color=clone(entry.color||prefix?.[1]||palette.type_colors?.[type]||palette.default_color||component.color);
    }
    return result;
  }
  function indexAt(query){const value=Number(query.get('index')||0);
    require(Number.isInteger(value)&&value>=0,'Invalid checkpoint index');return Math.min(value,(ticks?.count||timeline.length)-1);}
  window.CIRCUITSIM_RUNTIME={...(window.CIRCUITSIM_RUNTIME||{}),mode:'recorded'};
  return {async request(address,options={}){
    try{
      const request=new URL(address,document.baseURI),query=request.searchParams;
      const path=request.pathname.slice(request.pathname.indexOf('/api/')),method=(options.method||'GET').toUpperCase();
      const body=options.body?JSON.parse(options.body):{};let result;
      if(query.has('session'))require(query.get('session')===record.topology.sessionId,'Expired circuit session');
      if(path==='/api/scenarios'&&method==='GET')result={default:scenario,scenarios:menuEntries,groups:catalog.groups,catalog,runtime:'recorded'};
      else if(path==='/api/circuit'&&method==='GET'){
        require(!query.get('scenario')||query.get('scenario')===scenario,'Unknown recorded circuit');
        // The capture's timing is fixed: any requested profile gets the recorded one.
        result=payload();
      }else if(path==='/api/state'&&method==='GET'){
        const index=indexAt(query);
        if(ticks)result=await ticks.stateAt(index);
        else{const checkpoint=await checkpointAt(index);result={...checkpoint.state,index};}
      }else if(path==='/api/component-state'&&method==='GET'){
        const index=indexAt(query),id=query.get('componentId');require(checkedGraph.components.has(id),'Unknown component');
        if(ticks)result=await ticks.componentAt(index,id,query.has('address')?Number(query.get('address')):-1,Number(query.get('count')||64));
        else{const checkpoint=await checkpointAt(index);
          result={...(checkpoint.componentStates[id]||{componentId:id,supported:false,
            reason:'This component has no retained inspector data; its pin values are captured.'}),index,time:checkpoint.state.time};}
      }else if(path==='/api/topology'&&method==='POST'){
        require(body.session===record.topology.sessionId,'Expired circuit session');
        require(Array.isArray(body.ownerIds)&&body.ownerIds.length>0&&body.ownerIds.length<=512,'Invalid topology scopes');
        const owners=new Set(body.ownerIds),pinOwners=new Set(owners);
        for(const id of owners)require(checkedGraph.components.has(id),'Unknown topology owner');
        for(const component of checkedGraph.components.values())if(component.parentIndex!=null
          &&owners.has(record.topology.components[component.parentIndex].id))pinOwners.add(component.id);
        result={sessionId:record.topology.sessionId,scopeIds:[...owners],
          pins:[],wires:[]};
        if(graphIndex){
          const numbers=new Set([...pinOwners].map(id=>graph.ownerToScope.get(id)));
          const pins=new Map(),wires=new Map();
          for(const number of numbers){const slice=await readScope(number);
            for(const pin of slice.pins)if(pinOwners.has(pin.componentId))pins.set(pin.id,pin);
            for(const wire of slice.wires)if(owners.has(wire.ownerId))wires.set(wire.id,wire);
          }
          result.pins=[...pins.values()];result.wires=[...wires.values()];
        }else{result.pins=checkedGraph.pins.filter(pin=>pinOwners.has(pin.componentId));
          result.wires=checkedGraph.wires.filter(wire=>owners.has(wire.ownerId))};
      }else if(path==='/api/layout'&&method==='POST'){
        require(body.layout?.schema_version===2,'Invalid layout');layout=clone(body.layout);
        localStorage.setItem(storageKey,JSON.stringify(layout));result={ok:true,storage:'recording-local'};
      }else if(path==='/api/layout/defaults'&&method==='POST'){
        const parent=checkedGraph.components.get(body.parentId);require(parent,'Unknown layout parent');
        const placements=layoutEntry(parent,defaults).children||{};
        result=body.childName?{[body.childName]:placements[body.childName]}:placements;
      }else throw new Error('This action is unavailable for a captured circuit');
      return new Response(JSON.stringify(result),{status:200,headers:{'Content-Type':'application/json'}});
    }catch(error){return new Response(String(error.message||error),{status:400});}
  }};
}
