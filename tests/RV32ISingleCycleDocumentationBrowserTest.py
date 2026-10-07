"""Render 4.4 references and exercise each WASM embed in isolation."""
from pathlib import Path
import os
import json
import re
import shutil
import sys
from urllib.parse import urljoin
from playwright.sync_api import sync_playwright

ROOT = Path(__file__).resolve().parents[1]
base = (sys.argv[1] if len(sys.argv) > 1 else "http://127.0.0.1:8893/").rstrip("/") + "/"
exports = json.loads(Path(os.environ.get('CIRCUITSIM_EXAMPLE_ROWS', ROOT / "build-wasm/pages/simulator/example-rows.json")).read_text())["scenarios"]
scenarios = {}
with sync_playwright() as p:
    browser = p.chromium.launch(executable_path=shutil.which("google-chrome") or shutil.which("chromium"),
                                headless=True, args=["--no-sandbox"])
    context = browser.new_context(java_script_enabled=False, viewport={"width":1440,"height":1000})
    page = context.new_page()
    page.set_default_timeout(60000)
    assert page.goto(urljoin(base, "rv32i/4.4/"), wait_until="domcontentloaded").ok
    assert "No reading back" in page.locator("article").inner_text()
    for number, slug in enumerate(("fetch","decode","execute","memory","writeback","next-pc","core"), 1):
        assert page.goto(urljoin(base, f"rv32i/4.4/{slug}/"), wait_until="domcontentloaded").ok
        assert f"4.4.{number}." in page.locator("h1").inner_text()
        assert page.locator(".simulator-embed iframe").count() == 1
        if slug != "core":
            document = (ROOT / f"site-docs/rv32i/4.4/{slug}.md").read_text()
            alias = re.findall(r"<!-- simulator-examples: (\S+) \S+ -->", document)[0]
            rows = page.locator(f'.example-table-block[data-scenario="{alias}"] tbody tr').evaluate_all(
                "rows=>rows.map(r=>({label:r.dataset.checkpoint,time:Number(r.dataset.time),index:Number(r.dataset.timelineIndex)}))")
            assert len(rows) == len(exports[alias]["rows"])
            assert page.locator("iframe").evaluate("el=>el.src") == urljoin(base, f"simulator/?scenario={alias}&embed=1")
            scenarios[alias] = rows
        else:
            assert page.locator("iframe").evaluate("el=>el.src") == urljoin(base, "simulator/?scenario=rv32i-program3&core_fidelity=structural&embed=1")
        for width in (1440, 640, 360):
            page.set_viewport_size({"width":width,"height":1000})
            assert page.evaluate("document.documentElement.scrollWidth<=innerWidth+1"), (slug,width)
            for table in page.locator(".example-table-scroll").all():
                assert table.evaluate("el=>el.tabIndex===0 && el.clientWidth<=innerWidth")
        page.set_viewport_size({"width":1440,"height":1000})
        print(f"4.4.{number}: rendered pins, tables, embed URL and mobile widths passed", flush=True)
    context.close()

    for alias, rows in [*scenarios.items(), ("rv32i-program3", None)]:
        context = browser.new_context(viewport={"width":1440,"height":900})
        page = context.new_page(); page.set_default_timeout(90000)
        errors, api = [], []
        page.on("pageerror", lambda e: errors.append(str(e)))
        page.on("request", lambda r: api.append(r.url) if "/api/" in r.url else None)
        extra = "&core_fidelity=structural" if rows is None else ""
        assert page.goto(urljoin(base, f"simulator/?scenario={alias}{extra}&embed=1"), wait_until="domcontentloaded").ok
        page.wait_for_function("['ready','error'].includes(document.body.dataset.circuitsimStatus)")
        assert page.locator("body").get_attribute("data-circuitsim-status") == "ready", (alias,errors)
        assert page.locator("body").get_attribute("data-circuitsim-runtime") == "wasm"
        if rows is not None:
            assert page.evaluate("app.checkpoints.map(c=>({label:c.label,time:c.time,index:c.index}))") == rows
            failures = page.evaluate("""async rows=>{
                const root=app.components.get(app.rootId), failures=[];
                const pins=new Map([...root.inputPins,...root.outputPins].map(p=>[p.name,p]));
                for(const row of rows) {
                    await setStateIndex(row.timelineIndex);
                    for(const [name,value] of Object.entries({...row.inputs,...row.expectedOutputs}))
                        if(pinValue(pins.get(name))!==value) failures.push([row.checkpoint,name,value,pinValue(pins.get(name))]);
                }
                return failures;
            }""", exports[alias]["rows"])
            assert not failures, (alias,failures)
        else:
            assert page.evaluate("app.components.get('RV32I_PROGRAM_ROOT.CORE').fidelity") == "structural"
            names = page.evaluate("app.components.get('RV32I_PROGRAM_ROOT.CORE').childIds.map(id=>app.components.get(id).name).sort()")
            assert names == sorted(["FETCH", "DECODE", "EXECUTE", "MEMORY", "WRITEBACK",
                                    "NEXT_PC", "EXECUTION_STATUS", "INSTRUCTION_PORT", "DATA_PORT",
                                    "INSTRUCTION_NO_WRITE", "INSTRUCTION_WRITE_ZERO", "INSTRUCTION_MASK_ZERO",
                                    "STORE_AUTHORIZED"]), names
            assert page.evaluate("app.checkpoints.length") == 17
        assert not errors and not api, (alias,errors,api)
        print(f"{alias}: live WASM embed passed without backend API calls", flush=True)
        context.close()
    browser.close()
