"""Actual compiled game checkpoints displayed by the original circuit canvas."""
from pathlib import Path
from urllib.parse import urljoin
import gzip,hashlib,json,os,shutil,sys
from playwright.sync_api import sync_playwright

base=sys.argv[1].rstrip('/')+'/'
query=os.environ.get('CIRCUITSIM_RECORDED_CIRCUIT_URL','?recording=../assets/system/long-games/sokoban-long/playlist.json')
screens=Path(os.environ.get('CIRCUITSIM_BROWSER_SCREENSHOTS','build/recorded-circuit-browser'))
screens.mkdir(parents=True,exist_ok=True)
source=Path(os.environ.get('CIRCUITSIM_RECORDED_CIRCUIT_FILE',str(Path(__file__).resolve().parents[2]/'site-docs/assets/system/long-games/sokoban-long/playlist.json')))
def load(path):
    data=Path(path).read_bytes()
    return json.loads(gzip.decompress(data) if data[:2]==b'\x1f\x8b' else data)
snapshot=load(source)
if snapshot.get('kind')=='circuitsim-circuit-playlist':
    playlist=snapshot;parts=[load(source.parent/part['file']) for part in playlist['parts']]
    snapshot=parts[0].copy();snapshot['checkpoints']=[c for part in parts for c in part['checkpoints']]
    snapshot['recording']={**parts[0]['recording'],'frames':[f for part in parts for f in part['recording']['frames']]}
    source=source.parent/playlist['parts'][0]['file']
if snapshot.get('schemaVersion')==2:
    snapshot['topology']=load(source.parent/snapshot['topologyRef'])['topology']
