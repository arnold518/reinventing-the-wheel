# Cache browser scenario

Started: 2026-10-15. Status: closed. Code state: `alpha` at `1d861c0`, with uncommitted cache implementation.

## Summary

The cache component scenario is registered, passes natively, and now runs
in the rebuilt WebAssembly visualizer. The local port 8893 preview serves
the same new WASM binary and scenario menu. A focused browser test checks
the documented rows, checkpoint replay and visible cache layout.

## 2026-10-15 — Investigation and action

`CacheComponentScenarioTest` and its `cache-read-hit` alias are present in
the source registry and `visualizer-v2/static/scenarios.json`; the packaged
`build-wasm/pages/simulator/scenarios.json` lacked that alias. The new
cache page's 12 example rows were temporarily generated from the native
`VisualizerSession`, which restored the [port 8893 documentation
preview](2026-10-15-pages-preview-example-recovery.md) but could not add a
scenario to the old WASM module.

The previous conclusion that Emscripten was unavailable is corrected here:
`emcmake` is installed at
`/home/arnold/.cache/circuitsim-emsdk/upstream/emscripten/emcmake`. It was
absent only from the shell's `PATH`. Activated the existing SDK and started
`tools/build-pages.sh` with a focused `CacheComponentScenarioTest` example
filter. Extended `MemoryEndpointBrowserTest.py` to include the cache page
and its recorded scenario. Build and browser results are pending.

## 2026-10-15 — Build and browser verification

The first serial WebAssembly build compiled all cache and test sources but
the linker could not write a lock file under the SDK cache because of the
filesystem sandbox. Repeating the link with approved host access succeeded.
`tools/build-pages.sh` packaged the simulator, verified its saved memory
layouts, exported `cache-read-hit` as 12 WASM-verified rows with profile
fingerprint `0445f98e83c802f0`, and passed the strict MkDocs build.

The first browser check loaded the cache scenario but failed a layout
assertion written for visible endpoint register cells; the cache scenario
deliberately uses compact behavioral child registers. Replaced that
endpoint-only assertion with a cache-specific check for structural root,
controller, tag/data stores and nonempty child geometry. The rerun of
`MemoryEndpointBrowserTest` passed 1/1, including the cache page, row
timeline, controls and inspector. This focused test is part of the smoke
suite; the entire smoke suite was not rerun.

The local port 8893 cache page, scenario JSON and WASM binary each returned
HTTP 200. The served scenario JSON and WASM binary were byte-for-byte equal
to the rebuilt package, and the served scenario menu contains
`cache-read-hit`. The Pages build and local preview are validated here;
the GitHub Pages deployment was not changed. The cache still has no fused
behavioral fidelity or RV32I integration.
