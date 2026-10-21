"""Recorded screen playback plus a real scripted WASM bring-up run."""
import json,os,pathlib,shutil,subprocess,sys
from urllib.parse import urljoin
from playwright.sync_api import sync_playwright
base=sys.argv[1].rstrip('/')+'/'
probe=pathlib.Path(sys.argv[2]).resolve()
screens=pathlib.Path(os.environ['CIRCUITSIM_BROWSER_SCREENSHOTS']) if os.environ.get('CIRCUITSIM_BROWSER_SCREENSHOTS') else None
if screens:screens.mkdir(parents=True,exist_ok=True)
# Execute native simulation before launching Chromium to bound peak memory.
record=json.loads(subprocess.check_output([str(probe),'bringup'],text=True,timeout=3600))
def rgba(frame):
    result=[]
    for value,known in zip(bytes.fromhex(frame['pixels']),bytes.fromhex(frame['known'])):
        assert known==255
        r,g,b=value>>5,(value>>2)&7,value&3
        result.extend([(r<<5)|(r<<2)|(r>>1),(g<<5)|(g<<2)|(g>>1),b*85,255])
    return result
with sync_playwright() as p:
    browser=p.chromium.launch(executable_path=shutil.which('google-chrome') or shutil.which('chromium'),headless=True,args=['--no-sandbox','--disable-dev-shm-usage'])
    context=browser.new_context(locale='en-US',viewport={'width':1000,'height':900})
    page=context.new_page();errors=[];page.on('pageerror',lambda error:errors.append(str(error)))
    page.goto(urljoin(base,'simulator/games.html'))
    def load(value):
        # A unique label distinguishes completed async file reads even when
        # the previous recording already enabled Save.
        label=f"file-check-{load.counter}";load.counter+=1
        value={**value,'guest':label}
        page.locator('#file').set_input_files({'name':'recording.json','mimeType':'application/json','buffer':json.dumps(value).encode()})
        page.wait_for_function("label=>document.getElementById('status').textContent.startsWith(label+':')",arg=label)
    load.counter=0
    load(record)
    assert page.locator('#frame').inner_text().startswith('Frame 1 of 2')
    for index,frame in enumerate(record['frames']):
        page.locator('#scrub').evaluate('(element,value)=>{element.value=value;element.dispatchEvent(new Event("input"));}',str(index))
        actual=page.evaluate('Array.from(document.getElementById("screen").getContext("2d").getImageData(0,0,64,48).data)')
        assert actual==rgba(frame),index
    assert page.locator('#keys-summary').inner_text() != 'No input state captured.'
    # A larger exported surface shares the component preview's conversion.
    large={**record,'frames':[{'time':0,'sequence':1,'width':128,'height':96,'stride':128,
        'format':'rgb332','pixels':'e0'*(128*96),'known':'ff'*(128*96),
        'input':{'supported':True,'kind':'keyboard','levels':'1'+'0'*31,'count':'010','head':'0'*32,'overflow':'1'}}]}
    load(large)
    assert page.evaluate('document.getElementById("screen").width') == 128
    assert page.evaluate('document.getElementById("screen").height') == 96
    assert page.evaluate('Array.from(document.getElementById("screen").getContext("2d").getImageData(127,95,1,1).data)') == [255,0,0,255]
    assert 'Held: 31' in page.locator('#keys-summary').inner_text()
    assert 'overflow 1' in page.locator('#keys-summary').inner_text()
    if screens:page.screenshot(path=str(screens/'large-display-keyboard-desktop.png'),full_page=True)
    malformed=json.loads(json.dumps(large))
    malformed['frames'][0]['input'].update(kind='display',width=1000000,height=1000000)
    page.locator('#file').set_input_files({'name':'invalid-keyboard.json','mimeType':'application/json','buffer':json.dumps(malformed).encode()})
    page.wait_for_function("document.getElementById('status').textContent==='Invalid keyboard snapshot'")
    assert page.evaluate('document.getElementById("screen").width') == 128
    for width in [640,360]:
        page.set_viewport_size({'width':width,'height':900})
        assert page.evaluate('document.documentElement.scrollWidth<=innerWidth+1'),width
    if screens:page.screenshot(path=str(screens/'large-display-keyboard-mobile.png'),full_page=True)
    wide=json.loads(json.dumps(record));wide['frames'][0]['time']=str((1<<54)+3);wide['frames'][0]['known']='00'+wide['frames'][0]['known'][2:]
    load(wide)
    assert f'{(1<<54)+3:,}' in page.locator('#frame').inner_text()
    assert page.evaluate('Array.from(document.getElementById("screen").getContext("2d").getImageData(0,0,1,1).data)')==[255,60,255,255]
    empty={**record,'frames':[]};load(empty)
    assert page.locator('#frame').inner_text()=='No captured frame.'
    assert page.locator('#play').is_disabled()
    assert page.locator('#keys-summary').inner_text()=='No input state captured.'
    assert not any(page.evaluate('Array.from(document.getElementById("screen").getContext("2d").getImageData(0,0,64,48).data)'))
    # Deliver deliberately stale callbacks even after terminate, as if they
    # were already queued. They must not stop a replacement worker.
    page.evaluate('''() => {
      window.testWorkers=[];
      window.Worker=class {
        constructor(){this.handlers={};window.testWorkers.push(this);}
        addEventListener(kind,callback){this.handlers[kind]=callback;}
        postMessage(){}
        terminate(){this.terminated=true;}
        emit(kind,event){this.handlers[kind](event);}
      };
    }''')
    page.locator('#run').click();page.locator('#cancel').click();page.locator('#run').click()
    page.evaluate('''() => {
      testWorkers[0].emit('message',{data:{error:'stale message'}});
      testWorkers[0].emit('error',{message:'stale error'});
    }''')
    assert page.locator('#run').is_disabled()
    assert page.locator('#cancel').is_enabled()
    page.evaluate('(value)=>testWorkers[1].emit("message",{data:{record:JSON.stringify(value)}})',record)
    assert page.locator('#run').is_enabled()
    assert page.locator('#frame').inner_text().startswith('Frame 1 of 2')
    # A slow file read must not replace a run started after that read.
    page.evaluate('''() => {
      File.prototype.text=function(){return new Promise(resolve=>window.finishTestFile=resolve);};
    }''')
    page.locator('#file').set_input_files({'name':'slow.json','mimeType':'application/json','buffer':b'{}'})
    page.wait_for_function('typeof window.finishTestFile === "function"')
    page.locator('#run').click()
    page.evaluate('(value)=>window.finishTestFile(JSON.stringify({...value,guest:"stale file"}))',record)
    page.wait_for_timeout(50)
    assert page.locator('#run').is_disabled()
    assert not page.locator('#status').inner_text().startswith('stale file:')
    page.evaluate('testWorkers[2].emit("error",{message:"controlled worker failure"})')
    assert page.locator('#run').is_enabled()
    assert page.locator('#status').inner_text()=='controlled worker failure'
    assert not errors,errors
    context.close()
    # A separate page starts with no previous recording, so readiness proves
    # the worker actually completed guest execution rather than replaying data.
    context=browser.new_context(locale='en-US');page=context.new_page();errors=[]
    page.on('pageerror',lambda error:errors.append(str(error)))
    page.goto(urljoin(base,'simulator/games.html'));page.locator('#run').click()
    page.wait_for_function("!document.getElementById('run').disabled",timeout=3600000)
    assert page.locator('#save').is_enabled(),page.locator('#status').inner_text()
    assert page.locator('#frame').inner_text().startswith('Frame 1 of 2')
    evidence=json.loads(page.locator('#evidence').inner_text())
    for key in ['elfSha256','scriptSha256','cycles','instructions','report','reportKnown','presentations','changedFrames','deviceReads','events']:
        assert evidence[key]==record[key],key
    assert not errors,errors
    context.close();browser.close()
print('PASS: exact pixels, mobile layout, wide time, unknown/empty frames, stale callback rejection and real WASM guest worker')
