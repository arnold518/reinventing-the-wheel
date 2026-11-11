"""A long game whose machine saves this simulator refuses opens its frames.

When the machine changes after a game was recorded, its saves no longer fit
("Saved machine state does not fit"). The page then reloads with saves=off and
shows the recorded frames, saying why, instead of stopping on an error. The
check serves a save that does not fit (valid gzip, wrong content) for every
save request. CIRCUITSIM_GAME_RECORDING names the recording (default: the
published Compact Pong).
"""
import gzip, os, shutil, sys
from urllib.parse import urljoin
from playwright.sync_api import sync_playwright

base = sys.argv[1].rstrip('/') + '/'
recording = os.environ.get('CIRCUITSIM_GAME_RECORDING', '../assets/system/long-games/pong-long/playlist.json')
broken = gzip.compress(b'not a machine save' * 64, mtime=0)

with sync_playwright() as p:
    browser = p.chromium.launch(executable_path=shutil.which('google-chrome') or shutil.which('chromium'), headless=True,
        args=['--no-sandbox', '--disable-dev-shm-usage', '--renderer-process-limit=2'])
    page = browser.new_page(viewport={'width': 1400, 'height': 900}); errors = []
    page.on('pageerror', lambda e: errors.append(str(e)))
    page.route('**/saves/save-*.bin.gz', lambda route: route.fulfill(status=200, body=broken,
                                                                     headers={'Content-Type': 'application/gzip'}))
    page.goto(urljoin(base, f'?recording={recording}&frame=3'))
    page.wait_for_function("new URLSearchParams(location.search).get('saves') === 'off'", timeout=900000)
    page.wait_for_function("['ready','error'].includes(document.body.dataset.circuitsimStatus)", timeout=900000)
    assert page.evaluate("document.body.dataset.circuitsimStatus") == 'ready', page.locator('#loading').inner_text()
    assert not page.evaluate("document.body.classList.contains('game-session')"), 'Still a game session'
    status = page.locator('#recording-status').inner_text()
    assert 'frames only' in status, status
    assert page.evaluate('app.timestampCount') > 3, 'The recorded frames are not shown'
    assert not errors, errors
    browser.close()
print(f'PASS refused saves: the recording opened its frames instead ({status})')
