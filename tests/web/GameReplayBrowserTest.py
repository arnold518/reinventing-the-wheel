"""A long game recorded with machine saves, in the ordinary timeline.

The recording opens in the simulator: the machine is restored from the shown
frame's save. Checks: the frame bar spans the game; ▶ steps event by event and
simulates more cycles when it reaches the end of what has run; the core's
register file and main memory are inspectable; ▶| jumps to the next frame;
the Fidelity menu switches to the Compact recording at the same frame and
back. CIRCUITSIM_GAME_RECORDING names another recording (default: the
published Visual Pong).
"""
import os, shutil, sys, time
from urllib.parse import urljoin
from playwright.sync_api import sync_playwright

base = sys.argv[1].rstrip('/') + '/'
recording = os.environ.get('CIRCUITSIM_GAME_RECORDING', '../assets/system/long-games/visual/pong-long/playlist.json')
frame = int(os.environ.get('CIRCUITSIM_GAME_FRAME', '3'))
preset = os.environ.get('CIRCUITSIM_GAME_PRESET', 'visual')   # the recording's fidelity


def settled(page, timeout):
    page.wait_for_function("['ready','error'].includes(document.body.dataset.circuitsimStatus)", timeout=timeout)
    assert page.evaluate("document.body.dataset.circuitsimStatus") == 'ready', page.locator('#loading').inner_text()


def state(page, component, extra=''):
    return page.evaluate("""async ([id, extra]) => {
        const r = await visualizerRequest(apiUrl(`api/component-state?session=${app.sessionId}&index=${app.currentIndex}&componentId=${id}${extra}`));
        return r.json(); }""", [component, extra])


with sync_playwright() as p:
    browser = p.chromium.launch(executable_path=shutil.which('google-chrome') or shutil.which('chromium'), headless=True,
        args=['--no-sandbox', '--disable-dev-shm-usage', '--renderer-process-limit=2'])
    page = browser.new_page(viewport={'width': 1400, 'height': 900}); errors = []
    page.on('pageerror', lambda e: errors.append(str(e)))
    start = time.monotonic()
    page.goto(urljoin(base, f'?recording={recording}&frame={frame}'))
    settled(page, 900000)
    seconds = time.monotonic() - start
    assert page.evaluate("document.body.classList.contains('game-session')"), 'The recording did not open as a game session'
    frames = int(page.locator('#game-frames').get_attribute('max')) + 1
    assert page.locator('#game-frames').is_visible() and page.locator('#game-frames').input_value() == str(frame), frames
    assert page.evaluate('app.profile.fidelityPreset') == preset
    assert page.locator('#replay-events').count() == 0, 'No separate replay button'
    # ▶ goes event by event, simulating when it runs out of history.
    first = page.evaluate('app.timestampCount')
    assert first > 100, f'The frame opened without its first cycles simulated ({first} timestamps)'
    # Step from just before the end of the simulated stretch, through it.
    page.evaluate('index => setStateIndex(index)', first - 20)
    times = []
    advance = time.monotonic()
    for _ in range(60):
        page.click('#step-forward')
        page.wait_for_function('() => !app.stateRequestInFlight && !game.busy', timeout=120000)
        page.wait_for_timeout(50)
        times.append(int(page.evaluate('String(app.state.time)')))
    stepping = time.monotonic() - advance
    steps = [b - a for a, b in zip(times, times[1:])]
    assert all(0 < d < 4500 for d in steps), steps
    assert page.evaluate('app.timestampCount') > first, 'Stepping did not simulate further'
    if preset == 'visual':   # Compact's core is one block, without a register view
        registers = state(page, 'MACHINE.CORE.DECODE.REGISTER_FILE')
        assert registers.get('supported') and len(registers['registers']) == 32, registers
    memory = state(page, 'MACHINE.MEMORY.RAM', '&address=0&count=16')
    words = memory.get('windowWords', [])
    assert memory.get('supported') and len(words) == 16 and set(words[0]['bits']) <= {'0', '1'}, memory
    # ▶| jumps to the next frame; the frame bar follows.
    page.click('#checkpoint-forward')
    page.wait_for_function(f"document.querySelector('#game-frames').value === '{frame + 1}' && !app.stateRequestInFlight && !game.busy", timeout=120000)
    assert 'frame=%d' % (frame + 1) in page.url
    # The Fidelity menu: the game's other recording at the same frame, and back
    # (when the menu lists this game in another fidelity). A recording with
    # saves opens as a game session (frame bar); one without shows frames.
    other = 'compact' if preset == 'visual' else 'visual'
    listed = page.evaluate("Boolean(recordingEntry(new URLSearchParams(location.search).get('recording')))")
    switching = listed and not page.locator(f'#fidelity-select option[value={other}]').evaluate('o => o.disabled')
    def frame_shown():
        if page.evaluate("document.body.classList.contains('game-session')"):
            return int(page.locator('#game-frames').input_value())
        return page.evaluate('app.currentIndex')
    if switching:
        for target in (other, preset):
            page.select_option('#fidelity-select', target)
            page.wait_for_function(f"app.profile && app.profile.fidelityPreset === '{target}' && document.body.dataset.circuitsimStatus === 'ready'", timeout=900000)
            assert frame_shown() == frame + 1, (target, frame_shown())
    assert not errors, errors
    browser.close()
print(f'PASS game session ({preset}): opened in {seconds:.0f} s at frame {frame + 1} of {frames}; 60 event steps in {stepping:.0f} s, '
      f'memory readable; next frame' + ('; switched to the other fidelity and back at the same frame' if switching else ''))
