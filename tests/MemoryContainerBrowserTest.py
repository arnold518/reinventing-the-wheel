"""Exercise the new memory scenarios through the actual serverless visualizer."""
import shutil
import sys
from pathlib import Path
from urllib.parse import urljoin
from playwright.sync_api import sync_playwright


base = sys.argv[1].rstrip("/") + "/"
screenshots = Path(sys.argv[2]) if len(sys.argv) > 2 else None
if screenshots:
    screenshots.mkdir(parents=True, exist_ok=True)
with sync_playwright() as playwright:
    browser = playwright.chromium.launch(
        executable_path=shutil.which("google-chrome") or shutil.which("chromium"),
        headless=True, args=["--no-sandbox"])
    context = browser.new_context(viewport={"width": 1440, "height": 1000})
    page = context.new_page()
    page.set_default_timeout(30_000)
    errors, api_requests = [], []
    page.on("pageerror", lambda error: errors.append(str(error)))
    page.on("request", lambda request: api_requests.append(request.url) if "/api/" in request.url else None)
    for scenario, type_name in (("ff-memory-container", "FFMemory8"), ("sram-container", "SRAM8"),
                                ("rom-container", "ROM8"), ("memory-bus-adapter32", "MemoryBusAdapter32")):
        print(f"Loading {scenario}", flush=True)
        response = page.goto(urljoin(base, "?scenario=" + scenario), wait_until="domcontentloaded")
        assert response and response.ok
        try:
            page.wait_for_function("['ready', 'error'].includes(document.body.dataset.circuitsimStatus)", timeout=60_000)
            assert page.locator("body").get_attribute("data-circuitsim-status") == "ready", page.locator("#loading").inner_text()
        except Exception:
            print({"errors": errors, "loading": page.locator("#loading").inner_text()}, flush=True)
            raise
        assert page.locator("body").get_attribute("data-circuitsim-runtime") == "wasm"
        assert page.locator("#scenario-select").input_value() == scenario
        assert page.evaluate("app.components.get(app.rootId).type") == type_name
        assert page.evaluate("app.checkpoints.length") > 10
        if type_name == "FFMemory8":
            names = page.evaluate("""() => ['', '.STORAGE', '.STORAGE.WORD_0'].map(suffix => {
                const c = app.components.get(app.rootId + suffix);
                return [c.type, c.layoutType];
            })""")
            assert names == [["FFMemory8", "FFArray8"],
                             ["FFStorageBank<8,4>", "MemoryStorageBank<FFArray,8,4>"],
                             ["FFWord<8>", "MemoryWord<FFArray,8>"]], names
        if type_name != "MemoryBusAdapter32":
            hierarchy = page.evaluate("""() => {
                const root = app.components.get(app.rootId);
                const blocks = root.childIds.map(id => app.components.get(id));
                const storage = blocks.find(c => c.name === 'STORAGE');
                return {blocks: blocks.map(c => c.name).sort(),
                    words: storage?.childIds.map(id => {
                        const word = app.components.get(id);
                        return {name: word.name, cells: word.childIds.map(id => app.components.get(id))
                            .filter(c => c.name.startsWith('CELL_')).map(c => c.type)};
                    })};
            }""")
            assert hierarchy["blocks"] == ["ACCESS_CONTROL", "ADDRESS_DECODE", "READ_SELECT", "STORAGE"]
            assert [word["name"] for word in hierarchy["words"]] == [f"WORD_{n}" for n in range(4)]
            cell_type = {"FFMemory8": "FlipFlopCell", "SRAM8": "GatedDLatch", "ROM8": "ConstantValue"}[type_name]
            assert all(word["cells"] == [cell_type] * 8 for word in hierarchy["words"])
            page.evaluate("app.explorer.setOpen(true, {persist: false})")
            for suffix, label in (("", "array"), (".STORAGE", "storage"), (".STORAGE.WORD_0", "word")):
                page.evaluate("suffix => smoothlyFocusComponent(app.rootId + suffix)", suffix)
                page.wait_for_function("!app.cameraAnimation && !app.topologyRequestInFlight")
                assert page.evaluate("suffix => app.selectedComponentId === app.rootId + suffix", suffix)
                assert page.evaluate("""() => {
                    const component = app.components.get(app.selectedComponentId);
                    const center = worldToScreen(viewportCenter(componentFocusRect(component)));
                    const target = viewportCenter(cameraSafeViewport());
                    return Math.abs(center.x - target.x) < 2 && Math.abs(center.y - target.y) < 2;
                }"""), (scenario, label, "Focus missed the space between side panels")
                if screenshots:
                    page.screenshot(path=str(screenshots / f"{scenario}-{label}.png"))
            if type_name == "FFMemory8":
                # Exercise the real explorer controls at both new fidelity
                # boundaries, not only the root's existing switch.
                for suffix in (".STORAGE.WORD_0", ".STORAGE.WORD_1.CELL_0"):
                    page.evaluate("suffix => setSelectedComponent(app.rootId + suffix)", suffix)
                    row = page.locator(f'.explorer-row[title^="MEMORY_CONTAINER_ROOT{suffix}\\a "]')
                    assert row.locator(".explorer-arrow").is_enabled()
                    row.locator(".explorer-arrow").click()
                    page.locator("#apply-profile").click()
                    try:
                        page.wait_for_function("""suffix => {
                            const c = app.components.get(app.rootId + suffix);
                            return c?.fidelity === 'behavioral' && c.childIds.length === 0
                                && document.querySelector('#loading').classList.contains('hidden');
                        }""", arg=suffix)
                    except Exception:
                        print(page.evaluate("""suffix => ({suffix, errors: document.querySelector('#loading').textContent,
                            profile: app.explorer.profile, actualProfile: app.profile,
                            status: app.explorer.status.textContent, drafts: app.explorer.draftOverrides,
                            layoutDirty: app.layoutDirty, selected: app.selectedComponentId,
                            component: app.components.get(app.rootId + suffix)})""", suffix), flush=True)
                        raise
                    assert not page.evaluate("app.explorer.status.classList.contains('error')"), page.evaluate("app.explorer.status.textContent")
                    assert page.evaluate("app.explorer.profile.revision === app.profile.revision"), page.evaluate("({profile: app.profile, explorer: app.explorer.profile})")
                assert page.evaluate("app.components.get(app.rootId + '.STORAGE.WORD_1').fidelity") == "structural"
                # Collapsing only STORAGE preserves the device's decoder/control/read
                # blocks and restores the existing child choices when expanded.
                for fidelity in ("behavioral", "structural"):
                    page.evaluate("setSelectedComponent(app.rootId + '.STORAGE')")
                    page.locator('.explorer-row.selected .explorer-arrow').click()
                    page.locator('#apply-profile').click()
                    page.wait_for_function("""fidelity => {
                        const bank = app.components.get(app.rootId + '.STORAGE');
                        return bank?.fidelity === fidelity && bank.childIds.length === (fidelity === 'behavioral' ? 0 : 4)
                            && document.querySelector('#loading').classList.contains('hidden');
                    }""", arg=fidelity)
                    assert page.evaluate("app.components.get(app.rootId).childIds.length") == 4
                for suffix in (".STORAGE.WORD_0", ".STORAGE.WORD_1.CELL_0"):
                    assert page.evaluate("suffix => app.components.get(app.rootId + suffix).fidelity", suffix) == "behavioral"
            # Focusing a deep word scrolls the virtualized explorer. Return to
            # the root before using its fidelity switch, not the first cell row.
            page.evaluate("setSelectedComponent(app.rootId)")
            row = page.locator(".explorer-row").first
            page.wait_for_function("document.querySelector('.explorer-row .explorer-badge')?.textContent === 'S'")
            assert row.locator(".explorer-badge").inner_text() == "S"
            row.locator(".explorer-arrow").click()
            page.locator("#apply-profile").click()
            page.wait_for_function("app.components.size === 1 && app.components.get(app.rootId).fidelity === 'behavioral' && document.querySelector('#loading').classList.contains('hidden')")
            state = page.evaluate("""async () => {
                await setStateIndex(app.timestamps.length - 1);
                return app.componentStates.get(app.rootId);
            }""")
            assert state["supported"] and state["wordBits"] == 8 and state["capacityBytes"] == 4
            assert not state["touchedWordsTruncated"]
            assert "WE" in state["ports"] and "WRITE_MASK" in state["ports"]
            if type_name == "SRAM8":
                assert state["touchedWordCount"] == 4
            page.reload(wait_until="domcontentloaded")
            page.locator("body[data-circuitsim-status='ready']").wait_for(timeout=60_000)
            assert page.evaluate("app.components.get(app.rootId).fidelity") == "behavioral"
            if type_name == "FFMemory8":
                page.evaluate("app.explorer.setOpen(true, {persist: false}); setSelectedComponent(app.rootId)")
                page.locator(".explorer-row.selected .explorer-arrow").click()
                page.locator("#apply-profile").click()
                page.wait_for_function("app.components.get(app.rootId).fidelity === 'structural' && document.querySelector('#loading').classList.contains('hidden')")
                for suffix in (".STORAGE.WORD_0", ".STORAGE.WORD_1.CELL_0"):
                    assert page.evaluate("suffix => app.components.get(app.rootId + suffix).fidelity", suffix) == "behavioral"
        print(f"{scenario}: browser scenario, checkpoints and fidelity passed", flush=True)
    page.goto(urljoin(base, "?scenario=ff-word"), wait_until="domcontentloaded")
    page.locator("body[data-circuitsim-status='ready']").wait_for(timeout=60_000)
    assert page.evaluate("app.components.get(app.rootId).type") == "FFWord<16>"
    page.evaluate("app.explorer.setOpen(true, {persist: false}); setSelectedComponent(app.rootId)")
    page.locator(".explorer-row.selected .explorer-arrow").click()
    page.locator("#apply-profile").click()
    page.wait_for_function("app.components.size === 1 && app.components.get(app.rootId).fidelity === 'behavioral' && document.querySelector('#loading').classList.contains('hidden')")
    assert page.evaluate("app.checkpoints.length") > 10
    print("ff-word: standalone word fidelity passed", flush=True)
    assert not errors, errors
    assert not api_requests, api_requests
    browser.close()
