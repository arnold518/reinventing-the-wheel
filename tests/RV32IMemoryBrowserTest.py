"""Check the migrated CPU roots, memory inspector and layout in actual WASM."""
from pathlib import Path
import shutil
import sys
from urllib.parse import urljoin
from playwright.sync_api import sync_playwright

base = sys.argv[1] if len(sys.argv) > 1 else "http://127.0.0.1:8893/"
screens = Path(sys.argv[2]) if len(sys.argv) > 2 else None
if screens:
    screens.mkdir(parents=True, exist_ok=True)
settled = "!app.cameraAnimation && !app.geometryDirty && !app.topologyRequestInFlight && !app.pendingScopeIds.size"
with sync_playwright() as p:
    browser = p.chromium.launch(executable_path=shutil.which("google-chrome") or shutil.which("chromium"),
                                headless=True, args=["--no-sandbox"])
    for scenario in ("rv32i-program3", "rv32i-five-stage-program3"):
        for fidelity in ("structural", "behavioral"):
            context = browser.new_context(viewport={"width": 1440, "height": 1000})
            page = context.new_page()
            page.set_default_timeout(90000)
            errors, api = [], []
            page.on("pageerror", lambda e: errors.append(str(e)))
            page.on("request", lambda r: api.append(r.url) if "/api/" in r.url else None)
            assert page.goto(urljoin(base, f"simulator/?scenario={scenario}&core_fidelity={fidelity}"),
                             wait_until="domcontentloaded").ok
            page.locator("body[data-circuitsim-status='ready']").wait_for()
            assert page.locator("body").get_attribute("data-circuitsim-runtime") == "wasm"
            page.evaluate("()=>smoothlyFocusComponent(app.rootId)")
            page.wait_for_function(settled)
            children = page.evaluate("""()=>Object.fromEntries(app.components.get(app.rootId).childIds
                .map(id=>app.components.get(id)).map(c=>[c.name,{type:c.type,rect:c.rect}]))""")
            assert len(children) == 4
            parent = page.evaluate("()=>app.components.get(app.rootId).rect")
            for name, child in children.items():
                r = child["rect"]
                assert r["x"] >= parent["x"] and r["y"] >= parent["y"], name
                assert r["x"] + r["w"] <= parent["x"] + parent["w"] + .01, name
                assert r["y"] + r["h"] <= parent["y"] + parent["h"] + .01, name
            for memory in ("INSTRUCTION_MEMORY", "DATA_MEMORY"):
                assert children[memory]["type"] == "MemoryContainer<sram,32>"
            assert children["INSTRUCTION_MEMORY"]["rect"]["x"] < children["CORE"]["rect"]["x"] < children["DATA_MEMORY"]["rect"]["x"]
            rects = [(n, c["rect"]) for n, c in children.items()]
            for i, (name, a) in enumerate(rects):
                for other, b in rects[i + 1:]:
                    x = min(a["x"] + a["w"], b["x"] + b["w"]) - max(a["x"], b["x"])
                    y = min(a["y"] + a["h"], b["y"] + b["h"]) - max(a["y"], b["y"])
                    assert x <= .01 or y <= .01, (scenario, fidelity, name, other)
            status = page.evaluate("""async()=>{await setStateIndex(app.checkpoints.at(-1).index);
                return Object.fromEntries([...app.pins.values()].filter(p=>p.componentId===app.rootId)
                    .map(p=>[p.name,app.state.pins[p.stateIndex]]));}""")
            assert status["HALTED"] == "1" and status["TRAPPED"] == "0", status
            if screens:
                page.screenshot(path=str(screens / f"{scenario}-{fidelity}.png"))
                if fidelity == "structural":
                    for part in ("CORE", "CORE.FETCH.REQUEST" if scenario == "rv32i-program3" else "CORE.FETCH_REQUEST"):
                        page.evaluate("id=>smoothlyFocusComponent(id)", "RV32I_PROGRAM_ROOT." + part)
                        page.wait_for_function(settled)
                        page.screenshot(path=str(screens / f"{scenario}-{part}.png"))
            page.evaluate("()=>smoothlyFocusComponent(app.rootId+'.DATA_MEMORY')")
            page.wait_for_function(settled)
            page.wait_for_function("app.componentStates.get(app.rootId+'.DATA_MEMORY')?.supported === true")
            snapshot = page.evaluate("()=>app.componentStates.get(app.rootId+'.DATA_MEMORY')")
            assert "RSP_STATUS" in snapshot["ports"] and snapshot["windowWords"]
            assert not errors, errors
            assert not api, api
            context.close()
            print(f"{scenario}/{fidelity}: layout, terminal state and memory inspector passed", flush=True)
    browser.close()
