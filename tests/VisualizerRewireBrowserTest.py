"""Check actual Rewire drawing, geometry, and saved-layout upgrades in Chrome.

Run after tools/build-pages.sh; optional second argument saves screenshots.
"""
from pathlib import Path
import shutil
import sys
from playwright.sync_api import sync_playwright


base = sys.argv[1].rstrip("/") + "/"
screenshots = Path(sys.argv[2]) if len(sys.argv) > 2 else None
if screenshots:
    screenshots.mkdir(parents=True, exist_ok=True)


def idle(page):
    page.wait_for_function("!app.cameraAnimation && !app.geometryDirty && !app.topologyRequestInFlight && !app.pendingScopeIds.size")


with sync_playwright() as playwright:
    browser = playwright.chromium.launch(
        executable_path=shutil.which("google-chrome") or shutil.which("chromium"),
        headless=True, args=["--no-sandbox"])
    context = browser.new_context(viewport={"width": 1440, "height": 1000})
    page = context.new_page()
    errors, api_requests = [], []
    page.on("pageerror", lambda error: errors.append(str(error)))
    page.on("request", lambda request: api_requests.append(request.url) if "/api/" in request.url else None)
    for scenario, suffix in (("ff-memory-container", ".READ_SELECT.WORD_0_BITS"),
                             ("sram-container", ".READ_SELECT.WORD_0_BITS"),
                             ("rewire", "")):
        print(f"Checking {scenario} unpacker", flush=True)
        page.goto(base + "?scenario=" + scenario, wait_until="domcontentloaded")
        page.locator("body[data-circuitsim-status='ready']").wait_for(timeout=60_000)
        assert page.locator("body").get_attribute("data-circuitsim-runtime") == "wasm"
        target = page.evaluate("suffix => app.rootId + suffix", suffix)
        page.evaluate("id => smoothlyFocusComponent(id)", target)
        idle(page)
        page.evaluate("async () => { await setStateIndex(app.checkpoints[0].index); }")
        idle(page)
        drawing = page.evaluate("""id => {
            const component = app.components.get(id);
            const model = CircuitRewireGeometry.unpackedBusModel(component.rewire);
            const paths = [], labels = [];
            const originalLine = drawAdapterPolyline, originalDot = drawAdapterJunction;
            const originalText = ctx.fillText;
            let dots = 0;
            try {
                drawAdapterPolyline = (c, points, value, pins, width, ...rest) => {
                    if (c.id === id) paths.push({points, value, width});
                    return originalLine(c, points, value, pins, width, ...rest);
                };
                drawAdapterJunction = (...args) => { ++dots; return originalDot(...args); };
                ctx.fillText = function(text, ...args) { labels.push(text); return originalText.call(this, text, ...args); };
                drawRewireContents(component, componentDrawParts(component));
            } finally {
                drawAdapterPolyline = originalLine;
                drawAdapterJunction = originalDot;
                ctx.fillText = originalText;
                requestRender();
            }
            const source = component.inputPins.find(pin => pin.name === model.source);
            const center = worldToScreen(viewportCenter(componentFocusRect(component)));
            const safeCenter = viewportCenter(cameraSafeViewport());
            return {model, paths, labels, dots, value: pinValue(source),
                type: component.type, layoutType: component.layoutType,
                aspect: component.rect.h / component.rect.w,
                errorX: center.x - safeCenter.x, errorY: center.y - safeCenter.y};
        }""", target)
        assert drawing["type"] == "Rewire" and drawing["layoutType"] == "Rewire<in=1,out=8>"
        assert abs(drawing["aspect"] - 1.185) < 0.001, drawing
        assert abs(drawing["errorX"]) < 2 and abs(drawing["errorY"]) < 2
        assert drawing["dots"] == 0 and len(drawing["paths"]) == 10, drawing
        assert [path["width"] for path in drawing["paths"]] == [8, 8] + [1] * 8
        assert drawing["labels"] == [f"[{bit}]" for bit in range(8)], drawing
        assert [path["value"] for path in drawing["paths"][2:]] == list(reversed(drawing["value"]))
        if screenshots:
            page.screenshot(path=str(screenshots / f"{scenario}-unpack.png"))

        if scenario == "ff-memory-container":
            # Only this isolated browser context is changed. Old shared/profile
            # dimensions must not return after saving and reloading a layout.
            awaitable = """async id => {
                const component = app.components.get(id);
                const key = component.layoutType, fingerprint = component.profileFingerprint;
                app.layout.type_layouts.Rewire = {aspect_ratio: 9, min_aspect_ratio: 9, color: [10,20,30,190]};
                delete app.layout.type_layouts[key];
                const scoped = fingerprint && fingerprint !== 'explicit';
                if (scoped) {
                    app.layout.profile_layouts[fingerprint] ||= {};
                    app.layout.profile_layouts[fingerprint].Rewire = {aspect_ratio: 10, min_aspect_ratio: 10, color: [30,40,50,190]};
                    delete app.layout.profile_layouts[fingerprint][key];
                }
                await saveLayout();
                return Boolean(scoped);
            }"""
            profile_scoped = page.evaluate(awaitable, target)
            page.reload(wait_until="domcontentloaded")
            page.locator("body[data-circuitsim-status='ready']").wait_for(timeout=60_000)
            restored = page.evaluate("""id => {
                const component = app.components.get(id);
                return {aspect: component.aspectRatio, color: getTypeLayoutFor(component).color,
                    legacyAspect: app.layout.type_layouts.Rewire.aspect_ratio};
            }""", target)
            assert abs(restored["aspect"] - 1.185) < 0.001, restored
            assert restored["color"] == ([30, 40, 50, 190] if profile_scoped else [10, 20, 30, 190]), restored
            assert restored["legacyAspect"] == 9, "Retain saved records; don't delete them to repair geometry"
        print(f"{scenario}: taps, bit labels/colors, proportions and centering passed", flush=True)

    # Equal input/output labels must not collapse to a single browser pin.
    page.goto(base + "?scenario=rv32i-memory-lanes", wait_until="domcontentloaded")
    page.locator("body[data-circuitsim-status='ready']").wait_for(timeout=60_000)
    result = page.evaluate("""async () => {
        const failures=[];
        for(const row of app.checkpoints) {
            await setStateIndex(row.index);
            for(let lane=0;lane<4;lane++) {
                const c=app.components.get(`${app.rootId}.LANE_${lane}`);
                if(c.inputPins.length!==2 || c.outputPins.length!==2) throw Error('Missing lane pins');
                for(const [name,a,b] of [['LOAD',0x80ff3412,0x80ff3412 >>> (8*lane)],
                    ['STORE',0xdeadbeef,(0xdeadbeef << (8*lane)) >>> 0]]) {
                    const input=c.inputPins.find(p=>p.name===name), output=c.outputPins.find(p=>p.name===name);
                    if(input.id===output.id || input.stateIndex===output.stateIndex) throw Error('Colliding identity');
                    if(pinValue(input)!==a.toString(2).padStart(32,'0') || pinValue(output)!==b.toString(2).padStart(32,'0'))
                        failures.push([lane,name,row.label]);
                    if(!input.sinkWireIds.length || !output.sourceWireIds.length) throw Error('Missing wire');
                    for(const id of output.sourceWireIds)
                        if(app.wires.get(id).sourcePin!==output) throw Error('Wire attached to wrong side');
                }
            }
        }
        return {failures,rows:app.checkpoints.length};
    }""")
    assert not result["failures"], result
    for lane in range(4):
        target = f"RV32I_MEMORY_LANES_ROOT.LANE_{lane}"
        page.evaluate("id=>smoothlyFocusComponent(id)", target)
        idle(page)
        drawn = page.evaluate("""id=>{
            const c=app.components.get(id), paths=[],original=drawAdapterPolyline;
            try {
                drawAdapterPolyline=(comp,points,value,pins,width,...rest)=>{
                    if(comp.id===id) paths.push({points,width});
                    return original(comp,points,value,pins,width,...rest);};
                drawRewireContents(c,componentDrawParts(c));
            } finally {drawAdapterPolyline=original;requestRender();}
            return {paths, inputs:c.inputPins.map(p=>p.pos), outputs:c.outputPins.map(p=>p.pos)};
        }""", target)
        assert drawn["paths"], (target, "No internal Rewire drawing")
        assert min(p["x"] for p in drawn["outputs"]) > max(p["x"] for p in drawn["inputs"])
        if screenshots:
            page.screenshot(path=str(screenshots / f"rv32i-memory-lane-{lane}.png"))
    print(f'RV32I lanes: two inputs/two outputs, correct wire sides, drawing and {result["rows"]} checkpoints passed', flush=True)

    # A 32-bit instruction feeds eight fields, not eight overlapping spines.
    page.goto(base + "?scenario=rv32i-decode-control", wait_until="domcontentloaded")
    page.locator("body[data-circuitsim-status='ready']").wait_for(timeout=60_000)
    target = "RV32I_DECODE_CONTROL_ROOT.FIELDS_AND_IMMEDIATES"
    page.evaluate("id=>smoothlyFocusComponent(id)", target)
    idle(page)
    result = page.evaluate("""async id=>{
        const c=app.components.get(id), model=CircuitRewireGeometry.fieldExtractionModel(c.rewire);
        const source=c.inputPins.find(p=>p.name===model.source), outputs=pinMapByName(c.outputPins);
        const failures=[];
        for (const row of app.checkpoints) {
            await setStateIndex(row.index);
            for (const tap of model.taps) {
                const value=CircuitRewireGeometry.fieldValue(tap,pinValue(source),model.unmappedDefault);
                if(value!==pinValue(outputs.get(tap.destination))) failures.push([row.label,tap.destination,value]);
            }
        }
        await setStateIndex(app.checkpoints[4].index);
        return {failures,rows:app.checkpoints.length,model};
    }""", target)
    assert not result["failures"], result
    taps = {t["destination"]: t for t in result["model"]["taps"]}
    assert len(taps) == 8
    assert taps["RS1"]["label"] == "[19:15]"
    assert taps["IMM_I"]["label"] == "{20×[31], [31:20]}"
    assert taps["IMM_U"]["label"] == "{[31:12], 12×0}"
    assert taps["IMM_B"]["sourceBits"][0] is None
    assert taps["IMM_J"]["sourceBits"][0] is None
    drawing = page.evaluate("""id=>{
        const c=app.components.get(id),paths=[],labels=[];
        const originalLine=drawAdapterPolyline, originalDot=drawAdapterJunction, originalText=ctx.fillText;
        let dots=0;
        try {
            drawAdapterPolyline=(c,points,value,pins,width,annotate=true)=>{
                paths.push({points,value,width,annotate});return originalLine(c,points,value,pins,width,annotate);};
            drawAdapterJunction=(...args)=>{dots++;return originalDot(...args);};
            ctx.fillText=function(text,...args){labels.push(text);return originalText.call(this,text,...args);};
            drawRewireContents(c,componentDrawParts(c));
        } finally {drawAdapterPolyline=originalLine;drawAdapterJunction=originalDot;ctx.fillText=originalText;requestRender();}
        return {paths,labels,dots};
    }""", target)
    assert drawing["dots"] == 0 and len(drawing["paths"]) == 10, drawing
    assert len([p for p in drawing["paths"] if p["annotate"]]) == 1, "Duplicate bus slash annotations"
    assert sum(1 for p in drawing["paths"] for a,b in zip(p["points"],p["points"][1:])
               if a["x"]==b["x"] and a["y"]!=b["y"]) == 1, "Repeated vertical routes"
    assert [p["width"] for p in drawing["paths"][2:]] == [t["width"] for t in result["model"]["taps"]]
    for tap in taps.values():
        assert f'{tap["destination"]}[{tap["width"]}]' in drawing["labels"], drawing
    assert not any("…" in label for label in drawing["labels"]), "Focused field mappings should be legible in full"
    if screenshots: page.screenshot(path=str(screenshots / "rv32i-fields-and-immediates.png"))
    print(f'RV32I field extractor: one spine, eight labeled fields and {result["rows"]} WASM checkpoints passed', flush=True)

    # The pre-existing many-input control-bus packer must retain its own model.
    page.goto(base + "?scenario=rv32i-five-stage-program1", wait_until="domcontentloaded")
    page.locator("body[data-circuitsim-status='ready']").wait_for(timeout=60_000)
    target = page.evaluate("app.componentOrder.find(c => c.type === 'Rewire' && c.name === 'CONTROL_PACK')?.id")
    assert target, "Missing RV32I control packer"
    page.evaluate("id => smoothlyFocusComponent(id)", target)
    idle(page)
    assert page.evaluate("""id => {
        const c = app.components.get(id);
        return CircuitRewireGeometry.packedBusModel(c.rewire)?.destination === 'CONTROL'
            && CircuitRewireGeometry.unpackedBusModel(c.rewire) === null
            && c.layoutType !== 'Rewire<in=1,out=8>';
    }""", target)
    if screenshots:
        page.screenshot(path=str(screenshots / "rv32i-control-pack.png"))
    print("RV32I control packer passed", flush=True)
    assert not errors, errors
    assert not api_requests, api_requests
    browser.close()
