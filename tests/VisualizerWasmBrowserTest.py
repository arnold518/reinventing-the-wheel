"""End-to-end browser contract for the packaged static WASM visualizer."""

from __future__ import annotations

import json
import shutil
import sys
from time import perf_counter
from urllib.parse import urljoin

from playwright.sync_api import sync_playwright


def check_component_documentation_pattern(page, components: tuple[tuple[str, bool], ...]) -> None:
    component_count = len(components)
    overview = page.locator("article table").first
    assert overview.locator("thead th").all_text_contents() == ["Component", "Description", "Structural", "Behavioral"]
    availability = overview.locator("tbody tr").evaluate_all(
        "rows => rows.map(row => [0, 2, 3].map(i => row.cells[i].textContent.trim()))")
    assert availability == [[name, "Yes", "Yes" if behavioral else "No"]
                            for name, behavioral in components], availability
    headings = page.locator("article h3").evaluate_all(
        "nodes => nodes.map(node => node.childNodes[0].textContent.trim())")
    assert headings == ["IO pins", "Behavior", "Implementation", "Examples", "Source and tests"] * component_count
    subheadings = page.locator("article h4").evaluate_all("""nodes => nodes.map(node => {
      let parent = node.previousElementSibling;
      while (parent && parent.tagName !== 'H3') parent = parent.previousElementSibling;
      return [node.childNodes[0].textContent.trim(), parent?.childNodes[0].textContent.trim()];
    })""")
    for title, parent in (("C++ example", "Behavior"), ("Unknown inputs and timing", "Behavior"),
                          ("Structural implementation", "Implementation"), ("Behavioral implementation", "Implementation"),
                          ("Embedded simulation", "Examples")):
        matches = [item for item in subheadings if item[0] == title]
        count = sum(behavioral for _, behavioral in components) if title == "Behavioral implementation" else component_count
        assert matches == [[title, parent]] * count, matches
    assert not any(title in {"Binary behavioral reference", "Simulator variants", "Structural circuit"}
                   for title, _ in subheadings), subheadings


def assert_control_accessible(scope, selector: str) -> None:
    control = scope.locator(selector)
    geometry = control.evaluate("""element => {
      const r = element.getBoundingClientRect();
      const hit = document.elementFromPoint(r.x + r.width / 2, r.y + r.height / 2);
      return {inside: r.x >= 0 && r.y >= 0 && r.right <= innerWidth && r.bottom <= innerHeight,
              clickable: Boolean(hit && element.contains(hit))};
    }""")
    assert all(geometry.values()), (selector, geometry)


def assert_camera_framed(scope, component_id=None) -> dict:
    # Use actual DOM panel edges and projected component/pin bounds, not the
    # camera's own viewport/fitting helpers, as the regression oracle.
    result = scope.locator("body").evaluate("""async (_, componentId) => {
      const deadline = performance.now() + 10000;
      do {
        await new Promise(requestAnimationFrame);
        if (performance.now() > deadline) throw new Error('Camera did not settle: ' + JSON.stringify({
          animating: Boolean(app.cameraAnimation), pending: [...app.pendingScopeIds],
          missing: visibleMissingScopeIds(), error: document.body.dataset.circuitsimError,
        }));
      } while (app.cameraAnimation || app.geometryDirty || app.topologyRequestInFlight
               || app.pendingScopeIds.size || visibleMissingScopeIds().length);
      const component = app.components.get(componentId || app.selectedComponentId || app.rootId);
      const points = [];
      function addRect(r) {
        if (r) points.push({x: r.x, y: r.y}, {x: r.x + r.w, y: r.y + r.h});
      }
      addRect(component.rect);
      for (const pin of [...component.inputPins, ...component.outputPins]) {
        addRect(pin.rect);
        points.push(...(pin.outerStub || []));
      }
      const projected = points.map(p => ({
        x: (p.x - app.camera.offset.x) * app.camera.zoom,
        y: (p.y - app.camera.offset.y) * app.camera.zoom,
      }));
      const bounds = {
        left: Math.min(...projected.map(p => p.x)),
        right: Math.max(...projected.map(p => p.x)),
        top: Math.min(...projected.map(p => p.y)),
        bottom: Math.max(...projected.map(p => p.y)),
      };
      const canvasRect = document.querySelector('#circuit-canvas').getBoundingClientRect();
      const explorer = document.querySelector('#component-explorer').getBoundingClientRect();
      const inspector = document.querySelector('#inspector');
      let left = canvasRect.left;
      let right = canvasRect.right;
      if (document.body.classList.contains('explorer-open')) left = Math.max(left, explorer.right);
      if (!inspector.classList.contains('hidden')) right = Math.min(right, inspector.getBoundingClientRect().left);
      const top = document.querySelector('#topbar').getBoundingClientRect().bottom - canvasRect.top;
      const bottom = document.querySelector('#viewbar').getBoundingClientRect().top - canvasRect.top;
      const padding = Math.min(28, (right - left - 1) / 4, (bottom - top - 1) / 4);
      const safe = {
        left: left - canvasRect.left + padding,
        right: right - canvasRect.left - padding,
        top: top + padding, bottom: bottom - padding,
      };
      return {
        id: component.id, windowWidth: innerWidth, bounds, safe,
        centerErrorX: (bounds.left + bounds.right - safe.left - safe.right) / 2,
        centerErrorY: (bounds.top + bounds.bottom - safe.top - safe.bottom) / 2,
        fill: Math.max((bounds.right - bounds.left) / (safe.right - safe.left),
                       (bounds.bottom - bounds.top) / (safe.bottom - safe.top)),
      };
    }""", component_id)
    assert abs(result["centerErrorX"]) < 0.5, result
    assert abs(result["centerErrorY"]) < 0.5, result
    assert abs(result["fill"] - 1) < 0.005, result
    return result


