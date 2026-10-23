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
ctest --test-dir build-cleanup -j1 --output-on-failure
```

The direct runner is useful while editing one scenario. CTest also runs
standalone C++, Python and JavaScript checks from `tests/cpp/` and
`tests/checks/`. Use `ctest --test-dir build-cleanup -N` to see the actual
configured inventory. Timing or exact-edge claims need timing tests; settled
scenario rows alone check final values. Long benchmark campaigns are explicit
`tools/benchmarks/run.py` runs; a passing bounded benchmark CTest does not
stand in for a full campaign.

### Compiler memory for generated delay tables

An optional configure flag, `-DCIRCUITSIM_SPLIT_DELAY_TABLES=ON`, compiles each
profile's canonical generated table as eight smaller units. It requires
Python and changes only build organization: whole initializer entries are
copied verbatim into the build directory, retaining the same public table
functions and values. The default native path remains available without the
flag. `tools/build-pages.sh` enables partitioning for WebAssembly.

`DelayTableChunksTest` checks deterministic, lossless partitioning and malformed
input rejection. `DelayTableSnapshot` exports every compiled table field for
native/WASM value comparison; it does not recompute timing from gates.
Regenerating the canonical `.generated.inc` files triggers repartitioning at
the next CMake build. Continue compiling serially and keep host headroom;
smaller compiler units do not authorize parallel builds on this host.

The Emscripten cache option `CIRCUITSIM_WASM_LINK_OPTIMIZATION` selects the
main browser module's final optimization level (`0`, `1`, `2`, `3`, `s`, or
`z`; default `3`). `tools/build-pages.sh` forwards the environment variable
of the same name. A local memory-constrained build may select `1` after the
final optimizer exceeds its headroom. This does not change component fidelity
or timing tables. Record the selected level with artifact hashes and rerun
WASM/native parity; throughput measurements describe that build configuration.

Set `CIRCUITSIM_EXAMPLES_ISOLATE=1` for a memory-constrained Pages build.
The same example generator then runs each requested test group in a fresh
Node process, checking every row and recorded pin as usual. A full build
regenerates all groups; a filtered build preserves only the explicitly
unselected prior exports. The destination bundle is replaced after every
selected group succeeds. `ExampleRowsIsolationTest` checks byte-identical
full/filtered output and failure preservation using a tiny engine adapter.

### Scripted device and game checks

`RV32IMDeviceSystemTest` is the bounded integrated guest baseline. Its named
cached and wide-time variants cover the other supported configurations;
`RV32IMDeviceSystemLockstepTest` compares repetition, quantum size, an expanded
core and fully structural logic around scalable native memory.
`RV32IMRecordedBringupTest` and the five `RV32IMRecordedGame_*_Test` entries
use the same embedded ELF/input bytes as the portable batch probe. They turn
off circuit/memory histories and keep immutable frames separately. The small
`RV32IMDeviceSystemScenarioTest` is the recorded circuit example; its alias is
`rv32im-device-system`. `RecordedCircuitProbe <guest> <new-output.json>`
exports the actual game topology and bounded PRESENT checkpoints for the
original circuit viewer's `?recording=URL` mode. `games.html` remains available
for frame-only JSON files. The circuit capture observes current device state
without enabling histories; only retained checkpoints have pin/wire and device
observations. It does not offer intermediate gate transitions or RAM pages.

`RecordedCircuitTransportTest` covers the captured topology/state contract,
wide times, limits, mismatched frames and unsupported actions.
`RecordedCircuitBrowserTest` uses the real packaged Pong circuit capture in
the original canvas and checks every retained pin/wire, display/input state,
inspector, abstraction toggle and timeline playback without a simulation worker.

`RecordedGuestProbe <guest> [ff|cached-ff|cached-dram] [max-cycles]
[quantum-events] [compact|expanded-core|structural] [width] [height]` exports JSON;
`tools/rv32im/run-recorded.py` saves it in a fresh directory with provenance
and resource evidence.
`RV32IMGameReferenceTest.py` compares compiled guest reports/pixels with strict
host C reference execution. The independent host rule checks cover game
boundaries. These newly added system/game checks are under validation; consult
the open milestone record for passes, failures and unrun gates.

The exact-reference checks use the bounded recording CLI for each fresh native
run. They preserve JSON, RSS, progress and provenance under
`<build>/recorded-guest-native/<guest>-<unique>/`; a successful exact comparison
publishes `<build>/recorded-guest-native/<guest>.json` pointing to that artifact.
Existing artifacts are retained. Reference-only and `--recording` comparisons
do not execute another native guest.

`tools/rv32im/accept-recorded.py --runner PATH --output DIR` runs each of
the ten fixtures in compact baseline/repeat, compact quantum-97 and expanded
core configurations. It compares exact host references and deterministic
results, then saves performance/provenance summaries. Resuming requires the
same source, binary and fixtures. Use `--guest NAME` for a focused selection;
report that selection rather than claiming the full campaign.

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

`RV32IMTimingTest` separately certifies the new structural M-unit and IM-core
boundary under all three profiles. The new IM core's functional checks use a
fixed unit clock; they do not reuse the old I-core clock table. The scoped C
fixture can be rebuilt with
`python3 tools/rv32im/build-smoke.py --compiler /path/to/riscv-none-elf-gcc`.
Its compiler flags and artifacts are independent of the RV32I program path.
The phase 3 sweep checks are headless; `*ScenarioTest` aliases in
`src/tests/RV32IMPhase3Scenarios.cpp` retain smaller viewer examples and share
the same logical owners. See the [foundation reference](../reference/rv32im-device-foundation.md).
`RV32IMDeviceWasmParityTest` compares all 22 recorded component examples with
the native probe. `RV32IMDeviceBrowserTest` checks those public pages plus the
word-component page/ALU16 recording: 23 pages, mobile bounds and corresponding
WASM checkpoint values. Run them through
`tools/run-web-tests.py --test NAME` against the chosen isolated artifacts;
neither check runs an integrated game or a live input session.
For a bounded browser run, `CIRCUITSIM_BROWSER_SCENARIO_FILTER` selects
recorded sessions by scenario-alias regular expression. All 23 public pages
are still checked; report the selected session count separately. The default
runs every session. `CIRCUITSIM_DEVICE_SCENARIO_FILTER` similarly bounds the
Node device parity check, which still requires all 22 source example
markers and reports the selected count. `CIRCUITSIM_BROWSER_SCREENSHOTS` saves display/keyboard
previews (and the recording viewer's larger-screen views) to a chosen directory.

The visualizer scenarios are an exception: they also run under the
realistic profiles. Component scenarios mostly use settled checkpoints.
The RV32I program fixtures keep their `unit` times and, under another
profile, multiply every time by `rv32iFixtureTimeScale(core, unit_cycle)`,
the smallest whole factor that brings their cycle up to the certified clock
stored in `analysis::CERTIFIED_CORE_PERIODS` (`RV32IProfileSweepTest` checks
that table against `--period auto`). Fixed-time scenarios scale by gate
delays (`FullCircuitTest` by the slowest gate, `CacheTwoLevelScenarioTest`
by an inverter). To check them:
`CIRCUITSIM_TIMING_PROFILE=embedded ctest --test-dir build-cleanup -R "/program-|FullCircuitTest|CacheTwoLevelScenarioTest|RV32IBaselinePipelineCoreTest"`
(179 tests). A changed certified clock means updating the stored table.

`MemoryInspectorIntegrationTest` uses compact CPU cores with structural
arbiter/cache circuits for memory observation parity. It captures plain JSON
expected states and releases the Python circuit before constructing the native
comparison. The registered shared-memory program scenarios separately check
their default structural cores. This keeps observation checks bounded without
changing the product profiles.

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

### Wide-time and recorded-game web checks

`SimulationTimeWasmTest` executes the portable C++ time checks, then compares
exact large-tick playback with the native facade. `SimulationTimeBindingsTest`
checks the Python session's matching decimal-string transport. Their private
verification scenario is not added to the product menu.

For explicit portable verification, build the extra equipment serially:

```sh
cmake --build build-wasm --target circuitsim_verification circuitsim_recorded_guest circuitsim_delay_snapshot -j1
```

`RecordedGuestWasmParityTest` compares the same ten ELF/input runs with native
results, excluding host elapsed times. `RecordedGameViewerBrowserTest` checks
real captured pixels, exact time labels, unknown-bit rendering, empty records,
mobile bounds, superseded worker/file callbacks and a real bring-up run in its worker. The web runner checks for
the native RecordedGuestProbe and portable verifier when selected. These checks
are registered and pending execution in the milestone worktree.

For a completed native campaign, `CIRCUITSIM_NATIVE_GUEST_RECORDINGS` may point
to a directory containing `<guest>/baseline/recording.json` and `evidence.json`
from `run-recorded.py`. The parity check verifies the complete native source map,
native binary, fixture manifest, recording bytes, frame hashes and baseline
configuration before reusing each native result. A mismatch fails the check.
Without this variable the check executes all native guests before loading WASM.

`CIRCUITSIM_LARGE_NATIVE_RECORDING` may point to a `run-recorded.py` bring-up
directory created with `--width 97 --height 65`. The parity driver compares
that odd-width compiled run too; without the variable it captures the native
case before loading WASM. `CIRCUITSIM_WASM_GUEST_RECORDINGS` optionally saves
each successfully compared WASM record and hash receipt to a fresh directory.
Existing exports are never overwritten. This retains timing/capture data for
the acceptance report without another simulated game run.

### Width components

The component references in `site-docs/alu/2.1.md` through `2.6.md` specify
the Chapter 2 components, recorded mostly at 8 bits; the primary `ALU<W>`
recording is 16-bit. `site-docs/rv32i/4.3/alu.md` specifies `RV32IALU` and its
six parts. `DocumentationExamplesTest` checks the Chapter 2 examples and page
structure. `RV32IBuildingBlocksDocumentationTest` checks the RV32I ALU and the
other 4.3 references, including pin schemas and exact C++ reference replay.
`RV32ISingleCycleDocumentationTest` reads the 4.3 references before the stage
examples that call them. These checks use the existing native probe and
do not require a core or game campaign.

`WidthComponentsTest` covers `ALU<W>` and both adders at 8/16/32 bits, mux/register-file geometries, narrow transport/pixel helpers
and iterative M units at 4/8/16 bits. Recorded 32-bit M checks remain separate.
The same registered semantic test can run through `circuitsim_verification.js`.
`VisualizerScenarioWasmParityTest` compares the complete topology, examples,
states and rewind of the `alu` recording with native. `GuestDisplayGeometryTest` compiles the actual
guest drawing library for host reference IO at 64x48, 320x200 and odd/padded
row strides. This host test is distinct from compiled CPU execution.

### Parameterized devices and visual observations

`SynchronousFifoTest` checks depths 1–7 at widths 8/32 and compares
structural/compact results. `FramebufferDisplayTest` exercises larger and
non-power-of-two geometry, byte addressing, capacity and padding boundaries.
`RV32IMLargerDisplayTest` executes CPU stores and PRESENT beyond the original
4 KiB video range at 128×96 and 97×65, with independent frame checks.
`DeviceInspectorTest` verifies recorded pixel/input history, rewind and
read-only observation. `VisualizerDeviceViewTest` checks four-state keys and
RGB332 conversion, including 320×200 and padded-stride frames.
The structural 16-bit FIFO is covered by `WidthComponentsTest`.

The existing device browser/parity checks also compare observer JSON and
exercise device-view toggling and inspector previews. Run native and WASM
checks serially; browser tests must use a freshly packaged site. These
procedures do not imply that the pending milestone acceptance has passed.

`DelayRuleTest` rejects an explicit part selection when no parts were compared;
zero comparisons cannot establish a pass. Diagnostic `--report`/`--floors`
outputs describe measured conservatism and do not replace the normal test's
safety and floor checks. `DelayRuleSelectionTest` covers unmatched selection.
