"""Rendered memory features, not just snapshot parity. One WASM at a time."""
from pathlib import Path
import shutil
import sys
from urllib.parse import urljoin
from playwright.sync_api import sync_playwright

BASE=(sys.argv[1] if len(sys.argv)>1 else "http://127.0.0.1:8893/").rstrip("/")+"/"
SCREENS=Path(sys.argv[2]) if len(sys.argv)>2 else None
if SCREENS: SCREENS.mkdir(parents=True,exist_ok=True)
SETTLED="!app.cameraAnimation && !app.geometryDirty && !app.topologyRequestInFlight && !app.pendingScopeIds.size"
CAPTURE="""id=>{
 const labels=[],fills=[],original=ctx.fillText,rect=drawScreenRoundedRect;
 ctx.fillText=function(text,...args){labels.push({text,color:ctx.fillStyle});return original.call(this,text,...args)};
 drawScreenRoundedRect=function(r,fill,...args){fills.push(fill);return rect(r,fill,...args)};
 try{const c=app.components.get(id);drawMemoryContents(c,componentDrawParts(c));}
 finally{ctx.fillText=original;drawScreenRoundedRect=rect;}
 return {labels,activeRows:fills.filter(f=>f==='rgba(21, 42, 64, 0.78)').length};
}"""

with sync_playwright() as p:
    browser=p.chromium.launch(executable_path=shutil.which("google-chrome") or shutil.which("chromium"),
        headless=True,args=["--no-sandbox"])
    for scenario in ("rv32i-program3","rv32i-five-stage-program3","rv32i-baseline-program3"):
        context=browser.new_context(viewport={"width":1440,"height":1000})
        page=context.new_page(); page.set_default_timeout(90000)
        errors=[]
        page.on("pageerror",lambda e:errors.append(str(e)))
        page.goto(urljoin(BASE,f"simulator/?scenario={scenario}&core_fidelity=behavioral"),wait_until="domcontentloaded")
        page.wait_for_function("['ready','error'].includes(document.body.dataset.circuitsimStatus)")
        assert page.locator("body").get_attribute("data-circuitsim-status")=="ready",page.locator("#loading").inner_text()
        cases=page.evaluate("""async()=>{
          const found=new Map();
          for(let i=0;i<app.timestamps.length&&found.size<3;i++){
            await setStateIndex(i);
            for(const name of ['INSTRUCTION_MEMORY','DATA_MEMORY']){
              const state=app.componentStates.get(app.rootId+'.'+name),op=state?.transaction?.operation;
              const key=name+':'+op;
              if(state?.windowActive && state.activeAddress>0 && ['READ','WRITE'].includes(op) && !found.has(key))
                found.set(key,{id:app.rootId+'.'+name,index:i,op,address:state.activeBaseAddress});
            }
          }
          return [...found.values()];
        }""")
        assert len(cases)==3,cases
        for case in cases:
            page.evaluate("id=>smoothlyFocusComponent(id)",case["id"])
            page.wait_for_function(SETTLED)
            page.evaluate("async c=>{await setMemoryPage(c.id,null);await setStateIndex(c.index);}",case)
            capture=page.evaluate(CAPTURE,case["id"])
            assert capture["activeRows"]==1,(scenario,case,capture)
            colors=("rgb(89, 214, 141)","#59d68d") if case["op"]=="READ" else ("rgb(255, 184, 77)","#ffb84d")
            assert any(x["text"].startswith(case["op"]+" ") and x["color"] in colors for x in capture["labels"]),capture
            assert any(x["text"]==f"ADDR {case['address']:08X}" for x in capture["labels"]),capture
            if case["id"].endswith("INSTRUCTION_MEMORY"):
                assert any("addi" in x["text"] or "lb " in x["text"] for x in capture["labels"]),capture
                # Exercise the real row-scroll button and the Follow control.
                control=page.evaluate("id=>app.memoryScrollControls.find(c=>c.componentId===id&&c.direction==='down'&&c.enabled)",case["id"])
                assert control
                canvas=page.locator("canvas").bounding_box()
                page.mouse.click(canvas["x"]+control["rect"]["x"]+control["rect"]["w"]/2,
                                 canvas["y"]+control["rect"]["y"]+control["rect"]["h"]/2)
                assert page.evaluate("id=>app.memoryFollowing.get(id)",case["id"]) is False
                page.locator("#memory-follow").click()
                page.wait_for_function("!app.stateRequestInFlight")
                assert page.locator("#memory-follow").get_attribute("aria-pressed")=="true"
                page.locator("#memory-address").fill("0x640")
                page.locator("#memory-address").press("Enter")
                page.wait_for_function("id=>app.componentStates.get(id)?.pageAddress===1600",arg=case["id"])
                rows=page.evaluate("id=>memoryRowsForDisplay(app.componentStates.get(id)).map(r=>r.address)",case["id"])
                assert rows[0]==1600 and rows[-1]>1600,rows
                page.locator("#memory-page-next").click()
                page.wait_for_function("id=>app.componentStates.get(id)?.pageAddress===1856",arg=case["id"])
                page.locator("#memory-page-prev").click()
                page.wait_for_function("id=>app.componentStates.get(id)?.pageAddress===1600",arg=case["id"])
                page.locator("#memory-follow").click()
                page.wait_for_function("!app.stateRequestInFlight")
            if SCREENS: page.screenshot(path=str(SCREENS/f"{scenario}-{case['id'].split('.')[-1]}-{case['op']}.png"))
        rewind=page.evaluate("""async()=>{
          const id=app.rootId+'.DATA_MEMORY',values=[];
          for(const i of [0,app.timestamps.length-1,0]){await setStateIndex(i);values.push(app.componentStates.get(id).touchedWords);}
          return {same:JSON.stringify(values[0])===JSON.stringify(values[2]),changed:JSON.stringify(values[0])!==JSON.stringify(values[1])};
        }""")
        assert rewind=={"same":True,"changed":True},rewind
        if scenario=="rv32i-program3":
            cid="RV32I_PROGRAM_ROOT.INSTRUCTION_MEMORY"
            page.evaluate("id=>setSelectedComponent(id)",cid)
            page.locator(".explorer-row.selected .explorer-arrow").click()
            page.locator("#apply-profile").click()
            page.wait_for_function("id=>app.components.get(id)?.fidelity==='structural'&&document.querySelector('#loading').classList.contains('hidden')",arg=cid)
            page.evaluate("id=>smoothlyFocusComponent(id+'.MEMORY')",cid)
            page.wait_for_function(SETTLED)
            page.evaluate("()=>setStateIndex(0)")
            state=page.evaluate("id=>app.componentStates.get(id+'.MEMORY')",cid)
            assert state["memoryRole"]=="instruction" and state["transaction"]["endpointId"]==cid,state
            assert any(w.get("instructionText") for w in state["touchedWords"])
            if SCREENS: page.screenshot(path=str(SCREENS/"expanded-instruction-memory.png"))
        assert not errors,errors
        context.close()
        print(scenario+": active instruction/load/store rows, operation colors, paging, scrolling and rewind passed",flush=True)
    browser.close()
