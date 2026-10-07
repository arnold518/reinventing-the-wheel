"""Focused static/WASM checks. One browser session at a time to bound memory."""
from pathlib import Path
import shutil
import sys
from urllib.parse import urljoin
from playwright.sync_api import sync_playwright

ROOT = Path(__file__).resolve().parents[2]
base = sys.argv[1] if len(sys.argv) > 1 else "http://127.0.0.1:8893/"
screens = Path(sys.argv[2]) if len(sys.argv) > 2 else None
if screens:
    screens.mkdir(parents=True, exist_ok=True)
settled = "!app.cameraAnimation && !app.geometryDirty && !app.topologyRequestInFlight && !app.pendingScopeIds.size"


def focus(page, component):
    page.evaluate("id=>smoothlyFocusComponent(id)", component)
    page.wait_for_function(settled)


def geometry(page, component):
    focus(page, component)
    data = page.evaluate("""id=>{const c=app.components.get(id);return {
        parent:c.rect,children:c.childIds.map(id=>app.components.get(id)).map(c=>({name:c.name,...c.rect}))};}""", component)
    p, children = data["parent"], data["children"]
    for c in children:
        assert c["x"] >= p["x"] and c["y"] >= p["y"], (component,c)
        assert c["x"]+c["w"] <= p["x"]+p["w"]+.01 and c["y"]+c["h"] <= p["y"]+p["h"]+.01, (component,c)
    for i,a in enumerate(children):
        for b in children[i+1:]:
            dx=min(a["x"]+a["w"],b["x"]+b["w"])-max(a["x"],b["x"])
            dy=min(a["y"]+a["h"],b["y"]+b["h"])-max(a["y"],b["y"])
            assert dx <= .01 or dy <= .01, (component,a["name"],b["name"])
    return {c["name"]:c for c in children}


