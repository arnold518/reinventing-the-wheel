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

`GateInputCacheTest` is native regression equipment for the primitive gate
execution cache and disabled-history value buffers. It checks four-state
gate outputs/delay histories in every timing profile, pin replacement and
wrong-width fallback, rewiring, immediate release of replaced pins, and history
epoch resets. It is a standalone CTest target, not a visualizer scenario.
Changing IO/gate class layout requires rebuilding the full native archive and
its consumers before running it; old objects cannot be used as cache controls.

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

`ComputerSystemTest` is the bounded integrated guest baseline: a tiny
bring-up ELF on the `games-machine` description. Its cached, DRAM,
`TwoBus`, `RegisteredBridge`, wide-time and unsigned-time variants cover the
other machines and configurations; `ComputerSystemLockstepTest` compares
repetition, quantum size, an expanded core and fully structural logic around
scalable native memory, and `ComputerSystemFaultTest` the traps. The system
parts have `SystemDescriptionTest`, `BusInterconnectTest` (gate-level and
compiled routers under random traffic), `LedPanelTest`,
`GuestHeaderContentTest` and the CTest-only `GuestHeaderTest` (committed
guest headers equal `system_header` output). `ComputerSystemTimingTest`
certifies each machine's clock.

The guest fixtures in `tests/fixtures/rv32im/games` are built by
`tools/rv32im/build-guests.py --compiler <riscv-none-elf-gcc>`, which also
writes `src/runtime/RecordedGuestFixtures.generated.inc` (the ELF bytes and
inputs embedded for WebAssembly). Rebuild the native and WASM targets after
regenerating. There are three kinds:

| Fixtures | Screen | Input | Run by |
| --- | --- | --- | --- |
| `bringup`, `bringup-leds` | 8 × 8 | One press at a fixed cycle | The machine scenarios `ComputerSystem*ScenarioTest` |
| `<game>-short` | 32 × 24 | One or two presses at fixed cycles | The game scenarios `Game*ScenarioTest` |
| `<game>-long` | 64 × 48 | A computer player's batches, one after each frame | `LongGameplayProbe` and the long-game campaign |

`GuestScenarioTest` (`src/tests/GuestScenarioTests.cpp`) builds
`system.computer` with the scenario's build profile, loads the fixture and
calls `runRecordedGuestOn` with wire history on: the same checked runner as
the batch probe, so every instruction, data access, device read, frame and
console byte is checked while the visualizer gets the full timeline. Its
checkpoints are `reset released`, `press …`/`release …` (when the device
samples the input), `console: …`, `frame N` and `halted`. The clock follows the active timing profile:
100 ticks (`unit`) or 4.5 ns (`embedded`, `desktop`). The game scenarios
refuse Logic and Visual (with the core built from its stages a game's wire
history passed 3 GB natively, beyond a browser tab) and every whole computer
refuses Gate (its RAM is beyond the structural storage limit); `computer-system-structure` is the
program-free machine for switching profiles.