def check_camera_controls(page) -> list:
    return page.evaluate("""async () => {
      const close = (a, b) => Math.abs(a-b) < 1e-7 * Math.max(1, Math.abs(a), Math.abs(b));
      const require = (ok, message) => { if (!ok) throw new Error(message); };
      const pose = () => ({zoom: app.camera.zoom, x: app.camera.offset.x, y: app.camera.offset.y});
      const world = (pose, point) => ({x: point.x / pose.zoom + pose.x, y: point.y / pose.zoom + pose.y});
      const idle = async () => {
        const deadline = performance.now() + 10000;
        do {
          await new Promise(requestAnimationFrame);
          require(performance.now() < deadline, 'Camera controls did not settle');
        } while (app.cameraAnimation || app.geometryDirty || app.topologyRequestInFlight || app.pendingScopeIds.size);
      };
      frameRoot();
      await idle();
      const initialZoom = app.camera.zoom;
      // Independently measure the visible canvas center, excluding side panels.
      const canvas = document.querySelector('#circuit-canvas');
      const rect = canvas.getBoundingClientRect();
      const explorer = document.querySelector('#component-explorer').getBoundingClientRect();
      const inspector = document.querySelector('#inspector');
      const left = document.body.classList.contains('explorer-open') ? explorer.right : rect.left;
      const right = inspector.classList.contains('hidden') ? rect.right : inspector.getBoundingClientRect().left;
      const top = document.querySelector('#topbar').getBoundingClientRect().bottom;
      const bottom = document.querySelector('#viewbar').getBoundingClientRect().top;
      const center = {x: (left + right) / 2 - rect.left, y: (top + bottom) / 2 - rect.top};
      const pointer = {x: center.x + 30, y: center.y - 45};
      const results = [];
      async function check(label, action, factor, anchor) {
        await idle();
        const before = pose();
        const anchoredWorld = anchor && world(before, anchor);
        action();
        const animation = app.cameraAnimation;
        require(Boolean(animation), label + ': did not start an animation');
        if (anchor) {
          require(close(animation.screenCenter.x, anchor.x) && close(animation.screenCenter.y, anchor.y), label + ': wrong zoom center');
        }
        const targetZoom = Math.exp(animation.to.logZoom);
        if (factor) require(close(targetZoom, before.zoom * factor), label + ': dropped zoom input');
        const samples = [pose()];
        const deadline = performance.now() + 10000;
        while (app.cameraAnimation) {
          await new Promise(requestAnimationFrame);
          samples.push(pose());
          require(performance.now() < deadline, label + ': animation timed out');
        }
        require(samples.some(s => !close(s.zoom, before.zoom) && !close(s.zoom, targetZoom)), label + ': no intermediate movement');
        require(close(app.camera.zoom, targetZoom), label + ': missed destination');
        for (const sample of samples) if (anchor) {
          const actual = world(sample, anchor);
          require(close(actual.x, anchoredWorld.x) && close(actual.y, anchoredWorld.y), label + ': zoom anchor drifted');
        }
        results.push({label, frames: samples.length});
      }
      const click = id => document.querySelector(id).click();
      await check('zoom in', () => click('#zoom-in'), 1.25, center);
      await check('zoom out', () => click('#zoom-out'), 0.8, center);
      await check('rapid zoom', () => { for (let i = 0; i < 3; ++i) click('#zoom-in'); }, 1.25 ** 3, center);
      const wheel = deltaY => canvas.dispatchEvent(new WheelEvent('wheel', {
        deltaY, clientX: rect.left + pointer.x, clientY: rect.top + pointer.y, cancelable: true,
      }));
      await check('wheel in', () => wheel(-100), 1.1, pointer);
      await check('wheel out', () => wheel(100), 0.9, pointer);
      const selected = app.selectedComponentId;
      await check('reset view', () => click('#reset-view'), null, null);
      require(close(app.camera.zoom, initialZoom), 'Reset did not restore root framing');
      require(app.selectedComponentId === selected, 'Reset changed the selection');
      return results;
    }""")


