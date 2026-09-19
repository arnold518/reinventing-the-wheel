"""Published SRAM reference, all checkpoint tables, WASM embeds and fidelity."""
from pathlib import Path
import re
import shutil
import sys
from urllib.parse import urljoin
from playwright.sync_api import sync_playwright


def main():
    base = sys.argv[1].rstrip("/") + "/"
    root = Path(__file__).resolve().parents[1]
    source = (root / "site-docs/memory/3.1.2.md").read_text()
    scenarios = re.findall(r"<!-- simulator-examples: (\S+) (\S+) -->", source)
    screenshots = Path(sys.argv[2]) if len(sys.argv) > 2 else None
    if screenshots:
        screenshots.mkdir(parents=True, exist_ok=True)
    with sync_playwright() as p:
        browser = p.chromium.launch(executable_path=shutil.which("google-chrome") or shutil.which("chromium"), headless=True, args=["--no-sandbox"])
        context = browser.new_context(viewport={"width": 1440, "height": 1000})
        page = context.new_page()
        page.set_default_timeout(30_000)
        errors, api = [], []
        page.on("pageerror", lambda error: errors.append(str(error)))
        page.on("request", lambda request: api.append(request.url) if "/api/" in request.url else None)
        response = page.goto(urljoin(base, "memory/3.1.2/"), wait_until="domcontentloaded")
        assert response and response.ok
        assert "§ 3.1.2. SRAM" in page.locator("h1").inner_text()
        assert page.locator(".simulator-embed iframe").count() == 12
        assert page.locator(".example-table tbody tr").count() == 115
        assert page.locator("article table").first.locator("thead th").all_text_contents() == ["Component", "Description", "Structural", "Behavioral"]
        for scenario, _ in scenarios:
            selector = f'.simulator-embed iframe[src$="?scenario={scenario}"]'
            page.locator(selector).scroll_into_view_if_needed()
            frame = page.frame_locator(selector)
            frame.locator("body[data-circuitsim-status='ready']").wait_for(timeout=90_000)
            assert frame.locator("body").get_attribute("data-circuitsim-runtime") == "wasm"
            assert frame.locator("#toggle-explorer").is_visible()
            block = page.locator(f'.example-table-block[data-scenario="{scenario}"]')
            rows = block.locator("tbody tr").evaluate_all("""rows => rows.map(row => ({
                label: row.dataset.checkpoint, time: Number(row.dataset.time), index: Number(row.dataset.timelineIndex),
                pins: Object.fromEntries([...row.querySelectorAll('td[data-bits]')]
                    .filter(c => c.dataset.bits).map(c => [c.dataset.pin, c.dataset.bits]))
            }))""")
            model = frame.locator("body").evaluate("""() => ({profile:app.components.get(app.rootId).profileFingerprint,
                checkpoints:app.checkpoints.map(c => ({label:c.label,time:c.time,index:c.index}))})""")
            assert block.get_attribute("data-profile") == model["profile"]
            assert [{k: row[k] for k in ("label", "time", "index")} for row in rows] == model["checkpoints"]
            for row in rows:
                actual = frame.locator("body").evaluate("""async (_, index) => {
                    await setStateIndex(index);
                    return Object.fromEntries([...app.pins.values()].filter(p => p.componentId === app.rootId)
                        .map(p => [p.name, app.state.pins[p.stateIndex]]));
                }""", row["index"])
                for pin, expected in row["pins"].items():
                    assert actual[pin] == expected, (scenario, row["label"], pin, actual[pin], expected)
            print(f"{scenario}: {len(rows)} browser checkpoints passed", flush=True)
        # Verify the public root fidelity toggle rebuilds the actual SRAM device.
        selector = '.simulator-embed iframe[src$="?scenario=sram"]'
        page.locator(selector).scroll_into_view_if_needed()
        frame = page.frame_locator(selector)
        frame.locator("body").evaluate("() => {app.explorer.setOpen(true,{persist:false}); smoothlyFocusComponent(app.rootId);}")
        frame.locator('.explorer-row.selected .explorer-arrow').click()
        frame.locator('#apply-profile').click()
        runtime = next(f for f in page.frames if f.url.endswith('?scenario=sram'))
        runtime.wait_for_function("app.components.size === 1 && app.components.get(app.rootId).fidelity === 'behavioral' && document.querySelector('#loading').classList.contains('hidden')")
        frame.locator('.explorer-row.selected .explorer-arrow').click()
        frame.locator('#apply-profile').click()
        runtime.wait_for_function("app.components.size > 100 && app.components.get(app.rootId).fidelity === 'structural' && document.querySelector('#loading').classList.contains('hidden')")
        for width in (1440, 640, 360):
            page.set_viewport_size({"width": width, "height": 1000})
            scroller = page.locator('.example-table-block[data-scenario="sram-storage-bank"] .example-table-scroll')
            scroller.scroll_into_view_if_needed()
            assert scroller.evaluate("e => {let r=e.getBoundingClientRect(); return r.left >= 0 && r.right <= innerWidth+1;}")
            assert page.evaluate("document.documentElement.scrollWidth <= innerWidth+1")
            if screenshots:
                page.screenshot(path=str(screenshots / f"sram-{width}.png"))
        if screenshots:
            page.set_viewport_size({"width":1440,"height":1000})
            page.locator(selector).scroll_into_view_if_needed()
            page.screenshot(path=str(screenshots / "sram-device.png"))
        assert not errors, errors
        assert not api, api
        static = browser.new_context(java_script_enabled=False)
        static_page = static.new_page()
        static_page.goto(urljoin(base, "memory/3.1.2/"), wait_until="domcontentloaded")
        assert static_page.locator(".example-table tbody tr").count() == 115
        browser.close()
    print("SRAM documentation: 12 embeds, 115 checkpoints, fidelity toggle, mobile and no-JS reference passed")


if __name__ == "__main__":
    main()
