"""Actual compiled game checkpoints displayed by the original circuit canvas."""
from pathlib import Path
from urllib.parse import urljoin
import hashlib,json,os,shutil,sys
from playwright.sync_api import sync_playwright

base=sys.argv[1].rstrip('/')+'/'
query=os.environ.get('CIRCUITSIM_RECORDED_CIRCUIT_URL','?recording=../assets/rv32im/circuits/pong-rally.json')
screens=Path(os.environ.get('CIRCUITSIM_BROWSER_SCREENSHOTS','build/recorded-circuit-browser'))
screens.mkdir(parents=True,exist_ok=True)
source=Path(os.environ.get('CIRCUITSIM_RECORDED_CIRCUIT_FILE',str(Path(__file__).resolve().parents[2]/'site-docs/assets/rv32im/circuits/pong-rally.json')))
snapshot=json.loads(source.read_text())
with sync_playwright() as playwright:
    browser=playwright.chromium.launch(executable_path=shutil.which('google-chrome') or shutil.which('chromium'),
        headless=True,args=['--no-sandbox','--disable-dev-shm-usage','--renderer-process-limit=2','--js-flags=--max-old-space-size=256'])
    context=browser.new_context(viewport={'width':1800,'height':1100})
    page=context.new_page();errors=[];requests=[]
    page.on('pageerror',lambda error:errors.append(str(error)))
    page.on('request',lambda request:requests.append(request.url))
    page.goto(urljoin(base,query),wait_until='domcontentloaded')
    page.wait_for_function("['ready','error'].includes(document.body.dataset.circuitsimStatus)")
    assert page.locator('body').get_attribute('data-circuitsim-status')=='ready',(errors,page.locator('body').inner_text()[-1000:])
    assert page.locator('body').get_attribute('data-circuitsim-runtime')=='recorded'
    assert page.evaluate('app.components.get(app.rootId).type')=='RV32IMDeviceSystem'
    assert page.locator('#recording-status').is_visible()
    assert page.locator('#timing-select').is_disabled()
    components=snapshot['topology']['components']
    display=next(c['id'] for c in components if c['name']=='DISPLAY')
    buttons=next(c['id'] for c in components if c['name']=='BUTTONS')
    assert any(c['name']=='CORE' for c in components)
    assert any(c['name']=='DATA_ROUTER' for c in components)
    assert any(c['name']=='RAM' for c in components)
    assert page.evaluate('app.timestamps.length')==len(snapshot['checkpoints'])
    for index in range(len(snapshot['checkpoints'])):
        actual=page.evaluate('''async ({index,display,buttons})=>{
            await setStateIndex(index);
            const digest=async value=>Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',
                new TextEncoder().encode(JSON.stringify(value))))).map(byte=>byte.toString(16).padStart(2,'0')).join('');
            return {pins:await digest(app.state.pins),wires:await digest(app.state.wires),
                display:app.componentStates.get(display),buttons:app.componentStates.get(buttons)};
        }''',{'index':index,'display':display,'buttons':buttons})
        expected=snapshot['checkpoints'][index]
        digest=lambda value:hashlib.sha256(json.dumps(value,separators=(',',':')).encode()).hexdigest()
        assert actual['pins']==digest(expected['state']['pins']),index
        assert actual['wires']==digest(expected['state']['wires']),index
        assert actual['display']['pixels']==snapshot['recording']['frames'][index]['pixels'],index
        assert actual['buttons']['levels']==snapshot['recording']['frames'][index]['input']['levels'],index
    # Original component inspector, abstraction toggle and backward timeline.
    page.evaluate('id=>setSelection("component",id)',display)
    assert page.locator('#device-inspector-section').is_visible()
    assert '64 × 48' in page.locator('#device-summary').inner_text()
    page.screenshot(path=str(screens/'pong-original-overview.png'),full_page=True)
    page.locator('#device-preview').screenshot(path=str(screens/'pong-display.png'))
    page.evaluate('id=>setSelection("component",id)',buttons)
    assert 'Held:' in page.locator('#device-summary').inner_text()
    page.locator('#device-preview').screenshot(path=str(screens/'pong-keyboard.png'))
    page.locator('#toggle-device-views').click()
    assert page.locator('#toggle-device-views').get_attribute('aria-pressed')=='false'
    page.locator('#toggle-device-views').click()
    await_index=page.evaluate('''async ()=>{await setStateIndex(0);return app.currentIndex;}''')
    assert await_index==0
    page.locator('#play-pause').click()
    page.wait_for_function('app.currentIndex>0')
    page.evaluate('setPlaying(false)')
    assert not any('circuitsim_wasm.' in address for address in requests),'Captured playback started a WASM CPU'
    assert not errors,errors
    result={'guest':snapshot['recording']['guest'],'checkpoints':len(snapshot['checkpoints']),
        'allPinWireStatesEqual':True,'displayAndKeyboardEqual':True,'originalInspectorAndPlayback':True,
        'wasmModuleLoaded':False,'pageErrors':errors}
    (screens/'recorded-circuit-browser.json').write_text(json.dumps(result,indent=2)+'\n')
    browser.close()
print('PASS original circuit canvas: actual game topology, all checkpoints/pins/wires, display/keyboard, inspector and playback')
