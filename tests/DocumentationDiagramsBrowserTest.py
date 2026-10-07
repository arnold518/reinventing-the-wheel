"""Check static documentation figures in MkDocs without allocating WASM heaps.

Usage: python tests/DocumentationDiagramsBrowserTest.py [base-url] [screenshots]
Run serially; checks typography/geometry, themes, navigation, and no-JS use.
"""

from pathlib import Path
import json
import math
import shutil
import sys
from urllib.parse import urljoin

from playwright.sync_api import sync_playwright


PAGES = {
    "memory/3.1/": 2,
    "memory/3.1.1/": 1,
    "rv32i/4.3/register/": 1,
    "rv32i/4.3/register-file/": 1,
    "rv32i/4.3/memory-access/": 1,
    "rv32i/4.4/": 4,
    "rv32i/4.5/": 1,
}
BASE = (sys.argv[1] if len(sys.argv) > 1 else "http://127.0.0.1:8893/").rstrip("/") + "/"
SCREENSHOTS = Path(sys.argv[2]) if len(sys.argv) > 2 else None
if SCREENSHOTS:
    SCREENSHOTS.mkdir(parents=True, exist_ok=True)


def wait_for_diagrams(page, count):
    page.wait_for_function("""count => {
        const diagrams = [...document.querySelectorAll('.circuit-figure')];
        return diagrams.length === count && diagrams.every(el => el.querySelector('svg text'));
    }""", arg=count)
    page.evaluate("document.fonts.ready")


def check_geometry(page, width):
    result = page.locator(".circuit-figure").evaluate_all("""diagrams => diagrams.map(el => {
        const svg = el.querySelector('svg');
        const viewport = el.querySelector('.diagram-viewport');
        const texts = [...svg.querySelectorAll('text')];
        const labels = texts.map(node => ({text: node.textContent, box: node.getBBox()}));
        const rects = [...svg.querySelectorAll('rect[x]')].map(node => node.getBBox());
        const intersects = (a, b) => Math.min(a.x + a.width, b.x + b.width) > Math.max(a.x, b.x) + .5 &&
            Math.min(a.y + a.height, b.y + b.height) > Math.max(a.y, b.y) + .5;
        const contains = (outer, inner, tolerance = 0) => inner.x >= outer.x - tolerance &&
            inner.y >= outer.y - tolerance && inner.x + inner.width <= outer.x + outer.width + tolerance &&
            inner.y + inner.height <= outer.y + outer.height + tolerance;
        const problems = [];
        for (let i = 0; i < labels.length; i++) {
            const label = labels[i], b = label.box;
            if (!contains(svg.viewBox.baseVal, b)) problems.push('Outside canvas: ' + label.text);
            for (let j = i + 1; j < labels.length; j++)
                if (intersects(b, labels[j].box)) problems.push('Overlapping labels: ' + label.text + ' / ' + labels[j].text);
            // Each label must fit its smallest containing box with a 4px inset.
            const center = {x: b.x + b.width / 2, y: b.y + b.height / 2, width: 0, height: 0};
            const owner = rects.filter(rect => contains(rect, center)).sort((a, b) => a.width*a.height - b.width*b.height)[0];
            if (owner && !contains(owner, b, -4)) problems.push('Cramped or clipped label: ' + label.text);
        }
        for (const path of svg.querySelectorAll('path.dg-edge')) {
            const edge = path.getBBox();
            const b = {x: edge.x - 2, y: edge.y - 2, width: edge.width + 4, height: edge.height + 4};
            for (const label of labels)
                if (intersects(b, label.box)) problems.push('Connector crosses label: ' + label.text);
        }
        const ids = [...svg.querySelectorAll('[id]')].map(node => node.id);
        for (const id of ids)
            if (document.querySelectorAll('[id="' + id + '"]').length !== 1) problems.push('Duplicate ID: ' + id);
        const box = svg.getBoundingClientRect();
        return {
            slug: el.dataset.diagram, label: viewport.getAttribute('aria-label'),
            keyboard: viewport.tabIndex === 0, title: svg.querySelector('title')?.textContent,
            description: svg.querySelector('desc')?.textContent,
            width: box.width, height: box.height, scale: box.width / svg.viewBox.baseVal.width,
            scroll: getComputedStyle(viewport).overflowX,
            viewport: viewport.clientWidth, content: viewport.scrollWidth,
            color: getComputedStyle(texts[0]).fill,
            font: Math.min(...texts.filter(node => !node.classList.contains('dg-tag')).map(node => parseFloat(getComputedStyle(node).fontSize))),
            problems,
        };
    })""")
    assert page.evaluate("document.documentElement.scrollWidth <= innerWidth + 1"), "Page overflow"
    for diagram in result:
        assert diagram["label"] and diagram["keyboard"], diagram
        assert diagram["title"] and diagram["description"], diagram
        assert diagram["width"] == 640 and diagram["height"] > 80, diagram
        assert 0.98 <= diagram["scale"] <= 1.02, diagram
        assert math.isfinite(diagram["font"]) and diagram["font"] >= 14, diagram
        assert diagram["scroll"] == "auto", diagram
        assert not diagram["problems"], diagram
        if width == 1440:
            assert diagram["content"] <= diagram["viewport"] + 1, diagram
    for viewport in page.locator(".diagram-viewport").all():
        if viewport.evaluate("el => el.scrollWidth > el.clientWidth + 1"):
            viewport.focus()
            viewport.press("ArrowRight")
            page.wait_for_function("el => el.scrollLeft > 0", arg=viewport.element_handle())
            viewport.press("ArrowLeft")
            page.wait_for_function("el => el.scrollLeft === 0", arg=viewport.element_handle())
    return result


