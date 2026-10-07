"""Inspect focused RV32I datapaths in the real static/WASM visualizer.

Run serially: complete CPU sessions, not concurrent browser tabs.
Optional screenshot directory makes the geometry audit visually reviewable.
"""
from pathlib import Path
import json
import os
import shutil
import sys
from urllib.parse import urljoin
from playwright.sync_api import sync_playwright

base = sys.argv[1] if len(sys.argv) > 1 else "http://127.0.0.1:8893/"
screens = Path(sys.argv[2]) if len(sys.argv) > 2 else None
legacy = json.loads(Path(sys.argv[3]).read_text()) if len(sys.argv) > 3 else None
shipped = json.loads((Path(__file__).resolve().parents[2]/"visualizer-v2/layout.json").read_text())
if screens:
    screens.mkdir(parents=True, exist_ok=True)
settled = "!app.cameraAnimation && !app.geometryDirty && !app.topologyRequestInFlight && !app.pendingScopeIds.size"
with sync_playwright() as p:
    browser = p.chromium.launch(executable_path=shutil.which("google-chrome") or shutil.which("chromium"),
                                headless=True, args=["--no-sandbox"])
    for scenario, parts in (
        ("rv32i-core", ("", "FETCH", "NEXT_PC")),
        ("rv32i-program3", ("CORE", "CORE.FETCH", "CORE.DECODE", "CORE.EXECUTE", "CORE.MEMORY", "CORE.WRITEBACK", "CORE.NEXT_PC")),
        ("rv32i-five-stage-program3", ("CORE", "CORE.FETCH", "CORE.DECODE", "CORE.ID_EX",
                                      "CORE.EXECUTE", "CORE.MEMORY", "CORE.WRITEBACK", "CORE.COORDINATOR")),
    ):
        selected = os.environ.get("RITW_BROWSER_SCENARIOS", "").split(",")
        if selected != [""] and scenario not in selected:
            continue
        context = browser.new_context(viewport={"width": 1600, "height": 1000})
        page = context.new_page()
        page.set_default_timeout(90000)
        errors = []
        page.on("pageerror", lambda e: errors.append(str(e)))
        if legacy:
            seed = urljoin(base, "__layout_seed__.html")
            page.route(seed, lambda route: route.fulfill(status=200, content_type="text/html",
                body='<!doctype html><link rel="icon" href="data:,"><title>Layout setup</title>'))
            page.goto(seed, wait_until="domcontentloaded")
            page.evaluate("""layout=>new Promise((resolve,reject)=>{
                const req=indexedDB.open('circuitsim-visualizer-v2',1);
                req.onupgradeneeded=()=>req.result.createObjectStore('state');
                req.onerror=()=>reject(req.error);
                req.onsuccess=()=>{const db=req.result, tx=db.transaction('state','readwrite');
                    tx.objectStore('state').put(layout,'layout');
                    tx.oncomplete=()=>{db.close();resolve();}; tx.onerror=()=>reject(tx.error);};
            })""", legacy)
            page.unroute(seed)
        assert page.goto(urljoin(base, f"simulator/?scenario={scenario}&core_fidelity=structural"),
                         wait_until="domcontentloaded").ok
        page.locator("body[data-circuitsim-status='ready']").wait_for()
        for part in parts:
            root_id = "RV32I_SINGLE_CYCLE_CORE_ROOT" if scenario == "rv32i-core" else "RV32I_PROGRAM_ROOT"
            component_id = root_id + ("." + part if part else "")
            page.evaluate("id=>smoothlyFocusComponent(id)", component_id)
            page.wait_for_function(settled)
            if scenario != "rv32i-five-stage-program3" and part in ("", "CORE"):
                # First verify migration, then exercise the actual Restore
                # Children action: it must return the same stage-aware layout.
                restored = page.evaluate("""async id=>{
                    const parent=app.components.get(id);
                    const before=parent.childIds.map(key=>({...app.components.get(key).rect}));
                    await restoreSelectedChildrenLayout();
                    return before;
                }""", component_id)
                page.wait_for_function(settled)
                after = page.evaluate("id=>app.components.get(id).childIds.map(key=>app.components.get(key).rect)", component_id)
                assert restored == after, (scenario, "Restore Children changed stage-aware defaults")
            geometry = page.evaluate("""id=>{const parent=app.components.get(id);
                return {parent:parent.rect, type:parent.type, children:parent.childIds.map(key=>app.components.get(key))
                    .map(c=>({name:c.name,...c.rect}))};}""", component_id)
            parent, children = geometry["parent"], geometry["children"]
            expected = shipped["type_layouts"][geometry["type"]]["aspect_ratio"]
            assert abs(parent["h"]/parent["w"]-expected) < .002, (part,"stale proportions",parent,expected)
            assert children, part
            if geometry["type"] == "RV32ISingleCycleCore":
                by_name = {c["name"]: c for c in children}
                order = ("FETCH", "DECODE", "EXECUTE", "MEMORY", "WRITEBACK")
                for a,b in zip(order, order[1:]):
                    assert by_name[a]["x"] + by_name[a]["w"] < by_name[b]["x"], (scenario, "stage order")
                centers = [by_name[n]["y"] + by_name[n]["h"]/2 for n in order]
                assert max(centers) - min(centers) < .01, (scenario, "misaligned stage row")
                assert max(by_name[n]["y"] + by_name[n]["h"] for n in order) < min(
                    by_name[n]["y"] for n in ("NEXT_PC", "EXECUTION_STATUS")), (scenario, "control row")
            for c in children:
                assert c["x"] >= parent["x"] and c["y"] >= parent["y"], (part, c)
                assert c["x"]+c["w"] <= parent["x"]+parent["w"]+.01, (part, c)
                assert c["y"]+c["h"] <= parent["y"]+parent["h"]+.01, (part, c)
                if geometry["type"].startswith("RV32ISingleCycle"):
                    placement = shipped["type_layouts"][geometry["type"]]["children"][c["name"]]
                    actual = [(c["x"]-parent["x"])/parent["w"], (c["y"]-parent["y"])/parent["h"]]
                    assert all(abs(a-b) < 1e-6 for a,b in zip(actual, placement["rel_pos"])), (part,c["name"],"stale position")
                    assert abs(c["w"]/parent["w"]-placement["rel_width"]) < 1e-6, (part,c["name"],"stale width")
            for i, a in enumerate(children):
                for b in children[i+1:]:
                    dx = min(a["x"]+a["w"], b["x"]+b["w"])-max(a["x"], b["x"])
                    dy = min(a["y"]+a["h"], b["y"]+b["h"])-max(a["y"], b["y"])
                    assert dx <= .01 or dy <= .01, (part, a["name"], b["name"])
            if screens:
                page.screenshot(path=str(screens / f"{scenario}-{part}.png"))
            print(f"{scenario}/{part}: {len(children)} children checked", flush=True)
        assert not errors, errors
        context.close()
    browser.close()
