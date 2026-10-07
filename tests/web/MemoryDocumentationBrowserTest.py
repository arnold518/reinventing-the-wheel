"""Browser validation of the standalone Chapter 3 FF storage scenarios."""

from pathlib import Path
import shutil
import sys
from urllib.parse import urljoin

from playwright.sync_api import sync_playwright


def main():
    base = sys.argv[1].rstrip("/") + "/"
    screenshots = Path(sys.argv[2]) if len(sys.argv) > 2 else None
    if screenshots:
        screenshots.mkdir(parents=True, exist_ok=True)
    scenarios = [("sr-latch", "SRLatch", 8), ("gated-d-latch", "GatedDLatch", 12),
                 ("dff", "DFlipFlop", 14), ("flip-flop-cell", "FlipFlopCell", 13),
                 ("ff-word", "FFWord<16>", 30), ("ff-storage-bank", "FFStorageBank<16,3>", 134),
                 ("ff-memory-container", "FFMemory8", 52)]
    components = [("SRLatch", False), ("GatedDLatch", False), ("DFlipFlop", False),
                  ("FlipFlopCell", True), ("FFWord<W>", True), ("FFStorageBank<W>", True),
                  ("FFMemory<W>", True)]
    with sync_playwright() as playwright:
        browser = playwright.chromium.launch(
            executable_path=shutil.which("google-chrome") or shutil.which("chromium"),
            headless=True, args=["--no-sandbox"])
        context = browser.new_context(viewport={"width": 1440, "height": 1000})
        page = context.new_page()
        page.set_default_timeout(30_000)
        errors, api_requests, reload_frames = [], [], []
        page.on("pageerror", lambda error: errors.append(str(error)))
        page.on("request", lambda request: api_requests.append(request.url) if "/api/" in request.url else None)
        page.on("request", lambda request: reload_frames.append(request.frame.parent_frame is not None)
                if "/livereload/" in request.url else None)
        response = page.goto(urljoin(base, "memory/3.1.1/"), wait_until="domcontentloaded")
        assert response and response.ok
        assert "§ 3.1.1. Flip-Flop Storage" in page.locator("h1").inner_text()
        assert "Register<W>" not in page.locator("article").inner_text()
        assert page.locator('iframe[src*="scenario=register32"]').count() == 0
        overview = page.locator("article table").first
        assert overview.locator("thead th").all_text_contents() == ["Component", "Description", "Structural", "Behavioral"]
        availability = overview.locator("tbody tr").evaluate_all("""rows => rows.map(row => ({
          component: row.cells[0].textContent.trim(),
          structural: row.cells[2].textContent.trim(), behavioral: row.cells[3].textContent.trim(),
        }))""")
        assert availability == [
            {"component": component, "structural": "Yes", "behavioral": "Yes" if behavioral else "No"}
            for component, behavioral in components]
        assert "simulator-examples:" not in page.locator("article").text_content()
        assert page.locator(".simulator-embed iframe").count() == 7
        headings = page.locator("article h3").evaluate_all(
            "nodes => nodes.map(n => n.firstChild.textContent.trim())")
        assert headings == ["IO pins", "Behavior", "Implementation", "Examples", "Source and tests"] * 7
        subsections = page.locator("article h4").evaluate_all(
            "nodes => nodes.map(n => n.firstChild.textContent.trim())")
        assert subsections == [heading for _, behavioral in components for heading in
                               (["C++ example", "Unknown inputs and timing", "Structural implementation"]
                                + (["Behavioral implementation"] if behavioral else []) + ["Embedded simulation"])]
        behavioral = page.locator("article h4").filter(has_text="Behavioral implementation")
        assert behavioral.count() == 4
        assert behavioral.evaluate_all("""nodes => nodes.map(h => {
          let section = h.previousElementSibling;
          while (section && section.tagName !== 'H3') section = section.previousElementSibling;
          let component = section;
          while (component && component.tagName !== 'H2') component = component.previousElementSibling;
          return section?.firstChild.textContent.trim() === 'Implementation'
            ? component?.querySelector('code')?.textContent : null;
        })""") == [component for component, exists in components if exists]
        examples = page.locator("article h4").filter(has_text="C++ example")
        assert examples.count() == 7
        for heading in examples.all():
            assert heading.evaluate("""h => {
              let section = h.previousElementSibling;
              while (section && section.tagName !== 'H3') section = section.previousElementSibling;
              return section && section.firstChild.textContent.trim() === 'Behavior';
            }""")
        assert page.locator('a[href*="rv32i/4.1/"]').count() > 0

        for scenario, component_type, count in scenarios:
            print(f"Checking {scenario}: {count} checkpoints", flush=True)
            selector = f'.simulator-embed iframe[src$="scenario={scenario}"]'
            page.locator(selector).scroll_into_view_if_needed()
            frame = page.frame_locator(selector)
            try:
                frame.locator("body[data-circuitsim-status='ready']").wait_for(timeout=60_000)
            except Exception:
                print({"scenario": scenario, "errors": errors,
                       "status": frame.locator("body").get_attribute("data-circuitsim-status"),
                       "loading": frame.locator("#loading").inner_text()}, flush=True)
                raise
            assert frame.locator("body").get_attribute("data-circuitsim-runtime") == "wasm"
            assert frame.locator("#scenario-select").input_value() == scenario
            assert frame.locator("#toggle-explorer").is_visible()
            assert frame.locator("#inspector").is_hidden()
            model = frame.locator("body").evaluate("""() => {
              const root = app.components.get(app.rootId);
              return {type: root.type, profile: root.profileFingerprint,
                fidelities: root.availableFidelities,
                checkpoints: app.checkpoints.map(c => ({checkpoint: c.label, time: c.time, index: c.index}))};
            }""")
            assert model["type"] == component_type
            assert set(model["fidelities"]) == ({"structural", "behavioral"}
                if scenario in {"flip-flop-cell", "ff-word", "ff-storage-bank", "ff-memory-container"} else {"structural"})
            block = page.locator(f'.example-table-block[data-scenario="{scenario}"]')
            assert block.get_attribute("data-profile") == model["profile"]
            rows = block.locator("tbody tr").evaluate_all("""nodes => nodes.map(row => ({
              checkpoint: row.dataset.checkpoint, time: Number(row.dataset.time),
              index: Number(row.dataset.timelineIndex),
              pins: Object.fromEntries([...row.querySelectorAll('td[data-bits]')]
                .filter(cell => cell.dataset.bits).map(cell => [cell.dataset.pin, cell.dataset.bits])),
            }))""")
            assert len(rows) == count
            assert [{k: row[k] for k in ("checkpoint", "time", "index")} for row in rows] == model["checkpoints"]
            for row in rows:
                actual = frame.locator("body").evaluate("""async (_, index) => {
                  await setStateIndex(index);
                  return Object.fromEntries([...app.pins.values()]
                    .filter(pin => pin.componentId === app.rootId)
                    .map(pin => [pin.name, app.state.pins[pin.stateIndex]]));
                }""", row["index"])
                assert actual == row["pins"], (scenario, row, actual)
                assert row["checkpoint"] in frame.locator("#time-label").inner_text()
            scroller = block.locator(".example-table-scroll")
            assert scroller.get_attribute("tabindex") == "0"
            for width in (1440, 640, 360):
                page.set_viewport_size({"width": width, "height": 1000})
                scroller.scroll_into_view_if_needed()
                assert scroller.evaluate("e => { const r = e.getBoundingClientRect(); return r.left >= 0 && r.right <= innerWidth + 1; }")
                assert page.evaluate("document.documentElement.scrollWidth <= innerWidth + 1"), (scenario, width)
            if screenshots and scenario in {"flip-flop-cell", "ff-word", "ff-storage-bank", "ff-memory-container"}:
                page.screenshot(path=str(screenshots / f"{scenario}-mobile.png"))
                page.set_viewport_size({"width": 1440, "height": 1000})
                page.locator(selector).scroll_into_view_if_needed()
                page.screenshot(path=str(screenshots / f"{scenario}-desktop.png"))
            page.set_viewport_size({"width": 1440, "height": 1000})

        # A bank now has its own root, table and whole-bank fidelity toggle.
        bank_selector = '.simulator-embed iframe[title^="FFStorageBank"]'
        page.locator(bank_selector).scroll_into_view_if_needed()
        bank_frame = page.frame_locator(bank_selector)
        bank_body = bank_frame.locator("body")
        bank_frame.locator("body[data-circuitsim-status='ready']").wait_for(timeout=60_000)
        assert bank_body.get_attribute("data-circuitsim-runtime") == "wasm"
        bank_body.evaluate("""() => {
          app.explorer.setOpen(true, {persist: false});
          smoothlyFocusComponent(app.rootId);
        }""")
        bank_frame.locator('.explorer-row.selected').wait_for()
        bank_model = bank_body.evaluate("""() => {
          const bank = app.components.get(app.rootId);
          return {root: app.components.get(app.rootId).type, type: bank.type,
            selected: app.selectedComponentId === bank.id,
            words: bank.childIds.map(id => app.components.get(id).type)};
        }""")
        assert bank_model == {"root": "FFStorageBank<16,3>", "type": "FFStorageBank<16,3>",
                              "selected": True, "words": ["FFWord<16>"] * 3}
        bank_frame.locator('.explorer-row.selected .explorer-arrow').click()
        bank_frame.locator('#apply-profile').click()
        runtime = next(frame for frame in page.frames if frame.url.endswith('?scenario=ff-storage-bank'))
        runtime.wait_for_function("app.components.size === 1 && app.components.get(app.rootId).fidelity === 'behavioral' && document.querySelector('#loading').classList.contains('hidden')")
        table = page.locator('.example-table-block[data-scenario="ff-storage-bank"]')
        for label in ("select-5-mask-1-write-edge", "select-7-mask-3-write-edge", "reset-again"):
            expected = table.locator(f'tr[data-checkpoint="{label}"] td[data-direction="expectedOutputs"]').evaluate_all(
                "cells => Object.fromEntries(cells.map(c => [c.dataset.pin, c.dataset.bits]))")
            actual = runtime.evaluate("""async label => {
              await setStateIndex(app.checkpoints.find(c => c.label === label).index);
              return Object.fromEntries([...app.pins.values()].filter(p => p.componentId === app.rootId && p.name.startsWith('WORD_'))
                .map(p => [p.name, app.state.pins[p.stateIndex]]));
            }""", label)
            assert actual == expected, (label, actual, expected)
        bank_frame.locator('.explorer-row.selected .explorer-arrow').click()
        bank_frame.locator('#apply-profile').click()
        runtime.wait_for_function("app.components.get(app.rootId).fidelity === 'structural' && app.components.get(app.rootId).childIds.length === 3 && document.querySelector('#loading').classList.contains('hidden')")

        assert not errors, errors
        assert not api_requests, api_requests
        # In the local preview only the top-level document should long-poll;
        # the published static site has no live-reload requests at all.
        assert not any(reload_frames), "An embedded MkDocs poller can starve later iframe asset requests"
        if page.evaluate("typeof livereload === 'function'"):
            assert reload_frames, "The top-level preview must still poll for documentation changes"
        # The reference and all its rows remain readable without JavaScript.
        static = browser.new_context(java_script_enabled=False)
        static_page = static.new_page()
        static_page.goto(urljoin(base, "memory/3.1.1/"), wait_until="domcontentloaded")
        assert static_page.locator(".example-table tbody tr").count() == 263
        static.close()
        browser.close()
    print("Storage documentation: seven standalone components, 263 WASM checkpoints, bank fidelity, responsive and static views passed")


if __name__ == "__main__":
    main()
