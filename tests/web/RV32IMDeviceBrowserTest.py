"""New component pages and recorded WASM examples, one session at a time."""
from pathlib import Path
import json
import os
import re
import shutil
import sys
from urllib.parse import urljoin

from playwright.sync_api import sync_playwright


def check_fidelity_layouts(page, alias, expected_root):
    """Apply real explorer toggles and reject caught layout errors as well."""
    root = page.evaluate("""()=>{const r=app.components.get(app.rootId);return {
        id:r.id,name:r.name,fidelity:r.fidelity,availableFidelities:r.availableFidelities};}""")
    assert root['id'] == root['name'] == expected_root, (alias, root['id'], root['name'])
    assert root['fidelity'] == 'structural', (alias, root['fidelity'])

    def check_scope(path):
        page.evaluate("id=>smoothlyFocusComponent(id)", path)
        page.wait_for_function('!app.geometryDirty && !app.cameraAnimation && !app.topologyRequestInFlight && !app.pendingScopeIds.size')
        problems = page.evaluate('''id=>{
            const parent=app.components.get(id),p=parent.rect;
            const children=parent.childIds.map(id=>app.components.get(id));
            const problems=[];
            for(const child of children){const r=child.rect;
                if(!r || r.x<p.x-.01 || r.y<p.y-.01 || r.x+r.w>p.x+p.w+.01 || r.y+r.h>p.y+p.h+.01)
                    problems.push(['outside',child.id]);}
            for(let i=0;i<children.length;++i)for(let j=i+1;j<children.length;++j){
                const a=children[i].rect,b=children[j].rect;if(!a||!b)continue;
                if(Math.min(a.x+a.w,b.x+b.w)-Math.max(a.x,b.x)>.01 &&
                   Math.min(a.y+a.h,b.y+b.h)-Math.max(a.y,b.y)>.01)
                    problems.push(['overlap',children[i].id,children[j].id]);}
            return problems.slice(0,5);
        }''', path)
        assert not problems, (alias, path, problems)
        if alias == 'muldiv-w32' and path == expected_root:
            widths = page.evaluate('''id=>{const p=app.components.get(id);
                if(p.fidelity!=='structural')return null;
                return ['MULTIPLY','DIVIDE'].map(name=>app.components.get(id+'.'+name).rect.w/p.rect.w);
            }''', path)
            if widths is not None:
                assert all(width > .25 for width in widths), (alias, widths)

    def toggle(path, fidelity):
        page.locator('#explorer-search').fill(path)
        row = page.locator('.explorer-row').filter(has=page.locator(
            '.explorer-name', has_text=re.compile('^' + re.escape(path.split('.')[-1]) + '$'))).last
        assert row.locator('.explorer-arrow').is_enabled(), (alias, path)
        row.locator('.explorer-arrow').click()
        page.locator('#apply-profile').click()
        page.wait_for_function("document.querySelector('#loading').classList.contains('hidden')", timeout=120000)
        status = page.locator('#explorer-status')
        assert not status.evaluate("e=>e.classList.contains('error')"), (alias, path, status.inner_text())
        assert page.evaluate("id=>app.components.get(id)?.fidelity", path) == fidelity
        check_scope(path)

    check_scope(expected_root)

    # The compact form (one box, or a device's compiled groups) keeps no
    # structural part, and must return to a fully placed structural view.
    if 'behavioral' in root['availableFidelities']:
        toggle(expected_root, 'behavioral')
        structural = page.evaluate('''()=>[...app.components.values()].filter(c=>
            c.id!==app.rootId && c.fidelity==='structural').map(c=>c.id).slice(0,5)''')
        assert not structural, (alias, structural)
        toggle(expected_root, 'structural')
    child = page.evaluate('''()=>app.componentOrder.find(c=>c.id!==app.rootId &&
        c.profileSelectable && c.fidelity==='behavioral' && c.availableFidelities.includes('structural'))?.id''')
    if child:
        toggle(child, 'structural')
        toggle(child, 'behavioral')
    page.locator('#explorer-search').fill('')
    check_scope(expected_root)


