"""Actual browser geometry, saved-layout upgrade, and SRAM floorplan images."""
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


def verify_matrix(page, bank_id):
    result = page.evaluate("""id => {
        const bank = app.components.get(id), children = bank.childIds.map(id => app.components.get(id));
        const rows = children.filter(c => /^ROW_/.test(c.name)).sort((a,b) => Number(a.name.slice(4))-Number(b.name.slice(4)));
        const pairs = children.filter(c => /^BITLINE_/.test(c.name)).sort((a,b) => Number(a.name.slice(8))-Number(b.name.slice(8)));
        const cells = rows.map(row => row.childIds.map(id => app.components.get(id)).sort((a,b) => Number(a.name.slice(5))-Number(b.name.slice(5))));
        return {rows:rows.map(c=>c.rect), pairs:pairs.map(c=>c.rect), cells:cells.map(row=>row.map(c=>c.rect)), aspect:bank.aspectRatio};
    }""", bank_id)
    assert len(result["rows"]) == 2 and len(result["pairs"]) == 16, result
    assert result["aspect"] < 0.5
    assert result["rows"][0]["y"] < result["rows"][1]["y"]
    for cells in result["cells"]:
        assert len(cells) == 16 and max(c["y"] for c in cells) - min(c["y"] for c in cells) < 1e-6
        assert all(a["x"] + a["w"] < b["x"] for a, b in zip(cells, cells[1:]))
        for cell, pair in zip(cells, result["pairs"]):
            assert abs(cell["x"] + cell["w"]/2 - pair["x"] - pair["w"]/2) < 1e-6
    for row in result["rows"]:
        assert row["y"] + row["h"] < result["pairs"][0]["y"]


with sync_playwright() as p:
    browser = p.chromium.launch(executable_path=shutil.which("google-chrome") or shutil.which("chromium"), headless=True, args=["--no-sandbox"])
    context = browser.new_context(viewport={"width":1600,"height":1000})
    page = context.new_page()
    errors = []
    page.on("pageerror", lambda error: errors.append(str(error)))
    for scenario in ("sram-row", "sram-storage-bank", "sram"):
        page.goto(base + "?scenario=" + scenario, wait_until="domcontentloaded")
        page.locator("body[data-circuitsim-status='ready']").wait_for(timeout=90_000)
        page.evaluate("() => {app.explorer.setOpen(false,{persist:false}); smoothlyFocusComponent(app.rootId);}")
        idle(page)
        if scenario == "sram-row":
            assert page.evaluate("() => {const [a,b]=app.components.get(app.rootId).childIds.map(id=>app.components.get(id).rect); return Math.abs(a.y-b.y)<1e-6 && a.x+a.w<b.x;}")
        else:
            bank_id = page.evaluate("app.rootId") + (".STORAGE" if scenario == "sram" else "")
            verify_matrix(page, bank_id)
        if screenshots:
            page.screenshot(path=str(screenshots / f"{scenario}.png"))
        if scenario == "sram":
            page.evaluate("id => smoothlyFocusComponent(id)", bank_id)
            idle(page)
            if screenshots:
                page.screenshot(path=str(screenshots / "sram-bank-focus.png"))
        print(f"{scenario}: ordered rows/columns passed", flush=True)

    # Recreate a browser's pre-update snapshot from the retained default geometry.
    page.evaluate("""async () => {
        const old = cloneJson(app.layout);
        for (const store of [old.type_layouts,old.root_layouts,...Object.values(old.profile_layouts)]) {
            for (const [key,entry] of Object.entries(store)) if (entry.layout_revision === 'sram-matrix-v1')
                store[key] = {...cloneJson(entry.previous_default), ...(entry.color ? {color:entry.color} : {})};
        }
        old.type_layouts['SRAMRow<16>'].color = [70,150,120,190];
        old.type_layouts.ALU8.color = [11,22,33,190];
        app.layout = old;
        await saveLayout();
    }""")
    page.reload(wait_until="domcontentloaded")
    page.locator("body[data-circuitsim-status='ready']").wait_for(timeout=90_000)
    idle(page)
    verify_matrix(page, "SRAM_ROOT.STORAGE")
    assert page.evaluate("app.layout.type_layouts['SRAMRow<16>'].color") == [70,150,120,190]
    assert page.evaluate("app.layout.type_layouts.ALU8.color") == [11,22,33,190]
    # A later explicit placement must survive reload, rather than reflowing again.
    page.evaluate("""async () => {
        const row = app.components.get('SRAM_ROOT.STORAGE.ROW_0');
        const cell = app.components.get(row.childIds[0]);
        childEntryFor(row,cell).rel_pos = [0.123,0.456];
        await saveLayout();
    }""")
    page.reload(wait_until="domcontentloaded")
    page.locator("body[data-circuitsim-status='ready']").wait_for(timeout=90_000)
    assert page.evaluate("getChildLayout(app.components.get('SRAM_ROOT.STORAGE.ROW_0'),app.components.get('SRAM_ROOT.STORAGE.ROW_0.CELL_0')).rel_pos") == [0.123,0.456]
    assert not errors, errors
    browser.close()
print("SRAM matrix layouts, legacy upgrades, custom positions and colors passed")
