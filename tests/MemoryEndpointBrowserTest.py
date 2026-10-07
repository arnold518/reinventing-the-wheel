"""Check only the new protocol/controller/container pages, one WASM at a time."""
from pathlib import Path
import re
import shutil
import sys
from urllib.parse import urljoin
from playwright.sync_api import sync_playwright


def verify_endpoint_layout(page):
    """Check the saved, rendered layouts, not only the Python default generator."""
    port_id = page.evaluate("""() => [...app.components.values()]
        .find(c=>c.type.startsWith('MemoryTransactionPort<'))?.id ?? null""")
    if not port_id:
        return
    page.evaluate("id=>smoothlyFocusComponent(id)", port_id)
    settled = "!app.cameraAnimation && !app.geometryDirty && !app.topologyRequestInFlight && !app.pendingScopeIds.size"
    page.wait_for_function(settled)
    fields = page.evaluate("""id => Object.fromEntries(app.components.get(id).childIds
        .map(id=>app.components.get(id)).map(c=>[c.name,{id:c.id,rect:c.rect}]))""", port_id)
    ordered = [fields[n]["rect"]["x"] for n in ("REQUEST_OP", "REQUEST_ADDR", "REQUEST_DATA", "REQUEST_MASK", "RESPONSE")]
    assert ordered == sorted(ordered) and len(set(ordered)) == len(ordered)
    assert "PENDING" in fields and "PHASE_0" not in fields
    response = fields["RESPONSE"]["id"]
    page.evaluate("id=>smoothlyFocusComponent(id)", response)
    page.wait_for_function(settled)
    held = response+".HELD_DATA"
    page.evaluate("id=>smoothlyFocusComponent(id)", held)
    page.wait_for_function(settled)
    cells = page.evaluate("""id=>app.components.get(id).childIds.map(id=>app.components.get(id))
        .filter(c=>c.name.startsWith('BIT_')).sort((a,b)=>Number(a.name.slice(4))-Number(b.name.slice(4)))
        .map(c=>c.rect)""", held)
    assert len(cells) in (8,16,32)
    rows = [cells[i:i+8] for i in range(0,len(cells),8)]
    for row in rows:
        assert max(c["y"] for c in row) - min(c["y"] for c in row) < 1e-6
        assert all(a["x"]+a["w"] < b["x"] for a,b in zip(row,row[1:]))
    for top,bottom in zip(rows,rows[1:]):
        for a,b in zip(top,bottom):
            assert abs(a["x"]-b["x"]) < 1e-6 and a["y"]+a["h"] < b["y"]
    bounds = page.evaluate("""()=>[...app.components.values()]
        .filter(c=>/^Memory(Endpoint|Controller|TransactionPort|PayloadRegister)</.test(c.type))
        .flatMap(c=>c.childIds.map(id=>app.components.get(id)).filter(ch=>ch?.rect).map(ch=>({
            parent:c.id,child:ch.id,inside:ch.rect.x>=c.rect.x-0.01&&ch.rect.y>=c.rect.y-0.01&&
            ch.rect.x+ch.rect.w<=c.rect.x+c.rect.w+0.01&&ch.rect.y+ch.rect.h<=c.rect.y+c.rect.h+0.01})))""")
    assert bounds and all(b["inside"] for b in bounds), [b for b in bounds if not b["inside"]]
    page.evaluate("()=>smoothlyFocusComponent(app.rootId)")
    page.wait_for_function(settled)

