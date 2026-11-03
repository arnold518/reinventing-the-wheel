"""The packaged WebAssembly visualizer builds a scenario under a chosen timing profile.

Opens a scenario with ?timing=embedded, checks the selector, the profile the
worker reports and the time units on the timeline; then switches to unit
through the selector and checks the reload, the URL and the tick label.
"""

from __future__ import annotations

import shutil
import sys

from playwright.sync_api import sync_playwright


def ready(page) -> None:
    page.wait_for_function("document.body.dataset.circuitsimStatus === 'ready'", timeout=60_000)


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit("Usage: VisualizerTimingProfileBrowserTest.py <simulator-url>")
    chrome = shutil.which("google-chrome") or shutil.which("google-chrome-stable") or shutil.which("chromium")
    if chrome is None:
        raise RuntimeError("Chrome or Chromium is required for the browser test")
    with sync_playwright() as playwright:
        browser = playwright.chromium.launch(executable_path=chrome, headless=True,
                                             args=["--no-sandbox", "--disable-dev-shm-usage", "--js-flags=--max-old-space-size=256"])
        page = browser.new_context(viewport={"width": 1440, "height": 900}).new_page()
        errors: list[str] = []
        page.on("pageerror", lambda error: errors.append(str(error)))

        page.goto(f"{sys.argv[1]}?scenario=ripple-carry-adder-w8&timing=embedded", wait_until="domcontentloaded", timeout=30_000)
        ready(page)
        embedded = page.evaluate("""() => ({
          selected: document.querySelector('#timing-select').value,
          timing: app.timing,
          last: app.timestamps[app.timestamps.length - 1],
        })""")
        assert embedded["selected"] == "embedded", embedded
        assert embedded["timing"]["name"] == "embedded" and embedded["timing"]["tickPicoseconds"] == 1, embedded
        # Move to the last timestamp: the label is in ps or ns, not ticks.
        page.evaluate("setStateIndex(app.timestamps.length - 1)")
        page.wait_for_function("document.querySelector('#time-label').textContent.includes(' ns') || document.querySelector('#time-label').textContent.includes(' ps')")
        label = page.locator("#time-label").text_content()
        assert "ticks" not in label, label

        page.select_option("#timing-select", "unit")
        page.wait_for_function("app.timing && app.timing.name === 'unit'", timeout=60_000)
        ready(page)
        unit = page.evaluate("""() => ({url: location.search, last: app.timestamps[app.timestamps.length - 1]})""")
        assert "timing=" not in unit["url"], unit
        assert unit["last"] * 10 < embedded["last"], (unit, embedded)
        page.evaluate("setStateIndex(app.timestamps.length - 1)")
        page.wait_for_function("document.querySelector('#time-label').textContent.includes('ticks')")
        # Fidelity is an independent build choice, with separate persistent
        # explorer edits for each base preset.
        page.goto(f"{sys.argv[1]}?scenario=register32&fidelity=visual", wait_until='domcontentloaded')
        ready(page)
        for preset, fidelity in [('visual', 'behavioral'), ('logic', 'behavioral'),
                                 ('compact', 'behavioral'), ('gate', 'structural')]:
            page.select_option('#fidelity-select', preset)
            page.wait_for_function("p=>app.profile.fidelityPreset===p && document.body.dataset.circuitsimStatus==='ready'", arg=preset)
            assert page.evaluate('app.components.get(app.rootId).fidelity') == fidelity, preset
            # The scenario's own profile is the default and is left out of the URL.
            if preset == page.evaluate("scenarioFidelity('register32')"):
                assert 'fidelity=' not in page.url, page.url
            else:
                assert f'fidelity={preset}' in page.url, page.url
            assert page.locator('#timing-select').input_value() == 'unit'
        page.locator('.explorer-arrow').first.click()
        page.locator('#apply-profile').click()
        page.wait_for_function("app.profile.custom && app.components.get(app.rootId).fidelity==='behavioral' && document.querySelector('#loading').classList.contains('hidden')")
        assert '+ overrides' in page.locator('#fidelity-select option:checked').inner_text()
        page.select_option('#fidelity-select', 'visual')
        page.wait_for_function("app.profile.fidelityPreset==='visual' && !app.profile.custom && document.body.dataset.circuitsimStatus==='ready'")
        page.select_option('#fidelity-select', 'gate')
        page.wait_for_function("app.profile.fidelityPreset==='gate' && app.profile.custom && document.body.dataset.circuitsimStatus==='ready'")
        assert page.evaluate('app.components.get(app.rootId).fidelity') == 'behavioral'
        page.select_option('#timing-select', 'embedded')
        page.wait_for_function("app.timing.name==='embedded' && document.body.dataset.circuitsimStatus==='ready'")
        assert page.locator('#fidelity-select').input_value() == 'gate'
        for width in (1440, 800, 400):
            page.set_viewport_size({'width': width, 'height': 900})
            for selector in ('#scenario-select', '#timing-select', '#fidelity-select'):
                assert page.locator(selector).evaluate('e=>{const r=e.getBoundingClientRect();return r.width>0 && r.x>=0 && r.right<=innerWidth;}'), (width, selector)
        # An unsupported strict gate build must remain recoverable through
        # the selector, including when it is the initial URL.
        page.goto(f"{sys.argv[1]}?scenario=scalable-memory-container&fidelity=gate", wait_until='domcontentloaded')
        page.wait_for_function("document.body.dataset.circuitsimStatus==='error'")
        page.select_option('#fidelity-select', 'visual')
        ready(page)
        assert page.evaluate('app.profile.fidelityPreset') == 'visual'
        assert not errors, errors
        browser.close()
    print("PASS: timing/fidelity selection, actual implementations, custom isolation, URLs and responsive controls")


if __name__ == "__main__":
    main()
