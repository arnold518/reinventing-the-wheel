# Changelog

Record meaningful completed changes here, newest first. Each entry states
what changed and why, what was verified, and any limits or deliberate
omissions. Entries begin with adoption of this recording workflow; older
history remains in Git and the archived reports.

## 2026-11-08

### Compact long games published with a save after every frame

- The Compact core (`RV32IMCoreBehavioral`) declares its state, so a Compact
  machine saves and restores; the six Compact recordings were re-recorded
  with a save after each frame (35 MB of saves, frames unchanged) and now
  open in the page's simulator with main memory inspectable, like Visual.
- Fixed: restoring a version 2 save passed a save callback without a save
  point, which the runner refuses.
- `RecordedCircuitBrowserTest` keeps checking the frames-only viewer by
  hiding the saves index.
- Checks: all six games passed the campaign's host-reference checks with the
  same frames as before; `MachineStateResume_pong-short-compact_Test`;
  `GameReplayBrowserTest` on Compact and Visual Pong and
  `RecordedCircuitBrowserTest` on preview 8893; strict MkDocs build
  (preview configuration). Not run: the full CTest and web suites.

### Visual long games published with a save after every frame

- The six Visual recordings were re-recorded with a machine save after each
  frame and replace the set without saves (120 MB: 34 MB of frames, 86 MB of
  saves); a Visual recording now opens in the page's simulator.
- Fixed: the campaign's finalize read a peak memory from every capture's
  resource file and failed on an interrupted capture's empty one (Pong);
  Pong was finalized from its finished attempt with the fixed script.
- Checks: every game passed the campaign's host-reference checks;
  `GameReplayBrowserTest` passed on the published Visual Pong. Not run: the
  full CTest and web suites.

## 2026-11-06

### Long games: one menu entry per game, Fidelity picks Compact or Visual

- The recording menu lists each long game once; inside a recording the
  Fidelity menu offers the fidelities that game was recorded in and opens the
  other recording at the same frame (`&frame=N`). Replay buttons share one
  row, and the scenario menu names an open replay.
- Checked: Pong Compact → Visual at frame 6 and back on preview 8893;
  `VisualizerScenarioBrowserTest` passes.
- The time readout is exact: every digit down to the picosecond in the
  largest unit reached (`1.813 860 041 ms`), never rounded through a Number;
  in a recorded game the second line shows the clock cycle and the time into
  it (`cycle 403,077 + 41 ps`). It was rounded to three digits, so events a
  few picoseconds apart read the same.

### The six long games published in Visual

- All six long sessions recorded with the core built from its stages and
  published under `site-docs/assets/system/long-games/visual/` (34 MB
  compressed), listed in the viewer menu through `recordings.json`. § 5.6
  gains "The same sessions in Visual" with a generated table
  (`results-tables.py`: `long-games-visual`; host times over 90 minutes are
  shown in hours).
- Checks: every game passed the campaign's full-frame, report, input-log and
  button-level checks against the host reference; `RecordedCircuitBrowserTest`
  passed on the Visual Pong recording. About 25 hours of simulation; the
  first attempt stopped on the event budget (fixed 2026-11-04) and Pong was
  restarted once after a manual reboot.
- See [the Visual long-games record](records/2026-11-03-visual-long-games.md).

### Save and restore a whole machine; replays of long games

- New `circuit::state` save format (`include/simulator/MachineState.hpp`,
  [reference](reference/machine-state-saves.md)): simulator time, queue and
  counters, every wire and pin value, each evaluated part's timing state and
  own members. Every part type declares its state (`stateSupport`); an
  undeclared part makes saving fail by name. All 36 leaf kinds of the Visual
  game machine are declared; the Compact core and non-FF memories are not.
- The checked runner saves at the top of chosen cycles (machine only or with
  results), resumes from a save into a fresh machine, and can stop after a
  number of cycles (saving at the stop). `LongGameplayProbe` gains
  `--save-at-frames`, `--save-every`, a resume save at each circuit part and
  `--resume`; `run-long-gameplays.py launch --save-at-frames --replay-window W`
  publishes saves with an index, and an interrupted game continues from its
  resume save instead of cycle 0.
- A Visual recording with saves opens in the simulator: the machine is
  restored from the shown frame's save (every wire, register and memory word
  inspectable). A frame bar spans the game (|◀ ▶| and dragging restore frame
  saves); the event bar under it spans what has been simulated since the
  frame, and ▶ and Play simulate 20 more cycles when they reach its end.
  ▶ crosses frames like any other event (it never restores a save); a
  session keeps a rolling window of the last 600–900 cycles of history
  (`Simulator::trimHistoryBefore`) and stops at the program's halt.
  Opening or jumping to a frame simulates its first 20 cycles at once.
  An advance sends only the change to the timeline (`dropped`, `front`,
  `added`), found from the end of each history, instead of the whole list. The
  viewer's script versions are bumped so browsers load the new scripts.
  `GameReplayScenarioTest` (`game-replay`, hidden from the menu) is the
  session; `VisualizerSession::gameAdvanceJson/gameJumpJson` and the worker's
  `/api/game/*` routes drive it. Run saves are version 2 (machine section with
  its length, for continuing in place); version 1 saves are still restored.
- Why: frame recordings hold one state per frame and no main memory;
  storing every event would be about 50 GB. A save is about 80 KB gzip, and a
  replay recomputes the events exactly.
- Checks run: `MachineStateProbe` (Pong short 2,500→5,000 and smoke-long
  20,000→22,000 in Visual: saves byte-equal; a stop-time save equals the
  uninterrupted top-of-cycle save), `GameReplayScenarioTest.py` with
  `GameSessionProbe` (stepped, viewed and jumped sessions end byte-equal to
  one run), a smoke campaign with saves (all host-reference checks), a
  stopped and resumed smoke-long recording equal to the uninterrupted one,
  and `GameReplayBrowserTest` on the staged Sokoban recording. Not run: the
  full CTest and web suites.

## 2026-11-04

### Long-session event budget large enough for Visual

- Long game sessions allowed 20 billion simulator events, enough for
  Compact (about 300 events per cycle) but only about 1.1 million cycles in
  Visual (about 17,800 per cycle). The first Visual campaign stopped on it at
  2048. The budget is now 400 billion, which covers the 20 million cycle
  budget in Visual; the cycle budget remains the runaway guard.
- Checked: `LongGameplayProbe` rebuilt; the Visual campaign was relaunched
  with it. The campaign itself has not finished. No other tests were rerun:
  the change affects only the long fixtures' event budget.
- See [the Visual long-games record](records/2026-11-03-visual-long-games.md).

### Recording menu no longer requests a missing file (Pages smoke failure)

- Every simulator page asked for the Visual recordings' manifest before any
  Visual campaign existed; the 404 failed `RV32IBuildingBlocksBrowserTest`
  in the Pages workflow. The menu now reads an index of published recording
  sets (`assets/system/long-games/recordings.json`), which a campaign extends
  when it launches. Checks: the smoke suite passes locally (14 of 14).

## 2026-11-03

### Compressed recordings; Visual long-game campaign prepared

- Published long games store their topology, scopes, parts, layout and build
  manifest gzip-compressed; the viewer decompresses them. The six Compact
  recordings went from 58 MB to 3.5 MB (`compress-published`). The menu also
  lists Visual recordings when published.
- New `smoke-long` fixture (Pong's first two updates) and `--publication`
  option for equipment checks. A Visual smoke campaign passed (54,519
  cycles, 99 cycles/s, 293 MiB peak, 2.9 MB published) and its recording
  opens in the viewer with the core's stages.
- The six-game Visual campaign is ready (about 25 hours); not started.
- Checks: the full native suite (608 of 611; three stale checks fixed and
  rerun, 9 of 9 related tests pass; the complete browser suite was not
  rerun), `RecordedCircuitBrowserTest`
  on compressed Sokoban and Pong and on the Visual smoke recording, host
  references of all 15 fixtures (the six games' fixtures unchanged).

### Recordings open like the live visualizer; Logic/Visual refusal explained

- The recording view's header readouts no longer overflow, nothing is
  selected on opening, and its scenario menu is the full catalog plus the six
  long games (also listed in the live menu); choosing across modes reloads
  the page. The separate `games.html` viewer and its check were retired.
- The short games keep refusing Logic and Visual, now for the measured
  reason: with a gate-built core Pong's short version passed 3 GB of history
  within 100 s natively (a browser tab holds 2 GB). The message points to
  `computer-system` in Visual, which runs in 55 s and 386 MiB natively.
- Plan added for Visual close-ups of the long games
  (`docs/records/2026-11-03-long-game-closeups-plan.md`); not implemented.
- Checks: recording and menu browser checks, device-view and transport unit
  checks, live/recording switching in the browser.

### Games in two versions: short scenarios and long recordings; stronger device scenarios

- Each game now has a **short version**, a live scenario in the new menu group
  **Computer System / Games** (`game-2048`, `game-sokoban`, `game-snake`,
  `game-pong`, `game-tetris`, `game-snake-leds`). Each starts one or two
  presses from its key event (a merge, the winning push, a bite, a bounce, a
  line clear, a crash) on a 32 × 24 screen. It also has a **long version**:
  a computer player's goal-based session (2048 to the 128 tile, two Sokoban
  puzzles, Snake to length 10, Pong to two points, Tetris to 20 pieces) on
  64 × 48, recorded at every frame by the migrated long-game campaign.
- New `GuestScenarioTest` runs a guest program as a visualizer scenario
  through the same checked runner (`runRecordedGuestOn`, with wire history
  and timeline events), so every instruction, data access, device read,
  frame and console byte is checked. The five `ComputerSystem*ScenarioTest`
  machines now run the bring-up program (8 × 8 screen); the old program-free
  example is `computer-system-structure`. Games refuse Logic and Visual and
  whole computers refuse Gate (new `unavailableFidelityPresets` hook).
- The display, timer, buttons and LED scenarios were strengthened: a 4 × 4
  picture with a byte-masked pixel and two frames; divisor 4 and a declared
  222 MHz; a queue overflow and its clearing; eight cycles of blinking pins.
- Guest library: `rect` no longer compares 64-bit values per word,
  `game_board` replaces per-cell rule queries, the game state is cleared in
  words, and the cell size follows the display. Same pixels; the 2048 merge
  on 32 × 24 went from 23,117 to 7,649 instructions. Short runs skip the
  rule fingerprint.
- Fixtures: `bringup`, `bringup-leds`, six `-short` and six `-long`; the ten
  3-press and acceptance fixtures, their recordings, `prepare-long-gameplays.py`,
  `accept-recorded.py`, the demonstration scripts and the separate long guest
  were retired. `LongGameplayProbe` and `run-long-gameplays.py` take fixtures
  by name with a frozen timing profile and period (default embedded, 4.5 ns,
  Compact) and publish under `assets/system/long-games/`.
- § 5.6 rewritten (both versions, figures `game-versions` and `game-boards`
  drawn from the host frames), § 5.5 examples, § 5.4 device examples, § 5.7
  method, the visualizer page and the tests guide updated.
- Checks run: native and WASM builds; all 11 guest scenarios, the structure
  scenario and the four device scenarios (unit timing; the checked machine
  also at embedded 4.5 ns); host references of all 14 fixtures and circuit
  equality of the 8 short ones; rules, geometry, diagram and catalog
  checks; static layouts of all 387 scenarios (0 problems); the strict
  preview build; `GameScenarioBrowserTest` (new; Pong runs in the page in
  47 s), `RV32IMDeviceBrowserTest`, `DeviceSystemFidelityBrowserTest`,
  `DocumentationDiagramsBrowserTest` and `RV32IMDeviceWasmParityTest` (now
  also the bring-up machine and Pong, exact). The § 5.6 and § 5.7 tables come
  from new embedded and unit runs. Long games: all six passed and are published
  (0.67–2.18 M cycles each, 5 h 23 min native in all, 58 MB);
  `RecordedCircuitBrowserTest` passes on Sokoban. The full native and browser suites were not rerun.

### Full native and browser suites pass; stale web checks and layout gaps fixed

- Full native suite (`build-milestone`, `-j1`): 607 of 607 pass. Complete
  browser suite (`tools/run-web-tests.py --suite all` on the 8893 preview):
  38 of 49 passed first; the 11 failures were fixed and rerun with the four
  checks the fixes touch. Result: 48 pass, `TickRecordingBrowserTest`
  skipped (it needs an external capture, now declared as `requires_env` in
  `tests/web-tests.json`; the runner reports such checks as skipped).
- Stale checks updated: the device parity check reads the § 5 chapter
  (28 scenarios); the diagrams check follows `basic/1.2/` and now also
  covers § 4.6 and the five § 5 figures; the URL, Rewire, component-count,
  compact-availability, SRAM-placement and evidence-text expectations match
  current behaviour.
- Layouts: `rv32i-alu-result-select`, `rv32i-memory-request` and
  `decoder-address5` get root entries for the Gate profile they now open in
  (copied from their previous profile's entry, same children);
  `matrix-column-mux`'s plain entry loses proportions from before its
  reflow. `tools/check-static-layouts.py` now models the browser's start
  from a scenario's plain root entry; it reports 0 problems in 380
  scenarios.
- `profiles.defaults.json`: two no-op Gate overrides removed
  (`flip-flop-cell`, `rv32i-register-file`); they changed the profile
  fingerprint so the page no longer matched its example rows.
- `games.html` defaults to Compact again (the recordings' profile); in Visual
  the bring-up run exceeded the event budget. Other profiles stay
  selectable.
- Figures: `cache-topologies` small boxes enlarged, `system-overview`
  address notes moved below the drawing, `led-panel`'s `BLINK` tag moved
  off its connector.
- Checks after the fixes: every previously failing browser check plus
  `RegisterFileAndCellsBrowserTest`, `FFMemoryBrowserTest`,
  `FFMemoryWasmParityTest` and `RV32IMemoryBrowserTest` pass; native
  `VisualizerStaticLayoutTest`, `WebTestRunnerTest`,
  `DocumentationDiagramsTest`, `DocumentationExamplesTest` and the three
  profile tests pass. Not rerun after the fixes: the full native suite (no
  C++ changed) and the full browser suite as one run.

## 2026-11-02

### Device status choices on word gates; Device I/O record closed

- The register front end's three-bit status choices and the router's
  `DeviceStatusMux` groups are built by one helper (`detail::wordChoice`):
  select inverter, two Rewires spreading the select, two `ANDGate<3>` and one
  `ORGate<3>`, instead of per-bit scalar AND/AND/OR (there is no `Mux<2,3>`).
  Same gates and depth per bit, so timing is unchanged. This was the console
  audit's last open item; the Device I/O record is closed.
- Checks: native build; 23 targeted device, bus and machine tests; the ten
  clock certificates unchanged; bring-up re-recorded with identical results
  and recorded circuit; `DeviceStatusMux`/`RegisterStatus<…>` layouts
  reflowed (static check 0 problems in 15 scenarios); WebAssembly rebuilt and
  the 15 affected example groups regenerated (identical);
  `RV32IMDeviceBrowserTest` and `DeviceSystemFidelityBrowserTest` pass.
  Not rerun: the complete browser suite. § 5.4.1 updated.

### Visualizer: named default profiles; § 5 layout and colour review

- **No "Scenario default".** Every scenario opens in a named fidelity
  profile from its `fidelity` in `visualizer-v2/static/scenarios.json`, the
  named profile whose build is closest to the scenario's former curated build
  (part counts compared for all four): Gate 132, Logic 49, Visual 192 (all
  CPU programs, the § 4.7 arrangements, the machines), Compact 7. A first
  choice by kind (Gate for parts, Logic for cores) made 47 scenarios up to
  16× larger than before and was replaced. Switching scenario returns to its profile;
  another choice is kept in the URL. Explorer profiles are keyed
  `<scenario>::<preset>`; older keys move to the default on load
  (`profiles.defaults.json` migrated). Six fixed scenarios are shown as built.
  C++ keeps an empty preset (the test's own profile) for tests and example
  rows only; the probe defaults to the catalog profile (`--fidelity test` or
  `CIRCUITSIM_PROBE_DEFAULT_FIDELITY=test` for WebAssembly parity checks).
  Layout tools build through `server.scenario_topology`.
- **Review.** All 258 parents in the § 5 scenarios measured from the
  browser's own placements: floorplans for device roots, the bus, the
  register port, FIFO, counter and RV32IM decode control, with every part
  centred in its slot; distinct shades for 20 same-coloured families;
  `RegisterFrontEnd` grouped into `DECODE`, `CHECKS`, `WRITES`, `READS` (same
  gates). Docs: § 5.4.1, the visualizer page, the profile and catalog
  references, the guides and `AGENTS.md`.
- **Layout keys:** compact device groups lay out as `…[config-compact]` with a
  `CompactModel<…>` child; the DRAM and NOR-flash command controls are
  structure-keyed; `CHECKS` split into `PAYLOAD` and `STATUS`. Static check:
  0 problems in 380 scenarios; § 5 review: 0 overlaps, 0 same-coloured
  children, 0 uncoloured parts.
- **Recorded games and examples:** the recorded-game viewer no longer looks
  for a catalog profile (it was failing on 8893); example rows are recorded
  under each scenario's catalog profile; § 5.5 has an example per machine
  variant.
- **Full native suite** (one clean run): 15 of 615 failed, all stale tests,
  lists or pages (retired tests still in CMake, CTest order, an ALU
  expectation predating the RV32I ALU compact forms, § 2.6's fidelity table,
  layouts for the new profiles, a probe option order, the RV32I toggle audit);
  each fixed and rerun individually.
- **Checks:** device scenarios, `ComputerSystemTest`, machine scenarios and
  `LedGameTest` pass on the grouped front end; the fidelity, profile-store,
  catalog, session-contract, reference-profile, bindings and palette tests
  pass; `DocumentationDiagramsTest` fixed (stale page list).
- **Device browser check:** `RV32IMDeviceBrowserTest` passes after removing 12
  stale device root layouts (pre-grouping children), giving the
  `TimerDevice`/`ButtonDevice`/`LedPanel` type layouts their structural
  placements (compact-tuned ones kept under the Visual and Compact profiles;
  both forms share one type key), widening the machine's BUS caption to
  0.28, and updating two stale expectations (a compact device keeps its
  groups; the button scenario ends with an empty queue).
  `DeviceSystemFidelityBrowserTest` passes. Not rerun: the complete browser
  suite.
- **Recordings and experiments redone** after the structural changes, same
  tools and settings: all 11 games at embedded timing, 4.5 ns clock, compact
  parts (identical cycles, instructions, events, console and report words;
  host time 73–362 s, was 143–1,117 s), repackaged with the current layouts;
  the 19 unit-timing machine runs and the 10 clock certificates are identical
  to the published ones. § 5.6/§ 5.7 tables regenerated; § 5.7's stale limit
  removed. `RecordedCircuitBrowserTest`, `RecordedCircuitTransportTest`,
  `DocumentationExamplesTest` and the strict preview build pass.

## 2026-11-01

### Computer system: arbiter trees, compact bus, adapter chains, scenario and visual review

- **Bus.** Any number of masters may share a target port: the bus builds a
  tree of two-port arbiters (`<PORT>_ARBITER_<k>`). Shares are unequal and
  documented. The whole bus has an on-request compact form (its gates
  compiled with characterized storage, chosen only by an exact path or a
  parent). `TimingGraph` no longer counts a dead zero-delay pass-through as
  using the clock as data. `BusInterconnectTest` compares the gate-level
  bus, compiled routers and the compiled bus over 400 random cycles.
- **Adapter chains.** A split wired straight into a join (owner question) was
  an artifact of the bit-vector helper. `StructuralNetBuilder` now reuses a
  join's bits instead of splitting again, returns the original word for an
  in-order join, builds one Rewire otherwise, and prunes adapters that drive
  nothing; the Legacy adapter mode is retired. Census over every visualizer
  scenario: 68 chains in 20 parent types before, 0 in 380 after (the
  console's `TX_BYTE` is one Rewire). Rewriting the delay tables changed no
  stored delay; two tables of retired contracts were dropped and the three
  `bus.bridge.W` tables added.
- **Scenario checkpoints.** Every § 5 scenario row after reset checks the
  port rule (`include/tests/PortRule.hpp`); thin scenarios gained targeted
  rows (register-port timing and refusals, console queue, timer count, button
  events, display settings, FIFO overflow, counter wrap, router/bridge fields,
  simultaneous memory ports, LED blink, the full M control word, the execute
  packet, the arithmetic hold). Three first expectations were wrong and were
  corrected from the circuit; § 5.3 and § 5.4.1 now state the response timing
  and the arbiter's ready rule.
- **Visual review** (54 views of the 18 § 5 scenarios, no page errors):
  `BusPortChecker` regrouped into request/response watches, handshake and
  flags (same gates); the LED panel shows its picture under Visual; machine
  and bus floorplans place bridges between buses, checkers in a bottom row
  and tree arbiters under the routers; palette entries for the checker
  parts. Layouts refilled; static layout check 0 problems in 380 scenarios.
- **§ 4.7 on the memory subsystem.** `MemorySubsystemConfig` takes the full
  form (any memory configuration, per-port and shared caches, arbiter burst
  length, optional `INV_L1`/`INV_L2`); the § 4.7 program root and the § 4.8
  experiment builder use it for every shared arrangement. The subsystem has a
  layout variant per cache set; the memory inspector finds the shared memory
  as the subsystem's `RAM`. The four arrangement tests pass and five
  published § 4.8 runs reproduce their cycles exactly.
- **Docs.** § 5.2 bus page (tree, compact form, recorded example), checker
  example and parts table, § 5.5 machine variants, reference and visualizer
  page updated.
- **Tests repaired.** Four visualizer checks had stale harnesses from earlier
  work (camera, rewire geometry, reference profile, session contract's rewire
  key) and one name rule now names its two catalog-wide exceptions;
  `RV32IDecodeControlField` layouts were reflowed after the adapter change.
- **Checks run:** all § 5 component, machine and § 5.1 scenarios;
  `BusInterconnectTest`, `BusPortCheckerTest`, `ComputerSystemPortCheckTest`;
  32 visualizer/layout/palette tests; delay-table rewrite and comparison;
  adapter and colour censuses. **Not run:** full native suite, WebAssembly
  browser suites; games not re-recorded after the structural changes.
- **Incident.** A screenshot run whose two memory caps together exceeded
  free memory rebooted the host; no repository file was lost.

## 2026-10-29

### Visualizer: keep behavioral pictures with Device View off; solid outlines

- Remove the dotted/dashed behavioral component borders. Every component
  uses the existing solid outline; explorer S/B badges identify fidelity.
  Explicitly document and retain behavioral abstractions with Device View
  off, including while a structural rebuild is pending.
- Add direct canvas checks for behavioral word-gate symbols and recorded
  device panels with the toggle off and without dashed outlines. Two focused
  device/memory-cell checks, both updated browser checks on 8893 and the strict
  documentation build pass. Served assets match source; no native/WASM rebuild
  or broad suite was run. Evidence is in the
  [existing record](records/2026-10-28-visualizer-fidelity-pictures.md).

### Visualizer: restore Device View for outermost abstractions

- Restore the Device View button as an independent canvas toggle, initially
  off. It shows existing device/bus panels, gate symbols and storage views
  regardless of fidelity, only where no ancestor has an abstraction. Hidden
  descendant selections focus the visible owner; boundary pins remain
  selectable. Turning it off restores the fidelity/tree view. Sessions,
  timeline, fidelity draft and layout are preserved, including on recordings.
- Fetch state for outermost abstractions, reject late overlay responses
  after view changes, and label missing recorded table data. Update the
  public visualizer page, explorer reference and
  [topic record](records/2026-10-28-visualizer-fidelity-pictures.md).
- Checks: device-view and memory-cell Node checks pass. Updated explorer and
  recorded-circuit browser checks pass on 8893 under a 1,100 MiB cap,
  including structural word-gate closure, hidden-child focus, unchanged
  simulation/profile/layout and all 18 Pong checkpoints. The strict preview
  build passes; served simulator assets match the updated source. No
  native/WASM rebuild, delay-table regeneration or full suite was run;
  the earlier broad browser acceptance remains paused for host memory.

### Computer System chapter: complete component details and qualify evidence

- Expand chapter 5's interfaces, reset/backpressure/error and four-state
  boundaries, FIFO/command edge cases, construction and binary reference
  code, source links and test scope. Five tables render 20 selected paths
  from existing proven timing data. Correct unsupported bus-burst and
  full-checker claims, manual register-map provenance, the new LED fixture's
  baseline, and the cached-FF clock bound (4.254 ns, not 4.25 ns).
- Explain deliberate structural/runtime-timing choices and record missing
  protocol/register/width/configuration and native/WASM acceptance evidence
  in the [review](records/2026-10-29-computer-system-documentation-review.md)
  and open architecture record. Hardware, scenario rows and timing values
  are unchanged. The timing documentation check now allows added chapters
  while checking every paragraph and retaining its original coverage floor.
- The [whole-catalog follow-up](records/2026-10-29-computer-system-compact-availability.md)
  also corrects explicit compact multiply/divide-step, M decode-control and
  coordinator availability, which module-local factory lists alone omit.
  Their generic compact check passes 36 configurations / 4,032 vectors;
  these forms derive delays at build time and have no current stored table
  entries. The final strict build and live routes pass after this correction.
- Checks: standalone RV32IM certificates pass in all three profiles;
  timing documentation 7/7; four C++ references match 19 recorded checkpoints
  plus edge cases; construction/three C usage snippets compile; 175 pin
  definitions and 20 register entries match existing data/source. Strict
  preview build and all 22 live chapter routes pass (five timing and 27
  scenario tables rendered). No full native/browser/WASM suite, delay-table
  rewrite or game campaign was run.

### Local preview: use the current simulator package on port 8893

- The preview's asset-copy and example-rendering hooks now share
  `extra.preview_simulator_dir`, configured for
  `build-wasm-milestone/pages/simulator`. Previously they read the stale
  `build-wasm` package, so the new Basic pages failed on the missing
  `not-gate` example and left the live server stuck. The watch path follows
  the current package; the example-file environment override and production
  default remain supported. Update the preview guide and
  [recovery record](records/2026-10-29-local-preview-recovery.md).
- Verification: strict preview build passes under a 768 MiB address-space
  cap; all 145 page markers match the selected examples, all 32 simulator
  files copy exactly, and ten live HTTP routes return 200. Five served
  assets match their source SHA-256 hashes. The normal service has zero
  automatic restarts and OOM events, with a 302 MiB peak under its existing
  768 MiB/no-swap limit. No native/WASM rebuild or broad test suite was run.

### Device I/O: buses, bridges, register tables and machines described in JSON

- A machine is now a JSON description (`systems/*.json`, schema 1) read
  into `SystemDescription` and built by `ComputerSystem`
  (`system.computer`): parts by catalog contract, buses, outside pins, and
  the system clock and reset on every `CLK`/`RST`/`*_CLK`/`*_RST` input.
  Built-ins: `games-machine`, `games-machine-two-bus`,
  `games-machine-registered-bridge`, `games-machine-leds`,
  `games-machine-checked`. Why: the owner's goal of a general bus-and-device
  architecture where a device is added by describing it
  ([record](records/2026-10-28-device-io-architecture.md)).
- New parts: `BusRouter<W>` (`bus.router.<W>`, the old address router's gates
  per master with its own routes; compiled form), `Bus<W>`
  (`bus.interconnect.<W>`: routers per master, a two-port arbiter per shared
  target), `BusBridge<W>` (`bus.bridge.<W>`, pass-through or registered),
  `MemorySubsystem` (`memory.subsystem.32`), `BusPortChecker<W>`
  (`bus.port-checker.<W>`, a passive protocol observer; `"check_ports"` puts
  one on every link) and `LedPanel` (`device.leds`).
- Devices: register tables (`deviceRegisterTable`) generate each device's
  register front end (one `Decoder<n>` on the word index, the old status
  order, a word-mux read chain, strobes, read-write storage). Console,
  timer, buttons and display were rebuilt on it with unchanged offsets,
  statuses and cycles; the buttons' 32 comparators became one `Decoder<5>`.
- Software: `system_header` generates the guest header from a description;
  `tools/rv32im/guest/system.h` and `system-leds.h` are generated and checked
  (`GuestHeaderTest`). The guest sources use its names; all ten existing
  ELFs are byte-identical. New fixture `snake-leds` drives the LED panel.
  The recorded runner builds any described machine, models the LED panel and
  fails on `PORT_VIOLATION`; `system_experiment` runs a fixture on any machine,
  timing profile and period, optionally writing a visualizer recording.
- Visualizer: `bus` and `leds` device views; machine, bus and memory
  subsystem floorplans; a Computer System menu; layouts filled for the new
  scenarios and the changed devices, retired layout entries pruned.
- Retired, no aliases: `rv32im.system.devices` (`RV32IMDeviceSystem`),
  `bus.word.address-router.<W>`, `bus.word.instruction-guard.<W>`,
  `bus.word.error-target.<W>`, `video.pixel-selector.<W>`, the fixed
  `AddressMap`; their tests, scenarios, palette and layouts. Site chapters
  `rv32im/` and `device-io/` were replaced by § 5 Computer System
  (`site-docs/system/`) with five new figures; old game recordings were
  removed from the site (untracked evidence kept under
  `build/retired-site-assets/`). The device-foundation reference is archived;
  `docs/reference/computer-system.md` replaces it.
- Also fixed (from the earlier word-gate and compiled-form work): the
  catalog inventory now knows word gates at every width 2–32, and generic
  compiled forms take the gate-level form's contract tests and type name.
- Checks: the one-bus machine equals the old system cycle for cycle
  (bring-up program 126/206/393 cycles; bring-up game 38,811 and pong 88,367
  cycles with identical frames, console, reports, device reads and inputs;
  26–27% fewer events); `ComputerSystem{,CachedFF,CachedDRAM,TwoBus,RegisteredBridge,PortCheck,Fault,Lockstep}Test`,
  `SystemDescriptionTest`, `BusInterconnectTest`, `BusPortCheckerTest`,
  `LedPanelTest`, `LedGameTest`, `GuestHeaderContentTest`, the five new
  recorded scenarios, the device tests and scenarios, `WidthComponentsTest`
  and `BuiltinComponentCatalogInventoryTest` pass; every machine has a clock
  certificate (unit 84–94 ticks; embedded 4,254–4,470 ps); static layouts of
  the ten system scenarios have no problems.
- Experiments (§ 5.7, generated from tracked summaries): the old and new
  machine agree on bring-up, Pong and cached Snake (38,811 / 88,367 /
  260,970 cycles, identical results). Two buses add exactly one cycle per
  device access (40–81 per game, ≤ 0.10%), a registered bridge three; frames
  and console are identical on all three machines. All 11 fixtures were
  re-recorded at the chosen realistic fidelity (embedded timing, compact
  parts, 4.5 ns clock) with the same cycle counts as the old unit recordings.
- Site: all 174 example groups regenerated and verified in WebAssembly; strict
  MkDocs build passes; `RecordedCircuitBrowserTest` passes on a new recording;
  the recorded viewer now shows a recording's own timing profile. Fixed along
  the way: a Basic-chapter joiner row and clock-page marker, palette prefix
  colors, two stale binding-test expectations, router route validation.
- Layouts: the full static layout check now finds 0 problems in all 374
  scenarios (it found 19,792 missing placements in 244, mostly from the
  earlier word-gate regrouping); all 15 layout CTest checks pass after
  re-flowing the changed types and fixing the tag-match floorplan.
- Not run: the full native suite and the WebAssembly browser suites other
  than the recording check (`RV32IMDeviceBrowserTest` and
  `DeviceSystemFidelityBrowserTest` were updated but not run). Deliberately
  not done: N-way arbitration, a whole-bus compact form, moving § 4.7 onto the
  memory subsystem (see the record's plan status).

## 2026-10-28

### Visualizer: the explorer tree is the planned fidelity

- Expanded means structural, collapsed means behavioral; Apply & Run
  rebuilds to match and Revert returns the tree to what is built. The S/B
  badge is read-only and shows the built fidelity; parts with one form have
  no arrow. The canvas follows the tree at once for built parts: a part
  collapsed before Apply is drawn closed (word gate symbol, device view or a
  `▸` box) with live pin values; an expanded behavioral part stays a block
  titled "Apply to build", and the explorer lists the parts it would have in
  gray from a build-only preview
  (`VisualizerSession::previewComponentsJson`, WASM `previewComponentsJson`,
  worker `POST /api/circuit/preview`; nothing simulated). Behavioral parts
  have a dashed outline.
- Why: owner design — fidelity and open/close mean nearly the same; only the
  simulation differs. It replaces two interim designs from the same day
  (pictures only when behavioral with an inspector Open/Close; then a
  view-only explorer fold with Save tree / Reset tree), both removed.
- Removed the "Device views" toggle. Fixed hover picking: the index was
  built from the parts visible at layout time (parts that came into view
  after a pan or zoom could not be hovered), and pins/wires inside closed
  parts stayed pickable (invisible wires were highlighted). Picking now
  indexes everything once and accepts only what is drawn.
- Checks: browser checks `VisualizerExplorerTreeBrowserTest` (new),
  `VisualizerLayoutStorageBrowserTest` (new), `VisualizerScenarioBrowserTest`,
  `RecordedCircuitBrowserTest`, `VisualizerTimingProfileBrowserTest`,
  `MemoryInspectorBrowserTest` (pre-existing ambiguous canvas locator fixed),
  `VisualizerRewireBrowserTest` and `FFMemoryBrowserTest` pass; native
  preview probe checked; strict MkDocs build passes; deployed to the 8893
  preview. Not completed (host memory; stopped by a 1.1 GB cap or not run):
  `VisualizerWasmBrowserTest`, `RV32IMDeviceBrowserTest`, the SRAM, memory
  and memory-device documentation checks and `MemoryEndpointBrowserTest`.

### Browser layout saves store only edits

- The serverless visualizer (GitHub Pages, 8893) saved the whole merged
  layout (8.1 MB JSON) to IndexedDB on every save, and that snapshot then
  overrode the published defaults, so a browser that saved once never got
  later layout updates. It now stores only edited entries, fields and child
  placements (`visualizer-v2/static/layout_changes.mjs`, format
  `layout-changes-v1`); older snapshots still load. The running visualizer
  never writes `layout.json` (owner rule; the native server on port 8765,
  which did, is retired by the owner).
- Checks: new `VisualizerLayoutChangesTest` (Node): 454 bytes instead of
  8.1 MB for four edits, round trip, later defaults reach unedited parts,
  older snapshots load; new `VisualizerLayoutStorageBrowserTest` (no edits
  store an empty record; a color edit survives reload). Layout migration and
  profile-layout checks pass; `VisualizerReferenceProfileTest` passes again
  (its sandbox lacked functions added by the 2026-10-24 fidelity presets).

### Native suite fixes

- `tests/checks/RV32IMGameReferenceTest.py` passes `--logic-profile compact`:
  since the 2026-10-24 presets `run-recorded.py` defaults to Visual, which
  exceeded the 500 million event budget; all ten recorded-game tests pass.
- `RV32IProfileToggleSweepTest` audits the word gates and 32-bit prefix
  adder that became selectable in the RV32I ALU (20 contracts, 1,089
  instances); passes.
- The full suite was stopped by the host after 30 of 591 tests (memory
  pressure from other projects); 561 tests not run.

### Recorded-guest parity re-run

- The full `RecordedGuestWasmParityTest` passes after the preset fix: all ten
  scripted guests and the 97×65 run are exact (frames, reports, device
  samples, cycles, events), so the parity suite passes 16 of 16. The
  committed game recordings are stale for the current timing (`bringup`:
  same 38,811 cycles, 15,336,236 recorded events against 15,181,178 now); the
  owner chose to recapture them once after the device I/O work.
- Visualizer browser checks: `VisualizerRewireBrowserTest` and
  `VisualizerScenarioBrowserTest` pass; `VisualizerWasmBrowserTest` passes
  after updating three Chapter 2 embed component counts that changed with the
  word gates (prefix adder 16-bit 319, add-sub 8-bit 167, arithmetic unit
  8-bit 908; native and browser agree).

## 2026-10-27

### WebAssembly rebuild, parity checks and site build

- Rebuilt the WASM module and verification runner from the current tree
  (pooled events, D2, second `unit` table pass). 16 tests pass in the WASM
  runner (word-gate, adder, ALU and pipeline-control sweeps, the M unit at gate
  level, the RV32IM core and programs, `Rewire`, the steps). All 161 example
  groups verify with regenerated rows; the strict MkDocs build passes.
- Parity suite: 14 of 16 passed. Two test expectations were stale and are
  fixed: `ALUWasmParityTest` now checks the prefix adder's 32-bit
  `PROPAGATE`/`GENERATE` gates instead of 32 half adders (word-gate change),
  and `RecordedGuestWasmParityTest` selects the `compact` preset on the WASM
  bridge, matching `RecordedGuestProbe`'s default (event counts already
  matched). Re-run: ALU parity passes; recorded-guest parity re-checked on
  `bringup` only (exact). Not re-run: the other nine guests and the 97×65
  run (about an hour). Game recordings not regenerated (owner decision).

### Pooled events

- `circuit::makeEvent<T>` (`include/simulator/EventPool.hpp`) allocates
  events with `std::allocate_shared` from a per-thread free list, so finished
  events' memory is reused; shared/weak ownership and inertial cancellation
  are unchanged. Used at the nine hot allocation sites.
- Checks: 17 focused engine/M-unit CTests and all 26 non-slow timing CTests.
  Visual guest: −6.3% execution time with the same 15,457,313 events
  (six runs each, every pooled run faster); test-level blocks −2.4% to ±1%.
  Not run: WASM, full suite.

### `unit` delay tables: second pass; multiply/divide step splits

- The `unit` regeneration after the splitter change was one pass, and parts
  that contain storage were proven on the storage tables compiled into the
  tool, which still charged the splitter tick. A second pass changes 117 of
  833 entries, every number smaller (cores, pipeline registers and stages,
  the M unit, six device-part tables); a third pass is identical. The
  advanced core's `unit` certificate falls from 82 to 80 ticks (single-cycle
  142 and baseline 68 unchanged); documentation timing data regenerated
  (only the period changes), site and reference text updated.
- `Logic::addToWord` and `Logic::subtractWord` keep words whole in
  `MultiplyStep`/`DivideStep`, removing the last three unused Rewires (same
  gates and child names). Step layouts pruned and re-laid.
- Checks: all 26 non-slow timing CTests, `DelayRuleTest`, 15 focused M-unit
  CTests (including the slow gate-level one), static layout check 0 problems
  in 373 scenarios. Published §4.8 results remain at the old periods (owner
  decision pending).

### Run-time signal width (D2): packed values, one wire event

- A pin or wire now holds a four-state value of any width from 1 to 64 bits,
  packed in two 64-bit words (`PackedLogic`). `PinBase`/`WireBase` own the
  width and value with non-virtual accessors; `Pin<W>`/`Wire<W>` are stateless
  typed handles. `WireUpdateEvent` is one class for every width, and the typed
  fanout, the width dispatch in `makeWireUpdateEvent` and
  `TemplateInstantiations.cpp` are gone. `dispatchWidth` covers 1–64; which
  widths a family offers stays its own contract (`Register` lists its eight).
  `Rewire` accepts any width 1–64. `Rewire`, `BitJoiner`, gate input caches,
  word gates and the delay-table path work on packed values.
- Why: one value representation instead of a template per width, and widths
  outside the former fixed list (owner request in the non-device plan).
- New `SignalWidthTest`; `RewireValidationTest` now rejects 65 instead of 7/64.
- Checks: broad non-slow native suite 362/365 (the three failures predate D2:
  `DeviceParametersBindingsTest` from the uncommitted device work and two
  visualizer palette/profile tests); `DelayRuleTest` (slow) passes; focused
  CTests after each stage.
  Speed at load 3–4: test-level changes within ±3% (not resolved); the Visual
  guest runs 2% faster while processing 12.5% more events than the 09:04 build,
  a comparison that also spans typed fanout, Chapter 2 word gates and the
  splitter change. Not run: WASM, strict site build. The research scripts
  under `tools/engine-performance/` still name the 2026-10-26 file list.

### Broad suite fixes after the splitter change

- `tests/fixtures/memory-array-timing.json`: each memory transaction is one
  tick sooner with the same values. `DelayRuleTest` floors for the 8/16/32-bit
  cache controllers re-measured. `Register<32>` and `RV32ISingleCycleCore`
  saved layouts regenerated with the repository generators (both entries were
  empty before this session; 0 static-layout problems in 373 scenarios).

### Zero-delay splitter in `unit`; Chapter 2 word gates

- The bus splitter no longer costs a tick in `unit` timing; it is wiring, as in
  the realistic profiles. `LogicUnit<W>`, `ParallelPrefixAdder<W>` and
  `AddSub<W>` now use word gates (the same gates, regrouped).
- Delay tables regenerated: `embedded` and `desktop` byte-identical (833
  entries), `unit` 782 entries changed. `unit` re-certified: core periods
  142/68/82 (were 166/80/94), RV32IM core 84 (was 94), M unit 48 (was 68);
  documentation timing data, timing-test expectations and site figures
  updated. Layouts migrated; 0 static-layout problems in 373 scenarios.
- Checks: 26 non-slow timing CTests and the Chapter 2 ALU/adder tests pass.
  Published §4.8 results at the old periods stay dated (owner decision).

### Unused splits removed from the multiply/divide groups

- `Logic::out` wires words straight through, and new word-level helpers
  (`invertWord`, `subtractWords`, `negateWord`) let the M-unit result
  corrections, divide operands and divide datapath keep words whole. Unused
  Rewires fall from 25 to 3 in `MulDivUnit<32>`; `MultiplyResultCorrection<32>`
  has 9 parts instead of the original 82.
- Checks: affected delay tables unchanged in all profiles; 31 focused CTests;
  layouts migrated, static layout check 0 problems.

### Typed fanout and guest throughput

- Wire fanout copies values as typed arrays into same-width pins and
  forwarding events (vector path kept for other widths). 55 focused CTests
  pass; one paired block shows 1.1–6.6% less CPU.
- The record's Visual guest (2,000 cycles) runs in 8.82 s instead of 13.74 s
  against the frozen 2026-10-26 baseline: 1.04 M → 1.56 M events/s, with 3.9%
  fewer events for the same work. One A/B/B/A block; not a game capture.

### One-byte LogicValue

- `LogicValue` uses a `uint8_t` underlying type, so stored values take a
  quarter of the memory. 55 focused CTests pass; one paired block per test
  shows 1.4–1.9% less CPU. Not run: full suite, WASM.

### Engine: queue keys, no-op wire updates, sink routes, incremental twin

- The event queue stores each event's sort keys beside it; a wire update that
  restates the current value returns before allocating; wires cache what each
  sink does; and the compact parts' clocked timing model re-evaluates only the
  gates whose inputs changed (from the 2026-10-26 prototype).
- `DelayTwinIncrementalTest` keeps the previous twin as a test-only reference
  and requires identical answers, values and arrival times on 72,000 clocked
  steps in every timing profile (fault-checked).
- Checks: 51–52 focused CTests after each step; `DelayRuleTest` passes after
  the twin and route changes. One paired A/B/B/A block per test (pinned CPU):
  `ALUWidthContractTest` −33%, `MulDivGateLevelTest` −21% (all D1–D4),
  `MulDivUnitWidth32Test` −59% and `RV32IMFiveStageCoreTest` −4.4% (twin and
  routes). Limited evidence; not a guest or game measurement. Not run: full
  suite, WASM.

### Word gates and first engine cleanups

- Add word gate families `ANDGate<W>`, `NANDGate<W>`, `ORGate<W>`, `NORGate<W>`,
  `XORGate<W>` and `NOTGate<W>` (W = 4/8/16/32, contracts `logic.<kind>.<W>`):
  W one-bit gates between zero-delay Rewires, plus a compact form on proven
  delays. The M units' subtraction/negation use `NOTGate<W>` instead of W
  inverters with identical timing; `MultiplyResultCorrection<32>` drops from
  79 to 15 children. Chapter 2 components are unchanged (owner decision).
- The visualizer draws word gates as the one-bit symbol with a `/W` badge and
  shows their one-bit gates when one is selected; palette and layout defaults
  added; RV32IM/M group layouts migrated.
- Regenerate the three delay-table files: 24 new entries each, all 809
  existing entries byte-identical.
- Engine (exact): cache `Rewire` routes and `BitJoiner` inputs, build splitter
  output names once, read diagnostic environment switches once, and drop
  temporary vectors from timed evaluation. Gate-level test CPU time falls
  1.7–21% (two runs each, not the paired protocol).
- Checks: `WordGateSweepTest` (values on both forms and per-bit timing in every
  profile; fault-checked), 53 focused CTests, 19/21 visualizer CTests (two
  pre-existing failures), static layout check 0 problems. Not run: full suite,
  WASM/browser checks, strict site build.

### Fewer bit adapters in non-device recipes

- `circuit::structural::Logic` no longer builds one-bit split/join Rewires or
  split→join round trips, bridges a parent input to its own output with one
  `PASS_<output>` Rewire, and shares one select inverter per word mux. Skipped
  adapters keep their name numbers, so other child names are unchanged. Device
  recipes select the legacy construction until the device redesign. Why: the
  crowded `MULDIV…RESULT.SPLIT_0` and similar groups were pure round trips.
- Rewires drop by about half (`MulDivUnit<32>` 126 → 61, five-stage core
  336 → 206, pipeline coordinator 48 → 7).
- Checks: all 809 unit delay tables recompute identically; the affected base
  tables also match in embedded and desktop; 37 focused RV32IM/M/decoder/device
  CTests and 19 of 21 visualizer CTests pass (the two failures predate this
  change). Four group layouts were migrated and the static layout check reports
  0 problems on 14 scenarios.
- Fix `AdderWidthSweepTest` appearing as a visualizable scenario.
- Rejected: subtraction through `AddSub<W>`, which would add a splitter tick to
  the core's critical path. Not run: full suite, WASM/browser checks, strict
  site build; game recordings were not recaptured.

### Gate-level multiply/divide and pipeline control sweeps

- Add `MulDivGateLevelTest` (label `slow`, about 44 s): exhaustive 4-bit M unit
  on both forms, boundary and random 8/16-bit pairs, and strict gate-level
  32-bit multiplier, divider and M unit, against a width-generic reference.
- Add `RV32IPipelineControlSweepTest` (under 1 s): 8,502 forwarding and 2,358
  hazard vectors against independent rules on both forms.
- Extend `MemoryDeviceContractTest` with further ROM cell/row contents.
- Deliberate faults in the divide result correction and the forwarding
  comparator were caught by the new tests and missed by the narrow-width and
  recorded tests; both were reverted. Site pages for the M units, forwarding,
  hazards and ROM describe the new checks.
- Checks: the affected focused CTests pass. Not run: full suite, WASM/browser
  and the strict MkDocs build (disk headroom).

### Adder and ALU sweep tests

- Add shared sweep equipment (`include/tests/SettledSweep.hpp`) and three
  headless sweeps against independent integer references on both forms:
  `AdderWidthSweepTest` (ripple and prefix adders at 8/16/32 bits, every carry
  span at every start position, 1,024 random vectors and every single X/Z input
  bit), a 16/32-bit boundary grid and random vectors in `ALUWidthContractTest`,
  and `RV32IALUSweepTest`. Why: `ParallelPrefixAdderWidth16Test` had 8 cases
  and the 16/32-bit ALU sub-units had no real sweep.
- Deliberate faults in the prefix adder and in `RV32IALU`'s XOR were missed by
  the existing tests and caught by the new sweeps; both faults were reverted.
- Fix catalog ownership for `word.alu.8` and the 16-bit adders, which had two
  logical owner tests: `WordDatapath.cpp` now separates the contract owner from
  the equivalence evidence. `BuiltinComponentCatalogInventoryTest` passes again.
- Checks: 15 focused CTests and `DocumentationExamplesTest` pass. Runtimes:
  adder sweep about 7 s, `RV32IALUSweepTest` about 6 s, `ALUWidthContractTest`
  21–26 s (was about 5 s). Not run: full suite, WASM/browser checks and the
  strict MkDocs build (skipped for disk headroom; only test-table rows changed).

### Non-device improvement plan

- Add the [non-device improvement plan](records/2026-10-27-non-device-improvement-plan.md):
  test sweeps, the `Logic` adapter fix, word-wide gate families with a
  gate-symbol visual override, and engine phases including run-time pin
  width, in the owner's order. Device I/O work is excluded.
- Documentation only; no build or test was run.

### Device architecture review

- Add the [device architecture record](records/2026-10-27-device-architecture-review.md):
  maps the device platform against the textbook's bus, I/O bridge, DMA and
  interrupt model (CS:APP 1.4.1, 6.1, 8.1, 10) and proposes a data-driven
  interconnect, a standard device contract, interrupts/traps and DMA in seven
  phases that keep the ten game fixtures exact.
- Documentation only; no build, test or simulation was run. The fixed
  six-region `AddressMap` was checked against source.

### Device I/O and redundant bit-adapter review

- Add the [Device I/O review record](records/2026-10-27-device-io-review.md):
  `StructuralLogic.hpp` root causes for one-bit Rewires and split/join round
  trips (including `MULDIV.MULTIPLY.RESULT.SPLIT_0`), a construction-only
  census of eight scenarios, a staged fix plan with migration impact, and a
  per-component Device I/O review.
- Documentation only. The census built circuits with the 2026-10-24 Python
  module under a 2 GiB cap and ran no simulation, so counts may lag the
  working tree slightly. No build or test was run.

### Test strength review

- Add a [test strength record](records/2026-10-27-test-strength-review.md)
  with case counts by area and a phased plan for sweep tests: adders and
  16/32-bit ALU units first, then gate-level multiply/divide, then RV32I
  forwarding/hazard units and the address router. New coverage stays in
  headless tests so published example tables are unchanged.
- Documentation only; no build or test run (only `SimulationTestRunner --list`).
  Runtime figures are estimates from cached CTest averages. The 8-case
  `ParallelPrefixAdderWidth16Test` count was checked against source.

### Engine performance review and plan

- Add a source and record review with a phased optimization plan to the
  [engine performance record](records/2026-10-26-engine-performance.md). It
  puts native cost at about 1 µs CPU per event (10–12× from the 10 million
  events/second target) and finds that measured allocation savings explain
  only a few percent. New exact-cleanup candidates include per-bit pin-name
  work in splitters/joiners/Rewire, `getenv` on the output-publish path,
  int-sized `LogicValue` and inline heap keys. Seven owner decisions are listed.
- Close the 2026-10-13 engine performance plan as superseded; its open
  decisions are carried into the new plan.
- Documentation only; no build, test or measurement was run. Cited source
  lines were spot-checked.

## 2026-10-26

### Incremental clocked timing-model experiments

- Add private owned-buffer, dirty-node and ordered-mask prototypes with shared
  CSR dependencies. Preserve both value and arrival changes, capture ordering,
  unknown/reset/falling-only behavior and full traversal for unsupported models.
  Separate the algorithm from compiler settings; production remains unchanged.
- Fifty-eight focused checks pass on 104,664-case timing signatures, feedback,
  arrival-only changes, owner/cold-state behavior and existing scenario/decoder
  checks. All 82 guest results and 41 streamed packages match golden data.
  Add alternating within-process reference/candidate register kernels and an
  unchanged control that exposes residual measurement/layout bias.
- Ordered masks skip 66.1% of modeled evaluations with about 79 KiB cumulative
  metadata. Limited stable `-O1` guest blocks show 4.9% / 4.2% lower CPU without/
  with capture; one block per mode is stable, and all matched `-O2` blocks drift
  too much. Do not present larger kernel gains as whole-engine performance.
- Retain one corrected probe-entry compile failure and every unstable result.
  Peak RSS is 262 MiB, no experiment OOM/swap. Archive completed private
  verification ELFs losslessly, freeing 2.39 GiB with hashes/restoration metadata;
  a restored reference executable passes. Keep all code, objects and evidence.
- No candidate integration, full native/game/WASM/browser acceptance or live
  deployment. See the [open record](records/2026-10-26-engine-performance.md).

### Wire/timing experiments and bracketed performance controls

- Add matched actual-Clang/PIC controls for guarded wire updates, timing scratch
  reuse and selected-file `-O2`. Derived wires retain the generic value API.
  Add independent width/four-state/timing and clocked-repeat verification.
- Preserve an exploratory timing matrix interrupted after unrelated CPU load
  caused baseline drift. Complete 32 fixed-affinity baseline/candidate/candidate/
  baseline measurements with host context and a predeclared 5% control-drift
  threshold. Stable blocks show roughly 3% lower CPU separately for wire and
  compiler changes, with limited samples; one unstable block remains recorded.
- Wire updates eliminate 32% of C++ allocation requests. Timing scratch reuse
  removes only 0.02%; stateless memo has zero eligible hits and guarded stateful
  memo only 0.3%, adding allocations. Retain these negative results and avoid
  adopting them as workload optimizations.
- Eighty focused checks and all 84 completed guests pass; 42 streamed packages
  match previous golden bytes. Peak RSS is 262 MiB, zero experiment OOM/swap.
  Record the include-path failure, superseded wire draft and intentional stop.
  Production sources/archive and live modules/captures remain unchanged.
  No full-library adoption, full-game, broad native or WASM/browser acceptance.
  See the [open performance record](records/2026-10-26-engine-performance.md).

### Primitive gate input caches and signal-buffer reuse

- Cache typed inputs in all six primitive gates while preserving public sorted
  pin maps. Invalidate immediately on pin edits so replaced inputs are released
  before obsolete wire sinks can drive them. Reuse disabled-history pin value
  buffers; preserve four-state, delay, event and recorded-history semantics.
- Rebuild the complete isolated native archive and focused consumers. Thirteen
  native checks, full-tick decoding and complete stored-delay comparison pass.
  Thirty repeated RV32IM comparisons and ten calibration runs match state,
  counters and every trace byte; eighteen inverter-fanout runs match digests.
- Combined RV32IM median CPU time falls 0.7% without capture and 1.2% with it,
  with overlapping run ranges. Individual controls are mixed; this does not
  establish a broad engine speedup. Cache size grows 24/40 bytes per gate.
  Peak process RSS is 463 MiB, zero experiment swap/OOM. Preserve failed and
  interrupted attempts and correct launcher/driver and stale-consumer controls.
- Harden experiment completion logging against externally stopped services.
  Update benchmark/test procedures and the
  [open performance record](records/2026-10-26-engine-performance.md).
  No live deployment, full-game recapture, full native suite or WASM/browser
  acceptance; timing-model/wire allocation work and the throughput target remain.

### STL container review and compiler-control correction

- Audit scheduling, pin lookup, timing, memory/history and recorder container
  requirements. Add 237 standalone comparisons and two 42-run integrated
  matrices, retaining every result and the superseded exploratory comparisons.
  Tiny-table and trace-row crossovers support selective policies; matching-PIC
  controls do not establish an engine-wide win from simple container swaps.
- Correct attribution of earlier compiler/prototype differences: override
  objects had omitted the frozen library's `-fPIC`. The original numbers
  remain evidence for those configurations, not an isolated `-O2` effect.
- Twelve matching-PIC native and three full-tick decoder checks pass; integrated
  counters/state and 42 new traces match the existing golden data. Headless
  buffer reuse removes 24,837 allocations in the short guest, a modest 0.19%.
  Keep production unchanged and prioritize cached execution indexes and value
  buffers. No full-game, full-suite or WASM run. See the
  [performance record](records/2026-10-26-engine-performance.md).

### Engine performance research equipment and first measurements

- Open the [engine performance record](records/2026-10-26-engine-performance.md)
  and add serial, memory-capped benchmark/profiling equipment with frozen
  inputs, common result envelopes and per-command failure/resource receipts.
  Keep the live engine and gameplay artifacts unchanged.
- Complete 126 matched comparisons, allocation diagnostics and twelve queue
  stress runs. A selected compiler configuration reduces median CPU time
  11–18% on the small RV32IM loop (PIC attribution corrected above); direct
  array comparison removes 32% of C++ allocations. Preserve
  the slower per-bit attempt and the bucket prototype's sparse-queue regressions.
- Fifteen focused native, three full-tick decoder and two scheduler contract
  checks pass. All matched counters/states/digests and 21 streamed traces agree.
  Peak process RSS is about 340 MiB, zero swaps and no experiment OOM. Hardware
  perf is blocked; gprof attribution is partial. One prototype generation
  compile failure is recorded and corrected. Full-game/full-suite/WASM/timing
  coverage and the 10-million-event target remain open; no deployment.

### Preview recovery from RAM-backed recording copies

- Restore port 8893 after repeated 768 MiB OOM failures during MkDocs startup.
  Move temporary builds from tmpfs to a managed disk-backed user cache, so
  immutable tick recordings use same-filesystem hardlinks. Keep the memory
  cap, no-swap policy, live rebuilding and gameplay capture unchanged.
- Validate a private strict preview and the normal service startup. Six HTTP
  routes, the reported `0.0.0.0:8893` endpoint, recorded chunk SHA-256 and inode
  sharing pass. Cgroup OOM and automatic restart counts remain zero. The asset
  regression passes; no native/game/WASM rebuild or broad test run. Add a local
  preview guide. See the [recovery record](records/2026-10-26-preview-memory-recovery.md).

## 2026-10-25

### Full tick gameplay capture and replay

- Add compressed, seekable signal deltas to the existing visualizer, including
  delayed propagation, register state and native memory writes. The full
  timeline uses PRESENTs as navigation markers. Old checkpoint captures retain
  their original granularity and need native recapture for intervening ticks.
- Bound native capture/browser decoding and validate chunks plus every PRESENT
  state before publication. Preserve single-copy immutable binary chunks in
  same-filesystem MkDocs builds. Add a separate 12 GiB full-tick campaign for
  the approved five Visual demonstrations; preserve completed frame-only games
  and stop their old worker/deferred retry.
- Serial native builds, five focused CTests and a real 4,024-tick Visual
  browser guest pass, including every PRESENT pin/wire/device/register value,
  intermediate stepping, memory pages and rewind. Current frozen native output
  matches the trial byte for byte, with unchanged events/results excluding host
  timings. The strict preview build and ten live 8893 assets/routes pass; a
  small full-tick validation guest is published. The five-game background unit
  is active under its memory guards. Full gameplay completion and broader
  suites remain pending. See the
  [record](records/2026-10-25-full-tick-gameplay.md).

## 2026-10-24

### Device-system fidelity switching and layout completion

- Fix Logic, Visual and Compact failing on missing browser placements for
  newly selected memory/controller and private compact device structures.
  Reuse compatible layouts or generate geometry for the actual child shape,
  keeping original/manual/unrelated entries and Reset Layout defaults intact.
- Expose shared contract restrictions before expensive construction: the
  device system's 256 KiB RAM excludes strict Gate. Disable that choice with
  its reason, reject direct requests early in native/WASM scenarios and the
  guest runtime, and display readable C++ exception messages in the worker.
- Focused native capacity/API/migration/layout checks, three real native system
  builds, eight native/WASM parity cases and timing/system browser regressions
  pass. Strict package/preview builds and live 8893 asset, preset and recovery
  checks pass. The frozen background game queue is unchanged. No circuit,
  timing-table or full-suite changes/checks. See the
  [record](records/2026-10-24-fidelity-presets-and-visual-gameplays.md).

### Short gameplay demonstration campaign

- Cancel the 3,090-frame visual long queue at the owner's request and preserve
  its saved logs/topology/manifests. Start the approved 146-frame demonstrations
  serially with Visual: Snake 10 first, then Pong 45, 2048 25, Sokoban 34 and
  Tetris 32. Retain the 768 MiB service / 640 MiB native address-space caps,
  no service swap, frozen execution and host-headroom/disk guards.
- Add reproducible `--demonstrations` preparation using the reviewed input
  plans. Give the new campaign separate publication paths and `demo-*`
  recording identities; keep long preparation available. Update the recording
  library, procedures and cancelled-run index link.
- All five rebuilt ELFs, input scripts and expected frame/report files match
  the reviewed candidates exactly. Complete host references and instruction
  audits pass, along with the pacing fixture, syntax/diff checks and strict
  preview build. Live index/manifest/documentation links pass; the new unit
  advances real CPU cycles with no swap and the old unit is inactive. Native
  game completion is pending. No engine/WASM rebuild or full test suite was
  needed. See the [record](records/2026-10-24-fidelity-presets-and-visual-gameplays.md).

### Visualizer fidelity presets and visual gameplay restart

- Add Gate, Logic, Visual and Compact beside Timing in native/WASM scenarios,
  retaining scenario defaults, preset URLs and separately saved custom edits.
  Both runtimes use one C++ policy; recorded replay fixes its capture fidelity.
- Add validated compact console/timer/buttons/framebuffer compositions from
  their structural control builders and proven smaller models. Keep strict
  leaf validation for other behavioral implementations. Export large structural
  game graphs as bounded scope files shared by 16-checkpoint recording parts;
  replay retains two scopes and one state window, with every PRESENT preserved.
- Stop the earlier compact queue and preserve its partials. Start all five
  frozen long workloads serially with Visual under 768 MiB/no-swap service
  limits and a 640 MiB native address-space cap. Measured structural throughput
  requires a 90-day native wall allowance and 10-trillion-event bound, retaining
  100-million-cycle/instruction limits. Full-game results are pending.
- Checked focused native component/runtime/preset checks, four API/transport
  checks, eight native/WASM parity cases, profile and real cross-part replay
  browser checks, strict Pages/preview builds, live 8893 asset/selector/replay
  checks and syntax/diff checks. Initial
  implementation/test failures and their corrections are retained in the
  [record](records/2026-10-24-fidelity-presets-and-visual-gameplays.md).
  No full suite or completed long-game/native-WASM acceptance is claimed.

### Complete scenario menu order

- **Order:** apply the owner's selected overall sequence to all 26 groups:
  foundations, ALU, memories, RV32I and programs 01–22, RV32IM and programs
  23–27, Device I/O, then developer circuits. Rank all 197 component entries
  and 22 RV32I program definitions explicitly, placing supporting parts
  before complete devices/cores and widths in numeric order. Keep names,
  aliases, contracts and saved layout/profile identities; move native FF
  scaling beside its smaller FF technology scenarios.
- **Fix:** remove browser alphabetization that overrode catalog ranks;
  share deterministic ordering between Python and JavaScript. Add final-menu
  order assertions to catalog/browser checks and update public references
  and catalog-only refresh guidance.
- **Checked:** five focused catalog methods pass, including Python/JS,
  reversed registry order and legacy aliases. Strict packaging and preview
  builds pass; a fresh live 8893 WASM session verifies all 219 menu rows,
  group labels, both source asset hashes and zero page errors. Browser
  scripts pass syntax checks. No full native/browser suites, engine rebuild,
  new circuit recording or timing/layout regeneration. The frozen long
  gameplay queue stays active and unchanged. See the
  [record](records/2026-10-24-scenario-menu-order.md).

### Long gameplay plans, bounded timelines and background execution

- **Sessions:** prepare separate substantial play scripts for all five games,
  totaling 3,090 planned frames: 901 Pong, 513 2048, 303 Sokoban, 604 Tetris
  and 769 Snake. Host preflight demonstrates multi-point rallies, merges,
  eight solved multi-crate puzzles, 29 Tetris line clears and Snake growth.
  These are planned/reference outcomes; full native sessions are ongoing.
- **Execution/records:** add finite presentation-paced real device inputs,
  an audited external-ELF native probe and bounded 32-checkpoint recording
  parts. The original visualizer exposes one continuous lazy timeline with
  cross-part Play/rewind. Existing short fixtures and standalone records stay.
  No circuit or delay-table change was needed.
- **Background:** freeze executable/guest/script/reference/layout tools and
  launch `circuitsim-long-gameplays-1792818961.service`. Pong is actively
  simulating; four games follow serially. MemoryMax is 768 MiB, swap zero,
  native AS 640 MiB, CPU quota 90% of one CPU and nice 10. Owned work stops
  on host pressure; completed publication hashes support per-game resume.
- **Checked:** all five full host references/instruction audits; two focused
  native compatibility checks; three real native frames split across three
  parts with exact frame/report/input checks; legacy/playlist transport tests;
  continuous original-canvas browser playback; strict site/preview builds
  and live index/viewer checks. The active job passed 57,344 cycles and its
  first PRESENT. Full long native completion remains pending; no full general
  regression, WASM/expanded/cache campaign, commit, push or remote deployment.
  See the [plan and run record](records/2026-10-24-long-gameplay-campaign.md).

### Complete native gameplay recordings in the original circuit visualizer

- **Records:** freshly run all ten packaged fixtures on one source/binary
  snapshot: bring-up, five games, 2048 merge, Tetris line clear, Pong rally
  and Snake wall collision. Publish 47 synchronized PRESENT checkpoints,
  actual topology/pin/wire/device states, layouts and per-game source/build,
  timing, reference and browser evidence. The public game page links every
  capture in the existing visualizer. Earlier frame-only exports retain
  their source identities; previous Pong files are backed up locally.
- **Validation:** all ten exact host frame/report/input comparisons and ten
  original-canvas browser sessions pass, including every retained pin/wire
  vector, display/input state, inspector, rewind, Play and Device views.
  All runs report zero setup violations. Strict preview packaging passes;
  live 8893 checks verify three routes, all ten game links and 79 exact assets.
  The final live Pong replay also passes all 18 checkpoints and viewer controls.
- **Resources:** native execution totals 2h 6m 29s, with at most 155.58 MiB
  native RSS. Commands are serial, histories disabled, native AS capped at
  768 MiB, and the owned-process guard stops below 768 MiB host available.
  No capture or browser run hit OOM or a guard stop. Node/renderer/driver
  heaps are bounded; browser memory is separate from native RSS.
- **Scope:** the complete native baseline record set, not the separate
  repeat/quantum/expanded/cache/WASM campaign or live input. No full general
  regression suite, commit, push or remote deployment. Full phase 6 remains
  open in the [game record](records/2026-10-21-scripted-games-phase4-6-implementation.md).

### Device I/O hierarchy, overview layouts and scenario ordering

- **Structure:** replace flat device internals with functional structural
  groups for requests, control, decoding, storage and read-data selection.
  All twelve device scenarios were inspected; the router now has ten direct
  children instead of 1,589, and buttons have eight instead of 1,893. Public
  contracts and observer boundaries are retained. Regenerate the five changed
  unit timing entries; embedded/desktop entries remain unchanged.
- **Visualizer:** recalculate affected layouts and give the complete device
  system an explicit CPU/routing/RAM/peripheral floorplan with visible
  captions. Migrate moved saved fidelity paths and unchanged default styles
  while retaining manual choices. Order groups RV32IM, RV32IM programs,
  Device I/O; order components from steps to core, programs 23–27, and device
  helpers through the complete system. Update component references.
- **Checked:** 27 focused native registrations, 21 stored timing comparisons,
  catalog/profile/layout migration checks, sixteen static scenario layouts,
  twelve native/WASM comparisons and the 22-page/13-session browser selection
  pass. Twelve example groups regenerate and strict package/preview builds
  pass. Final system-only geometry checks pass in isolated and live views;
  8893 serves fourteen verified routes and 26 exact assets. The full browser
  selection preceded the final system caption adjustment, which received a
  focused recheck. No complete suites, expanded-core timing certificate,
  game campaign, push or remote deployment. See the
  [record](records/2026-10-24-device-io-hierarchy-and-layouts.md).

### Shared structural constants and arithmetic width labels

- **Constant wiring:** use existing supported-width constant primitives for
  fixed words and share scalar zero/one sources per wiring scope. Arithmetic
  launch/sign defaults avoid splitting and rejoining a constant bus; the
  SRAM/ROM sequencer also shares its zero tie. Both 32-bit arithmetic
  datapaths shrink from 46 children to 14, with one zero-word source each.
- **Inspection:** refresh 61 layout scopes, preserving surviving-child
  migration history and manual choices. Correct step-component pages and
  navigation to `<W>`, list all four widths, and show `<4>` in the menu.
- **Validation:** nine focused native registrations and the catalog retry
  pass; 45 stored timing tables recompute unchanged across three profiles.
  Thirty static layouts, 729 automatic migration entries, three native/WASM
  comparisons, three isolated browser sessions and one live divider session
  pass. Nine regenerated example groups leave the bundle byte-identical.
  Strict package/preview builds and eight routes/26 asset hashes pass on 8893.
  See the [record](records/2026-10-24-structural-constant-words.md) for corrected
  check-launch/filter mistakes. No complete suite, game campaign or push.

### RV32IM algorithms, numbered programs and instruction inspection

- **Fixed:** the core's saved EXECUTE placement still assumed its former
  tall shape. Refreshed M-core floorplans to align stage centers and widths;
  unchanged shipped layouts upgrade while custom/manual placements survive.
- **Instruction memory:** M machines now show all eight M mnemonics in
  structural and compact views. ISA selection follows the containing
  machine's contract; I-only contexts retain strict RV32I decoding.
- **Programs:** number the M scenarios 23–27, preserving existing descriptive
  aliases and adding numbered URLs. Program 26 computes Fibonacci by fast
  doubling with 12 MUL and five DIVU instructions. Program 27 computes GCD
  with REMU/DIVU and a MUL quotient check. Both run loops, store intermediate
  results and verify commits, registers, memory and M retirement counts.
- **Checked:** eight focused native tests (19.59 seconds), six static
  layouts, alias/migration checks, five WASM program exports with decoded
  memory text, strict combined site and two browser sessions (75.54 seconds).
  The refreshed 8893 preview passes eight routes, 26 exact asset hashes and
  live Fibonacci inspection/playback.
  Details and limits are in the
  [topic record](records/2026-10-23-rv32im-program-expansion-and-inspection.md).
- **Limits:** no production circuit/timing-table changes, full suite,
  benchmark/game campaign, push or published-site deployment.

## 2026-10-23

### Grouped RV32IM hardware and dedicated recorded programs

- **Structure:** multiplier/divider now expose CONTROL, OPERANDS, DATAPATH
  and RESULT groups. MulDiv has named result selectors; execute groups its
  M control and selection alongside the I stage and forwarding. Decoder
  overrides and coordinator holds also have separate structural boundaries.
  Existing lower-level component fidelity remains selectable.
- **Examples:** strengthened all ten M component recordings to 7–49 rows,
  adding all M encodings, signed/unsigned boundaries, forwarding and
  pause/hold/take/cancel/reset cases. Added three dedicated visualizer
  programs: arithmetic/x0, dependencies with stores/loads and branch flush,
  and divide/sign edges. They use real memory endpoints, check commits and
  architectural state against the oracle, compare final memory and retain
  inspectable timelines. `recorded-program` selects only those three CTests.
- **Views and timing:** complete shared group layouts and an explicit execute
  floorplan. Migrate moved named register/step fidelity choices and retain
  custom layout settings for children still present. Regenerated the changed MulDiv delay entry;
  its unit certificate is 68 ticks, while the core remains 94. Embedded and
  desktop certificates are unchanged.
- **Checked:** focused native scenarios/programs, registry and migration;
  selected delay recomputation and standalone timing certificates; 14 static
  scenario layouts and 14 regenerated WASM example groups; strict combined
  site; four browser sessions and a two-session final floorplan recheck.
  Port 8893 passes eight routes, 26 exact asset hashes and live program
  inspection/rewind after its strict rebuild. Retries and the later standalone
  MkDocs shell's SIGTERM are recorded in the
  [topic record](records/2026-10-23-rv32im-grouping-and-program-scenarios.md).
- **Limits:** focused acceptance only; no full suite, benchmark/game campaign,
  push or published-site deployment. Unrelated verified example exports and
  I-core type/profile layouts are retained.

### Structural RV32IM/device scenario views and descriptive roots

- **Changed:** renamed 22 M/device/system scenario roots to component-specific
  names, matching existing core views. A shared inspection profile shows
  structural architecture/functional blocks, including MulDiv and the IM core,
  while retaining compact storage and lower-level word choices. The existing
  RV32I core defaults remain structural.
- **Fixed:** root expansion previously built a structural circuit but then
  failed with missing static layout placements. Filled shared structural type
  layouts and current/canonical root overlays from matching saved structures.
  Native/WASM profile migration preserves old `DUT` root/descendant choices.
  MulDiv now places its multiplier and divider prominently above control
  logic; saved unchanged defaults upgrade while custom/manual layouts survive.
- **Checks:** the native build, 28 focused native registrations and static
  layout checks across 25 scenarios pass. Serial WASM/site builds, all 156
  example groups, 22 native/WASM comparisons, frozen Pong playback and the
  22-page/23-session device browser selection pass. A final MulDiv/I-core
  browser check verifies the new floorplan. Port 8893 serves the package:
  seven routes, 26 exact asset hashes and three structural views pass. Failed
  fixture attempts and successful retries are recorded in the
  [implementation record](records/2026-10-23-rv32im-scenario-visualization-fixes.md).
- **Scope:** scenario presentation and saved views; individual component
  circuits, timing tables and frozen gameplay captures retain their contracts.
  Full native/web suites, the remaining game campaign and published-site
  deployment were not run.

### ALU interface: `ALU<W>` in Chapter 2, `RV32IALU` for the cores

- **Why:** the operation-contract switch gave eight ALU unit names two
  unrelated pin sets, and configuration fields were silently ignored. The
  owner chose two named ALUs and dropped the adder-topology parameter
  ([record](records/2026-10-22-alu-exposure-review.md)).
- **Chapter 2:** one header per component; word datapath units take only W.
  `RippleCarryAdder<W>` and `ParallelPrefixAdder<W>` replace `Adder<W>` with a
  topology argument; `AddSub<W>`, `ArithmeticUnit<W>` and `ALU<W>` always use
  the prefix adder. Carry ports are `C_IN`/`C_OUT` (`C_SIGN` on the prefix
  adder). Contract IDs lose the operation and topology segments (34 instead
  of 77). New compact forms: ripple adder at 16/32, prefix adder at 8/16;
  the 8-bit add/subtract, arithmetic unit and ALU forms were recharacterized
  for prefix carry.
- **RV32I:** fixed 32-bit `RV32IALU` and six `RV32IALU*` parts (`rv32i.alu*`),
  with the former integer gates unchanged; opcodes `rv32i::ALUOp` with
  `CLEAR` = 12. The integer full-flag ALU, flag unit and 8/16-bit/ripple
  integer variants are retired. Branch-target and PC adders are requested as
  prefix-adder families.
- **Site:** pages 2.1–2.6 describe only the Chapter 2 components, with a
  ripple-versus-prefix comparison and a construction section; page 2.7 is
  removed; § 4.3.3 now specifies `RV32IALU` and its parts. Visualizer menu,
  palette, layout keys and Python bindings follow the new names.
  `create_rv32i_alu` replaces `create_alu`; `create_word_component` takes only
  unit, name, width and fidelity, and the control decoder is width-independent.
- **Layouts and preview:** migrated 44 saved scopes to current structural
  signatures while preserving geometry. The pruning formatter preserves
  bracketed storage keys. Restored port 8893 with the fresh package and a
  service restart; the timing hook now refreshes its table cache each build.
- **Checked:** guarded serial native/WASM builds; focused native ALU,
  documentation and SRAM/cache layout checks; all 156 generated example groups;
  strict site build; 18 changed tables in all three profiles; all compiled
  native/WASM delay snapshots match (809 entries per profile). The three
  focused portable parity registrations and all six selected browser
  registrations pass, including 731 Chapter 2 table rows, live fidelity
  switching and 32 camera bounds checks. The preview passes six live routes
  and 26 exact non-HTML asset hashes. Failed fixture/environment attempts and
  their successful corrections are retained in the topic record.
- **Limits:** the complete static scan cleared 367 scenarios, then its two
  fetch-root findings were repaired and verified with a focused recheck.
  No full native/web suite, remaining phase-6 game campaign, complete
  gate-level reanalysis of all timing entries, push or published-site check
  is claimed.

## 2026-10-22

### Complete replacement of fixed components with parameterized recipes

- **Components:** `ALU<W>` and arithmetic/logic/shift/compare/flag/select units
  use shared width-driven structural recipes and value rules. Ripple and
  parallel-prefix addition work at 8/16/32 bits; prefix is the ALU default.
  `Mux<Inputs,W>`, `Decoder<AddressBits>`, `RegisterFile<W,Count>`,
  `Incrementer<W,Step>`, iterative M units and bus/pixel helpers replace their
  fixed wrappers. Configured peripherals keep the declared 32-bit MMIO ABI.
  `RegisterFile32x32` is the approved architecture exception and shares the
  generic register-file recipe.
- **Contracts and views:** retired classes/factories/IDs and runtime aliases
  are removed. Callers, exact profile rules, scenario names, saved layouts,
  Python/WASM bindings and current references migrate completely. The ALU
  chapter uses a shared 64-row generator (16-bit prefix as its primary view),
  documents both adders, and owns the integer component references; RV32I
  specifies its configured ALU. Old document routes/anchor aliases are removed.
  Parameter-aware layouts, pin-footprint clearance and family colors support
  the generic parts. The 8893 package includes fresh Pong circuit/evidence
  assets and the current references.
- **Checked:** 23 focused native registrations, two API registrations, seven
  reference/metadata registrations, four catalog methods and six portable/
  browser registrations pass. Portable checks cover 21 extended-ALU scenarios,
  nine integer/Execute scenarios and all 22 M/device recordings. Browser checks
  cover the adder article/embeds, 23 component pages, selected ALU/display/button
  sessions and every retained Pong pin/wire/device state with original playback.
  All 158 example groups and strict package builds pass. Three timing profiles
  are regenerated with 809 entries each; all 780 retained-ID entries are
  unchanged in every profile. Complete unit-file verification passes. Selected
  layout checks and live 8893 routes/assets/retired-anchor checks pass.
- **Game/geometry evidence:** fresh Pong has 18 exact-reference frames and
  identical retained signal vectors, simulated times and event counters to
  the prior capture, with zero setup violations. Current APIs cover 320x200;
  actual CPU high-address stores/geometry reads/PRESENT pass at 128x96/97x65.
- **Limits:** serial guarded local builds/checks, not full native/web suites,
  a controlled throughput benchmark, the remaining phase-6 game campaign or
  phase 7/live input. Embedded/desktop timing files are regenerated, with only
  unit rerun through complete-file verification. Failed migration checks and
  environment attempts are preserved with their successful corrections in the
  [closed replacement record](records/2026-10-22-general-component-retirement.md).
  Unrelated dirty work and deployed native extensions remain preserved; no
  commit, push or published-site verification is claimed.


### Parameterized ALU references through RV32I

- **Changed:** Chapter 2 specifies width-parametric arithmetic/logic, shifts,
  comparison, flags and complete ALUs. Width, operation contract, topology and
  behavioral availability are explicit. The integer component references are
  in §2.7; RV32I §4.3.3 specifies the CPU configuration and Execute wiring.
  Home/navigation, generic API, register-file and core references agree with
  that organization. Concrete example aliases, C++ references and component
  fragments are preserved. The 8893 preview includes the timing-table hook.
- **Checked:** six focused documentation checks pass, including compiled
  reference replay and live pin/fidelity schemas; final integer/timing repeat
  passes; strict MkDocs builds pass. All 22 relevant C++ examples and recorded
  marker identities are unchanged. All 18 affected live articles match the
  strict build, and 989 local article links/anchors pass. Checks/builds ran
  serially under the 768 MiB cap. Missing storage/flag links and the omitted
  timing paragraph found during review are corrected.
- **Limits:** documentation and check-path/expectation changes; existing
  simulator package reused. No full browser/WASM suite, game campaign,
  runtime component change or remote deployment. See the open
  [width record](records/2026-10-22-width-component-audit.md).

### Pong replay through the original circuit visualizer

- **Implemented:** bounded captures from the actual checked game circuit,
  a native `RecordedCircuitProbe`, layout packaging and a captured-state
  transport for the existing canvas/explorer/inspector/timeline. Device views
  show the game screen and input state on their actual components. Progressive
  wiring, a small layout sidecar and a capture-specific detail threshold keep
  replay within the host's limits; existing scenario/layout defaults remain.
- **Verified:** actual Pong rally, 115,686 cycles / 29,005 retirements / 18
  changing frames / zero setup violations; all exact host-reference screens
  and reports; six focused native and three JS checks; strict preview build;
  all 18 retained pin/wire vectors and display/input states in the live
  original canvas, with inspector, rewind, Play and Device views. All 32
  served simulator/circuit artifact hashes match. The first browser attempt
  exposed missing layouts; three guarded memory stops led to the bounded
  implementation before the final browser pass.
- **Limits:** native-produced presentation checkpoints only; no intermediate
  gate transitions, historical RAM pages or live input. Full game/WASM capture
  acceptance is still open in the [phase-6 record](records/2026-10-21-scripted-games-phase4-6-implementation.md).
  No native live-module deployment, linked worktree, commit or remote push.
  Contract: [recorded circuit checkpoints](reference/recorded-circuit-checkpoints.md).

### Clarified game visualization acceptance

- **Recorded:** the owner requires scripted games in the original circuit
  visualizer, with abstract device visuals on the actual CPU/device assembly.
  Existing component views are present, but game/session integration remains
  missing. Added that requirement to the open phase-6 records and status;
  separate recording/GIF checks cannot certify it. Live input remains phase 7.
- **Reviewed:** original canvas/inspector rendering, device metadata, the
  device-system scenario and batch/session boundaries. Documentation only;
  no runtime/UI edits, new tests, builds or interruption of the serial campaign.

### Visible native bring-up and 2048 recordings

- **Changed:** added actual bring-up/2048 JSON captures, animated previews
  and a hash manifest to the public recorded-games page. Restored the main
  8893 preview with the already-built current simulator/example package;
  the previous package is preserved. Its old example bundle had blocked
  strict rebuilds after ALU16 was added.
- **Checked:** bring-up's four native configurations pass; 2048 baseline
  passes four exact host-reference frames. Strict preview build passes
  (55.94 seconds, 178,652 KiB peak RSS), five live routes and all 27 simulator
  plus five export hashes match, and all six saved frames pass live browser
  RGBA/keyboard replay checks. The serial campaign resumes after these checks.
- **Limits:** these are short predetermined recordings. The GIFs replay at
  one frame per second, independently of simulation throughput. Other game
  configurations, game WASM and full browser acceptance remain pending;
  phase 6 is open and phase 7 is unimplemented. No native live module,
  circuit source, commit or remote deployment changes. See the
  [phase-6 record](records/2026-10-21-scripted-games-phase4-6-implementation.md).

### Shared word components and configurable device helpers

- **Implemented:** shared structural builders behind `ALU<W>`, word units,
  adders, muxes, register files and iterative multiply/divide components.
  Operation contracts, adder topology and flag outputs are explicit choices;
  legacy factories, pins, operation maps and profile IDs are retained.
  RV32IM continues to use the shared five-stage assembly.
- **Implemented:** 8/16/32-bit device transport and pixel selectors, validated
  router maps and device-view capabilities. The game peripheral register ABI
  remains 32-bit. Guest drawing now reads physical display geometry and
  aligns stores using the actual row address. Rebuilt all ten audited RV32IM
  fixtures. Public contracts are in the new
  [word component reference](../site-docs/components/word-components.md).
- **Checked so far:** serial isolated native build; 555 distinct selected
  native registrations, including three large SRAM checks; 72 new width/timing
  cases; all 808 stored delay entries across each of three profiles; WASM width
  execution and 15 portable compatibility checks; strict packaging of 158
  example groups; 367 static layouts. Added 23 layout entries without
  changing old entries. The 175-name full Python API snapshot is preserved,
  and all five later additions remain present. Node required an unchanged
  retry without a virtual-address cap. No task command was observed OOM-killed.
- **Open:** current-source compiled-game, game WASM, browser, performance
  and final package acceptance continue in the
  [phase-6 record](records/2026-10-21-scripted-games-phase4-6-implementation.md)
  and [width record](records/2026-10-22-width-component-audit.md). New width
  variants are structural; phase 6 is not yet accepted and live phase 7 is
  unimplemented. The isolated build does not replace the native live module.

### ALU width configuration design

- **Documented:** continued the ALU8/ALU32 comparison in the consolidated
  [width record](records/2026-10-22-width-component-audit.md), reopened for
  design discussion. Proposed one ALU<W> implementation with named semantic
  contracts, explicit adder topology and compatible old catalog factories.
  Word width alone must not change operation maps or flag conventions.
- **Scope:** reviewed current ALU source/public pins and RV32IM composition.
  Confirmed shared pipeline assembly and unchanged fetch/memory/writeback;
  M extends decode, EX and coordination. Recorded duplicated physical operand
  forwarding as a future sharing opportunity. Added an implementation sequence
  and compatibility/validation criteria; no component, test, public page,
  build, simulation or service changed.

### Consolidated component width record

- **Changed:** merged the game-roadmap width analysis into the earlier
  [component width record](records/2026-10-22-width-component-audit.md) at the
  owner's request, retained both sets of dated evidence, updated links and
  removed the duplicate record. The summary now states the protocol/data-
  width refinement and the relationship between the 40-class and 22-component
  inventories.
- **Checked:** dated-body preservation, local reference targets and whitespace.
  Documentation only; no component changes, build, simulation, test suite or
  service restart.

### Game-roadmap width and parameter review

- **Changed:** classified the adopted 22 components into 12 fixed-width
  suffixes, six ISA identities and four parameterized blocks. Proposed reuse
  of the existing data-width protocol, with separate arithmetic, register,
  device-state and display parameters. Updated the roadmap summary for the
  main alpha checkout and current implemented contracts, preserving dated
  sections. See the [review](records/2026-10-22-width-component-audit.md).
- **Reviewed:** current hardware/protocol source, register ABI, public device
  references, observation adapters and guest drivers. Fixed sizes extend to
  operation counters, register transfers, 32 keyboard levels and 64×48 guest
  drawing. The prior geometry work does not establish generic game rendering.
- **Limits:** analysis/internal documentation only; no component/guest change,
  tests, compile, simulation, browser or service restart. Width refactoring
  remains proposed; phase-6/phase-7 acceptance status is unchanged.

### Main alpha checkout and retirement of linked worktrees

- **Changed:** switched the main repository to `alpha`, restored the recent
  preview/audit documentation there and moved port 8893 to the main directory.
  Saved older pending work in a named stash and external backup. Removed both
  linked Git worktree registrations while retaining their source/build/evidence
  directories as ordinary ignored archives with unchanged entry metadata.
- **Verified:** both sets of pending-file hashes in stashes/backups; only the
  main alpha checkout in `git worktree list`; guarded strict preview build
  (43.76 seconds, 179,736 KiB peak RSS); eight local documentation routes and
  all 27 served simulator hashes. Preview peak was 287.72 MiB, with zero OOM
  events or automatic restarts. Existing branch references and prior build
  artifacts are preserved. See the
  [consolidation record](records/2026-10-22-main-alpha-checkout.md).
- **Limits:** no commit/push, native/WASM compile, native regression or
  browser gameplay check. Documentation remains uncommitted on main alpha;
  phase-6 acceptance and phase-7 status are unchanged.

### Fixed-width component inventory and ALU comparison

- **Changed:** recorded 40 public classes named for fixed 8/32-bit widths,
  the additional ALU32Core family, existing parameterized precedents and
  proposed generic implementations. Distinguished data width from opcode
  contracts, mux input count, bus format and framebuffer geometry.
- **Reviewed:** current headers/circuit implementations, public ALU pages,
  catalog/profile contracts, visualizer type matching and test definitions.
  The audit identifies compatibility and four-state/timing differences that
  a future refactor must preserve. See the
  [audit record](records/2026-10-22-width-component-audit.md).
- **Limits:** internal documentation only; no component refactor, test run,
  build, simulation or service change. Existing work remains untouched.

### Local documentation preview follows the alpha checkout

- **Changed:** moved port 8893 to the dedicated `alpha` worktree using a
  local user-service drop-in, including both absolute startup checks.
  Staged its matching simulator package without a compile; retained the
  preview hook and durable runtime directory. The service now has a 768 MiB
  memory limit with no swap.
- **Verified:** guarded strict preview build (46.15 seconds, 180,176 KiB peak
  RSS), eight HTTP documentation routes and all 27 served simulator file
  hashes. Service peak after the HTTP checks was 279.72 MiB, with no OOM
  events or automatic restarts. Primary pending files and the feature
  checkout are unchanged. See the
  [preview record](records/2026-10-22-alpha-local-preview.md).
- **Limits:** reused the milestone's example bundle; no full regeneration,
  browser/runtime validation, native suite, GitHub push or remote deployment
  ran. Phase-6 acceptance remains open; phase 7 has not started.

### RV32IM milestone checkpoint and bounded build tooling

- **Changed:** merged the RV32IM/device/scripted-game checkpoint into `alpha`,
  retaining the newer visualizer work and original playability audit tools.
  Optional WASM link optimization and isolated example generation reduce
  peak memory without changing their defaults. Inspector parity releases its
  captured Python circuit before building the native comparison. A generator check verifies
  full/filtered output identity and failure-safe publication.
- **Verified before integration:** 54 portable checks, 11 portable system
  checks, exact native/WASM comparison of all 22 device scenarios and wide-time
  transport pass. All 175 old Python API entries are preserved, with five
  additions. Strict site packaging passes with 22 current new example groups
  and 135 retained legacy exports. Complete timing snapshots match in native
  and WASM; all 808 entries per profile verify.
- **Integrated checks:** serial native/WASM builds; 80 of 81 selected
  native/Python checks (one resource-limited cache-layout case); all 13 Node
  checks; four portable core/system/input checks; five device parity cases;
  exact-time and old memory parity; complete native/WASM timing identity;
  all 366 static layouts. Strict packaging refreshes five device groups and
  retains 152 earlier exports. Primary pending files remain byte-identical.
- **Limits:** full legacy example regeneration and a large SRAM retry stop
  safely at the host-memory guard. Current full-topology and browser
  attempts also stop there; no current browser appearance pass is claimed. Revised full-game/fidelity/quantum/performance and
  compiled-game WASM comparisons remain follow-up work. Phase 6 acceptance
  is open; phase 7 has not started. Integration results are recorded in the
  [wrap record](records/2026-10-22-rv32im-milestone-wrap-and-integration.md).

### Visualizer: modern look for toolbars, panels and controls (branch `realistic-timing`)

- **Changed:**
  - `visualizer-v2/static/styles.css` is rewritten around theme tokens:
    one dark theme, rounded floating panels, system sans and mono fonts,
    icon buttons (inline SVG masks), captioned Scenario and Timing
    dropdowns, a styled timeline with a progress fill, and a bottom dock
    centered over the visible canvas.
  - `index.html`: transport reordered around Play; icon-only buttons carry
    `aria-label` and `title`. All element IDs are kept.
  - `app.js`: a `playing` class for the pause icon, the timeline fill, and
    checkpoint marks drawn under the track (the styled slider hides the
    datalist ticks); canvas background and background grid are darker.
  - The desktop top bar no longer cuts off the Timing dropdown.
  - Kept mergeable with the uncommitted device I/O work in
    `.worktrees/rv32im-phase3`: the Fit button keeps its original line, the
    toolbar divider comes from CSS, and its `Device views` toggle is styled
    in advance. A three-way merge of the shared files has no conflicts.
- **Why:** the owner found the buttons and windows old-fashioned.
- **Verified:** screenshots at 1440, 1100 and 360 px, program view forced on
  Full Adder, and the docs embed; every top-bar and dock control inside the
  viewport and clickable at 960, 640, 360 and 320 px. Browser checks run
  against a scratch mirror of today's `build-wasm/site` with the edited
  files: `FFMemoryBrowserTest`, `MemoryInspectorBrowserTest`,
  `VisualizerScenarioBrowserTest` and `VisualizerTimingProfileBrowserTest`
  pass. `VisualizerWasmBrowserTest` passes its desktop and compact panel
  checks, then times out at line 618 waiting for the `/visualizer/` embed.
  The unmodified files fail at the same step in that mirror, so it is not
  from this change, but the rest of that test was not reached.
- **Limits:** the Pages build was not run, so the published simulator is
  unchanged. The native server was not used: the deployed
  `circuit_backend*.so` is older than the scenario catalog. Canvas drawing
  (component shapes, wire colors, Arial labels) is unchanged.

## 2026-10-21

### General device parameters and recorded visual abstractions

- **Changed:** framebuffer dimensions and backing capacity are validated
  independently, with surfaces up to 256 KiB. The system propagates capacity
  into video routing and reference checks. Queue depths 1–7 are parameters;
  default geometry, mappings, pins and child configurations are preserved.
  Non-default device configurations have separate visual floorplan keys;
  default keys and existing colors are retained.
- **Visuals:** shared native/Python/WASM observation adapters and browser
  display/keyboard/console/timer/FIFO previews are staged. Recorded-game
  frames carry checked input state; old recordings still open. Captured
  pixels have an aggregate budget. Browser acceptance remains pending.
- **Verified:** serial native build (369268 KiB peak RSS), 28 focused native
  checks (149812 KiB peak), FIFO structural/compact agreement at every depth,
  standalone 320×200 storage, actual CPU presentation at 128×96 and 97×65,
  and read-only pixel/keyboard history with rewind. An initial observer type
  match and test-order error were corrected and the full focused set passes.
  Subsequent parameter/observer/clock checks, Python declarations and JS
  helpers pass. All 363 static scenario layouts pass with 52 new entries and
  no old value changed. Consecutive sampled input logs and halt-boundary
  rejection pass; fresh bring-up has two exact frames, checked reports and
  zero setup violations. CLI timeout/cycle/memory failures and stale-output
  rejection pass, with native build/source/binary/fixture evidence retained.
- **Limits:** the larger CPU checks execute short sequences and write a word
  near the visible surface's end; they establish routing/capture, not full
  screen drawing FPS. Final site/browser/WASM and broader phase-6
  acceptance remain open. Phase 7 is authorized after acceptance. Primary
  checkout work and deployed modules are untouched. See the
  [parameter/visual record](records/2026-10-21-device-parameters-and-visuals.md).


### Scripted platform bring-up and wide-time execution

- **Changed:** isolated RV32IM CPU/RAM/device assembly, batch input and frame
  capture, independent ISA/device checking, audited guest sources and an
  additive recording viewer. Explicit 64-bit simulation time and exact wide
  JSON ticks are a separate prerequisite.
- **Verified so far:** WASM standalone time/history/overflow checks; actual
  short guests with FF, structural logic, cached FF/DRAM and epochs above
  2^53/2^63. Compiled bring-up completed 23,922 cycles / 6,445 instructions,
  two checked frames and zero setup violations in 466.13 s, ~203 MiB peak RSS.
  Revised ten RV32IM fixtures, strict C rule checks and host reference builds
  pass. A guarded multi-configuration run stopped at low host memory; each
  configuration is now a separate check/process.
- **Limits:** bring-up used the preceding structural-M build. New compact M
  source, full game execution, exact binding parity, current native regressions,
  integrated timing and packaged browser/site acceptance remain pending. No
  live-session support, deployment or primary-worktree edits. See the
  [open milestone record](records/2026-10-21-scripted-games-phase4-6-implementation.md).


### Smaller Python binding compilation groups

- **Changed:** moved the module registration statements into six compilation
  groups with private shared templates, called in their original order.
- **Why:** the single binding source exhausted a 768 MiB compiler cap; a
  guarded 1 GiB retry stopped at low host headroom.
- **Verified:** the original registration body reconstructs byte for byte;
  the split builds at 768 MiB with 538,172 KiB peak RSS. Separate-process API
  snapshots preserve all 175 old public names/signatures/docs and add only
  the two recorded-guest functions. Full runtime regressions remain pending.

### Optional bounded compilation of generated timing tables

- **Changed:** a build-only partitioner copies canonical entries into eight
  compiler units per profile; native builds opt in, and the Pages build enables
  it. Public table functions and tracked timing data remain unchanged.
- **Why:** the fresh wasm32 Unit table compile exhausted its 1 GiB compiler
  cap. Keeping smaller units avoids raising memory pressure on this host.
- **Verified:** deterministic verbatim round-trip for all 754 entries in each
  profile and malformed/duplicate input rejection. Saved the preceding native
  value snapshot; fresh native/WASM complete snapshots match it byte for byte
  in all three profiles.
- **Pending:** isolated compiler-memory
  evidence and existing timing checks. See the [open record](records/2026-10-21-delay-table-compile-memory.md).

### RV32IM and device foundations staged in an isolated worktree

- **Changed:** additive structural M arithmetic and five-stage core, independent
  reference semantics and pinned C fixture; device routing, register frontend,
  counter/FIFO, console/timer/buttons and framebuffer storage/control. Added
  component references, recorded examples and viewer metadata/layouts.
- **Compatibility:** old I families retain their identities and private assembly
  paths. Shared assembly is extracted without changing their pins or packets.
  A prerequisite table-only delay-publication guard fixes an existing empty
  vector access; a focused regression covers it. Corrected an existing CTest
  registration-order mismatch for the shared-memory program scenario.
- **Verified:** all 46 exact-label phase-3 native checks pass in 716.91 seconds,
  serial and memory capped, including independent oracle/C execution,
  structural/compact-child core agreement and three-profile timing analysis.
  All existing layout values are preserved; 48 new entries are added.
- **Additional preceding-binary checks:** 23 selected native compatibility
  checks and seven viewer checks pass; memory inspection reached its configured
  180-second limit. All 21 added scenarios have complete saved layouts.
- **Pending:** refresh these checks after the wide-time migration, finish the
  memory-inspector check, and validate the packaged site/WASM. These preceding
  checks do not validate the new integrated machine or games.
- **Scope:** uncommitted `rv32im-phase3` worktree from `60acda2`; primary ongoing
  work and deployed viewer modules are untouched. The owner subsequently
  authorized [phases 4–6](records/2026-10-21-scripted-games-phase4-6-implementation.md).

### Visualizer: two memory panels and one floorplan for every shared arrangement (branch `realistic-timing`)

- **Changed:**
  - Behind a shared cache (D-0, D-ID) the memory inspector tells the sides
    apart by the arbiter's owner bit for the request in flight, so these
    memories show two panels too.
  - One floorplan for all four shared arrangements: core and memory in the
    same place, middle parts spread evenly, constants by their caches, and
    the memory wide enough for its panels. §4.7 is updated.
- **Why:** the owner found D-0 and D-ID oddly laid out and without the
  two-panel view.
- **Verified:** inspector integration (two panels in B, D-0 and D-ID, sides
  at the right addresses), static layouts with the content guard, four other
  layout and catalog tests; Pages build; four browser and parity checks and
  screenshots.

### Visualizer: children kept inside their parent's content area (branch `realistic-timing`)

- **Changed:**
  - The shared-memory program-root floorplans place children inside the
    content area (below the title bar, inside the pin margins), and the four
    roots are recomputed. One `mux2to1-32bit` placement is moved below its
    title bar.
  - `VisualizerStaticLayoutTest` now fails on any placement outside a
    content area.
- **Why:** the owner saw parts over title bars in the new arrangement
  scenarios. A check found 11 such placements in five layouts.
- **Verified:** the static layout check over every scenario (0 problems, and
  it reports the old placements); five layout tests; Pages build; browser
  screenshots of the four scenarios.

### RV32I memory systems section; cache experiments split (branch `realistic-timing`)

- **Changed:**
  - New §4.7 Memory Systems: the five memory arrangements of the advanced
    core, each with an embedded scenario (new: `rv32i-memory-b`, `-d0`,
    `-did`).
  - The results page is now §4.8, with a memory-systems section that merges
    the former §3.8 experiments 2 and 3 as final results. §3.8 keeps the
    cache-alone experiment.
  - `RV32IProgramRoot` builds every shared arrangement, including an L2, and
    the inspector shows one panel behind a shared cache.
  - New floorplan and saved layouts, and a figure rescaled.
- **Why:** the owner pointed out that §3.8 read as history and that the
  arrangement experiment belongs to the RV32I chapter. The owner also wanted
  a scenario per arrangement to show how the arbiter is connected.
- **Verified:**
  - The four arrangement scenarios against the instruction-set reference.
  - Inspector, layout, catalog, topology and clock checks; 18 documentation
    checks; the plot validation (602 cells).
  - Strict Pages build, and 11 browser/parity checks plus a browser load of
    each new scenario.
- **Not run:** the full native suite.

## 2026-10-20

### Site checks: deterministic gate order, figure fixes, records closed (branch `realistic-timing`)

- **Changed:**
  - Gates that were built inside one call's arguments are now created in
    explicit statements, in the native build's order. The affected files are
    `MemoryFFController.hpp`, `DRAM.cpp`, `NORFlash.cpp`,
    `MemorySequencers.cpp` and `TwoPortMemoryArbiter.cpp`. C++ leaves that
    order open, so GCC and clang (WebAssembly) had listed the same parts
    differently.
  - Seven figures get label spacing fixes: cache access patterns, cache
    topologies, memory endpoints, SRAM device, transaction handshake, SRAM
    access chain, memory latency.
  - The simulator page shows its embed before its explanation.
  - `MemoryDeviceDocumentationBrowserTest` counts example tables only.
  - The general cache plan, memory chapter and shared layout keys records are
    closed, and STATUS is updated.
- **Why:** the owed site build and browser/parity checks, run once the
  general cache plan settled.
- **Verified:**
  - Pages build passes, with every example row verified.
  - 26 browser, WebAssembly-runtime and parity checks pass; the parity checks
    used the fresh native probe in `build-cleanup`.
  - Native: 30 memory-labelled tests, 48 further memory tests, the layout and
    catalog checks, and all eight cache tests (including `CacheReadTest`).
  - The unified memory view checked in the browser.
- **Not run:** `PartDelayTablesCompleteTest` and the full native suite.

### Visualizer: cache controller layouts regenerated (branch `realistic-timing`)

- **Changed:**
  - The saved placements of the five cache controllers the scenarios use are
    regenerated, and six unused entries are removed. Their layout keys follow
    the controller's children, which the general cache plan changed.
  - The cache floorplan keeps its old row arithmetic.
  - `CacheControllerEquivalenceTest` is renamed `CacheControllerLockstepTest`:
    registered test names may not contain "Equivalence".
- **Why:** `VisualizerStaticLayoutTest` reported 1,351 problems once the
  Python module was rebuilt.
- **Verified:** `VisualizerStaticLayoutTest` (0 problems), `CacheLayoutTest`,
  `VisualizerScenarioCatalogTest`, `RV32IMemoryLayoutTest`,
  `MemoryInspectorIntegrationTest`, `VisualizerSessionContractTest`,
  `VisualizerAutomaticLayoutTest`, `VisualizerModuleBindingsTest`,
  `BuiltinComponentCatalogInventoryTest`, `MemoryDeviceDocumentationTest`.
- **Not run yet:** the Pages package and browser checks (a memory-gated run
  is in progress).

### Cache behavior campaign and rewritten cache pages (branch `realistic-timing`)

- **Changed:**
  - §3.7 (`site-docs/memory/caches.md`) is rewritten: every setting (write
    policy, write miss, write buffer, replacement, fill, line state,
    controller), burst reads, invalidation with flush, the line-state store,
    the controller's parts and the compact controller.
  - §3.8 gains experiment 3, cache behaviors: write-through, a two-entry write
    buffer, write-back with allocate, and write-back with four ways, on C-ID,
    D-0 and D-ID, four programs and both profiles.
  - The simulator page describes the unified memory view.
  - New `tools/rv32i-performance/cache_policy_campaign.py`, and the tools
    README covers cache options.
  - The experiment's clock analysis has a larger node budget (three four-way
    caches exceeded the default).
- **Why:** step 7 of the general cache plan, and the owner's request to update
  the site docs once the plan was done.
- **Verified:**
  - 96 of 96 runs verified, with zero setup violations.
  - The 24 write-through runs reproduce the published campaign exactly.
  - The clock analysis certifies all 24 combinations at 4,104 / 2,858 ps.
  - Strict MkDocs build, `CacheDocumentationTest` and
    `DocumentationDiagramsTest` pass.
- **Results:** a write buffer gives 1.18–1.57× on programs with stores and
  removes most of the L2 penalty; write-back gains less and costs 9–21 % on
  `stream`; four ways change little except for the L2 alone.
- **Not run:** the Pages package, the browser checks and
  `MemoryDeviceDocumentationTest`. The last fails on the ROM page's `READY`
  pin with the stale native probe in `build-cleanup`, and is to be rechecked
  after the rebuild.

### Cache: compact controller running the gate list (branch `realistic-timing`)

- **Changed:** the cache controller is now a catalog contract,
  `memory.cache.controller.<W>`. The cache setting
  `controller=structural|behavioral` picks gates (the default) or a compact
  part.
  - The compact part runs the gate-level controller's own gate list
    (`GateListModel`: four-state gates, compact state rules, only the bits a
    change reaches).
  - It proves its delay table on the same gates when built.
  - Experiment tool option `--cache-controller`.
  - New `CacheControllerEquivalenceTest`.
  - The timing graph no longer treats a passed-on clock as a data output.
  - `part_delay_tables` skips the contract.
- **Why:** step 6 of the general cache plan, revised from a hand-written
  behavioral cache. The controller is the slow part, and running its gate
  list keeps every cache rule in one place (see the record).
- **Verified:**
  - Lockstep equivalence test: 8,561 steps identical.
  - S0–S2 traces: 24 of 24 records identical; cache paths 6.3 times faster.
  - CPU runs on C-ID, D-0 and D-ID, including write-back, store-mode, 4/8-way
    and buffered caches: identical cycles and final state, with no setup
    violations. D-ID `sort`: 325 → 1,646 cycles per second.
  - 4,104 ps certified with either controller.
  - Catalog, profile, topology and cache scenario tests pass.
- **Not run:** `PartDelayTablesCompleteTest`, and the visualizer and browser
  checks (pending).

### Cache: 4 and 8 ways, a choice of replacement (branch `realistic-timing`)

- **Changed:** caches take 1, 2, 4 or 8 ways and `replacement=lru|plru|fifo|random`.
  Exact lru works for up to two ways; with more ways the default is plru.
  The controller carries way indexes instead of one way bit. The two-way lru
  structure is unchanged, and the new policy bits sit in flip-flops
  (`CachePolicyBits`) or in the line-state word. The cache floorplan takes up
  to eight way rows and the line-state store. The cache test rig no longer
  records wire history.
- **Why:** step 4 of the general cache plan. Next-line prefetch (step 5) is
  deferred, because the controller serves one request at a time (see the
  record).
- **Verified:** `CacheReadTest` passes, with eight new reference-model streams
  covering every policy at 2, 4 and 8 ways, in flip-flop and store modes.
  `RV32ITopologyTest`, `RV32IBinaryClockCLITest` and the cache scenarios pass.
  D-ID cycle counts are unchanged for the existing caches (6,481 and 4,796),
  and the 4- and 8-way variants verify. 4,104 ps is certified with 4- and
  8-way caches.
- **Not run:** the Python module was not rebuilt (too little free memory),
  so the full `CacheLayoutTest`, `VisualizerStaticLayoutTest` and the browser
  checks are pending.

### Cache: line-state store for large caches (branch `realistic-timing`)

- **Changed:** `line_state=store` keeps the valid, dirty and replacement
  bits in a memory bank (one word per set) instead of controller flip-flops,
  lifting the set limit to 128 (gate-level) or 4,096 (scalable); the store
  is cleared set by set after reset and on invalidation. The default
  (`registers`) is unchanged.
- **Why:** step 3 of the general cache plan, revised from "bits in the tag
  words" to keep existing caches and results unchanged (see the record).
- **Verified:** `CacheReadTest` (store-mode streams including 256 sets), the
  cache scenarios, `RV32ITopologyTest`, a store-mode D-ID run.

### Cache: write-back and write-allocate (branch `realistic-timing`)

- **Changed:** caches take `write_policy=back` (dirty bits, write-back of a
  dirty victim before its refill, a flush walk before invalidation) and
  `write_miss=allocate` (a write miss fills its line, then writes into it).
  The experiment tool flushes the L1s, then the L2, after the program stops
  (new inputs `FLUSH_L1`, `FLUSH_L2`, fixed low for the clock analysis).
- **Why:** step 2 of the general cache plan.
- **Verified:** `CacheReadTest` with a write-policy reference stream,
  `RV32ITopologyTest`, `RV32IBinaryClockCLITest`, the cache scenarios; D-ID
  certified at 4,104 ps with write-back caches.

### Cache: write buffer (branch `realistic-timing`)

- **Changed:** caches take a `write_buffer` of 1, 2 or 4 entries: writes are
  answered in their lookup cycle and drained to the backing memory in order
  while no fill needs the lower port; a read miss waits for the drain. New
  structural part `CacheWriteQueue<W>`. The experiment tool takes per-cache
  settings (`--il1/--dl1/--l2 KEY=VALUE`) and drains buffers before checking
  memory.
- **Why:** step 1 of the general cache plan; write-through to two levels was
  the measured cost of the L2 under the L1s.
- **Verified:** `CacheReadTest` with a new reference-model stream, the four
  cache scenarios, `RV32ITopologyTest` (buffered D-ID `sort`: 4,765 cycles
  against 6,481, verified).

## 2026-10-19

### Visualizer: unified memory view and a shared-memory RV32I scenario (branch `realistic-timing`)

- **Changed:** a memory shared by instruction fetches and data accesses is
  drawn as two panels (instructions disassembled, data words), each around
  its latest access, telling the two apart by the arbiter's grant. New
  scenario `rv32i-shared-memory` (program 21, advanced core, two caches, one
  memory behind the arbiter) with its own floorplan. The structure hash of
  cache and predictor layout keys now includes child types; layouts migrated.
- **Why:** the owner asked for a two-panel view of unified memory.
- **Verified:** ten focused checks (layouts in all 341 scenarios, catalog,
  inspector parity including the new unified check, the scenario itself).
  Browser checks pending.

### Shared memory: refuse overlapping instruction and data images (branch `realistic-timing`)

- **Changed:** a topology run (`--topology`, every arrangement, since A loads
  the whole image into both DRAMs) and the program root's unified memory now
  stop when an instruction and a data block put different bytes at one
  address. Program 19 (code to 0x108, data from 0x100) and `load-use` /
  `load-scheduled` at 340–341 elements (code reaching the input at 0x1000;
  larger sizes already fail on branch range) are refused; before, the merged
  image silently held a different program.
- **Why:** the owner asked where I and D sit in one memory; code starts at 0
  and data at its fixed addresses, with nothing checking for collisions. No
  published result is affected (the cache campaign used `calls`, `sort`,
  `stream`, `pointer-chase`; fixed programs ran with separate memories).
- **Verified:** `RV32ITopologyTest` with a new refusal case (A and B).

### Two-port memory arbiter: catalog, scenario, structure and page 3.5.3 (branch `realistic-timing`)

- **Changed:** `TwoPortMemoryArbiter<W>` gets catalog contracts
  `memory.arbiter.<W>`, a recorded scenario (`memory-arbiter`), a floorplan,
  a figure and a component section in 3.5. Its flat gate list is grouped into
  `GRANT`, `STATE` and `RESPONSE_ROUTE` with unchanged gates. The separate C++
  test program became the registry test `TwoPortMemoryArbiterTest`, now also
  at width 16.
- **Why:** the owner asked to document the arbiter in 3.5; it had no page,
  scenario or contract.
- **Verified:** 28 CTest checks (documentation, layout, catalog, both arbiter
  tests, `RV32ITopologyTest`), the scenario in all three profiles, the page's
  C++ reference against all 53 recorded values, strict MkDocs with preview
  rows. WebAssembly rows and browser checks pending in the memory-gated run.

### Live interaction and real-time execution design

- **Changed:** extended the [device/game record](records/2026-10-19-device-io-games-plan.md)
  with a persistent session, queued input, current-state observation,
  cooperative execution and wall-clock pacing proposal. The proposed order
  now proves live operation before MMIO/display integration.
- **Why:** the owner asked how to move from recorded playback to real
  interaction. Source inspection confirms forward execution already exists;
  it also identifies past-event acceptance, partial-timestamp yields and
  wasm32 `size_t` time range as work for a live runtime.
- **Reviewed:** simulator, component/clock/memory state, viewer/worker and
  WASM build metadata; official Emscripten and ns-3 references. The 4.29 ms
  range at 1 ps is inferred from 32-bit time, not a reproduced failure.
  No code, test, benchmark or browser run was performed.

### Device I/O and games planning resumed

- **Changed:** added a focused [planning record](records/2026-10-19-device-io-games-plan.md)
  and linked it from status and the existing future-steps record. It records
  the current shared-memory/cache foundations, missing device/live-runtime
  contracts and proposed stages toward guest-executed Pong and Tetris.
- **Why:** the owner considers the primary cache implementation ready and
  wants to discuss the next milestone. Architecture, first game, ISA choice
  and performance targets remain proposals or open decisions.
- **Reviewed:** current source, test sources, public references and earlier
  records; official RISC-V ordering and arithmetic-extension specifications.
  No build, tests, benchmarks or live play check was run. Existing source
  and visualizer changes were preserved.

### Cache experiments page; RV32I results rewritten (branch `realistic-timing`)

- **Changed:** new § 3.8 Cache Experiments in the memory chapter: the cache
  campaign presented as a memory-system study (access patterns with no
  processor first, then programs over five arrangements), with two new charts
  drawn from the page's tables and the arrangements figure moved there. § 4.7
  rewritten: short answer, how to read (targets, cycles and time, checks), one
  summary table, then each workload group as what it tests, its table and
  what it shows; the cache section is gone from it. Tables unchanged cell for
  cell; old anchors kept.
- **Why:** the owner asked for a cache experiment section without RV32I and
  a complete, easier rewrite of the RV32I experiment page.
- **Verified:** 18 documentation checks, the plot script's validation of 602
  table cells, figure check, strict MkDocs build, screenshots of both pages and
  charts. Browser checks pending in the memory-gated run.

### Visualizer: one saved floorplan per internal structure (branch `realistic-timing`)

- **Changed:** SRAM and FF memory devices get layout keys that include their
  storage bank; caches, predictor tables and entries, the branch predictor and
  memory timers keep their child-name hash key (Python server and C++ session
  alike). SRAM floorplans seed their children's proportions before placing
  them. The layout file's formatter no longer rewrites bracketed text inside
  strings. `layout.json` gains the 19 new keys with regenerated placements;
  the squeezed 8-row SRAM bank is recomputed; 10 unread profile entries are
  removed. `tools/check-static-layouts.py` fails on a key shared by parts
  with different children and on squeezed SRAM floorplans.
- **Why:** the owner found the scalable cache's `TAG_STORE_1` drawn broken;
  all four of its SRAM stores were squeezed into a 2 % strip, and six more
  keys were shared by different structures.
- **Verified:** 13 native layout and visualizer checks, including
  `VisualizerStaticLayoutTest` (0 problems in 339 scenarios) and C++/Python
  key agreement. WebAssembly rebuild and browser checks pending; see
  [the record](records/2026-10-19-shared-layout-keys.md).

### Component timing: only where it says something, inside the implementation (branch `realistic-timing`)

- **Changed:** 67 stock per-component Timing sections removed (a fixed
  sentence plus, at most, the full path table). The 17 components whose
  timing has something of its own to show (adders and ALUs, flip-flop,
  register, register file, memory devices, controller, endpoint,
  single-cycle core) end their implementation with a **Timing** paragraph:
  what sets the delay, and a generated table of only the paths that matter
  in every profile, through the new `<!-- timing-rows -->` marker. The
  paragraph ends "Behavioral implementation" (structural for `FullAdder`
  and `DFlipFlop`, which have no compact form), so component pages have five
  headings again. The paragraphs state facts inline instead of linking to
  § 1, § 3.6 or the 4.2 certificates; DRAM and NOR show their latency rows
  and the core its certified clocks directly. The template descriptions on
  § 1, the ALU8 conventions, 4.3 and 3.1 follow.
- **Why:** the owner found the sections disappointing and asked to remove
  those with nothing to say, not to send readers to the timing chapters,
  and suggested merging timing into the behavioral implementation.
- **Verified:** all 18 CTest `documentation` checks, including the updated
  heading checks and `TimingTableDocumentationTest` with the new marker and
  a guard against `### Timing` headings and timing-chapter links. The Pages
  rebuild and the browser checks run in a memory-gated run; see
  [the record](records/2026-10-18-memory-docs-rewrite.md).

### Timing across the site: clock certificates, results per profile, figures and plots (branch `realistic-timing`, documentation phase)

- **Changed:** 4.2 gains per-profile certified clocks (generated) and the
  cached-root and clock-wire rules; 4.7 states its results in cycles with
  per-profile summaries from the current certificates (advanced two-bit
  now leads 13/14 nontrap fixed programs); the timed-memory rows of the old
  memory design are replaced by a pointer to 3.6; the visualizer page
  describes the Timing selector; the home page gets an overview. Seven new
  figures in the site's figure format and colors (gate delays per profile,
  the SRAM access chain, DRAM reads by row state, read latency by
  technology, the transaction handshake, the SRAM device, the ALU8
  datapath), charts drawn from the generated timing data; the four § 4.7
  plots regenerated at the `embedded` clocks in the site palette. A
  diagram-design profile (`circuitsim`) holds the site tokens; the project
  marker `.diagram-design` selects it. Browser and parity tests follow the
  new memory URLs.
- **Why:** the owner asked for timing-aware pages, a site-wide figure
  review with diagram-design in the site's design, and no hand-typed
  timing numbers.
- **Verified:** strict MkDocs build; `DocumentationDiagramsTest`,
  `RV32IBenchmarkDocumentationTest`, `MemoryDeviceDocumentationTest`,
  `TimingTableDocumentationTest`, `DocumentationTimingDataTest`; the plot
  script validated all 602 cycle cells against the campaign data; figures
  inspected in light and dark; `tools/build-pages.sh` (before the caches
  section). Browser checks: 4 of 19 passed before the run was stopped for low
  host memory; the other 15 are not run.
- **Also:** § 4.7 publishes the cache campaign (five memory topologies,
  four workloads, both realistic profiles, standalone locality traces),
  tables generated from the run records, with a topology figure.

### Memory chapter rewritten; timing sections on every component page (branch `realistic-timing`, documentation phase)

- **Changed:** `site-docs/memory/` restructured and rewritten against the
  current code: 3.1 Memory Architecture, 3.2 Shared Building Blocks (with the
  timing and control parts), 3.3 Memory Technologies (3.3.1–3.3.6 as
  subsections), 3.4 Transaction Protocol (READ_BURST, registered
  `REQ_READY`, `LAST`/`SPACE`), 3.5 Controllers and Endpoints (gate-level
  sequencers, current configuration), 3.6 Memory Timing (new), 3.7 Caches.
  New top-level "§ 1. Time and Delay". The memory, ALU8 and RV32I 4.3–4.4
  component pages split "Unknown inputs and timing" into "Unknown inputs"
  and a "Timing" section with generated delay, interval and clock tables;
  the abstract-tick disclaimers are gone. Long controller and endpoint C++
  listings are replaced by a request check and an endpoint response
  function. Links from the RV32I pages follow the new addresses.
- **Why:** the owner asked for a complete memory rewrite after testing, with
  the technologies under one section, and for the site to state timing from
  the code (profiles, proven tables, measured latency).
- **Verified:** strict MkDocs build; `MemoryDeviceDocumentationTest`,
  `MemoryEndpointIntegrationTest` (the documented request check equals the
  production check in every case), `DocumentationExamplesTest`,
  `RV32IBuildingBlocksDocumentationTest`, `RV32ISingleCycleDocumentationTest`,
  `RegisterFileDocumentationTest`, `RegisterDocumentationTest`,
  `CacheDocumentationTest`, `DocumentationDiagramsTest`,
  `TimingTableDocumentationTest`.
- **Not yet:** browser tests that open the old memory URLs, the diagram
  review, 4.2/4.7/visualizer pages, the Pages build.

### Documentation timing data (branch `realistic-timing`, phase 8)

- **Changed:** new tool `documentation_timing_data` writes
  `tools/timing/documentation-timing.generated.json` (leaf delays, measured
  flip-flop and SRAM-cell contracts, certified periods, memory intervals,
  access latency of each technology at the advanced core's clock); new
  markers `profile-table` and `latency-table` in `tools/mkdocs_timing.py`.
- **Why:** no timing number on the site is typed by hand.
- **Verified:** `DocumentationTimingDataTest` (measured again, equal) and
  `MemoryLatencyGatesTest` (compact and gate-level endpoints take the same
  cycles in all three profiles, 38 of 38). Latencies equal the phase-6
  records.

### Static visualizer: complete layouts, compact Timing control, layout check (branch `realistic-timing`, phase 8)

- **Changed:** `layout.json` gains the 13 child placements the static site
  lacked (FF controller RUNNING/READY/NOTHING_HELD, a memory timer stage,
  the two-level cache's L1 variant); the compact view places the Timing
  selector beside the scenario; new `tools/check-static-layouts.py`
  (CTest `VisualizerStaticLayoutTest`) replays the browser's layout lookup
  for every scenario; new browser check `VisualizerTimingProfileBrowserTest`.
- **Why:** the endpoint scenarios stopped in the WebAssembly site on a
  missing placement, and the compact view hid the scenario selector.
- **Verified:** the Pages build (WebAssembly, example rows, strict MkDocs);
  browser checks `VisualizerWasmBrowserTest`,
  `VisualizerTimingProfileBrowserTest`, `MemoryEndpointBrowserTest`; the
  layout check over 339 scenarios (0 missing).

### Visualizer: choose the timing profile (branch `realistic-timing`, phase 8)

- **Changed:** a Timing selector (`unit`, `embedded`, `desktop`) beside the
  scenario, kept in the URL as `?timing=`; the circuit is built and
  simulated under it and times are shown in ps, ns, µs or ms when the
  profile's tick has a length. Native server: sessions per scenario and
  profile, the library's profile set under one lock while a session is
  built and restored afterwards; `timing` accepted by the circuit, state
  and apply requests; the topology response names the profile. Python
  binding `timing_profile_info`; WebAssembly `setTimingProfile` and
  `timingProfileJson`.
- **Why:** the realistic profiles were reachable only from the command line.
- **Verified:** `VisualizerTimingProfileTest` (new),
  `VisualizerReferenceProfileTest` (extended), the visualizer CTest group
  (only the known `site-docs/` check fails).
- **Limits:** the WebAssembly build and browser checks were not yet run
  when this was committed.

## 2026-10-18

### Program fixtures and fixed-time scenarios under every timing profile (branch `realistic-timing`, phase 8)

- **Changed:** `analysis::CERTIFIED_CORE_PERIODS` stores the certified FF
  clock of each core per profile (`rv32i_experiment
  --list-certified-periods`). The RV32I program fixtures keep their `unit`
  times and multiply them, under another profile, by the smallest whole
  factor that reaches the certified clock (`rv32iFixtureTimeScale`; the
  baseline fixture on its half cycle, since it releases reset half a cycle
  before its first edge). `FullCircuitTest` and `CacheTwoLevelScenarioTest`
  scale their times by gate delays.
- **Why:** the visualizer is to offer the realistic profiles; every program
  scenario failed under `embedded` because its times were `unit` ticks.
- **Verified:** the 179 program and fixed-time scenarios under `embedded`
  and `unit` (all pass; under `embedded` 1 passed before); baseline
  scenarios under `desktop`; `RV32IProfileSweepTest` with the new check
  that the stored periods equal the analysis.
- **Limits:** the full native suite was not rerun for this change yet (it
  runs once more at the end of phase 8).

### Clock certified for cached and shared roots (branch `realistic-timing`, phase 8)

- **Changed:** the two-port arbiter is clocked by I_CLK instead of an AND
  of both port clocks (D_CLK must carry the same clock; pins unchanged).
  The safe-clock analyzer treats a zero-delay one-bit Rewire fed by the
  clock as part of the clock net and states it as an assumption.
- **Why:** the analyzer refused every cached or arbitrated root. The
  arbiter's gate put one gate delay of skew on its clock; the caches hand
  their clock to the banks through zero-delay wires, which the delay-table
  analyzer already accepted.
- **Verified:** `--safe-clock` on A, B, C-ID, D-0 and D-ID in `embedded`
  and `desktop`: all safe at the core's clock (4104 / 2858 ps), no
  diagnostics; `--period auto` runs C-ID and D-ID verified with 0 setup
  violations; B and D-ID `sort` reruns match the campaign exactly. Tests:
  SafeClockTimingTest (new clock-wire case), RV32ITopologyTest (new
  certified-clock case), MemoryEndpointSafeClockTest, RV32IExperimentTest,
  RV32IExperimentCLITest, RV32IProfileSweepTest, TwoPortMemoryArbiterTest,
  CacheReadTest.
- **Limits:** the period is the core's (its port setups dominate); the
  caches' own shortest safe clock is not computed. Endpoints behind caches
  are analysed with all arcs, without handshake-state cuts.

### Visualizer data for the cache and memory timing parts (branch `realistic-timing`, phase 8)

- **Changed:** `layout.json` drops entries of removed parts and stale cache
  variants, rebuilds the three cache scenario floorplans for the direct
  banks, and adds default layouts for new parts (memory timers, sequencers,
  request checks, cache bank controller). `palette.json` colors the cache
  family and the memory timing parts.
- **Why:** three visualizer tests failed on layouts saved before phase 7a
  and on new types without colors.
- **Verified:** CTest label `visualizer`, 28 / 29 (the failure is the
  unchanged `site-docs/` checks of MemoryEndpointIntegrationTest).
- **Limits:** a full automatic refresh would also move saved children of
  nine entries this branch did not touch; they were left unchanged. Browser
  and WebAssembly checks were not run.

### Delay twin: a flip-flop stuck at X is timed from every edge (branch `realistic-timing`, phase 8)

- **Changed:** `DelayTwin.cpp` renews a flip-flop's launch time at every
  clock edge (and unknown reset) while its value is X, not only when the
  held value changes.
- **Why:** the strict-settling delay rule failed on five memory
  controllers. Traced on `memory.rom.controller.32`: an earlier situation
  left the twin's state X while the gates resolved; X looked unchanging, so
  paths from DONE and LAST counted as quiet and REQ_READY settled one tick
  earlier in the compact part than in the gates.
- **Verified:** DelayRuleTest `--strict-settling` on the five controllers in
  `unit`, `embedded` and `desktop` (0 breaks, 0 target misses); full suite
  under `unit`, 461 / 467 (the six failures are the known documentation
  and visualizer-data checks).

### Test timing audit: realistic profiles covered by their own tests (branch `realistic-timing`, phase 8)

- **Changed:** new `RV32IProfileSweepTest` runs every core on programs 1,
  9, 16, 19 and 20 under `embedded` and `desktop` at the analyzer's
  certified clock (single-cycle and baseline with FF memory; advanced with
  FF, on-chip SRAM and DRAM), oracle-verified, no setup violation. The tests
  guide states that the native suite is a `unit` suite.
- **Why:** under `embedded` 217 of 466 tests fail because they are written
  in `unit` ticks (exact tick counts, or clocks and sampling points in
  ticks); rewriting them would duplicate the phase 6.5 core sweep. A
  harness-only scaling attempt fixed none and was withdrawn.
- **Verified:** full suite under `unit` (459 / 466; the failures are six
  documentation or visualizer-data checks and the strict-settling delay
  rule, recorded as an open delay-twin defect); full suite under `embedded`
  (249 / 466, recorded as the evidence); `RV32IProfileSweepTest` (50 runs,
  113 s).

### DRAM command counters in the experiments (branch `realistic-timing`, phase 8)

- **Changed:** the compact DRAM control counts the commands it accepts;
  `BehavioralMemoryEndpoint::commandCounts()` reports them and
  `rv32i_experiment` adds `DEVICE_<memory>_<command>` per measured window.
  Benchmark-record units for observations are now requests, words,
  commands or cycles (all were cycles).
- **Why:** the cache campaign's shared-memory slowdown was explained by DRAM
  row changes without measuring them. Measured: B's shared DRAM opens rows
  23× as often as A's two memories in `sort`.
- **Verified:** the A and B `sort` runs (cycle counts unchanged),
  `RV32IExperimentTest`, `RV32IExperimentCLITest`, `RV32ITopologyTest`,
  `MemoryEndpointDRAMTest`, `NativeMemoryDeviceContractTest`. The standalone
  cache benchmark with gate-level banks matched the compact banks exactly
  after the SRAM macro fix.

## 2026-10-17

### Clocked SRAM macro: no write disturb on the next access (branch `realistic-timing`, phase 8)

- **Changed:** the gate-level clocked SRAM macro's settle line (how long its
  registered address and data are held back when an access starts) now
  covers the row decoder's enable path and the write drivers' turn-off
  (`circuit::sram::clockedSettle`); the compact macro's withdrawal and the
  fallback access times use the same value. Tables regenerated.
- **Why:** a finished write left its drivers and word line on; at the next
  access the new address and data reached the array 50 ps before the old
  word line closed (`embedded`), writing the new word into the old row
  (writing word 0 changed word 4, and so on). Found as stale tags in a cache
  bank; compact banks were not affected. A first fix that ended the write
  drive at READY was withdrawn: it added a 0.7-ns false path to the proven
  read-data launch.
- **Verified:** a write-after-write disturbance test (0 pairs in all three
  profiles; 4 at `embedded` before), the SRAM device tests, tables at a fixed
  point and recomputed identically, DelayRuleTest on the SRAM parts in all
  three profiles, the on-chip-SRAM CPU panel (unchanged), the CLI test, the
  standalone cache benchmark (identical to the campaign run).
- **Limits:** every clocked SRAM access starts about 310 ps later at
  `embedded` (a real decoder-reset delay); certified CPU clocks and cycle
  counts did not change. Cached roots now fail below 3300 / 2300 ps
  (`embedded` / `desktop`), leaving 24 % margin at the campaign clocks.

### Cache experiment runs and locality traces (branch `realistic-timing`, phase 7d)

- **Changed:** `CachePerformanceBenchmark` gained `stride-line` and
  `stride-row` traces (the same 2048 reads with one word per line, or per
  DRAM row) and builds its parts with the campaign profile (compact banks,
  as the CPU runs); diagnostic options run one trace/path or chosen
  addresses. No model or timing change.
- **Why:** the cache plan's Step 2 and Step 3, and the owner's question
  whether locality-friendly code is faster.
- **Verified (results, not tests):** standalone traces in `embedded` and
  `desktop`, every read and the final DRAM image checked; the five-topology
  CPU panel (calls, sort, stream) and pointer-chase in both profiles, 40
  runs, all verified against the oracle. Key figures and the counter-based
  explanations are in the realistic-timing progress log.
- **Limits:** exploratory: cached roots are not certified (run at the core's
  clock with a measured margin); the campaign used compact banks while the
  gate-level macro has an open defect inside the cache; DRAM row changes and
  refreshes are not counted in the CPU runs, so the shared-memory slowdown's
  row explanation is an inference. Results are not published.

### Cache experiment: topologies, standalone benchmark, arbiter fixes (branch `realistic-timing`, phase 7c)

- **Changed:** `rv32i_experiment --topology A|B|C-ID|D-0|D-ID` builds the
  campaign's five memory systems (two memories; one memory behind the
  two-port arbiter; L1s, a shared L2, or both) with the frozen geometry
  (`campaignCache`: 8 or 64 sets, two ways, 16-byte lines, SRAM banks, burst
  fills), whole-image loading and boundary counters. `TwoPortMemoryArbiter`
  takes a burst length and keeps a granted burst to its last word, and has
  a one-entry response buffer per port: a response leaves the bus into its
  owner's buffer, so a core stalled on its other port no longer deadlocks
  the shared bus. New `CachePerformanceBenchmark` (S0–S2 standalone traces,
  in the memory benchmark runner). A clocked SRAM macro no longer gets a
  delay twin (it changed nothing but cost a cached CPU 3–40× run time). The
  generated benchmarks accept up to the data layout's 1024 words.
- **Why:** the cache plan's Step 2 and Step 3 infrastructure; the arbiter
  defects were found by the new topology test.
- **Verified:** Step 1 in both profiles (L1 hit 2, L2 hit 8, DRAM 8–22
  cycles); `RV32ITopologyTest` (new: all five topologies verify a sort, plus
  a program through D-ID over SRAM), `TwoPortMemoryArbiterTest` (rewritten,
  burst cases), cache scenario tests, `CacheReadTest`,
  `RV32IMemoryMigrationTest`, `RV32IExperimentCLITest`, the benchmark tests;
  DelayRuleTest on the SRAM parts without the twin in all three profiles;
  the standalone benchmark in `embedded` and `desktop`, every read verified.
- **Limits:** cached and arbitrated roots are not certified by the analyzer
  (no all-state contract for an endpoint behind them); they run at the
  core's certified clock, measured to be at least 37 % (`embedded`) and 43 %
  (`desktop`) above their failure point, and below that point they fail
  silently (the cache's state bits have no setup check). Open defect: the
  gate-level clocked SRAM macro corrupts a row neighbour when driven by the
  cache; compact banks (the campaign profile) are correct. The CPU panel is
  not yet run.

### Burst reads and burst-filled cache lines (branch `realistic-timing`, phase 7b)

- **Changed:** a burst read (op 4, `Operation::ReadBurst`) returns the
  `block_words` words of the aligned block from the requested word,
  wrapping, one response per word. The transaction port reports and uses
  LAST (a burst holds the port until its last word) and SPACE; the static
  sequencer steps a word address within the block (the clocked macro is
  given the next word as one is taken); the DRAM command control issues one
  READ per word in the open row (`block_words` ≤ `words_per_row`). The cache
  accepts op 4 (a hit streams the line from its data bank, a miss forwards
  the fill), has `fill=words|burst` (one op-4 request per miss), and fills a
  line from the requested word in both modes; its response still waits for
  the whole line. `block_words` = 4 has its own proven delay tables
  (`@block=4`). The safe-clock handshake model takes the multi-word states
  only for requesters that can ask for bursts. Fixed on the way: the
  transaction-port scenario's undriven LAST, the cache's top-level helper
  names (the floorplan rejected them since 7a), and MemoryEndpointTimingTest's
  pre-6.4a expectation that a request is taken over a held response.
- **Why:** word-by-word fills made an L1 miss cost about five DRAM reads (38
  cycles against an 8-cycle row hit), a protocol defect against caches. With
  bursts the cache plan's Step 1 order holds at `embedded`: L1 hit 2, L2 hit
  8, L1 miss 14 (DRAM open row 8), L1+L2 miss 17 cycles. No memory interval
  was changed.
- **Verified:** tables regenerated to a fixed point (754 entries per
  profile) and recomputed identically in all three profiles; DelayRuleTest
  on 14 memory contracts with their variants (24 parts) in `unit`,
  `embedded` and `desktop`: no rule break, no target missed; a 42-test
  CTest subset at `unit` (tables, timing, safe clock, memory endpoints,
  controllers, devices, contracts, caches); the CPU panel (66 runs, all
  verified): unchanged at `embedded` and `desktop`.
- **Limits:** the advanced core's `unit` safe clock with clocked SRAM rose
  158 → 174 ps: the burst gates on the macro's request-to-CE_N path,
  extrapolated to 1024 subarrays (recorded in the progress log); `embedded`
  and `desktop` clocks did not change. Documentation checks
  (`MemoryDeviceDocumentationTest`, three `MemoryEndpointIntegrationTest`
  checks) and CacheLayoutTest's saved layouts fail until phase 8. NOR flash
  received unused `@block=4` tables (it rejects bursts); they go at the next
  regeneration. The full suite was not run.

### Cache with directly driven banks (branch `realistic-timing`, phase 7a)

- **Changed:** `Cache<W>` looks a request up in every way at once in one
  cycle: per way a tag bank and a data bank are native devices driven
  directly (`sram` = the clocked macro, `ff` = the flip-flop device), the
  hit way's word is taken into the result register at the next edge, and the
  response is used one edge later (a two-cycle hit). A read miss writes each
  fill word into the victim's data bank as it arrives and takes the requested
  word as it passes; writes stay write-through, no-allocate. The serial
  endpoint-store phase machine is replaced (`CacheLookupPhases`,
  `CacheBankController`); bank names `TAG_STORE_k`/`DATA_STORE_k` are kept.
- **Why:** the old controller made every phase a whole endpoint transaction
  (a 14-cycle L1 hit); the timing plan's phase 7 rebuilds the hit path.
- **Verified:** cache component, direct-mapped, scalable and two-level
  scenarios, CacheReadTest and RV32IMemoryMigrationTest at `unit`; a latency
  probe at `embedded` (L1 hit 2 cycles).
- **Limits:** line fills still move word by word (L1 miss 38 cycles against
  an 8-cycle DRAM read); burst reads follow (phase 7b) before the cache
  experiment. Public cache pages and examples are not yet updated (phase 8).

### Endpoint handshake from registered state, one DRAM edge per command, core sweep (branch `realistic-timing`, phase 6.4–6.5)

- **Changed:** every memory endpoint (gate-level transaction port and FF
  controller, compact controller, fused endpoint) takes a new request from
  registered state only: REQ_READY = !RST & (!PENDING | COMPLETION) &
  BACK_READY & !FULL, no longer following the same cycle's RSP_READY. A held
  response and one request in flight can coexist. The clocked SRAM macro
  has no logic before its registers: subarrays get CE_N and WE_N ungated and
  their address with the select bits inverted, and the bus enables are
  decoded after the edge (setup at 512 words 930 → 408 ps). The DRAM device
  shows a completion as soon as its sampler sees it and takes the next
  command at that edge (one synchronizer edge per command instead of two).
  The analysis's handshake models (built-in FF endpoint, declared FF
  contract, safe-clock state enumeration with DRAM refresh phases) follow.
- **Why:** the old handshake let the core's stall logic reach the memory's
  request inputs in one cycle (clocked SRAM certified at 5,236 ps against
  4,104 now); DRAM was 38–52 % over a real controller's latency.
- **Verified:** tables at a fixed point and verified in all profiles; delay
  rule (default) 0 breaks on 53 memory parts in all profiles; CTest subset
  (endpoint safe clock, experiment and CLI, examples, arbiter, migration,
  memory state and feedback timing) passes; gate-level handshake-state checks
  pass for FF, SRAM and DRAM; 66-run panel; core sweep of 306 verified runs.
- **Limits:** DRAM still +23–33 % over a counted controller; macro 3.1×
  CACTI; STM32F7 check not done; no full suite.

### Datasheet DRAM and NOR timing and a clocked on-chip SRAM macro (branch `realistic-timing`, phase 6.3)

- **Changed (6.3a):** the timing profiles hold datasheet intervals for
  external DRAM (`embedded` SDR SDRAM -6A, `desktop` DDR4-3200: tRCD, CL, tWR,
  tRP, tRFC, tREFI, page size) and NOR flash (tACC 120 ns, program 150 µs,
  erase 410 ms). DRAM REFRESH can include the full restore; DRAM rows up to
  1,024 words are allowed for compact devices (gate-level stays at 16); the
  RV32I experiment takes DRAM row size and refresh rate from the profile.
  Functional flash endpoint scenarios set short program and erase pulses.
- **Changed (6.3b):** SRAM devices, controllers and endpoints accept
  `clocked=1`: a synchronous macro with input registers, an access token in
  place of the address-transition detector, and the same self-timed stages.
  Large arrays drive a shared return bus (`MemoryBus` leaf, READY pulled
  low) instead of a multiplexer tree; in a clocked array every subarray takes
  each access and only WE_N is gated by the select, and the clock is ideal
  (the global wire counts in the setup). The sense replica now covers the
  wordline path and the column multiplexer, so READY follows the data (also
  for the asynchronous SRAM); in clocked mode precharge covers a fully swung
  bitline. `--memory sram` is now the clocked macro and `--memory sram-async`
  the external chip. Delay tables gain `@clocked` entries (649 per profile).
- **Fixed on the way:** the timing graph marked a one-bit write mask as a
  clock carrier ("clock used as data"); `MemoryOutputBuffer` rejected one-bit
  status lines; `DelayRuleTest` stepped one tick at a time (it now steps from
  event to event, about 100× faster with picosecond ticks) and skipped the
  DRAM/NOR endpoints as restless (it now follows a part until it goes quiet).
- **Why:** owner rule that memories be realistic, not cache-favouring; the
  clocked macro is the bank the cache phase will use.
- **Verified:** tables at a fixed point and `--verify` identical in all three
  profiles; delay rule in the CTest configuration on DRAM/NOR/SRAM/ROM plus the
  clocked variants, 0 breaks in all three profiles; SRAM, endpoint, controller,
  native-device and cache tests at `unit` and the SRAM/endpoint set at
  `embedded`; a probe of 400 random accesses per size shows READY after the
  data in every read; 66 oracle-verified CPU runs.
- **Limits:** the clocked macro is 3.1× CACTI at 512 B (reported, not tuned);
  its integration makes the core–endpoint handshake longer (certified clock
  `embedded` 5,236 ps against 4,608 ps), to be decoupled in 6.4; a 30-round
  delay-rule sample reaches a known gap in the delay twin's X timing; no full
  suite; CacheReadTest and the other tick-driven tests stay `unit`-only.

## 2026-10-16

### Memory controller protocols closer to real controllers (branch `realistic-timing`, phase 6.1–6.2)

- **Changed:** DRAM and NOR devices take the next command during the
  releasing cycle (BUSY shown low there), with FRESH and REPEAT state so a
  stale sample or a repeated command cannot complete early; the command
  sequencer shows the next command combinationally when the device is idle
  and fault-free. SRAM/ROM controllers count `access_cycles` (new endpoint
  parameter, chosen from the proven access time at the clock by
  `staticAccessCycles`) instead of synchronizing READY; READY is checked at
  the capturing edge (status Device if not high).
- **Why:** owner rule that memories be realistic, not cache-favouring: the
  removed cycles (about three per DRAM/NOR command, two per SRAM/ROM
  access) are not spent by real controllers.
- **Verified:** DRAM/NOR/SRAM/ROM device, controller, endpoint and cache
  scenarios (with a new back-to-back command case); tables at a fixed point
  in all profiles; delay rule under `unit` and `embedded`; 54
  oracle-verified CPU runs at re-certified clocks.
- **Limits:** realistic memory intervals still placeholders (6.3); no full
  suite.

### On-chip SRAM geometry and READY after data (branch `realistic-timing`, phase 5)

- **Changed:** SRAM and ROM devices raise READY only after the read data are
  valid (an extra timer stage copying the sense and output path). A gate-level
  SRAM above 32 rows is built from 32-row subarrays with a global decoder,
  global wires (new `MemoryWire` leaf, `sram_wire_per_side` in the profile)
  and a return multiplexer and output buffer; the contract is unchanged. SRAM
  tables are proven up to 512 words; larger SRAMs use `sramSubarrayEstimate`
  (per-level growth fitted at 2 and 4 subarrays, wire term from the profile).
  The compact SRAM withdraws its data no sooner than the gates' path for that
  transition. New tests: `SRAMSubarrayTest`, `SRAMSubarrayFormulaTest`.
- **Why:** realistic on-chip arrays are divided into short-bitline subarrays;
  cache banks driven directly need READY after data; the delay rule under the
  realistic profile found the early withdrawal.
- **Verified:** held-out 8-subarray tables matched exactly in all three
  profiles; SRAM/ROM/endpoint/cache scenarios under `unit` and `embedded`;
  delay rule for SRAM/ROM parts under all profiles; tables verified.
- **Limits:** `SRAMTimingTest` remains a `unit` test (tick-driven); realistic
  memory intervals are still placeholders; SRAM CPU cycles rose with
  READY-after-data; no full suite.

### Memory timing under realistic profiles (branch `realistic-timing`, phase 4.4)

- **Changed:** the SRAM/ROM timer's settle line is sized from its own
  change-detection path (`memoryTimerSettle`); DRAM sense and precharge steps
  wait for a row-decoder replica (`dramDecodeReplica`) in the gates and the
  cycle-exact copy; compact and fused DRAM models keep storage as the row's
  logical contents while a row is open; DRAM retention defaults are timing
  profile values (64 ms in the realistic profiles, `unit` unchanged) with the
  limit widened to 10^12 ticks; a compact part whose gate list contradicts a
  change is timed by its table; delay tables regenerated for all profiles.
- **Why:** the realistic profiles exposed races hidden by one-tick gates
  (lost change pulse, sensing before the wordline, precharge before the
  wordline close) and a 100 ns retention left over from tick-era defaults.
- **Verified:** tables at a fixed point and `--verify` identical for all three
  profiles; 18 memory scenarios each under `embedded` and `desktop` and the
  affected `unit` scenarios pass; full `DelayRuleTest` passes; 54
  oracle-verified CPU runs (five memories, three profiles).
- **Limits:** realistic memory intervals are placeholders until phase 6;
  tick-driven timing tests remain `unit` tests; no full suite.

### Gate-level memory control and self-timed memory devices (branch `realistic-timing`, phase 4.1–4.3)

- **Changed:** the memory controllers' sequencing is now gates. SRAM and ROM
  use `StaticMemorySequencer` (request check, registers, READY through a
  two-stage synchronizer); DRAM and NOR flash use `CommandMemorySequencer`
  (pending command list, one-cycle ISSUE, wait for the device's registered
  BUSY, open-row tracking, a cycle-counted refresh). The C++
  `NativeSequencer` primitive is removed. The devices time themselves: SRAM
  and ROM raise READY from replica timing (`MemoryTimer`, delay lines); DRAM
  and NOR command controls are synchronous gates with analog delay-line
  chains and a sampler. New timing leaves: `MemoryDelayLine`,
  `SymbolicDiffer`, `Synchronizer`, `ProtocolStableData`, `KnownBits`,
  `PowerOnReset`. Compact controllers, compact devices and the fused endpoint
  are cycle-exact copies (`NativeSequence`, `DRAMSyncControl`,
  `NORSyncControl`) timed by tables proven on the gates. DRAM refresh is
  configured in cycles (`refresh_interval_cycles`, default 96; 0 or at least
  24). The device's open row is visible as ROW_OPEN.
- **Why:** decision 2A of the realistic-timing plan (devices time themselves
  and signal completion) and the lower-level-first rule; tick timers in C++
  could not follow realistic gate delays.
- **Verified (unit profile):** focused memory scenarios (device, contract,
  timing, controller, all endpoints, scalable endpoint, cache scenarios) pass
  with every fidelity compared; `DelayRuleTest` on SRAM/ROM/DRAM/NOR devices,
  controllers and endpoints: no rule break; oracle-verified CPU runs, e.g.
  program 21 at 696 (SRAM), 1,375 (DRAM) and 732 (NOR) cycles; safe clock
  102 (SRAM/ROM), 106 (DRAM), 104 (NOR) ticks. Details in the progress record.
- **Limits:** cycle counts with timed memories grew (synchronizer and
  clocked command protocol); published results remain reproducible at tag
  `timing-legacy`. Realistic-profile tables for the memory parts are stale
  until step 4.4. `MemoryEndpointIntegrationTest` fails its two C++ example
  checks because they compile reference models from `site-docs/` snippets
  that still describe the old sequencing; the public site is updated in
  phase 8 by owner request. No full suite.

### Timing profiles, inertial gate delay and realistic gate delays (branch `realistic-timing`)

- **Changed:** leaf parts (logic gates, splitter, memory primitives) read
  their delays from a process-wide timing profile
  (`include/components/TimingProfile.hpp`), selected by
  `CIRCUITSIM_TIMING_PROFILE`, the Python `set_timing_profile` or code.
  Profiles: `unit` (default; unchanged timing), `embedded` (65 nm, FO4 30 ps)
  and `desktop` (32 nm, FO4 21 ps) with 1 tick = 1 ps and sourced gate ratios.
  Logic gates use inertial delay (pulses shorter than the delay vanish).
  Flip-flop and SRAM-cell contracts are measured per profile and stored in
  it. Delay tables are generated per profile, each in its own source file;
  `part_delay_tables --profile` and two CTest verify entries were added.
- **Why:** the owner decided to give leaf parts realistic delays while keeping
  behavioral parts derived from their gate forms (see the realistic-timing
  plan and progress records).
- **Verified:** under `unit`, `part_delay_tables --verify` identical (601
  entries) and nine CPU runs identical in every field but host time against
  the baseline binary, with and without inertial delay; 23 scenarios passed.
  Under `embedded`/`desktop`: tables verified at a fixed point, flip-flop and
  SRAM-cell tests passed, all three cores certified and ran programs with
  identical cycles and oracle checks in compact and gate-level form;
  `DelayRuleTest` sample passed.
- **Limits:** no full suite. Timed memories (SRAM/DRAM/ROM/NOR) keep
  tick-based timers and are not valid under realistic profiles until the
  gate-level memory control phases. Scenario tests stay on `unit`. The public
  site is unchanged by owner request.

### Retire the redundant memory container and document test workflows

- **Changed:** scalable endpoints now select each technology's behavioral
  native device under the existing device contract while keeping the
  controller visible. Retired container contract IDs map to native devices.
  Updated memory pages, diagrams, visualizer aliases and examples. Added a
  test and scenario workflow guide linked from AGENTS.md.
- **Why:** the container only reselected and relabeled an existing behavioral
  device; removing it leaves one replaceable native-device contract per
  technology. The guide makes registry, CTest, scenario and site checks easier
  to repeat when components change.
- **Checked:** serial native build, 28 focused CTests, 601 delay-table entries
  verified, diagram and JSON checks, full local Pages/WASM package with strict
  MkDocs, and 7/7 focused browser/WASM parity checks passed.
- **Limits:** the full CTest suite and published URL were not checked. These
  memory and guide changes remain uncommitted in the local working tree.

### Trigger Pages deployment from alpha

- **Changed:** the Pages workflow push trigger now listens to `alpha`; the
  workflow-only change was pushed to `origin/alpha`.
- **Why:** `alpha` is the selected source branch, and no `github-pages` source
  branch exists.
- **Checked:** GitHub Actions reported a run in progress for the pushed
  trigger commit. The full local site package and seven focused web checks
  passed after the push.
- **Limits:** the user directed us to treat publication as done while the
  remote run continued; its final result and live URL were not verified. The
  local memory and guide changes were not in that pushed commit.

### Fix one technology stack for the cache experiments

- **Changed:** replaced the broad FF/SRAM/DRAM performance sweeps in the open
  cache record with a complete plan for SRAM L1 and L2 caches over DRAM main
  memory. Fixed the initial geometry, small structural/scalable check, three
  standalone paths, five CPU paths, workloads, counters and completion gates;
  updated status.
- **Why:** one consistent technology stack tests the intended cache hierarchy
  without multiplying the campaign by technologies. DRAM is backing memory;
  the cache's tag/data banks currently support SRAM or FF, not DRAM.
- **Checked:** reviewed cache and endpoint source, CPU root and experiment
  runner, benchmark tooling and current documentation; `git diff --check`
  passed. No benchmark or simulation was run for this planning change.
- **Limits:** SRAM cache-hit timing, shared CPU bypass/L2 wiring, benchmarks
  and physical timing calibration remain open. The plan does not claim speedup.

### Focus the planned CPU cache campaign

- **Changed:** reduced the primary CPU matrix from nine variants to five:
  separate memories, shared uncached memory, both L1s, L2 alone, and both
  L1s over L2. Kept I-only/D-only runs as targeted follow-ups; updated the
  open cache record and status.
- **Why:** the five cases answer the main sharing, L1 and L2 questions while
  avoiding duplicate `C-0`/shared-uncached runs and a broad ablation sweep.
- **Checked:** reviewed the existing topology plan and cache contract;
  `git diff --check` passed. No simulation was run for this planning change.
- **Limits:** the CPU bypass, shared L2 and benchmark campaign remain to be
  implemented.

## 2026-10-15

### Refine the advanced-core cache comparison matrix

- **Changed:** specified nine CPU topologies in the open cache record:
  separate memories, shared uncached memory, I/D L1 combinations, and the
  same combinations over a shared L2. Added comparisons, workload choices,
  counters, correctness gates and implementation order; updated status.
- **Why:** one-level and two-level results need shared-memory controls, and
  instruction/data cache effects need independent switches.
- **Checked:** reviewed current CPU root/runner wiring, cache and arbiter
  contracts, benchmark workload catalogue and memory-wait reference;
  `git diff --check` passed after the documentation edit.
- **Limits:** plan only. The shared uncached bypass, CPU-side L2 path and
  nine-topology campaign have not been implemented or run.

### Specify matched cache experiments

- **Changed:** added an executable experiment protocol to the open cache
  record and updated status. It defines direct-versus-cache, two-level and
  advanced-core pairs, workloads, measured boundaries and result gates.
- **Why:** cache traffic savings, modeled latency, CPU cycles and simulator
  host cost need separate evidence under matched backing-memory conditions.
- **Checked:** inspected the cache and memory contracts, current scenario and
  benchmark runners, CPU topology construction and timing documentation;
  `git diff --check` passed. No performance experiment was run.
- **Limits:** this is a plan. No benchmark fixture, timing model or CPU path
  was changed, and no direct-versus-cached performance run was made.

### Recognize existing memory delay comparisons in the cache plan

- **Changed:** corrected the open cache record and status to account for
  `DelayRuleTest` and its gate-versus-compact memory timing comparisons.
- **Why:** the prior timing review overlooked these tests and made a new
  comparison phase sound necessary before exploratory cache experiments.
- **Checked:** a focused run of five 8-bit endpoint contracts and an FF
  variant completed 3,208 situations, with 970 paired output changes and no
  delay-rule break or unrecorded closeness failure.
- **Limits:** the self-timed DRAM endpoint yielded no paired output changes
  in this focused run; exact transaction latency under every request and
  physical hardware timing remain unverified. No cache speedup was measured.

### Review memory realism before cache performance comparisons

- **Changed:** added a current-source audit and staged comparison design to
  the open cache record; clarified the outstanding timing work in status.
- **Why:** structural/behavioral agreement establishes digital model fidelity,
  while phase choices, controller waits and large-depth estimates still need
  separate physical plausibility checks before interpreting cache speedups.
- **Checked:** eight focused memory contract and timing CTests passed in the
  existing `build-cleanup` binary; inspected cache, native-device, endpoint
  and delay-table source plus the repository textbook and vendor references.
- **Limits:** no code or timing model was changed, the test binary was not
  rebuilt for this review, and no direct-versus-cached latency comparison was
  run. Vendor specifications do not calibrate simulator ticks by themselves.

### Explain cache parameters through four runnable configurations

- **Changed:** rewrote the cache page's configuration and behavior text within
  its component template, preserving the C++ reference. Added direct-mapped
  and scalable single-cache scenarios plus a connected two-level FF-backed
  scenario, with menu aliases, saved layouts and browser checks. Added an
  opt-in precomputed-playback hook so the two-level layout can be built
  without simulating its transactions, and documented that contract in the
  test guide and reference.
- **Why:** the original page offered one visual configuration and did not
  clearly explain word width, sets, ways, line size or backing range.
- **Checked:** eight focused native CTests passed; the strict Pages/WASM build
  generated 30, 116 and 58 verified rows for the single-cache examples; the
  focused memory browser test passed 1/1, including all four cache scenarios.
  `CacheDocumentationTest` still compiles the unchanged C++ reference.
- **Limits:** the first three examples script their lower port. The fourth
  uses one requester and broadcasts invalidation only while both caches are
  idle. The full native and browser suites were not rerun; these scenarios do
  not establish cache speedup or coherence.

### Broaden the cache scenario and verify a two-level cache path

- **Changed:** expanded the visual component scenario to two sets, two ways
  and two-word lines, covering spatial hits, LRU replacement, writes,
  invalidation and range rejection. Added a headless upper-cache/lower-cache
  fixture over FF RAM, including idle-time invalidation of both levels;
  updated the saved layout and public page.
- **Why:** the original one-line read-only scenario could not demonstrate the
  cache geometry or whether cache components compose into levels.
- **Checked:** native scenario and broad cache test passed; focused CTests
  passed 3/3; the Pages build generated 116 WASM-verified cache rows and
  passed strict MkDocs; the memory endpoint browser suite passed against
  port 8893, including the two-way cache layout.
- **Limits:** the stack is a functional single-requester path. CPU I/D
  arbitration, MMIO bypass, coordinated invalidation, coherence and clock
  measurements remain open. The first browser attempt used a venv without
  Playwright; the installed system Python completed the test.

### Replace the cache construction snippet with a behavioral reference

- **Changed:** the public cache page now gives a compilable C++ reference for
  lookup, full-line refill, two-way replacement, masked write-through and
  invalidation. Added a documentation CTest that extracts the exact block.
- **Why:** neighboring component pages use C++ examples to explain behavior;
  merely constructing a cache did not explain its algorithm.
- **Checked:** the new CTest passed 1/1; the strict MkDocs build passed with
  the current generated cache rows; `git diff --check` passed.
- **Limits:** the reference covers successful settled binary transactions,
  not clocked handshakes, four-state behavior or failure statuses. It is not
  a registered behavioral cache implementation.

### Make the cache scenario runnable in the local WebAssembly visualizer

- **Changed:** activated the cached Emscripten SDK, rebuilt the Pages
  simulator, and regenerated the cache example from its WASM timeline.
  Extended the memory browser smoke check to include the cache page and a
  cache-specific layout assertion. Clarified the separate tag/data stores
  in the public reference.
- **Why:** the new cache scenario existed in native tests but the previous
  local browser package could not load it.
- **Checked:** serial WASM build, layout packaging, 12 verified cache rows,
  strict Pages build, and focused `MemoryEndpointBrowserTest` passed. Port
  8893 serves the new scenario JSON and the byte-identical rebuilt WASM
  binary; its cache page returned HTTP 200. `git diff --check` passed.
- **Limits:** the whole browser smoke suite and GitHub Pages deployment were
  not run; there is still no fused behavioral cache or RV32I integration.

### Restore the local Pages preview after adding the cache example

- **Changed:** placed the native-verified 12-row cache export into the
  ignored local example bundle used by the port 8893 MkDocs service. Recorded
  the outage and clarified the cache page's test and fidelity scope.
- **Why:** the new page requested `cache-read-hit`, but the preview's older
  generated bundle lacked that scenario; MkDocs aborted its rebuild and
  left the service unresponsive.
- **Checked:** the preview's strict MkDocs build passed; its home, cache and
  simulator pages returned HTTP 200. Both cache tests passed 2/2, including
  the headless broad test and the recorded component scenario.
- **Limits:** at that recovery step, the simulator package still predated
  the cache scenario. The subsequent Pages rebuild above replaced it.

### Add endpoint-backed structural and scalable-storage caches

- **Changed:** added `Cache<W>` for 8/16/32-bit words with structural tag
  lookup, one/two-way replacement, one/two/four-word fills, all-line
  invalidation, and optional masked write-through/no-write-allocate. Tag and
  data banks independently use existing FF or SRAM endpoints. A second
  catalog form selects scalable endpoint storage while retaining the same
  public pins and structural controller. Added a registered cold-miss/hit
  component scenario, visualizer alias and saved layout. Rewrote the public
  cache reference in the component guide's format, with a generated example.
- **Why:** build a reusable cache from the existing transaction endpoint
  contract and memory technologies before wiring one into RV32I.
- **Checked:** serial `SimulationTestRunner` build; six focused cache,
  catalog and profile CTests passed. Cache checks include a deterministic
  independent set/way reference stream and partial-fill fault. The new
  component scenario passed with 12 checkpoints. A final focused CTest run
  passed 4/4, including visualizer catalog (8/8) against the rebuilt native
  backend. Native-generated example rows passed a strict MkDocs build and
  appeared in the rendered page. The C++ page example compiled; `git diff
  --check` passed.
- **Limits:** geometry is explicitly bounded, the standalone fixture does
  not cover every four-state/fault timing path, and cache timing and
  advanced-core integration have not been measured. Shared RAM and
  arbitration remain open in the cache record. The initial Pages attempt
  lacked an active `emcmake`; the subsequent rebuild above verified the
  local browser scenario. The published site has not been checked.

## 2026-10-14

### Define cache geometry as construction parameters

- **Changed:** refined the proposed `Cache<W>` interface: W selects the
  supported endpoint word-width specialization; textbook sets, ways and
  block bytes become fixed construction parameters; address width remains
  32 bits under the current endpoint contract. Derive words per line from
  block bytes and W instead of exposing both values independently.
- **Why:** the parameters must shape a structural circuit, while the existing
  component catalog needs to select geometry without a C++ type for every
  combination.
- **Checked:** current memory width templates, construction configuration,
  endpoint catalog parameters and the local textbook's cache organization;
  edited Markdown links and whitespace.
- **Limits:** design clarification only; no cache code, build or test ran.

### Generalize the proposed cache across endpoint word widths

- **Changed:** corrected the cache plan from a 32-bit-only component to one
  width-parameterized structural blueprint with 8, 16 and 32-bit contracts.
  Distinguished 32-bit byte addresses from data-word width, kept 32-bit tag
  endpoints, and added width and geometry proof gates.
- **Why:** the existing memory endpoints already generalize word width; a
  reusable cache should match all supported endpoint widths.
- **Checked:** endpoint catalog registrations, parameter validation, REQ/RSP
  pin schema, transaction frontend and storage widths; edited Markdown links
  and whitespace.
- **Limits:** design correction only; no cache code or width-parity test ran.

### Specify cache components and endpoint wiring

- **Changed:** added an ordered inventory for the proposed `Cache32`
  assembly, structural tag matcher, line state, controller, reused tag/data
  endpoints and standalone test root. Clarified exact REQ/RSP port
  compatibility and the distinct cache-control responsibility.
- **Why:** memory endpoints supply replaceable storage, while a cache must
  still map addresses, track valid tags and coordinate refill transactions.
- **Checked:** current endpoint pin schema and assembly, transaction frontend,
  gate/splitter/register blocks and an existing structural tag comparator;
  edited Markdown links and whitespace.
- **Limits:** planning only; no code, compile or simulation was run.

### Require geometry evidence for the cache plan

- **Changed:** clarified how the textbook's set, way, block-size and address
  parameters determine the structural circuit. Added one- and two-set checks
  to the first read-cache gate, followed by separate block-size and way checks.
- **Why:** a single four-set example cannot demonstrate a general cache.
- **Checked:** current cache record and local textbook chapter 6; edited
  Markdown links and whitespace.
- **Limits:** this is a planning correction; no cache implementation or
  geometry test was run.

### Scope the first structural-cache deliverable

- **Changed:** specified a standalone four-set, one-way, one-word read-cache
  slice, its new structural logic, reused endpoint children, a concrete
  hit/conflict trace and an internal endpoint replacement check.
- **Why:** the first implementation step now has an observable finish line
  while keeping memory technologies selectable.
- **Checked:** current cache record, memory protocol, endpoint classes,
  transaction frontend, component catalog and structural wiring; edited
  Markdown links and whitespace.
- **Limits:** proposed scope only; no cache code or simulator test was run.

### Detail the standalone structural-cache plan

- **Changed:** expanded the open cache record with a textbook-grounded cache
  organization, one proposed public cache contract, independently replaceable
  tag/data endpoint banks, a visible lookup/refill/write sequence and staged
  verification gates. Updated living status.
- **Why:** the first structural cache needs a concrete general interface and
  replaceability tests before implementation.
- **Checked:** local textbook chapter 6, current memory protocol and endpoint
  assembly, structural wiring, component profiles and endpoint scenarios;
  checked edited Markdown links and whitespace.
- **Limits:** this is design work. No cache code, simulation, benchmark or
  public component page changed; proposed policies and timing are unverified.

### Add seven-target RV32I result figures to §4.7

- **Changed:** added a measured FF workload-group overview, controlled
  dependency/load-scheduling contrasts, a longer-branch matrix and a separate
  conditional full Embench projection. Each core comparison displays
  single-cycle, baseline and all five advanced prediction modes. Added a
  reproducible chart generator and instructions.
- **Why:** the replacement figures make the three-core comparison visible
  while keeping narrow experiments and historical projections in their
  proper scope.
- **Checked:** validated all 602 public result-table cells against completed
  inputs and the 133 projected Embench cycles against the original historical
  cases; inspected PNG previews and SVG metadata; confirmed four built SVGs
  match the source assets; passed two focused documentation CTests, strict
  MkDocs build and `git diff --check`.
- **Limits:** no benchmark rerun or simulator change. Full Embench remains a
  clock-only projection from older cycles; no full native suite, WebAssembly
  package or browser check was run.

### Rewrite the component creation guide for the current workflow

- **Changed:** replaced the deprecated checklist with the current path for
  structural construction, catalog registration, scenario and equivalence
  tests, generated behavioral delays, frontend registration and the public
  component page template. Corrected linked timing commands to preserve
  clocked tables and use complete verification.
- **Why:** the older instructions omitted the proven-delay step and treated
  optional direct Python bindings as mandatory.
- **Checked:** traced the instructions through source and build scripts;
  37 local links and 12 source example paths resolved; `git diff --check`
  passed.
- **Limits:** no component code or public page changed, so C++ tests, delay
  generation, the site build and browser checks were not run.

### Withdraw §4.7 charts and replan the visual comparisons

- **Changed:** removed all four §4.7 SVG charts, their page embeds and the
  chart-only generator. Recorded a new figure plan grounded in the current
  §4.2 test groups and §4.7 result tables; each proposed core comparison
  shows all seven comparable FF targets.
- **Why:** the previous core and Embench plots hid advanced/two-bit as an
  implicit denominator and displayed only two core series; other plots had
  narrower scopes than the report's central three-core question.
- **Checked:** reviewed the current experiment setup, report and source
  results; confirmed the rendered page has no chart embeds or remaining
  §4.7 assets; passed `RV32IBenchmarkDocumentationTest` (1/1), strict MkDocs
  build and `git diff --check`.
- **Limits:** no replacement chart or benchmark run was made. The full
  Embench figures remain conditional projections from older cycles.

### Correct cache-storage replaceability at the endpoint boundary

- **Changed:** corrected the open cache record and status: tag/data banks can
  be built from existing transaction endpoints and later swapped for scalable
  endpoint forms without a native FF/SRAM pin adapter. Native array wiring is
  now an optional optimization.
- **Why:** the existing endpoint interface was designed to make memory
  technologies interchangeable for requesters, including a cache controller.
- **Checked:** current `protocolPins`, endpoint catalog and structural
  controller/device assembly; edited Markdown links and whitespace.
- **Limits:** replaceability preserves pins and handshake, while operation
  support and latency vary by technology. No cache was built or benchmarked.

### Rewrite §4.7 around the §4.2 workload groups

- **Changed:** reorganized the complete results report into fixed programs,
  generated families, controlled core-gap cases, compiled fixtures and a
  separately marked full Embench projection. Placed each result table beside
  its interpretation, split core-gap settings by mechanism, and made the
  seven-target tables show modeled ticks with a clear clock conversion.
- **Why:** the earlier analysis and tables were far apart, making it hard to
  follow how the three cores and five advanced branch policies performed on
  each test group.
- **Checked:** validated all 99 displayed result rows against the previous
  report and complete summaries; recomputed winner counts and ratios;
  passed two focused documentation CTests, the strict MkDocs build, chart
  link/anchor checks and `git diff --check`.
- **Limits:** no benchmark or code change. Full Embench is still a
  conditional clock-only projection; no full native suite, WebAssembly
  package or browser check was run.

### Assess reuse of existing memory components inside caches

- **Changed:** added a cache-record feasibility review of reusable FF/SRAM
  arrays, scalable native storage, and technology-selectable backing endpoints.
  Clarified the proposed structural-controller-plus-scalable-array path.
- **Why:** the owner asked whether cache construction can follow the endpoint
  composition pattern without rebuilding memory devices.
- **Checked:** current endpoint assembly and catalog, native FF/SRAM pins,
  memory protocol, structural size limits, and primary Ibex/AMD cache
  descriptions; edited Markdown links and whitespace.
- **Limits:** no cache was assembled or timed. FF/SRAM cache-array adaptation,
  policy compatibility and hit-path speed remain implementation questions.

### Reorder the cache plan around a standalone structural component

- **Changed:** revised the open cache and future-steps records and living
  status to adopt structural cache, measured/timed unified scalable RAM, then
  equivalent scalable caches. Added component contracts, latency measurement,
  shared-memory integration and differential-test gates.
- **Why:** the owner chose this sequence and requires structural/scalable
  compatibility like the existing memory endpoints.
- **Checked:** inspected current memory and RV32I wiring, endpoint tests,
  timing-table extrapolation, benchmark scope, and the public memory and
  advanced-core contracts; checked the RISC-V instruction-fetch rule and a
  modern split-cache hierarchy against primary documentation; checked edited
  Markdown links and whitespace.
- **Limits:** this is a plan. No cache, shared-RAM root, timing calibration,
  benchmark or simulator behavior changed; the selected cache policy and
  timing model remain proposals.

### Expand §4.7 result charts and keep run provenance internal

- **Changed:** added predictor-pattern, timed-memory and conditional Embench
  projection charts to the existing core comparison; removed source/binary
  hashes and direct experiment artifact paths from public §4.7. The internal
  [results presentation record](records/2026-10-14-rv32i-results-presentation.md)
  retains their provenance and chart-generation method.
- **Why:** the single core chart did not visualize policy sensitivity, memory
  wait costs or the dense projected Embench comparison. Run-file details
  belong with internal evidence rather than in the public results narrative.
- **Checked:** regenerated four SVGs with validated source rows, inspected
  chart previews and XML, passed the focused documentation CTest and strict
  MkDocs build, confirmed chart links and built SVG copies, and passed
  `git diff --check`.
- **Limits:** no benchmark rerun or simulator change; Embench remains a
  conditional clock-only projection. No full native suite, WebAssembly
  package or browser check was run.

### Focus §4.7 on current results and performance comparison

- **Changed:** moved the former public clock-development analysis to the
  internal archive, removed it from the MkDocs navigation and §4.2 links,
  and added a dedicated §4.7 analysis of matched core, predictor and memory
  results with a reproducible SVG chart.
- **Why:** §4.7 should compare the current completed experiments without
  presenting retired timing work as part of the public results chapter.
- **Checked:** recomputed chart ratios from complete summaries with matching
  source/binary fingerprints, visually inspected the plot, and confirmed the
  archived page is absent from the built site. The focused documentation
  CTest, strict MkDocs build and `git diff --check` passed.
- **Limits:** the full Embench table remains an explicitly conditional
  clock-only projection; no new performance runs, full native suite,
  WebAssembly package or browser checks were performed.

### Assess the shared-RAM arbiter choice

- **Changed:** added a comparison of a scoped 2:1 arbiter, true two-port RAM
  and banked RAM to the open cache plan; clarified that the proposed arbiter
  serves the shared backing port while L1 hits remain independent.
- **Why:** the owner asked whether arbitration is a sound choice and whether
  modern memory systems use it.
- **Checked:** current core wiring, transaction contract and registered tests;
  AMD interconnect/cache documentation and Chipyard's memory hierarchy;
  edited Markdown links and whitespace.
- **Limits:** the recommendation is provisional; no arbitration wait or
  cache performance measurement was made, and no code was changed.

### Start the cache system plan

- **Changed:** recorded the owner's cache → game-program → AI-accelerator
  order, opened a cache topic record with proposed shared-RAM, structural
  cache, scalable-cache and measurement gates, and linked it from status.
- **Why:** the cache stage needs an inspectable contract and acceptance gates
  before implementation or game-program work begins.
- **Checked:** reviewed current memory/core wiring, transaction and wait
  contracts, registered tests, profile rules and the open future plan;
  checked the official RISC-V `FENCE.I` rule. Checked edited Markdown for
  whitespace and links.
- **Limits:** architecture and policies are proposals; no cache, unified RAM,
  program run, site build or timing measurement was completed.

### Correct behavioral-delay documentation

- **Changed:** replaced stale one-tick descriptions across ALU8, FF memory
  and RV32I component references with the catalog's gate-derived delay-table
  and optional delay-twin behavior. Updated the timing guide/reference, source
  contract comment and RV32I runner README to distinguish variable
  propagation from fixed certified periods (FF 166/80/100; timed 102).
- **Why:** the old prose described the constructor fallback as the normal
  behavioral timing model and still quoted retired clock certificates.
- **Checked:** source paths and generated table coverage; 15/15
  documentation CTests; strict MkDocs build with existing example rows;
  `git diff --check`. A direct advanced/SRAM clock query returned 102 ticks.
- **Limits:** no runtime behavior changed; the full native suite, WebAssembly
  package and browser checks were not rerun. Large-memory delay extensions
  are estimates, and a full Embench current-source run remains a projection
  in §4.7.

### Research source programs for the first guest games

- **Changed:** added a candidate sequence and direct source references for
  2048, Snake, Pong, Tetris, a later sprite game, Mandelbrot, and DOOM to the
  open future plan; no first game was selected.
- **Why:** available source can shorten a CPU-game port only after its host
  input, timing, display and runtime dependencies are identified.
- **Checked:** inspected upstream source/README/license pages and the current
  platform limits recorded in this repository.
- **Limits:** no source was downloaded into the repository, compiled for the
  guest, or run on the simulator; runtime and memory feasibility remain open.

### Add a pre-game ISA expansion decision to the future plan

- **Changed:** recorded RV32I, Zmmul, and RV32IM options and a provisional
  RV32IM recommendation for the compiled-game target; the ISA choice remains
  open, and memory/cache work stays first.
- **Why:** the owner wants to consider adding useful CPU operations, not just
  rely on software arithmetic.
- **Checked:** reviewed current decode/ALU and the RISC-V M specification;
  compiled small multiply/divide C functions with all three `-march` choices.
  The pinned compiler emitted helper calls for RV32I and Zmmul, and hardware
  instructions for RV32IM. Tiny ELFs linked, but the Zmmul image pulled in
  compressed instructions from the default runtime; RV32IM selected its
  dedicated multilib and emitted `mul`.
- **Limits:** no simulator run, hardware design, or game performance
  measurement was made.

### Record memory-first priority and RV32I arithmetic feasibility

- **Changed:** recorded the owner's decision to finish unified-memory/cache
  work before games, superseding the earlier game-first proposal; added a
  source-based assessment of software multiplication on RV32I.
- **Why:** the implementation order is now owner-directed, and hardware MUL
  is a performance option rather than a functional prerequisite for simple
  integer games.
- **Checked:** read the current ISA/toolchain contracts and official RISC-V
  and GCC references; inspected a built RV32I matrix-multiply ELF whose
  `__mulsi3` helper uses add/shift instructions.
- **Limits:** no new build, simulation, or game performance check was run;
  cache design and any M/Zmmul extension remain open.

### Sequence program and memory milestones

- **Changed:** proposed a stable address/MMIO contract, one playable CPU
  program on existing memories, shared RAM on the wait-capable core, then
  optional caches tested against slower backing memory.
- **Why:** the first game does not need shared code/data storage, while DMA
  integration does; today's zero-wait FF endpoint supplies no modeled cache
  miss penalty to reduce.
- **Checked:** reviewed current machine, memory-wait and scalable-memory
  contracts and the open plan; checked Chipyard's hierarchy reference.
- **Limits:** sequencing is a proposal; no new implementation, game run, or
  cache-speedup measurement was performed.

### Propose a shared-memory integration path

- **Changed:** recorded a preferred first merge: retain the two CPU request
  paths, arbitrate them onto one shared RAM/address map on the advanced core,
  then add optional caches after the uncached contract works.
- **Why:** one RAM image removes instruction/data divergence while the
  advanced core can tolerate contention from a single-ported backing.
- **Checked:** compared current core-memory wiring and wait contracts with
  Intel cache-hierarchy and RISC-V instruction-visibility references.
- **Limits:** planning only; no arbiter, shared RAM, or new test was built.

### Clarify scalable-memory access timing

- **Changed:** added a source-based timing comparison of the five scalable
  native devices and their transaction endpoints to the open future plan.
- **Why:** cache and shared-memory proposals depend on whether the existing
  backing memory models an address-dependent or variable access cost.
- **Checked:** inspected container, device, controller, sparse-storage and
  delay-table code; four focused memory contract/timing CTests passed using
  the existing `build-cleanup` binary.
- **Limits:** no fresh build or per-address latency sweep; large-depth FF
  delay tables are estimates, and host runtime was not measured.

### Report current RV32I performance panels at final periods

- **Changed:** replaced §4.7's older result tables with 497 completed
  current-source panel case/target rows, core/predictor/memory comparisons,
  and a separately labeled conditional 133-cell full Embench clock-only
  projection. Updated §4.2 links to the new result scope.
- **Why:** the earlier page mixed source snapshots and could present old ticks
  as current results; the full Embench workload was intentionally left as a
  projection rather than rerun.
- **Checked:** current tables were rendered from completed local summaries
  and focused direct-run JSON pairs; 420 broad FF cases, 35 long branches,
  18 memory pairs, seven focused program-20 memory pairs, three capacity
  cases and 14 ELF fixtures passed their recorded checks. The focused
  documentation CTest, strict MkDocs build and `git diff --check` passed.
  The non-Embench native suite had 449/450 sandbox passes and its sole
  sandbox `EPERM` case passed separately outside the sandbox.
- **Limits:** projected Embench ticks assume unchanged historical cycles;
  no current-source full or reduced Embench suite was run. The site build
  reused generated examples; WebAssembly packaging and browser checks were
  not run.

### Evaluate unified memory and cache integration

- **Changed:** added a source-based comparison of the current dual-memory
  testbench, unified backing-memory options, and optional small/scalable
  cache stages to the open future-steps plan.
- **Why:** device I/O and future DMA need one address-map decision, while cache
  misses and coherence would change current core and timing contracts.
- **Checked:** traced memory wiring, endpoint storage, ELF loading, oracle
  modes, core timing limits and public contracts; checked primary RISC-V and
  Chipyard references. No build or test was run for this planning increment.
- **Limits:** no shared-memory/cache prototype or measured cache benefit;
  design and integration order remain proposals.

### Assess interactive platform, GPU, and AI accelerator goals

- **Changed:** recorded current implementation gaps, external architecture
  references, and candidate milestones in the open future-steps plan.
- **Why:** the three requested goals need distinct success criteria and shared
  device/memory groundwork before implementation choices are made.
- **Checked:** read current simulator, RV32I, memory, visualizer and test
  sources, open records, and primary external references; `ctest -N` lists
  451 tests, but no build or test was run for this planning review.
- **Limits:** no game or accelerator prototype, throughput measurement, or
  approved design; feasibility and sequence are proposals.

### Compare the Embench core results

- **Changed:** recorded per-core full Embench winners, a fixed-policy
  modeled-time comparison, reduced-work structural evidence and the boundary
  between measured old periods and a conditional final-clock projection.
- **Why:** the 19-workload suite gives broader performance evidence than the
  current short branch and program-20 cases, but it ran on an older circuit.
- **Checked:** audited all 133 completed full case summaries and nine reduced
  structural summaries; recomputed periods, winners and geometric mean ratios.
- **Limits:** artifact analysis only; no current-source Embench run, new
  simulation, CTest or site build.

### Analyze performance of the three RV32I cores

- **Changed:** recorded the three-core modeled-time comparison, clock/cycle
  break-even thresholds, predictor effects and historical counterexamples.
- **Why:** a shorter certified period does not by itself determine which core
  finishes a workload first, and historical broad rankings are not current
  measurements.
- **Checked:** recalculated five current branch comparisons and current
  program-20 times from artifacts; counted winners across the 133 historical
  Embench cases and checked selected core-gap values.
- **Limits:** analysis only; no new simulation, build, CTest or site build.
  Broad current-source core-gap and compiled performance remains unmeasured.

### Rewrite RV32I experiment results around final clock periods

- **Changed:** replaced §4.7 with final current-source clock certificates,
  current 35-case branch cycles and ticks, seven verified program-20 memory
  pairs, and dated broad fixed, core-gap and compiled results. Added the
  completed prefix and logic panels to the coverage inventory.
- **Why:** the old page mixed development chronology with results and could
  be read as applying historical measurements to current 166/80/100/102-tick
  clocks.
- **Checked:** rechecked ten safe-clock certificates and 14 current program-20
  runs; matched all new and transcribed result tables against artifacts.
  `RV32IBenchmarkDocumentationTest`, strict MkDocs build and `git diff --check`
  passed; retained inbound result-page anchors.
- **Limits:** broader FF, timed-memory and compiled campaigns were not rerun.
  The site build used existing example rows; WebAssembly packaging and browser
  tests were not run.

### Run current-source long branch structural comparisons

- **Changed:** completed all 35 long branch-pattern cases with oracle,
  expected-result and structural-reference checks; recorded the new result
  in §4.7 and a separate topic record.
- **Why:** the September 24 long branch supplements had 35 behavioral checks
  but no structural comparisons, and current-source evidence was needed.
- **Checked:** five seven-case campaigns and a one-case pilot completed at
  certified 166/80/100 tick periods; artifact audit verified 35 structural
  flags, 105 phase records, signature equality and `COMPLETE` markers.
  The serial native target build found the experiment binary up to date.
  `RV32IBenchmarkDocumentationTest`, strict MkDocs build and `git diff --check`
  passed.
- **Limits:** one measurement repetition with no host warmup; the broader
  core-gap, compiled and timed-memory campaigns were not rerun. The site build
  used existing example rows; WebAssembly packaging and browser tests were
  not run.

### Assess missing RV32I structural comparisons

- **Changed:** recorded measured-cost estimates for the 35 long branch cases
  and two full CRC32 cases, with the full Embench budget limits and a
  practical structural-check scope.
- **Why:** a completed behavioral/oracle result does not indicate whether
  adding a structural comparison is affordable on this host.
- **Checked:** compared historical structural run times, cycle/event counts,
  case summaries and campaign options; checked record links and whitespace.
- **Limits:** extrapolations use an older source/host. No new simulation,
  build, CTest or site build was run.

### Audit all recorded RV32I experiment outcomes

- **Changed:** recorded complete and partial campaign inventory, 19 × seven
  full Embench cycle results, compiled/fixed/core-gap evidence and explicit
  rerun decisions; updated living status to distinguish historical snapshots
  from current performance claims.
- **Why:** the published results page summarizes only selected completed
  campaigns, and new delay and memory-edge behavior changes the basis for
  present-day performance conclusions.
- **Checked:** compared `COMPLETE` markers, 1,179 case summaries,
  structural flags, periods and provenance; machine-checked all 133
  transcribed Embench values, record link targets and whitespace.
- **Limits:** artifact/documentation audit only. No experiment, CTest, source
  build, WebAssembly or site build was run; the broad current-source reruns
  remain open.

### Separate RV32I experiment setups from results

- **Changed:** moved dated campaign outcomes and clock-certificate values
  into §4.7; kept §4.2 focused on setup, test methods, artifact contracts
  and links to those results. Labeled the older 124/66/88 periods as an
  intermediate snapshot.
- **Why:** readers need a clear boundary between how experiments are run
  and what the completed runs established.
- **Checked:** the RV32I benchmark documentation CTest and strict MkDocs
  build passed; rendered result anchors and `git diff --check` passed.
- **Limits:** this is documentation work; no broad performance campaign or
  current-checkout Embench run, WebAssembly package or browser check was
  performed. The site build used the existing generated example rows.

### Update RV32I benchmark and verification chapter

- **Changed:** completed §4.2's campaign, recovery and external-runner test
  inventory, timed-memory handoff, certificate method and handshake-state
  regression; linked its compiled workloads to the results page.
- **Why:** let readers distinguish registered test passes, structural
  comparisons and full behavioral workloads while using current timing rules.
- **Checked:** six focused CTests passed serially; the documentation CTest
  passed again after the final wording edit; ten current safe-clock reports
  matched the table; strict MkDocs build, chapter links and `git diff --check`
  passed using the existing generated example bundle.
- **Limits:** no new performance campaign, full-suite rerun, WebAssembly
  package or browser checks. The performance README remains open follow-up
  work; the §4.7 clock claim was corrected in the entry above.

### Audit RV32I chapter 4.2 against experiments and benchmark tests

- **Changed:** recorded which chapter 4.2 contracts remain current and which
  harness tests, timed-memory validation details and completed compiled
  campaign links the public pages still need.
- **Why:** answer whether the chapter reflects the available experiment
  evidence before editing the public pages.
- **Checked:** compared chapter 4.2 and §4.7 with test registration, test
  source, campaign tools, completion artifacts and the delay record; checked
  the 13 benchmark-label CTest registrations without executing them.
- **Limits:** no public page, code, experiment or site build changed or ran.

## 2026-10-13

### Record RV32I test and experiment evidence for documentation planning

- **Changed:** completed the RV32I test inventory and dated campaign evidence
  record; opened a separate documentation-update record with a proposed page
  sequence, evidence boundaries and verification steps.
- **Why:** separate current registered checks and their recorded pass result
  from historical structural, reduced compiled and full behavioral campaigns
  before revising the public results.
- **Checked:** inspected CTest registration, test source, campaign completion
  and per-case metadata; ran current safe-clock certificates. The tracked
  clean-build record reports 451/451 native tests at `448e4e3`.
- **Limits:** no new full suite, performance campaign, site build or browser
  validation for this review; no public RV32I experiment page was edited.

### Align component references with current four-state behavior

- **Changed:** corrected public Fetch, Decode, memory-requester and pipeline-
  register behavior descriptions for unknown values; named the existing
  advanced Fetch, Memory and WB-bypass parts; updated the internal
  single-cycle stage reference to name `IncrementBy4`.
- **Why:** several component pages still described older binary fallbacks or
  conflated standalone requester state with the complete core's binary model.
- **Verified:** 11 selected documentation CTests passed serially; strict
  MkDocs build passed using the existing generated example rows;
  `git diff --check` passed.
- **Limits:** documentation only; the full native suite, WebAssembly package
  and browser checks were not rerun. RV32I experiment and test-case
  inconsistencies and behavioral delay computation descriptions were left out.

### Sort maintained internal docs into guides and reference

- **Changed:** moved three procedural or explanatory pages to `docs/guides/`
  and eight contract and architecture pages to `docs/reference/`. Added folder
  indexes, updated the placement rule in `AGENTS.md`, and repaired navigation
  links and the exact-path documentation test.
- **Why:** make a document's purpose clear from its location and leave the root
  of `docs/` for the index, living status and changelog.
- **Verified:** 1,024 source Markdown links resolved; the RV32I reference
  documentation test passed through CTest (1/1); `git diff --check` passed.
- **Limits:** generated simulator directory links are checked by the site
  build, which was not rerun. The move does not resolve stale claims found by
  the ongoing documentation audit.

### Delay model explained in one document

- **Changed:** added `docs/delay-model-explained.md`, a guided explanation of
  the delay work from the problem to the results (engine, delay tables, gate
  lists, X values, the safe clock, timed memories, experiments, limits), and
  linked it from `docs/README.md`.
- **Checked:** numbers copied from `docs/circuit-timing-analyzer.md`, the open
  delay record and the source; quoted commands and test names checked against
  the tools' options and the CTest list. No code changed; nothing rerun.
- **Limits:** it repeats the published-result gaps (advanced core at 100,
  timed memories at 102, not rerun).

### Advanced core clock covers the compact core; handshake states checked

- **Changed:** the certified clock includes the compact core's setup needs at
  its memory pins for every memory (advanced core with flip-flop memory: 98
  to 100). New validation option `--check-handshake-states`.
- **Why:** 98 was proven for the gates but only observed for the compact
  form; the handshake state lists had no check against simulation.
- **Verified:** every port state in program 20 is in its list for all five
  memories; nine clock and experiment tests pass.
- **Limits:** full suite and site build not rerun; published results at
  period 98 not rerun.

### Clock certified for cores with SRAM, DRAM, ROM and NOR-flash memory

- **Changed:** the safe-clock analysis enumerates the handshake states of a
  timed memory endpoint with the core's requester and settles each case;
  `--period auto` works for those memories (102 for the advanced core) and
  includes what the compact core's table needs at its memory pins.
- **Why:** the analysis stopped at a dependency loop between "request ready"
  and "response ready", so those systems needed a hand-picked period.
- **Verified:** gates and compact form agree at the automatic period for all
  four memories with 0 setup violations; 13 timing and experiment tests pass.
- **Limits:** full suite and site build not rerun; flip-flop memory unchanged
  at 98; no binary-startup certificate for timed memories. See the
  [record](records/2026-10-12-behavioral-delays-and-memory-clock-edge.md).

### Organize existing internal documentation

- **Changed:** moved 14 superseded guides, completed reports and older plans
  from the root of `docs/` to `docs/archive/`; added an index covering all 61
  archived documents. Reworked `docs/README.md` around the current developer
  references and repaired affected links in internal notes, the root README,
  agent instructions and a public site reference.
- **Why:** make the current guidance easy to find and keep historical claims
  separate from maintained contracts and procedures.
- **Verified:** the strict MkDocs build passed; 1,010 local Markdown document
  links resolved; the archive index covered every archived Markdown file;
  `git diff --check` passed. No C++ tests were run for this file relocation.
- **Limits:** archived content retains the claims and measurements of its
  original code state. This organization does not revalidate those claims.

### Compact parts follow their gates after X/Z; memories answer at clock edges

- **Changed:** compact parts leave X where their gates do; the self-timed
  memory sequencer shows its answer only at a clock edge; the timing analysis
  marks memory endpoints as keeping to the clock; delay tables regenerated;
  experiments report `setup_violations`; `--period auto` refuses non-FF
  memories; the `RV32IBinaryClockCLITest` limit went from 180 s to 300 s.
- **Why:** gate-level and compact forms disagreed after unknown inputs, and
  SRAM and DRAM systems lost responses or hung at some clock periods.
- **Verified:** native suite 450 of 451 in one serial run, the remaining test
  passing alone in 134 s; `DelayRuleTest` 0 rule breaks over 120,081
  situations; site build; 36 of 36 web checks; gate-level equals compact in 14
  system runs.
- **Limits:** published results for sequenced memories are not rerun; no clock
  is certified for cores with those memories. See the
  [record](records/2026-10-12-behavioral-delays-and-memory-clock-edge.md).

### Establish documentation recording rules

- **Changed:** made `AGENTS.md` the single agent rule set and made `CLAUDE.md`
  point to it. Added a living status, changelog and dated topic-record template.
  Moved the historical three-year roadmap out of the agent instructions and
  updated the internal documentation index.
- **Why:** keep current operating rules separate from historical plans and
  record substantive work without requiring a separate documentation request.
- **Verified:** `git diff --check` passed; 54 relative Markdown links in the
  edited and new documents resolved; the archived roadmap matched the former
  agent-file text verbatim; the dated record header and status listing agreed.
  No C++ tests or site build were run because source and `site-docs/` were not
  changed.
- **Limits:** this change establishes the workflow and an initial status. It
  does not revalidate every existing component claim or reclassify every older
  root-level internal document.ecord meaningful completed changes here, newest first. Each entry states
what changed and why, what was verified, and any limits or deliberate
omissions. Entries begin with adoption of this recording workflow; older
history remains in Git and the archived reports.

### `unit` delay tables: second pass; multiply/divide step splits

- The `unit` regeneration after the splitter change was one pass, and parts
  that contain storage were proven on the storage tables compiled into the
  tool, which still charged the splitter tick. A second pass changes 117 of
  833 entries, every number smaller (cores, pipeline registers and stages,
  the M unit, six device-part tables); a third pass is identical. The
  advanced core's `unit` certificate falls from 82 to 80 ticks (single-cycle
  142 and baseline 68 unchanged); documentation timing data regenerated
  (only the period changes), site and reference text updated.
- `Logic::addToWord` and `Logic::subtractWord` keep words whole in
  `MultiplyStep`/`DivideStep`, removing the last three unused Rewires (same
  gates and child names). Step layouts pruned and re-laid.
- Checks: all 26 non-slow timing CTests, `DelayRuleTest`, 15 focused M-unit
  CTests (including the slow gate-level one), static layout check 0 problems
  in 373 scenarios. Published §4.8 results remain at the old periods (owner
  decision pending).

### Run-time signal width (D2): packed values, one wire event

- A pin or wire now holds a four-state value of any width from 1 to 64 bits,
  packed in two 64-bit words (`PackedLogic`). `PinBase`/`WireBase` own the
  width and value with non-virtual accessors; `Pin<W>`/`Wire<W>` are stateless
  typed handles. `WireUpdateEvent` is one class for every width, and the typed
  fanout, the width dispatch in `makeWireUpdateEvent` and
  `TemplateInstantiations.cpp` are gone. `dispatchWidth` covers 1–64; which
  widths a family offers stays its own contract (`Register` lists its eight).
  `Rewire` accepts any width 1–64. `Rewire`, `BitJoiner`, gate input caches,
  word gates and the delay-table path work on packed values.
- Why: one value representation instead of a template per width, and widths
  outside the former fixed list (owner request in the non-device plan).
- New `SignalWidthTest`; `RewireValidationTest` now rejects 65 instead of 7/64.
- Checks: broad non-slow native suite 362/365 (the three failures predate D2:
  `DeviceParametersBindingsTest` from the uncommitted device work and two
  visualizer palette/profile tests); `DelayRuleTest` (slow) passes; focused
  CTests after each stage.
  Speed at load 3–4: test-level changes within ±3% (not resolved); the Visual
  guest runs 2% faster while processing 12.5% more events than the 09:04 build,
  a comparison that also spans typed fanout, Chapter 2 word gates and the
  splitter change. Not run: WASM, strict site build. The research scripts
  under `tools/engine-performance/` still name the 2026-10-26 file list.

### Broad suite fixes after the splitter change

- `tests/fixtures/memory-array-timing.json`: each memory transaction is one
  tick sooner with the same values. `DelayRuleTest` floors for the 8/16/32-bit
  cache controllers re-measured. `Register<32>` and `RV32ISingleCycleCore`
  saved layouts regenerated with the repository generators (both entries were
  empty before this session; 0 static-layout problems in 373 scenarios).

### Zero-delay splitter in `unit`; Chapter 2 word gates

- The bus splitter no longer costs a tick in `unit` timing; it is wiring, as in
  the realistic profiles. `LogicUnit<W>`, `ParallelPrefixAdder<W>` and
  `AddSub<W>` now use word gates (the same gates, regrouped).
- Delay tables regenerated: `embedded` and `desktop` byte-identical (833
  entries), `unit` 782 entries changed. `unit` re-certified: core periods
  142/68/82 (were 166/80/94), RV32IM core 84 (was 94), M unit 48 (was 68);
  documentation timing data, timing-test expectations and site figures
  updated. Layouts migrated; 0 static-layout problems in 373 scenarios.
- Checks: 26 non-slow timing CTests and the Chapter 2 ALU/adder tests pass.
  Published §4.8 results at the old periods stay dated (owner decision).

### Unused splits removed from the multiply/divide groups

- `Logic::out` wires words straight through, and new word-level helpers
  (`invertWord`, `subtractWords`, `negateWord`) let the M-unit result
  corrections, divide operands and divide datapath keep words whole. Unused
  Rewires fall from 25 to 3 in `MulDivUnit<32>`; `MultiplyResultCorrection<32>`
  has 9 parts instead of the original 82.
- Checks: affected delay tables unchanged in all profiles; 31 focused CTests;
  layouts migrated, static layout check 0 problems.

### Typed fanout and guest throughput

- Wire fanout copies values as typed arrays into same-width pins and
  forwarding events (vector path kept for other widths). 55 focused CTests
  pass; one paired block shows 1.1–6.6% less CPU.
- The record's Visual guest (2,000 cycles) runs in 8.82 s instead of 13.74 s
  against the frozen 2026-10-26 baseline: 1.04 M → 1.56 M events/s, with 3.9%
  fewer events for the same work. One A/B/B/A block; not a game capture.

### One-byte LogicValue

- `LogicValue` uses a `uint8_t` underlying type, so stored values take a
  quarter of the memory. 55 focused CTests pass; one paired block per test
  shows 1.4–1.9% less CPU. Not run: full suite, WASM.

### Engine: queue keys, no-op wire updates, sink routes, incremental twin

- The event queue stores each event's sort keys beside it; a wire update that
  restates the current value returns before allocating; wires cache what each
  sink does; and the compact parts' clocked timing model re-evaluates only the
  gates whose inputs changed (from the 2026-10-26 prototype).
- `DelayTwinIncrementalTest` keeps the previous twin as a test-only reference
  and requires identical answers, values and arrival times on 72,000 clocked
  steps in every timing profile (fault-checked).
- Checks: 51–52 focused CTests after each step; `DelayRuleTest` passes after
  the twin and route changes. One paired A/B/B/A block per test (pinned CPU):
  `ALUWidthContractTest` −33%, `MulDivGateLevelTest` −21% (all D1–D4),
  `MulDivUnitWidth32Test` −59% and `RV32IMFiveStageCoreTest` −4.4% (twin and
  routes). Limited evidence; not a guest or game measurement. Not run: full
  suite, WASM.

### Word gates and first engine cleanups

- Add word gate families `ANDGate<W>`, `NANDGate<W>`, `ORGate<W>`, `NORGate<W>`,
  `XORGate<W>` and `NOTGate<W>` (W = 4/8/16/32, contracts `logic.<kind>.<W>`):
  W one-bit gates between zero-delay Rewires, plus a compact form on proven
  delays. The M units' subtraction/negation use `NOTGate<W>` instead of W
  inverters with identical timing; `MultiplyResultCorrection<32>` drops from
  79 to 15 children. Chapter 2 components are unchanged (owner decision).
- The visualizer draws word gates as the one-bit symbol with a `/W` badge and
  shows their one-bit gates when one is selected; palette and layout defaults
  added; RV32IM/M group layouts migrated.
- Regenerate the three delay-table files: 24 new entries each, all 809
  existing entries byte-identical.
- Engine (exact): cache `Rewire` routes and `BitJoiner` inputs, build splitter
  output names once, read diagnostic environment switches once, and drop
  temporary vectors from timed evaluation. Gate-level test CPU time falls
  1.7–21% (two runs each, not the paired protocol).
- Checks: `WordGateSweepTest` (values on both forms and per-bit timing in every
  profile; fault-checked), 53 focused CTests, 19/21 visualizer CTests (two
  pre-existing failures), static layout check 0 problems. Not run: full suite,
  WASM/browser checks, strict site build.

### Fewer bit adapters in non-device recipes

- `circuit::structural::Logic` no longer builds one-bit split/join Rewires or
  split→join round trips, bridges a parent input to its own output with one
  `PASS_<output>` Rewire, and shares one select inverter per word mux. Skipped
  adapters keep their name numbers, so other child names are unchanged. Device
  recipes select the legacy construction until the device redesign. Why: the
  crowded `MULDIV…RESULT.SPLIT_0` and similar groups were pure round trips.
- Rewires drop by about half (`MulDivUnit<32>` 126 → 61, five-stage core
  336 → 206, pipeline coordinator 48 → 7).
- Checks: all 809 unit delay tables recompute identically; the affected base
  tables also match in embedded and desktop; 37 focused RV32IM/M/decoder/device
  CTests and 19 of 21 visualizer CTests pass (the two failures predate this
  change). Four group layouts were migrated and the static layout check reports
  0 problems on 14 scenarios.
- Fix `AdderWidthSweepTest` appearing as a visualizable scenario.
- Rejected: subtraction through `AddSub<W>`, which would add a splitter tick to
  the core's critical path. Not run: full suite, WASM/browser checks, strict
  site build; game recordings were not recaptured.

### Gate-level multiply/divide and pipeline control sweeps

- Add `MulDivGateLevelTest` (label `slow`, about 44 s): exhaustive 4-bit M unit
  on both forms, boundary and random 8/16-bit pairs, and strict gate-level
  32-bit multiplier, divider and M unit, against a width-generic reference.
- Add `RV32IPipelineControlSweepTest` (under 1 s): 8,502 forwarding and 2,358
  hazard vectors against independent rules on both forms.
- Extend `MemoryDeviceContractTest` with further ROM cell/row contents.
- Deliberate faults in the divide result correction and the forwarding
  comparator were caught by the new tests and missed by the narrow-width and
  recorded tests; both were reverted. Site pages for the M units, forwarding,
  hazards and ROM describe the new checks.
- Checks: the affected focused CTests pass. Not run: full suite, WASM/browser
  and the strict MkDocs build (disk headroom).

### Adder and ALU sweep tests

- Add shared sweep equipment (`include/tests/SettledSweep.hpp`) and three
  headless sweeps against independent integer references on both forms:
  `AdderWidthSweepTest` (ripple and prefix adders at 8/16/32 bits, every carry
  span at every start position, 1,024 random vectors and every single X/Z input
  bit), a 16/32-bit boundary grid and random vectors in `ALUWidthContractTest`,
  and `RV32IALUSweepTest`. Why: `ParallelPrefixAdderWidth16Test` had 8 cases
  and the 16/32-bit ALU sub-units had no real sweep.
- Deliberate faults in the prefix adder and in `RV32IALU`'s XOR were missed by
  the existing tests and caught by the new sweeps; both faults were reverted.
- Fix catalog ownership for `word.alu.8` and the 16-bit adders, which had two
  logical owner tests: `WordDatapath.cpp` now separates the contract owner from
  the equivalence evidence. `BuiltinComponentCatalogInventoryTest` passes again.
- Checks: 15 focused CTests and `DocumentationExamplesTest` pass. Runtimes:
  adder sweep about 7 s, `RV32IALUSweepTest` about 6 s, `ALUWidthContractTest`
  21–26 s (was about 5 s). Not run: full suite, WASM/browser checks and the
  strict MkDocs build (skipped for disk headroom; only test-table rows changed).

### Non-device improvement plan

- Add the [non-device improvement plan](records/2026-10-27-non-device-improvement-plan.md):
  test sweeps, the `Logic` adapter fix, word-wide gate families with a
  gate-symbol visual override, and engine phases including run-time pin
  width, in the owner's order. Device I/O work is excluded.
- Documentation only; no build or test was run.

### Device architecture review

- Add the [device architecture record](records/2026-10-27-device-architecture-review.md):
  maps the device platform against the textbook's bus, I/O bridge, DMA and
  interrupt model (CS:APP 1.4.1, 6.1, 8.1, 10) and proposes a data-driven
  interconnect, a standard device contract, interrupts/traps and DMA in seven
  phases that keep the ten game fixtures exact.
- Documentation only; no build, test or simulation was run. The fixed
  six-region `AddressMap` was checked against source.

### Device I/O and redundant bit-adapter review

- Add the [Device I/O review record](records/2026-10-27-device-io-review.md):
  `StructuralLogic.hpp` root causes for one-bit Rewires and split/join round
  trips (including `MULDIV.MULTIPLY.RESULT.SPLIT_0`), a construction-only
  census of eight scenarios, a staged fix plan with migration impact, and a
  per-component Device I/O review.
- Documentation only. The census built circuits with the 2026-10-24 Python
  module under a 2 GiB cap and ran no simulation, so counts may lag the
  working tree slightly. No build or test was run.

### Test strength review

- Add a [test strength record](records/2026-10-27-test-strength-review.md)
  with case counts by area and a phased plan for sweep tests: adders and
  16/32-bit ALU units first, then gate-level multiply/divide, then RV32I
  forwarding/hazard units and the address router. New coverage stays in
  headless tests so published example tables are unchanged.
- Documentation only; no build or test run (only `SimulationTestRunner --list`).
  Runtime figures are estimates from cached CTest averages. The 8-case
  `ParallelPrefixAdderWidth16Test` count was checked against source.

### Engine performance review and plan

- Add a source and record review with a phased optimization plan to the
  [engine performance record](records/2026-10-26-engine-performance.md). It
  puts native cost at about 1 µs CPU per event (10–12× from the 10 million
  events/second target) and finds that measured allocation savings explain
  only a few percent. New exact-cleanup candidates include per-bit pin-name
  work in splitters/joiners/Rewire, `getenv` on the output-publish path,
  int-sized `LogicValue` and inline heap keys. Seven owner decisions are listed.
- Close the 2026-10-13 engine performance plan as superseded; its open
  decisions are carried into the new plan.
- Documentation only; no build, test or measurement was run. Cited source
  lines were spot-checked.

## 2026-10-26

### Incremental clocked timing-model experiments

- Add private owned-buffer, dirty-node and ordered-mask prototypes with shared
  CSR dependencies. Preserve both value and arrival changes, capture ordering,
  unknown/reset/falling-only behavior and full traversal for unsupported models.
  Separate the algorithm from compiler settings; production remains unchanged.
- Fifty-eight focused checks pass on 104,664-case timing signatures, feedback,
  arrival-only changes, owner/cold-state behavior and existing scenario/decoder
  checks. All 82 guest results and 41 streamed packages match golden data.
  Add alternating within-process reference/candidate register kernels and an
  unchanged control that exposes residual measurement/layout bias.
- Ordered masks skip 66.1% of modeled evaluations with about 79 KiB cumulative
  metadata. Limited stable `-O1` guest blocks show 4.9% / 4.2% lower CPU without/
  with capture; one block per mode is stable, and all matched `-O2` blocks drift
  too much. Do not present larger kernel gains as whole-engine performance.
- Retain one corrected probe-entry compile failure and every unstable result.
  Peak RSS is 262 MiB, no experiment OOM/swap. Archive completed private
  verification ELFs losslessly, freeing 2.39 GiB with hashes/restoration metadata;
  a restored reference executable passes. Keep all code, objects and evidence.
- No candidate integration, full native/game/WASM/browser acceptance or live
  deployment. See the [open record](records/2026-10-26-engine-performance.md).

### Wire/timing experiments and bracketed performance controls

- Add matched actual-Clang/PIC controls for guarded wire updates, timing scratch
  reuse and selected-file `-O2`. Derived wires retain the generic value API.
  Add independent width/four-state/timing and clocked-repeat verification.
- Preserve an exploratory timing matrix interrupted after unrelated CPU load
  caused baseline drift. Complete 32 fixed-affinity baseline/candidate/candidate/
  baseline measurements with host context and a predeclared 5% control-drift
  threshold. Stable blocks show roughly 3% lower CPU separately for wire and
  compiler changes, with limited samples; one unstable block remains recorded.
- Wire updates eliminate 32% of C++ allocation requests. Timing scratch reuse
  removes only 0.02%; stateless memo has zero eligible hits and guarded stateful
  memo only 0.3%, adding allocations. Retain these negative results and avoid
  adopting them as workload optimizations.
- Eighty focused checks and all 84 completed guests pass; 42 streamed packages
  match previous golden bytes. Peak RSS is 262 MiB, zero experiment OOM/swap.
  Record the include-path failure, superseded wire draft and intentional stop.
  Production sources/archive and live modules/captures remain unchanged.
  No full-library adoption, full-game, broad native or WASM/browser acceptance.
  See the [open performance record](records/2026-10-26-engine-performance.md).

### Primitive gate input caches and signal-buffer reuse

- Cache typed inputs in all six primitive gates while preserving public sorted
  pin maps. Invalidate immediately on pin edits so replaced inputs are released
  before obsolete wire sinks can drive them. Reuse disabled-history pin value
  buffers; preserve four-state, delay, event and recorded-history semantics.
- Rebuild the complete isolated native archive and focused consumers. Thirteen
  native checks, full-tick decoding and complete stored-delay comparison pass.
  Thirty repeated RV32IM comparisons and ten calibration runs match state,
  counters and every trace byte; eighteen inverter-fanout runs match digests.
- Combined RV32IM median CPU time falls 0.7% without capture and 1.2% with it,
  with overlapping run ranges. Individual controls are mixed; this does not
  establish a broad engine speedup. Cache size grows 24/40 bytes per gate.
  Peak process RSS is 463 MiB, zero experiment swap/OOM. Preserve failed and
  interrupted attempts and correct launcher/driver and stale-consumer controls.
- Harden experiment completion logging against externally stopped services.
  Update benchmark/test procedures and the
  [open performance record](records/2026-10-26-engine-performance.md).
  No live deployment, full-game recapture, full native suite or WASM/browser
  acceptance; timing-model/wire allocation work and the throughput target remain.

### STL container review and compiler-control correction

- Audit scheduling, pin lookup, timing, memory/history and recorder container
  requirements. Add 237 standalone comparisons and two 42-run integrated
  matrices, retaining every result and the superseded exploratory comparisons.
  Tiny-table and trace-row crossovers support selective policies; matching-PIC
  controls do not establish an engine-wide win from simple container swaps.
- Correct attribution of earlier compiler/prototype differences: override
  objects had omitted the frozen library's `-fPIC`. The original numbers
  remain evidence for those configurations, not an isolated `-O2` effect.
- Twelve matching-PIC native and three full-tick decoder checks pass; integrated
  counters/state and 42 new traces match the existing golden data. Headless
  buffer reuse removes 24,837 allocations in the short guest, a modest 0.19%.
  Keep production unchanged and prioritize cached execution indexes and value
  buffers. No full-game, full-suite or WASM run. See the
  [performance record](records/2026-10-26-engine-performance.md).

### Engine performance research equipment and first measurements

- Open the [engine performance record](records/2026-10-26-engine-performance.md)
  and add serial, memory-capped benchmark/profiling equipment with frozen
  inputs, common result envelopes and per-command failure/resource receipts.
  Keep the live engine and gameplay artifacts unchanged.
- Complete 126 matched comparisons, allocation diagnostics and twelve queue
  stress runs. A selected compiler configuration reduces median CPU time
  11–18% on the small RV32IM loop (PIC attribution corrected above); direct
  array comparison removes 32% of C++ allocations. Preserve
  the slower per-bit attempt and the bucket prototype's sparse-queue regressions.
- Fifteen focused native, three full-tick decoder and two scheduler contract
  checks pass. All matched counters/states/digests and 21 streamed traces agree.
  Peak process RSS is about 340 MiB, zero swaps and no experiment OOM. Hardware
  perf is blocked; gprof attribution is partial. One prototype generation
  compile failure is recorded and corrected. Full-game/full-suite/WASM/timing
  coverage and the 10-million-event target remain open; no deployment.

### Preview recovery from RAM-backed recording copies

- Restore port 8893 after repeated 768 MiB OOM failures during MkDocs startup.
  Move temporary builds from tmpfs to a managed disk-backed user cache, so
  immutable tick recordings use same-filesystem hardlinks. Keep the memory
  cap, no-swap policy, live rebuilding and gameplay capture unchanged.
- Validate a private strict preview and the normal service startup. Six HTTP
  routes, the reported `0.0.0.0:8893` endpoint, recorded chunk SHA-256 and inode
  sharing pass. Cgroup OOM and automatic restart counts remain zero. The asset
  regression passes; no native/game/WASM rebuild or broad test run. Add a local
  preview guide. See the [recovery record](records/2026-10-26-preview-memory-recovery.md).

## 2026-10-25

### Full tick gameplay capture and replay

- Add compressed, seekable signal deltas to the existing visualizer, including
  delayed propagation, register state and native memory writes. The full
  timeline uses PRESENTs as navigation markers. Old checkpoint captures retain
  their original granularity and need native recapture for intervening ticks.
- Bound native capture/browser decoding and validate chunks plus every PRESENT
  state before publication. Preserve single-copy immutable binary chunks in
  same-filesystem MkDocs builds. Add a separate 12 GiB full-tick campaign for
  the approved five Visual demonstrations; preserve completed frame-only games
  and stop their old worker/deferred retry.
- Serial native builds, five focused CTests and a real 4,024-tick Visual
  browser guest pass, including every PRESENT pin/wire/device/register value,
  intermediate stepping, memory pages and rewind. Current frozen native output
  matches the trial byte for byte, with unchanged events/results excluding host
  timings. The strict preview build and ten live 8893 assets/routes pass; a
  small full-tick validation guest is published. The five-game background unit
  is active under its memory guards. Full gameplay completion and broader
  suites remain pending. See the
  [record](records/2026-10-25-full-tick-gameplay.md).

## 2026-10-24

### Device-system fidelity switching and layout completion

- Fix Logic, Visual and Compact failing on missing browser placements for
  newly selected memory/controller and private compact device structures.
  Reuse compatible layouts or generate geometry for the actual child shape,
  keeping original/manual/unrelated entries and Reset Layout defaults intact.
- Expose shared contract restrictions before expensive construction: the
  device system's 256 KiB RAM excludes strict Gate. Disable that choice with
  its reason, reject direct requests early in native/WASM scenarios and the
  guest runtime, and display readable C++ exception messages in the worker.
- Focused native capacity/API/migration/layout checks, three real native system
  builds, eight native/WASM parity cases and timing/system browser regressions
  pass. Strict package/preview builds and live 8893 asset, preset and recovery
  checks pass. The frozen background game queue is unchanged. No circuit,
  timing-table or full-suite changes/checks. See the
  [record](records/2026-10-24-fidelity-presets-and-visual-gameplays.md).

### Short gameplay demonstration campaign

- Cancel the 3,090-frame visual long queue at the owner's request and preserve
  its saved logs/topology/manifests. Start the approved 146-frame demonstrations
  serially with Visual: Snake 10 first, then Pong 45, 2048 25, Sokoban 34 and
  Tetris 32. Retain the 768 MiB service / 640 MiB native address-space caps,
  no service swap, frozen execution and host-headroom/disk guards.
- Add reproducible `--demonstrations` preparation using the reviewed input
  plans. Give the new campaign separate publication paths and `demo-*`
  recording identities; keep long preparation available. Update the recording
  library, procedures and cancelled-run index link.
- All five rebuilt ELFs, input scripts and expected frame/report files match
  the reviewed candidates exactly. Complete host references and instruction
  audits pass, along with the pacing fixture, syntax/diff checks and strict
  preview build. Live index/manifest/documentation links pass; the new unit
  advances real CPU cycles with no swap and the old unit is inactive. Native
  game completion is pending. No engine/WASM rebuild or full test suite was
  needed. See the [record](records/2026-10-24-fidelity-presets-and-visual-gameplays.md).

### Visualizer fidelity presets and visual gameplay restart

- Add Gate, Logic, Visual and Compact beside Timing in native/WASM scenarios,
  retaining scenario defaults, preset URLs and separately saved custom edits.
  Both runtimes use one C++ policy; recorded replay fixes its capture fidelity.
- Add validated compact console/timer/buttons/framebuffer compositions from
  their structural control builders and proven smaller models. Keep strict
  leaf validation for other behavioral implementations. Export large structural
  game graphs as bounded scope files shared by 16-checkpoint recording parts;
  replay retains two scopes and one state window, with every PRESENT preserved.
- Stop the earlier compact queue and preserve its partials. Start all five
  frozen long workloads serially with Visual under 768 MiB/no-swap service
  limits and a 640 MiB native address-space cap. Measured structural throughput
  requires a 90-day native wall allowance and 10-trillion-event bound, retaining
  100-million-cycle/instruction limits. Full-game results are pending.
- Checked focused native component/runtime/preset checks, four API/transport
  checks, eight native/WASM parity cases, profile and real cross-part replay
  browser checks, strict Pages/preview builds, live 8893 asset/selector/replay
  checks and syntax/diff checks. Initial
  implementation/test failures and their corrections are retained in the
  [record](records/2026-10-24-fidelity-presets-and-visual-gameplays.md).
  No full suite or completed long-game/native-WASM acceptance is claimed.

### Complete scenario menu order

- **Order:** apply the owner's selected overall sequence to all 26 groups:
  foundations, ALU, memories, RV32I and programs 01–22, RV32IM and programs
  23–27, Device I/O, then developer circuits. Rank all 197 component entries
  and 22 RV32I program definitions explicitly, placing supporting parts
  before complete devices/cores and widths in numeric order. Keep names,
  aliases, contracts and saved layout/profile identities; move native FF
  scaling beside its smaller FF technology scenarios.
- **Fix:** remove browser alphabetization that overrode catalog ranks;
  share deterministic ordering between Python and JavaScript. Add final-menu
  order assertions to catalog/browser checks and update public references
  and catalog-only refresh guidance.
- **Checked:** five focused catalog methods pass, including Python/JS,
  reversed registry order and legacy aliases. Strict packaging and preview
  builds pass; a fresh live 8893 WASM session verifies all 219 menu rows,
  group labels, both source asset hashes and zero page errors. Browser
  scripts pass syntax checks. No full native/browser suites, engine rebuild,
  new circuit recording or timing/layout regeneration. The frozen long
  gameplay queue stays active and unchanged. See the
  [record](records/2026-10-24-scenario-menu-order.md).

### Long gameplay plans, bounded timelines and background execution

- **Sessions:** prepare separate substantial play scripts for all five games,
  totaling 3,090 planned frames: 901 Pong, 513 2048, 303 Sokoban, 604 Tetris
  and 769 Snake. Host preflight demonstrates multi-point rallies, merges,
  eight solved multi-crate puzzles, 29 Tetris line clears and Snake growth.
  These are planned/reference outcomes; full native sessions are ongoing.
- **Execution/records:** add finite presentation-paced real device inputs,
  an audited external-ELF native probe and bounded 32-checkpoint recording
  parts. The original visualizer exposes one continuous lazy timeline with
  cross-part Play/rewind. Existing short fixtures and standalone records stay.
  No circuit or delay-table change was needed.
- **Background:** freeze executable/guest/script/reference/layout tools and
  launch `circuitsim-long-gameplays-1792818961.service`. Pong is actively
  simulating; four games follow serially. MemoryMax is 768 MiB, swap zero,
  native AS 640 MiB, CPU quota 90% of one CPU and nice 10. Owned work stops
  on host pressure; completed publication hashes support per-game resume.
- **Checked:** all five full host references/instruction audits; two focused
  native compatibility checks; three real native frames split across three
  parts with exact frame/report/input checks; legacy/playlist transport tests;
  continuous original-canvas browser playback; strict site/preview builds
  and live index/viewer checks. The active job passed 57,344 cycles and its
  first PRESENT. Full long native completion remains pending; no full general
  regression, WASM/expanded/cache campaign, commit, push or remote deployment.
  See the [plan and run record](records/2026-10-24-long-gameplay-campaign.md).

### Complete native gameplay recordings in the original circuit visualizer

- **Records:** freshly run all ten packaged fixtures on one source/binary
  snapshot: bring-up, five games, 2048 merge, Tetris line clear, Pong rally
  and Snake wall collision. Publish 47 synchronized PRESENT checkpoints,
  actual topology/pin/wire/device states, layouts and per-game source/build,
  timing, reference and browser evidence. The public game page links every
  capture in the existing visualizer. Earlier frame-only exports retain
  their source identities; previous Pong files are backed up locally.
- **Validation:** all ten exact host frame/report/input comparisons and ten
  original-canvas browser sessions pass, including every retained pin/wire
  vector, display/input state, inspector, rewind, Play and Device views.
  All runs report zero setup violations. Strict preview packaging passes;
  live 8893 checks verify three routes, all ten game links and 79 exact assets.
  The final live Pong replay also passes all 18 checkpoints and viewer controls.
- **Resources:** native execution totals 2h 6m 29s, with at most 155.58 MiB
  native RSS. Commands are serial, histories disabled, native AS capped at
  768 MiB, and the owned-process guard stops below 768 MiB host available.
  No capture or browser run hit OOM or a guard stop. Node/renderer/driver
  heaps are bounded; browser memory is separate from native RSS.
- **Scope:** the complete native baseline record set, not the separate
  repeat/quantum/expanded/cache/WASM campaign or live input. No full general
  regression suite, commit, push or remote deployment. Full phase 6 remains
  open in the [game record](records/2026-10-21-scripted-games-phase4-6-implementation.md).

### Device I/O hierarchy, overview layouts and scenario ordering

- **Structure:** replace flat device internals with functional structural
  groups for requests, control, decoding, storage and read-data selection.
  All twelve device scenarios were inspected; the router now has ten direct
  children instead of 1,589, and buttons have eight instead of 1,893. Public
  contracts and observer boundaries are retained. Regenerate the five changed
  unit timing entries; embedded/desktop entries remain unchanged.
- **Visualizer:** recalculate affected layouts and give the complete device
  system an explicit CPU/routing/RAM/peripheral floorplan with visible
  captions. Migrate moved saved fidelity paths and unchanged default styles
  while retaining manual choices. Order groups RV32IM, RV32IM programs,
  Device I/O; order components from steps to core, programs 23–27, and device
  helpers through the complete system. Update component references.
- **Checked:** 27 focused native registrations, 21 stored timing comparisons,
  catalog/profile/layout migration checks, sixteen static scenario layouts,
  twelve native/WASM comparisons and the 22-page/13-session browser selection
  pass. Twelve example groups regenerate and strict package/preview builds
  pass. Final system-only geometry checks pass in isolated and live views;
  8893 serves fourteen verified routes and 26 exact assets. The full browser
  selection preceded the final system caption adjustment, which received a
  focused recheck. No complete suites, expanded-core timing certificate,
  game campaign, push or remote deployment. See the
  [record](records/2026-10-24-device-io-hierarchy-and-layouts.md).

### Shared structural constants and arithmetic width labels

- **Constant wiring:** use existing supported-width constant primitives for
  fixed words and share scalar zero/one sources per wiring scope. Arithmetic
  launch/sign defaults avoid splitting and rejoining a constant bus; the
  SRAM/ROM sequencer also shares its zero tie. Both 32-bit arithmetic
  datapaths shrink from 46 children to 14, with one zero-word source each.
- **Inspection:** refresh 61 layout scopes, preserving surviving-child
  migration history and manual choices. Correct step-component pages and
  navigation to `<W>`, list all four widths, and show `<4>` in the menu.
- **Validation:** nine focused native registrations and the catalog retry
  pass; 45 stored timing tables recompute unchanged across three profiles.
  Thirty static layouts, 729 automatic migration entries, three native/WASM
  comparisons, three isolated browser sessions and one live divider session
  pass. Nine regenerated example groups leave the bundle byte-identical.
  Strict package/preview builds and eight routes/26 asset hashes pass on 8893.
  See the [record](records/2026-10-24-structural-constant-words.md) for corrected
  check-launch/filter mistakes. No complete suite, game campaign or push.

### RV32IM algorithms, numbered programs and instruction inspection

- **Fixed:** the core's saved EXECUTE placement still assumed its former
  tall shape. Refreshed M-core floorplans to align stage centers and widths;
  unchanged shipped layouts upgrade while custom/manual placements survive.
- **Instruction memory:** M machines now show all eight M mnemonics in
  structural and compact views. ISA selection follows the containing
  machine's contract; I-only contexts retain strict RV32I decoding.
- **Programs:** number the M scenarios 23–27, preserving existing descriptive
  aliases and adding numbered URLs. Program 26 computes Fibonacci by fast
  doubling with 12 MUL and five DIVU instructions. Program 27 computes GCD
  with REMU/DIVU and a MUL quotient check. Both run loops, store intermediate
  results and verify commits, registers, memory and M retirement counts.
- **Checked:** eight focused native tests (19.59 seconds), six static
  layouts, alias/migration checks, five WASM program exports with decoded
  memory text, strict combined site and two browser sessions (75.54 seconds).
  The refreshed 8893 preview passes eight routes, 26 exact asset hashes and
  live Fibonacci inspection/playback.
  Details and limits are in the
  [topic record](records/2026-10-23-rv32im-program-expansion-and-inspection.md).
- **Limits:** no production circuit/timing-table changes, full suite,
  benchmark/game campaign, push or published-site deployment.

## 2026-10-23

### Grouped RV32IM hardware and dedicated recorded programs

- **Structure:** multiplier/divider now expose CONTROL, OPERANDS, DATAPATH
  and RESULT groups. MulDiv has named result selectors; execute groups its
  M control and selection alongside the I stage and forwarding. Decoder
  overrides and coordinator holds also have separate structural boundaries.
  Existing lower-level component fidelity remains selectable.
- **Examples:** strengthened all ten M component recordings to 7–49 rows,
  adding all M encodings, signed/unsigned boundaries, forwarding and
  pause/hold/take/cancel/reset cases. Added three dedicated visualizer
  programs: arithmetic/x0, dependencies with stores/loads and branch flush,
  and divide/sign edges. They use real memory endpoints, check commits and
  architectural state against the oracle, compare final memory and retain
  inspectable timelines. `recorded-program` selects only those three CTests.
- **Views and timing:** complete shared group layouts and an explicit execute
  floorplan. Migrate moved named register/step fidelity choices and retain
  custom layout settings for children still present. Regenerated the changed MulDiv delay entry;
  its unit certificate is 68 ticks, while the core remains 94. Embedded and
  desktop certificates are unchanged.
- **Checked:** focused native scenarios/programs, registry and migration;
  selected delay recomputation and standalone timing certificates; 14 static
  scenario layouts and 14 regenerated WASM example groups; strict combined
  site; four browser sessions and a two-session final floorplan recheck.
  Port 8893 passes eight routes, 26 exact asset hashes and live program
  inspection/rewind after its strict rebuild. Retries and the later standalone
  MkDocs shell's SIGTERM are recorded in the
  [topic record](records/2026-10-23-rv32im-grouping-and-program-scenarios.md).
- **Limits:** focused acceptance only; no full suite, benchmark/game campaign,
  push or published-site deployment. Unrelated verified example exports and
  I-core type/profile layouts are retained.

### Structural RV32IM/device scenario views and descriptive roots

- **Changed:** renamed 22 M/device/system scenario roots to component-specific
  names, matching existing core views. A shared inspection profile shows
  structural architecture/functional blocks, including MulDiv and the IM core,
  while retaining compact storage and lower-level word choices. The existing
  RV32I core defaults remain structural.
- **Fixed:** root expansion previously built a structural circuit but then
  failed with missing static layout placements. Filled shared structural type
  layouts and current/canonical root overlays from matching saved structures.
  Native/WASM profile migration preserves old `DUT` root/descendant choices.
  MulDiv now places its multiplier and divider prominently above control
  logic; saved unchanged defaults upgrade while custom/manual layouts survive.
- **Checks:** the native build, 28 focused native registrations and static
  layout checks across 25 scenarios pass. Serial WASM/site builds, all 156
  example groups, 22 native/WASM comparisons, frozen Pong playback and the
  22-page/23-session device browser selection pass. A final MulDiv/I-core
  browser check verifies the new floorplan. Port 8893 serves the package:
  seven routes, 26 exact asset hashes and three structural views pass. Failed
  fixture attempts and successful retries are recorded in the
  [implementation record](records/2026-10-23-rv32im-scenario-visualization-fixes.md).
- **Scope:** scenario presentation and saved views; individual component
  circuits, timing tables and frozen gameplay captures retain their contracts.
  Full native/web suites, the remaining game campaign and published-site
  deployment were not run.

### ALU interface: `ALU<W>` in Chapter 2, `RV32IALU` for the cores

- **Why:** the operation-contract switch gave eight ALU unit names two
  unrelated pin sets, and configuration fields were silently ignored. The
  owner chose two named ALUs and dropped the adder-topology parameter
  ([record](records/2026-10-22-alu-exposure-review.md)).
- **Chapter 2:** one header per component; word datapath units take only W.
  `RippleCarryAdder<W>` and `ParallelPrefixAdder<W>` replace `Adder<W>` with a
  topology argument; `AddSub<W>`, `ArithmeticUnit<W>` and `ALU<W>` always use
  the prefix adder. Carry ports are `C_IN`/`C_OUT` (`C_SIGN` on the prefix
  adder). Contract IDs lose the operation and topology segments (34 instead
  of 77). New compact forms: ripple adder at 16/32, prefix adder at 8/16;
  the 8-bit add/subtract, arithmetic unit and ALU forms were recharacterized
  for prefix carry.
- **RV32I:** fixed 32-bit `RV32IALU` and six `RV32IALU*` parts (`rv32i.alu*`),
  with the former integer gates unchanged; opcodes `rv32i::ALUOp` with
  `CLEAR` = 12. The integer full-flag ALU, flag unit and 8/16-bit/ripple
  integer variants are retired. Branch-target and PC adders are requested as
  prefix-adder families.
- **Site:** pages 2.1–2.6 describe only the Chapter 2 components, with a
  ripple-versus-prefix comparison and a construction section; page 2.7 is
  removed; § 4.3.3 now specifies `RV32IALU` and its parts. Visualizer menu,
  palette, layout keys and Python bindings follow the new names.
  `create_rv32i_alu` replaces `create_alu`; `create_word_component` takes only
  unit, name, width and fidelity, and the control decoder is width-independent.
- **Layouts and preview:** migrated 44 saved scopes to current structural
  signatures while preserving geometry. The pruning formatter preserves
  bracketed storage keys. Restored port 8893 with the fresh package and a
  service restart; the timing hook now refreshes its table cache each build.
- **Checked:** guarded serial native/WASM builds; focused native ALU,
  documentation and SRAM/cache layout checks; all 156 generated example groups;
  strict site build; 18 changed tables in all three profiles; all compiled
  native/WASM delay snapshots match (809 entries per profile). The three
  focused portable parity registrations and all six selected browser
  registrations pass, including 731 Chapter 2 table rows, live fidelity
  switching and 32 camera bounds checks. The preview passes six live routes
  and 26 exact non-HTML asset hashes. Failed fixture/environment attempts and
  their successful corrections are retained in the topic record.
- **Limits:** the complete static scan cleared 367 scenarios, then its two
  fetch-root findings were repaired and verified with a focused recheck.
  No full native/web suite, remaining phase-6 game campaign, complete
  gate-level reanalysis of all timing entries, push or published-site check
  is claimed.

## 2026-10-22

### Complete replacement of fixed components with parameterized recipes

- **Components:** `ALU<W>` and arithmetic/logic/shift/compare/flag/select units
  use shared width-driven structural recipes and value rules. Ripple and
  parallel-prefix addition work at 8/16/32 bits; prefix is the ALU default.
  `Mux<Inputs,W>`, `Decoder<AddressBits>`, `RegisterFile<W,Count>`,
  `Incrementer<W,Step>`, iterative M units and bus/pixel helpers replace their
  fixed wrappers. Configured peripherals keep the declared 32-bit MMIO ABI.
  `RegisterFile32x32` is the approved architecture exception and shares the
  generic register-file recipe.
- **Contracts and views:** retired classes/factories/IDs and runtime aliases
  are removed. Callers, exact profile rules, scenario names, saved layouts,
  Python/WASM bindings and current references migrate completely. The ALU
  chapter uses a shared 64-row generator (16-bit prefix as its primary view),
  documents both adders, and owns the integer component references; RV32I
  specifies its configured ALU. Old document routes/anchor aliases are removed.
  Parameter-aware layouts, pin-footprint clearance and family colors support
  the generic parts. The 8893 package includes fresh Pong circuit/evidence
  assets and the current references.
- **Checked:** 23 focused native registrations, two API registrations, seven
  reference/metadata registrations, four catalog methods and six portable/
  browser registrations pass. Portable checks cover 21 extended-ALU scenarios,
  nine integer/Execute scenarios and all 22 M/device recordings. Browser checks
  cover the adder article/embeds, 23 component pages, selected ALU/display/button
  sessions and every retained Pong pin/wire/device state with original playback.
  All 158 example groups and strict package builds pass. Three timing profiles
  are regenerated with 809 entries each; all 780 retained-ID entries are
  unchanged in every profile. Complete unit-file verification passes. Selected
  layout checks and live 8893 routes/assets/retired-anchor checks pass.
- **Game/geometry evidence:** fresh Pong has 18 exact-reference frames and
  identical retained signal vectors, simulated times and event counters to
  the prior capture, with zero setup violations. Current APIs cover 320x200;
  actual CPU high-address stores/geometry reads/PRESENT pass at 128x96/97x65.
- **Limits:** serial guarded local builds/checks, not full native/web suites,
  a controlled throughput benchmark, the remaining phase-6 game campaign or
  phase 7/live input. Embedded/desktop timing files are regenerated, with only
  unit rerun through complete-file verification. Failed migration checks and
  environment attempts are preserved with their successful corrections in the
  [closed replacement record](records/2026-10-22-general-component-retirement.md).
  Unrelated dirty work and deployed native extensions remain preserved; no
  commit, push or published-site verification is claimed.


### Parameterized ALU references through RV32I

- **Changed:** Chapter 2 specifies width-parametric arithmetic/logic, shifts,
  comparison, flags and complete ALUs. Width, operation contract, topology and
  behavioral availability are explicit. The integer component references are
  in §2.7; RV32I §4.3.3 specifies the CPU configuration and Execute wiring.
  Home/navigation, generic API, register-file and core references agree with
  that organization. Concrete example aliases, C++ references and component
  fragments are preserved. The 8893 preview includes the timing-table hook.
- **Checked:** six focused documentation checks pass, including compiled
  reference replay and live pin/fidelity schemas; final integer/timing repeat
  passes; strict MkDocs builds pass. All 22 relevant C++ examples and recorded
  marker identities are unchanged. All 18 affected live articles match the
  strict build, and 989 local article links/anchors pass. Checks/builds ran
  serially under the 768 MiB cap. Missing storage/flag links and the omitted
  timing paragraph found during review are corrected.
- **Limits:** documentation and check-path/expectation changes; existing
  simulator package reused. No full browser/WASM suite, game campaign,
  runtime component change or remote deployment. See the open
  [width record](records/2026-10-22-width-component-audit.md).

### Pong replay through the original circuit visualizer

- **Implemented:** bounded captures from the actual checked game circuit,
  a native `RecordedCircuitProbe`, layout packaging and a captured-state
  transport for the existing canvas/explorer/inspector/timeline. Device views
  show the game screen and input state on their actual components. Progressive
  wiring, a small layout sidecar and a capture-specific detail threshold keep
  replay within the host's limits; existing scenario/layout defaults remain.
- **Verified:** actual Pong rally, 115,686 cycles / 29,005 retirements / 18
  changing frames / zero setup violations; all exact host-reference screens
  and reports; six focused native and three JS checks; strict preview build;
  all 18 retained pin/wire vectors and display/input states in the live
  original canvas, with inspector, rewind, Play and Device views. All 32
  served simulator/circuit artifact hashes match. The first browser attempt
  exposed missing layouts; three guarded memory stops led to the bounded
  implementation before the final browser pass.
- **Limits:** native-produced presentation checkpoints only; no intermediate
  gate transitions, historical RAM pages or live input. Full game/WASM capture
  acceptance is still open in the [phase-6 record](records/2026-10-21-scripted-games-phase4-6-implementation.md).
  No native live-module deployment, linked worktree, commit or remote push.
  Contract: [recorded circuit checkpoints](reference/recorded-circuit-checkpoints.md).

### Clarified game visualization acceptance

- **Recorded:** the owner requires scripted games in the original circuit
  visualizer, with abstract device visuals on the actual CPU/device assembly.
  Existing component views are present, but game/session integration remains
  missing. Added that requirement to the open phase-6 records and status;
  separate recording/GIF checks cannot certify it. Live input remains phase 7.
- **Reviewed:** original canvas/inspector rendering, device metadata, the
  device-system scenario and batch/session boundaries. Documentation only;
  no runtime/UI edits, new tests, builds or interruption of the serial campaign.

### Visible native bring-up and 2048 recordings

- **Changed:** added actual bring-up/2048 JSON captures, animated previews
  and a hash manifest to the public recorded-games page. Restored the main
  8893 preview with the already-built current simulator/example package;
  the previous package is preserved. Its old example bundle had blocked
  strict rebuilds after ALU16 was added.
- **Checked:** bring-up's four native configurations pass; 2048 baseline
  passes four exact host-reference frames. Strict preview build passes
  (55.94 seconds, 178,652 KiB peak RSS), five live routes and all 27 simulator
  plus five export hashes match, and all six saved frames pass live browser
  RGBA/keyboard replay checks. The serial campaign resumes after these checks.
- **Limits:** these are short predetermined recordings. The GIFs replay at
  one frame per second, independently of simulation throughput. Other game
  configurations, game WASM and full browser acceptance remain pending;
  phase 6 is open and phase 7 is unimplemented. No native live module,
  circuit source, commit or remote deployment changes. See the
  [phase-6 record](records/2026-10-21-scripted-games-phase4-6-implementation.md).

### Shared word components and configurable device helpers

- **Implemented:** shared structural builders behind `ALU<W>`, word units,
  adders, muxes, register files and iterative multiply/divide components.
  Operation contracts, adder topology and flag outputs are explicit choices;
  legacy factories, pins, operation maps and profile IDs are retained.
  RV32IM continues to use the shared five-stage assembly.
- **Implemented:** 8/16/32-bit device transport and pixel selectors, validated
  router maps and device-view capabilities. The game peripheral register ABI
  remains 32-bit. Guest drawing now reads physical display geometry and
  aligns stores using the actual row address. Rebuilt all ten audited RV32IM
  fixtures. Public contracts are in the new
  [word component reference](../site-docs/components/word-components.md).
- **Checked so far:** serial isolated native build; 555 distinct selected
  native registrations, including three large SRAM checks; 72 new width/timing
  cases; all 808 stored delay entries across each of three profiles; WASM width
  execution and 15 portable compatibility checks; strict packaging of 158
  example groups; 367 static layouts. Added 23 layout entries without
  changing old entries. The 175-name full Python API snapshot is preserved,
  and all five later additions remain present. Node required an unchanged
  retry without a virtual-address cap. No task command was observed OOM-killed.
- **Open:** current-source compiled-game, game WASM, browser, performance
  and final package acceptance continue in the
  [phase-6 record](records/2026-10-21-scripted-games-phase4-6-implementation.md)
  and [width record](records/2026-10-22-width-component-audit.md). New width
  variants are structural; phase 6 is not yet accepted and live phase 7 is
  unimplemented. The isolated build does not replace the native live module.

### ALU width configuration design

- **Documented:** continued the ALU8/ALU32 comparison in the consolidated
  [width record](records/2026-10-22-width-component-audit.md), reopened for
  design discussion. Proposed one ALU<W> implementation with named semantic
  contracts, explicit adder topology and compatible old catalog factories.
  Word width alone must not change operation maps or flag conventions.
- **Scope:** reviewed current ALU source/public pins and RV32IM composition.
  Confirmed shared pipeline assembly and unchanged fetch/memory/writeback;
  M extends decode, EX and coordination. Recorded duplicated physical operand
  forwarding as a future sharing opportunity. Added an implementation sequence
  and compatibility/validation criteria; no component, test, public page,
  build, simulation or service changed.

### Consolidated component width record

- **Changed:** merged the game-roadmap width analysis into the earlier
  [component width record](records/2026-10-22-width-component-audit.md) at the
  owner's request, retained both sets of dated evidence, updated links and
  removed the duplicate record. The summary now states the protocol/data-
  width refinement and the relationship between the 40-class and 22-component
  inventories.
- **Checked:** dated-body preservation, local reference targets and whitespace.
  Documentation only; no component changes, build, simulation, test suite or
  service restart.

### Game-roadmap width and parameter review

- **Changed:** classified the adopted 22 components into 12 fixed-width
  suffixes, six ISA identities and four parameterized blocks. Proposed reuse
  of the existing data-width protocol, with separate arithmetic, register,
  device-state and display parameters. Updated the roadmap summary for the
  main alpha checkout and current implemented contracts, preserving dated
  sections. See the [review](records/2026-10-22-width-component-audit.md).
- **Reviewed:** current hardware/protocol source, register ABI, public device
  references, observation adapters and guest drivers. Fixed sizes extend to
  operation counters, register transfers, 32 keyboard levels and 64×48 guest
  drawing. The prior geometry work does not establish generic game rendering.
- **Limits:** analysis/internal documentation only; no component/guest change,
  tests, compile, simulation, browser or service restart. Width refactoring
  remains proposed; phase-6/phase-7 acceptance status is unchanged.

### Main alpha checkout and retirement of linked worktrees

- **Changed:** switched the main repository to `alpha`, restored the recent
  preview/audit documentation there and moved port 8893 to the main directory.
  Saved older pending work in a named stash and external backup. Removed both
  linked Git worktree registrations while retaining their source/build/evidence
  directories as ordinary ignored archives with unchanged entry metadata.
- **Verified:** both sets of pending-file hashes in stashes/backups; only the
  main alpha checkout in `git worktree list`; guarded strict preview build
  (43.76 seconds, 179,736 KiB peak RSS); eight local documentation routes and
  all 27 served simulator hashes. Preview peak was 287.72 MiB, with zero OOM
  events or automatic restarts. Existing branch references and prior build
  artifacts are preserved. See the
  [consolidation record](records/2026-10-22-main-alpha-checkout.md).
- **Limits:** no commit/push, native/WASM compile, native regression or
  browser gameplay check. Documentation remains uncommitted on main alpha;
  phase-6 acceptance and phase-7 status are unchanged.

### Fixed-width component inventory and ALU comparison

- **Changed:** recorded 40 public classes named for fixed 8/32-bit widths,
  the additional ALU32Core family, existing parameterized precedents and
  proposed generic implementations. Distinguished data width from opcode
  contracts, mux input count, bus format and framebuffer geometry.
- **Reviewed:** current headers/circuit implementations, public ALU pages,
  catalog/profile contracts, visualizer type matching and test definitions.
  The audit identifies compatibility and four-state/timing differences that
  a future refactor must preserve. See the
  [audit record](records/2026-10-22-width-component-audit.md).
- **Limits:** internal documentation only; no component refactor, test run,
  build, simulation or service change. Existing work remains untouched.

### Local documentation preview follows the alpha checkout

- **Changed:** moved port 8893 to the dedicated `alpha` worktree using a
  local user-service drop-in, including both absolute startup checks.
  Staged its matching simulator package without a compile; retained the
  preview hook and durable runtime directory. The service now has a 768 MiB
  memory limit with no swap.
- **Verified:** guarded strict preview build (46.15 seconds, 180,176 KiB peak
  RSS), eight HTTP documentation routes and all 27 served simulator file
  hashes. Service peak after the HTTP checks was 279.72 MiB, with no OOM
  events or automatic restarts. Primary pending files and the feature
  checkout are unchanged. See the
  [preview record](records/2026-10-22-alpha-local-preview.md).
- **Limits:** reused the milestone's example bundle; no full regeneration,
  browser/runtime validation, native suite, GitHub push or remote deployment
  ran. Phase-6 acceptance remains open; phase 7 has not started.

### RV32IM milestone checkpoint and bounded build tooling

- **Changed:** merged the RV32IM/device/scripted-game checkpoint into `alpha`,
  retaining the newer visualizer work and original playability audit tools.
  Optional WASM link optimization and isolated example generation reduce
  peak memory without changing their defaults. Inspector parity releases its
  captured Python circuit before building the native comparison. A generator check verifies
  full/filtered output identity and failure-safe publication.
- **Verified before integration:** 54 portable checks, 11 portable system
  checks, exact native/WASM comparison of all 22 device scenarios and wide-time
  transport pass. All 175 old Python API entries are preserved, with five
  additions. Strict site packaging passes with 22 current new example groups
  and 135 retained legacy exports. Complete timing snapshots match in native
  and WASM; all 808 entries per profile verify.
- **Integrated checks:** serial native/WASM builds; 80 of 81 selected
  native/Python checks (one resource-limited cache-layout case); all 13 Node
  checks; four portable core/system/input checks; five device parity cases;
  exact-time and old memory parity; complete native/WASM timing identity;
  all 366 static layouts. Strict packaging refreshes five device groups and
  retains 152 earlier exports. Primary pending files remain byte-identical.
- **Limits:** full legacy example regeneration and a large SRAM retry stop
  safely at the host-memory guard. Current full-topology and browser
  attempts also stop there; no current browser appearance pass is claimed. Revised full-game/fidelity/quantum/performance and
  compiled-game WASM comparisons remain follow-up work. Phase 6 acceptance
  is open; phase 7 has not started. Integration results are recorded in the
  [wrap record](records/2026-10-22-rv32im-milestone-wrap-and-integration.md).

### Visualizer: modern look for toolbars, panels and controls (branch `realistic-timing`)

- **Changed:**
  - `visualizer-v2/static/styles.css` is rewritten around theme tokens:
    one dark theme, rounded floating panels, system sans and mono fonts,
    icon buttons (inline SVG masks), captioned Scenario and Timing
    dropdowns, a styled timeline with a progress fill, and a bottom dock
    centered over the visible canvas.
  - `index.html`: transport reordered around Play; icon-only buttons carry
    `aria-label` and `title`. All element IDs are kept.
  - `app.js`: a `playing` class for the pause icon, the timeline fill, and
    checkpoint marks drawn under the track (the styled slider hides the
    datalist ticks); canvas background and background grid are darker.
  - The desktop top bar no longer cuts off the Timing dropdown.
  - Kept mergeable with the uncommitted device I/O work in
    `.worktrees/rv32im-phase3`: the Fit button keeps its original line, the
    toolbar divider comes from CSS, and its `Device views` toggle is styled
    in advance. A three-way merge of the shared files has no conflicts.
- **Why:** the owner found the buttons and windows old-fashioned.
- **Verified:** screenshots at 1440, 1100 and 360 px, program view forced on
  Full Adder, and the docs embed; every top-bar and dock control inside the
  viewport and clickable at 960, 640, 360 and 320 px. Browser checks run
  against a scratch mirror of today's `build-wasm/site` with the edited
  files: `FFMemoryBrowserTest`, `MemoryInspectorBrowserTest`,
  `VisualizerScenarioBrowserTest` and `VisualizerTimingProfileBrowserTest`
  pass. `VisualizerWasmBrowserTest` passes its desktop and compact panel
  checks, then times out at line 618 waiting for the `/visualizer/` embed.
  The unmodified files fail at the same step in that mirror, so it is not
  from this change, but the rest of that test was not reached.
- **Limits:** the Pages build was not run, so the published simulator is
  unchanged. The native server was not used: the deployed
  `circuit_backend*.so` is older than the scenario catalog. Canvas drawing
  (component shapes, wire colors, Arial labels) is unchanged.

## 2026-10-21

### General device parameters and recorded visual abstractions

- **Changed:** framebuffer dimensions and backing capacity are validated
  independently, with surfaces up to 256 KiB. The system propagates capacity
  into video routing and reference checks. Queue depths 1–7 are parameters;
  default geometry, mappings, pins and child configurations are preserved.
  Non-default device configurations have separate visual floorplan keys;
  default keys and existing colors are retained.
- **Visuals:** shared native/Python/WASM observation adapters and browser
  display/keyboard/console/timer/FIFO previews are staged. Recorded-game
  frames carry checked input state; old recordings still open. Captured
  pixels have an aggregate budget. Browser acceptance remains pending.
- **Verified:** serial native build (369268 KiB peak RSS), 28 focused native
  checks (149812 KiB peak), FIFO structural/compact agreement at every depth,
  standalone 320×200 storage, actual CPU presentation at 128×96 and 97×65,
  and read-only pixel/keyboard history with rewind. An initial observer type
  match and test-order error were corrected and the full focused set passes.
  Subsequent parameter/observer/clock checks, Python declarations and JS
  helpers pass. All 363 static scenario layouts pass with 52 new entries and
  no old value changed. Consecutive sampled input logs and halt-boundary
  rejection pass; fresh bring-up has two exact frames, checked reports and
  zero setup violations. CLI timeout/cycle/memory failures and stale-output
  rejection pass, with native build/source/binary/fixture evidence retained.
- **Limits:** the larger CPU checks execute short sequences and write a word
  near the visible surface's end; they establish routing/capture, not full
  screen drawing FPS. Final site/browser/WASM and broader phase-6
  acceptance remain open. Phase 7 is authorized after acceptance. Primary
  checkout work and deployed modules are untouched. See the
  [parameter/visual record](records/2026-10-21-device-parameters-and-visuals.md).


### Scripted platform bring-up and wide-time execution

- **Changed:** isolated RV32IM CPU/RAM/device assembly, batch input and frame
  capture, independent ISA/device checking, audited guest sources and an
  additive recording viewer. Explicit 64-bit simulation time and exact wide
  JSON ticks are a separate prerequisite.
- **Verified so far:** WASM standalone time/history/overflow checks; actual
  short guests with FF, structural logic, cached FF/DRAM and epochs above
  2^53/2^63. Compiled bring-up completed 23,922 cycles / 6,445 instructions,
  two checked frames and zero setup violations in 466.13 s, ~203 MiB peak RSS.
  Revised ten RV32IM fixtures, strict C rule checks and host reference builds
  pass. A guarded multi-configuration run stopped at low host memory; each
  configuration is now a separate check/process.
- **Limits:** bring-up used the preceding structural-M build. New compact M
  source, full game execution, exact binding parity, current native regressions,
  integrated timing and packaged browser/site acceptance remain pending. No
  live-session support, deployment or primary-worktree edits. See the
  [open milestone record](records/2026-10-21-scripted-games-phase4-6-implementation.md).


### Smaller Python binding compilation groups

- **Changed:** moved the module registration statements into six compilation
  groups with private shared templates, called in their original order.
- **Why:** the single binding source exhausted a 768 MiB compiler cap; a
  guarded 1 GiB retry stopped at low host headroom.
- **Verified:** the original registration body reconstructs byte for byte;
  the split builds at 768 MiB with 538,172 KiB peak RSS. Separate-process API
  snapshots preserve all 175 old public names/signatures/docs and add only
  the two recorded-guest functions. Full runtime regressions remain pending.

### Optional bounded compilation of generated timing tables

- **Changed:** a build-only partitioner copies canonical entries into eight
  compiler units per profile; native builds opt in, and the Pages build enables
  it. Public table functions and tracked timing data remain unchanged.
- **Why:** the fresh wasm32 Unit table compile exhausted its 1 GiB compiler
  cap. Keeping smaller units avoids raising memory pressure on this host.
- **Verified:** deterministic verbatim round-trip for all 754 entries in each
  profile and malformed/duplicate input rejection. Saved the preceding native
  value snapshot; fresh native/WASM complete snapshots match it byte for byte
  in all three profiles.
- **Pending:** isolated compiler-memory
  evidence and existing timing checks. See the [open record](records/2026-10-21-delay-table-compile-memory.md).

### RV32IM and device foundations staged in an isolated worktree

- **Changed:** additive structural M arithmetic and five-stage core, independent
  reference semantics and pinned C fixture; device routing, register frontend,
  counter/FIFO, console/timer/buttons and framebuffer storage/control. Added
  component references, recorded examples and viewer metadata/layouts.
- **Compatibility:** old I families retain their identities and private assembly
  paths. Shared assembly is extracted without changing their pins or packets.
  A prerequisite table-only delay-publication guard fixes an existing empty
  vector access; a focused regression covers it. Corrected an existing CTest
  registration-order mismatch for the shared-memory program scenario.
- **Verified:** all 46 exact-label phase-3 native checks pass in 716.91 seconds,
  serial and memory capped, including independent oracle/C execution,
  structural/compact-child core agreement and three-profile timing analysis.
  All existing layout values are preserved; 48 new entries are added.
- **Additional preceding-binary checks:** 23 selected native compatibility
  checks and seven viewer checks pass; memory inspection reached its configured
  180-second limit. All 21 added scenarios have complete saved layouts.
- **Pending:** refresh these checks after the wide-time migration, finish the
  memory-inspector check, and validate the packaged site/WASM. These preceding
  checks do not validate the new integrated machine or games.
- **Scope:** uncommitted `rv32im-phase3` worktree from `60acda2`; primary ongoing
  work and deployed viewer modules are untouched. The owner subsequently
  authorized [phases 4–6](records/2026-10-21-scripted-games-phase4-6-implementation.md).

### Visualizer: two memory panels and one floorplan for every shared arrangement (branch `realistic-timing`)

- **Changed:**
  - Behind a shared cache (D-0, D-ID) the memory inspector tells the sides
    apart by the arbiter's owner bit for the request in flight, so these
    memories show two panels too.
  - One floorplan for all four shared arrangements: core and memory in the
    same place, middle parts spread evenly, constants by their caches, and
    the memory wide enough for its panels. §4.7 is updated.
- **Why:** the owner found D-0 and D-ID oddly laid out and without the
  two-panel view.
- **Verified:** inspector integration (two panels in B, D-0 and D-ID, sides
  at the right addresses), static layouts with the content guard, four other
  layout and catalog tests; Pages build; four browser and parity checks and
  screenshots.

### Visualizer: children kept inside their parent's content area (branch `realistic-timing`)

- **Changed:**
  - The shared-memory program-root floorplans place children inside the
    content area (below the title bar, inside the pin margins), and the four
    roots are recomputed. One `mux2to1-32bit` placement is moved below its
    title bar.
  - `VisualizerStaticLayoutTest` now fails on any placement outside a
    content area.
- **Why:** the owner saw parts over title bars in the new arrangement
  scenarios. A check found 11 such placements in five layouts.
- **Verified:** the static layout check over every scenario (0 problems, and
  it reports the old placements); five layout tests; Pages build; browser
  screenshots of the four scenarios.

### RV32I memory systems section; cache experiments split (branch `realistic-timing`)

- **Changed:**
  - New §4.7 Memory Systems: the five memory arrangements of the advanced
    core, each with an embedded scenario (new: `rv32i-memory-b`, `-d0`,
    `-did`).
  - The results page is now §4.8, with a memory-systems section that merges
    the former §3.8 experiments 2 and 3 as final results. §3.8 keeps the
    cache-alone experiment.
  - `RV32IProgramRoot` builds every shared arrangement, including an L2, and
    the inspector shows one panel behind a shared cache.
  - New floorplan and saved layouts, and a figure rescaled.
- **Why:** the owner pointed out that §3.8 read as history and that the
  arrangement experiment belongs to the RV32I chapter. The owner also wanted
  a scenario per arrangement to show how the arbiter is connected.
- **Verified:**
  - The four arrangement scenarios against the instruction-set reference.
  - Inspector, layout, catalog, topology and clock checks; 18 documentation
    checks; the plot validation (602 cells).
  - Strict Pages build, and 11 browser/parity checks plus a browser load of
    each new scenario.
- **Not run:** the full native suite.

## 2026-10-20

### Site checks: deterministic gate order, figure fixes, records closed (branch `realistic-timing`)

- **Changed:**
  - Gates that were built inside one call's arguments are now created in
    explicit statements, in the native build's order. The affected files are
    `MemoryFFController.hpp`, `DRAM.cpp`, `NORFlash.cpp`,
    `MemorySequencers.cpp` and `TwoPortMemoryArbiter.cpp`. C++ leaves that
    order open, so GCC and clang (WebAssembly) had listed the same parts
    differently.
  - Seven figures get label spacing fixes: cache access patterns, cache
    topologies, memory endpoints, SRAM device, transaction handshake, SRAM
    access chain, memory latency.
  - The simulator page shows its embed before its explanation.
  - `MemoryDeviceDocumentationBrowserTest` counts example tables only.
  - The general cache plan, memory chapter and shared layout keys records are
    closed, and STATUS is updated.
- **Why:** the owed site build and browser/parity checks, run once the
  general cache plan settled.
- **Verified:**
  - Pages build passes, with every example row verified.
  - 26 browser, WebAssembly-runtime and parity checks pass; the parity checks
    used the fresh native probe in `build-cleanup`.
  - Native: 30 memory-labelled tests, 48 further memory tests, the layout and
    catalog checks, and all eight cache tests (including `CacheReadTest`).
  - The unified memory view checked in the browser.
- **Not run:** `PartDelayTablesCompleteTest` and the full native suite.

### Visualizer: cache controller layouts regenerated (branch `realistic-timing`)

- **Changed:**
  - The saved placements of the five cache controllers the scenarios use are
    regenerated, and six unused entries are removed. Their layout keys follow
    the controller's children, which the general cache plan changed.
  - The cache floorplan keeps its old row arithmetic.
  - `CacheControllerEquivalenceTest` is renamed `CacheControllerLockstepTest`:
    registered test names may not contain "Equivalence".
- **Why:** `VisualizerStaticLayoutTest` reported 1,351 problems once the
  Python module was rebuilt.
- **Verified:** `VisualizerStaticLayoutTest` (0 problems), `CacheLayoutTest`,
  `VisualizerScenarioCatalogTest`, `RV32IMemoryLayoutTest`,
  `MemoryInspectorIntegrationTest`, `VisualizerSessionContractTest`,
  `VisualizerAutomaticLayoutTest`, `VisualizerModuleBindingsTest`,
  `BuiltinComponentCatalogInventoryTest`, `MemoryDeviceDocumentationTest`.
- **Not run yet:** the Pages package and browser checks (a memory-gated run
  is in progress).

### Cache behavior campaign and rewritten cache pages (branch `realistic-timing`)

- **Changed:**
  - §3.7 (`site-docs/memory/caches.md`) is rewritten: every setting (write
    policy, write miss, write buffer, replacement, fill, line state,
    controller), burst reads, invalidation with flush, the line-state store,
    the controller's parts and the compact controller.
  - §3.8 gains experiment 3, cache behaviors: write-through, a two-entry write
    buffer, write-back with allocate, and write-back with four ways, on C-ID,
    D-0 and D-ID, four programs and both profiles.
  - The simulator page describes the unified memory view.
  - New `tools/rv32i-performance/cache_policy_campaign.py`, and the tools
    README covers cache options.
  - The experiment's clock analysis has a larger node budget (three four-way
    caches exceeded the default).
- **Why:** step 7 of the general cache plan, and the owner's request to update
  the site docs once the plan was done.
- **Verified:**
  - 96 of 96 runs verified, with zero setup violations.
  - The 24 write-through runs reproduce the published campaign exactly.
  - The clock analysis certifies all 24 combinations at 4,104 / 2,858 ps.
  - Strict MkDocs build, `CacheDocumentationTest` and
    `DocumentationDiagramsTest` pass.
- **Results:** a write buffer gives 1.18–1.57× on programs with stores and
  removes most of the L2 penalty; write-back gains less and costs 9–21 % on
  `stream`; four ways change little except for the L2 alone.
- **Not run:** the Pages package, the browser checks and
  `MemoryDeviceDocumentationTest`. The last fails on the ROM page's `READY`
  pin with the stale native probe in `build-cleanup`, and is to be rechecked
  after the rebuild.

### Cache: compact controller running the gate list (branch `realistic-timing`)

- **Changed:** the cache controller is now a catalog contract,
  `memory.cache.controller.<W>`. The cache setting
  `controller=structural|behavioral` picks gates (the default) or a compact
  part.
  - The compact part runs the gate-level controller's own gate list
    (`GateListModel`: four-state gates, compact state rules, only the bits a
    change reaches).
  - It proves its delay table on the same gates when built.
  - Experiment tool option `--cache-controller`.
  - New `CacheControllerEquivalenceTest`.
  - The timing graph no longer treats a passed-on clock as a data output.
  - `part_delay_tables` skips the contract.
- **Why:** step 6 of the general cache plan, revised from a hand-written
  behavioral cache. The controller is the slow part, and running its gate
  list keeps every cache rule in one place (see the record).
- **Verified:**
  - Lockstep equivalence test: 8,561 steps identical.
  - S0–S2 traces: 24 of 24 records identical; cache paths 6.3 times faster.
  - CPU runs on C-ID, D-0 and D-ID, including write-back, store-mode, 4/8-way
    and buffered caches: identical cycles and final state, with no setup
    violations. D-ID `sort`: 325 → 1,646 cycles per second.
  - 4,104 ps certified with either controller.
  - Catalog, profile, topology and cache scenario tests pass.
- **Not run:** `PartDelayTablesCompleteTest`, and the visualizer and browser
  checks (pending).

### Cache: 4 and 8 ways, a choice of replacement (branch `realistic-timing`)

- **Changed:** caches take 1, 2, 4 or 8 ways and `replacement=lru|plru|fifo|random`.
  Exact lru works for up to two ways; with more ways the default is plru.
  The controller carries way indexes instead of one way bit. The two-way lru
  structure is unchanged, and the new policy bits sit in flip-flops
  (`CachePolicyBits`) or in the line-state word. The cache floorplan takes up
  to eight way rows and the line-state store. The cache test rig no longer
  records wire history.
- **Why:** step 4 of the general cache plan. Next-line prefetch (step 5) is
  deferred, because the controller serves one request at a time (see the
  record).
- **Verified:** `CacheReadTest` passes, with eight new reference-model streams
  covering every policy at 2, 4 and 8 ways, in flip-flop and store modes.
  `RV32ITopologyTest`, `RV32IBinaryClockCLITest` and the cache scenarios pass.
  D-ID cycle counts are unchanged for the existing caches (6,481 and 4,796),
  and the 4- and 8-way variants verify. 4,104 ps is certified with 4- and
  8-way caches.
- **Not run:** the Python module was not rebuilt (too little free memory),
  so the full `CacheLayoutTest`, `VisualizerStaticLayoutTest` and the browser
  checks are pending.

### Cache: line-state store for large caches (branch `realistic-timing`)

- **Changed:** `line_state=store` keeps the valid, dirty and replacement
  bits in a memory bank (one word per set) instead of controller flip-flops,
  lifting the set limit to 128 (gate-level) or 4,096 (scalable); the store
  is cleared set by set after reset and on invalidation. The default
  (`registers`) is unchanged.
- **Why:** step 3 of the general cache plan, revised from "bits in the tag
  words" to keep existing caches and results unchanged (see the record).
- **Verified:** `CacheReadTest` (store-mode streams including 256 sets), the
  cache scenarios, `RV32ITopologyTest`, a store-mode D-ID run.

### Cache: write-back and write-allocate (branch `realistic-timing`)

- **Changed:** caches take `write_policy=back` (dirty bits, write-back of a
  dirty victim before its refill, a flush walk before invalidation) and
  `write_miss=allocate` (a write miss fills its line, then writes into it).
  The experiment tool flushes the L1s, then the L2, after the program stops
  (new inputs `FLUSH_L1`, `FLUSH_L2`, fixed low for the clock analysis).
- **Why:** step 2 of the general cache plan.
- **Verified:** `CacheReadTest` with a write-policy reference stream,
  `RV32ITopologyTest`, `RV32IBinaryClockCLITest`, the cache scenarios; D-ID
  certified at 4,104 ps with write-back caches.

### Cache: write buffer (branch `realistic-timing`)

- **Changed:** caches take a `write_buffer` of 1, 2 or 4 entries: writes are
  answered in their lookup cycle and drained to the backing memory in order
  while no fill needs the lower port; a read miss waits for the drain. New
  structural part `CacheWriteQueue<W>`. The experiment tool takes per-cache
  settings (`--il1/--dl1/--l2 KEY=VALUE`) and drains buffers before checking
  memory.
- **Why:** step 1 of the general cache plan; write-through to two levels was
  the measured cost of the L2 under the L1s.
- **Verified:** `CacheReadTest` with a new reference-model stream, the four
  cache scenarios, `RV32ITopologyTest` (buffered D-ID `sort`: 4,765 cycles
  against 6,481, verified).

## 2026-10-19

### Visualizer: unified memory view and a shared-memory RV32I scenario (branch `realistic-timing`)

- **Changed:** a memory shared by instruction fetches and data accesses is
  drawn as two panels (instructions disassembled, data words), each around
  its latest access, telling the two apart by the arbiter's grant. New
  scenario `rv32i-shared-memory` (program 21, advanced core, two caches, one
  memory behind the arbiter) with its own floorplan. The structure hash of
  cache and predictor layout keys now includes child types; layouts migrated.
- **Why:** the owner asked for a two-panel view of unified memory.
- **Verified:** ten focused checks (layouts in all 341 scenarios, catalog,
  inspector parity including the new unified check, the scenario itself).
  Browser checks pending.

### Shared memory: refuse overlapping instruction and data images (branch `realistic-timing`)

- **Changed:** a topology run (`--topology`, every arrangement, since A loads
  the whole image into both DRAMs) and the program root's unified memory now
  stop when an instruction and a data block put different bytes at one
  address. Program 19 (code to 0x108, data from 0x100) and `load-use` /
  `load-scheduled` at 340–341 elements (code reaching the input at 0x1000;
  larger sizes already fail on branch range) are refused; before, the merged
  image silently held a different program.
- **Why:** the owner asked where I and D sit in one memory; code starts at 0
  and data at its fixed addresses, with nothing checking for collisions. No
  published result is affected (the cache campaign used `calls`, `sort`,
  `stream`, `pointer-chase`; fixed programs ran with separate memories).
- **Verified:** `RV32ITopologyTest` with a new refusal case (A and B).

### Two-port memory arbiter: catalog, scenario, structure and page 3.5.3 (branch `realistic-timing`)

- **Changed:** `TwoPortMemoryArbiter<W>` gets catalog contracts
  `memory.arbiter.<W>`, a recorded scenario (`memory-arbiter`), a floorplan,
  a figure and a component section in 3.5. Its flat gate list is grouped into
  `GRANT`, `STATE` and `RESPONSE_ROUTE` with unchanged gates. The separate C++
  test program became the registry test `TwoPortMemoryArbiterTest`, now also
  at width 16.
- **Why:** the owner asked to document the arbiter in 3.5; it had no page,
  scenario or contract.
- **Verified:** 28 CTest checks (documentation, layout, catalog, both arbiter
  tests, `RV32ITopologyTest`), the scenario in all three profiles, the page's
  C++ reference against all 53 recorded values, strict MkDocs with preview
  rows. WebAssembly rows and browser checks pending in the memory-gated run.

### Live interaction and real-time execution design

- **Changed:** extended the [device/game record](records/2026-10-19-device-io-games-plan.md)
  with a persistent session, queued input, current-state observation,
  cooperative execution and wall-clock pacing proposal. The proposed order
  now proves live operation before MMIO/display integration.
- **Why:** the owner asked how to move from recorded playback to real
  interaction. Source inspection confirms forward execution already exists;
  it also identifies past-event acceptance, partial-timestamp yields and
  wasm32 `size_t` time range as work for a live runtime.
- **Reviewed:** simulator, component/clock/memory state, viewer/worker and
  WASM build metadata; official Emscripten and ns-3 references. The 4.29 ms
  range at 1 ps is inferred from 32-bit time, not a reproduced failure.
  No code, test, benchmark or browser run was performed.

### Device I/O and games planning resumed

- **Changed:** added a focused [planning record](records/2026-10-19-device-io-games-plan.md)
  and linked it from status and the existing future-steps record. It records
  the current shared-memory/cache foundations, missing device/live-runtime
  contracts and proposed stages toward guest-executed Pong and Tetris.
- **Why:** the owner considers the primary cache implementation ready and
  wants to discuss the next milestone. Architecture, first game, ISA choice
  and performance targets remain proposals or open decisions.
- **Reviewed:** current source, test sources, public references and earlier
  records; official RISC-V ordering and arithmetic-extension specifications.
  No build, tests, benchmarks or live play check was run. Existing source
  and visualizer changes were preserved.

### Cache experiments page; RV32I results rewritten (branch `realistic-timing`)

- **Changed:** new § 3.8 Cache Experiments in the memory chapter: the cache
  campaign presented as a memory-system study (access patterns with no
  processor first, then programs over five arrangements), with two new charts
  drawn from the page's tables and the arrangements figure moved there. § 4.7
  rewritten: short answer, how to read (targets, cycles and time, checks), one
  summary table, then each workload group as what it tests, its table and
  what it shows; the cache section is gone from it. Tables unchanged cell for
  cell; old anchors kept.
- **Why:** the owner asked for a cache experiment section without RV32I and
  a complete, easier rewrite of the RV32I experiment page.
- **Verified:** 18 documentation checks, the plot script's validation of 602
  table cells, figure check, strict MkDocs build, screenshots of both pages and
  charts. Browser checks pending in the memory-gated run.

### Visualizer: one saved floorplan per internal structure (branch `realistic-timing`)

- **Changed:** SRAM and FF memory devices get layout keys that include their
  storage bank; caches, predictor tables and entries, the branch predictor and
  memory timers keep their child-name hash key (Python server and C++ session
  alike). SRAM floorplans seed their children's proportions before placing
  them. The layout file's formatter no longer rewrites bracketed text inside
  strings. `layout.json` gains the 19 new keys with regenerated placements;
  the squeezed 8-row SRAM bank is recomputed; 10 unread profile entries are
  removed. `tools/check-static-layouts.py` fails on a key shared by parts
  with different children and on squeezed SRAM floorplans.
- **Why:** the owner found the scalable cache's `TAG_STORE_1` drawn broken;
  all four of its SRAM stores were squeezed into a 2 % strip, and six more
  keys were shared by different structures.
- **Verified:** 13 native layout and visualizer checks, including
  `VisualizerStaticLayoutTest` (0 problems in 339 scenarios) and C++/Python
  key agreement. WebAssembly rebuild and browser checks pending; see
  [the record](records/2026-10-19-shared-layout-keys.md).

### Component timing: only where it says something, inside the implementation (branch `realistic-timing`)

- **Changed:** 67 stock per-component Timing sections removed (a fixed
  sentence plus, at most, the full path table). The 17 components whose
  timing has something of its own to show (adders and ALUs, flip-flop,
  register, register file, memory devices, controller, endpoint,
  single-cycle core) end their implementation with a **Timing** paragraph:
  what sets the delay, and a generated table of only the paths that matter
  in every profile, through the new `<!-- timing-rows -->` marker. The
  paragraph ends "Behavioral implementation" (structural for `FullAdder`
  and `DFlipFlop`, which have no compact form), so component pages have five
  headings again. The paragraphs state facts inline instead of linking to
  § 1, § 3.6 or the 4.2 certificates; DRAM and NOR show their latency rows
  and the core its certified clocks directly. The template descriptions on
  § 1, the ALU8 conventions, 4.3 and 3.1 follow.
- **Why:** the owner found the sections disappointing and asked to remove
  those with nothing to say, not to send readers to the timing chapters,
  and suggested merging timing into the behavioral implementation.
- **Verified:** all 18 CTest `documentation` checks, including the updated
  heading checks and `TimingTableDocumentationTest` with the new marker and
  a guard against `### Timing` headings and timing-chapter links. The Pages
  rebuild and the browser checks run in a memory-gated run; see
  [the record](records/2026-10-18-memory-docs-rewrite.md).

### Timing across the site: clock certificates, results per profile, figures and plots (branch `realistic-timing`, documentation phase)

- **Changed:** 4.2 gains per-profile certified clocks (generated) and the
  cached-root and clock-wire rules; 4.7 states its results in cycles with
  per-profile summaries from the current certificates (advanced two-bit
  now leads 13/14 nontrap fixed programs); the timed-memory rows of the old
  memory design are replaced by a pointer to 3.6; the visualizer page
  describes the Timing selector; the home page gets an overview. Seven new
  figures in the site's figure format and colors (gate delays per profile,
  the SRAM access chain, DRAM reads by row state, read latency by
  technology, the transaction handshake, the SRAM device, the ALU8
  datapath), charts drawn from the generated timing data; the four § 4.7
  plots regenerated at the `embedded` clocks in the site palette. A
  diagram-design profile (`circuitsim`) holds the site tokens; the project
  marker `.diagram-design` selects it. Browser and parity tests follow the
  new memory URLs.
- **Why:** the owner asked for timing-aware pages, a site-wide figure
  review with diagram-design in the site's design, and no hand-typed
  timing numbers.
- **Verified:** strict MkDocs build; `DocumentationDiagramsTest`,
  `RV32IBenchmarkDocumentationTest`, `MemoryDeviceDocumentationTest`,
  `TimingTableDocumentationTest`, `DocumentationTimingDataTest`; the plot
  script validated all 602 cycle cells against the campaign data; figures
  inspected in light and dark; `tools/build-pages.sh` (before the caches
  section). Browser checks: 4 of 19 passed before the run was stopped for low
  host memory; the other 15 are not run.
- **Also:** § 4.7 publishes the cache campaign (five memory topologies,
  four workloads, both realistic profiles, standalone locality traces),
  tables generated from the run records, with a topology figure.

### Memory chapter rewritten; timing sections on every component page (branch `realistic-timing`, documentation phase)

- **Changed:** `site-docs/memory/` restructured and rewritten against the
  current code: 3.1 Memory Architecture, 3.2 Shared Building Blocks (with the
  timing and control parts), 3.3 Memory Technologies (3.3.1–3.3.6 as
  subsections), 3.4 Transaction Protocol (READ_BURST, registered
  `REQ_READY`, `LAST`/`SPACE`), 3.5 Controllers and Endpoints (gate-level
  sequencers, current configuration), 3.6 Memory Timing (new), 3.7 Caches.
  New top-level "§ 1. Time and Delay". The memory, ALU8 and RV32I 4.3–4.4
  component pages split "Unknown inputs and timing" into "Unknown inputs"
  and a "Timing" section with generated delay, interval and clock tables;
  the abstract-tick disclaimers are gone. Long controller and endpoint C++
  listings are replaced by a request check and an endpoint response
  function. Links from the RV32I pages follow the new addresses.
- **Why:** the owner asked for a complete memory rewrite after testing, with
  the technologies under one section, and for the site to state timing from
  the code (profiles, proven tables, measured latency).
- **Verified:** strict MkDocs build; `MemoryDeviceDocumentationTest`,
  `MemoryEndpointIntegrationTest` (the documented request check equals the
  production check in every case), `DocumentationExamplesTest`,
  `RV32IBuildingBlocksDocumentationTest`, `RV32ISingleCycleDocumentationTest`,
  `RegisterFileDocumentationTest`, `RegisterDocumentationTest`,
  `CacheDocumentationTest`, `DocumentationDiagramsTest`,
  `TimingTableDocumentationTest`.
- **Not yet:** browser tests that open the old memory URLs, the diagram
  review, 4.2/4.7/visualizer pages, the Pages build.

### Documentation timing data (branch `realistic-timing`, phase 8)

- **Changed:** new tool `documentation_timing_data` writes
  `tools/timing/documentation-timing.generated.json` (leaf delays, measured
  flip-flop and SRAM-cell contracts, certified periods, memory intervals,
  access latency of each technology at the advanced core's clock); new
  markers `profile-table` and `latency-table` in `tools/mkdocs_timing.py`.
- **Why:** no timing number on the site is typed by hand.
- **Verified:** `DocumentationTimingDataTest` (measured again, equal) and
  `MemoryLatencyGatesTest` (compact and gate-level endpoints take the same
  cycles in all three profiles, 38 of 38). Latencies equal the phase-6
  records.

### Static visualizer: complete layouts, compact Timing control, layout check (branch `realistic-timing`, phase 8)

- **Changed:** `layout.json` gains the 13 child placements the static site
  lacked (FF controller RUNNING/READY/NOTHING_HELD, a memory timer stage,
  the two-level cache's L1 variant); the compact view places the Timing
  selector beside the scenario; new `tools/check-static-layouts.py`
  (CTest `VisualizerStaticLayoutTest`) replays the browser's layout lookup
  for every scenario; new browser check `VisualizerTimingProfileBrowserTest`.
- **Why:** the endpoint scenarios stopped in the WebAssembly site on a
  missing placement, and the compact view hid the scenario selector.
- **Verified:** the Pages build (WebAssembly, example rows, strict MkDocs);
  browser checks `VisualizerWasmBrowserTest`,
  `VisualizerTimingProfileBrowserTest`, `MemoryEndpointBrowserTest`; the
  layout check over 339 scenarios (0 missing).

### Visualizer: choose the timing profile (branch `realistic-timing`, phase 8)

- **Changed:** a Timing selector (`unit`, `embedded`, `desktop`) beside the
  scenario, kept in the URL as `?timing=`; the circuit is built and
  simulated under it and times are shown in ps, ns, µs or ms when the
  profile's tick has a length. Native server: sessions per scenario and
  profile, the library's profile set under one lock while a session is
  built and restored afterwards; `timing` accepted by the circuit, state
  and apply requests; the topology response names the profile. Python
  binding `timing_profile_info`; WebAssembly `setTimingProfile` and
  `timingProfileJson`.
- **Why:** the realistic profiles were reachable only from the command line.
- **Verified:** `VisualizerTimingProfileTest` (new),
  `VisualizerReferenceProfileTest` (extended), the visualizer CTest group
  (only the known `site-docs/` check fails).
- **Limits:** the WebAssembly build and browser checks were not yet run
  when this was committed.

## 2026-10-18

### Program fixtures and fixed-time scenarios under every timing profile (branch `realistic-timing`, phase 8)

- **Changed:** `analysis::CERTIFIED_CORE_PERIODS` stores the certified FF
  clock of each core per profile (`rv32i_experiment
  --list-certified-periods`). The RV32I program fixtures keep their `unit`
  times and multiply them, under another profile, by the smallest whole
  factor that reaches the certified clock (`rv32iFixtureTimeScale`; the
  baseline fixture on its half cycle, since it releases reset half a cycle
  before its first edge). `FullCircuitTest` and `CacheTwoLevelScenarioTest`
  scale their times by gate delays.
- **Why:** the visualizer is to offer the realistic profiles; every program
  scenario failed under `embedded` because its times were `unit` ticks.
- **Verified:** the 179 program and fixed-time scenarios under `embedded`
  and `unit` (all pass; under `embedded` 1 passed before); baseline
  scenarios under `desktop`; `RV32IProfileSweepTest` with the new check
  that the stored periods equal the analysis.
- **Limits:** the full native suite was not rerun for this change yet (it
  runs once more at the end of phase 8).

### Clock certified for cached and shared roots (branch `realistic-timing`, phase 8)

- **Changed:** the two-port arbiter is clocked by I_CLK instead of an AND
  of both port clocks (D_CLK must carry the same clock; pins unchanged).
  The safe-clock analyzer treats a zero-delay one-bit Rewire fed by the
  clock as part of the clock net and states it as an assumption.
- **Why:** the analyzer refused every cached or arbitrated root. The
  arbiter's gate put one gate delay of skew on its clock; the caches hand
  their clock to the banks through zero-delay wires, which the delay-table
  analyzer already accepted.
- **Verified:** `--safe-clock` on A, B, C-ID, D-0 and D-ID in `embedded`
  and `desktop`: all safe at the core's clock (4104 / 2858 ps), no
  diagnostics; `--period auto` runs C-ID and D-ID verified with 0 setup
  violations; B and D-ID `sort` reruns match the campaign exactly. Tests:
  SafeClockTimingTest (new clock-wire case), RV32ITopologyTest (new
  certified-clock case), MemoryEndpointSafeClockTest, RV32IExperimentTest,
  RV32IExperimentCLITest, RV32IProfileSweepTest, TwoPortMemoryArbiterTest,
  CacheReadTest.
- **Limits:** the period is the core's (its port setups dominate); the
  caches' own shortest safe clock is not computed. Endpoints behind caches
  are analysed with all arcs, without handshake-state cuts.

### Visualizer data for the cache and memory timing parts (branch `realistic-timing`, phase 8)

- **Changed:** `layout.json` drops entries of removed parts and stale cache
  variants, rebuilds the three cache scenario floorplans for the direct
  banks, and adds default layouts for new parts (memory timers, sequencers,
  request checks, cache bank controller). `palette.json` colors the cache
  family and the memory timing parts.
- **Why:** three visualizer tests failed on layouts saved before phase 7a
  and on new types without colors.
- **Verified:** CTest label `visualizer`, 28 / 29 (the failure is the
  unchanged `site-docs/` checks of MemoryEndpointIntegrationTest).
- **Limits:** a full automatic refresh would also move saved children of
  nine entries this branch did not touch; they were left unchanged. Browser
  and WebAssembly checks were not run.

### Delay twin: a flip-flop stuck at X is timed from every edge (branch `realistic-timing`, phase 8)

- **Changed:** `DelayTwin.cpp` renews a flip-flop's launch time at every
  clock edge (and unknown reset) while its value is X, not only when the
  held value changes.
- **Why:** the strict-settling delay rule failed on five memory
  controllers. Traced on `memory.rom.controller.32`: an earlier situation
  left the twin's state X while the gates resolved; X looked unchanging, so
  paths from DONE and LAST counted as quiet and REQ_READY settled one tick
  earlier in the compact part than in the gates.
- **Verified:** DelayRuleTest `--strict-settling` on the five controllers in
  `unit`, `embedded` and `desktop` (0 breaks, 0 target misses); full suite
  under `unit`, 461 / 467 (the six failures are the known documentation
  and visualizer-data checks).

### Test timing audit: realistic profiles covered by their own tests (branch `realistic-timing`, phase 8)

- **Changed:** new `RV32IProfileSweepTest` runs every core on programs 1,
  9, 16, 19 and 20 under `embedded` and `desktop` at the analyzer's
  certified clock (single-cycle and baseline with FF memory; advanced with
  FF, on-chip SRAM and DRAM), oracle-verified, no setup violation. The tests
  guide states that the native suite is a `unit` suite.
- **Why:** under `embedded` 217 of 466 tests fail because they are written
  in `unit` ticks (exact tick counts, or clocks and sampling points in
  ticks); rewriting them would duplicate the phase 6.5 core sweep. A
  harness-only scaling attempt fixed none and was withdrawn.
- **Verified:** full suite under `unit` (459 / 466; the failures are six
  documentation or visualizer-data checks and the strict-settling delay
  rule, recorded as an open delay-twin defect); full suite under `embedded`
  (249 / 466, recorded as the evidence); `RV32IProfileSweepTest` (50 runs,
  113 s).

### DRAM command counters in the experiments (branch `realistic-timing`, phase 8)

- **Changed:** the compact DRAM control counts the commands it accepts;
  `BehavioralMemoryEndpoint::commandCounts()` reports them and
  `rv32i_experiment` adds `DEVICE_<memory>_<command>` per measured window.
  Benchmark-record units for observations are now requests, words,
  commands or cycles (all were cycles).
- **Why:** the cache campaign's shared-memory slowdown was explained by DRAM
  row changes without measuring them. Measured: B's shared DRAM opens rows
  23× as often as A's two memories in `sort`.
- **Verified:** the A and B `sort` runs (cycle counts unchanged),
  `RV32IExperimentTest`, `RV32IExperimentCLITest`, `RV32ITopologyTest`,
  `MemoryEndpointDRAMTest`, `NativeMemoryDeviceContractTest`. The standalone
  cache benchmark with gate-level banks matched the compact banks exactly
  after the SRAM macro fix.

## 2026-10-17

### Clocked SRAM macro: no write disturb on the next access (branch `realistic-timing`, phase 8)

- **Changed:** the gate-level clocked SRAM macro's settle line (how long its
  registered address and data are held back when an access starts) now
  covers the row decoder's enable path and the write drivers' turn-off
  (`circuit::sram::clockedSettle`); the compact macro's withdrawal and the
  fallback access times use the same value. Tables regenerated.
- **Why:** a finished write left its drivers and word line on; at the next
  access the new address and data reached the array 50 ps before the old
  word line closed (`embedded`), writing the new word into the old row
  (writing word 0 changed word 4, and so on). Found as stale tags in a cache
  bank; compact banks were not affected. A first fix that ended the write
  drive at READY was withdrawn: it added a 0.7-ns false path to the proven
  read-data launch.
- **Verified:** a write-after-write disturbance test (0 pairs in all three
  profiles; 4 at `embedded` before), the SRAM device tests, tables at a fixed
  point and recomputed identically, DelayRuleTest on the SRAM parts in all
  three profiles, the on-chip-SRAM CPU panel (unchanged), the CLI test, the
  standalone cache benchmark (identical to the campaign run).
- **Limits:** every clocked SRAM access starts about 310 ps later at
  `embedded` (a real decoder-reset delay); certified CPU clocks and cycle
  counts did not change. Cached roots now fail below 3300 / 2300 ps
  (`embedded` / `desktop`), leaving 24 % margin at the campaign clocks.

### Cache experiment runs and locality traces (branch `realistic-timing`, phase 7d)

- **Changed:** `CachePerformanceBenchmark` gained `stride-line` and
  `stride-row` traces (the same 2048 reads with one word per line, or per
  DRAM row) and builds its parts with the campaign profile (compact banks,
  as the CPU runs); diagnostic options run one trace/path or chosen
  addresses. No model or timing change.
- **Why:** the cache plan's Step 2 and Step 3, and the owner's question
  whether locality-friendly code is faster.
- **Verified (results, not tests):** standalone traces in `embedded` and
  `desktop`, every read and the final DRAM image checked; the five-topology
  CPU panel (calls, sort, stream) and pointer-chase in both profiles, 40
  runs, all verified against the oracle. Key figures and the counter-based
  explanations are in the realistic-timing progress log.
- **Limits:** exploratory: cached roots are not certified (run at the core's
  clock with a measured margin); the campaign used compact banks while the
  gate-level macro has an open defect inside the cache; DRAM row changes and
  refreshes are not counted in the CPU runs, so the shared-memory slowdown's
  row explanation is an inference. Results are not published.

### Cache experiment: topologies, standalone benchmark, arbiter fixes (branch `realistic-timing`, phase 7c)

- **Changed:** `rv32i_experiment --topology A|B|C-ID|D-0|D-ID` builds the
  campaign's five memory systems (two memories; one memory behind the
  two-port arbiter; L1s, a shared L2, or both) with the frozen geometry
  (`campaignCache`: 8 or 64 sets, two ways, 16-byte lines, SRAM banks, burst
  fills), whole-image loading and boundary counters. `TwoPortMemoryArbiter`
  takes a burst length and keeps a granted burst to its last word, and has
  a one-entry response buffer per port: a response leaves the bus into its
  owner's buffer, so a core stalled on its other port no longer deadlocks
  the shared bus. New `CachePerformanceBenchmark` (S0–S2 standalone traces,
  in the memory benchmark runner). A clocked SRAM macro no longer gets a
  delay twin (it changed nothing but cost a cached CPU 3–40× run time). The
  generated benchmarks accept up to the data layout's 1024 words.
- **Why:** the cache plan's Step 2 and Step 3 infrastructure; the arbiter
  defects were found by the new topology test.
- **Verified:** Step 1 in both profiles (L1 hit 2, L2 hit 8, DRAM 8–22
  cycles); `RV32ITopologyTest` (new: all five topologies verify a sort, plus
  a program through D-ID over SRAM), `TwoPortMemoryArbiterTest` (rewritten,
  burst cases), cache scenario tests, `CacheReadTest`,
  `RV32IMemoryMigrationTest`, `RV32IExperimentCLITest`, the benchmark tests;
  DelayRuleTest on the SRAM parts without the twin in all three profiles;
  the standalone benchmark in `embedded` and `desktop`, every read verified.
- **Limits:** cached and arbitrated roots are not certified by the analyzer
  (no all-state contract for an endpoint behind them); they run at the
  core's certified clock, measured to be at least 37 % (`embedded`) and 43 %
  (`desktop`) above their failure point, and below that point they fail
  silently (the cache's state bits have no setup check). Open defect: the
  gate-level clocked SRAM macro corrupts a row neighbour when driven by the
  cache; compact banks (the campaign profile) are correct. The CPU panel is
  not yet run.

### Burst reads and burst-filled cache lines (branch `realistic-timing`, phase 7b)

- **Changed:** a burst read (op 4, `Operation::ReadBurst`) returns the
  `block_words` words of the aligned block from the requested word,
  wrapping, one response per word. The transaction port reports and uses
  LAST (a burst holds the port until its last word) and SPACE; the static
  sequencer steps a word address within the block (the clocked macro is
  given the next word as one is taken); the DRAM command control issues one
  READ per word in the open row (`block_words` ≤ `words_per_row`). The cache
  accepts op 4 (a hit streams the line from its data bank, a miss forwards
  the fill), has `fill=words|burst` (one op-4 request per miss), and fills a
  line from the requested word in both modes; its response still waits for
  the whole line. `block_words` = 4 has its own proven delay tables
  (`@block=4`). The safe-clock handshake model takes the multi-word states
  only for requesters that can ask for bursts. Fixed on the way: the
  transaction-port scenario's undriven LAST, the cache's top-level helper
  names (the floorplan rejected them since 7a), and MemoryEndpointTimingTest's
  pre-6.4a expectation that a request is taken over a held response.
- **Why:** word-by-word fills made an L1 miss cost about five DRAM reads (38
  cycles against an 8-cycle row hit), a protocol defect against caches. With
  bursts the cache plan's Step 1 order holds at `embedded`: L1 hit 2, L2 hit
  8, L1 miss 14 (DRAM open row 8), L1+L2 miss 17 cycles. No memory interval
  was changed.
- **Verified:** tables regenerated to a fixed point (754 entries per
  profile) and recomputed identically in all three profiles; DelayRuleTest
  on 14 memory contracts with their variants (24 parts) in `unit`,
  `embedded` and `desktop`: no rule break, no target missed; a 42-test
  CTest subset at `unit` (tables, timing, safe clock, memory endpoints,
  controllers, devices, contracts, caches); the CPU panel (66 runs, all
  verified): unchanged at `embedded` and `desktop`.
- **Limits:** the advanced core's `unit` safe clock with clocked SRAM rose
  158 → 174 ps: the burst gates on the macro's request-to-CE_N path,
  extrapolated to 1024 subarrays (recorded in the progress log); `embedded`
  and `desktop` clocks did not change. Documentation checks
  (`MemoryDeviceDocumentationTest`, three `MemoryEndpointIntegrationTest`
  checks) and CacheLayoutTest's saved layouts fail until phase 8. NOR flash
  received unused `@block=4` tables (it rejects bursts); they go at the next
  regeneration. The full suite was not run.

### Cache with directly driven banks (branch `realistic-timing`, phase 7a)

- **Changed:** `Cache<W>` looks a request up in every way at once in one
  cycle: per way a tag bank and a data bank are native devices driven
  directly (`sram` = the clocked macro, `ff` = the flip-flop device), the
  hit way's word is taken into the result register at the next edge, and the
  response is used one edge later (a two-cycle hit). A read miss writes each
  fill word into the victim's data bank as it arrives and takes the requested
  word as it passes; writes stay write-through, no-allocate. The serial
  endpoint-store phase machine is replaced (`CacheLookupPhases`,
  `CacheBankController`); bank names `TAG_STORE_k`/`DATA_STORE_k` are kept.
- **Why:** the old controller made every phase a whole endpoint transaction
  (a 14-cycle L1 hit); the timing plan's phase 7 rebuilds the hit path.
- **Verified:** cache component, direct-mapped, scalable and two-level
  scenarios, CacheReadTest and RV32IMemoryMigrationTest at `unit`; a latency
  probe at `embedded` (L1 hit 2 cycles).
- **Limits:** line fills still move word by word (L1 miss 38 cycles against
  an 8-cycle DRAM read); burst reads follow (phase 7b) before the cache
  experiment. Public cache pages and examples are not yet updated (phase 8).

### Endpoint handshake from registered state, one DRAM edge per command, core sweep (branch `realistic-timing`, phase 6.4–6.5)

- **Changed:** every memory endpoint (gate-level transaction port and FF
  controller, compact controller, fused endpoint) takes a new request from
  registered state only: REQ_READY = !RST & (!PENDING | COMPLETION) &
  BACK_READY & !FULL, no longer following the same cycle's RSP_READY. A held
  response and one request in flight can coexist. The clocked SRAM macro
  has no logic before its registers: subarrays get CE_N and WE_N ungated and
  their address with the select bits inverted, and the bus enables are
  decoded after the edge (setup at 512 words 930 → 408 ps). The DRAM device
  shows a completion as soon as its sampler sees it and takes the next
  command at that edge (one synchronizer edge per command instead of two).
  The analysis's handshake models (built-in FF endpoint, declared FF
  contract, safe-clock state enumeration with DRAM refresh phases) follow.
- **Why:** the old handshake let the core's stall logic reach the memory's
  request inputs in one cycle (clocked SRAM certified at 5,236 ps against
  4,104 now); DRAM was 38–52 % over a real controller's latency.
- **Verified:** tables at a fixed point and verified in all profiles; delay
  rule (default) 0 breaks on 53 memory parts in all profiles; CTest subset
  (endpoint safe clock, experiment and CLI, examples, arbiter, migration,
  memory state and feedback timing) passes; gate-level handshake-state checks
  pass for FF, SRAM and DRAM; 66-run panel; core sweep of 306 verified runs.
- **Limits:** DRAM still +23–33 % over a counted controller; macro 3.1×
  CACTI; STM32F7 check not done; no full suite.

### Datasheet DRAM and NOR timing and a clocked on-chip SRAM macro (branch `realistic-timing`, phase 6.3)

- **Changed (6.3a):** the timing profiles hold datasheet intervals for
  external DRAM (`embedded` SDR SDRAM -6A, `desktop` DDR4-3200: tRCD, CL, tWR,
  tRP, tRFC, tREFI, page size) and NOR flash (tACC 120 ns, program 150 µs,
  erase 410 ms). DRAM REFRESH can include the full restore; DRAM rows up to
  1,024 words are allowed for compact devices (gate-level stays at 16); the
  RV32I experiment takes DRAM row size and refresh rate from the profile.
  Functional flash endpoint scenarios set short program and erase pulses.
- **Changed (6.3b):** SRAM devices, controllers and endpoints accept
  `clocked=1`: a synchronous macro with input registers, an access token in
  place of the address-transition detector, and the same self-timed stages.
  Large arrays drive a shared return bus (`MemoryBus` leaf, READY pulled
  low) instead of a multiplexer tree; in a clocked array every subarray takes
  each access and only WE_N is gated by the select, and the clock is ideal
  (the global wire counts in the setup). The sense replica now covers the
  wordline path and the column multiplexer, so READY follows the data (also
  for the asynchronous SRAM); in clocked mode precharge covers a fully swung
  bitline. `--memory sram` is now the clocked macro and `--memory sram-async`
  the external chip. Delay tables gain `@clocked` entries (649 per profile).
- **Fixed on the way:** the timing graph marked a one-bit write mask as a
  clock carrier ("clock used as data"); `MemoryOutputBuffer` rejected one-bit
  status lines; `DelayRuleTest` stepped one tick at a time (it now steps from
  event to event, about 100× faster with picosecond ticks) and skipped the
  DRAM/NOR endpoints as restless (it now follows a part until it goes quiet).
- **Why:** owner rule that memories be realistic, not cache-favouring; the
  clocked macro is the bank the cache phase will use.
- **Verified:** tables at a fixed point and `--verify` identical in all three
  profiles; delay rule in the CTest configuration on DRAM/NOR/SRAM/ROM plus the
  clocked variants, 0 breaks in all three profiles; SRAM, endpoint, controller,
  native-device and cache tests at `unit` and the SRAM/endpoint set at
  `embedded`; a probe of 400 random accesses per size shows READY after the
  data in every read; 66 oracle-verified CPU runs.
- **Limits:** the clocked macro is 3.1× CACTI at 512 B (reported, not tuned);
  its integration makes the core–endpoint handshake longer (certified clock
  `embedded` 5,236 ps against 4,608 ps), to be decoupled in 6.4; a 30-round
  delay-rule sample reaches a known gap in the delay twin's X timing; no full
  suite; CacheReadTest and the other tick-driven tests stay `unit`-only.

## 2026-10-16

### Memory controller protocols closer to real controllers (branch `realistic-timing`, phase 6.1–6.2)

- **Changed:** DRAM and NOR devices take the next command during the
  releasing cycle (BUSY shown low there), with FRESH and REPEAT state so a
  stale sample or a repeated command cannot complete early; the command
  sequencer shows the next command combinationally when the device is idle
  and fault-free. SRAM/ROM controllers count `access_cycles` (new endpoint
  parameter, chosen from the proven access time at the clock by
  `staticAccessCycles`) instead of synchronizing READY; READY is checked at
  the capturing edge (status Device if not high).
- **Why:** owner rule that memories be realistic, not cache-favouring: the
  removed cycles (about three per DRAM/NOR command, two per SRAM/ROM
  access) are not spent by real controllers.
- **Verified:** DRAM/NOR/SRAM/ROM device, controller, endpoint and cache
  scenarios (with a new back-to-back command case); tables at a fixed point
  in all profiles; delay rule under `unit` and `embedded`; 54
  oracle-verified CPU runs at re-certified clocks.
- **Limits:** realistic memory intervals still placeholders (6.3); no full
  suite.

### On-chip SRAM geometry and READY after data (branch `realistic-timing`, phase 5)

- **Changed:** SRAM and ROM devices raise READY only after the read data are
  valid (an extra timer stage copying the sense and output path). A gate-level
  SRAM above 32 rows is built from 32-row subarrays with a global decoder,
  global wires (new `MemoryWire` leaf, `sram_wire_per_side` in the profile)
  and a return multiplexer and output buffer; the contract is unchanged. SRAM
  tables are proven up to 512 words; larger SRAMs use `sramSubarrayEstimate`
  (per-level growth fitted at 2 and 4 subarrays, wire term from the profile).
  The compact SRAM withdraws its data no sooner than the gates' path for that
  transition. New tests: `SRAMSubarrayTest`, `SRAMSubarrayFormulaTest`.
- **Why:** realistic on-chip arrays are divided into short-bitline subarrays;
  cache banks driven directly need READY after data; the delay rule under the
  realistic profile found the early withdrawal.
- **Verified:** held-out 8-subarray tables matched exactly in all three
  profiles; SRAM/ROM/endpoint/cache scenarios under `unit` and `embedded`;
  delay rule for SRAM/ROM parts under all profiles; tables verified.
- **Limits:** `SRAMTimingTest` remains a `unit` test (tick-driven); realistic
  memory intervals are still placeholders; SRAM CPU cycles rose with
  READY-after-data; no full suite.

### Memory timing under realistic profiles (branch `realistic-timing`, phase 4.4)

- **Changed:** the SRAM/ROM timer's settle line is sized from its own
  change-detection path (`memoryTimerSettle`); DRAM sense and precharge steps
  wait for a row-decoder replica (`dramDecodeReplica`) in the gates and the
  cycle-exact copy; compact and fused DRAM models keep storage as the row's
  logical contents while a row is open; DRAM retention defaults are timing
  profile values (64 ms in the realistic profiles, `unit` unchanged) with the
  limit widened to 10^12 ticks; a compact part whose gate list contradicts a
  change is timed by its table; delay tables regenerated for all profiles.
- **Why:** the realistic profiles exposed races hidden by one-tick gates
  (lost change pulse, sensing before the wordline, precharge before the
  wordline close) and a 100 ns retention left over from tick-era defaults.
- **Verified:** tables at a fixed point and `--verify` identical for all three
  profiles; 18 memory scenarios each under `embedded` and `desktop` and the
  affected `unit` scenarios pass; full `DelayRuleTest` passes; 54
  oracle-verified CPU runs (five memories, three profiles).
- **Limits:** realistic memory intervals are placeholders until phase 6;
  tick-driven timing tests remain `unit` tests; no full suite.

### Gate-level memory control and self-timed memory devices (branch `realistic-timing`, phase 4.1–4.3)

- **Changed:** the memory controllers' sequencing is now gates. SRAM and ROM
  use `StaticMemorySequencer` (request check, registers, READY through a
  two-stage synchronizer); DRAM and NOR flash use `CommandMemorySequencer`
  (pending command list, one-cycle ISSUE, wait for the device's registered
  BUSY, open-row tracking, a cycle-counted refresh). The C++
  `NativeSequencer` primitive is removed. The devices time themselves: SRAM
  and ROM raise READY from replica timing (`MemoryTimer`, delay lines); DRAM
  and NOR command controls are synchronous gates with analog delay-line
  chains and a sampler. New timing leaves: `MemoryDelayLine`,
  `SymbolicDiffer`, `Synchronizer`, `ProtocolStableData`, `KnownBits`,
  `PowerOnReset`. Compact controllers, compact devices and the fused endpoint
  are cycle-exact copies (`NativeSequence`, `DRAMSyncControl`,
  `NORSyncControl`) timed by tables proven on the gates. DRAM refresh is
  configured in cycles (`refresh_interval_cycles`, default 96; 0 or at least
  24). The device's open row is visible as ROW_OPEN.
- **Why:** decision 2A of the realistic-timing plan (devices time themselves
  and signal completion) and the lower-level-first rule; tick timers in C++
  could not follow realistic gate delays.
- **Verified (unit profile):** focused memory scenarios (device, contract,
  timing, controller, all endpoints, scalable endpoint, cache scenarios) pass
  with every fidelity compared; `DelayRuleTest` on SRAM/ROM/DRAM/NOR devices,
  controllers and endpoints: no rule break; oracle-verified CPU runs, e.g.
  program 21 at 696 (SRAM), 1,375 (DRAM) and 732 (NOR) cycles; safe clock
  102 (SRAM/ROM), 106 (DRAM), 104 (NOR) ticks. Details in the progress record.
- **Limits:** cycle counts with timed memories grew (synchronizer and
  clocked command protocol); published results remain reproducible at tag
  `timing-legacy`. Realistic-profile tables for the memory parts are stale
  until step 4.4. `MemoryEndpointIntegrationTest` fails its two C++ example
  checks because they compile reference models from `site-docs/` snippets
  that still describe the old sequencing; the public site is updated in
  phase 8 by owner request. No full suite.

### Timing profiles, inertial gate delay and realistic gate delays (branch `realistic-timing`)

- **Changed:** leaf parts (logic gates, splitter, memory primitives) read
  their delays from a process-wide timing profile
  (`include/components/TimingProfile.hpp`), selected by
  `CIRCUITSIM_TIMING_PROFILE`, the Python `set_timing_profile` or code.
  Profiles: `unit` (default; unchanged timing), `embedded` (65 nm, FO4 30 ps)
  and `desktop` (32 nm, FO4 21 ps) with 1 tick = 1 ps and sourced gate ratios.
  Logic gates use inertial delay (pulses shorter than the delay vanish).
  Flip-flop and SRAM-cell contracts are measured per profile and stored in
  it. Delay tables are generated per profile, each in its own source file;
  `part_delay_tables --profile` and two CTest verify entries were added.
- **Why:** the owner decided to give leaf parts realistic delays while keeping
  behavioral parts derived from their gate forms (see the realistic-timing
  plan and progress records).
- **Verified:** under `unit`, `part_delay_tables --verify` identical (601
  entries) and nine CPU runs identical in every field but host time against
  the baseline binary, with and without inertial delay; 23 scenarios passed.
  Under `embedded`/`desktop`: tables verified at a fixed point, flip-flop and
  SRAM-cell tests passed, all three cores certified and ran programs with
  identical cycles and oracle checks in compact and gate-level form;
  `DelayRuleTest` sample passed.
- **Limits:** no full suite. Timed memories (SRAM/DRAM/ROM/NOR) keep
  tick-based timers and are not valid under realistic profiles until the
  gate-level memory control phases. Scenario tests stay on `unit`. The public
  site is unchanged by owner request.

### Retire the redundant memory container and document test workflows

- **Changed:** scalable endpoints now select each technology's behavioral
  native device under the existing device contract while keeping the
  controller visible. Retired container contract IDs map to native devices.
  Updated memory pages, diagrams, visualizer aliases and examples. Added a
  test and scenario workflow guide linked from AGENTS.md.
- **Why:** the container only reselected and relabeled an existing behavioral
  device; removing it leaves one replaceable native-device contract per
  technology. The guide makes registry, CTest, scenario and site checks easier
  to repeat when components change.
- **Checked:** serial native build, 28 focused CTests, 601 delay-table entries
  verified, diagram and JSON checks, full local Pages/WASM package with strict
  MkDocs, and 7/7 focused browser/WASM parity checks passed.
- **Limits:** the full CTest suite and published URL were not checked. These
  memory and guide changes remain uncommitted in the local working tree.

### Trigger Pages deployment from alpha

- **Changed:** the Pages workflow push trigger now listens to `alpha`; the
  workflow-only change was pushed to `origin/alpha`.
- **Why:** `alpha` is the selected source branch, and no `github-pages` source
  branch exists.
- **Checked:** GitHub Actions reported a run in progress for the pushed
  trigger commit. The full local site package and seven focused web checks
  passed after the push.
- **Limits:** the user directed us to treat publication as done while the
  remote run continued; its final result and live URL were not verified. The
  local memory and guide changes were not in that pushed commit.

### Fix one technology stack for the cache experiments

- **Changed:** replaced the broad FF/SRAM/DRAM performance sweeps in the open
  cache record with a complete plan for SRAM L1 and L2 caches over DRAM main
  memory. Fixed the initial geometry, small structural/scalable check, three
  standalone paths, five CPU paths, workloads, counters and completion gates;
  updated status.
- **Why:** one consistent technology stack tests the intended cache hierarchy
  without multiplying the campaign by technologies. DRAM is backing memory;
  the cache's tag/data banks currently support SRAM or FF, not DRAM.
- **Checked:** reviewed cache and endpoint source, CPU root and experiment
  runner, benchmark tooling and current documentation; `git diff --check`
  passed. No benchmark or simulation was run for this planning change.
- **Limits:** SRAM cache-hit timing, shared CPU bypass/L2 wiring, benchmarks
  and physical timing calibration remain open. The plan does not claim speedup.

### Focus the planned CPU cache campaign

- **Changed:** reduced the primary CPU matrix from nine variants to five:
  separate memories, shared uncached memory, both L1s, L2 alone, and both
  L1s over L2. Kept I-only/D-only runs as targeted follow-ups; updated the
  open cache record and status.
- **Why:** the five cases answer the main sharing, L1 and L2 questions while
  avoiding duplicate `C-0`/shared-uncached runs and a broad ablation sweep.
- **Checked:** reviewed the existing topology plan and cache contract;
  `git diff --check` passed. No simulation was run for this planning change.
- **Limits:** the CPU bypass, shared L2 and benchmark campaign remain to be
  implemented.

## 2026-10-15

### Refine the advanced-core cache comparison matrix

- **Changed:** specified nine CPU topologies in the open cache record:
  separate memories, shared uncached memory, I/D L1 combinations, and the
  same combinations over a shared L2. Added comparisons, workload choices,
  counters, correctness gates and implementation order; updated status.
- **Why:** one-level and two-level results need shared-memory controls, and
  instruction/data cache effects need independent switches.
- **Checked:** reviewed current CPU root/runner wiring, cache and arbiter
  contracts, benchmark workload catalogue and memory-wait reference;
  `git diff --check` passed after the documentation edit.
- **Limits:** plan only. The shared uncached bypass, CPU-side L2 path and
  nine-topology campaign have not been implemented or run.

### Specify matched cache experiments

- **Changed:** added an executable experiment protocol to the open cache
  record and updated status. It defines direct-versus-cache, two-level and
  advanced-core pairs, workloads, measured boundaries and result gates.
- **Why:** cache traffic savings, modeled latency, CPU cycles and simulator
  host cost need separate evidence under matched backing-memory conditions.
- **Checked:** inspected the cache and memory contracts, current scenario and
  benchmark runners, CPU topology construction and timing documentation;
  `git diff --check` passed. No performance experiment was run.
- **Limits:** this is a plan. No benchmark fixture, timing model or CPU path
  was changed, and no direct-versus-cached performance run was made.

### Recognize existing memory delay comparisons in the cache plan

- **Changed:** corrected the open cache record and status to account for
  `DelayRuleTest` and its gate-versus-compact memory timing comparisons.
- **Why:** the prior timing review overlooked these tests and made a new
  comparison phase sound necessary before exploratory cache experiments.
- **Checked:** a focused run of five 8-bit endpoint contracts and an FF
  variant completed 3,208 situations, with 970 paired output changes and no
  delay-rule break or unrecorded closeness failure.
- **Limits:** the self-timed DRAM endpoint yielded no paired output changes
  in this focused run; exact transaction latency under every request and
  physical hardware timing remain unverified. No cache speedup was measured.

### Review memory realism before cache performance comparisons

- **Changed:** added a current-source audit and staged comparison design to
  the open cache record; clarified the outstanding timing work in status.
- **Why:** structural/behavioral agreement establishes digital model fidelity,
  while phase choices, controller waits and large-depth estimates still need
  separate physical plausibility checks before interpreting cache speedups.
- **Checked:** eight focused memory contract and timing CTests passed in the
  existing `build-cleanup` binary; inspected cache, native-device, endpoint
  and delay-table source plus the repository textbook and vendor references.
- **Limits:** no code or timing model was changed, the test binary was not
  rebuilt for this review, and no direct-versus-cached latency comparison was
  run. Vendor specifications do not calibrate simulator ticks by themselves.

### Explain cache parameters through four runnable configurations

- **Changed:** rewrote the cache page's configuration and behavior text within
  its component template, preserving the C++ reference. Added direct-mapped
  and scalable single-cache scenarios plus a connected two-level FF-backed
  scenario, with menu aliases, saved layouts and browser checks. Added an
  opt-in precomputed-playback hook so the two-level layout can be built
  without simulating its transactions, and documented that contract in the
  test guide and reference.
- **Why:** the original page offered one visual configuration and did not
  clearly explain word width, sets, ways, line size or backing range.
- **Checked:** eight focused native CTests passed; the strict Pages/WASM build
  generated 30, 116 and 58 verified rows for the single-cache examples; the
  focused memory browser test passed 1/1, including all four cache scenarios.
  `CacheDocumentationTest` still compiles the unchanged C++ reference.
- **Limits:** the first three examples script their lower port. The fourth
  uses one requester and broadcasts invalidation only while both caches are
  idle. The full native and browser suites were not rerun; these scenarios do
  not establish cache speedup or coherence.

### Broaden the cache scenario and verify a two-level cache path

- **Changed:** expanded the visual component scenario to two sets, two ways
  and two-word lines, covering spatial hits, LRU replacement, writes,
  invalidation and range rejection. Added a headless upper-cache/lower-cache
  fixture over FF RAM, including idle-time invalidation of both levels;
  updated the saved layout and public page.
- **Why:** the original one-line read-only scenario could not demonstrate the
  cache geometry or whether cache components compose into levels.
- **Checked:** native scenario and broad cache test passed; focused CTests
  passed 3/3; the Pages build generated 116 WASM-verified cache rows and
  passed strict MkDocs; the memory endpoint browser suite passed against
  port 8893, including the two-way cache layout.
- **Limits:** the stack is a functional single-requester path. CPU I/D
  arbitration, MMIO bypass, coordinated invalidation, coherence and clock
  measurements remain open. The first browser attempt used a venv without
  Playwright; the installed system Python completed the test.

### Replace the cache construction snippet with a behavioral reference

- **Changed:** the public cache page now gives a compilable C++ reference for
  lookup, full-line refill, two-way replacement, masked write-through and
  invalidation. Added a documentation CTest that extracts the exact block.
- **Why:** neighboring component pages use C++ examples to explain behavior;
  merely constructing a cache did not explain its algorithm.
- **Checked:** the new CTest passed 1/1; the strict MkDocs build passed with
  the current generated cache rows; `git diff --check` passed.
- **Limits:** the reference covers successful settled binary transactions,
  not clocked handshakes, four-state behavior or failure statuses. It is not
  a registered behavioral cache implementation.

### Make the cache scenario runnable in the local WebAssembly visualizer

- **Changed:** activated the cached Emscripten SDK, rebuilt the Pages
  simulator, and regenerated the cache example from its WASM timeline.
  Extended the memory browser smoke check to include the cache page and a
  cache-specific layout assertion. Clarified the separate tag/data stores
  in the public reference.
- **Why:** the new cache scenario existed in native tests but the previous
  local browser package could not load it.
- **Checked:** serial WASM build, layout packaging, 12 verified cache rows,
  strict Pages build, and focused `MemoryEndpointBrowserTest` passed. Port
  8893 serves the new scenario JSON and the byte-identical rebuilt WASM
  binary; its cache page returned HTTP 200. `git diff --check` passed.
- **Limits:** the whole browser smoke suite and GitHub Pages deployment were
  not run; there is still no fused behavioral cache or RV32I integration.

### Restore the local Pages preview after adding the cache example

- **Changed:** placed the native-verified 12-row cache export into the
  ignored local example bundle used by the port 8893 MkDocs service. Recorded
  the outage and clarified the cache page's test and fidelity scope.
- **Why:** the new page requested `cache-read-hit`, but the preview's older
  generated bundle lacked that scenario; MkDocs aborted its rebuild and
  left the service unresponsive.
- **Checked:** the preview's strict MkDocs build passed; its home, cache and
  simulator pages returned HTTP 200. Both cache tests passed 2/2, including
  the headless broad test and the recorded component scenario.
- **Limits:** at that recovery step, the simulator package still predated
  the cache scenario. The subsequent Pages rebuild above replaced it.

### Add endpoint-backed structural and scalable-storage caches

- **Changed:** added `Cache<W>` for 8/16/32-bit words with structural tag
  lookup, one/two-way replacement, one/two/four-word fills, all-line
  invalidation, and optional masked write-through/no-write-allocate. Tag and
  data banks independently use existing FF or SRAM endpoints. A second
  catalog form selects scalable endpoint storage while retaining the same
  public pins and structural controller. Added a registered cold-miss/hit
  component scenario, visualizer alias and saved layout. Rewrote the public
  cache reference in the component guide's format, with a generated example.
- **Why:** build a reusable cache from the existing transaction endpoint
  contract and memory technologies before wiring one into RV32I.
- **Checked:** serial `SimulationTestRunner` build; six focused cache,
  catalog and profile CTests passed. Cache checks include a deterministic
  independent set/way reference stream and partial-fill fault. The new
  component scenario passed with 12 checkpoints. A final focused CTest run
  passed 4/4, including visualizer catalog (8/8) against the rebuilt native
  backend. Native-generated example rows passed a strict MkDocs build and
  appeared in the rendered page. The C++ page example compiled; `git diff
  --check` passed.
- **Limits:** geometry is explicitly bounded, the standalone fixture does
  not cover every four-state/fault timing path, and cache timing and
  advanced-core integration have not been measured. Shared RAM and
  arbitration remain open in the cache record. The initial Pages attempt
  lacked an active `emcmake`; the subsequent rebuild above verified the
  local browser scenario. The published site has not been checked.

## 2026-10-14

### Define cache geometry as construction parameters

- **Changed:** refined the proposed `Cache<W>` interface: W selects the
  supported endpoint word-width specialization; textbook sets, ways and
  block bytes become fixed construction parameters; address width remains
  32 bits under the current endpoint contract. Derive words per line from
  block bytes and W instead of exposing both values independently.
- **Why:** the parameters must shape a structural circuit, while the existing
  component catalog needs to select geometry without a C++ type for every
  combination.
- **Checked:** current memory width templates, construction configuration,
  endpoint catalog parameters and the local textbook's cache organization;
  edited Markdown links and whitespace.
- **Limits:** design clarification only; no cache code, build or test ran.

### Generalize the proposed cache across endpoint word widths

- **Changed:** corrected the cache plan from a 32-bit-only component to one
  width-parameterized structural blueprint with 8, 16 and 32-bit contracts.
  Distinguished 32-bit byte addresses from data-word width, kept 32-bit tag
  endpoints, and added width and geometry proof gates.
- **Why:** the existing memory endpoints already generalize word width; a
  reusable cache should match all supported endpoint widths.
- **Checked:** endpoint catalog registrations, parameter validation, REQ/RSP
  pin schema, transaction frontend and storage widths; edited Markdown links
  and whitespace.
- **Limits:** design correction only; no cache code or width-parity test ran.

### Specify cache components and endpoint wiring

- **Changed:** added an ordered inventory for the proposed `Cache32`
  assembly, structural tag matcher, line state, controller, reused tag/data
  endpoints and standalone test root. Clarified exact REQ/RSP port
  compatibility and the distinct cache-control responsibility.
- **Why:** memory endpoints supply replaceable storage, while a cache must
  still map addresses, track valid tags and coordinate refill transactions.
- **Checked:** current endpoint pin schema and assembly, transaction frontend,
  gate/splitter/register blocks and an existing structural tag comparator;
  edited Markdown links and whitespace.
- **Limits:** planning only; no code, compile or simulation was run.

### Require geometry evidence for the cache plan

- **Changed:** clarified how the textbook's set, way, block-size and address
  parameters determine the structural circuit. Added one- and two-set checks
  to the first read-cache gate, followed by separate block-size and way checks.
- **Why:** a single four-set example cannot demonstrate a general cache.
- **Checked:** current cache record and local textbook chapter 6; edited
  Markdown links and whitespace.
- **Limits:** this is a planning correction; no cache implementation or
  geometry test was run.

### Scope the first structural-cache deliverable

- **Changed:** specified a standalone four-set, one-way, one-word read-cache
  slice, its new structural logic, reused endpoint children, a concrete
  hit/conflict trace and an internal endpoint replacement check.
- **Why:** the first implementation step now has an observable finish line
  while keeping memory technologies selectable.
- **Checked:** current cache record, memory protocol, endpoint classes,
  transaction frontend, component catalog and structural wiring; edited
  Markdown links and whitespace.
- **Limits:** proposed scope only; no cache code or simulator test was run.

### Detail the standalone structural-cache plan

- **Changed:** expanded the open cache record with a textbook-grounded cache
  organization, one proposed public cache contract, independently replaceable
  tag/data endpoint banks, a visible lookup/refill/write sequence and staged
  verification gates. Updated living status.
- **Why:** the first structural cache needs a concrete general interface and
  replaceability tests before implementation.
- **Checked:** local textbook chapter 6, current memory protocol and endpoint
  assembly, structural wiring, component profiles and endpoint scenarios;
  checked edited Markdown links and whitespace.
- **Limits:** this is design work. No cache code, simulation, benchmark or
  public component page changed; proposed policies and timing are unverified.

### Add seven-target RV32I result figures to §4.7

- **Changed:** added a measured FF workload-group overview, controlled
  dependency/load-scheduling contrasts, a longer-branch matrix and a separate
  conditional full Embench projection. Each core comparison displays
  single-cycle, baseline and all five advanced prediction modes. Added a
  reproducible chart generator and instructions.
- **Why:** the replacement figures make the three-core comparison visible
  while keeping narrow experiments and historical projections in their
  proper scope.
- **Checked:** validated all 602 public result-table cells against completed
  inputs and the 133 projected Embench cycles against the original historical
  cases; inspected PNG previews and SVG metadata; confirmed four built SVGs
  match the source assets; passed two focused documentation CTests, strict
  MkDocs build and `git diff --check`.
- **Limits:** no benchmark rerun or simulator change. Full Embench remains a
  clock-only projection from older cycles; no full native suite, WebAssembly
  package or browser check was run.

### Rewrite the component creation guide for the current workflow

- **Changed:** replaced the deprecated checklist with the current path for
  structural construction, catalog registration, scenario and equivalence
  tests, generated behavioral delays, frontend registration and the public
  component page template. Corrected linked timing commands to preserve
  clocked tables and use complete verification.
- **Why:** the older instructions omitted the proven-delay step and treated
  optional direct Python bindings as mandatory.
- **Checked:** traced the instructions through source and build scripts;
  37 local links and 12 source example paths resolved; `git diff --check`
  passed.
- **Limits:** no component code or public page changed, so C++ tests, delay
  generation, the site build and browser checks were not run.

### Withdraw §4.7 charts and replan the visual comparisons

- **Changed:** removed all four §4.7 SVG charts, their page embeds and the
  chart-only generator. Recorded a new figure plan grounded in the current
  §4.2 test groups and §4.7 result tables; each proposed core comparison
  shows all seven comparable FF targets.
- **Why:** the previous core and Embench plots hid advanced/two-bit as an
  implicit denominator and displayed only two core series; other plots had
  narrower scopes than the report's central three-core question.
- **Checked:** reviewed the current experiment setup, report and source
  results; confirmed the rendered page has no chart embeds or remaining
  §4.7 assets; passed `RV32IBenchmarkDocumentationTest` (1/1), strict MkDocs
  build and `git diff --check`.
- **Limits:** no replacement chart or benchmark run was made. The full
  Embench figures remain conditional projections from older cycles.

### Correct cache-storage replaceability at the endpoint boundary

- **Changed:** corrected the open cache record and status: tag/data banks can
  be built from existing transaction endpoints and later swapped for scalable
  endpoint forms without a native FF/SRAM pin adapter. Native array wiring is
  now an optional optimization.
- **Why:** the existing endpoint interface was designed to make memory
  technologies interchangeable for requesters, including a cache controller.
- **Checked:** current `protocolPins`, endpoint catalog and structural
  controller/device assembly; edited Markdown links and whitespace.
- **Limits:** replaceability preserves pins and handshake, while operation
  support and latency vary by technology. No cache was built or benchmarked.

### Rewrite §4.7 around the §4.2 workload groups

- **Changed:** reorganized the complete results report into fixed programs,
  generated families, controlled core-gap cases, compiled fixtures and a
  separately marked full Embench projection. Placed each result table beside
  its interpretation, split core-gap settings by mechanism, and made the
  seven-target tables show modeled ticks with a clear clock conversion.
- **Why:** the earlier analysis and tables were far apart, making it hard to
  follow how the three cores and five advanced branch policies performed on
  each test group.
- **Checked:** validated all 99 displayed result rows against the previous
  report and complete summaries; recomputed winner counts and ratios;
  passed two focused documentation CTests, the strict MkDocs build, chart
  link/anchor checks and `git diff --check`.
- **Limits:** no benchmark or code change. Full Embench is still a
  conditional clock-only projection; no full native suite, WebAssembly
  package or browser check was run.

### Assess reuse of existing memory components inside caches

- **Changed:** added a cache-record feasibility review of reusable FF/SRAM
  arrays, scalable native storage, and technology-selectable backing endpoints.
  Clarified the proposed structural-controller-plus-scalable-array path.
- **Why:** the owner asked whether cache construction can follow the endpoint
  composition pattern without rebuilding memory devices.
- **Checked:** current endpoint assembly and catalog, native FF/SRAM pins,
  memory protocol, structural size limits, and primary Ibex/AMD cache
  descriptions; edited Markdown links and whitespace.
- **Limits:** no cache was assembled or timed. FF/SRAM cache-array adaptation,
  policy compatibility and hit-path speed remain implementation questions.

### Reorder the cache plan around a standalone structural component

- **Changed:** revised the open cache and future-steps records and living
  status to adopt structural cache, measured/timed unified scalable RAM, then
  equivalent scalable caches. Added component contracts, latency measurement,
  shared-memory integration and differential-test gates.
- **Why:** the owner chose this sequence and requires structural/scalable
  compatibility like the existing memory endpoints.
- **Checked:** inspected current memory and RV32I wiring, endpoint tests,
  timing-table extrapolation, benchmark scope, and the public memory and
  advanced-core contracts; checked the RISC-V instruction-fetch rule and a
  modern split-cache hierarchy against primary documentation; checked edited
  Markdown links and whitespace.
- **Limits:** this is a plan. No cache, shared-RAM root, timing calibration,
  benchmark or simulator behavior changed; the selected cache policy and
  timing model remain proposals.

### Expand §4.7 result charts and keep run provenance internal

- **Changed:** added predictor-pattern, timed-memory and conditional Embench
  projection charts to the existing core comparison; removed source/binary
  hashes and direct experiment artifact paths from public §4.7. The internal
  [results presentation record](records/2026-10-14-rv32i-results-presentation.md)
  retains their provenance and chart-generation method.
- **Why:** the single core chart did not visualize policy sensitivity, memory
  wait costs or the dense projected Embench comparison. Run-file details
  belong with internal evidence rather than in the public results narrative.
- **Checked:** regenerated four SVGs with validated source rows, inspected
  chart previews and XML, passed the focused documentation CTest and strict
  MkDocs build, confirmed chart links and built SVG copies, and passed
  `git diff --check`.
- **Limits:** no benchmark rerun or simulator change; Embench remains a
  conditional clock-only projection. No full native suite, WebAssembly
  package or browser check was run.

### Focus §4.7 on current results and performance comparison

- **Changed:** moved the former public clock-development analysis to the
  internal archive, removed it from the MkDocs navigation and §4.2 links,
  and added a dedicated §4.7 analysis of matched core, predictor and memory
  results with a reproducible SVG chart.
- **Why:** §4.7 should compare the current completed experiments without
  presenting retired timing work as part of the public results chapter.
- **Checked:** recomputed chart ratios from complete summaries with matching
  source/binary fingerprints, visually inspected the plot, and confirmed the
  archived page is absent from the built site. The focused documentation
  CTest, strict MkDocs build and `git diff --check` passed.
- **Limits:** the full Embench table remains an explicitly conditional
  clock-only projection; no new performance runs, full native suite,
  WebAssembly package or browser checks were performed.

### Assess the shared-RAM arbiter choice

- **Changed:** added a comparison of a scoped 2:1 arbiter, true two-port RAM
  and banked RAM to the open cache plan; clarified that the proposed arbiter
  serves the shared backing port while L1 hits remain independent.
- **Why:** the owner asked whether arbitration is a sound choice and whether
  modern memory systems use it.
- **Checked:** current core wiring, transaction contract and registered tests;
  AMD interconnect/cache documentation and Chipyard's memory hierarchy;
  edited Markdown links and whitespace.
- **Limits:** the recommendation is provisional; no arbitration wait or
  cache performance measurement was made, and no code was changed.

### Start the cache system plan

- **Changed:** recorded the owner's cache → game-program → AI-accelerator
  order, opened a cache topic record with proposed shared-RAM, structural
  cache, scalable-cache and measurement gates, and linked it from status.
- **Why:** the cache stage needs an inspectable contract and acceptance gates
  before implementation or game-program work begins.
- **Checked:** reviewed current memory/core wiring, transaction and wait
  contracts, registered tests, profile rules and the open future plan;
  checked the official RISC-V `FENCE.I` rule. Checked edited Markdown for
  whitespace and links.
- **Limits:** architecture and policies are proposals; no cache, unified RAM,
  program run, site build or timing measurement was completed.

### Correct behavioral-delay documentation

- **Changed:** replaced stale one-tick descriptions across ALU8, FF memory
  and RV32I component references with the catalog's gate-derived delay-table
  and optional delay-twin behavior. Updated the timing guide/reference, source
  contract comment and RV32I runner README to distinguish variable
  propagation from fixed certified periods (FF 166/80/100; timed 102).
- **Why:** the old prose described the constructor fallback as the normal
  behavioral timing model and still quoted retired clock certificates.
- **Checked:** source paths and generated table coverage; 15/15
  documentation CTests; strict MkDocs build with existing example rows;
  `git diff --check`. A direct advanced/SRAM clock query returned 102 ticks.
- **Limits:** no runtime behavior changed; the full native suite, WebAssembly
  package and browser checks were not rerun. Large-memory delay extensions
  are estimates, and a full Embench current-source run remains a projection
  in §4.7.

### Research source programs for the first guest games

- **Changed:** added a candidate sequence and direct source references for
  2048, Snake, Pong, Tetris, a later sprite game, Mandelbrot, and DOOM to the
  open future plan; no first game was selected.
- **Why:** available source can shorten a CPU-game port only after its host
  input, timing, display and runtime dependencies are identified.
- **Checked:** inspected upstream source/README/license pages and the current
  platform limits recorded in this repository.
- **Limits:** no source was downloaded into the repository, compiled for the
  guest, or run on the simulator; runtime and memory feasibility remain open.

### Add a pre-game ISA expansion decision to the future plan

- **Changed:** recorded RV32I, Zmmul, and RV32IM options and a provisional
  RV32IM recommendation for the compiled-game target; the ISA choice remains
  open, and memory/cache work stays first.
- **Why:** the owner wants to consider adding useful CPU operations, not just
  rely on software arithmetic.
- **Checked:** reviewed current decode/ALU and the RISC-V M specification;
  compiled small multiply/divide C functions with all three `-march` choices.
  The pinned compiler emitted helper calls for RV32I and Zmmul, and hardware
  instructions for RV32IM. Tiny ELFs linked, but the Zmmul image pulled in
  compressed instructions from the default runtime; RV32IM selected its
  dedicated multilib and emitted `mul`.
- **Limits:** no simulator run, hardware design, or game performance
  measurement was made.

### Record memory-first priority and RV32I arithmetic feasibility

- **Changed:** recorded the owner's decision to finish unified-memory/cache
  work before games, superseding the earlier game-first proposal; added a
  source-based assessment of software multiplication on RV32I.
- **Why:** the implementation order is now owner-directed, and hardware MUL
  is a performance option rather than a functional prerequisite for simple
  integer games.
- **Checked:** read the current ISA/toolchain contracts and official RISC-V
  and GCC references; inspected a built RV32I matrix-multiply ELF whose
  `__mulsi3` helper uses add/shift instructions.
- **Limits:** no new build, simulation, or game performance check was run;
  cache design and any M/Zmmul extension remain open.

### Sequence program and memory milestones

- **Changed:** proposed a stable address/MMIO contract, one playable CPU
  program on existing memories, shared RAM on the wait-capable core, then
  optional caches tested against slower backing memory.
- **Why:** the first game does not need shared code/data storage, while DMA
  integration does; today's zero-wait FF endpoint supplies no modeled cache
  miss penalty to reduce.
- **Checked:** reviewed current machine, memory-wait and scalable-memory
  contracts and the open plan; checked Chipyard's hierarchy reference.
- **Limits:** sequencing is a proposal; no new implementation, game run, or
  cache-speedup measurement was performed.

### Propose a shared-memory integration path

- **Changed:** recorded a preferred first merge: retain the two CPU request
  paths, arbitrate them onto one shared RAM/address map on the advanced core,
  then add optional caches after the uncached contract works.
- **Why:** one RAM image removes instruction/data divergence while the
  advanced core can tolerate contention from a single-ported backing.
- **Checked:** compared current core-memory wiring and wait contracts with
  Intel cache-hierarchy and RISC-V instruction-visibility references.
- **Limits:** planning only; no arbiter, shared RAM, or new test was built.

### Clarify scalable-memory access timing

- **Changed:** added a source-based timing comparison of the five scalable
  native devices and their transaction endpoints to the open future plan.
- **Why:** cache and shared-memory proposals depend on whether the existing
  backing memory models an address-dependent or variable access cost.
- **Checked:** inspected container, device, controller, sparse-storage and
  delay-table code; four focused memory contract/timing CTests passed using
  the existing `build-cleanup` binary.
- **Limits:** no fresh build or per-address latency sweep; large-depth FF
  delay tables are estimates, and host runtime was not measured.

### Report current RV32I performance panels at final periods

- **Changed:** replaced §4.7's older result tables with 497 completed
  current-source panel case/target rows, core/predictor/memory comparisons,
  and a separately labeled conditional 133-cell full Embench clock-only
  projection. Updated §4.2 links to the new result scope.
- **Why:** the earlier page mixed source snapshots and could present old ticks
  as current results; the full Embench workload was intentionally left as a
  projection rather than rerun.
- **Checked:** current tables were rendered from completed local summaries
  and focused direct-run JSON pairs; 420 broad FF cases, 35 long branches,
  18 memory pairs, seven focused program-20 memory pairs, three capacity
  cases and 14 ELF fixtures passed their recorded checks. The focused
  documentation CTest, strict MkDocs build and `git diff --check` passed.
  The non-Embench native suite had 449/450 sandbox passes and its sole
  sandbox `EPERM` case passed separately outside the sandbox.
- **Limits:** projected Embench ticks assume unchanged historical cycles;
  no current-source full or reduced Embench suite was run. The site build
  reused generated examples; WebAssembly packaging and browser checks were
  not run.

### Evaluate unified memory and cache integration

- **Changed:** added a source-based comparison of the current dual-memory
  testbench, unified backing-memory options, and optional small/scalable
  cache stages to the open future-steps plan.
- **Why:** device I/O and future DMA need one address-map decision, while cache
  misses and coherence would change current core and timing contracts.
- **Checked:** traced memory wiring, endpoint storage, ELF loading, oracle
  modes, core timing limits and public contracts; checked primary RISC-V and
  Chipyard references. No build or test was run for this planning increment.
- **Limits:** no shared-memory/cache prototype or measured cache benefit;
  design and integration order remain proposals.

### Assess interactive platform, GPU, and AI accelerator goals

- **Changed:** recorded current implementation gaps, external architecture
  references, and candidate milestones in the open future-steps plan.
- **Why:** the three requested goals need distinct success criteria and shared
  device/memory groundwork before implementation choices are made.
- **Checked:** read current simulator, RV32I, memory, visualizer and test
  sources, open records, and primary external references; `ctest -N` lists
  451 tests, but no build or test was run for this planning review.
- **Limits:** no game or accelerator prototype, throughput measurement, or
  approved design; feasibility and sequence are proposals.

### Compare the Embench core results

- **Changed:** recorded per-core full Embench winners, a fixed-policy
  modeled-time comparison, reduced-work structural evidence and the boundary
  between measured old periods and a conditional final-clock projection.
- **Why:** the 19-workload suite gives broader performance evidence than the
  current short branch and program-20 cases, but it ran on an older circuit.
- **Checked:** audited all 133 completed full case summaries and nine reduced
  structural summaries; recomputed periods, winners and geometric mean ratios.
- **Limits:** artifact analysis only; no current-source Embench run, new
  simulation, CTest or site build.

### Analyze performance of the three RV32I cores

- **Changed:** recorded the three-core modeled-time comparison, clock/cycle
  break-even thresholds, predictor effects and historical counterexamples.
- **Why:** a shorter certified period does not by itself determine which core
  finishes a workload first, and historical broad rankings are not current
  measurements.
- **Checked:** recalculated five current branch comparisons and current
  program-20 times from artifacts; counted winners across the 133 historical
  Embench cases and checked selected core-gap values.
- **Limits:** analysis only; no new simulation, build, CTest or site build.
  Broad current-source core-gap and compiled performance remains unmeasured.

### Rewrite RV32I experiment results around final clock periods

- **Changed:** replaced §4.7 with final current-source clock certificates,
  current 35-case branch cycles and ticks, seven verified program-20 memory
  pairs, and dated broad fixed, core-gap and compiled results. Added the
  completed prefix and logic panels to the coverage inventory.
- **Why:** the old page mixed development chronology with results and could
  be read as applying historical measurements to current 166/80/100/102-tick
  clocks.
- **Checked:** rechecked ten safe-clock certificates and 14 current program-20
  runs; matched all new and transcribed result tables against artifacts.
  `RV32IBenchmarkDocumentationTest`, strict MkDocs build and `git diff --check`
  passed; retained inbound result-page anchors.
- **Limits:** broader FF, timed-memory and compiled campaigns were not rerun.
  The site build used existing example rows; WebAssembly packaging and browser
  tests were not run.

### Run current-source long branch structural comparisons

- **Changed:** completed all 35 long branch-pattern cases with oracle,
  expected-result and structural-reference checks; recorded the new result
  in §4.7 and a separate topic record.
- **Why:** the September 24 long branch supplements had 35 behavioral checks
  but no structural comparisons, and current-source evidence was needed.
- **Checked:** five seven-case campaigns and a one-case pilot completed at
  certified 166/80/100 tick periods; artifact audit verified 35 structural
  flags, 105 phase records, signature equality and `COMPLETE` markers.
  The serial native target build found the experiment binary up to date.
  `RV32IBenchmarkDocumentationTest`, strict MkDocs build and `git diff --check`
  passed.
- **Limits:** one measurement repetition with no host warmup; the broader
  core-gap, compiled and timed-memory campaigns were not rerun. The site build
  used existing example rows; WebAssembly packaging and browser tests were
  not run.

### Assess missing RV32I structural comparisons

- **Changed:** recorded measured-cost estimates for the 35 long branch cases
  and two full CRC32 cases, with the full Embench budget limits and a
  practical structural-check scope.
- **Why:** a completed behavioral/oracle result does not indicate whether
  adding a structural comparison is affordable on this host.
- **Checked:** compared historical structural run times, cycle/event counts,
  case summaries and campaign options; checked record links and whitespace.
- **Limits:** extrapolations use an older source/host. No new simulation,
  build, CTest or site build was run.

### Audit all recorded RV32I experiment outcomes

- **Changed:** recorded complete and partial campaign inventory, 19 × seven
  full Embench cycle results, compiled/fixed/core-gap evidence and explicit
  rerun decisions; updated living status to distinguish historical snapshots
  from current performance claims.
- **Why:** the published results page summarizes only selected completed
  campaigns, and new delay and memory-edge behavior changes the basis for
  present-day performance conclusions.
- **Checked:** compared `COMPLETE` markers, 1,179 case summaries,
  structural flags, periods and provenance; machine-checked all 133
  transcribed Embench values, record link targets and whitespace.
- **Limits:** artifact/documentation audit only. No experiment, CTest, source
  build, WebAssembly or site build was run; the broad current-source reruns
  remain open.

### Separate RV32I experiment setups from results

- **Changed:** moved dated campaign outcomes and clock-certificate values
  into §4.7; kept §4.2 focused on setup, test methods, artifact contracts
  and links to those results. Labeled the older 124/66/88 periods as an
  intermediate snapshot.
- **Why:** readers need a clear boundary between how experiments are run
  and what the completed runs established.
- **Checked:** the RV32I benchmark documentation CTest and strict MkDocs
  build passed; rendered result anchors and `git diff --check` passed.
- **Limits:** this is documentation work; no broad performance campaign or
  current-checkout Embench run, WebAssembly package or browser check was
  performed. The site build used the existing generated example rows.

### Update RV32I benchmark and verification chapter

- **Changed:** completed §4.2's campaign, recovery and external-runner test
  inventory, timed-memory handoff, certificate method and handshake-state
  regression; linked its compiled workloads to the results page.
- **Why:** let readers distinguish registered test passes, structural
  comparisons and full behavioral workloads while using current timing rules.
- **Checked:** six focused CTests passed serially; the documentation CTest
  passed again after the final wording edit; ten current safe-clock reports
  matched the table; strict MkDocs build, chapter links and `git diff --check`
  passed using the existing generated example bundle.
- **Limits:** no new performance campaign, full-suite rerun, WebAssembly
  package or browser checks. The performance README remains open follow-up
  work; the §4.7 clock claim was corrected in the entry above.

### Audit RV32I chapter 4.2 against experiments and benchmark tests

- **Changed:** recorded which chapter 4.2 contracts remain current and which
  harness tests, timed-memory validation details and completed compiled
  campaign links the public pages still need.
- **Why:** answer whether the chapter reflects the available experiment
  evidence before editing the public pages.
- **Checked:** compared chapter 4.2 and §4.7 with test registration, test
  source, campaign tools, completion artifacts and the delay record; checked
  the 13 benchmark-label CTest registrations without executing them.
- **Limits:** no public page, code, experiment or site build changed or ran.

## 2026-10-13

### Record RV32I test and experiment evidence for documentation planning

- **Changed:** completed the RV32I test inventory and dated campaign evidence
  record; opened a separate documentation-update record with a proposed page
  sequence, evidence boundaries and verification steps.
- **Why:** separate current registered checks and their recorded pass result
  from historical structural, reduced compiled and full behavioral campaigns
  before revising the public results.
- **Checked:** inspected CTest registration, test source, campaign completion
  and per-case metadata; ran current safe-clock certificates. The tracked
  clean-build record reports 451/451 native tests at `448e4e3`.
- **Limits:** no new full suite, performance campaign, site build or browser
  validation for this review; no public RV32I experiment page was edited.

### Align component references with current four-state behavior

- **Changed:** corrected public Fetch, Decode, memory-requester and pipeline-
  register behavior descriptions for unknown values; named the existing
  advanced Fetch, Memory and WB-bypass parts; updated the internal
  single-cycle stage reference to name `IncrementBy4`.
- **Why:** several component pages still described older binary fallbacks or
  conflated standalone requester state with the complete core's binary model.
- **Verified:** 11 selected documentation CTests passed serially; strict
  MkDocs build passed using the existing generated example rows;
  `git diff --check` passed.
- **Limits:** documentation only; the full native suite, WebAssembly package
  and browser checks were not rerun. RV32I experiment and test-case
  inconsistencies and behavioral delay computation descriptions were left out.

### Sort maintained internal docs into guides and reference

- **Changed:** moved three procedural or explanatory pages to `docs/guides/`
  and eight contract and architecture pages to `docs/reference/`. Added folder
  indexes, updated the placement rule in `AGENTS.md`, and repaired navigation
  links and the exact-path documentation test.
- **Why:** make a document's purpose clear from its location and leave the root
  of `docs/` for the index, living status and changelog.
- **Verified:** 1,024 source Markdown links resolved; the RV32I reference
  documentation test passed through CTest (1/1); `git diff --check` passed.
- **Limits:** generated simulator directory links are checked by the site
  build, which was not rerun. The move does not resolve stale claims found by
  the ongoing documentation audit.

### Delay model explained in one document

- **Changed:** added `docs/delay-model-explained.md`, a guided explanation of
  the delay work from the problem to the results (engine, delay tables, gate
  lists, X values, the safe clock, timed memories, experiments, limits), and
  linked it from `docs/README.md`.
- **Checked:** numbers copied from `docs/circuit-timing-analyzer.md`, the open
  delay record and the source; quoted commands and test names checked against
  the tools' options and the CTest list. No code changed; nothing rerun.
- **Limits:** it repeats the published-result gaps (advanced core at 100,
  timed memories at 102, not rerun).

### Advanced core clock covers the compact core; handshake states checked

- **Changed:** the certified clock includes the compact core's setup needs at
  its memory pins for every memory (advanced core with flip-flop memory: 98
  to 100). New validation option `--check-handshake-states`.
- **Why:** 98 was proven for the gates but only observed for the compact
  form; the handshake state lists had no check against simulation.
- **Verified:** every port state in program 20 is in its list for all five
  memories; nine clock and experiment tests pass.
- **Limits:** full suite and site build not rerun; published results at
  period 98 not rerun.

### Clock certified for cores with SRAM, DRAM, ROM and NOR-flash memory

- **Changed:** the safe-clock analysis enumerates the handshake states of a
  timed memory endpoint with the core's requester and settles each case;
  `--period auto` works for those memories (102 for the advanced core) and
  includes what the compact core's table needs at its memory pins.
- **Why:** the analysis stopped at a dependency loop between "request ready"
  and "response ready", so those systems needed a hand-picked period.
- **Verified:** gates and compact form agree at the automatic period for all
  four memories with 0 setup violations; 13 timing and experiment tests pass.
- **Limits:** full suite and site build not rerun; flip-flop memory unchanged
  at 98; no binary-startup certificate for timed memories. See the
  [record](records/2026-10-12-behavioral-delays-and-memory-clock-edge.md).

### Organize existing internal documentation

- **Changed:** moved 14 superseded guides, completed reports and older plans
  from the root of `docs/` to `docs/archive/`; added an index covering all 61
  archived documents. Reworked `docs/README.md` around the current developer
  references and repaired affected links in internal notes, the root README,
  agent instructions and a public site reference.
- **Why:** make the current guidance easy to find and keep historical claims
  separate from maintained contracts and procedures.
- **Verified:** the strict MkDocs build passed; 1,010 local Markdown document
  links resolved; the archive index covered every archived Markdown file;
  `git diff --check` passed. No C++ tests were run for this file relocation.
- **Limits:** archived content retains the claims and measurements of its
  original code state. This organization does not revalidate those claims.

### Compact parts follow their gates after X/Z; memories answer at clock edges

- **Changed:** compact parts leave X where their gates do; the self-timed
  memory sequencer shows its answer only at a clock edge; the timing analysis
  marks memory endpoints as keeping to the clock; delay tables regenerated;
  experiments report `setup_violations`; `--period auto` refuses non-FF
  memories; the `RV32IBinaryClockCLITest` limit went from 180 s to 300 s.
- **Why:** gate-level and compact forms disagreed after unknown inputs, and
  SRAM and DRAM systems lost responses or hung at some clock periods.
- **Verified:** native suite 450 of 451 in one serial run, the remaining test
  passing alone in 134 s; `DelayRuleTest` 0 rule breaks over 120,081
  situations; site build; 36 of 36 web checks; gate-level equals compact in 14
  system runs.
- **Limits:** published results for sequenced memories are not rerun; no clock
  is certified for cores with those memories. See the
  [record](records/2026-10-12-behavioral-delays-and-memory-clock-edge.md).

### Establish documentation recording rules

- **Changed:** made `AGENTS.md` the single agent rule set and made `CLAUDE.md`
  point to it. Added a living status, changelog and dated topic-record template.
  Moved the historical three-year roadmap out of the agent instructions and
  updated the internal documentation index.
- **Why:** keep current operating rules separate from historical plans and
  record substantive work without requiring a separate documentation request.
- **Verified:** `git diff --check` passed; 54 relative Markdown links in the
  edited and new documents resolved; the archived roadmap matched the former
  agent-file text verbatim; the dated record header and status listing agreed.
  No C++ tests or site build were run because source and `site-docs/` were not
  changed.
- **Limits:** this change establishes the workflow and an initial status. It
  does not revalidate every existing component claim or reclassify every older
  root-level internal document.
