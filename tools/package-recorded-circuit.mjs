#!/usr/bin/env node
// Prepare an actual capture for the existing canvas without shipping the
// unrelated global layout store into the browser's replay session.
import {readFile,writeFile} from 'node:fs/promises';
import {resolve,dirname,basename,join,relative} from 'node:path';
import {createHash} from 'node:crypto';
import {validateRecordedCircuit,validateRecordedTopology,prepareRecordedLayout} from '../visualizer-v2/static/recorded_circuit_transport.mjs';
const [capturePath,layoutPath,outputPath,...extra]=process.argv.slice(2);
if(!outputPath||extra.length)throw new Error('Usage: node tools/package-recorded-circuit.mjs capture.json layout.json new-output.json');
const raw=await readFile(capturePath);if(raw.byteLength>32*1024*1024)throw new Error('Capture exceeds 32 MiB');
const record=JSON.parse(raw);let graphPath=null,graphBytes=null;
async function boundedFile(path){const bytes=await readFile(path);if(bytes.byteLength>32*1024*1024)throw new Error('Topology exceeds 32 MiB');return bytes;}
if(record.schemaVersion===2){
  graphPath=resolve(dirname(resolve(capturePath)),record.topologyRef);graphBytes=await boundedFile(graphPath);
  const index=JSON.parse(graphBytes);
  const graph=await validateRecordedTopology(index,async file=>{
    const path=resolve(dirname(graphPath),file);
    if(dirname(path)!==dirname(graphPath))throw new Error('Scope must be beside its topology index');
    return JSON.parse(await boundedFile(path));
  });
  validateRecordedCircuit(record,graph);record.topology=index.topology;
}else validateRecordedCircuit(record);
const layout=prepareRecordedLayout(record.topology,JSON.parse(await readFile(layoutPath,'utf8')));
const output=resolve(outputPath),layoutName=basename(output).replace(/\.json$/,'')+'.layout.json';
const layoutBytes=JSON.stringify(layout)+'\n';
record.presentation={layout:layoutName};
if(graphPath){record.topologyRef=relative(dirname(output),graphPath);delete record.topology;}
const bytes=JSON.stringify(record)+'\n';if(Buffer.byteLength(bytes)>32*1024*1024)throw new Error('Prepared capture exceeds 32 MiB');
await writeFile(join(dirname(output),layoutName),layoutBytes,{flag:'wx'});
await writeFile(output,bytes,{flag:'wx'});
const sha=bytes=>createHash('sha256').update(bytes).digest('hex');
await writeFile(output.replace(/\.json$/,'')+'.package.json',JSON.stringify({schemaVersion:1,
  topologySha256:graphBytes?sha(graphBytes):undefined,nativeCaptureSha256:sha(raw),preparedCaptureSha256:sha(bytes),layoutSha256:sha(layoutBytes),
  layoutBytes:Buffer.byteLength(layoutBytes),captureBytes:Buffer.byteLength(bytes)},null,2)+'\n',{flag:'wx'});
console.log(`Prepared ${record.recording.guest}: ${record.checkpoints.length} checkpoints; ${Buffer.byteLength(layoutBytes)} layout bytes`);
