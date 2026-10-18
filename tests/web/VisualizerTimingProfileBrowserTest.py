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
                                             args=["--no-sandbox", "--disable-dev-shm-usage"])
        page = browser.new_context(viewport={"width": 1440, "height": 900}).new_page()
        errors: list[str] = []
        page.on("pageerror", lambda error: errors.append(str(error)))

        page.goto(f"{sys.argv[1]}?scenario=adder8&timing=embedded", wait_until="domcontentloaded", timeout=30_000)
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
        assert not errors, errors
        browser.close()
    print("PASS: timing profile selection, reported profile, time units and reload")


if __name__ == "__main__":
    main()
