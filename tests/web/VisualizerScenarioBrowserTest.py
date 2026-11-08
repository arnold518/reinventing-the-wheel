"""Grouped menu, separate CPU/prediction selection, old URLs and configuration."""
import shutil
import sys
from urllib.parse import urljoin
from playwright.sync_api import sync_playwright

base = sys.argv[1].rstrip('/') + '/'
with sync_playwright() as p:
    browser = p.chromium.launch(executable_path=shutil.which('google-chrome') or shutil.which('chromium'),
                                headless=True, args=['--no-sandbox'])
    page = browser.new_page(viewport={'width': 1600, 'height': 1000})
    page.set_default_timeout(90000)
    errors = []
    page.on('pageerror', lambda error: errors.append(str(error)))
    page.goto(urljoin(base, '?scenario=rv32i-predict-two-bit-program4&core_fidelity=behavioral'))
    page.wait_for_function("['ready', 'error'].includes(document.body.dataset.circuitsimStatus)")
    assert page.locator('body').get_attribute('data-circuitsim-status') == 'ready', (errors, page.locator('body').inner_text()[-2000:])
    assert page.locator('#scenario-select').input_value() == 'rv32i-program4'
    assert page.locator('#program-core').input_value() == 'advanced'
    assert page.locator('#program-policy').input_value() == 'two-bit'
    assert page.evaluate('app.scenario') == 'rv32i-predict-two-bit-program4'
    menu = page.evaluate("""()=>({
      actual: [...document.querySelectorAll('#scenario-select optgroup')].map(g=>({
        title:g.label, keys:[...g.querySelectorAll('option')].map(o=>o.value)})),
      expected: scenarioCatalogData.groups.map(g=>({title:g.title,
        keys:scenarioOptions.filter(o=>o.group===g.id && !o.hidden && (!o.workload || o.core==='single')).map(o=>o.key)
      })).filter(g=>g.keys.length)
    })""")
    # The published long-game recordings are one extra group, before the
    # developer circuits, with one entry per game (its recorded fidelities
    # are the Fidelity menu's choices); choosing one opens the recording.
    recorded = [g for g in menu['actual'] if g['title'] == 'Computer System / Games (long versions, recorded)']
    assert len(recorded) == 1 and len(recorded[0]['keys']) == 6, recorded
    assert all(key.startswith('recording:../assets/system/long-games/') for key in recorded[0]['keys']), recorded
    assert [g['title'] for g in menu['actual']][-2:] == [recorded[0]['title'], 'Developer circuits'], menu['actual']
    assert [g for g in menu['actual'] if g is not recorded[0]] == menu['expected'], menu
    titles = page.locator('#scenario-select option').all_text_contents()
    assert sum('Conditional branches and fallthrough' in text for text in titles) == 1
    assert not any('Test' in title for title in titles)
    assert page.locator('#scenario-select option[value*="simulator-advance"]').count() == 0
    assert page.locator('#scenario-select option[value*="five-stage-program"]').count() == 0
    # Switching core chooses the same program and preserves the URL fidelity.
    for core, expected in [('baseline', 'rv32i-baseline-program4'), ('single', 'rv32i-program4')]:
        page.locator('#program-core').select_option(core)
        page.wait_for_function('expected => app.scenario === expected && document.body.dataset.circuitsimStatus === "ready"', arg=expected)
        assert page.locator('#program-policy').is_disabled()
        assert page.evaluate('app.components.get("RV32I_PROGRAM_ROOT.CORE").fidelity') == 'behavioral'
    page.locator('#program-core').select_option('advanced')
    page.wait_for_function('app.scenario === "rv32i-five-stage-program4" && document.body.dataset.circuitsimStatus === "ready"')
    page.locator('#program-policy').select_option('backward-taken')
    page.wait_for_function('app.scenario === "rv32i-predict-backward-taken-program4" && document.body.dataset.circuitsimStatus === "ready"')
    for width in (1600, 800):
        page.set_viewport_size({'width': width, 'height': 1000})
        for selector in ('#scenario-select', '#program-core', '#program-policy', '#toggle-explorer'):
            assert page.locator(selector).evaluate('e => { const r=e.getBoundingClientRect(); return r.width > 0 && r.x >= 0 && r.right <= innerWidth; }'), selector
    page.locator('#scenario-select').select_option('alu-width8')
    page.wait_for_function('app.scenario === "alu-width8" && document.body.dataset.circuitsimStatus === "ready"')
    assert page.locator('#program-controls').is_hidden()
    assert not errors, errors
    browser.close()
print('Grouped catalog, legacy URL, core/prediction changes, fidelity and compact controls passed')
