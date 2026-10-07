"""Real-browser checks for the RV32I reference manual and its WASM embeds."""

import json
from pathlib import Path
import re
import shutil
import sys
from urllib.parse import urljoin, urldefrag

from playwright.sync_api import sync_playwright


def main():
    base = sys.argv[1].rstrip("/") + "/"
    screenshots = Path(sys.argv[2]) if len(sys.argv) > 2 else None
    if screenshots:
        screenshots.mkdir(parents=True, exist_ok=True)
    sections = ["Purpose", "Assembly syntax", "Encoding and decoding", "Operands",
                "Operation", "Exceptions and special cases", "Example"]
    widths = (960, 640, 360)
    with sync_playwright() as playwright:
        browser = playwright.chromium.launch(
            executable_path=shutil.which("google-chrome") or shutil.which("chromium"),
            headless=True, args=["--no-sandbox"])
        context = browser.new_context(viewport={"width": 1440, "height": 1000})
        page = context.new_page()
        page.set_default_timeout(60_000)
        errors = []
        page.on("pageerror", lambda error: errors.append(str(error)))

        def visit(path):
            print(f"Checking {path}", flush=True)
            response = page.goto(urljoin(base, path), wait_until="domcontentloaded")
            assert response and response.ok, path
            content = page.locator("article").text_content()
            assert "simulator-source:" not in content, path
            assert "simulator-program:" not in content, path

        def check_widths(path):
            for width in widths:
                page.set_viewport_size({"width": width, "height": 900})
                assert page.evaluate("document.documentElement.scrollWidth <= innerWidth + 1"), (path, width)
                for table in page.locator(".reference-table-scroll, .example-table-scroll").all():
                    if not table.is_visible():
                        continue
                    geometry = table.evaluate("""el => ({
                      tab: el.tabIndex, bounded: el.clientWidth <= innerWidth,
                      rows: el.querySelectorAll('tbody tr').length,
                      vertical: el.scrollHeight > el.clientHeight,
                    })""")
                    assert geometry["tab"] == 0 and geometry["bounded"], geometry
                    if geometry["rows"] >= 29:
                        assert geometry["vertical"], geometry
            page.set_viewport_size({"width": 1440, "height": 1000})

        visit("rv32i/4.1/")
        assert "RV32I Reference" in page.locator("h1").inner_text()
        links = page.locator('article a[href*="instructions/"]').evaluate_all(
            "links => links.filter(a => a.hash).map(a => a.href)")
        assert len(links) == len(set(links)) == 40
        families = {}
        for link in links:
            path, anchor = urldefrag(link)
            families.setdefault(path, set()).add(anchor)
        assert len(families) == 10
        check_widths("index")

        visit("rv32i/4.1/instructions/")
        catalogue = page.locator('article a').evaluate_all(
            "links => links.filter(a => a.hash && a.href.includes('/instructions/')).map(a => a.href)")
        assert set(links) <= set(catalogue)
        check_widths("instruction-index")
        for path, anchors in families.items():
            visit(path)
            entries = page.locator("article h2").evaluate_all("""headings => headings.map(h => {
              const sections = [];
              let codeBlocks = 0;
              for (let el = h.nextElementSibling; el && el.tagName !== 'H2'; el = el.nextElementSibling) {
                if (el.tagName === 'H3') sections.push(el.firstChild.textContent.trim());
                codeBlocks += el.querySelectorAll('pre code').length;
              }
              return {anchor: h.id, sections, codeBlocks};
            })""")
            assert {entry["anchor"] for entry in entries} == anchors, path
            for entry in entries:
                assert entry["sections"] == sections, entry
                assert entry["codeBlocks"] == 3, entry  # Syntax, operation, and example.
            # Following a mnemonic link must land at its own visible entry.
            anchor = sorted(anchors)[0]
            # Material's instant navigation may expand a fragment to a full URL.
            page.locator(f'article a[href$="#{anchor}"]').first.click()
            page.wait_for_function("anchor => location.hash === '#' + anchor", arg=anchor)
            assert page.locator(f"h2#{anchor}").is_visible()
            if screenshots and path.endswith("/arithmetic/"):
                page.screenshot(path=str(screenshots / "arithmetic-desktop.png"))
                page.set_viewport_size({"width": 360, "height": 900})
                page.screenshot(path=str(screenshots / "arithmetic-mobile.png"))
            check_widths(path)

        visit("rv32i/4.1/machine/")
        check_widths("machine")
        visit("rv32i/4.1/formats/")
        for marker, count in (("rv32i-decode-examples", 7), ("rv32i-encodings", 40)):
            table = page.locator(f'.reference-table-scroll[aria-label="{marker}"]')
            assert table.locator("tbody tr").count() == count
            if marker == "rv32i-encodings":
                details = table.locator("xpath=ancestor::details[1]")
                assert details.count() == 1 and details.get_attribute("open") is None
                assert not table.is_visible()
                details.locator("summary").click()
            assert table.is_visible()
        check_widths("formats")

        visit("rv32i/4.1/implementation/")
        text = page.locator("article").text_content()
        assert "Executable RV32I Reference" in page.locator("h1").inner_text()
        assert "RV32IDecodedInstruction RV32IDecoder::decode" in text
        assert "RV32IInstructionTrace stepImpl" in text
        assert "Core IO pins" not in text and "void RV32IReferenceCore::evaluate" not in text
        assert "ADD x5=13 pc=0x40->0x44 halted=0 trapped=0" in text
        assert page.locator("article details.example pre code").count() == 11
        source = page.locator("article details.example").first
        source.locator("summary").click()
        assert source.locator("pre").first.is_visible()
        check_widths("implementation")

        for marker, count in (("rv32i-step-examples", 40), ("rv32i-edge-examples", 14)):
            table = page.locator(f'.reference-table-scroll[aria-label="{marker}"]')
            assert table.locator("tbody tr").count() == count

        repository = Path(__file__).resolve().parent.parent
        benchmark_pages = [source for source in sorted((repository / "site-docs/rv32i/4.2").rglob("*.md"))
                           if "programs" not in source.parts]
        for source in benchmark_pages:
            relative = source.relative_to(repository / "site-docs").with_suffix("")
            route = str(relative.parent if relative.name == "index" else relative) + "/"
            visit(route)
            check_widths(route)
        visit("rv32i/4.2/")
        assert "Benchmarking and Verification" in page.locator("h1").inner_text()
        for scheme in ("slate", "default"):
            page.evaluate("scheme => document.body.dataset.mdColorScheme = scheme", scheme)
            check_widths("benchmark-index/" + scheme)
            if screenshots:
                page.screenshot(path=str(screenshots / f"benchmark-index-{scheme}.png"))

        visit("rv32i/4.2/programs/")
        program_links = page.locator('article a[href*="program-"]').evaluate_all(
            r"links => links.filter(a => /\/program-[0-9]+\/$/.test(a.pathname) && !a.hash).map(a => a.href)")
        count_source = (repository / "include/rv32i/RV32IValidationConfig.hpp").read_text()
        program_count = int(re.search(r"\bProgramCount\s*=\s*(\d+)", count_source)[1])
        assert len(program_links) == len(set(program_links)) == program_count, program_links
        assert {int(re.search(r"program-(\d+)/", link)[1]) for link in program_links} == set(range(1, program_count + 1))
        check_widths("program-catalogue")

        results = []
        for link in program_links:
            number = int(re.search(r"program-(\d+)/", link)[1])
            scenario = f"rv32i-program{number}"
            source = (repository / f"site-docs/rv32i/4.2/programs/program-{number:02}.md").read_text()
            expected = {key: int(value, 0) for key, value in re.findall(
                r"(?m)^\| `(PC|Attempts|HALTED|TRAPPED|TRAP_CAUSE)` \| `?([0-9A-Fx]+)`? \|", source)}
            visit(link)
            assert page.locator("article h2").evaluate_all(
                "heads => heads.map(h => h.firstChild.textContent.trim())") == [
                    "Purpose", "Setup", "Assembly", "Execution", "Expected result", "Simulation and checkpoints"]
            table = page.locator(f'.example-table-block[data-scenario="{scenario}"]')
            rows = table.locator("tbody tr").evaluate_all("""rows => rows.map(row => ({
              label: row.cells[2].textContent,
              detail: row.cells[3].textContent,
              time: Number(row.dataset.time), index: Number(row.dataset.timelineIndex),
            }))""")
            assert len(rows) == expected["Attempts"]
            element = page.locator(f'iframe[src*="scenario={scenario}&"]')
            element.scroll_into_view_if_needed()
            frame = element.element_handle().content_frame()
            frame.wait_for_function("document.body.dataset.circuitsimStatus === 'ready'")
            result = frame.evaluate("""async rows => {
              const core = app.components.get('RV32I_PROGRAM_ROOT.CORE');
              if (core.fidelity !== 'behavioral') throw new Error('Reference is not behavioral');
              for (const name of ['INSTRUCTION_MEMORY', 'DATA_MEMORY']) {
                if (app.components.get('RV32I_PROGRAM_ROOT.' + name)?.type !== 'MemoryEndpoint<ff,32>')
                  throw new Error('Reference program is not using an FF endpoint: ' + name);
              }
              if (app.components.get(app.rootId).childIds.length !== 4)
                throw new Error('Expected core, two endpoints and clock');
              const corePins = [...app.pins.values()].filter(p => p.componentId === core.id).map(p => p.name);
              for (const name of ['IMEM_REQ_ADDR', 'IMEM_RSP_READ_DATA', 'DMEM_REQ_VALID', 'DMEM_RSP_STATUS'])
                if (!corePins.includes(name)) throw new Error('Missing native CPU port: '+name);
              const pins = [...app.pins.values()].filter(pin => pin.componentId === app.rootId);
              for (const [i, row] of rows.entries()) {
                const expected = app.checkpoints[i];
                if (expected.time !== row.time || expected.detail !== row.detail)
                  throw new Error('Document/checkpoint mismatch ' + i);
                await setStateIndex(row.index);
                const pc = pins.find(pin => pin.name === 'PC');
                const expectedPC = parseInt(row.detail.match(/pc_after=0x([0-9a-f]+)/i)[1], 16);
                if (parseInt(app.state.pins[pc.stateIndex], 2) !== expectedPC)
                  throw new Error('Recorded PC mismatch ' + i);
                for (const name of ['HALTED', 'TRAPPED']) {
                  const expectedStatus = row.detail.includes(name.toLowerCase() + '=true') ? 1 : 0;
                  const pin = pins.find(pin => pin.name === name);
                  if (Number(app.state.pins[pin.stateIndex]) !== expectedStatus)
                    throw new Error('Recorded ' + name + ' mismatch ' + i);
                }
              }
              return {fidelity: core.fidelity,
                pc: parseInt(app.state.pins[pins.find(p => p.name === 'PC').stateIndex], 2),
                trapped: Number(app.state.pins[pins.find(p => p.name === 'TRAPPED').stateIndex]),
                halted: Number(app.state.pins[pins.find(p => p.name === 'HALTED').stateIndex]),
                cause: parseInt(app.state.pins[pins.find(p => p.name === 'TRAP_CAUSE').stateIndex], 2),
                count: app.checkpoints.length};
            }""", rows)
            assert result == {"fidelity": "behavioral", "pc": expected["PC"],
                              "trapped": expected["TRAPPED"], "halted": expected["HALTED"],
                              "cause": expected["TRAP_CAUSE"], "count": expected["Attempts"]}, result
            results.append({"scenario": scenario, **result})
            check_widths(link)
            if screenshots and number == 16:
                page.locator("#execution").scroll_into_view_if_needed()
                page.screenshot(path=str(screenshots / "program16-desktop.png"))
                page.set_viewport_size({"width": 360, "height": 900})
                page.locator("#execution").scroll_into_view_if_needed()
                page.screenshot(path=str(screenshots / "program16-mobile.png"))
                page.set_viewport_size({"width": 1440, "height": 1000})

        # The URL-only default must not become an IndexedDB preference.
        plain = context.new_page()
        plain.goto(urljoin(base, "simulator/?scenario=rv32i-program9"), wait_until="domcontentloaded")
        plain.wait_for_function("document.body.dataset.circuitsimStatus === 'ready'", timeout=120_000)
        assert plain.evaluate("app.components.get('RV32I_PROGRAM_ROOT.CORE').fidelity") == "structural"
        plain.close()
        assert not errors, errors
        print(json.dumps({"instructionEntries": len(links), "familyPages": len(families),
                          "benchmarkReferencePages": len(benchmark_pages),
                          "programPages": len(results), "programCheckpoints": sum(r["count"] for r in results),
                          "referenceEmbeds": results, "widths": widths,
                          "sourceListings": "expanded", "savedProfile": "unchanged"}))
        browser.close()


if __name__ == "__main__":
    main()