def check_focus_camera(page, component_types=("FullAdder", "HalfAdder", "XORGate")) -> list:
    results = []
    root_id = page.evaluate("app.rootId")
    page.locator(".explorer-row").first.click()
    results.append(assert_camera_framed(page, root_id))
    if root_id == "ADDER8_ROOT":
        page.screenshot(path="/tmp/circuitsim-camera-root.png")
    # Root reset and explorer focus must include the same pins and padding.
    page.locator("#reset-view").click()
    results.append(assert_camera_framed(page, root_id))
    children = page.evaluate("""types => types.map(
      type => app.componentOrder.find(c => c.type === type)?.id).filter(Boolean)""", list(component_types))
    assert len(children) == len(component_types), (component_types, children)
    for component_id in children:
        page.evaluate("id => app.explorer.onFocus(id)", component_id)
        results.append(assert_camera_framed(page, component_id))
    if root_id == "ADDER8_ROOT":
        page.screenshot(path="/tmp/circuitsim-camera-child.png")
    return results


def check_alu_assembly(page, scenario: str) -> dict:
    page.evaluate("id => app.explorer.onFocus(id)", page.evaluate("app.rootId"))
    assert_camera_framed(page)
    result = page.evaluate("""() => {
      const root = app.components.get(app.rootId);
      const units = app.componentOrder.filter(c => c.parentId === app.rootId);
      return {type: root.type, names: units.map(c => c.name).sort(),
        widths: units.map(c => c.rect.w / root.rect.w)};
    }""")
    names = ["CONTROL", "ARITHMETIC", "LOGIC", "RESULT_SELECT", "FLAGS"]
    names += ["SHIFTER", "COMPARE_SELECT"] if scenario == "alu8" else ["SHIFT", "COMPARE"]
    assert result["names"] == sorted(names), result
    assert all(width > 0.10 for width in result["widths"]), result
    page.screenshot(path=f"/tmp/circuitsim-{scenario}-assembly.png")
    return result


def check_example_table(page, embed, scenario: str, *, behavioral=True) -> int:
    block = page.locator(f'.example-table-block[data-scenario="{scenario}"]')
    assert block.count() == 1
    rows = block.locator("tbody tr").evaluate_all("""nodes => nodes.map(row => ({
      checkpoint: row.dataset.checkpoint, time: Number(row.dataset.time),
      index: Number(row.dataset.timelineIndex),
      pins: Object.fromEntries([...row.querySelectorAll('td[data-bits]')]
        .filter(cell => cell.dataset.bits).map(cell => [cell.dataset.pin, cell.dataset.bits])),
    }))""")
    model = embed.locator("body").evaluate("""() => ({
      profile: app.components.get(app.rootId).profileFingerprint,
      selectable: Boolean(app.components.get(app.rootId).profileSelectable),
      fidelities: app.components.get(app.rootId).availableFidelities,
      checkpoints: app.checkpoints.map(c => ({checkpoint: c.label, time: c.time, index: c.index})),
    })""")
    assert block.get_attribute("data-profile") == model["profile"]
    assert model["selectable"] == behavioral, (scenario, model)
    assert set(model["fidelities"]) == ({"structural", "behavioral"} if behavioral else {"structural"}), scenario
    assert [{k: row[k] for k in ("checkpoint", "time", "index")} for row in rows] == model["checkpoints"]
    # The exported expectations must match actual recorded pins in the iframe,
    # including the final X/Z examples where present.
    for row in (rows[0], rows[-1]):
        actual = embed.locator("body").evaluate("""async (_, index) => {
          await setStateIndex(index);
          return Object.fromEntries([...app.pins.values()]
            .filter(pin => pin.componentId === app.rootId)
            .map(pin => [pin.name, app.state.pins[pin.stateIndex]]));
        }""", row["index"])
        assert all(actual[name] == value for name, value in row["pins"].items()), (scenario, row, actual)
        assert row["checkpoint"] in embed.locator("#time-label").inner_text()

    scroller = block.locator(".example-table-scroll")
    scroller.scroll_into_view_if_needed()
    geometry = scroller.evaluate("""element => {
      element.scrollTop = 0; element.scrollLeft = 0;
      const header = element.querySelector('thead th');
      const first = element.querySelector('tbody th');
      const top = header.getBoundingClientRect().top;
      const left = first.getBoundingClientRect().left;
      element.scrollTop = 120; element.scrollLeft = 160;
      return {vertical: element.scrollHeight > element.clientHeight,
        moved: element.scrollTop > 0, headerError: header.getBoundingClientRect().top - top,
        columnError: first.getBoundingClientRect().left - left,
        width: element.clientWidth, height: element.clientHeight,
        documentOverflow: document.documentElement.scrollWidth > innerWidth + 1};
    }""")
    assert geometry["vertical"] and geometry["moved"], (scenario, geometry)
    assert abs(geometry["headerError"]) < 1 and abs(geometry["columnError"]) < 1, (scenario, geometry)
    assert not geometry["documentOverflow"], (scenario, geometry)
    assert scroller.get_attribute("tabindex") == "0"
    scroller.evaluate("element => { element.scrollTop = 0; element.scrollLeft = 0; }")
    scroller.focus()
    page.keyboard.press("ArrowDown")
    page.wait_for_function("selector => document.querySelector(selector).scrollTop > 0",
                           arg=f'.example-table-block[data-scenario="{scenario}"] .example-table-scroll')
    if scenario == "alu8-flags":
        page.screenshot(path="/tmp/circuitsim-example-table-desktop.png")
        original = page.viewport_size
        for width in (640, 360):
            page.set_viewport_size({"width": width, "height": 800})
            scroller.scroll_into_view_if_needed()
            assert scroller.evaluate("e => e.scrollWidth > e.clientWidth")
            # Existing display equations can extend the mobile document. The
            # example table itself must stay within the viewport and must not
            # add any document overflow beyond those unrelated elements.
            assert scroller.evaluate("e => { const r=e.getBoundingClientRect(); return r.left >= 0 && r.right <= innerWidth; }")
            assert block.evaluate("""e => {
              const before = document.documentElement.scrollWidth;
              e.style.display = 'none';
              const without = document.documentElement.scrollWidth;
              e.style.removeProperty('display');
              return before <= without;
            }""")
        page.screenshot(path="/tmp/circuitsim-example-table-mobile.png")
        page.set_viewport_size(original)
    return len(rows)


