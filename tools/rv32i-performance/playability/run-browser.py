#!/usr/bin/env python3
"""Run the audit-only WASM experiment binding in a Chromium worker.

Requests are fulfilled from local artifacts by Playwright routing; no HTTP
server, game platform, or production viewer modification is involved.
"""
import argparse
import hashlib
import json
import mimetypes
from pathlib import Path
from playwright.sync_api import sync_playwright

ROOT=Path(__file__).resolve().parents[3]
WORKER='''import createAudit from './audit.mjs';
const audit=await createAudit();
self.addEventListener('message',event=>{
 const c=event.data;
 try {self.postMessage({id:c.id,result:JSON.parse(audit.runAudit(c.name,c.memory,c.topology,c.fidelity,c.verify))});}
 catch(error){self.postMessage({id:c.id,error:String(error)});}
});
self.postMessage({ready:true});
'''
HTML='''<!doctype html><canvas id="screen"></canvas><script>
window.beats=0;function beat(){++beats;requestAnimationFrame(beat)};requestAnimationFrame(beat);
window.worker=new Worker('/worker.mjs',{type:'module'});
window.pending=new Map();window.nextId=1;
window.ready=new Promise((resolve,reject)=>{
 worker.onerror=e=>reject(new Error(e.message));
 worker.onmessage=e=>{
  if(e.data.ready){resolve();return;}
  const entry=pending.get(e.data.id);pending.delete(e.data.id);
  e.data.error?entry.reject(new Error(e.data.error)):entry.resolve(e.data.result);
 };
});
window.run=async c=>{
 await ready;const before=beats;const started=performance.now();
 const r=await new Promise((resolve,reject)=>{
  c.id=nextId++;pending.set(c.id,{resolve,reject});worker.postMessage(c);
 });
 r.browser_roundtrip_seconds=(performance.now()-started)/1000;r.ui_animation_callbacks=beats-before;
 return r;
};
</script>'''

def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--out',type=Path,default=ROOT/'build/playability-audit-2026-10-19')
 p.add_argument('--cases',nargs='+',required=True)
 p.add_argument('--memory',default='ff');p.add_argument('--topology',default='')
 p.add_argument('--fidelity',default='behavioral');p.add_argument('--repeats',type=int,default=3)
 p.add_argument('--executable',type=Path)
 a=p.parse_args();out=a.out.resolve()
 tag='browser-'+a.fidelity+'-'+a.memory+('-'+a.topology if a.topology else '')
 with sync_playwright() as pw:
  launch=dict(headless=True,args=['--no-sandbox','--disable-dev-shm-usage'])
  if a.executable:launch['executable_path']=str(a.executable)
  browser=pw.chromium.launch(**launch)
  page=browser.new_page();page.set_default_timeout(600000)
  def route(request):
   path=request.request.url.split('audit.local',1)[1].split('?',1)[0]
   if path=='/':return request.fulfill(body=HTML,content_type='text/html')
   if path=='/worker.mjs':return request.fulfill(body=WORKER,content_type='text/javascript')
   file=(out/path.lstrip('/')).resolve()
   assert file.is_relative_to(out),path
   content_type='application/wasm' if file.suffix=='.wasm' else 'text/javascript' if file.suffix in ('.js','.mjs') else mimetypes.guess_type(str(file))[0]
   request.fulfill(path=str(file),content_type=content_type or 'application/octet-stream')
  page.route('http://audit.local/**',route)
  page.goto('http://audit.local/');page.evaluate('ready')
  (out/(tag+'-metadata.json')).write_text(json.dumps(dict(user_agent=page.evaluate('navigator.userAgent'),version=browser.version,
   wasm_sha256=hashlib.sha256((out/'audit.wasm').read_bytes()).hexdigest()),indent=2)+'\n')
  for name in a.cases:
   fingerprint=None
   for rep in range(a.repeats):
    stem=f'{tag}-{name}-{rep}';print('START',stem,flush=True)
    config=dict(name=name,memory=a.memory,topology=a.topology,fidelity=a.fidelity,verify=rep==0)
    r=page.evaluate('c=>run(c)',config)
    assert r['completed'] and r['setup_violations']==0,stem
    if rep==0:assert r['verified'],stem
    if fingerprint is None:fingerprint=r['final_state_fnv1a64']
    assert r['final_state_fnv1a64']==fingerprint,stem
    r.update(config=config,repeat=rep)
    native_tag=f'{a.fidelity}-{a.memory}{"-"+a.topology if a.topology else ""}'
    native=out/f'{native_tag}-{name}-0.json'
    if native.exists():assert json.loads(native.read_text())['final_state_fnv1a64']==fingerprint,stem
    (out/(stem+'.json')).write_text(json.dumps(r,indent=2)+'\n')
    print('DONE',stem,json.dumps({k:r[k] for k in ('fps','cycles','host_execution_seconds','total_wall_seconds','ui_animation_callbacks')}),flush=True)
  presentation=page.evaluate('''()=>[64,160].map(w=>{
   const h=w===64?48:120,n=w*h,source=new Uint8Array(n),rgba=new Uint8ClampedArray(n*4);
   const canvas=document.querySelector('canvas');canvas.width=w;canvas.height=h;
   const ctx=canvas.getContext('2d');let elapsed=0;
   for(let rep=0;rep<256;++rep){const start=performance.now();
    for(let i=0;i<n;++i){const color=source[i];rgba[i*4]=color;rgba[i*4+1]=color;rgba[i*4+2]=color;rgba[i*4+3]=255;}
    ctx.putImageData(new ImageData(rgba,w,h),0,0);elapsed+=performance.now()-start;
   }return {width:w,height:h,mean_cpu_ms:elapsed/256,includes_worker_transfer:false};
  })''')
  (out/(tag+'-presentation.json')).write_text(json.dumps(presentation,indent=2)+'\n')
  print('PRESENTATION',presentation,flush=True);browser.close()

if __name__=='__main__':main()
