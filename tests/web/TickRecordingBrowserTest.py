"""Full-tick native capture in the ordinary circuit canvas; no WASM CPU."""
from pathlib import Path
from urllib.parse import urljoin
import hashlib,json,os,shutil,sys
from playwright.sync_api import sync_playwright

base=sys.argv[1].rstrip('/')+'/'
source=Path(os.environ['CIRCUITSIM_TICK_RECORDING_FILE'])
query=os.environ.get('CIRCUITSIM_TICK_RECORDING_URL','?recording=../captures/ticks.json')
record=json.loads(source.read_text());playlist=json.loads((source.parent/record['presentations']).read_text())
parts=[json.loads((source.parent/part['file']).read_text()) for part in playlist['parts']]
checkpoints=[c for part in parts for c in part['checkpoints']]
screens=Path(os.environ.get('CIRCUITSIM_BROWSER_SCREENSHOTS','build/tick-recording-browser'));screens.mkdir(parents=True,exist_ok=True)
digest=lambda value:hashlib.sha256(json.dumps(value,separators=(',',':')).encode()).hexdigest()
with sync_playwright() as p:
    browser=p.chromium.launch(executable_path=shutil.which('google-chrome') or shutil.which('chromium'),headless=True,
        args=['--no-sandbox','--disable-dev-shm-usage','--disable-gpu','--no-zygote','--single-process','--js-flags=--max-old-space-size=192'])
    page=browser.new_page(viewport={'width':1800,'height':1100});errors=[];requests=[]
    page.on('pageerror',lambda error:errors.append(str(error)));page.on('request',lambda req:requests.append(req.url))
    page.goto(urljoin(base,query),wait_until='domcontentloaded')
    page.wait_for_function("['ready','error'].includes(document.body.dataset.circuitsimStatus)",timeout=120000)
    assert page.locator('body').get_attribute('data-circuitsim-status')=='ready',(errors,page.locator('body').inner_text()[-1000:])
    assert page.evaluate('app.timestampCount')==record['timestampCount']>len(checkpoints)
    assert int(page.locator('#time-slider').get_attribute('max'))==record['timestampCount']-1
    assert 'signal ticks' in page.locator('#recording-status').inner_text()
    assert page.locator('#fidelity-select').is_disabled() and page.locator('#timing-select').is_disabled()
    for marker,checkpoint in zip(record['markers'],checkpoints):
        actual=page.evaluate('''async index=>{
          await setStateIndex(index);
          const digest=async v=>{
            const hash=await crypto.subtle.digest('SHA-256',new TextEncoder().encode(JSON.stringify(v)));
            return Array.from(new Uint8Array(hash),b=>b.toString(16).padStart(2,'0')).join('');
          };
          return {pins:await digest(app.state.pins),wires:await digest(app.state.wires),time:app.state.time};
        }''',marker['index'])
        assert actual['pins']==digest(checkpoint['state']['pins']) and actual['wires']==digest(checkpoint['state']['wires'])
        assert int(actual['time'])==int(marker['time'])
    # Step off a PRESENT to a real intervening signal tick, then rewind and
    # exercise the usual clock/device/register/memory inspector paths.
    first=record['markers'][0]['index'];assert first>1
    page.evaluate('async index=>{await setStateIndex(index)}',first)
    page.locator('#step-back').click();page.wait_for_function('app.currentIndex===app.checkpoints[0].index-1')
    assert 'PRESENT' not in page.locator('#time-label').inner_text()
    sample=page.evaluate('''async ()=>{
      const snapshots=[];
      for(const index of [0,Math.floor(app.checkpoints[0].index/2),app.checkpoints[0].index-1,app.timestampCount-1]){
        await setStateIndex(index);snapshots.push({index,time:app.state.time,pins:app.state.pins.slice(0,24)});
      }
      const display=[...app.components.values()].find(c=>c.deviceViewKind==='display');
      const registers=[...app.components.values()].find(c=>c.registerView);
      const ram=[...app.components.values()].find(c=>c.name==='RAM'&&c.parentId===app.rootId);
      const read=async id=>(await visualizerRequest(`api/component-state?session=${encodeURIComponent(app.sessionId)}&index=${app.currentIndex}&componentId=${encodeURIComponent(id)}`)).json();
      return {snapshots,display:await read(display.id),registers:await read(registers.id),ram:await read(ram.id),displayId:display.id};
    }''')
    assert all(s.get('supported') for s in [sample['display'],sample['registers'],sample['ram']]),{
        k:{field:value for field,value in sample[k].items() if field in ('componentId','supported','reason','type')} for k in ('display','registers','ram')}
    assert len(sample['registers']['registers'])==32 and sample['ram']['windowWords']
    assert len(sample['display']['pixels'])==sample['display']['stride']*sample['display']['height']*2
    assert len({int(s['time']) for s in sample['snapshots']})==4
    page.evaluate('id=>setSelection("component",id)',sample['displayId'])
    assert page.locator('#device-inspector-section').is_visible()
    page.evaluate('async ()=>{await setStateIndex(0)}')
    page.locator('#checkpoint-forward').click();page.wait_for_function('app.currentIndex===app.checkpoints[0].index')
    page.evaluate('async ()=>{await setStateIndex(0)}');page.locator('#play-pause').click()
    page.wait_for_function('app.currentIndex>0');page.evaluate('setPlaying(false)')
    page.screenshot(path=str(screens/'full-tick-canvas.png'),full_page=True)
    assert not any('circuitsim_wasm.' in url for url in requests)
    assert not errors,errors
    (screens/'tick-recording-browser.json').write_text(json.dumps({'ticks':record['timestampCount'],'presentations':len(checkpoints),
        'allPresentationStatesEqual':True,'intermediateTickStepping':True,'deviceRegisterMemoryViews':True,'playAndRewind':True,
        'wasmModuleLoaded':False,'pageErrors':errors},indent=2)+'\n')
    browser.close()
print('PASS full-tick canvas: intermediate stepping, PRESENT equality, device/register/memory views, play and rewind')