with sync_playwright() as p:
    browser = p.chromium.launch(executable_path=shutil.which("google-chrome") or shutil.which("chromium"),
                               headless=True,args=["--no-sandbox"])
    # Verify rendered reference table independently of loading a simulator.
    context=browser.new_context(java_script_enabled=False,viewport={"width":1440,"height":1000})
    page=context.new_page()
    assert page.goto(urljoin(base,"rv32i/4.3/"),wait_until="domcontentloaded").ok
    assert "4.3. RV32I Building Blocks" in page.locator("h1").inner_text()
    assert page.goto(urljoin(base,"rv32i/4.3/register-file/"),wait_until="domcontentloaded").ok
    assert "4.3.2. Register File" in page.locator("h1").inner_text()
    assert page.locator('.md-nav--primary a.md-nav__link').filter(has_text="4.3.2. Register File").count()==1
    assert page.locator('.md-nav--primary a.md-nav__link').filter(has_text="3.4. Register File").count()==0
    simulator_url=page.locator(".simulator-embed iframe").evaluate("iframe=>iframe.src")
    assert simulator_url==urljoin(base,"simulator/?scenario=rv32i-register-file&embed=1")
    rows=page.locator('.example-table-block[data-scenario="rv32i-register-file"] tbody tr').evaluate_all(
        "rows=>rows.map(r=>({label:r.dataset.checkpoint,time:Number(r.dataset.time),index:Number(r.dataset.timelineIndex)}))")
    assert rows and page.locator(".simulator-embed iframe").count()==1
    for width in (1440,640,360):
        page.set_viewport_size({"width":width,"height":1000})
        assert page.evaluate("document.documentElement.scrollWidth<=innerWidth+1"),width
    context.close()
    for scenario,behavioral in (("register32",False),("rv32i-register-file",False),("flip-flop-cell",True),("sram-cell",True),
                               ("dram-cell",False),("rom-cell",True),("nor-flash-cell",False)):
        context=browser.new_context(viewport={"width":1600,"height":1000})
        page=context.new_page();page.set_default_timeout(90000)
        errors=[]
        page.on("pageerror",lambda error:errors.append(str(error)))
        assert page.goto(urljoin(base,f"simulator/?scenario={scenario}"),wait_until="domcontentloaded").ok
        page.locator("body[data-circuitsim-status='ready']").wait_for()
        assert page.locator("body").get_attribute("data-circuitsim-runtime")=="wasm"
        root=page.evaluate("app.rootId")
        if behavioral:
            page.evaluate("""async()=>{await applyExplorerProfile({revision:app.profile.revision,
                exactOverrides:{[app.rootId]:'behavioral'},pendingFocusId:app.rootId});}""")
            assert page.evaluate("app.components.get(app.rootId).fidelity")=="behavioral"
        focus(page,root)
        if scenario=="register32":
            word=geometry(page,root)
            assert set(word)=={f"BIT_{bit}" for bit in range(32)} | {"INPUT_BITS","OUTPUT_BITS"}
            for row in range(4):
                for bit in range(row*8+1,row*8+8):
                    assert word[f"BIT_{bit}"]["x"]>word[f"BIT_{bit-1}"]["x"]
                    assert abs(word[f"BIT_{bit}"]["y"]-word[f"BIT_{bit-1}"]["y"])<.01
            if screens: page.screenshot(path=str(screens/"register32.png"))
            print("register32: four numeric rows of eight FF cells passed",flush=True)
        elif scenario=="rv32i-register-file":
            assert page.evaluate("app.checkpoints.map(c=>({label:c.label,time:c.time,index:c.index}))")==rows
            top=geometry(page,root)
            assert set(top)=={"RD_DECODER","STORAGE","X0_ZERO","RS1_MUX","RS2_MUX"}
            assert top["RD_DECODER"]["x"] < top["STORAGE"]["x"] < top["RS1_MUX"]["x"] < top["RS2_MUX"]["x"]
            assert top["STORAGE"]["w"] / page.evaluate("app.components.get(app.rootId).rect.w") >= .25
            if screens: page.screenshot(path=str(screens/"register-file.png"))
            bank=geometry(page,root+".STORAGE")
            assert len(bank)==31
            for i in range(1,31):
                if i%4:
                    assert bank[f"WORD_{i}"]["x"]>bank[f"WORD_{i-1}"]["x"]
                    assert abs(bank[f"WORD_{i}"]["y"]-bank[f"WORD_{i-1}"]["y"])<.01
                else: assert bank[f"WORD_{i}"]["y"]>bank[f"WORD_{i-1}"]["y"]
            if screens: page.screenshot(path=str(screens/"register-bank.png"))
            word=geometry(page,root+".STORAGE.WORD_0")
            assert set(word)=={f"BIT_{bit}" for bit in range(32)} | {"INPUT_BITS","OUTPUT_BITS"}
            if screens: page.screenshot(path=str(screens/"register-word.png"))
            print(f"register file: {len(rows)} documented checkpoints, bank and word geometry passed",flush=True)
        else:
            # Intercept actual canvas labels while rendering the cell at each
            # recorded checkpoint, in reverse order to exercise time travel.
            checks=page.evaluate("""async()=>{
                const c=app.components.get(app.rootId), checks=[];
                for(const checkpoint of [...app.checkpoints].reverse()) {
                    await setStateIndex(checkpoint.index);
                    const pins=Object.fromEntries([...c.inputPins,...c.outputPins].map(p=>[p.name,pinValue(p)]));
                    const model=CircuitMemoryCellView.model(c.type,c.fidelity,pins);
                    const text=[],original=ctx.fillText;
                    ctx.fillText=function(value,...args){text.push(String(value));return original.call(this,value,...args);};
                    try { drawMemoryCellValue(c, componentDrawParts(c)); } finally {ctx.fillText=original;}
                    checks.push({model,text,pins});
                }
                return checks;
            }""")
            assert checks
            for check in checks:
                for field in check["model"]["fields"]:
                    assert field["value"]==check["pins"][field["pin"]]
                    assert field["label"] in check["text"] and field["value"] in check["text"],(scenario,check)
                for control in check["model"]["controls"]:
                    assert any(f'{control["pin"]}={control["value"]}' in text for text in check["text"])
            if scenario=="dram-cell":
                assert any(c["pins"]["Q"]!=c["pins"]["SAMPLE"] for c in checks)
            if screens: page.screenshot(path=str(screens/f"{scenario}.png"))
            print(f"{scenario}: actual canvas values match pins at {len(checks)} checkpoints",flush=True)
        assert not errors,errors
        context.close()
    browser.close()