def check_compact_panels(scope) -> None:
    body = scope.locator("body")
    assert "compact-view" in body.get_attribute("class")
    toggle = scope.locator("#toggle-explorer")
    info = scope.locator("#toggle-inspector")
    inspector = scope.locator("#inspector")
    assert toggle.get_attribute("aria-pressed") == "false"
    assert inspector.is_hidden()
    for selector in ("#toggle-explorer", "#toggle-inspector", "#scenario-select", "#time-slider",
                     "#play-pause", "#checkpoint-back", "#step-back", "#step-forward",
                     "#checkpoint-forward", "#reset-sim"):
        assert_control_accessible(scope, selector)
    toggle.click()
    assert toggle.get_attribute("aria-pressed") == "true"
    scope.locator("#explorer-close").click(trial=True)
    assert_control_accessible(scope, "#toggle-explorer")
    assert_control_accessible(scope, "#explorer-close")
    scope.locator(".explorer-row").first.click()
    assert toggle.get_attribute("aria-pressed") == "false"
    assert inspector.is_hidden(), "Selecting a component should not cover the embedded circuit"
    assert_camera_framed(scope)
    info.click()
    assert inspector.is_visible()
    assert toggle.get_attribute("aria-pressed") == "false"
    assert_control_accessible(scope, "#toggle-explorer")
    assert_control_accessible(scope, "#toggle-inspector")
    assert_control_accessible(scope, "#inspector-close")
    toggle.click()
    assert inspector.is_hidden(), "Compact panes must not overlap each other"
    assert toggle.get_attribute("aria-pressed") == "true"
    scope.locator("#explorer-close").click()
    assert toggle.get_attribute("aria-pressed") == "false"
    # Secondary actions may scroll, but panel toggles must remain in place.
    scope.locator("#save-layout").scroll_into_view_if_needed()
    assert_control_accessible(scope, "#save-layout")
    assert_control_accessible(scope, "#toggle-explorer")
    assert_control_accessible(scope, "#toggle-inspector")


