"""Open the Pong short version: the whole machine runs the game in the page.

Checks the timeline checkpoints the checked runner reports, that the
scenario opens in Compact with Logic, Visual and Gate refused (memory), and that the
display and button queue are part of the machine.
"""
import shutil, sys, time
from playwright.sync_api import sync_playwright

def settled(page, timeout):
    page.wait_for_function("['ready','error'].includes(document.body.dataset.circuitsimStatus)", timeout=timeout)
    assert page.evaluate("document.body.dataset.circuitsimStatus") == 'ready', page.locator('#loading').inner_text()

with sync_playwright() as p:
    browser = p.chromium.launch(executable_path=shutil.which('google-chrome') or shutil.which('chromium'), headless=True,
        args=['--no-sandbox', '--disable-dev-shm-usage', '--renderer-process-limit=2', '--js-flags=--max-old-space-size=512'])
    page = browser.new_page(viewport={'width': 1280, 'height': 900}); errors = []
    page.on('pageerror', lambda e: errors.append(str(e)))
    base = sys.argv[1].rstrip('/') + '/'
    start = time.monotonic()
    page.goto(base + '?scenario=game-pong'); settled(page, 900000)
    seconds = time.monotonic() - start
    labels = page.evaluate('()=>app.checkpoints.map(c=>c.label)')
    for label in ('reset released', 'console: pong', 'press UPDATE', 'frame 1', 'frame 2', 'frame 3', 'console: done', 'halted'):
        assert label in labels, (label, labels)
    # The presses are queued right after reset, before the program first polls.
    order = [labels.index(l) for l in ('reset released', 'press UPDATE', 'frame 1', 'frame 3', 'console: done', 'halted')]
    assert order == sorted(order) and labels.count('press UPDATE') == 2, labels
    assert page.evaluate('app.profile.fidelityPreset') == 'compact'
    for preset in ('gate', 'logic', 'visual'):
        assert page.locator(f'#fidelity-select option[value={preset}]').evaluate('o=>o.disabled'), preset
    names = page.evaluate('()=>app.components.get(app.rootId).childIds.map(id=>app.components.get(id).name)')
    assert {'CORE', 'BUS', 'MEMORY', 'DISPLAY', 'BUTTONS', 'CONSOLE', 'TIMER'} <= set(names), names
    assert not errors, errors
    browser.close()
print(f'PASS Pong short version ran in the page in {seconds:.0f} s: {len(labels)} checkpoints, Compact only')
