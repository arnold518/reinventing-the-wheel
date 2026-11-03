"""Switch real system presets on the machine's structure scenario, preserve saved
geometry and reject Gate safely."""
import json,shutil,sys
from playwright.sync_api import sync_playwright

def settled(page):
    page.wait_for_function("['ready','error'].includes(document.body.dataset.circuitsimStatus)",timeout=120000)
    assert page.evaluate("document.body.dataset.circuitsimStatus")=='ready',page.locator('#loading').inner_text()

with sync_playwright() as p:
    browser=p.chromium.launch(executable_path=shutil.which('google-chrome') or shutil.which('chromium'),headless=True,
        args=['--no-sandbox','--disable-dev-shm-usage','--renderer-process-limit=2','--js-flags=--max-old-space-size=256'])
    context=browser.new_context(viewport={'width':1440,'height':1000});page=context.new_page();errors=[]
    page.on('pageerror',lambda e:errors.append(str(e)))
    base=sys.argv[1].rstrip('/')+'/'
    page.goto(base+'?scenario=computer-system-structure');settled(page)
    assert page.locator('#fidelity-select option[value=gate]').evaluate('option=>option.disabled'),page.evaluate('app.profile')
    assert 'main memory' in page.locator('#fidelity-select').get_attribute('title')
    # The machine opens in its catalog profile; there is no scenario-defined one.
    assert page.evaluate('app.profile.fidelityPreset')=='visual' and page.locator('#fidelity-select').input_value()=='visual'
    assert page.locator('#fidelity-select option[value=scenario]').count()==0
    for preset,core,display in [('logic','structural','structural'),('compact','behavioral','behavioral'),
                                ('visual','structural','behavioral')]:
        page.select_option('#fidelity-select',preset);settled(page)
        actual=page.evaluate('''()=>({preset:app.profile.fidelityPreset,root:app.components.get(app.rootId).fidelity,
          core:app.components.get(app.rootId+'.CORE').fidelity,display:app.components.get(app.rootId+'.DISPLAY').fidelity,
          repaired:[...app.components.values()].filter(c=>c.layoutType?.includes('[config-fidelity-')).length})''')
        assert actual['preset']==preset and actual['root']=='structural' and actual['core']==core and actual['display']==display,actual
        issues=page.evaluate('''()=>{const issues=[];
          for(const parent of app.components.values())if(parent.layoutType?.includes('[config-fidelity-')){
            const p=parent.rect;if(!p)continue;
            for(const id of parent.childIds){const child=app.components.get(id),r=child.rect;
              if(!r||r.x<p.x-.01||r.y<p.y-.01||r.x+r.w>p.x+p.w+.01||r.y+r.h>p.y+p.h+.01)issues.push(id);
            }
          }return issues.slice(0,10);}''')
        assert not issues,(preset,issues)
        assert page.locator('#fidelity-select option[value=gate]').evaluate('option=>option.disabled')
        print(json.dumps(actual),flush=True)
    page.select_option('#fidelity-select','visual');settled(page)
    saved=page.evaluate('''async ()=>{
      const entry=app.layout.root_layouts[app.layoutKey].children.CORE;
      entry.rel_pos[0]+=.001;
      app.layout.type_layouts.UNRELATED_SAVED_SENTINEL={color:[12,34,56,255]};
      const result=await visualizerRequest(apiUrl('api/layout'),{method:'POST',body:JSON.stringify({layout:app.layout})});
      if(!result.ok)throw Error(await result.text());return entry.rel_pos[0];}''')
    page.select_option('#fidelity-select','compact');settled(page)
    page.select_option('#fidelity-select','visual');settled(page)
    assert page.evaluate('app.layout.root_layouts[app.layoutKey].children.CORE.rel_pos[0]')==saved
    assert page.evaluate('app.layout.type_layouts.UNRELATED_SAVED_SENTINEL.color')==[12,34,56,255]
    page.goto(base+'?scenario=computer-system-structure&fidelity=gate')
    page.wait_for_function("document.body.dataset.circuitsimStatus==='error'",timeout=30000)
    assert 'main memory' in page.locator('#loading').inner_text()
    assert '[object Object]' not in page.locator('#loading').inner_text()
    page.select_option('#fidelity-select','logic');settled(page)
    assert page.evaluate('app.profile.fidelityPreset')=='logic'
    assert not errors,errors
    browser.close()
print('PASS system preset switching, actual selections, repaired geometry, saved layouts and readable Gate recovery')