def check_device_hierarchy(page, alias, expected_root):
    """Inspect each distinct functional device scope, including nested groups."""
    expected = {
        'clocked-counter': {'TICK_CONTROL','COUNT_STATE','INCREMENT'},
        'synchronous-fifo': {'CONTROL','POINTERS','STORAGE'},
        'device-register-port-w32': {'REQUEST','CONTROL','EXECUTE','RESPONSE'},
        'bus-router': {'CONTROL','DECODE','REQUEST','RESPONSE_SELECT','ROUTE_DEVICE','ROUTE_MEMORY'},
        'memory-subsystem': {'ARBITER','RAM'},
        'console-device': {'PORT','REGISTERS','TX_BYTE','TX_FIFO','ZERO_0'},
        'timer-device': {'COUNTER','ONE_0','PORT','REGISTERS'},
        'button-device': {'EVENTS','EVENT_ENCODING','INPUT_UPDATE','LEVELS','ONE_0','PORT','REGISTERS','STATUS_BITS'},
        'framebuffer-display': {'FRAME_COUNTER','ONE_0','PIXELS','PORT','REGISTERS'},
        'led-panel': {'BLINK_COUNTER','LIGHTS','ONE_0','PORT','REGISTERS'},
        'computer-system-structure': {'BUS','BUTTONS','CONSOLE','CORE','DISPLAY','MEMORY','TIMER'},
    }
    actual=page.evaluate('()=>app.components.get(app.rootId).childIds.map(id=>app.components.get(id).name)')
    assert set(actual)==expected[alias],(alias,actual)
    menu=page.evaluate("()=>[...document.querySelectorAll('#scenario-select optgroup')].slice(0,3).map(g=>g.label)")
    assert menu==['Logic gates and storage cells','Wiring, multiplexers and decoders','ALU / Chapter 2 components'],menu
    public = ('ClockedCounter<','SynchronousFifo<','DeviceRegisterPort<','BusRouter<','BusFetchRouter<','Bus<',
              'BusBridge<','MemorySubsystem','LedPanel',
              'ConsoleDevice','TimerDevice','ButtonDevice','FramebufferDisplay','TwoPortMemoryArbiter<')
    private = ('ClockTick','ClockDivider','CounterIncrement<','Fifo','DeviceRegister',
               'DeviceAddress','DeviceRouter','DeviceRoute','DeviceStatusMux','FetchAddress',
               'FetchRequest','RouterRequest','ConsoleCommand','ConsoleRead','ConsoleRegister',
               'TimerAccess','TimerRead','TimerRegister','ButtonInput','ButtonLevel','ButtonEvent',
               'ButtonCommands','ButtonRead','ButtonRegister','ButtonStatusBits','RegisterFrontEnd<',
               'ConsoleTxByte','LedBlink','DisplayFrameCounter')
    pending=[expected_root];seen=set();count=0
    while pending:
        path=pending.pop(0)
        component=page.evaluate('id=>{const c=app.components.get(id);return c&&{type:c.type,key:c.layoutType||c.type,children:c.childIds};}',path)
        if not component or not component['children'] or component['key'] in seen:
            continue
        seen.add(component['key'])
        page.evaluate('id=>smoothlyFocusComponent(id)',path)
        page.wait_for_function('!app.geometryDirty && !app.cameraAnimation && !app.topologyRequestInFlight && !app.pendingScopeIds.size')
        details=page.evaluate('''id=>{
            const p=app.components.get(id),r=p.rect,issues=[];
            const children=p.childIds.map(cid=>app.components.get(cid));
            for(const c of children){const b=c?.rect;
                if(!b||b.x<r.x-.01||b.y<r.y-.01||b.x+b.w>r.x+r.w+.01||b.y+b.h>r.y+r.h+.01)issues.push(['outside',c?.id]);}
            for(let i=0;i<children.length;++i)for(let j=i+1;j<children.length;++j){const a=children[i]?.rect,b=children[j]?.rect;
                if(a&&b&&Math.min(a.x+a.w,b.x+b.w)-Math.max(a.x,b.x)>.01&&Math.min(a.y+a.h,b.y+b.h)-Math.max(a.y,b.y)>.01)issues.push(['overlap',children[i].id,children[j].id]);}
            return {issues:issues.slice(0,5),children:children.map(c=>({id:c.id,type:c.type}))};}''',path)
        assert not details['issues'],(alias,path,details['issues'])
        assert not page.locator('#explorer-status').evaluate("e=>e.classList.contains('error')"),(alias,path)
        for child in details['children']:
            if child['type'].startswith(public+private):pending.append(child['id'])
        count+=1
    page.evaluate('id=>smoothlyFocusComponent(id)',expected_root)
    page.wait_for_function('!app.geometryDirty && !app.cameraAnimation')
    if alias=='computer-system-structure':
        fonts=page.evaluate('''()=>app.components.get(app.rootId).childIds.map(id=>{
            const c=app.components.get(id),p=componentDrawParts(c),r=rectToScreenRect(p.titleRect);
            return [c.name,Math.min(c.rect.w*c.fontWidthRatio*app.camera.zoom,r.h*.68,MAX_COMPONENT_FONT_PX)];})''')
        assert all(size>=6 for name,size in fonts),(alias,'hidden overview captions',fonts)
    print(f'{alias}: inspected {count} distinct device hierarchy scopes',flush=True)

