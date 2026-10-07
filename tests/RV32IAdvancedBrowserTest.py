"""Check live pages and one WASM canvas at a time, including dark theme."""
from pathlib import Path
import shutil
import sys
from urllib.parse import urljoin
from playwright.sync_api import sync_playwright

BASE=(sys.argv[1] if len(sys.argv)>1 else "http://127.0.0.1:8893/").rstrip("/")+"/"
screens=Path(sys.argv[2]) if len(sys.argv)>2 else None
if screens: screens.mkdir(parents=True,exist_ok=True)
SETTLED="!app.cameraAnimation && !app.geometryDirty && !app.topologyRequestInFlight && !app.pendingScopeIds.size"
ORDER=["FETCH","IF_ID","DECODE","ID_EX","EXECUTE","EX_MEM","MEMORY","MEM_WB","WRITEBACK"]
with sync_playwright() as p:
    browser=p.chromium.launch(executable_path=shutil.which("google-chrome") or shutil.which("chromium"),headless=True,args=["--no-sandbox"])
    context=browser.new_context(viewport={"width":1440,"height":1000})
    context.route("**/simulator/**",lambda r:r.abort())
    page=context.new_page()
    for slug,count in [("",0),("forwarding/",2),("hazards/",1),("prediction/",3),("recovery/",2),("memory-waits/",1),("core/",1)]:
        response=page.goto(urljoin(BASE,"rv32i/4.6/"+slug),wait_until="domcontentloaded")
        assert response.ok,(slug,response.status)
        assert page.locator(".md-content h1").inner_text().startswith("§ 4.6")
        assert page.locator(".example-table-scroll").count()==count,slug
        assert page.locator(".simulator-embed iframe").count()==count,slug
        assert "--8<--" not in page.locator(".md-content").inner_text(),slug
        if not slug:
            for theme in ["default","slate"]:
                page.locator("body").evaluate("(e,t)=>e.setAttribute('data-md-color-scheme',t)",theme)
                figure=page.locator('[data-diagram="advanced-prediction"]')
                assert figure.count()==1
                if screens: figure.screenshot(path=str(screens/f"advanced-diagram-{theme}.png"))
        print(f"4.6/{slug}: navigation, examples and embeds checked",flush=True)
    context.close()
    for scenario in ["rv32i-branch-predictor","rv32i-prediction-path","rv32i-predict-two-bit-program20"]:
        context=browser.new_context(viewport={"width":1440,"height":1000})
        page=context.new_page();page.set_default_timeout(90000)
        errors=[];api=[]
        page.on("pageerror",lambda e:errors.append(str(e)))
        page.on("request",lambda r:api.append(r.url) if "/api/" in r.url else None)
        page.goto(urljoin(BASE,"simulator/?scenario="+scenario),wait_until="domcontentloaded")
        page.locator("body[data-circuitsim-status='ready']").wait_for()
        assert page.locator("body").get_attribute("data-circuitsim-runtime")=="wasm"
        root=page.evaluate("app.rootId")
        core=root+".CORE" if scenario.endswith("program20") else root
        page.evaluate("id=>smoothlyFocusComponent(id)",core);page.wait_for_function(SETTLED)
        def children(component):
            return page.evaluate("""id=>Object.fromEntries(app.components.get(id).childIds.map(k=>app.components.get(k)).map(c=>[c.name,c.rect]))""",component)
        rects=children(core)
        if scenario.endswith("program20"):
            assert all(rects[a]["x"]+rects[a]["w"]<rects[b]["x"] for a,b in zip(ORDER,ORDER[1:]))
            assert max(rects[n]["y"]+rects[n]["h"] for n in ORDER)<rects["PREDICTION"]["y"]
            assert not {"DATA_ISSUE", "FETCH_STOPPED", "FETCH_DISCARD", "FETCH_CONSUME"} & rects.keys()
            stage_height=max(rects[n]["h"] for n in ORDER[::2])
            assert all(rects[n]["h"]>=.8*stage_height for n in ORDER[1::2])
            assert all(abs(rects[n]["h"]/rects[n]["w"]-2.6)<.001 for n in ORDER[1::2])
        if screens: page.screenshot(path=str(screens/(scenario+".png")))
        scopes=[core]
        if scenario.endswith("program20"): scopes += [core+".COORDINATOR",core+".IF_ID",core+".ID_EX",core+".EX_MEM",core+".MEM_WB",core+".EXECUTE",core+".PREDICTION",core+".PREDICTION.PREDICTOR.TABLE"]
        elif scenario=="rv32i-branch-predictor": scopes += [core+".TABLE"]
        for scope in scopes:
            page.evaluate("id=>smoothlyFocusComponent(id)",scope);page.wait_for_function(SETTLED)
            boxes=children(scope)
            if scope.endswith("COORDINATOR"):
                assert {"DATA_ISSUE", "FETCH_STOPPED", "FETCH_DISCARD", "FETCH_CONSUME"} <= boxes.keys()
            for a,r in boxes.items():
                for b,s in boxes.items():
                    if a>=b: continue
                    assert not(r["x"]<s["x"]+s["w"]-.01 and s["x"]<r["x"]+r["w"]-.01 and r["y"]<s["y"]+s["h"]-.01 and s["y"]<r["y"]+r["h"]-.01),(scope,a,b)
            if scope.endswith("TABLE"):
                rows=[boxes[n] for n in sorted((n for n in boxes if n.startswith("ROW_") and not n.startswith("ROW_BITS_")),key=lambda n:int(n[4:]))]
                assert all(a["y"]<b["y"] for a,b in zip(rows,rows[1:])),scope
            if scope.endswith("EXECUTE"):
                assert {"FORWARDED_OPERANDS","DATAPATH","NEXT_PC","PACKET_STATUS"}<=boxes.keys()
                assert "ALU" not in boxes and "CONTROL_FLOW" not in boxes
            if screens: page.screenshot(path=str(screens/(scenario+"-"+scope.split(".")[-1]+".png")))
        assert not errors,errors
        assert not api,api
        print(f"{scenario}: WASM startup, focus, groups and non-overlap checked",flush=True)
        context.close()
    browser.close()
