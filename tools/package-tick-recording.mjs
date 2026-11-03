#!/usr/bin/env node
// Validate bounded native trace chunks, and bind them to the independently
// checked PRESENT playlist. Publication is handled atomically by the runner.
import {readFile,writeFile} from 'node:fs/promises';
import {join} from 'node:path';
import {createHash} from 'node:crypto';
import {gunzipSync} from 'node:zlib';
import assert from 'node:assert/strict';
import {validateRecordedTopology} from '../visualizer-v2/static/recorded_circuit_transport.mjs';
import {validateTickRecording,decodeTickChunk,createTickPlayback} from '../visualizer-v2/static/tick_recording.mjs';
const [directory,...extra]=process.argv.slice(2);
if(!directory||extra.length)throw new Error('Usage: node tools/package-tick-recording.mjs ready-directory');
const json=async file=>JSON.parse(await readFile(join(directory,file),'utf8'));
const sha=bytes=>createHash('sha256').update(bytes).digest('hex');
const record=validateTickRecording(await json('ticks.json')),playlist=await json(record.presentations);
const graphIndex=await json('graph.json'),graph=await validateRecordedTopology(graphIndex,json);
const widths=record.auxiliary.flatMap(part=>Array(part.count).fill(part.width));
let compressedBytes=0,decodedBytes=0;
for(const descriptor of record.chunks){
  const bytes=await readFile(join(directory,descriptor.file));
  assert.equal(bytes.length,descriptor.compressedBytes);descriptor.sha256=sha(bytes);
  decodeTickChunk(gunzipSync(bytes,{maxOutputLength:16*1024*1024}),descriptor,graph.pins,graph.wires,widths);
  compressedBytes+=bytes.length;decodedBytes+=descriptor.bytes;
}
const first=await json(playlist.parts[0].file);
globalThis.fetch=async file=>new Response(await readFile(join(directory,String(file))));
const player=await createTickPlayback(record,graph,json,file=>file,playlist.checkpoints,first.recording.frames[0]);
let marker=0;
for(const part of playlist.parts){
  const packet=await json(part.file);
  for(const checkpoint of packet.checkpoints){
    const index=record.markers[marker++].index,state=await player.stateAt(index);
    assert.equal(BigInt(state.time),BigInt(checkpoint.state.time));
    assert.deepEqual(state.wires,checkpoint.state.wires,'Full-tick wires differ from settled PRESENT capture');
    assert.deepEqual(state.pins,checkpoint.state.pins,'Full-tick pins differ from settled PRESENT capture');
    for(const [id,snapshot] of Object.entries(checkpoint.componentStates)){
      const actual=await player.componentAt(index,id);
      for(const field of ['kind','pixels','known','sequence','present','levels','count','head','overflow','data','valid','full','registers'])
        if(snapshot[field]!=null)assert.deepEqual(actual[field],snapshot[field],`Tick inspector ${id}.${field} differs`);
    }
  }
}
record.pinSourcesSha256=sha(await readFile(join(directory,record.pinSources)));
await writeFile(join(directory,'ticks.json'),JSON.stringify(record)+'\n');
await writeFile(join(directory,'ticks.package.json'),JSON.stringify({schemaVersion:1,capturePolicy:record.capturePolicy,
  timestamps:record.timestampCount,chunks:record.chunks.length,compressedBytes,decodedBytes,
  ticksSha256:sha(await readFile(join(directory,'ticks.json'))),allChunksValidated:true,exactPresentationStates:true},null,2)+'\n',{flag:'wx'});
console.log(`Validated ${record.timestampCount} full ticks, ${record.chunks.length} chunks, ${compressedBytes} compressed bytes; all PRESENT states match`);
