"""Rendered 4.3 pages and every new embed, serially to bound browser memory."""
from pathlib import Path
import os
import json
import re
import shutil
import sys
from urllib.parse import urljoin
from playwright.sync_api import sync_playwright

ROOT=Path(__file__).resolve().parents[1]
base=(sys.argv[1] if len(sys.argv)>1 else "http://127.0.0.1:8893/").rstrip("/")+"/"
screens=Path(sys.argv[2]) if len(sys.argv)>2 else None
if screens: screens.mkdir(parents=True,exist_ok=True)
exports=json.loads(Path(os.environ.get('CIRCUITSIM_EXAMPLE_ROWS', ROOT/"build-wasm/pages/simulator/example-rows.json")).read_text())["scenarios"]
shipped=json.loads((ROOT/"visualizer-v2/layout.json").read_text())
scenarios={}
DETAILS = {
    "register32": ("BIT_0",),
    "alu32": ("CONTROL","ARITHMETIC","LOGIC","SHIFT","COMPARE","COMPARE.ARITHMETIC_ZERO","RESULT_SELECT","FLAGS","FLAGS.RESULT_ZERO"),
    "rv32i-decode-control": ("RECOGNITION","CONTROL_SIGNALS","CONTROL_SIGNALS.ALU_OP","CONTROL_SIGNALS.LEGAL"),
    "rv32i-memory-lanes": ("ADDRESS","LOAD_EXTEND","LOAD_EXTEND.BYTE_0","LOAD_EXTEND.BYTE_3","WRITE_MASK"),
    "rv32i-memory-request": ("TAG_MATCH",),
    "rv32i-execution-status": ("PRIORITY_PC_MISALIGNED",),
}
legacy=json.loads(Path(sys.argv[3]).read_text()) if len(sys.argv)>3 else None
with sync_playwright() as p:
    browser=p.chromium.launch(executable_path=shutil.which("google-chrome") or shutil.which("chromium"),
        headless=True,args=["--no-sandbox"])
    context=browser.new_context(java_script_enabled=False,viewport={"width":1440,"height":1000})
    page=context.new_page()
    assert page.goto(urljoin(base,"rv32i/4.3/"),wait_until="domcontentloaded").ok
    assert "Planned" not in page.locator("article").inner_text()
    for number,slug in enumerate(("register","register-file","alu32","instruction-decoder","control-flow","memory-access","execution-status"),1):
        assert page.goto(urljoin(base,f"rv32i/4.3/{slug}/"),wait_until="domcontentloaded").ok
        assert f"4.3.{number}." in page.locator("h1").inner_text()
        if slug == "register":
            assert "Register<W>" in page.locator("h1").inner_text()
            assert "Register<W>" in page.title()
            assert "Register<W>" in page.locator("nav.md-nav--primary").inner_text()
            assert "1, 2, 3, 4, 5, 8, 16, and 32 bits" in page.locator("article").inner_text()
        document=(ROOT/f"site-docs/rv32i/4.3/{slug}.md").read_text()
        aliases=re.findall(r"<!-- simulator-examples: (\S+) \S+ -->",document)
        assert page.locator(".simulator-embed iframe").count()==len(aliases)
        for alias in aliases:
            rows=page.locator(f'.example-table-block[data-scenario="{alias}"] tbody tr').evaluate_all(
                "rows=>rows.map(r=>({label:r.dataset.checkpoint,time:Number(r.dataset.time),index:Number(r.dataset.timelineIndex)}))")
            assert len(rows)==len(exports[alias]["rows"])
            assert page.locator(f'iframe[src*="scenario={alias}&"]').evaluate("el=>el.src")==urljoin(base,f"simulator/?scenario={alias}&embed=1")
            scenarios[alias]=rows
        for width in (1440,640,360):
            page.set_viewport_size({"width":width,"height":1000})
            assert page.evaluate("document.documentElement.scrollWidth<=innerWidth+1"),(slug,width)
            for table in page.locator(".example-table-scroll").all():
                assert table.evaluate("el=>el.tabIndex===0 && el.clientWidth<=innerWidth")
        page.set_viewport_size({"width":1440,"height":1000})
        if screens: page.screenshot(path=str(screens/f"{slug}-document.png"))
        print(f"4.3.{number}: rendered tables, navigation, embed paths and mobile widths passed",flush=True)
    context.close()
    for alias,rows in scenarios.items():
        selected=os.environ.get("RITW_BROWSER_SCENARIOS")
        if selected and alias not in selected.split(","): continue
        context=browser.new_context(viewport={"width":1600,"height":1000})
        page=context.new_page();page.set_default_timeout(90000)
        errors=[]
        page.on("pageerror",lambda error:errors.append(str(error)))
        page.on("console",lambda message:errors.append(message.text) if message.type=="error" else None)
        if legacy:
            # Seed this origin without loading the documentation's CDN scripts
            # or live-reload client; only the simulator is under test here.
            seed=urljoin(base,"__layout_seed__.html")
            page.route(seed,lambda route:route.fulfill(status=200,content_type="text/html",
                body='<!doctype html><link rel="icon" href="data:,"><title>Layout setup</title>'))
            page.goto(seed,wait_until="domcontentloaded")
            page.evaluate("""layout=>new Promise((resolve,reject)=>{
                const req=indexedDB.open('circuitsim-visualizer-v2',1);
                req.onupgradeneeded=()=>req.result.createObjectStore('state');
                req.onerror=()=>reject(req.error);
                req.onsuccess=()=>{const db=req.result,tx=db.transaction('state','readwrite');
                    tx.objectStore('state').put(layout,'layout');
                    tx.oncomplete=()=>{db.close();resolve();};tx.onerror=()=>reject(tx.error);};
            })""",legacy)
            page.unroute(seed)
        assert page.goto(urljoin(base,f"simulator/?scenario={alias}&embed=1"),wait_until="domcontentloaded").ok
        page.wait_for_function("['ready','error'].includes(document.body.dataset.circuitsimStatus)")
        assert page.locator("body").get_attribute("data-circuitsim-status")=="ready",(alias,errors,page.locator("body").inner_text())
        assert page.locator("body").get_attribute("data-circuitsim-runtime")=="wasm"
        assert page.evaluate("app.checkpoints.map(c=>({label:c.label,time:c.time,index:c.index}))")==rows,alias
        page.evaluate("smoothlyFocusComponent(app.rootId)")
        page.wait_for_function("!app.cameraAnimation && !app.geometryDirty && !app.topologyRequestInFlight && !app.pendingScopeIds.size")
        geometry=page.evaluate("""()=>{const c=app.components.get(app.rootId);return {
            parent:c.rect,children:c.childIds.map(id=>app.components.get(id)).map(c=>({name:c.name,...c.rect}))};}""")
        parent,children=geometry["parent"],geometry["children"]
        if alias == "register32":
            assert {c["name"] for c in children} == {"INPUT_BITS", "OUTPUT_BITS", *(f"BIT_{bit}" for bit in range(32))}
            assert page.evaluate("app.components.get(app.rootId+'.BIT_0').type") == "FlipFlopCell"
            assert not page.evaluate("app.components.has(app.rootId+'.STORAGE')")
        # A root layout saved for the active profile takes precedence over the scenario-wide one.
        expected=shipped["root_layouts"].get(page.evaluate("app.layoutKey")) or shipped["root_layouts"].get(alias,{})
        if expected.get("layout_revision") in {"rv32i-blocks-v4","rv32i-alu-auto-v5","rv32i-alu-auto-v6"}:
            if "aspect_ratio" in expected:
                assert abs(parent["h"]/parent["w"]-expected["aspect_ratio"])<.002,(alias,"stale proportions")
            for c in children:
                placement=expected["children"][c["name"]]
                actual=[(c["x"]-parent["x"])/parent["w"],(c["y"]-parent["y"])/parent["h"]]
                assert all(abs(a-b)<1e-6 for a,b in zip(actual,placement["rel_pos"])),(alias,c["name"],"stale position")
                assert abs(c["w"]/parent["w"]-placement["rel_width"])<1e-6,(alias,c["name"],"stale width")
        for c in children:
            assert c["x"]>=parent["x"] and c["y"]>=parent["y"],(alias,c)
            assert c["x"]+c["w"]<=parent["x"]+parent["w"]+.01,(alias,c)
            assert c["y"]+c["h"]<=parent["y"]+parent["h"]+.01,(alias,c)
        for i,a in enumerate(children):
            for b in children[i+1:]:
                dx=min(a["x"]+a["w"],b["x"]+b["w"])-max(a["x"],b["x"])
                dy=min(a["y"]+a["h"],b["y"]+b["h"])-max(a["y"],b["y"])
                assert dx<=.01 or dy<=.01,(alias,a["name"],b["name"])
        # Every exported expectation is checked at the exact displayed timeline
        # index, including partial expected-output rows and X/Z values.
        failures=page.evaluate("""async rows=>{
            const root=app.components.get(app.rootId), failures=[];
            const pins=new Map([...root.inputPins,...root.outputPins].map(p=>[p.name,p]));
            for(const row of rows) {
                await setStateIndex(row.timelineIndex);
                for(const [name,value] of Object.entries({...row.inputs,...row.expectedOutputs}))
                    if(pinValue(pins.get(name))!==value) failures.push([row.checkpoint,name,value,pinValue(pins.get(name))]);
            }
            return failures;
        }""",exports[alias]["rows"])
        assert not failures,(alias,failures)
        if alias == "alu32-flags":
            page.evaluate("smoothlyFocusComponent(app.rootId+'.SIGN_BIT')")
            page.wait_for_function("!app.cameraAnimation && !app.geometryDirty && !app.topologyRequestInFlight && !app.pendingScopeIds.size")
            shape=page.evaluate("""()=>{const c=app.components.get(app.rootId+'.SIGN_BIT');return {
                type:c.type,inputs:c.inputPins.map(p=>[p.name,p.width]),outputs:c.outputPins.map(p=>[p.name,p.width]),
                old:app.components.has(app.rootId+'.RESULT_SPLIT')};}""")
            assert shape=={"type":"Rewire","inputs":[["RESULT",32]],"outputs":[["SIGN",1]],"old":False},shape
            if screens: page.screenshot(path=str(screens/f"{alias}-SIGN_BIT.png"))
            page.evaluate("smoothlyFocusComponent(app.rootId)")
            page.wait_for_function("!app.cameraAnimation && !app.geometryDirty && !app.topologyRequestInFlight && !app.pendingScopeIds.size")
        if screens: page.screenshot(path=str(screens/f"{alias}-simulator.png"))
        # Inspect the new hierarchy and every named ALU32 subunit, not just
        # the parent rectangle. Run sequentially to avoid extra WASM instances.
        for suffix in DETAILS.get(alias,()):
            page.evaluate("suffix=>smoothlyFocusComponent(app.rootId+'.'+suffix)",suffix)
            page.wait_for_function("!app.cameraAnimation && !app.geometryDirty && !app.topologyRequestInFlight && !app.pendingScopeIds.size")
            result=page.evaluate("""suffix=>{const c=app.components.get(app.rootId+'.'+suffix);return {
                parent:c.rect,children:c.childIds.map(id=>app.components.get(id)).map(c=>({name:c.name,...c.rect}))};}""",suffix)
            pbox,parts=result["parent"],result["children"]
            assert parts,(alias,suffix,"missing structural internals")
            for child in parts:
                assert child["x"]>=pbox["x"]-.01 and child["y"]>=pbox["y"]-.01,(alias,suffix,child)
                assert child["x"]+child["w"]<=pbox["x"]+pbox["w"]+.01,(alias,suffix,child)
                assert child["y"]+child["h"]<=pbox["y"]+pbox["h"]+.01,(alias,suffix,child)
            for i,a in enumerate(parts):
                for b in parts[i+1:]:
                    dx=min(a["x"]+a["w"],b["x"]+b["w"])-max(a["x"],b["x"])
                    dy=min(a["y"]+a["h"],b["y"]+b["h"])-max(a["y"],b["y"])
                    assert dx<=.01 or dy<=.01,(alias,suffix,a["name"],b["name"])
            if suffix in {"COMPARE.ARITHMETIC_ZERO", "FLAGS.RESULT_ZERO"}:
                wire = page.evaluate("""suffix=>{
                    const id=app.rootId+'.'+suffix;
                    const source=app.components.get(id).inputPins.find(pin=>pin.name==='A');
                    const sink=app.components.get(id+'.A_SPLIT').inputPins.find(pin=>pin.name==='IN');
                    const w=[...app.wires.values()].find(w=>w.sourcePinId===source?.id &&
                        w.sinkPins.some(pin=>pin.id===sink?.id));
                    if (!w) throw new Error('Missing zero-detector input wire: '+suffix);
                    return {source:activeStub(source,true), sink:activeStub(sink,false),
                        points:w.branches[0].points};
                }""",suffix)
                assert wire["sink"][1]["x"] >= wire["source"][1]["x"]-1e-9, (suffix,"overlapping input stubs",wire)
                assert all(b["x"]>=a["x"]-1e-9 for a,b in zip(wire["points"],wire["points"][1:])), (suffix,"input bus doubles back",wire)
            if screens: page.screenshot(path=str(screens/f"{alias}-{suffix}.png"))
        assert not errors,(alias,errors)
        print(f"{alias}: {len(rows)} live WASM checkpoints and root layout passed",flush=True)
        context.close()
    browser.close()