def check_responsive_panels(page) -> None:
    toggle = page.locator("#toggle-explorer")
    inspector = page.locator("#inspector")
    assert "compact-view" not in page.locator("body").get_attribute("class")
    assert toggle.get_attribute("aria-pressed") == "true"
    # Desktop keeps selection-driven inspection and independent panels.
    page.locator(".explorer-row").first.click()
    assert inspector.is_visible()
    assert toggle.get_attribute("aria-pressed") == "true"
    assert_control_accessible(page, "#toggle-explorer")
    assert_control_accessible(page, "#toggle-inspector")
    page.locator("#inspector-close").click()
    toggle.click()
    toggle.click()
    for width in (960, 640, 360, 320):
        page.set_viewport_size({"width": width, "height": 720})
        page.wait_for_function("document.body.classList.contains('compact-view')")
        try:
            check_compact_panels(page)
        except Exception:
            page.screenshot(path="/tmp/circuitsim-compact-failure.png")
            raise
        assert page.evaluate("localStorage.getItem('circuitsim.explorer.open')") == "true"
    page.set_viewport_size({"width": 1440, "height": 900})
    page.wait_for_function("!document.body.classList.contains('compact-view')")
    assert toggle.get_attribute("aria-pressed") == "true"
    # A desktop preference for a closed explorer must also survive resizing.
    toggle.click()
    page.set_viewport_size({"width": 640, "height": 720})
    page.wait_for_function("document.body.classList.contains('compact-view')")
    check_compact_panels(page)
    assert page.evaluate("localStorage.getItem('circuitsim.explorer.open')") == "false"
    page.set_viewport_size({"width": 1440, "height": 900})
    page.wait_for_function("!document.body.classList.contains('compact-view')")
    assert toggle.get_attribute("aria-pressed") == "false"
    toggle.click()


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit("Usage: VisualizerWasmBrowserTest.py <simulator-url>")
    chrome = (
        shutil.which("google-chrome")
        or shutil.which("google-chrome-stable")
        or shutil.which("chromium")
    )
    if chrome is None:
        raise RuntimeError("Chrome or Chromium is required for the browser smoke test")

    with sync_playwright() as playwright:
        browser = playwright.chromium.launch(
            executable_path=chrome,
            headless=True,
            args=["--no-sandbox", "--disable-dev-shm-usage"],
        )
        context = browser.new_context(viewport={"width": 1440, "height": 900})
        page = context.new_page()
        page_errors: list[str] = []
        page.on("pageerror", lambda error: page_errors.append(str(error)))
        started = perf_counter()
        page.goto(
            f"{sys.argv[1]}?scenario=adder8",
            wait_until="domcontentloaded",
            timeout=30_000,
        )
        page.wait_for_function(
            "document.body.dataset.circuitsimStatus === 'ready'",
            timeout=30_000,
        )
        browser_load_ms = (perf_counter() - started) * 1000

        visible = page.evaluate(
            """() => ({
              status: document.body.dataset.circuitsimStatus,
              runtime: document.body.dataset.circuitsimRuntime,
              scenarios: document.querySelector('#scenario-select').options.length,
              selected: document.querySelector('#scenario-select').value,
              stats: document.querySelector('#stats-label').textContent,
              loadingHidden: document.querySelector('#loading').classList.contains('hidden'),
              canvasWidth: document.querySelector('#circuit-canvas').width,
            })"""
        )
        assert visible["status"] == "ready"
        assert visible["runtime"] == "wasm"
        assert visible["selected"] == "adder8"
        assert visible["scenarios"] > 0
        assert visible["loadingHidden"]
        assert visible["canvasWidth"] > 0
        assert "68 comps" in visible["stats"]
        camera_controls = check_camera_controls(page)
        camera_checks = check_focus_camera(page)
        for width in (1024, 1120, 1140, 1280, 1920):
            page.set_viewport_size({"width": width, "height": 900})
            page.wait_for_function("app.width === Math.round(canvas.getBoundingClientRect().width)")
            page.locator(".explorer-row").first.click()
            camera_checks.append(assert_camera_framed(page))
        page.set_viewport_size({"width": 1440, "height": 900})
        page.locator("#toggle-explorer").click()
        page.wait_for_function("app.width === 1440")
        page.evaluate("smoothlyFocusComponent(app.rootId)")
        camera_checks.append(assert_camera_framed(page))
        page.locator("#inspector-close").click()
        page.locator("#reset-view").click()
        camera_checks.append(assert_camera_framed(page, page.evaluate("app.rootId")))
        page.locator("#toggle-explorer").click()
        page.locator(".explorer-row").first.click()
        page.locator("#scenario-select").select_option("arithmetic8")
        page.wait_for_function("app.scenario === 'arithmetic8' && !app.geometryDirty")
        camera_checks.append(assert_camera_framed(page, page.evaluate("app.rootId")))
        camera_checks.extend(check_focus_camera(page))
        page.locator("#scenario-select").select_option("adder8")
        page.wait_for_function("app.scenario === 'adder8' && !app.geometryDirty")
        camera_checks.append(assert_camera_framed(page, page.evaluate("app.rootId")))

        contracts_started = perf_counter()
        contracts = page.evaluate(
            """async () => {
              async function request(path, options) {
                const response = await window.visualizerRequest(
                  new URL(path, document.baseURI).toString(),
                  options,
                );
                const text = await response.text();
                let body = null;
                if (text) {
                  try { body = JSON.parse(text); } catch { body = text; }
                }
                return { ok: response.ok, status: response.status, body };
              }

              const adder = await request('api/circuit?scenario=adder8');
              const originalLayout = adder.body.layout;
              const changedLayout = JSON.parse(JSON.stringify(originalLayout));
              changedLayout.default_settings.wasm_browser_test_marker = 'persisted';
              // Simulate a saved layout from before the ALU8 replacement.
              for (const type of ['ALU8', 'AddSub8', 'ArithmeticUnit8', 'LogicUnit8',
                'BarrelShifter8', 'CompareSelect8', 'ALU8FlagUnit']) {
                delete changedLayout.type_layouts[type];
              }
              for (const key of Object.keys(changedLayout.root_layouts || {})) {
                if (key.startsWith('alu8')) delete changedLayout.root_layouts[key];
              }
              // A pre-regrouping saved ALU layout must not keep the old tiny
              // functional blocks or restore raw gates at the new top level.
              const legacyAluChildren = {
                OPERATION_DECODER: {rel_pos: [0.1, 0.1], rel_width: 0.01},
                ARITHMETIC: {rel_pos: [0.5, 0.1], rel_width: 0.01},
              };
              changedLayout.type_layouts.ALU8 = {children: legacyAluChildren};
              for (const key of Object.keys(originalLayout.root_layouts || {})) {
                if (key.startsWith('alu8@')) {
                  changedLayout.root_layouts[key] = {...originalLayout.root_layouts[key], children: legacyAluChildren};
                }
              }
              const saved = await request('api/layout', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ layout: changedLayout }),
              });
              const defaults = await request('api/layout/defaults', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                  scenario: 'adder8',
                  parentId: adder.body.rootId,
                  layout: changedLayout,
                }),
              });

              const pipeline = await request('api/circuit?scenario=rv32i-five-stage-program1');
              const controlPack = pipeline.body.components.find(
                (component) => component.id === 'RV32I_PROGRAM_ROOT.CORE.DECODE.CONTROL_PACK',
              );
              const packModel = controlPack
                ? window.CircuitRewireGeometry.packedBusModel(controlPack.rewire)
                : null;

              const memory = await request('api/circuit?scenario=flip-flop-cell');
              const root = memory.body.components.find((component) => component.id === memory.body.rootId);
              const applied = await request('api/circuit/apply', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                  scenario: 'flip-flop-cell',
                  revision: memory.body.profile.revision,
                  exactOverrides: { [memory.body.rootId]: 'behavioral' },
                }),
              });
              const stale = await request('api/circuit/apply', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                  scenario: 'flip-flop-cell',
                  revision: memory.body.profile.revision,
                  exactOverrides: {},
                }),
              });
              const topology = await request('api/topology', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                  session: applied.body.sessionId,
                  ownerIds: [applied.body.rootId],
                }),
              });
              return {
                saved: saved.ok,
                defaultsLoaded: defaults.ok && Object.keys(defaults.body.placements).length > 0,
                bundledProfileLoaded: pipeline.ok
                  && pipeline.body.profile.revision === 3
                  && pipeline.body.profile.exactOverrides['RV32I_PROGRAM_ROOT.CORE.DECODE'] === 'structural',
                controlPackRecognized: Boolean(packModel)
                  && packModel.groups.length === 17
                  && packModel.width === 32
                  && packModel.unmappedBits.length === 1
                  && packModel.unmappedBits[0] === 31,
                semanticPaletteLoaded: Boolean(controlPack)
                  && JSON.stringify(controlPack.color) === JSON.stringify([100, 116, 139, 190]),
                rootSelectable: root.profileSelectable && root.availableFidelities.includes('behavioral'),
                applied: applied.ok && applied.body.components[0].fidelity === 'behavioral',
                profileRevisionApplied: applied.body.profile.revision,
                staleRejected: !stale.ok,
                topologyLoaded: topology.ok && topology.body.scopeIds[0] === applied.body.rootId,
              };
            }"""
        )
        assert all(contracts.values()), contracts

        page.reload(wait_until="domcontentloaded", timeout=30_000)
        page.wait_for_function(
            "document.body.dataset.circuitsimStatus === 'ready'",
            timeout=30_000,
        )
        persisted = page.evaluate(
            """async (expectedProfileRevision) => {
              async function request(path, options) {
                const response = await window.visualizerRequest(
                  new URL(path, document.baseURI).toString(),
                  options,
                );
                return { ok: response.ok, body: JSON.parse(await response.text()) };
              }
              const adder = await request('api/circuit?scenario=adder8');
              const memory = await request('api/circuit?scenario=flip-flop-cell');
              const defaultLayout = await (await fetch(
                new URL('layout.json', document.baseURI),
              )).json();
              const restored = await request('api/layout', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ layout: defaultLayout }),
              });
              return {
                layoutPersisted: adder.body.layout.default_settings.wasm_browser_test_marker === 'persisted',
                profilePersisted: memory.body.profile.revision === expectedProfileRevision
                  && memory.body.components[0].fidelity === 'behavioral',
                restored: restored.ok,
                newDefaultsMerged: Boolean(adder.body.layout.type_layouts.ArithmeticUnit8),
              };
            }""",
            contracts["profileRevisionApplied"],
        )
        assert all(persisted.values()), persisted
        assert not page_errors, page_errors
        check_responsive_panels(page)

        # Same browser context as the desktop page: iframe toggles must not
        # overwrite its saved panel preference in shared localStorage.
        embed_page = page.context.new_page()
        embed_page.set_viewport_size({"width": 1440, "height": 900})
        embed_errors: list[str] = []
        embed_page.on("pageerror", lambda error: embed_errors.append(str(error)))
        embed_page.goto(
            urljoin(sys.argv[1], "../visualizer/"),
            wait_until="domcontentloaded",
            timeout=30_000,
        )
        embed = embed_page.frame_locator(".simulator-embed iframe")
        embed.locator("body[data-circuitsim-status='ready']").wait_for(
            timeout=30_000,
        )
        embedded = {
            "status": embed.locator("body").get_attribute("data-circuitsim-status"),
            "runtime": embed.locator("body").get_attribute("data-circuitsim-runtime"),
        }
        assert embedded == {"status": "ready", "runtime": "wasm"}
        check_compact_panels(embed)
        assert not embed_errors, embed_errors

        # The chapter page must resolve simulator links under a project-site path.
        embed_page.goto(
            urljoin(sys.argv[1], "../alu8/2.1/"),
            wait_until="domcontentloaded",
            timeout=30_000,
        )
        assert "Arithmetic Components" in embed_page.locator("h1").inner_text()
        check_component_documentation_pattern(embed_page, (
            ("HalfAdder", False), ("FullAdder", False), ("Adder8", True),
            ("AddSub8", True), ("ArithmeticUnit8", True)))
        assert embed_page.locator(".simulator-embed iframe").count() == 5
        chapter_embeds = {}
        example_row_count = 0
        for scenario, component_count in (("half-adder", 3), ("full-adder", 8),
                                           ("adder8", 68), ("addsub8", 85), ("arithmetic8", 826)):
            # Each iframe is lazy-loaded below its own component tables.
            selector = (f".simulator-embed iframe[src$='scenario={scenario}'], "
                        f".simulator-embed iframe[src*='scenario={scenario}&']")
            embed_page.locator(selector).scroll_into_view_if_needed()
            chapter_embed = embed_page.frame_locator(selector)
            chapter_embed.locator("body[data-circuitsim-status='ready']").wait_for(timeout=30_000)
            assert chapter_embed.locator("body").get_attribute("data-circuitsim-runtime") == "wasm"
            assert chapter_embed.locator("#scenario-select").input_value() == scenario
            stats = chapter_embed.locator("#stats-label").inner_text()
            assert f"{component_count} comps" in stats, stats
            try:
                check_compact_panels(chapter_embed)
            except Exception:
                embed_page.screenshot(path="/tmp/circuitsim-embed-failure.png")
                raise
            if scenario == "adder8":
                embed_page.screenshot(path="/tmp/circuitsim-embed-compact.png")
            chapter_embeds[scenario] = stats
            example_row_count += check_example_table(embed_page, chapter_embed, scenario,
                behavioral=scenario not in {"half-adder", "full-adder"})
            link = embed_page.locator(f"a.md-button[href$='scenario={scenario}']")
            assert link.count() == 1

        # Each new reference page must retain the five-section component
        # pattern and expose its own recorded component scenario.
        reference_pages = (
            ("2.2", "Logic Operations", (("logic8", "LogicUnit8", 32),)),
            ("2.3", "Shifts and Rotations", (("barrel-shifter8", "BarrelShifter8", 96),)),
            ("2.4", "Comparison and Selection", (
                ("zero-detect8", "ZeroDetect8", 11), ("compare-select8", "CompareSelect8", 40))),
            ("2.5", "Flags", (("alu8-flags", "ALU8FlagUnit", 128),)),
            ("2.6", "Complete ALU8", (
                ("alu8-control", "ALU8ControlDecoder", 32),
                ("alu8-result-selector", "ALU8ResultSelector", 66),
                ("alu8", "ALU8", 70))),
        )
        for section, title, scenarios in reference_pages:
            embed_page.goto(urljoin(sys.argv[1], f"../alu8/{section}/"),
                            wait_until="domcontentloaded", timeout=30_000)
            assert title in embed_page.locator("h1").inner_text()
            check_component_documentation_pattern(embed_page, tuple((kind, True) for _, kind, _ in scenarios))
            assert embed_page.locator(".simulator-embed iframe").count() == len(scenarios)
            for scenario, component_type, rows in scenarios:
                selector = f".simulator-embed iframe[src$='scenario={scenario}']"
                embed_page.locator(selector).scroll_into_view_if_needed()
                chapter_embed = embed_page.frame_locator(selector)
                chapter_embed.locator("body[data-circuitsim-status='ready']").wait_for(timeout=30_000)
                assert chapter_embed.locator("#scenario-select").input_value() == scenario
                model = chapter_embed.locator("body").evaluate("""() => ({
                  type: app.components.get(app.rootId).type,
                  rows: app.checkpoints.length,
                  runtime: document.body.dataset.circuitsimRuntime,
                })""")
                assert model == {"type": component_type, "rows": rows, "runtime": "wasm"}, model
                check_compact_panels(chapter_embed)
                assert embed_page.locator(f"a.md-button[href$='scenario={scenario}']").count() == 1
                chapter_embeds[scenario] = model
                example_row_count += check_example_table(embed_page, chapter_embed, scenario)
        assert example_row_count == 729  # Includes 16 half-adder and 64 full-adder four-state rows.
        static_context = browser.new_context(java_script_enabled=False)
        static_page = static_context.new_page()
        static_page.goto(urljoin(sys.argv[1], "../alu8/2.6/"), wait_until="domcontentloaded")
        assert static_page.locator(".example-table tbody tr").count() == 168
        assert static_page.locator(".example-table-scroll").first.is_visible()
        static_context.close()
        assert page.evaluate("localStorage.getItem('circuitsim.explorer.open')") == "true"

        page.bring_to_front()
        page.goto(f"{sys.argv[1]}?scenario=alu8", wait_until="domcontentloaded", timeout=30_000)
        try:
            page.wait_for_function("['ready', 'error'].includes(document.body.dataset.circuitsimStatus)", timeout=60_000)
            assert page.locator("body").get_attribute("data-circuitsim-status") == "ready", page.locator("#loading").inner_text()
        except Exception:
            print(json.dumps({"pageErrors": page_errors, "loading": page.locator("#loading").inner_text()}))
            raise
        assert page.locator("#scenario-select").input_value() == "alu8"
        # Exercise the live UI before the API-only probe replaces its worker
        # session; deferred pin requests belong to the currently rendered one.
        camera_checks.extend(check_focus_camera(page))
        alu8_assembly = check_alu_assembly(page, "alu8")
        # Real explorer interaction: collapse, persist across reload, expand.
        # Keep the default profile restored before checking the documentation.
        page.evaluate("app.explorer.setOpen(true, {persist: false})")
        root_row = page.locator(".explorer-row").first
        assert root_row.locator(".explorer-badge").inner_text() == "S"
        root_row.locator(".explorer-arrow").click()
        page.locator("#apply-profile").click()
        page.wait_for_function("app.components.size === 1 && app.components.get(app.rootId).fidelity === 'behavioral' && document.querySelector('#loading').classList.contains('hidden')")
        page.reload(wait_until="domcontentloaded")
        page.wait_for_function("document.body.dataset.circuitsimStatus === 'ready'")
        assert page.evaluate("app.components.size === 1 && app.components.get(app.rootId).fidelity === 'behavioral'")
        root_row.locator(".explorer-arrow").click()
        page.locator("#apply-profile").click()
        page.wait_for_function("app.components.size === 2330 && app.components.get(app.rootId).fidelity === 'structural' && document.querySelector('#loading').classList.contains('hidden')")
        alu_contract = page.evaluate(
            """async () => {
              const response = await window.visualizerRequest(
                new URL('api/circuit?scenario=alu8', document.baseURI).toString(),
              );
              const payload = await response.json();
              const pins = payload.pins.filter(pin => pin.componentId === payload.rootId);
              return {
                loaded: response.ok && payload.rootType === 'ALU8',
                opcode: pins.some(pin => pin.name === 'OP' && pin.width === 5),
                carryInput: pins.some(pin => pin.name === 'C_IN' && pin.width === 1),
                invalidFlag: pins.some(pin => pin.name === 'INVALID_OP' && pin.width === 1),
                sharedAdder: payload.components.filter(component => component.type === 'Adder8').length === 1,
                fullAdders: payload.components.filter(component => component.type === 'FullAdder').length === 8,
                sevenBlocks: payload.components.filter(component => component.parentIndex ===
                  payload.components.findIndex(parent => parent.id === payload.rootId)).length === 7,
                reflowedLayout: Object.keys(payload.layout.root_layouts[payload.layoutKey].children).length === 7
                  && Object.values(payload.layout.root_layouts[payload.layoutKey].children)
                    .every(child => child.rel_width > 0.06),
                checkpoints: payload.checkpoints.length,
              };
            }"""
        )
        assert all(alu_contract.values()), alu_contract
        page.goto(f"{sys.argv[1]}?scenario=alu32", wait_until="domcontentloaded", timeout=30_000)
        page.wait_for_function("document.body.dataset.circuitsimStatus === 'ready'", timeout=60_000)
        # ALU32's optimized carry path is a prefix tree, not a ripple chain.
        # Keep testing nested arithmetic and gates against actual DOM bounds.
        camera_checks.extend(check_focus_camera(page,
            ("AddSub32", "ParallelPrefixAdder32", "CarryCombine", "XORGate")))
        alu32_assembly = check_alu_assembly(page, "alu32")
        assert not page_errors, page_errors
        assert not embed_errors, embed_errors

        visible["browserLoadMs"] = browser_load_ms
        visible["contractTestMs"] = (perf_counter() - contracts_started) * 1000
        visible["contracts"] = {**contracts, **persisted}
        visible["embedded"] = embedded
        visible["chapterEmbeds"] = chapter_embeds
        visible["documentedExampleRows"] = example_row_count
        visible["compactPanelWidths"] = [960, 640, 360, 320]
        visible["cameraChecks"] = {
            "count": len(camera_checks),
            "maxCenterErrorPx": max(max(abs(check["centerErrorX"]), abs(check["centerErrorY"]))
                                    for check in camera_checks),
            "maxFitError": max(abs(check["fill"] - 1) for check in camera_checks),
        }
        visible["cameraControls"] = camera_controls
        visible["alu8"] = alu_contract
        visible["aluAssemblies"] = [alu8_assembly, alu32_assembly]
        print(json.dumps(visible, sort_keys=True))
        browser.close()


if __name__ == "__main__":
    main()