def main():
    base = sys.argv[1].rstrip('/') + '/'
    root = Path(__file__).resolve().parents[2]
    catalog = json.loads((root / 'visualizer-v2/static/scenarios.json').read_text())
    roots = {catalog['aliases'][test][0]: entry['root']
             for test, entry in catalog['components'].items() if 'root' in entry}
    screens = Path(os.environ['CIRCUITSIM_BROWSER_SCREENSHOTS']) if os.environ.get('CIRCUITSIM_BROWSER_SCREENSHOTS') else None
    if screens:
        screens.mkdir(parents=True, exist_ok=True)
    cases = []
    with sync_playwright() as playwright:
        browser = playwright.chromium.launch(
            executable_path=shutil.which('google-chrome') or shutil.which('chromium'),
            headless=True, args=['--no-sandbox', '--disable-dev-shm-usage',
                                 '--renderer-process-limit=2', '--js-flags=--max-old-space-size=512'])
        for source in sorted((root / 'site-docs' / 'system').rglob('*.md')):
            for alias, _ in re.findall(r'<!-- simulator-examples: (\S+) (\S+) -->', source.read_text()):
                if alias not in roots:
                    continue
                relative = source.relative_to(root / 'site-docs').with_suffix('')
                url = (relative.parent if relative.name == 'index' else relative).as_posix() + '/'
                context = browser.new_context(java_script_enabled=False)
                page = context.new_page()
                response = page.goto(urljoin(base, url))
                assert response.ok, source
                block = page.locator(f'.example-table-block[data-scenario="{alias}"]')
                rows = block.locator('tbody tr').evaluate_all('''rows => rows.map(row => ({
                    label:row.dataset.checkpoint,time:Number(row.dataset.time),index:Number(row.dataset.timelineIndex),
                    pins:Object.fromEntries([...row.querySelectorAll('td[data-bits]')].filter(c=>c.dataset.bits)
                        .map(c=>[c.dataset.pin,c.dataset.bits]))}))''')
                assert rows, alias
                assert page.locator(f'iframe[src*="scenario={alias}&"]').count() == 1, (alias, source)  # exact alias, not a prefix
                for width in (640, 360):
                    page.set_viewport_size({'width': width, 'height': 1000})
                    assert page.evaluate('document.documentElement.scrollWidth <= innerWidth+1'), source
                cases.append((alias, block.get_attribute('data-profile'), rows))
                context.close()
        # The machine scenarios run a program and have no example table
        # (GameScenarioBrowserTest and the parity check cover guest runs).
        programs = {'computer-system', 'computer-system-two-bus', 'computer-system-registered-bridge',
                    'computer-system-leds', 'computer-system-checked'}
        chapter = {alias for alias in roots if alias not in programs and catalog['components'][next(t for t, a in catalog['aliases'].items() if a[0] == alias)].get('group') in ('rv32im', 'system')}
        assert {case[0] for case in cases} == chapter, sorted(chapter ^ {case[0] for case in cases})
        pattern = os.environ.get('CIRCUITSIM_BROWSER_SCENARIO_FILTER')
        selected = [case for case in cases if not pattern or re.search(pattern, case[0])]
        assert selected, 'No browser scenarios match the selected filter'
        for alias, profile, rows in selected:
            context = browser.new_context(viewport={'width': 1440, 'height': 1000})
            page = context.new_page()
            errors = []
            page.on('pageerror', lambda error: errors.append(str(error)))
            page.goto(urljoin(base, f'simulator/?scenario={alias}'), wait_until='domcontentloaded')
            page.locator("body[data-circuitsim-status='ready']").wait_for(timeout=120000)
            assert page.locator('body').get_attribute('data-circuitsim-runtime') == 'wasm'
            actual = page.evaluate('''() => ({profile:app.components.get(app.rootId).profileFingerprint,
                checkpoints:app.checkpoints.map(c=>({label:c.label,time:c.time,index:c.index}))})''')
            assert actual['profile'] == profile, alias
            assert actual['checkpoints'] == [{k: row[k] for k in ('label', 'time', 'index')} for row in rows], alias
            for row in rows:
                pins = page.evaluate('''async index => {await setStateIndex(index);return Object.fromEntries(
                    [...app.pins.values()].filter(p=>p.componentId===app.rootId).map(p=>[p.name,app.state.pins[p.stateIndex]]));}''', row['index'])
                assert all(pins[name] == bits for name, bits in row['pins'].items()), (alias, row['label'])
            if alias in roots:
                if alias == 'muldiv-w32':
                    # A browser saved before the root rename must still open
                    # and apply its root/descendant fidelity choices.
                    page.evaluate('''()=>new Promise((resolve,reject)=>{
                        const request=indexedDB.open('circuitsim-visualizer-v2',1);
                        request.onerror=()=>reject(request.error);
                        request.onsuccess=()=>{const db=request.result,tx=db.transaction('state','readwrite');
                            tx.objectStore('state').put({schema_version:1,scenarios:{'muldiv-w32':{
                                revision:4,exact_overrides:{DUT:'structural','DUT.OPERATION':'behavioral'}}}},'profiles');
                            tx.oncomplete=()=>{db.close();resolve();};tx.onerror=()=>reject(tx.error);};
                    })''')
                    page.reload(wait_until='domcontentloaded')
                    page.locator("body[data-circuitsim-status='ready']").wait_for(timeout=120000)
                    assert page.evaluate('app.profile.exactOverrides') == {
                        roots[alias]: 'structural', roots[alias] + '.OPERATION': 'behavioral'}
                check_fidelity_layouts(page, alias, roots[alias])
                if alias in ('clocked-counter','synchronous-fifo','device-register-port-w32',
                             'bus-router','memory-subsystem','console-device','timer-device','button-device',
                             'framebuffer-display','led-panel','computer-system-structure'):
                    check_device_hierarchy(page,alias,roots[alias])
                    if screens:
                        page.screenshot(path=str(screens/(alias+'-hierarchy.png')))
                if screens and alias in ('muldiv-w32', 'rv32im-core', 'computer-system-structure'):
                    page.screenshot(path=str(screens / (alias + '-structural.png')), full_page=True)
            if screens and alias == 'alu':
                page.wait_for_function('!app.geometryDirty && !app.cameraAnimation && !app.topologyRequestInFlight && !app.pendingScopeIds.size')
                page.screenshot(path=str(screens / 'alu-default-prefix.png'), full_page=True)
            device = page.evaluate('''async () => {
                const component=app.components.get(app.rootId);
                if(!CircuitDeviceView.supports(component.type))return null;
                await setStateIndex(app.timestamps.length-1);
                setSelection('component',app.rootId);
                return app.componentStates.get(app.rootId);
            }''')
            if device:
                assert device['supported'], (alias,device)
                assert page.locator('#device-inspector-section').is_visible()
                assert page.locator('#device-summary').inner_text()
                # Collapsing a selectable structural device root in the
                # explorer draws it closed at once (its device view), without
                # a rebuild; Revert restores the built tree.
                root_row = page.locator('.explorer-row').first
                if root_row.locator('.explorer-arrow').is_enabled() and page.evaluate(
                        'app.components.get(app.rootId).fidelity') == 'structural':
                    session = page.evaluate('app.sessionId')
                    root_row.locator('.explorer-arrow').click()
                    assert not page.evaluate('componentOpen(app.components.get(app.rootId))'), alias
                    assert page.evaluate('app.sessionId') == session, alias
                    page.locator('#revert-profile').click()
                    assert page.evaluate('plannedFidelity(app.components.get(app.rootId))') == 'structural', alias
                if device['kind'] == 'display':
                    # Red around green, with (1, 1) turned blue by a byte-masked write.
                    assert device['pixels'].startswith('e0e0e0e0e0031ce0e01c1ce0e0e0e0e0'), device
                if device['kind'] == 'keyboard':
                    # The scenario ends with the overflow cleared and four of the
                    # five presses queued; all five buttons are held.
                    assert device['levels'] == '0'*27+'11111' and int(device['count'],2) == 4 and device['overflow'] == '0', device
                if screens and device['kind'] in ('display', 'keyboard'):
                    page.screenshot(path=str(screens / (alias + '.png')), full_page=True)
                await_rewind = page.evaluate('''async () => { await setStateIndex(0); return app.componentStates.get(app.rootId); }''')
                assert await_rewind['time'] == actual['checkpoints'][0]['time'] or int(await_rewind['time']) <= int(device['time'])
            assert not errors, errors
            context.close()
            print(f'{alias}: component page, mobile bounds and recorded WASM checkpoints passed', flush=True)
        # The existing single-cycle core keeps its structural inspection default.
        context = browser.new_context(viewport={'width': 1440, 'height': 1000})
        page = context.new_page()
        errors = []
        page.on('pageerror', lambda error: errors.append(str(error)))
        page.goto(urljoin(base, 'simulator/?scenario=rv32i-core'), wait_until='domcontentloaded')
        page.locator("body[data-circuitsim-status='ready']").wait_for(timeout=120000)
        check_fidelity_layouts(page, 'rv32i-core', 'RV32I_SINGLE_CYCLE_CORE_ROOT')
        assert not errors, errors
        context.close()
        browser.close()
    print(f'{len(cases)} component pages and {len(selected) + 1} recorded sessions passed (including rv32i-core)')


if __name__ == '__main__':
    main()
