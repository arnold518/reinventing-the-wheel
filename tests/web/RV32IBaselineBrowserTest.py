"""Baseline docs and actual WASM UI; create only one simulator context at a time."""
from pathlib import Path
import shutil
import sys
from urllib.parse import urljoin
from playwright.sync_api import sync_playwright

BASE = (sys.argv[1] if len(sys.argv) > 1 else "http://127.0.0.1:8893/").rstrip("/") + "/"
SCREENS = Path(sys.argv[2]) if len(sys.argv) > 2 else None
if SCREENS:
    SCREENS.mkdir(parents=True, exist_ok=True)
SETTLED = "!app.cameraAnimation && !app.geometryDirty && !app.topologyRequestInFlight && !app.pendingScopeIds.size"
ORDER = ["FETCH", "IF_ID", "DECODE", "ID_EX", "EXECUTE", "EX_MEM", "MEMORY", "MEM_WB", "WRITEBACK"]

with sync_playwright() as p:
    browser = p.chromium.launch(executable_path=shutil.which("google-chrome") or shutil.which("chromium"),
                                headless=True, args=["--no-sandbox"])
    # Check docs without starting many lazy iframe simulators.
    context = browser.new_context(viewport={"width": 1440, "height": 1000})
    context.route("**/simulator/**", lambda route: route.abort())
    page = context.new_page()
    page.set_default_navigation_timeout(90000)
    for slug, tables, embeds in [("",0,0),("registers/",4,4),("fetch/",1,1),("decode/",1,1),
                                ("execute/",1,1),("memory/",1,1),("writeback/",2,2),("control/",2,2),("core/",2,2)]:
        print(f"Checking documentation: 4.5/{slug}", flush=True)
        response = page.goto(urljoin(BASE,"rv32i/4.5/"+slug),wait_until="domcontentloaded")
        assert response.ok, (slug,response.status)
        assert page.locator(".md-content h1").inner_text().startswith("§ 4.5")
        assert page.locator(".example-table-scroll").count() == tables, slug
        assert page.locator(".simulator-embed iframe").count() == embeds, slug
        assert "--8<--" not in page.locator(".md-content").inner_text(), slug
        if slug == "control/":
            assert page.locator('.example-table-scroll').last.locator('tbody tr').count() == 2058
            assert page.locator('.example-table-scroll').last.evaluate("e=>getComputedStyle(e).overflowX") == "auto"
            for anchor in ["control-responsibilities", "a-stall-does-not-stop-the-clock",
                           "data-hazard-hold-the-consumer-advance-the-producer",
                           "jump-stop-younger-fetches-let-the-jump-resolve",
                           "a-branch-or-jalr-can-wait-for-data-too", "signals-behind-the-walkthroughs"]:
                assert page.locator("#"+anchor).count() == 1, anchor
            for header, count in [("Dependency / ending-edge action",9),
                                  ("Fetch and ending-edge action",5), ("IF_ID WRITE / FLUSH",5)]:
                table = page.locator(".md-content table").filter(
                    has=page.get_by_role("columnheader",name=header,exact=True))
                assert table.count() == 1 and table.locator("tbody tr").count() == count, header
            for width in [1440,360]:
                page.set_viewport_size({"width":width,"height":1000})
                for scheme in ["default","slate"]:
                    page.locator("body").evaluate("(e,s)=>e.setAttribute('data-md-color-scheme',s)",scheme)
                    assert page.evaluate("document.documentElement.scrollWidth <= innerWidth+1"), (width,scheme)
            page.set_viewport_size({"width":1440,"height":1000})
            page.locator("body").evaluate("e=>e.setAttribute('data-md-color-scheme','default')")
    context.close()
    for scenario in ["rv32i-baseline-core", "rv32i-baseline-program1"]:
        print(f"Checking WASM visualizer: {scenario}", flush=True)
        context = browser.new_context(viewport={"width":1440,"height":1000})
        page = context.new_page();page.set_default_timeout(90000)
        errors,api=[],[]
        page.on("pageerror",lambda e:errors.append(str(e)))
        page.on("request",lambda r:api.append(r.url) if "/api/" in r.url else None)
        assert page.goto(urljoin(BASE,"simulator/?scenario="+scenario),wait_until="domcontentloaded").ok
        try:
            page.locator("body[data-circuitsim-status='ready']").wait_for()
        except Exception:
            print("Startup errors:", errors, page.locator("body").inner_text()[-2000:], flush=True)
            if SCREENS:
                page.screenshot(path=str(SCREENS/(scenario+"-startup-error.png")))
            raise
        assert page.locator("body").get_attribute("data-circuitsim-runtime") == "wasm"
        root = page.evaluate("app.rootId")
        core = root+".CORE" if scenario.endswith("program1") else root
        page.evaluate("id=>smoothlyFocusComponent(id)",core);page.wait_for_function(SETTLED)
        children = page.evaluate("""id=>Object.fromEntries(app.components.get(id).childIds.map(k=>app.components.get(k))
            .map(c=>[c.name,{type:c.type,rect:c.rect}]))""",core)
        assert set(children) == {*ORDER,"DEPENDENCIES","CONTROL","NO_MEMORY_WAIT","EXECUTION_STATUS",
            "INSTRUCTION_PORT","DATA_PORT","INSTRUCTION_NO_WRITE","INSTRUCTION_WRITE_ZERO","INSTRUCTION_MASK_ZERO"}
        rects = [children[n]["rect"] for n in ORDER]
        assert all(a["x"]+a["w"]<b["x"] for a,b in zip(rects,rects[1:])), children
        assert max(r["y"]+r["h"] for r in rects) < min(children[n]["rect"]["y"] for n in ["DEPENDENCIES","CONTROL"])
        # Actual rendered rectangles, not just the saved JSON.
        parent = page.evaluate("id=>app.components.get(id).rect",core)
        for name,c in children.items():
            r=c["rect"]
            assert r["x"]>=parent["x"] and r["y"]>=parent["y"],name
            assert r["x"]+r["w"]<=parent["x"]+parent["w"]+.01 and r["y"]+r["h"]<=parent["y"]+parent["h"]+.01,name
        if SCREENS:
            page.screenshot(path=str(SCREENS/(scenario+"-core.png")))
        for group in ["DECODE","EXECUTE","MEMORY","WRITEBACK","DEPENDENCIES","CONTROL","EXECUTION_STATUS"]:
            page.evaluate("id=>smoothlyFocusComponent(id)",core+"."+group);page.wait_for_function(SETTLED)
            primary = {"DECODE": ["CONTROL", "REGISTER_FILE"],
                       "EXECUTE": ["DATAPATH"], "MEMORY": ["ACCESS"], "WRITEBACK": ["RETIREMENT", "WRITEBACK_MUX"]}.get(group, [])
            if primary:
                stage = page.evaluate("""id=>{
                    const p=app.components.get(id);
                    return {rect:p.rect,children:Object.fromEntries(p.childIds.map(k=>app.components.get(k))
                        .map(c=>[c.name,c.rect]))};
                }""",core+"."+group)
                if group == "WRITEBACK":
                    assert not any(name.startswith("RETIRED_") for name in stage["children"])
                    assert "COMMITTED_PC_STATE" not in stage["children"]
                    assert "HALTED_STATE" not in stage["children"]
                else:
                    assert "PACKET_STATUS" in stage["children"], group
                # The functional blocks must not shrink to gate-sized boxes.
                for name in primary:
                    assert stage["children"][name]["w"] / stage["rect"]["w"] > .20, (group,name,stage)
                for name,r in stage["children"].items():
                    p_rect=stage["rect"]
                    assert r["x"]>=p_rect["x"]-.01 and r["y"]>=p_rect["y"]-.01,(group,name)
                    assert r["x"]+r["w"]<=p_rect["x"]+p_rect["w"]+.01,(group,name)
                    assert r["y"]+r["h"]<=p_rect["y"]+p_rect["h"]+.01,(group,name)
            if SCREENS and scenario=="rv32i-baseline-core":
                page.screenshot(path=str(SCREENS/("baseline-"+group.lower()+".png")))
        if scenario.endswith("program1"):
            page.evaluate("async()=>await setStateIndex(app.checkpoints.at(-1).index)")
            page.evaluate("id=>smoothlyFocusComponent(id)",root+".DATA_MEMORY");page.wait_for_function(SETTLED)
            page.wait_for_function("app.componentStates.get(app.rootId+'.DATA_MEMORY')?.supported === true")
            memory = page.evaluate("app.componentStates.get(app.rootId+'.DATA_MEMORY')")
            assert "RSP_STATUS" in memory["ports"] and memory["windowWords"]
        assert not errors,errors
        assert not api,api
        context.close()
        print(scenario+": stage floorplan, focus, memory inspection and serverless runtime passed",flush=True)
    browser.close()
