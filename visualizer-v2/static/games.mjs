const $=id=>document.getElementById(id);
const ui=Object.fromEntries(['guest','run','cancel','file','status','screen','play','scrub','rate','save','frame','evidence','keys','keys-summary'].map(id=>[id,$(id)]));
let record=null,worker=null,timer=null,generation=0;
function stopPlayback(){if(timer!==null)clearInterval(timer);timer=null;ui.play.textContent='Play recording';}
function showFrame(index){
  if(!record)return;
  const f=record.frames[index];if(!f)return;
  ui.scrub.value=index;ui.screen.width=f.width;ui.screen.height=f.height;
  const context=ui.screen.getContext('2d'),image=context.createImageData(f.width,f.height);
  image.data.set(CircuitDeviceView.rgba(f));
  context.putImageData(image,0,0);
  const input=f.input;
  CircuitDeviceView.draw(ui.keys.getContext('2d'),{x:0,y:0,w:ui.keys.width,h:ui.keys.height},input);
  ui['keys-summary'].textContent=input?CircuitDeviceView.summary(input):'This recording has no keyboard snapshot.';
  ui.frame.textContent=`Frame ${index+1} of ${record.frames.length} · sequence ${f.sequence} · ${BigInt(f.time).toLocaleString()} ticks`;
}
function loadRecord(value){
  if(value.version!==1||!Array.isArray(value.frames)||value.frames.length>1024)throw new Error('Unsupported recording');
  let capturedBytes=0;
  for(const f of value.frames){
    if(!Number.isInteger(f.width)||!Number.isInteger(f.height)||!Number.isInteger(f.stride)||f.width<1||f.height<1||f.width>4096||f.height>4096||f.stride<f.width||f.stride*f.height>262144||f.format!=='rgb332')throw new Error('Invalid frame geometry or format');
    capturedBytes+=f.stride*f.height;
    if(capturedBytes>4*1024*1024)throw new Error('Recording exceeds the pixel budget');
    const length=f.stride*f.height*2;
    if(typeof f.pixels!=='string'||typeof f.known!=='string'||f.pixels.length!==length||f.known.length!==length||!/^[0-9a-f]*$/i.test(f.pixels+f.known))throw new Error('Invalid pixel data');
    if(f.input!=null&&!CircuitDeviceView.validKeyboardState(f.input))throw new Error('Invalid keyboard snapshot');
    if((typeof f.time==='string'&&!/^(0|[1-9][0-9]*)$/.test(f.time))||(typeof f.time!=='string'&&!Number.isSafeInteger(f.time))||BigInt(f.time)<0n||BigInt(f.time)>18446744073709551615n)throw new Error('Invalid exact frame timestamp');
  }
  stopPlayback();record=value;ui.scrub.max=Math.max(0,value.frames.length-1);ui.scrub.disabled=!value.frames.length;ui.play.disabled=!value.frames.length;ui.save.disabled=false;
  ui.status.textContent=`${value.guest||'Guest'}: ${value.frames.length} frames captured, ${Number(value.cycles).toLocaleString()} CPU cycles.`;
  const {frames,...evidence}=value;ui.evidence.textContent=JSON.stringify(evidence,null,2);if(value.frames.length)showFrame(0);else {ui.screen.width=64;ui.screen.height=48;ui.screen.getContext('2d').clearRect(0,0,64,48);ui.frame.textContent='No captured frame.';ui.keys.getContext('2d').clearRect(0,0,ui.keys.width,ui.keys.height);ui['keys-summary'].textContent='No input state captured.';}
}
function finishWorker(){++generation;if(worker)worker.terminate();worker=null;ui.run.disabled=false;ui.cancel.disabled=true;}
ui.run.addEventListener('click',()=>{
  stopPlayback();finishWorker();ui.run.disabled=true;ui.cancel.disabled=false;ui.status.textContent='Running predetermined script. This can take several minutes.';
  const running=new Worker(new URL('./games_worker.mjs',import.meta.url),{type:'module'});worker=running;
  running.addEventListener('message',({data})=>{if(worker!==running)return;finishWorker();try{if(data.error)throw new Error(data.error);loadRecord(JSON.parse(data.record));}catch(error){ui.status.textContent=String(error.message||error);}});
  running.addEventListener('error',event=>{if(worker!==running)return;finishWorker();ui.status.textContent=event.message||'Game worker failed';});
  running.postMessage({guest:ui.guest.value});
});
ui.cancel.addEventListener('click',()=>{finishWorker();ui.status.textContent='Run cancelled.';});
ui.file.addEventListener('change',async()=>{let loading;try{const file=ui.file.files[0];if(!file)return;finishWorker();loading=generation;if(file.size>32*1024*1024)throw new Error('Recording exceeds 32 MiB');const value=JSON.parse(await file.text());if(generation===loading)loadRecord(value);}catch(error){if(loading===undefined||generation===loading)ui.status.textContent=error.message;}});
ui.scrub.addEventListener('input',()=>{stopPlayback();showFrame(Number(ui.scrub.value));});
ui.play.addEventListener('click',()=>{if(timer!==null){stopPlayback();return;}const rate=Math.max(1,Math.min(30,Number(ui.rate.value)||3));ui.play.textContent='Pause recording';timer=setInterval(()=>{const next=Number(ui.scrub.value)+1;if(next>=record.frames.length){stopPlayback();return;}showFrame(next);},1000/rate);});
ui.save.addEventListener('click',()=>{if(!record)return;const url=URL.createObjectURL(new Blob([JSON.stringify(record)],{type:'application/json'})),link=document.createElement('a');link.href=url;link.download=`circuitsim-${record.guest||'game'}-recording.json`;link.click();setTimeout(()=>URL.revokeObjectURL(url),1000);});
