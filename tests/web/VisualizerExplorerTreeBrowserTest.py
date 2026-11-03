"""Explorer tree as the planned fidelity; the canvas follows it for built parts.

Expanded means "build from parts" (structural), collapsed means "one block"
(behavioral); Apply & Run rebuilds to match and the read-only badge shows what
is built. Collapsing a structural part draws it closed at once (its symbol),
without a rebuild, and its inner parts are no longer hoverable. Expanding a
behavioral part marks it pending; its parts appear after Apply & Run. Revert
returns the tree to what is built. Hover reaches parts that came into view
after a camera move.
"""
import shutil
import sys
from urllib.parse import urljoin
from playwright.sync_api import sync_playwright, expect

base = sys.argv[1].rstrip('/') + '/'

SETTLED = '!app.geometryDirty && !app.cameraAnimation && !app.topologyRequestInFlight && !app.pendingScopeIds.size'


def frame_without_selecting(page, component_id):
    """Move the camera onto a part without selecting it (no geometry rebuild)."""
    page.evaluate('''id=>{
        const c=app.components.get(id),rect=componentFocusRect(c),viewport=cameraSafeViewport();
        animateCameraTo(fitZoomForRect(rect,viewport),viewportCenter(rect),viewportCenter(viewport));
    }''', component_id)
    page.wait_for_function(SETTLED)


def hover_center(page, component_id):
    point = page.evaluate('''id=>{
        const r=app.components.get(id).rect,box=canvas.getBoundingClientRect();
        const s=worldToScreen({x:r.x+r.w/2,y:r.y+r.h/2});
        return {x:box.left+s.x,y:box.top+s.y};
    }''', component_id)
    page.mouse.move(point['x'], point['y'])
    page.wait_for_timeout(200)
    return page.evaluate('app.hover')


def gate_row(page):
    """The top-level PROPAGATE word gate (inner carry cells reuse the name)."""
    page.locator('#explorer-search').fill('')
    row = page.locator('.explorer-row:has(.explorer-name:text-is("PROPAGATE"))').first
    row.wait_for()
    return row


def applied(page, gate_id, fidelity):
    page.locator('#apply-profile').click()
    page.wait_for_function('''([id,fidelity])=>document.body.dataset.circuitsimStatus==='ready'
        && app.components.get(id)?.fidelity===fidelity''', arg=[gate_id, fidelity])


def behavioral_picture(page, component_id):
    """Observe the actual canvas renderer with Device View off."""
    frame_without_selecting(page, component_id)
    rendered = page.evaluate('''id=>{
        const c=app.components.get(id),symbols=[],dashes=[];
        const symbol=drawLogicGateSymbol,dash=ctx.setLineDash;
        drawLogicGateSymbol=(component,...args)=>{symbols.push(component.id);return symbol(component,...args);};
        ctx.setLineDash=function(pattern){if(pattern.length)dashes.push(pattern);return dash.call(this,pattern);};
        try {drawComponentForeground(c,c.rect);}
        finally {drawLogicGateSymbol=symbol;ctx.setLineDash=dash;}
        return {enabled:deviceViewsEnabled(),symbols,dashes};
    }''', component_id)
    assert rendered == {'enabled':False,'symbols':[component_id],'dashes':[]}, rendered


