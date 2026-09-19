"""Real WASM layout refresh, seeded with a pre-refresh browser snapshot."""
from copy import deepcopy
import json
from pathlib import Path
import shutil
import sys
from playwright.sync_api import sync_playwright

ROOT = Path(__file__).resolve().parents[1]
base = sys.argv[1].rstrip("/") + "/"
old = json.loads(Path(sys.argv[2]).read_text())
shipped = json.loads((ROOT / "visualizer-v2/layout.json").read_text())
screens = Path(sys.argv[3]) if len(sys.argv) > 3 else None
if screens:
    screens.mkdir(parents=True, exist_ok=True)
saved = deepcopy(old)
saved["type_layouts"]["ALU8"]["color"] = [17, 34, 51, 190]
saved["type_layouts"]["ALU8"]["children"]["CONTROL"]["rel_pos"] = [.123456, .234567]
saved["type_layouts"]["SRAMRow<16>"]["children"]["CELL_0"]["rel_pos"] = [.0314159, .271828]


def idle(page):
    page.wait_for_function("!app.cameraAnimation && !app.geometryDirty && !app.topologyRequestInFlight && !app.pendingScopeIds.size")


def ready(page):
    page.wait_for_function("['ready','error'].includes(document.body.dataset.circuitsimStatus)")
    assert page.locator("body").get_attribute("data-circuitsim-status") == "ready", page.locator("body").inner_text()[-2000:]


with sync_playwright() as p:
    browser = p.chromium.launch(executable_path=shutil.which("google-chrome") or shutil.which("chromium"),
                                headless=True, args=["--no-sandbox"])
    page = browser.new_page(viewport={"width": 1600, "height": 1000})
    page.set_default_timeout(60000)
    errors = []
    page.on("pageerror", lambda error: errors.append(str(error)))
    seed = base + "layout-seed.html"
    page.route(seed, lambda route: route.fulfill(body="<!doctype html><title>Layout fixture</title>",content_type="text/html"))
    page.goto(seed)
    page.evaluate("""layout=>new Promise((resolve,reject)=>{
        const r=indexedDB.open('circuitsim-visualizer-v2',1);
        r.onupgradeneeded=()=>r.result.createObjectStore('state');r.onerror=()=>reject(r.error);
        r.onsuccess=()=>{const db=r.result,tx=db.transaction('state','readwrite');
            tx.objectStore('state').put(layout,'layout');tx.oncomplete=()=>{db.close();resolve();};
            tx.onerror=()=>reject(tx.error);};
    })""", saved)
    page.unroute(seed)
    page.goto(base + "?scenario=rv32i-memory-lanes", wait_until="domcontentloaded")
    ready(page)
    assert page.locator("body").get_attribute("data-circuitsim-runtime") == "wasm"
    page.evaluate("smoothlyFocusComponent(app.rootId)")
    idle(page)
    migrated = page.evaluate("app.layout")
    count = 0
    for section, source in [("type_layouts", shipped["type_layouts"]), ("root_layouts", shipped["root_layouts"])]:
        for name, entry in source.items():
            if not entry.get("automatic_layout_revision"):
                continue
            actual = migrated[section][name]
            for child, geometry in entry["children"].items():
                if section == "type_layouts" and name == "ALU8" and child == "CONTROL":
                    assert actual["children"][child]["rel_pos"] == [.123456, .234567]
                elif child in saved[section].get(name, {}).get("children", {}):
                    assert actual["children"][child]["rel_pos"] == geometry["rel_pos"], (section,name,child)
                    assert actual["children"][child]["rel_width"] == geometry["rel_width"]
            count += 1
    assert count > 100
    assert migrated["type_layouts"]["ALU8"]["color"] == [17,34,51,190]
    assert migrated["type_layouts"]["SRAMRow<16>"] == saved["type_layouts"]["SRAMRow<16>"]

    def check_routes():
        routes = page.evaluate("""()=>{
            const routes=[];
            for(const wire of app.wireOrder) {
                const source=wire.sourcePin;
                if(!source || source.componentId!==app.rootId || !['WORD_DATA','WRITE_DATA'].includes(source.name)) continue;
                wire.sinkPins.forEach((sink,index)=>{
                    if(!/^LANE_[0-3]$/.test(app.components.get(sink.componentId).name)) return;
                    routes.push({source:activeStub(source,true),sink:activeStub(sink,false),
                        points:wire.branches[index].points,name:sink.id});
                });
            }
            return routes;
        }""")
        assert len(routes) == 8, routes
        for route in routes:
            assert route["sink"][1]["x"] >= route["source"][1]["x"]-1e-9, route
            assert all(b["x"] >= a["x"]-1e-9 for a,b in zip(route["points"],route["points"][1:])), route
        return routes

    check_routes()
    if screens:
        page.screenshot(path=str(screens / "memory-lanes-overview.png"))
    page.evaluate("smoothlyFocusComponent(app.rootId+'.LANE_0')")
    idle(page)
    check_routes()
    if screens:
        page.screenshot(path=str(screens / "memory-lane-0.png"))
    # Save the migrated snapshot, reload, and ensure the one-time upgrade sticks.
    page.evaluate("saveLayout()")
    page.reload(wait_until="domcontentloaded")
    ready(page)
    page.evaluate("smoothlyFocusComponent(app.rootId)")
    idle(page)
    check_routes()
    assert page.evaluate("app.layout.type_layouts.ALU8.children.CONTROL.rel_pos") == [.123456,.234567]
    assert not errors, errors
    print(f"{count} saved type/root defaults migrated; manual edits preserved; all eight lane inputs route forward before/after reload")
    browser.close()
