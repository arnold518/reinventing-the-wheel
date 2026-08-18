"""End-to-end browser contract for the packaged static WASM visualizer."""

from __future__ import annotations

import json
import shutil
import sys
from time import perf_counter

from playwright.sync_api import sync_playwright


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
        page = browser.new_page(viewport={"width": 1440, "height": 900})
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

              const memory = await request('api/circuit?scenario=memory-bit');
              const root = memory.body.components.find((component) => component.id === memory.body.rootId);
              const applied = await request('api/circuit/apply', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                  scenario: 'memory-bit',
                  revision: memory.body.profile.revision,
                  exactOverrides: { [memory.body.rootId]: 'behavioral' },
                }),
              });
              const stale = await request('api/circuit/apply', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                  scenario: 'memory-bit',
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
                rootSelectable: root.profileSelectable && root.availableFidelities.includes('behavioral'),
                applied: applied.ok && applied.body.components[0].fidelity === 'behavioral',
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
            """async () => {
              async function request(path, options) {
                const response = await window.visualizerRequest(
                  new URL(path, document.baseURI).toString(),
                  options,
                );
                return { ok: response.ok, body: JSON.parse(await response.text()) };
              }
              const adder = await request('api/circuit?scenario=adder8');
              const memory = await request('api/circuit?scenario=memory-bit');
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
                profilePersisted: memory.body.profile.revision === 1
                  && memory.body.components[0].fidelity === 'behavioral',
                restored: restored.ok,
              };
            }"""
        )
        assert all(persisted.values()), persisted
        assert not page_errors, page_errors
        visible["browserLoadMs"] = browser_load_ms
        visible["contractTestMs"] = (perf_counter() - contracts_started) * 1000
        visible["contracts"] = {**contracts, **persisted}
        print(json.dumps(visible, sort_keys=True))
        browser.close()


if __name__ == "__main__":
    main()
