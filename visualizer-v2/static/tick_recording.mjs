// Sparse full-tick replay. One independently seekable chunk is retained; the
// global timeline needs only chunk ranges, not millions of JS timestamps.
const MAX_CHUNK_BYTES=16*1024*1024;
const require=(value,message)=>{if(!value)throw new Error(message);};
const time=value=>{require((typeof value==='string'&&/^(0|[1-9][0-9]*)$/.test(value))
  ||(Number.isSafeInteger(value)&&value>=0),'Invalid tick time');const n=BigInt(value);
  require(n<=0xffffffffffffffffn,'Tick time overflow');return n;};
const jsonTime=n=>n<=9007199254740991n?Number(n):String(n);
const QUARTETS=Array.from({length:256},(_,n)=>[6,4,2,0].map(shift=>'01XZ'[(n>>shift)&3]).join(''));

export function validateTickRecording(record) {
  require(record?.schemaVersion===1&&record.kind==='circuitsim-tick-recording'
    &&record.capturePolicy==='all-signal-ticks','Unsupported tick recording');
  require(typeof record.presentations==='string'&&typeof record.pinSources==='string','Missing tick references');
  for(const key of ['pinCount','wireCount'])require(Number.isInteger(record[key])&&record[key]>=0&&record[key]<=262144,'Invalid tick signal count');
  require(Number.isSafeInteger(record.timestampCount)&&record.timestampCount>0&&record.timestampCount<=1e9,'Invalid tick count');
  require(Array.isArray(record.chunks)&&record.chunks.length>0&&record.chunks.length<=65536,'Invalid tick chunks');
  let start=0,last=-1n;
  for(const chunk of record.chunks){
    require(chunk.start===start&&Number.isInteger(chunk.count)&&chunk.count>0&&chunk.count<=8192,'Invalid tick chunk range');
    require(typeof chunk.file==='string'&&chunk.file.length>0&&chunk.file.length<=512,'Invalid tick chunk file');
    require(Number.isInteger(chunk.bytes)&&chunk.bytes>0&&chunk.bytes<=MAX_CHUNK_BYTES
      &&Number.isInteger(chunk.compressedBytes)&&chunk.compressedBytes>0&&chunk.compressedBytes<=MAX_CHUNK_BYTES,'Invalid tick chunk size');
    require(time(chunk.firstTime)>last&&time(chunk.lastTime)>=time(chunk.firstTime),'Tick chunk times must increase');
    if(chunk.sha256!=null)require(/^[a-f0-9]{64}$/.test(chunk.sha256),'Invalid tick chunk digest');
    start+=chunk.count;last=time(chunk.lastTime);
  }
  require(start===record.timestampCount,'Tick count differs');
  require(Array.isArray(record.auxiliary)&&record.auxiliary.length<=128,'Invalid auxiliary trace state');
  let auxiliaryCount=0;const ids=new Set();
  for(const part of record.auxiliary){
    require(['memory','registers'].includes(part.kind)&&part.start===auxiliaryCount&&Number.isInteger(part.count)&&part.count>0
      &&Number.isInteger(part.width)&&part.width>=1&&part.width<=32,'Invalid auxiliary trace range');
    require(Array.isArray(part.componentIds)&&part.componentIds.length>0&&part.componentIds.length<=128,'Invalid auxiliary owners');
    for(const id of part.componentIds){require(typeof id==='string'&&!ids.has(id),'Duplicate auxiliary owner');ids.add(id);}
    if(part.kind==='memory')require(part.width===8&&[1,2,4].includes(part.wordBytes)&&part.count%part.wordBytes===0,'Invalid trace memory geometry');
    auxiliaryCount+=part.count;require(auxiliaryCount+record.pinCount+record.wireCount<=1048576,'Trace state exceeds signal budget');
  }
  require(Array.isArray(record.markers)&&record.markers.length>0&&record.markers.length<=1024,'Invalid tick markers');
  let previous=-1,previousTime=-1n;
  for(const marker of record.markers){
    require(Number.isInteger(marker.index)&&marker.index>previous&&marker.index<start&&time(marker.time)>previousTime,'Invalid tick marker');
    previous=marker.index;previousTime=time(marker.time);
  }
  return record;
}

