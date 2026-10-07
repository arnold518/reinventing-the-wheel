"""Real WASM layout refresh, seeded with an intentionally stale snapshot."""
from copy import deepcopy
import json
from pathlib import Path
import shutil
import sys
from playwright.sync_api import sync_playwright

ROOT = Path(__file__).resolve().parents[2]
base = sys.argv[1].rstrip("/") + "/"
shipped = json.loads((ROOT / "visualizer-v2/layout.json").read_text())
old = deepcopy(shipped)
# Mimic a browser snapshot saved before these automatic scopes existed. The
# static package must restore them while keeping unrelated user placements.
old["type_layouts"].pop("Register<32>")
old["root_layouts"].pop("rv32i-memory-lanes")
screens = Path(sys.argv[2]) if len(sys.argv) > 2 else None
if screens:
    screens.mkdir(parents=True, exist_ok=True)
saved = deepcopy(old)
saved["type_layouts"]["ALU8"]["color"] = [17, 34, 51, 190]
saved["type_layouts"]["ALU8"]["children"]["CONTROL"]["rel_pos"] = [.123456, .234567]
saved["type_layouts"]["SRAMRow<16>"]["children"]["CELL_0"]["rel_pos"] = [.0314159, .271828]
# Real pre-refresh geometry, including a two-input rewire's old proportions.
# Exercise both shared and profile-scoped defaults; an already-current fixture
# would not prove that a user's saved browser snapshot can actually upgrade.
routing_scopes = []
for section in ["type_layouts", *shipped["profile_layouts"]]:
    source = shipped[section] if section == "type_layouts" else shipped["profile_layouts"][section]
    destination = saved[section] if section == "type_layouts" else saved["profile_layouts"][section]
    for kind, entry in source.items():
        if not (kind.startswith("RV32IDecodeControlField<") or kind == "RV32IFetchRequest"):
            continue
        history = entry.get("automatic_layout_history", [])
        if not history:
            continue
        snapshot = history[-1]
        destination[kind]["children"] = deepcopy(snapshot["children"])
        destination[kind]["automatic_layout_revision"] = snapshot["revision"]
        destination[kind]["color"] = [23, 45, 67, 190]
        routing_scopes.append((section, kind))
rewire_kind = "Rewire<in=2,out=40>"
saved["type_layouts"][rewire_kind] = deepcopy(shipped["type_layouts"][rewire_kind]["previous_default"])
saved["type_layouts"][rewire_kind]["color"] = [31, 42, 53, 190]


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
    assert migrated["type_layouts"]["Register<32>"]["children"] == shipped["type_layouts"]["Register<32>"]["children"]
    assert migrated["root_layouts"]["rv32i-memory-lanes"]["children"] == shipped["root_layouts"]["rv32i-memory-lanes"]["children"]
    assert migrated["type_layouts"]["ALU8"]["color"] == [17,34,51,190]
    assert migrated["type_layouts"]["SRAMRow<16>"] == saved["type_layouts"]["SRAMRow<16>"]

    def check_routing_migration():
        current = page.evaluate("app.layout")
        assert len(routing_scopes) >= 19
        for section, kind in routing_scopes:
            source = shipped[section] if section == "type_layouts" else shipped["profile_layouts"][section]
            target = current[section] if section == "type_layouts" else current["profile_layouts"][section]
            for name, expected in source[kind]["children"].items():
                assert target[kind]["children"][name]["rel_pos"] == expected["rel_pos"], (section, kind, name)
                assert target[kind]["children"][name]["rel_width"] == expected["rel_width"]
            assert target[kind]["color"] == [23, 45, 67, 190]
        actual = current["type_layouts"][rewire_kind]
        for key in ("aspect_ratio", "min_aspect_ratio", "title_bar_ratio", "pin_size_ratio", "boundary_area_ratio"):
            assert actual[key] == shipped["type_layouts"][rewire_kind][key], key
        assert actual["color"] == [31, 42, 53, 190]

    check_routing_migration()

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
    check_routing_migration()
    assert not errors, errors
    print(f"{count} saved type/root defaults migrated; manual edits preserved; all eight lane inputs route forward before/after reload")
    print(f"{len(routing_scopes)} decode/fetch defaults and two-bus rewire style upgraded before/after reload; colors preserved")
    browser.close()
