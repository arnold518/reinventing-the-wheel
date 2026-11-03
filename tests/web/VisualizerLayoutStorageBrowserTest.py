"""Serverless layout saves store only the viewer's edits in IndexedDB.

A save with no edits stores almost nothing; an edited color is stored as a
change, survives a reload and leaves every other entry on the shipped layout.
"""
import shutil
import sys
from urllib.parse import urljoin
from playwright.sync_api import sync_playwright

base = sys.argv[1].rstrip('/') + '/'

READ_RECORD = '''()=>new Promise((resolve,reject)=>{
    const request=indexedDB.open('circuitsim-visualizer-v2',1);
    request.onerror=()=>reject(request.error);
    request.onsuccess=()=>{const db=request.result;
        const get=db.transaction('state','readonly').objectStore('state').get('layout');
        get.onsuccess=()=>{db.close();resolve(get.result??null);};get.onerror=()=>reject(get.error);};
})'''


def ready(page):
    page.wait_for_function("['ready', 'error'].includes(document.body.dataset.circuitsimStatus)")
    assert page.locator('body').get_attribute('data-circuitsim-status') == 'ready'


with sync_playwright() as p:
    browser = p.chromium.launch(executable_path=shutil.which('google-chrome') or shutil.which('chromium'),
                                headless=True, args=['--no-sandbox'])
    page = browser.new_page(viewport={'width': 1600, 'height': 1000})
    page.set_default_timeout(90000)
    errors = []
    page.on('pageerror', lambda error: errors.append(str(error)))
    page.goto(urljoin(base, '?scenario=ripple-carry-adder-w8'))
    ready(page)
    page.evaluate('''()=>new Promise(resolve=>{const r=indexedDB.deleteDatabase('circuitsim-visualizer-v2');
        r.onsuccess=r.onerror=r.onblocked=()=>resolve();})''')
    page.reload()
    ready(page)

    # Saving without edits stores (almost) nothing.
    page.locator('#save-layout').click()
    page.wait_for_function("document.querySelector('#save-status')?.textContent === 'Layout saved'")
    record = page.evaluate(READ_RECORD)
    assert record and record['format'] == 'layout-changes-v1', record
    empty_size = len(str(record['layout']))
    assert empty_size < 4096, (empty_size, list(record['layout']))

    # Edit one component's color through the inspector, then save.
    target = page.evaluate('''()=>{const c=[...app.components.values()].find(c=>c.parentId&&c.childIds.length);
        setSelection('component',c.id);return {id:c.id,type:c.layoutType||c.type};}''')
    page.locator('#component-color-code').fill('#123456')
    page.locator('#component-color-code').dispatch_event('change')
    page.locator('#save-layout').click()
    page.wait_for_function("document.querySelector('#save-status')?.textContent === 'Layout saved'")
    record = page.evaluate(READ_RECORD)
    stored = str(record['layout'])
    assert len(stored) < empty_size + 4096, len(stored)
    assert '18, 52, 86' in stored or '[18,52,86' in stored.replace(' ', ''), stored[:2000]

    # The edit survives a reload; the record is still only changes.
    page.reload()
    ready(page)
    color = page.evaluate('id=>app.components.get(id).color', target['id'])
    assert list(color[:3]) == [0x12, 0x34, 0x56], color
    assert page.evaluate(READ_RECORD)['format'] == 'layout-changes-v1'

    assert not errors, errors
    browser.close()
    print(f'Layout storage: {empty_size} characters for no edits; color edit saved as a change and restored')