with sync_playwright() as playwright:
    browser = playwright.chromium.launch(
        executable_path=shutil.which("google-chrome") or shutil.which("chromium"),
        headless=True, args=["--no-sandbox"],
    )
    # No renderer script, remote diagram service, or JS execution is required.
    context = browser.new_context(viewport={"width": 1440, "height": 1000}, java_script_enabled=False)
    context.route("**/simulator/**", lambda route: route.abort())
    context.route("**/fonts.googleapis.com/**", lambda route: route.abort())
    context.route("**/fonts.gstatic.com/**", lambda route: route.abort())
    page = context.new_page()
    for path, count in PAGES.items():
        assert page.goto(urljoin(BASE, path), wait_until="domcontentloaded").ok
        assert page.locator(".circuit-figure svg").count() == count
        page.evaluate("document.fonts.ready")
        check_geometry(page, 1440)
    context.close()

    context = browser.new_context(viewport={"width": 1440, "height": 1000}, color_scheme="light")
    context.route("**/simulator/**", lambda route: route.abort())
    page = context.new_page()
    page.set_default_timeout(30000)
    errors = []
    page.on("pageerror", lambda error: errors.append(str(error)))
    for path, count in PAGES.items():
        assert page.goto(urljoin(BASE, path), wait_until="domcontentloaded").ok
        wait_for_diagrams(page, count)
        colors = {}
        for scheme in ("default", "slate"):
            page.locator(f'input[data-md-color-scheme="{scheme}"]').evaluate("el => el.click()")
            page.wait_for_function("scheme => document.body.dataset.mdColorScheme === scheme", arg=scheme)
            for width in (1440, 640, 360):
                page.set_viewport_size({"width": width, "height": 1000})
                metrics = check_geometry(page, width)
                colors[scheme] = [diagram["color"] for diagram in metrics]
                print(json.dumps({"path": path, "scheme": scheme, "width": width, "diagrams": metrics}), flush=True)
                if SCREENSHOTS and width in (1440, 360):
                    page.evaluate("document.activeElement?.blur()")
                    for diagram in page.locator(".circuit-figure").all():
                        slug = diagram.get_attribute("data-diagram")
                        diagram.screenshot(
                            path=str(SCREENSHOTS / f"{slug}-{scheme}-{width}.png"),
                            style=".md-header, .md-tabs, .md-top { visibility: hidden !important; }",
                        )
        assert colors["default"] != colors["slate"], (path, colors)
        page.set_viewport_size({"width": 1440, "height": 1000})

    page.goto(urljoin(BASE, "rv32i/4.4/"), wait_until="domcontentloaded")
    wait_for_diagrams(page, 4)
    positions = page.locator('[data-diagram="single-cycle-stages"] [data-node]').evaluate_all("""nodes =>
        Object.fromEntries(nodes.map(node => [node.dataset.node, {x: node.getBBox().x, y: node.getBBox().y}]))
    """)
    stages = ["FETCH", "DECODE", "EXECUTE", "MEMORY", "WRITEBACK"]
    assert all(positions[a]["x"] < positions[b]["x"] for a, b in zip(stages, stages[1:])), positions
    assert len({position["y"] for position in positions.values()}) == 1, positions

    # Keep the diagram contract across site navigation. MkDocs may replace the
    # document even when navigation.instant is configured in this local build.
    link = page.locator('.md-tabs__link[href$="/memory/3.1/"]').first
    assert link.count(), "Memory navigation link not found"
    link.click()
    page.wait_for_url(urljoin(BASE, "memory/3.1/"))
    wait_for_diagrams(page, 2)
    for scheme in ("default", "slate"):
        page.locator(f'input[data-md-color-scheme="{scheme}"]').evaluate("el => el.click()")
        page.wait_for_function("scheme => document.body.dataset.mdColorScheme === scheme", arg=scheme)
        check_geometry(page, 1440)
    assert not errors, errors
    context.close()

    browser.close()
    print("PASS: 11 figures; light/dark; desktop/tablet/mobile; label/connector geometry; keyboard scrolling; stage order; site navigation; JavaScript and web fonts disabled", flush=True)
