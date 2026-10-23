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
    # A one-box compact form must return to a fully placed structural view.
    if 'behavioral' in root['availableFidelities']:
        toggle(expected_root, 'behavioral')
        assert page.evaluate('app.components.size') == 1, alias
        toggle(expected_root, 'structural')
    child = page.evaluate('''()=>app.componentOrder.find(c=>c.id!==app.rootId &&
        c.profileSelectable && c.fidelity==='behavioral' && c.availableFidelities.includes('structural'))?.id''')
    if child:
        toggle(child, 'structural')
        toggle(child, 'behavioral')
    page.locator('#explorer-search').fill('')
    check_scope(expected_root)


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
        for directory in ('rv32im', 'device-io'):
            for source in sorted((root / 'site-docs' / directory).glob('*.md')):
                matches = re.findall(r'<!-- simulator-examples: (\S+) (\S+) -->', source.read_text())
                if not matches:
                    continue
                assert len(matches) == 1
                alias, _ = matches[0]
                context = browser.new_context(java_script_enabled=False)
                page = context.new_page()
                response = page.goto(urljoin(base, f'{directory}/{source.stem}/'))
                assert response.ok, source
                block = page.locator(f'.example-table-block[data-scenario="{alias}"]')
                rows = block.locator('tbody tr').evaluate_all('''rows => rows.map(row => ({
                    label:row.dataset.checkpoint,time:Number(row.dataset.time),index:Number(row.dataset.timelineIndex),
                    pins:Object.fromEntries([...row.querySelectorAll('td[data-bits]')].filter(c=>c.dataset.bits)
                        .map(c=>[c.dataset.pin,c.dataset.bits]))}))''')
                assert rows, alias
                assert page.locator(f'iframe[src*="scenario={alias}"]').count() == 1, (alias, source)
                for width in (640, 360):
                    page.set_viewport_size({'width': width, 'height': 1000})
                    assert page.evaluate('document.documentElement.scrollWidth <= innerWidth+1'), source
                cases.append((alias, block.get_attribute('data-profile'), rows))
                context.close()
        assert len(cases) == len(roots) == 22
        assert {case[0] for case in cases} == set(roots)
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
                page.locator('#toggle-device-views').click()
                check_fidelity_layouts(page, alias, roots[alias])
                page.locator('#toggle-device-views').click()
                if screens and alias in ('muldiv-w32', 'rv32im-core', 'rv32im-device-system'):
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
                assert page.locator('#toggle-device-views').get_attribute('aria-pressed') == 'true'
                page.locator('#toggle-device-views').click()
                assert page.locator('#toggle-device-views').get_attribute('aria-pressed') == 'false'
                page.locator('#toggle-device-views').click()
                if device['kind'] == 'display':
                    assert device['pixels'].startswith('e01c03ff'), device
                if device['kind'] == 'keyboard':
                    assert device['levels'] == '0'*32 and int(device['count'],2) == 1, device
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
