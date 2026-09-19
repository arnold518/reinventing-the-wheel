"""New documentation only; load one simulator per context to bound RAM usage."""
from pathlib import Path
import re
import shutil
import sys
from urllib.parse import urljoin
from playwright.sync_api import sync_playwright


def verify_matrix(page, bank_id):
    page.evaluate("id => smoothlyFocusComponent(id)", bank_id)
    page.wait_for_function("!app.cameraAnimation && !app.geometryDirty && !app.topologyRequestInFlight && !app.pendingScopeIds.size")
    matrix = page.evaluate("""id => {
        const bank=app.components.get(id), children=bank.childIds.map(id=>app.components.get(id));
        const rows=children.filter(c=>/^ROW_/.test(c.name)).sort((a,b)=>Number(a.name.slice(4))-Number(b.name.slice(4)));
        const columns=children.filter(c=>/^COLUMN_/.test(c.name)).sort((a,b)=>Number(a.name.slice(7))-Number(b.name.slice(7)));
        const cells=rows.map(r=>r.childIds.map(id=>app.components.get(id))
            .sort((a,b)=>Number(a.name.slice(5))-Number(b.name.slice(5))));
        return {bank:bank.rect,rows:rows.map(c=>c.rect),columns:columns.map(c=>c.rect),cells:cells.map(cs=>cs.map(c=>c.rect))};
    }""", bank_id)
    assert len(matrix["rows"]) == 2 and len(matrix["columns"]) == 16
    assert matrix["rows"][0]["y"] < matrix["rows"][1]["y"]
    bank = matrix["bank"]
    for child in matrix["rows"] + matrix["columns"]:
        assert child["x"] >= bank["x"] and child["y"] >= bank["y"]
        assert child["x"] + child["w"] <= bank["x"] + bank["w"] + 1e-6
        assert child["y"] + child["h"] <= bank["y"] + bank["h"] + 1e-6
    for cells in matrix["cells"]:
        assert len(cells) == 16
        assert max(c["y"] for c in cells) - min(c["y"] for c in cells) < 1e-6
        assert all(a["x"] + a["w"] < b["x"] for a, b in zip(cells, cells[1:]))
        for cell, column in zip(cells, matrix["columns"]):
            assert abs(cell["x"] + cell["w"] / 2 - column["x"] - column["w"] / 2) < 1e-6
    for row in matrix["rows"]:
        assert row["y"] + row["h"] < matrix["columns"][0]["y"]


def verify_peripheral(page, component_id):
    page.evaluate("id => smoothlyFocusComponent(id)", component_id)
    page.wait_for_function("!app.cameraAnimation && !app.geometryDirty && !app.topologyRequestInFlight && !app.pendingScopeIds.size")
    data = page.evaluate("""id => {
        const parent=app.components.get(id);
        return {parent:parent.rect,
            children:parent.childIds.map(id=>app.components.get(id)).map(c=>({name:c.name,rect:c.rect}))};
    }""", component_id)
    rectangles = {c["name"]: c["rect"] for c in data["children"]}
    mux = "SELECT_0_0" in rectangles
    rows = [[rectangles[f"SELECT_{slot}_{bit}" if mux else f"WRITE_{slot * 8 + bit}"]
             for bit in range(8)] for slot in range(2)]
    for row in rows:
        assert all(a["x"] + a["w"] < b["x"] for a,b in zip(row,row[1:]))
        assert max(r["y"] for r in row) - min(r["y"] for r in row) < 1e-6
    for top, bottom in zip(*rows):
        assert abs(top["x"] - bottom["x"]) < 1e-6
        assert top["y"] + top["h"] < bottom["y"]
    parent = data["parent"]
    for rect in rectangles.values():
        assert rect["x"] >= parent["x"] and rect["y"] >= parent["y"]
        assert rect["x"] + rect["w"] <= parent["x"] + parent["w"] + 1e-6
        assert rect["y"] + rect["h"] <= parent["y"] + parent["h"] + 1e-6


