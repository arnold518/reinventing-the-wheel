"""Check the new one-bit adder references and their WASM embeds, serially.

Usage: python tests/AdderDocumentationBrowserTest.py [preview-url] [screenshots]
Other simulator embeds are blocked while checking the article's layout.
"""

from pathlib import Path
import os
import json
import re
import shutil
import sys
from urllib.parse import urljoin

from playwright.sync_api import sync_playwright

ROOT = Path(__file__).resolve().parents[1]
BASE = (sys.argv[1] if len(sys.argv) > 1 else "http://127.0.0.1:8893/").rstrip("/") + "/"
SCREENSHOTS = Path(sys.argv[2]) if len(sys.argv) > 2 else None
if SCREENSHOTS:
    SCREENSHOTS.mkdir(parents=True, exist_ok=True)
EXPORTS = json.loads(Path(os.environ.get('CIRCUITSIM_EXAMPLE_ROWS', ROOT / "build-wasm/pages/simulator/example-rows.json")).read_text())["scenarios"]
SCENARIOS = {"half-adder": ("HalfAdder", 16), "full-adder": ("FullAdder", 64)}


with sync_playwright() as p:
    browser = p.chromium.launch(
        executable_path=shutil.which("google-chrome") or shutil.which("chromium"),
        headless=True, args=["--no-sandbox"],
    )
    context = browser.new_context(viewport={"width": 1440, "height": 1000})
    context.route("**/simulator/**", lambda route: route.abort())
    page = context.new_page()
    page.set_default_timeout(30000)
    assert page.goto(urljoin(BASE, "alu8/2.1/"), wait_until="domcontentloaded").ok
    page.wait_for_function("document.querySelectorAll('mjx-container').length > 0")
    page.evaluate("MathJax.startup.promise")
    headings = page.locator("article h2").all_text_contents()
    for index, name in enumerate(("HalfAdder", "FullAdder", "Adder8", "AddSub8", "ArithmeticUnit8"), 1):
        assert any(f"2.1.{index}. {name}" in heading for heading in headings), (name, headings)
    for anchor in ("211-adder8", "212-addsub8", "213-arithmeticunit8"):
        assert page.locator(f'[id="{anchor}"]').count() == 1, anchor
    assert page.locator(".simulator-embed iframe").count() == 5
    source = (ROOT / "site-docs/alu8/2.1.md").read_text()
    expected_code = re.findall(r"```cpp\n(.*?)\n```", source, re.S)
    rendered_code = page.locator("article .language-cpp").all_text_contents()
    assert [code.strip() for code in rendered_code] == [code.strip() for code in expected_code]

    urls = {}
    for alias, (component, count) in SCENARIOS.items():
        rows = page.locator(f'.example-table-block[data-scenario="{alias}"] tbody tr').evaluate_all("""rows => rows.map(row => ({
            checkpoint: row.dataset.checkpoint, time: Number(row.dataset.time),
            timelineIndex: Number(row.dataset.timelineIndex),
            pins: Object.fromEntries([...row.querySelectorAll('[data-pin]')].map(cell => [cell.dataset.pin, cell.dataset.bits])),
        }))""")
        expected = EXPORTS[alias]["rows"]
        assert len(rows) == len(expected) == count
        for actual, reference in zip(rows, expected, strict=True):
            assert actual == {key: reference[key] for key in ("checkpoint", "time", "timelineIndex")} | {
                "pins": reference["inputs"] | reference["expectedOutputs"]}
        embed = page.locator(f'.simulator-embed iframe[src*="scenario={alias}&"]')
        urls[alias] = embed.evaluate("el => el.src")
        assert urls[alias] == urljoin(BASE, f"simulator/?scenario={alias}&embed=1")

    for scheme in ("default", "slate"):
        page.locator(f'input[data-md-color-scheme="{scheme}"]').evaluate("el => el.click()")
        for width in (1440, 640, 360):
            page.set_viewport_size({"width": width, "height": 1000})
            assert page.evaluate("document.documentElement.scrollWidth <= innerWidth + 1"), (scheme, width)
            assert page.locator(".example-table-scroll").evaluate_all("nodes => nodes.every(n => n.tabIndex === 0 && n.clientWidth <= innerWidth)")
            assert page.locator("mjx-merror").count() == 0
            if SCREENSHOTS and width in (1440, 360):
                page.locator('[id="211-halfadder"]').scroll_into_view_if_needed()
                page.screenshot(path=str(SCREENSHOTS / f"half-adder-{scheme}-{width}.png"))
    context.close()
    print("PASS: section order, preserved anchors, five C++ snippets, 80 exact table rows, theme and mobile layout", flush=True)

    # One WASM runtime at a time; use the exact URL published in each iframe.
    for alias, (component, count) in SCENARIOS.items():
        context = browser.new_context(viewport={"width": 1000, "height": 740})
        page = context.new_page()
        page.set_default_timeout(30000)
        errors, api = [], []
        page.on("pageerror", lambda error: errors.append(str(error)))
        page.on("request", lambda request: api.append(request.url) if "/api/" in request.url else None)
        assert page.goto(urls[alias], wait_until="domcontentloaded").ok
        page.wait_for_function("['ready', 'error'].includes(document.body.dataset.circuitsimStatus)")
        assert page.locator("body").get_attribute("data-circuitsim-status") == "ready", (alias, errors)
        assert page.locator("body").get_attribute("data-circuitsim-runtime") == "wasm"
        assert page.evaluate("app.components.get(app.rootId).type") == component
        assert page.evaluate("app.checkpoints.map(c => ({checkpoint: c.label, time: c.time, timelineIndex: c.index}))") == [
            {key: row[key] for key in ("checkpoint", "time", "timelineIndex")} for row in EXPORTS[alias]["rows"]]
        failures = page.evaluate("""async rows => {
            const root = app.components.get(app.rootId), failures = [];
            const pins = new Map([...root.inputPins, ...root.outputPins].map(pin => [pin.name, pin]));
            for (const row of rows) {
                await setStateIndex(row.timelineIndex);
                for (const [name, expected] of Object.entries({...row.inputs, ...row.expectedOutputs}))
                    if (pinValue(pins.get(name)) !== expected) failures.push([row.checkpoint, name, expected, pinValue(pins.get(name))]);
            }
            return failures;
        }""", EXPORTS[alias]["rows"])
        assert not failures, (alias, failures)
        assert not errors and not api, (alias, errors, api)
        if SCREENSHOTS:
            page.screenshot(path=str(SCREENSHOTS / f"{alias}-embed.png"))
        context.close()
        print(f"PASS: {alias} WASM embed, all {count} checkpoints, no backend API", flush=True)
    browser.close()