`system_experiment --guest NAME [--machine M] [--memory ...] [--fidelity ...]
[--period P] [--recording NEW.json]` runs one short fixture on any machine,
timing profile (`CIRCUITSIM_TIMING_PROFILE`) and period.
`tools/system/run-experiments.py` runs a set of them serially (`--machines
own` uses each fixture's own machine) and `tools/system/results-tables.py`
writes the result tables of § 5.6 and § 5.7 from the saved summaries.
`RecordedGuestProbe <guest> [ff|cached-ff|cached-dram] [max-cycles]
[quantum-events] [compact|expanded-core|structural] [width] [height]` exports
one checked run as JSON; `tools/rv32im/run-recorded.py` saves it with
provenance and resource evidence. `RecordedCircuitProbe <guest>
<new-output.json>` exports a short fixture's topology and PRESENT checkpoints
for the viewer's `?recording=URL` mode.

`RV32IMGameReferenceTest.py` compiles the same guest C for the host with
stand-in devices (`tests/fixtures/rv32im/games/reference-host.c`, driven by
`tools/rv32im/long_gameplay/reference.py`). For a short fixture
(`RV32IMRecorded_<fixture>_Test`) it runs the circuit through
`RecordedGuestProbe` and requires equal frames, console and report. For a
long fixture (`RV32IMReference_<game>-long_Test`, `--reference-only`) it
requires the computer player's predicted score, final state and rule
fingerprint. `RV32IMGameRulesTest` checks the rules on hand-made boards and
that `game_board` equals `game_cell`.

`RecordedCircuitTransportTest` covers the captured topology/state contract,
wide times, limits, mismatched frames and unsupported actions.
`RecordedCircuitBrowserTest` opens a packaged long-game playlist in the
original canvas and checks the retained pins and wires, display/input state,
inspector, abstraction toggle and timeline playback without a simulation
worker. Set `CIRCUITSIM_RECORDED_CIRCUIT_FILE` and
`CIRCUITSIM_RECORDED_CIRCUIT_URL` to check another capture. Run browser
checks serially under the host-memory guard.

### Long games: the background campaign

Each long fixture is a computer player's session
(`tools/rv32im/long_gameplay/plans.py`), frozen into the fixture by
`build-guests.py`: the player runs the real C rules through ctypes and stops
at its goal. `LongGameplayProbe FIXTURE NEW-DIR [--period P] [--fidelity F]
[--part-frames N] [--ticks] [--save-every CYCLES]` runs it through the checked
runner and writes the topology, one checkpoint per frame in parts, and the
result. `--save-every` also writes a machine save about every CYCLES cycles
to `saves/` ([machine state saves](../reference/machine-state-saves.md)), for
replays in the browser. Each run keeps `resume.bin`, a save with the results
so far, taken whenever a circuit part is written; `LongGameplayProbe FIXTURE
DIR --resume [same options]` continues an interrupted run from it.
`tools/rv32im/run-long-gameplays.py` runs all six serially in a background
user unit and publishes each finished game under
`site-docs/assets/system/long-games/<game>/` (playlist, parts, topology,
evidence):

```sh
cmake --build build-milestone --target LongGameplayProbe -j1
python3 tools/rv32im/run-long-gameplays.py launch \
  --runner build-milestone/tests/LongGameplayProbe \
  --output build-milestone/long-games-campaign            # compact, embedded, 4.5 ns
python3 tools/rv32im/run-long-gameplays.py status --output build-milestone/long-games-campaign
```

`launch` first runs each fixture's host reference (frames, report and
button levels per frame) and checks it against the player's prediction. The
unit freezes the runner, Node executable, fixtures, references, layout and
packaging tools; later repository edits do not change a running campaign.
The timing profile and period are frozen with it (`--timing`, `--period`).
One game runs at a time under a memory cap (`--memory-mib`, default 1024; the
native address space is five sixths of it), no swap, 90% of one CPU and
nice 10. It waits below 1,536 MiB available and stops only owned work below
1,024 MiB. Completed games receive exact full-frame, report, input-log and
per-frame button-level checks and validated transport parts before atomic
publication. `--games pong-long,...` selects a subset; `--fidelity` and
`--ticks` publish under a separate `<preset>[-ticks]` folder (Visual:
`long-games/visual/`, listed in the viewer's menu). Published games store
their topology, scopes, parts, layout and build manifest gzip-compressed
(`.json.gz`); the viewer decompresses them, and `compress-published --output
DIR` converts games published before. The `smoke-long` fixture (Pong's first
two updates) checks the whole chain in minutes; it runs only when named,
e.g. `--games smoke-long --fidelity visual --publication build-milestone/x`
(`--publication` keeps an equipment check out of the site). Visual runs at
about 100 cycles per second and peaked below 300 MiB on the smoke check;
use `--memory-mib 2048`.

Pass `--ticks` to record every signal-change timestamp instead of one state
per frame. It retains bounded compressed deltas and independent snapshots,
including memory bytes and register state; PRESENTs remain navigation
markers. Open its `ticks.json` rather than `playlist.json`.
`--trace-disk-gib` bounds the compressed trace bytes (default 12 GiB). The
native exporter links zlib.

`MachineStateResume_pong-short_Test` and `MachineStateResume_smoke-long_Test`
(`MachineStateProbe FIXTURE --save N --compare M [--fidelity visual]
[--out DIR]`) prove a save exact: an uninterrupted Visual run saves at
cycles N and M, a second run restores the save at N into a freshly built
machine and saves at M, and the two saves at M must be equal byte for byte;
the frames after N must also be equal. `--out` keeps the saves for size
measurements. `GameReplayScenarioTest` also runs `GameSessionProbe FIXTURE
SAVE`: a session stepped and viewed in pieces, and one that jumps back to its
save, must end byte-equal to a single run. Both take minutes (Visual runs at about 100 cycles per second).

`TickRecordingTest` runs `SignalTraceProbe` and compares every tick against
retained histories, including wide time, X/Z, future pin publications, memory
writes/fills, registers, cancellation, small drain quanta and chunk rewind.
`TickRecordingBrowserTest` requires `CIRCUITSIM_TICK_RECORDING_FILE` and its
same-site `CIRCUITSIM_TICK_RECORDING_URL` query; serve that capture alongside
the simulator. Without the file, `tools/run-web-tests.py` reports the check
as skipped. It checks the full timeline, PRESENT equality, intermediate
stepping, inspectors, Play and rewind without loading WASM. The fixture must
be produced by the native exporter; no full game run is required for this check.

`launch --save-at-frames --replay-window W` (or `--save-every N`) publishes
each game's saves gzip-compressed under `<game>/saves/` with
`saves/index.json`; the playlist names the index. A Visual recording with
saves then opens in the simulator (the `game-replay` scenario, Visual only):
the frame bar restores frame saves and ▶ simulates on from the frame event by
event. `GameReplayBrowserTest` checks it on a recording named by
`CIRCUITSIM_GAME_RECORDING` (the replay window is no longer used by the
viewer).

Use `stop` with the same output to stop the owned unit, and `resume` to start
the frozen queue again after stopping or reboot. Verified completed games are
retained and checked before skipping. An interrupted game continues from its
last resume save in the same attempt (at most one circuit part is simulated
again); a campaign frozen before resume saves existed restarts the game.
Partial attempts remain available and are never labeled verified. Pressure retries
are bounded to three attempts; other game failures do not skip later games.
Live progress and logs are in `campaign.json` and `jobs/` under the output.
These long sessions can take days; a healthy launch is not a completed run.

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
the same logical owners. See the [computer system reference](../reference/computer-system.md).
`RV32IMDeviceWasmParityTest` compares all 22 recorded component examples with
the native probe. `RV32IMDeviceBrowserTest` checks the 22 public M/device/system component
pages, mobile bounds and corresponding WASM checkpoint values. Run them through
`tools/run-web-tests.py --test NAME` against the chosen isolated artifacts;
neither check runs an integrated game or a live input session.
The component demos use descriptive roots and a shared structural inspection
profile. The browser check also verifies root/nested fidelity round trips,
saved `DUT` profile migration, caught explorer layout errors and visible
child bounds/overlaps. It always checks the existing `rv32i-core` as one
additional session; a scenario filter does not skip that core check.

Dedicated RV32IM programs **23–27** appear in the **RV32IM programs / Recorded
execution** group. The numbered URLs are `rv32im-program23` through
`rv32im-program27`; the first three retain their descriptive aliases
`rv32im-program-arithmetic`, `rv32im-program-dependencies` and
`rv32im-program-divide-edges`. Their registered tests are
`RV32IMArithmeticProgramTest`, `RV32IMDependenciesProgramTest` and
`RV32IMDivideEdgesProgramTest`, followed by `RV32IMFibonacciProgramTest`
and `RV32IMGcdProgramTest`. They use the shared program fixture with the
IM core, real memory endpoints and oracle-checked commits, retaining a
timeline for inspection. `setupCircuit()` builds topology only; recording
runs through `preparePrecomputedSimulation()` in both native and WASM.
The Fibonacci fixture uses fast doubling with MUL/DIVU; GCD uses Euclid's
loop with REMU/DIVU and a MUL quotient check. Both verify the output arrays
and actual retired M-operation counts. Run only these five for a small
program check:

```sh
ctest --test-dir build-cleanup -j1 --output-on-failure -L '^recorded-program$'
```

The RV32IM component recordings separately cover the eight M encodings,
signed/unsigned arithmetic boundaries and pause/hold/take/cancel/reset
transitions. Their concise timelines complement the larger headless sweeps.
`RV32IMInspectionBrowserTest` is a bounded check of multiplier/divider group
scopes, execute groups and the dependency program's structural view and
rewind, plus the Fibonacci/GCD programs, pipeline alignment and rendered M
instruction text. `CIRCUITSIM_INSPECTION_FILTER` optionally selects those browser
sessions by alias regex; report a filtered run's session count separately.

For a bounded browser run, `CIRCUITSIM_BROWSER_SCENARIO_FILTER` selects
recorded sessions by scenario-alias regular expression. All 22 public pages
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
supplies its menu title, group, explicit numeric `order`, stable URL alias
and `fidelity`, the named profile the visualizer opens it in (every program
opens in `visual`). Layout tools build a scenario through
`server.scenario_topology`, under that profile, because saved layouts are
keyed by its fingerprint. `VisualizerSessionProbe` opens a scenario in the
same profile unless given `--fidelity`; `--fidelity test` keeps the test's own
build profile, which `tools/run-web-tests.py` selects for its WebAssembly
parity checks (`CIRCUITSIM_PROBE_DEFAULT_FIDELITY=test`), matching the
bridge before a page chooses a preset.
Its group array defines the overall component sequence. Give components
consecutive ranks starting at zero within each group; fixed RV32I programs
use their program number.
Python and JavaScript sort from this metadata, and the final browser menu
preserves that order. Check the visible selector as well as backend parity.
A saved layout in
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
the generated example bundle is available. Catalog/order-only changes may
reuse an existing engine/example package: copy the changed catalog and option
builder into its simulator directory, build the site strictly and check the
actual browser selector. They require no engine compilation or new circuit
recordings. For scenario behavior, other browser behavior, WASM or
generated-example changes, build the full package with
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

Headless sweep tests use `circuit::test::SettledSweep` and `FidelitySweep`
(`include/tests/SettledSweep.hpp`). `FidelitySweep` builds the strict
gate-level form and, when the family has one, the behavioral form; every
vector runs on both and their outputs must match exactly. Expected values come
from an independent integer reference in the test, never from production value
functions. `requireSoundUnknown` checks a single X or Z input: an output bit may
be pessimistically `X`, but never a known value that the unknown bit could
change. `AdderWidthSweepTest` sweeps both adders at 8/16/32 bits (every carry
span at every start position, patterns, 1,024 random vectors and every
single-unknown input bit); `RV32IALUSweepTest` sweeps `RV32IALU`; and
`ALUWidthContractTest` adds a boundary grid and random vectors at 16/32 bits,
which reach every 16/32-bit sub-unit through the gate-level ALU. `WordGateSweepTest`
also checks per-bit output timing of the compact word gates against their gates
by sampling the output wire every tick in every timing profile (sample the
wire, not the pin: a basic part sets its pin at evaluation). Each new or
changed width needs such a sweep against an independent reference; a handful of
recorded example rows is not enough. Keep sweeps out of the scenarios that site
markers name, so published tables do not change.

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

`RV32IMDeviceBrowserTest` also inspects each distinct functional device scope
in the twelve Device I/O sessions, checks root group names and the menu order,
and rejects missing/out-of-bounds/overlapping child placements and explorer
errors. Its filter still selects sessions by alias. With screenshots enabled,
`*-hierarchy.png` records the circuit overview after nested-scope inspection.

`DelayRuleTest` rejects an explicit part selection when no parts were compared;
zero comparisons cannot establish a pass. Diagnostic `--report`/`--floors`
outputs describe measured conservatism and do not replace the normal test's
safety and floor checks. `DelayRuleSelectionTest` covers unmatched selection.

### Named fidelity presets

Normal native/WASM scenario loads accept `fidelity=scenario|gate|logic|visual|compact`
independently of timing. Manual overrides are isolated by preset and marked
custom. `FidelityPresetsTest` checks capacity boundaries, structural CPU and
compact visual devices, strict failures and per-file export budgets.
`VisualizerFidelityProfileTest` checks native/JS identities, cache isolation
and custom overrides; `VisualizerTimingProfileBrowserTest` also covers preset
URLs, timing independence and responsive selector placement.
`FidelityPresetWasmParityTest` uses the generic native probe
(`--fidelity PRESET`) to compare actual trees, profile identity and indexed
states for all four register presets and four visual device scenarios.

Long visual game recordings use schema 2 parts of at most 16 checkpoints with
a shared chunked topology. `RecordedCircuitTransportTest` covers both formats;
the browser checks a native capture. Preparing fixtures or launching a queue
is not evidence that its full native games passed.

`VisualizerProfileLayoutsTest` covers dynamic layout completion for newly
selected child structures, compatible placement reuse and preservation of
manual/unrelated saved entries. `DeviceSystemFidelityBrowserTest` switches
Logic/Visual/Compact on the actual device-system scenario,
checks repaired geometry and saved placements, and verifies readable strict
Gate rejection/recovery. A named preset may be unavailable for a contract;
that is distinct from a missing layout for a supported build.