class Reader {
  constructor(bytes){this.bytes=bytes;this.at=0;}
  byte(){require(this.at<this.bytes.length,'Truncated tick chunk');return this.bytes[this.at++];}
  integer(){let n=0n,shift=0n;
    for(let i=0;i<10;i++){const b=this.byte();n|=BigInt(b&127)<<shift;
      if(!(b&128)){require(n<=0xffffffffffffffffn,'Tick integer overflow');return n;}shift+=7n;}
    throw new Error('Tick integer overflow');
  }
  number(){let n=0,factor=1;
    for(let i=0;i<8;i++){const b=this.byte();n+=(b&127)*factor;require(Number.isSafeInteger(n),'Tick index overflow');
      if(!(b&128))return n;factor*=128;}throw new Error('Tick index overflow');}
  bits(width){let result='',last=0;for(let i=0;i<Math.ceil(width/4);i++){last=this.byte();result=QUARTETS[last]+result;}
    if(width%4)require(last>>(2*(width%4))===0,'Invalid tick padding');return result.slice(-width);}
  skip(width){this.at+=Math.ceil(width/4);require(this.at<=this.bytes.length,'Truncated tick value');
    if(width%4)require(this.bytes[this.at-1]>>(2*(width%4))===0,'Invalid tick padding');}
}

// Exported so native equipment can cross-check the exact same decoder.
export function decodeTickChunk(bytes,descriptor,pinWidths,wireWidths,auxiliaryWidths=[]) {
  require(bytes.byteLength===descriptor.bytes&&bytes.byteLength<=MAX_CHUNK_BYTES,'Tick chunk byte count differs');
  const reader=new Reader(bytes);
  require(reader.byte()===67&&reader.byte()===83&&reader.byte()===84&&reader.byte()===2,'Invalid tick chunk header');
  require(reader.number()===pinWidths.length&&reader.number()===wireWidths.length,'Tick chunk signal counts differ');
  require(reader.number()===auxiliaryWidths.length,'Auxiliary trace counts differ');
  const widths=Uint8Array.from([...pinWidths,...wireWidths,...auxiliaryWidths]);
  const baseline=Array.from(widths,width=>reader.bits(width));
  const offsets=new Uint32Array(descriptor.count),times=[];
  let previous=-1n;
  for(let row=0;row<descriptor.count;row++){
    offsets[row]=reader.at;const tick=reader.integer();require(tick>previous,'Tick times must increase');previous=tick;times.push(tick);
    const count=reader.number();require(count<=widths.length,'Too many tick changes');let index=0;
    for(let j=0;j<count;j++){
      const delta=reader.number();require(!j||delta>0,'Duplicate tick change');index+=delta;
      require(index<widths.length,'Invalid tick signal index');reader.skip(widths[index]);
    }
  }
  require(reader.at===bytes.length&&times[0]===time(descriptor.firstTime)&&times.at(-1)===time(descriptor.lastTime),'Tick chunk range differs');
  let current=-1,values=baseline.slice();
  return {times,at(index){
    require(Number.isInteger(index)&&index>=0&&index<offsets.length,'Invalid tick state index');
    if(index<current){values=baseline.slice();current=-1;}
    for(let row=current+1;row<=index;row++){
      reader.at=offsets[row];reader.integer();const count=reader.number();let signal=0;
      for(let j=0;j<count;j++){signal+=reader.number();values[signal]=reader.bits(widths[signal]);}
    }
    current=index;return values;
  }};
}

async function readBounded(response,maxBytes) {
  require(response.ok,`Cannot load tick chunk: ${response.status}`);
  require(Number(response.headers.get('content-length')||0)<=maxBytes,'Tick chunk exceeds byte budget');
  const reader=response.body.getReader(),chunks=[];let size=0;
  try{for(;;){const {done,value}=await reader.read();if(done)break;size+=value.byteLength;
    require(size<=maxBytes,'Tick chunk exceeds byte budget');chunks.push(value);}}
  catch(error){await reader.cancel();throw error;}
  const bytes=new Uint8Array(size);let offset=0;for(const chunk of chunks){bytes.set(chunk,offset);offset+=chunk.length;}return bytes;
}

