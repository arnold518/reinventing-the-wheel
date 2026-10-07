# Test organization

The shared C++ registry owns executable component and RV32I scenarios. CTest,
the native visualizer and WebAssembly use the same scenario factories. Tests
that check loading, timing analysis, campaigns or frontend geometry remain
separate suites because they exercise software infrastructure rather than one
recorded component circuit.

## Visualizer catalog

`visualizer-v2/static/scenarios.json` supplies group names, component titles,
stable aliases and all 22 fixed-program titles. Python and JavaScript read this
same file. Display names are independent of C++ test IDs, scenario URLs and
saved layout/profile keys. An unclassified visible component fails the catalog
test instead of silently appearing in an arbitrary group.

The menu groups logic, wiring, ALU8, memory technologies and interfaces,
registers, ALU32, RV32I building blocks/stages, and developer circuits. Programs
are shown once in functional (01–08), fault (09–16) and performance (17–22)
groups. Separate selectors choose single-cycle, baseline or advanced execution
and the advanced core's five prediction policies. Every policy/program pair is
registered; existing URLs keep resolving to their original scenario identities.

Simulator-history checks and the ALU32 representative-slice verification stay
in CTest but are excluded from the menu: they do not build a visible root during
setup. Component examples retain their existing structural and behavioral
implementations, checkpoint rows and explorer fidelity controls.

The generated and compiled benchmark catalogs remain under §4.2 and the common
campaign runner. Their large executions are not automatically added to the
visualizer's precomputed scenario list. Fixed-program visualizer recording
clocks remain distinct from benchmark clock certification and performance data.

## Native and benchmark checks

```sh
ctest --test-dir build -N
build/tests/SimulationTestRunner --list
ctest --test-dir build -L benchmark --output-on-failure -j1
ctest --test-dir build -R 'ScenarioCatalog|TestRegistryCoverage|WebTestRunner' -j1
```

The benchmark label includes workload generation, campaign orchestration,
compiled/middle-suite manifest validation, recovery, differential equivalence,
clock/execution checks, documentation and the compatibility ELF CLI. These are
bounded regression tests; full Embench and middle equivalence campaigns are
explicit runs through `tools/benchmarks/run.py` and the background campaign tools.
The structural datapath and representative slices are retained.

## Browser and WebAssembly checks

Standalone checks live in three folders by how they are run: `tests/cpp/`
(C++ programs built by CMake), `tests/checks/` (Python and JavaScript run by
CTest) and `tests/web/` (browser and WebAssembly checks).

`tests/web-tests.json` inventories every standalone browser/WASM test, its
arguments, prerequisites, timeout and suite membership. `tools/run-web-tests.py`
executes them serially and writes per-test logs and `results.json` into a new
directory. The runner passes the selected artifact's example rows to browser
checks, preventing accidental validation against another build's examples.
GitHub Pages CI and cleanup browser validation use this runner.

```sh
python3 tools/run-web-tests.py --suite all --list
python3 tools/run-web-tests.py --suite smoke --browser-python /usr/bin/python3
python3 tools/run-web-tests.py --suite parity --build-dir build-cleanup \
  --wasm-dir build-wasm-cleanup
python3 tools/run-web-tests.py --test VisualizerScenarioBrowserTest \
  --wasm-dir build-wasm-cleanup --browser-python /usr/bin/python3
```

`smoke` is the Pages CI selection, `cleanup` includes the focused cleanup checks,
and `browser`, `parity` and `all` expose complete inventories. `--test` can select
any registered check. Parity checks require the selected native
`VisualizerSessionProbe`; browser checks require Playwright and the packaged
site. The runner can serve that site or accept `--base-url`. It stops on failure
unless `--keep-going` is supplied. A timeout terminates the test process group.
No test installs dependencies or downloads/builds compiled benchmark workloads.

`WebTestRunnerTest` guards registration completeness and execution failure/timeout
handling. `VisualizerScenarioCatalogTest` checks visible topology, all program
configurations, names, aliases, classification and Python/JS parity. The matching
WASM and browser tests check the compiled registry and actual selection flow.
The Pages packaging preflight also requires a complete root layout for every
registered fixed-program configuration (currently 176: three cores plus five
prediction policies, each with 22 programs). After adding program scenarios,
publish their reviewed layouts with
`tools/generate-advanced-pipeline-layout.py --program-roots-only` when only
policy root aliases are missing.