with sync_playwright() as p:
    browser = p.chromium.launch(executable_path=shutil.which('google-chrome') or shutil.which('chromium'),
                                headless=True, args=['--no-sandbox'])
    page = browser.new_page(viewport={'width': 1600, 'height': 1000})
    page.set_default_timeout(120000)
    errors = []
    page.on('pageerror', lambda error: errors.append(str(error)))
    page.goto(urljoin(base, '?scenario=parallel-prefix-adder-w32'))
    page.wait_for_function("['ready', 'error'].includes(document.body.dataset.circuitsimStatus)")
    assert page.locator('body').get_attribute('data-circuitsim-status') == 'ready'
    assert page.locator('#toggle-device-views').get_attribute('aria-pressed') == 'false'

    gate = page.evaluate('''()=>{const c=app.components.get(app.rootId+'.PROPAGATE');
        return {id:c.id,type:c.type,fidelity:c.fidelity,children:c.childIds.length,
                first:c.childIds.find(id=>app.components.get(id).type==='XORGate')};}''')
    assert gate['type'] == 'XORGate<32>' and gate['fidelity'] == 'structural', gate
    assert gate['children'] == 35, gate  # 32 one-bit XOR gates and 3 bit adapters

    # Built structural and planned expanded: contents drawn, hoverable after a
    # camera move with no geometry rebuild.
    frame_without_selecting(page, gate['id'])
    assert page.evaluate('id=>componentOpen(app.components.get(id))', gate['id'])
    hovered = hover_center(page, gate['first'])
    assert hovered and (hovered['type'] != 'component' or hovered['id'] == gate['first']), (hovered, gate)

    # A view-only abstraction of a built structural word gate hides its scalar
    # gates, without editing the fidelity draft, recording, or layout.
    before = page.evaluate('''()=>JSON.stringify({session:app.sessionId,index:app.currentIndex,
        overrides:app.explorer.draftOverrides,layout:app.layout,state:app.state})''')
    page.locator('#toggle-device-views').click()
    page.wait_for_function('deviceViewsEnabled()')
    assert page.evaluate('id=>deviceViewComponent(app.components.get(id))', gate['id'])
    assert not page.evaluate('id=>componentOpen(app.components.get(id))', gate['id'])
    assert not page.evaluate('id=>componentDrawn(app.components.get(id))', gate['first'])
    assert hover_center(page, gate['first']) == {'type': 'component', 'id': gate['id']}
    page.evaluate('id=>smoothlyFocusComponent(id)', gate['first'])
    page.wait_for_function(SETTLED)
    assert page.evaluate('app.selection') == {'type': 'component', 'id': gate['id']}
    assert page.evaluate('''()=>JSON.stringify({session:app.sessionId,index:app.currentIndex,
        overrides:app.explorer.draftOverrides,layout:app.layout,state:app.state})''') == before
    assert gate_row(page).locator('.explorer-badge').inner_text() == 'S'
    page.locator('#toggle-device-views').click()
    assert page.evaluate('id=>componentOpen(app.components.get(id))', gate['id'])
    assert page.evaluate('id=>componentDrawn(app.components.get(id))', gate['first'])

    # Collapse: the canvas closes it at once (no rebuild); the badge still
    # shows what is built; the plan is one unapplied change.
    session = page.evaluate('app.sessionId')
    row = gate_row(page)
    assert row.locator('.explorer-arrow').inner_text() == '▾'
    assert row.locator('.explorer-badge').inner_text() == 'S'
    row.locator('.explorer-arrow').click()
    row = gate_row(page)
    expect(row.locator('.explorer-arrow')).to_have_text('▸')
    assert row.locator('.explorer-badge').inner_text() == 'S'
    assert 'pending' in (row.get_attribute('class') or '')
    assert page.locator('#explorer-status').inner_text() == '1 unapplied change'
    assert page.evaluate('app.sessionId') == session, 'collapsing must not rebuild'
    assert not page.evaluate('id=>componentOpen(app.components.get(id))', gate['id'])
    assert not page.evaluate('id=>componentDrawn(app.components.get(id))', gate['first'])
    assert hover_center(page, gate['first']) == {'type': 'component', 'id': gate['id']}

    # Revert: the tree returns to what is built and the canvas follows.
    page.locator('#revert-profile').click()
    expect(gate_row(page).locator('.explorer-arrow')).to_have_text('▾')
    assert page.evaluate('id=>componentOpen(app.components.get(id))', gate['id'])

    # Collapse and apply: one behavioral block, badge B, nothing to hover inside.
    gate_row(page).locator('.explorer-arrow').click()
    applied(page, gate['id'], 'behavioral')
    row = gate_row(page)
    assert row.locator('.explorer-badge').inner_text() == 'B'
    assert page.evaluate('id=>app.components.get(id).childIds.length', gate['id']) == 0
    behavioral_picture(page, gate['id'])

    # Expand a behavioral part: pending until Apply builds its parts. A
    # build-only preview lists them in gray, without badges.
    row.locator('.explorer-arrow').click()
    assert page.evaluate('id=>pendingStructural(app.components.get(id))', gate['id'])
    page.wait_for_function('id=>app.explorer.preview.get(id)?.childIds.length===35', arg=gate['id'])
    page.wait_for_function("document.querySelectorAll('.explorer-row.preview').length>=1")
    assert page.evaluate("()=>[...document.querySelectorAll('.explorer-row.preview .explorer-badge')]"
                         ".every(badge=>!badge.textContent)")
    assert page.locator('.explorer-row.placeholder').count() == 0
    assert gate_row(page).locator('.explorer-badge').inner_text() == 'B'
    behavioral_picture(page, gate['id'])
    applied(page, gate['id'], 'structural')
    assert page.evaluate('id=>app.components.get(id).childIds.length', gate['id']) == 35
    assert gate_row(page).locator('.explorer-badge').inner_text() == 'S'

    # A one-bit gate has one form: no arrow.
    leaf = page.evaluate('id=>app.components.get(id).name', gate['first'])
    page.locator('#explorer-search').fill(leaf)
    leaf_row = page.locator(f'.explorer-row:has(.explorer-name:text-is("{leaf}"))').first
    leaf_row.wait_for()
    assert leaf_row.locator('.explorer-arrow').is_disabled()

    assert not errors, errors
    browser.close()
    print('Explorer plan, outermost Device View, behavioral pictures with Device View off, solid outlines and hover passed')
