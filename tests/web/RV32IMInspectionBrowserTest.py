"""Bounded browser acceptance for grouped arithmetic and recorded M programs."""
from pathlib import Path
import os
import re
import shutil
import sys
from playwright.sync_api import sync_playwright


def main():
    base = sys.argv[1].rstrip('/')
    screens = Path(os.environ['CIRCUITSIM_BROWSER_SCREENSHOTS']) if os.environ.get('CIRCUITSIM_BROWSER_SCREENSHOTS') else None
    if screens:
        screens.mkdir(parents=True, exist_ok=True)
    cases = [
        ('multiplier-w32', 'MULTIPLIER_W32_ROOT', {'CONTROL', 'OPERANDS', 'DATAPATH', 'RESULT'}),
        ('divider-w32', 'DIVIDER_W32_ROOT', {'CONTROL', 'OPERANDS', 'DATAPATH', 'RESULT'}),
        ('rv32im-execute-stage', 'RV32IM_EXECUTE_STAGE_ROOT', {'INTEGER', 'M_OPERANDS', 'M_CONTROL', 'MULDIV', 'RESULT_SELECTION'}),
        ('rv32im-program-dependencies', 'RV32IM_PROGRAM_ROOT', {'CLOCK', 'CORE', 'INSTRUCTION_MEMORY', 'DATA_MEMORY'}),
    ]
    pattern = os.environ.get('CIRCUITSIM_INSPECTION_FILTER')
    if pattern:
        cases = [case for case in cases if re.search(pattern, case[0])]
        assert cases, 'No inspection scenarios selected'
    with sync_playwright() as playwright:
        browser = playwright.chromium.launch(executable_path=shutil.which('google-chrome') or shutil.which('chromium'),
            headless=True, args=['--no-sandbox', '--disable-dev-shm-usage', '--renderer-process-limit=2',
                                 '--js-flags=--max-old-space-size=512'])
        for alias, expected_root, expected_children in cases:
            context = browser.new_context(viewport={'width': 1440, 'height': 1000})
            page = context.new_page()
            errors = []
            page.on('pageerror', lambda error: errors.append(str(error)))
            page.goto(f'{base}/simulator/?scenario={alias}', wait_until='domcontentloaded')
            page.locator("body[data-circuitsim-status='ready']").wait_for(timeout=120000)
            assert page.evaluate('app.rootId') == expected_root
            scopes = [expected_root]
            if alias in ('multiplier-w32', 'divider-w32'):
                scopes += [expected_root + '.' + child for child in expected_children]
                scopes += [expected_root + '.DATAPATH.STEP']
            elif alias == 'rv32im-execute-stage':
                scopes += [expected_root + '.M_CONTROL', expected_root + '.MULDIV']
            else:
                assert page.evaluate("app.components.get(app.rootId+'.CORE').fidelity") == 'structural'
                scopes += [expected_root + '.CORE', expected_root + '.CORE.EXECUTE']
                menu = page.locator('#scenario-select').inner_text()
                for title in ('All eight M instructions', 'M dependencies', 'Division edges'):
                    assert title in menu, title
                checkpoints = page.evaluate('app.checkpoints')
                assert checkpoints[0]['label'] == 'reset' and checkpoints[-1]['label'] == 'halted'
                assert any(c['label'] == 'arithmetic-wait' for c in checkpoints)
                assert len([c for c in checkpoints if c['label'].startswith('commit-')]) >= 8
                for checkpoint in (checkpoints[-1], checkpoints[0], checkpoints[-1]):
                    pins = page.evaluate('''async c=>{await setStateIndex(c.index);return Object.fromEntries(
                        [...app.pins.values()].filter(p=>p.componentId===app.rootId).map(p=>[p.name,app.state.pins[p.stateIndex]]));}''', checkpoint)
                    assert pins['HALTED'] == ('1' if checkpoint['label'] == 'halted' else '0')
                    assert pins['TRAPPED'] == '0'
            actual_children = page.evaluate("()=>app.components.get(app.rootId).childIds.map(id=>app.components.get(id).name)")
            assert expected_children <= set(actual_children), (alias, actual_children)
            if alias in ('multiplier-w32', 'divider-w32'):
                assert set(actual_children) == expected_children
            for scope in scopes:
                page.evaluate('id=>smoothlyFocusComponent(id)', scope)
                page.wait_for_function('!app.geometryDirty && !app.cameraAnimation && !app.topologyRequestInFlight && !app.pendingScopeIds.size')
                problems = page.evaluate('''id=>{const p=app.components.get(id),r=p.rect,issues=[];
                    const children=p.childIds.map(id=>app.components.get(id));
                    for(const c of children){const b=c.rect;
                        if(!b||b.x<r.x-.01||b.y<r.y-.01||b.x+b.w>r.x+r.w+.01||b.y+b.h>r.y+r.h+.01)issues.push(c.id);}
                    for(let i=0;i<children.length;++i)for(let j=i+1;j<children.length;++j){const a=children[i].rect,b=children[j].rect;
                        if(a&&b&&Math.min(a.x+a.w,b.x+b.w)-Math.max(a.x,b.x)>.01&&Math.min(a.y+a.h,b.y+b.h)-Math.max(a.y,b.y)>.01)issues.push('overlap');}
                    return issues.slice(0,5);}''', scope)
                assert not problems, (alias, scope, problems)
                assert not page.locator('#explorer-status').evaluate("e=>e.classList.contains('error')")
            page.evaluate('id=>smoothlyFocusComponent(id)', expected_root)
            page.wait_for_function('!app.geometryDirty && !app.cameraAnimation')
            if screens:
                page.screenshot(path=str(screens / (alias + '.png')), full_page=True)
            assert not errors, errors
            context.close()
            print(f'{alias}: grouped scopes and recorded inspection passed', flush=True)
        browser.close()


if __name__ == '__main__':
    main()