export async function createTickPlayback(record,graph,readJson,urlFor,frames,geometry={}) {
  validateTickRecording(record);
  require(record.pinCount===graph.pins.length&&record.wireCount===graph.wires.length,'Tick topology differs');
  let sources;
  if(record.pinSourcesSha256){
    require(/^[a-f0-9]{64}$/.test(record.pinSourcesSha256),'Invalid pin-source digest');
    const bytes=await readBounded(await fetch(urlFor(record.pinSources)),32*1024*1024);
    const hash=Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',bytes)),b=>b.toString(16).padStart(2,'0')).join('');
    require(hash===record.pinSourcesSha256,'Pin-source digest differs');sources=JSON.parse(new TextDecoder().decode(bytes));
  }else sources=await readJson(urlFor(record.pinSources));
  require(Array.isArray(sources)&&sources.length===record.pinCount,'Invalid tick pin sources');
  for(let i=0;i<sources.length;i++)require(Number.isInteger(sources[i])&&sources[i]>=-1&&sources[i]<record.wireCount
    &&(sources[i]===-1||graph.pins[i]===graph.wires[sources[i]]),'Invalid tick pin source');
  require(record.markers.length===frames.length,'Tick presentations differ');
  record.markers.forEach((marker,i)=>require(time(marker.time)===time(frames[i].time),'Tick presentation time differs'));
  const auxiliaryWidths=[],auxiliaryOwners=new Map();
  for(const part of record.auxiliary){
    for(const id of part.componentIds){require(graph.components.has(id),'Unknown auxiliary trace owner');auxiliaryOwners.set(id,part);}
    for(let i=0;i<part.count;i++)auxiliaryWidths.push(part.width);
  }
  const components=[...graph.components.values()],children=new Map();
  for(const component of components)if(component.parentIndex!=null){
    const parent=components[component.parentIndex].id;if(!children.has(parent))children.set(parent,new Map());
    children.get(parent).set(component.name,component.id);
  }
  const hex=bits=>/^[01]+$/.test(bits)?'0x'+BigInt('0b'+bits).toString(16).padStart(Math.ceil(bits.length/4),'0'):null;
  function inspect(state,values,id,address,count){
    const component=graph.components.get(id),part=auxiliaryOwners.get(id),base={componentId:id,type:component.type,index:state.index,time:state.time,supported:true};
    const field=(owner,name)=>{const i=graph.observationPins?.get(owner)?.get(name);return i==null?'X':state.pinValue(i);};
    const child=name=>children.get(id)?.get(name);
    const extra=(part,start=0,count=part.count)=>values.slice(record.pinCount+record.wireCount+part.start+start,record.pinCount+record.wireCount+part.start+start+count);
    if(part?.kind==='registers'){
      base.registers=extra(part).map((bits,index)=>({index,name:'x'+index,bits,hex:hex(bits),known:hex(bits)!=null}));
      base.ports=Object.fromEntries(['RS1_ADDR','RS2_ADDR','RD_ADDR','WRITE_DATA','REG_WRITE','CLK','RST','RS1_DATA','RS2_DATA']
        .map(name=>{const value=field(id,name);return [name,{value,hex:hex(value)}];}));return base;
    }
    if(part?.kind==='memory'){
      require(Number.isInteger(count)&&count>0&&count<=256&&Number.isInteger(address)&&address>=-1&&address<part.count,'Invalid trace memory page');
      const start=address<0?0:address-address%part.wordBytes,n=Math.min(count,(part.count-start)/part.wordBytes),contents=extra(part,start,n*part.wordBytes);
      const windowWords=Array.from({length:n},(_,i)=>{
        const at=start+i*part.wordBytes,bits=contents.slice(i*part.wordBytes,(i+1)*part.wordBytes).reverse().join('');
        return {address:at,addressHex:'0x'+at.toString(16).padStart(8,'0'),bits,hex:hex(bits),known:hex(bits)!=null};
      });
      return {...base,capacityBytes:part.count,capacityWords:part.count/part.wordBytes,wordBits:part.wordBytes*8,historyEnabled:true,
        memoryRole:'generic',windowActive:false,activeAddress:null,activeBaseAddress:null,touchedWordCount:0,touchedWordsTruncated:false,
        pageAddress:start,pageWords:n,pageSize:count,manualPage:address>=0,hasPreviousPage:start>0,hasNextPage:start+n*part.wordBytes<part.count,
        touchedWords:[],windowWords,transaction:null};
    }
    switch(component.deviceViewKind){
      case 'display':{
        const pixelsPart=auxiliaryOwners.get(child('PIXELS'));
        if(!pixelsPart)break;
        require(Number.isInteger(geometry.width)&&geometry.width>0&&Number.isInteger(geometry.height)&&geometry.height>0
          &&Number.isInteger(geometry.stride)&&geometry.stride>=geometry.width&&geometry.stride*geometry.height<=pixelsPart.count,'Invalid trace display geometry');
        const pixels=extra(pixelsPart,0,geometry.stride*geometry.height);let data='',known='';
        for(const bits of pixels){let byte=0,mask=0;for(const bit of bits){byte=(byte<<1)|(bit==='1'?1:0);mask=(mask<<1)|(/[01]/.test(bit)?1:0);}
          data+=byte.toString(16).padStart(2,'0');known+=mask.toString(16).padStart(2,'0');}
        return {...base,kind:'display',width:geometry.width,height:geometry.height,stride:geometry.stride,format:'rgb332',pixels:data,known,
          sequence:field(id,'FRAME_SEQUENCE'),present:field(id,'PRESENT')};
      }
      case 'keyboard':return {...base,kind:'keyboard',buttonCount:field(child('LEVELS'),'Q').length,levels:field(child('LEVELS'),'Q'),
        count:field(child('EVENTS'),'COUNT'),head:field(child('EVENTS'),'DATA'),overflow:field(child('EVENTS'),'OVERFLOW')};
      case 'console':return {...base,kind:'console',valid:field(id,'TX_VALID'),data:field(id,'TX_DATA'),count:field(child('TX_FIFO'),'COUNT')};
      case 'timer':return {...base,kind:'timer',count:field(child('COUNTER'),'COUNT')};
      case 'queue':return {...base,kind:'queue',dataWidth:field(id,'DATA').length,count:field(id,'COUNT'),head:field(id,'DATA'),full:field(id,'FULL'),overflow:field(id,'OVERFLOW')};
    }
    return {...base,supported:false,reason:'Pin and wire values are captured for this component.'};
  }
  let loaded=-1,chunk=null,tail=Promise.resolve();
  const markers=new Map(record.markers.map((marker,i)=>[marker.index,i]));
  async function stateAt(index,observe){
      const previous=tail;let release;tail=new Promise(resolve=>{release=resolve;});await previous;
      try{
        const number=record.chunks.findIndex(item=>index>=item.start&&index<item.start+item.count);
        require(number>=0,'Missing tick state');const descriptor=record.chunks[number];
        if(loaded!==number){
          loaded=-1;chunk=null;
          const compressed=await readBounded(await fetch(urlFor(descriptor.file)),descriptor.compressedBytes);
          require(compressed.length===descriptor.compressedBytes,'Compressed tick size differs');
          if(descriptor.sha256){
            const hash=Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',compressed)),b=>b.toString(16).padStart(2,'0')).join('');
            require(hash===descriptor.sha256,'Tick chunk digest differs');
          }
          const stream=new Blob([compressed]).stream().pipeThrough(new DecompressionStream('gzip'));
          const bytes=await readBounded(new Response(stream),descriptor.bytes);
          chunk=decodeTickChunk(bytes,descriptor,graph.pins,graph.wires,auxiliaryWidths);loaded=number;
        }
        const local=index-descriptor.start,values=chunk.at(local);
        const tick=chunk.times[local];
        const marker=markers.get(index);if(marker!=null)require(tick===time(record.markers[marker].time),'Tick marker points to another time');
        const state={encoding:'indexed-v1',index,time:jsonTime(tick)};
        if(observe)return observe({...state,pinValue:i=>sources[i]<0?values[i]:values[record.pinCount+sources[i]]},values);
        return {...state,pins:sources.map((source,i)=>source<0?values[i]:values[record.pinCount+source]),
          wires:values.slice(record.pinCount,record.pinCount+record.wireCount)};
      }finally{release();}
    }
  return {count:record.timestampCount,markers:record.markers,presentationAt:index=>markers.get(index),stateAt,
    componentAt:(index,id,address=-1,count=64)=>stateAt(index,(state,values)=>inspect(state,values,id,address,count))};
}
