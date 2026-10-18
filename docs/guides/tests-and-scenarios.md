# Running tests and scenarios

Read this guide before changing a component test, a visualizer scenario, or a
public example. The maintained inventory and suite boundaries are in the
[test organization reference](../reference/test-organization.md); the steps
for a new component are in [adding a component](adding-a-component.md).

## Find the existing coverage

Start with the component's contract ID and source. Search
`src/tests/TestRegistry.cpp` for its logical test, then read that test's
`ComponentTestSpec` and `ActionScenario` in `src/tests/`. The same registry
serves CTest, `create_test_by_name` in Python, and WebAssembly. The matching
name must appear in `tests/CMakeLists.txt` under `SIMULATION_TESTS`.

```sh
rg -n 'MyContract|MyTest' src/tests include/tests tests/CMakeLists.txt
build-cleanup/tests/SimulationTestRunner --list
```

Use `ComponentScenarioTest` for named, repeatable input and expected-output
checkpoints. The runner builds every available fidelity and compares their
public outputs. A general `SimulationTest` may construct a multi-component
fixture, but only a component scenario with declared actions can generate the
site's example table. A test called an oracle or benchmark has its own
verification boundary; do not infer gate-level equivalence from it.

## Run the narrowest useful check

Build serially to stay within this host's memory budget. Use the isolated
build so the live Python extension is not overwritten.

```sh
cmake -S . -B build-cleanup -DCIRCUITSIM_DEPLOY_PYTHON=OFF
prlimit --as=4294967296 cmake --build build-cleanup -j1
build-cleanup/tests/SimulationTestRunner MyTest
ctest --test-dir build-cleanup -R '^MyTest$' --output-on-failure -j1
ctest --test-dir build-cleanup -j2 --output-on-failure
```

The direct runner is useful while editing one scenario. CTest also runs
standalone C++, Python and JavaScript checks from `tests/cpp/` and
`tests/checks/`. Use `ctest --test-dir build-cleanup -N` to see the actual
configured inventory. Timing or exact-edge claims need timing tests; settled
scenario rows alone check final values. Long benchmark campaigns are explicit
`tools/benchmarks/run.py` runs; a passing bounded benchmark CTest does not
stand in for a full campaign.

### Timing profiles

The native suite runs under the `unit` timing profile (the code default:
one tick per gate). Its scenarios and timing tests are written in `unit`
ticks — clock half periods, reset releases, exact edge times — so running
the whole suite with `CIRCUITSIM_TIMING_PROFILE=embedded` is not a valid
check: most of it then fails for that reason alone. Realistic profiles are
covered by tests that choose their own profile: `RV32IProfileSweepTest`
(every core on a sample of programs at its certified `embedded` and
`desktop` clock, oracle-checked), `RV32ITopologyTest` (the cache campaign's
memory systems at `embedded`), the per-profile table checks
(`PartDelayTablesComplete_embedded_Test`, `..._desktop_Test`), and the
experiment and benchmark runners. A new test of timing under realistic
delays should set its profile itself and derive clocks from the analyzer's
certified period, not from tick constants.

## Connect a scenario to the visualizer and site

The registry name identifies the test. `visualizer-v2/static/scenarios.json`
supplies its menu title, group and stable URL alias. A saved layout in
`visualizer-v2/layout.json` may depend on root IDs, child names, the selected
profile and the scenario alias; refresh affected layouts with the
[layout guide](automatic-layout-refresh.md). Do not silently rename a saved
identity without checking its users.

The browser only exposes registry entries marked visualizable. Contract
evidence attaches metadata to many component tests automatically; a standalone
scenario may need an explicit kind and visualizable entry in `TestRegistry.cpp`.
Check its descriptor and run `VisualizerScenarioCatalogTest` after adding or
renaming an alias.

Public component examples use a marker such as
`<!-- simulator-examples: my-scenario MyTest -->`. The build generates its
rows from the test's checkpoints; never hand-edit those rows. Keep the page's
C++ example as an independently understandable behavioral reference, then
check it against the scenario when that page has a compilation check.

For a Markdown-only site change, run `python3 -m mkdocs build --strict` when
the generated example bundle is available. For a scenario, browser, WASM or
generated-example change, build the full package with
`tools/build-pages.sh`, then run the relevant registered browser check.
`tests/web-tests.json` is the inventory; `tools/run-web-tests.py --suite all
--list` shows choices. The focused runner form is:

```sh
python3 tools/run-web-tests.py --test VisualizerScenarioBrowserTest \
  --wasm-dir build-wasm-cleanup --browser-python /usr/bin/python3
```

Report which native, site and browser checks actually ran. A native pass does
not verify the published WebAssembly page.
