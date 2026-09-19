"""Check exact memory C++ snippets and narrow-screen layout, without starting WASM."""
from pathlib import Path
import re
import shutil
import sys
from urllib.parse import urljoin

from playwright.sync_api import sync_playwright

ROOT = Path(__file__).resolve().parents[1]
base = sys.argv[1] if len(sys.argv) > 1 else "http://127.0.0.1:8893/"
sections = ("3.1.0", "3.1.1", "3.1.2", "3.1.3", "3.1.4", "3.1.5", "3.1.6", "3.2", "3.3")
count = 0
with sync_playwright() as p:
    browser = p.chromium.launch(executable_path=shutil.which("google-chrome") or shutil.which("chromium"),
                                headless=True, args=["--no-sandbox"])
    for section in sections:
        source = (ROOT / f"site-docs/memory/{section}.md").read_text()
        expected = re.findall(r"```cpp\n(.*?)\n```", source, re.S)
        context = browser.new_context(java_script_enabled=False, viewport={"width": 1440, "height": 1000})
        page = context.new_page()
        response = page.goto(urljoin(base, f"memory/{section}/"), wait_until="domcontentloaded")
        assert response.ok, section
        examples = page.locator("article h4").filter(has_text="C++ example")
        assert examples.count() == len(expected), section
        for heading, snippet in zip(examples.all(), expected):
            actual = heading.evaluate("""h => {
                for (let n=h.nextElementSibling; n && !/^H[1-4]$/.test(n.tagName); n=n.nextElementSibling) {
                    const code=n.querySelector('pre code');
                    if (code) return code.textContent;
                }
                return null;
            }""")
            assert actual is not None and actual.strip() == snippet.strip(), (section, heading.inner_text())
            count += 1
        assert page.locator(".simulator-embed iframe").count() == source.count('class="simulator-embed"')
        for width in (1440, 640, 360):
            page.set_viewport_size({"width": width, "height": 1000})
            assert page.evaluate("document.documentElement.scrollWidth <= innerWidth+1"), (section, width)
            assert page.locator("article .highlight").evaluate_all("""nodes => nodes.every(n =>
                n.getBoundingClientRect().right <= innerWidth+1)"""), (section, width)
        context.close()
    browser.close()
print(f"{count} exact C++ snippets on {len(sections)} memory pages; desktop/tablet/mobile layout passed")