root=Path(__file__).resolve().parents[1]
base=sys.argv[1] if len(sys.argv)>1 else "http://127.0.0.1:8893/"
screens=Path(sys.argv[2]) if len(sys.argv)>2 else None
if screens: screens.mkdir(parents=True, exist_ok=True)
cases=[]
with sync_playwright() as p:
    browser=p.chromium.launch(executable_path=shutil.which("google-chrome") or shutil.which("chromium"),
                              headless=True,args=["--no-sandbox"])
    for section in ("3.2","3.3","3.1.6"):
        context=browser.new_context(java_script_enabled=False,viewport={"width":1440,"height":1000})
        page=context.new_page()
        assert page.goto(urljoin(base,f"memory/{section}/"),wait_until="domcontentloaded").ok
        source=(root/f"site-docs/memory/{section}.md").read_text()
        scenarios=re.findall(r"<!-- simulator-examples: (\S+) (\S+) -->",source)
        assert page.locator(".simulator-embed iframe").count()==len(scenarios)
        assert page.locator("article table").first.locator("thead th").all_text_contents()==["Component","Description","Structural","Behavioral"]
        for scenario,_ in scenarios:
            block=page.locator(f'.example-table-block[data-scenario="{scenario}"]')
            rows=block.locator("tbody tr").evaluate_all("""rows=>rows.map(row=>({label:row.dataset.checkpoint,
                time:Number(row.dataset.time),index:Number(row.dataset.timelineIndex),
                pins:Object.fromEntries([...row.querySelectorAll('td[data-bits]')].filter(c=>c.dataset.bits)
                    .map(c=>[c.dataset.pin,c.dataset.bits]))}))""")
            assert rows,(section,scenario)
            cases.append((scenario,block.get_attribute("data-profile"),rows))
        for width in (640,360):
            page.set_viewport_size({"width":width,"height":1000})
            assert page.evaluate("document.documentElement.scrollWidth <= innerWidth+1"),section
        context.close()
    for scenario,profile,rows in cases:
        context=browser.new_context(viewport={"width":1440,"height":1000})
        page=context.new_page();errors=[];api=[]
        page.on("pageerror",lambda e:errors.append(str(e)))
        page.on("request",lambda r:api.append(r.url) if "/api/" in r.url else None)
        page.goto(urljoin(base,f"simulator/?scenario={scenario}"),wait_until="domcontentloaded")
        page.locator("body[data-circuitsim-status='ready']").wait_for(timeout=90000)
        assert page.locator("body").get_attribute("data-circuitsim-runtime")=="wasm"
        assert page.locator("#toggle-explorer").is_visible()
        state=page.evaluate("""()=>({profile:app.components.get(app.rootId).profileFingerprint,
            checkpoints:app.checkpoints.map(c=>({label:c.label,time:c.time,index:c.index}))})""")
        assert state["profile"]==profile,scenario
        assert state["checkpoints"]==[{k:r[k] for k in ("label","time","index")} for r in rows]
        for row in (rows[0],rows[len(rows)//2],rows[-1]):
            pins=page.evaluate("""async i=>{await setStateIndex(i);return Object.fromEntries(
                [...app.pins.values()].filter(p=>p.componentId===app.rootId).map(p=>[p.name,app.state.pins[p.stateIndex]]));}""",row["index"])
            for pin,value in row["pins"].items(): assert pins[pin]==value,(scenario,row["label"],pin)
        page.evaluate("() => {app.explorer.setOpen(true,{persist:false});smoothlyFocusComponent(app.rootId);}")
        page.wait_for_function("!app.cameraAnimation && !app.geometryDirty && !app.topologyRequestInFlight && !app.pendingScopeIds.size")
        if screens: page.screenshot(path=str(screens/f"{scenario}.png"))
        verify_endpoint_layout(page)
        if scenario=="memory-endpoint-sram":
            page.evaluate("() => smoothlyFocusComponent(app.rootId+'.MEMORY.STORAGE')")
            page.wait_for_function("!app.cameraAnimation && !app.geometryDirty && !app.topologyRequestInFlight && !app.pendingScopeIds.size")
            bounds=page.evaluate("""()=>[...app.components.values()].filter(c=>c.type.startsWith('SRAM'))
                .flatMap(c=>c.childIds.map(id=>app.components.get(id)).filter(ch=>ch?.rect).map(ch=>({
                    parent:c.id,child:ch.id,inside:ch.rect.x>=c.rect.x-0.01&&ch.rect.y>=c.rect.y-0.01&&
                    ch.rect.x+ch.rect.w<=c.rect.x+c.rect.w+0.01&&ch.rect.y+ch.rect.h<=c.rect.y+c.rect.h+0.01})))""")
            assert bounds and all(b["inside"] for b in bounds),[b for b in bounds if not b["inside"]]
            if screens: page.screenshot(path=str(screens/f"{scenario}-rows.png"))
            page.evaluate("() => smoothlyFocusComponent(app.rootId)")
            page.wait_for_function("!app.cameraAnimation && !app.geometryDirty && !app.topologyRequestInFlight && !app.pendingScopeIds.size")
            # Camera focus does not need to change the selected root row.
        if scenario.startswith("memory-endpoint-") and scenario!="memory-endpoint-relay":
            page.locator('.explorer-row.selected .explorer-arrow').click()
            page.locator('#apply-profile').click()
            page.wait_for_function("app.components.size===1 && app.components.get(app.rootId).fidelity==='behavioral' && document.querySelector('#loading').classList.contains('hidden')")
        if scenario in ("scalable-memory-container", "native-memory-container") or scenario.startswith("memory-endpoint-") and scenario!="memory-endpoint-relay":
            info=page.evaluate("""async()=>{await setStateIndex(app.timestamps.length-1);return app.componentStates.get(app.rootId);}""")
            assert info and info["supported"] and len(info["windowWords"])==4
            assert ("FAULT" if scenario == "native-memory-container" else "RSP_STATUS") in info["ports"]
            labels=page.evaluate("""async()=>{const labels=[],original=ctx.fillText;
                ctx.fillText=function(text,...args){labels.push(String(text));return original.call(this,text,...args)};
                try{requestRender();await new Promise(r=>requestAnimationFrame(()=>requestAnimationFrame(r)))}
                finally{ctx.fillText=original}return labels;}""")
            if scenario == "native-memory-container":
                # A direct FF device has a live word-address port.
                assert "REQ_VALID" not in info["ports"]
                assert info["activeAddress"] is not None
                assert f"ADDR {info['activeAddress']:08X}" in labels,(scenario,labels)
            else:
                style=page.evaluate("s=>CircuitMemoryView.transactionStyle(s)",info)
                assert style and style["mode"] in labels,(scenario,info,labels)
                if info["windowActive"]:
                    assert f"ADDR {info['activeAddress']:08X}" in labels,(scenario,labels)
            if screens: page.screenshot(path=str(screens/f"{scenario}-compact.png"))
        assert not errors,(scenario,errors)
        assert not api,(scenario,api)
        context.close()
        print(f"{scenario}: documentation, WASM, controls and inspector passed",flush=True)
    browser.close()