with sync_playwright() as playwright:
    browser=playwright.chromium.launch(executable_path=shutil.which('google-chrome') or shutil.which('chromium'),
        headless=True,args=['--no-sandbox','--disable-dev-shm-usage','--renderer-process-limit=2','--js-flags=--max-old-space-size=256'])
    context=browser.new_context(viewport={'width':1800,'height':1100})
    page=context.new_page();errors=[];requests=[]
    page.on('pageerror',lambda error:errors.append(str(error)))
    page.on('request',lambda request:requests.append(request.url))
    page.goto(urljoin(base,query),wait_until='domcontentloaded')
    page.wait_for_function("['ready','error'].includes(document.body.dataset.circuitsimStatus)",timeout=120_000)
    assert page.locator('body').get_attribute('data-circuitsim-status')=='ready',(errors,page.locator('body').inner_text()[-1000:])
    assert page.locator('body').get_attribute('data-circuitsim-runtime')=='recorded'
    assert page.evaluate('app.components.get(app.rootId).type')=='ComputerSystem'
    assert page.locator('#recording-status').is_visible()
    assert page.locator('#timing-select').is_disabled()
    assert page.locator('#fidelity-select').is_disabled()
    components=snapshot['topology']['components']
    display=next(c['id'] for c in components if c.get('deviceViewKind')=='display')
    buttons=next(c['id'] for c in components if c.get('deviceViewKind')=='keyboard')
    assert any(c['name']=='CORE' for c in components)
    assert any(c['name']=='BUS' for c in components)
    assert any(c['name']=='MEMORY' for c in components)
    assert any(c.get('deviceViewKind')=='bus' for c in components)
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
    # The view toggle also works with a fixed recording. Outermost devices
    # hide nested abstractions and selecting a hidden child focuses its owner.
    page.evaluate('setSelection(null,null)')
    before=page.evaluate('''()=>JSON.stringify({session:app.sessionId,index:app.currentIndex,
        overrides:app.explorer.draftOverrides,layout:app.layout,state:app.state})''')
    assert page.locator('#toggle-device-views').get_attribute('aria-pressed')=='false'
    page.locator('#toggle-device-views').click()
    page.wait_for_function('deviceViewsEnabled()')
    coverage=page.evaluate('''()=>{
        const pictures=app.componentOrder.filter(deviceViewComponent);
        const nested=app.componentOrder.filter(c=>canvasAbstractionKind(c)&&outermostAbstraction(c)!==c);
        return {kinds:[...new Set(pictures.map(canvasAbstractionKind))],
            devices:pictures.filter(c=>CircuitDeviceView.supports(c)).map(c=>c.deviceViewKind),
            nested:nested.length,hidden:nested.every(c=>!componentDrawn(c)),
            pair:nested.map(c=>({child:c.id,parent:outermostAbstraction(c).id}))
                .find(p=>app.components.get(p.parent).deviceViewKind==='console')};
    }''')
    assert {'display','keyboard','bus','console'}.issubset(coverage['devices']),coverage
    assert coverage['nested']>0 and coverage['hidden'],coverage
    assert coverage['pair'],coverage
    pair=coverage['pair']
    page.evaluate('id=>smoothlyFocusComponent(id)',pair['child'])
    page.wait_for_function('!app.cameraAnimation')
    assert page.evaluate('app.selection')=={'type':'component','id':pair['parent']}
    assert page.evaluate('''()=>JSON.stringify({session:app.sessionId,index:app.currentIndex,
        overrides:app.explorer.draftOverrides,layout:app.layout,state:app.state})''')==before
    # Backward seeking must refresh the visible pictures from that checkpoint.
    page.evaluate('async ()=>{await setStateIndex(0)}')
    assert page.evaluate('id=>app.componentStates.get(id).pixels',display)==snapshot['recording']['frames'][0]['pixels']
    page.screenshot(path=str(screens/'pong-device-view.png'),full_page=True)
    page.locator('#toggle-device-views').click()
    assert page.locator('#toggle-device-views').get_attribute('aria-pressed')=='false'
    # Behavioral devices retain their actual picture with the toggle off.
    page.evaluate('id=>smoothlyFocusComponent(id)',display)
    page.wait_for_function('!app.cameraAnimation')
    rendered=page.evaluate('''id=>{
        const c=app.components.get(id),pictures=[],dashes=[];
        const draw=CircuitDeviceView.draw,dash=ctx.setLineDash;
        CircuitDeviceView.draw=(context,rect,state)=>{pictures.push({kind:state.kind,pixels:state.pixels});return draw(context,rect,state);};
        ctx.setLineDash=function(pattern){if(pattern.length)dashes.push(pattern);return dash.call(this,pattern);};
        try {drawComponentForeground(c,c.rect);}
        finally {CircuitDeviceView.draw=draw;ctx.setLineDash=dash;}
        return {enabled:deviceViewsEnabled(),pictures,dashes};
    }''',display)
    assert rendered=={'enabled':False,'pictures':[{'kind':'display','pixels':snapshot['recording']['frames'][0]['pixels']}],'dashes':[]},rendered
    # Original component inspector and backward timeline.
    page.evaluate('id=>setSelection("component",id)',display)
    assert page.locator('#device-inspector-section').is_visible()
    assert '64 × 48' in page.locator('#device-summary').inner_text()
    page.screenshot(path=str(screens/'pong-original-overview.png'),full_page=True)
    page.locator('#device-preview').screenshot(path=str(screens/'pong-display.png'))
    page.evaluate('id=>setSelection("component",id)',buttons)
    assert 'Held:' in page.locator('#device-summary').inner_text()
    page.locator('#device-preview').screenshot(path=str(screens/'pong-keyboard.png'))
    # A recording's fidelity is fixed: its explorer arrows cannot plan a rebuild.
    assert page.locator('.explorer-arrow:not([disabled])').count()==0
    await_index=page.evaluate('''async ()=>{await setStateIndex(0);return app.currentIndex;}''')
    assert await_index==0
    page.locator('#play-pause').click()
    page.wait_for_function('app.currentIndex>0')
    page.evaluate('setPlaying(false)')
    assert not any('circuitsim_wasm.' in address for address in requests),'Captured playback started a WASM CPU'
    assert not errors,errors
    result={'guest':snapshot['recording']['guest'],'checkpoints':len(snapshot['checkpoints']),
        'allPinWireStatesEqual':True,'displayAndKeyboardEqual':True,'originalInspectorAndPlayback':True,
        'outermostDeviceView':coverage,'behavioralPictureWithDeviceViewOff':True,
        'solidComponentOutlines':True,'wasmModuleLoaded':False,'pageErrors':errors}
    (screens/'recorded-circuit-browser.json').write_text(json.dumps(result,indent=2)+'\n')
    browser.close()
print('PASS original circuit canvas: actual game topology, all checkpoints/pins/wires, display/keyboard, inspector and playback')