def main():
    base = sys.argv[1].rstrip("/") + "/"
    root = Path(__file__).resolve().parents[1]
    screenshots = Path(sys.argv[2]) if len(sys.argv) > 2 else None
    if screenshots:
        screenshots.mkdir(parents=True, exist_ok=True)
    with sync_playwright() as p:
        browser = p.chromium.launch(executable_path=shutil.which("google-chrome") or shutil.which("chromium"),
                                    headless=True, args=["--no-sandbox"])
        cases = []
        # Check static pages/tables first with scripts disabled: no background WASM instances.
        for section in ("3.1", "3.1.0", "3.1.3", "3.1.4", "3.1.5"):
            source = (root / "site-docs/memory" / ("3.1/index.md" if section == "3.1" else f"{section}.md")).read_text()
            scenarios = re.findall(r"<!-- simulator-examples: (\S+) (\S+) -->", source)
            context = browser.new_context(java_script_enabled=False, viewport={"width": 1440, "height": 1000})
            page = context.new_page()
            response = page.goto(urljoin(base, f"memory/{section}/"), wait_until="domcontentloaded")
            assert response.ok
            if section == "3.1":
                nav = page.locator('.md-nav--primary a.md-nav__link').filter(has_text="3.1. Memory Devices")
                assert nav.count() == 1 and nav.get_attribute("href").rstrip("/") in (".", "../3.1"), nav.all_text_contents()
                assert not page.locator('.md-nav--primary a.md-nav__link').filter(has_text=re.compile(r'^\s*Architecture\s*$')).count()
                assert not scenarios and not page.locator(".simulator-embed").count()
                helpers = page.locator('article a').filter(has_text="3.1.0. Helper Components")
                assert helpers.count() == 1 and helpers.get_attribute("href").rstrip("/") == "../3.1.0"
            assert f"§ {section}." in page.locator("h1").inner_text()
            assert page.locator(".simulator-embed iframe").count() == len(scenarios)
            if section != "3.1":
                assert page.locator("article table").first.locator("thead th").all_text_contents() == ["Component", "Description", "Structural", "Behavioral"]
            for scenario, _ in scenarios:
                block = page.locator(f'.example-table-block[data-scenario="{scenario}"]')
                rows = block.locator("tbody tr").evaluate_all("""rows => rows.map(row => ({
                    label:row.dataset.checkpoint,time:Number(row.dataset.time),index:Number(row.dataset.timelineIndex),
                    pins:Object.fromEntries([...row.querySelectorAll('td[data-bits]')].filter(c=>c.dataset.bits)
                        .map(c=>[c.dataset.pin,c.dataset.bits]))}))""")
                assert rows
                cases.append((scenario, block.get_attribute("data-profile"), rows))
            for width in (640, 360):
                page.set_viewport_size({"width": width, "height": 1000})
                assert page.evaluate("document.documentElement.scrollWidth <= innerWidth+1"), section
                assert page.locator(".example-table-scroll").count() == len(scenarios)
            context.close()
        for scenario, profile, rows in cases:
            context = browser.new_context(viewport={"width": 1440, "height": 1000})
            page = context.new_page()
            errors, api = [], []
            page.on("pageerror", lambda e: errors.append(str(e)))
            page.on("request", lambda r: api.append(r.url) if "/api/" in r.url else None)
            page.goto(urljoin(base, f"simulator/?scenario={scenario}"), wait_until="domcontentloaded")
            page.locator("body[data-circuitsim-status='ready']").wait_for(timeout=90000)
            assert page.locator("body").get_attribute("data-circuitsim-runtime") == "wasm"
            assert page.locator("#toggle-explorer").is_visible()
            state = page.evaluate("""() => ({profile:app.components.get(app.rootId).profileFingerprint,
                checkpoints:app.checkpoints.map(c=>({label:c.label,time:c.time,index:c.index}))})""")
            assert state["profile"] == profile, scenario
            assert state["checkpoints"] == [{k:r[k] for k in ("label","time","index")} for r in rows]
            for row in rows:
                pins = page.evaluate("""async index=>{await setStateIndex(index);return Object.fromEntries(
                    [...app.pins.values()].filter(p=>p.componentId===app.rootId).map(p=>[p.name,app.state.pins[p.stateIndex]]));}""", row["index"])
                for pin, expected in row["pins"].items():
                    assert pins[pin] == expected, (scenario, row["label"], pin)
            if scenario in ("dram", "rom", "nor-flash"):
                verify_matrix(page, "MEMORY_ROOT.STORAGE")
                verify_peripheral(page, "MEMORY_ROOT.COLUMN_SELECT")
                if scenario != "rom":
                    verify_peripheral(page, "MEMORY_ROOT.WRITE_DRIVERS")
                page.evaluate("() => smoothlyFocusComponent(app.rootId)")
                page.wait_for_function("!app.cameraAnimation && !app.geometryDirty")
                page.evaluate("() => {app.explorer.setOpen(true,{persist:false}); smoothlyFocusComponent(app.rootId);}")
                page.wait_for_function("!app.cameraAnimation && !app.geometryDirty")
                if screenshots:
                    page.screenshot(path=str(screenshots / f"{scenario}-device.png"))
                page.locator('.explorer-row.selected .explorer-arrow').click()
                page.locator('#apply-profile').click()
                page.wait_for_function("app.components.size === 1 && app.components.get(app.rootId).fidelity === 'behavioral' && document.querySelector('#loading').classList.contains('hidden')")
                for index in (0, -1):
                    state = page.evaluate("""async index => {
                        await setStateIndex(index < 0 ? app.timestamps.length-1 : index);
                        return app.componentStates.get(app.rootId);
                    }""", index)
                    assert state and state["supported"], (scenario, "behavioral contents overlay missing")
                    assert state["wordBits"] == 8 and len(state["windowWords"]) == 4
                    words = {word["address"]: word["bits"] for word in state["windowWords"]}
                    expected = {"dram": "XXXXXXXX" if index == 0 else "01011010",
                                "rom": "10100101", "nor-flash": "11111111"}
                    assert words[0] == expected[scenario], (scenario, index, words)
                    if scenario != "rom":
                        assert state["activeAddress"] is None and "ERROR" in state["ports"]
                # Check the rendered canvas path, not merely a backend snapshot.
                labels = page.evaluate("""async () => {
                    const labels=[], original=ctx.fillText;
                    ctx.fillText=function(text,...args) {labels.push(String(text)); return original.call(this,text,...args);};
                    try { requestRender(); await new Promise(r=>requestAnimationFrame(()=>requestAnimationFrame(r))); }
                    finally {ctx.fillText=original;}
                    return labels;
                }""")
                assert any(label.startswith("SNAPSHOT" if scenario != "rom" else "ADDR")
                           for label in labels), (scenario, labels)
                assert {"dram": "5A", "rom": "A5", "nor-flash": "78"}[scenario] in labels
                assert page.evaluate("""() => [8,16,32].map(width => [
                    registerWordDisplay({bits:'0'.repeat(width-1)+'1',hex:'0x1'}),
                    registerWordDisplay({bits:'X'.repeat(width)})])""") == [
                        ["01", "XX"], ["0001", "XXXX"], ["00000001", "XXXXXXXX"]]
                if screenshots:
                    page.screenshot(path=str(screenshots / f"{scenario}-behavioral.png"))
                page.locator('.explorer-row.selected .explorer-arrow').click()
                page.locator('#apply-profile').click()
                page.wait_for_function("app.components.size > 20 && app.components.get(app.rootId).fidelity === 'structural' && document.querySelector('#loading').classList.contains('hidden')")
            if scenario.endswith('storage-bank'):
                verify_matrix(page, "MEMORY_ROOT")
                if screenshots:
                    page.screenshot(path=str(screenshots / f"{scenario}.png"))
            if scenario in ("matrix-column-mux", "matrix-write-distributor"):
                verify_peripheral(page, "MEMORY_ROOT")
                if screenshots:
                    page.screenshot(path=str(screenshots / f"{scenario}.png"))
            assert not errors, errors
            assert not api, api
            print(f"{scenario}: {len(rows)} browser checkpoints passed", flush=True)
            context.close()
        # Real document embeds, not only their full-screen counterparts. Other
        # frames receive inert HTML so only one WASM heap exists per context.
        for section, scenario in (("3.1.3", "dram"), ("3.1.4", "rom"), ("3.1.5", "nor-flash")):
            context = browser.new_context(viewport={"width": 1440, "height": 1000})
            def isolate(route):
                if route.request.url.endswith("?scenario=" + scenario):
                    route.continue_()
                else:
                    route.fulfill(status=200, content_type="text/html", body="<html><body>Other example paused for this test</body></html>")
            context.route("**/simulator/?scenario=*", isolate)
            page = context.new_page()
            page.goto(urljoin(base, f"memory/{section}/"), wait_until="domcontentloaded")
            selector = f'iframe[src$="?scenario={scenario}"]'
            page.locator(selector).scroll_into_view_if_needed()
            frame = page.frame_locator(selector)
            frame.locator("body[data-circuitsim-status='ready']").wait_for(timeout=90000)
            assert frame.locator("#toggle-explorer").is_visible()
            if screenshots:
                page.screenshot(path=str(screenshots / f"{scenario}-embedded.png"))
            context.close()
        browser.close()
    print(f"{len(cases)} new-memory scenarios / {sum(len(rows) for _, _, rows in cases)} checkpoints passed; "
          "tables, profiles, mobile bounds, matrix/peripheral geometry and WASM runtime checked")


if __name__ == "__main__":
    main()
