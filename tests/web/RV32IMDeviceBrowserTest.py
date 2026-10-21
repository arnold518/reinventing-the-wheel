"""New component pages and recorded WASM examples, one session at a time."""
from pathlib import Path
import os
import re
import shutil
import sys
from urllib.parse import urljoin

from playwright.sync_api import sync_playwright


def main():
    base = sys.argv[1].rstrip('/') + '/'
    root = Path(__file__).resolve().parents[2]
    screens = Path(os.environ['CIRCUITSIM_BROWSER_SCREENSHOTS']) if os.environ.get('CIRCUITSIM_BROWSER_SCREENSHOTS') else None
    if screens:
        screens.mkdir(parents=True, exist_ok=True)
    cases = []
    with sync_playwright() as playwright:
        browser = playwright.chromium.launch(
            executable_path=shutil.which('google-chrome') or shutil.which('chromium'),
            headless=True, args=['--no-sandbox'])
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
        assert len(cases) == 22
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
        browser.close()
    print(f'{len(cases)} new component pages and {len(selected)} recorded sessions passed')


if __name__ == '__main__':
    main()
