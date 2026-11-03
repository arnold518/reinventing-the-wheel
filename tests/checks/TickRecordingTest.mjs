import assert from 'node:assert/strict';
import {readFileSync,mkdtempSync,rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {join} from 'node:path';
import {execFileSync} from 'node:child_process';
import {gunzipSync} from 'node:zlib';
import {validateTickRecording,decodeTickChunk,createTickPlayback} from '../../visualizer-v2/static/tick_recording.mjs';

const [probe]=process.argv.slice(2);
assert(probe,'Pass SignalTraceProbe executable');
const temporary=mkdtempSync(join(tmpdir(),'circuitsim-ticks-')),directory=join(temporary,'capture');
try{
  process.stdout.write(execFileSync(probe,[directory]));
  const read=file=>JSON.parse(readFileSync(join(directory,file),'utf8'));
  const manifest=validateTickRecording(read('ticks.json')),expected=read('expected.json'),sources=read('pin-sources.json');
  const auxiliaryWidths=manifest.auxiliary.flatMap(part=>Array(part.count).fill(part.width));
  assert(manifest.chunks.length>1,'Fixture must exercise independently seekable chunks');
  let row=0;
  for(const descriptor of manifest.chunks){
    const bytes=gunzipSync(readFileSync(join(directory,descriptor.file)));
    const chunk=decodeTickChunk(bytes,descriptor,expected.pinWidths,expected.wireWidths,auxiliaryWidths);
    for(let i=0;i<descriptor.count;i++){
      const values=chunk.at(i),wires=values.slice(manifest.pinCount,manifest.pinCount+manifest.wireCount),pins=sources.map((source,j)=>source<0?values[j]:wires[source]);
      assert.deepEqual({time:String(chunk.times[i]),pins,wires,auxiliary:values.slice(manifest.pinCount+manifest.wireCount)},expected.states[row++]);
    }
    const first=chunk.at(0).slice(),last=chunk.at(descriptor.count-1).slice();
    assert.deepEqual(chunk.at(0),first,'Chunk rewind differs');
    assert.deepEqual(chunk.at(descriptor.count-1),last,'Chunk forward seek differs');
    assert.throws(()=>decodeTickChunk(bytes.subarray(0,-1),descriptor,expected.pinWidths,expected.wireWidths,auxiliaryWidths));
    const bad=bytes.slice();bad[0]=0;
    assert.throws(()=>decodeTickChunk(bad,descriptor,expected.pinWidths,expected.wireWidths,auxiliaryWidths));
  }
  assert.equal(row,expected.states.length);
  for(const mutate of [r=>r.chunks[0].count=0,r=>r.chunks[1].start++,r=>r.timestampCount++,r=>r.markers[0].index=r.timestampCount,
    r=>r.chunks[0].bytes=17*1024*1024,r=>r.chunks[0].firstTime='18446744073709551616']){
    const bad=structuredClone(manifest);mutate(bad);assert.throws(()=>validateTickRecording(bad));
  }
  // The same decoder/fetch path used in browsers loads one window, handles
  // concurrent state requests, and reloads discarded chunks on rewind.
  const loads=[];
  globalThis.fetch=async url=>{loads.push(String(url));return new Response(readFileSync(join(directory,String(url))));};
  const components=new Map(manifest.auxiliary.flatMap(part=>part.componentIds.map(id=>[id,{id,type:'TraceEquipment',name:id}])));
  const player=await createTickPlayback(manifest,{pins:expected.pinWidths,wires:expected.wireWidths,components},
    async file=>read(String(file)),file=>file,manifest.markers);
  const lastIndex=manifest.timestampCount-1;
  const pair=await Promise.all([player.stateAt(lastIndex),player.stateAt(0)]);
  for(const [i,index] of [[0,lastIndex],[1,0]]){
    const {auxiliary,...reference}=expected.states[index];
    assert.deepEqual({...pair[i],time:String(pair[i].time)},{encoding:'indexed-v1',index,...reference});
  }
  for(const part of manifest.auxiliary){
    const state=await player.componentAt(lastIndex,part.componentIds[0]);
    assert.equal(state.supported,true);
    if(part.kind==='memory')assert.deepEqual(state.windowWords.map(word=>word.bits),expected.states.at(-1).auxiliary.slice(part.start,part.start+part.count));
    else assert.deepEqual(state.registers.map(word=>word.bits),expected.states.at(-1).auxiliary.slice(part.start,part.start+part.count));
  }
  await player.stateAt(lastIndex);
  assert.equal(loads.filter(file=>file===manifest.chunks.at(-1).file).length,2,'Rewind retained all chunks');
  await assert.rejects(()=>player.componentAt(0,'missing'));
  console.log('PASS every retained-history tick, wide time, four-state values, chunk seek/rewind, streaming replay and malformed traces');
}finally{rmSync(temporary,{recursive:true,force:true});}
