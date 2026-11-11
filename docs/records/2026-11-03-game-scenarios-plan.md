# Short game scenarios and long game recordings

Started: 2026-11-03. Status: closed (2026-11-10). Code state:
`alpha` at `5549ee0` plus the uncommitted Device I/O work closed in
[the device I/O record](2026-10-28-device-io-architecture.md).

## Summary

Each game gets two versions.

- **Short:** a normal simulator scenario that runs live in the visualizer.
  It starts from a prepared board so the game's key event (a merge, a crate on
  its goal, a bite of food, a paddle bounce, a cleared line) happens within
  1–3 button presses. It runs on a smaller screen, and nothing is precomputed
  into a file.
- **Long:** a full game session played by a deterministic computer player.
  It runs natively and is published as a recording (`?recording=…`) in § 5.6.

Both run on the § 5.6 machine at one realistic fidelity: `games-machine`
(`games-machine-leds` for the LED game), Compact parts on their proven
delays, embedded timing, and a 4.5 ns clock.

Implemented and published in `acaa085` (2026-11-03): the six short games
are scenarios under **Computer System / Games**, and the six long sessions
are published recordings. Follow-up work is in [Visual long
games](2026-11-03-visual-long-games.md), [replayable
recordings](2026-11-06-replayable-recordings.md) and [SRAM main
memory](2026-11-09-sram-main-memory.md), which re-recorded the long games
on the SRAM machine.

## 2026-11-03 — Starting point

What runs today:

- Eleven short scripted fixtures run natively under every check (§ 5.6 table)
  and are published only as recordings. Most use 3 presses and show 2–4
  frames; the Pong rally shows 18.
- No game runs as a menu scenario. The five `ComputerSystem*ScenarioTest`
  scenarios (`src/tests/RV32IMRecordedTests.cpp`, `recordedSpec`) build the
  machines with a 4×4 display and only check reset, disable and one queued
  input. They load no program.
- Program scenarios that run live already exist: RV32IM programs 23–27
  (`src/tests/RV32IMProgramTests.cpp`). They precompute 1,200–1,800 cycles
  with full history and check every commit against the oracle.
- The long-game tooling predates the redesign:
  - `tools/rv32im/long_gameplay/plans.py` holds players for all five games
    (2048 lookahead, Sokoban puzzle generator and solver, Snake on a
    Hamiltonian route, Pong tracking both paddles, Tetris placement search).
  - `long_gameplay/guest.c` is a session guest (episodes, restart, Sokoban
    levels compiled in).
  - `demonstrations.json` holds 9–44-move scripts.
  - `LongGameplayProbe`, `run-long-gameplays.py` and
    `prepare-long-gameplays.py` run at unit timing in Visual and publish to
    retired `assets/rv32im/…` paths.
  - The [full-tick campaign](2026-10-25-full-tick-gameplay.md) is still
    open: Snake was published, the worker was OOM-killed, and the other games
    are incomplete.

Cost from the § 5.6 recordings (native, Compact, embedded, 4.5 ns,
frame capture on):

| Quantity | Value | Source |
| --- | ---: | --- |
| Host speed | ≈ 530 cycles/s | Tetris: 191,818 cycles in 362 s |
| Bring-up (greeting, clear, 2 frames) | 38,811 cycles | Bring-up row |
| One Pong update | ≈ 2,000 cycles | Pong rally minus Pong, per extra frame |
| One 2048 move | ≈ 17,000 cycles | 2048 minus 2048 merge, per extra move |
| One Tetris move | ≈ 25,000 cycles | Tetris minus Tetris line clear, per extra move |
| Recording size | ≈ 700 KB + ≈ 48 KB per frame | Pong rally (18 frames) 1.47 MB vs Pong (4) 0.80 MB |

These per-move numbers are differences between different starting boards,
so they are rough until phase 1 measures them directly.

Three consequences follow:

1. **Short versions must be small.** One full-screen clear (64×48, 768
   word stores) and the first draw already cost tens of thousands of cycles.
   A live scenario also keeps every wire's history, so it needs a smaller
   screen and only 1–3 presses.
2. **Long versions must have a goal, not a move count.** The old
   512–1023-update plans would take 10–20 hours each at about 530 cycles/s.
3. **Long recordings get too large.** 300 frames at about 48 KB each would
   make a file of about 15 MB.

## 2026-11-03 — Plan

### The two versions of each game

| Game | Short scenario (live, small screen) | Long recording (full session, 64×48) |
| --- | --- | --- |
| 2048 | Row of four 2s; **LEFT** merges into two 4s and a new tile spawns. 1 press, 2 frames | Lookahead player until the first **128** tile (≈ 90–120 moves) |
| Sokoban | Player, crate, goal in a row; **RIGHT** pushes the crate onto the goal and wins. 1 press, 2 frames | Two generated puzzles solved, with a restart between them (≈ 100–150 moves) |
| Snake | Food two cells ahead; 2 updates: the snake eats and grows to 4. 3 frames | Route player until length **10** (7 bites), then a turn into the wall (≈ 200 updates) |
| Pong | Ball one step from the right paddle; 2 updates: it bounces. 3 frames | Both paddles tracked through two points with ≥ 10 paddle bounces (≈ 300 updates) |
| Tetris | Bottom row missing four cells; an I piece is hard-dropped and the line clears. 1 press, 2 frames | Placement player for 20 pieces with ≥ 3 cleared lines (≈ 150 presses) |
| Snake with LEDs | One step from the wall; the crash makes the 3 lit LEDs blink. 1 update, 2 frames | The long Snake script on the LED machine: LEDs count 3 → 10, then blink |
| Bring-up | Greeting on the console, timer and button reads, a moving square | — (not a game) |

The four acceptance fixtures (2048 merge, Tetris line clear, Pong rally,
Snake wall collision) become the short versions above. They stop being
separate recordings. The plain 3-press fixtures are retired too: a short
version shows more in fewer cycles, and a long version shows real play.

### Short version: a normal scenario

- **Menu:** a new group **Computer System / Games**, one entry per row
  above (`game-2048`, `game-sokoban`, …, `game-snake-leds`, `game-bringup`).
  Each opens in **Compact**.
- **Circuit:** the `system.computer` contract with the game's machine and a
  smaller display (32×24 planned, see D2). The guest program is loaded
  into RAM before reset.
- **Inputs:** the scenario drives `INPUT_VALID`, `INPUT_BUTTON` and
  `INPUT_DOWN` at fixed cycles, as the input scripts do now.
- **Timeline checkpoints:** reset released, each press ("press RIGHT"),
  each `PRESENT` ("frame 2"), console `done`, halt. Select the display for
  the frame, the button queue for the press, the core for the instruction.
- **Clock:** the clock certified for the active timing profile: 4.5 ns at
  embedded ([§ 5.7](../../site-docs/system/5.7.md)).
- **Budget:** at most about 20,000 cycles per scenario. Precomputing must
  finish in about 30 s in the WebAssembly page, with history that fits a
  browser tab. If a game misses this budget, first drop the console
  greeting, then shrink the screen, then remove a press.
- **Checks** (CTest, a Program test per scenario): every retired
  instruction against the RV32IM oracle, every data access and device read
  against the device model, every frame against the host reference
  (`reference-host.c`), console and report. These are the `runRecordedGuest`
  checks run on the same fixture. A WASM parity check compares the
  visualizer scenario's frames with the native run.

Guest changes:

- `render()` takes its cell size from `DISPLAY_WIDTH` and `DISPLAY_HEIGHT`,
  so one game source fits 32×24 and 64×48. The 2048 digits still fit in an
  8×6 cell at 32×24 for one-digit tiles; two-digit tiles need checking in
  phase 1.
- The new short presets (Snake food ahead, Pong near the paddle, Sokoban
  one push) join the existing `GAME_PRESET` values.

### Long version: a recording

- **Machine and fidelity:** as § 5.6 (`games-machine`, Compact, embedded,
  4.5 ns, 64×48). Long runs therefore compare directly with the short ones
  and the old recordings.
- **Players:** reuse `plans.py`. Replace each fixed move count with the goal
  in the table above and freeze the result as
  `tests/fixtures/rv32im/games/<game>-long.input.json`, with its hash in
  the manifest. A player is checked against the real C rules (it runs
  `games.c` through ctypes), so a script that misplays fails before any
  circuit run.
- **Guest:** merge the session mode of `long_gameplay/guest.c` (restart
  between Sokoban puzzles, puzzles compiled in, updates counted instead of
  presses) into `guest/main.c` behind `GAME_SESSION`, so short and long
  versions share one source.
- **Runner:** the existing checked recording path
  (`run-recorded.py` → `SystemExperiment`), with per-fixture budgets in the
  manifest (up to about 5 M cycles and 400 frames) instead of the global
  500,000-cycle and 64-frame limits. Frame capture only; no full-tick capture
  (see D3).
- **File size:** if a long recording exceeds about 5 MB, store each frame
  as its changes since the previous one, with a full key frame every 32
  frames, and teach the viewer to rebuild frames. Measure before building
  this.
- **Campaign:** one game at a time, under a memory cap
  (`systemd-run --user --scope -p MemoryMax=… -p MemorySwapMax=0`,
  `prlimit --as=4 GiB`), cheapest first: Pong, Sokoban, 2048, Tetris,
  Snake, Snake with LEDs. The estimate is about 0.5 M (Pong) to 4 M cycles,
  that is 15 minutes to 2 hours each, about 8 hours in all. A stopped run
  restarts from the beginning; record each run's cycles, instructions,
  events, host time and peak memory here.

### Phases

1. **Measure** (one native job at a time; no browser run with a server):
   - one short preset at 32×24 and at 64×48;
   - one 5-move run per game at 64×48 to get cycles per move;
   - the WASM precompute time and memory of one short scenario.

   Then fix the budgets, the long goals and D1–D3.
2. **Guest:** runtime cell size, the new presets and session mode. Rebuild
   the fixtures with the pinned compiler, audit the instructions, and
   cross-check against the reference model.
3. **Short scenarios:** the scenario class, registry entries,
   `scenarios.json` group, layouts for the Compact fingerprint, CTest Program
   tests, WASM parity, and one browser check.
4. **Long scripts:** goal-based players, frozen scripts, manifest entries
   and budgets.
5. **Recording size:** frame deltas, only if phase 1 shows they are needed.
6. **Campaign:** run the six long sessions serially; record the evidence.
7. **Documentation and retirement:**
   - Rewrite § 5.6 with:
     - a "Short games" part: one section per game with an embedded
       scenario, the board diagram before and after, and the press sequence;
     - a "Long games" part: the recordings table with goal, moves, frames,
       cycles, simulated time, host time and file size;
     - a figure comparing the two versions.
   - Retire `LongGameplayProbe`, `run-long-gameplays.py`,
     `prepare-long-gameplays.py`, `demonstrations.json`, the 3-press
     fixtures and their recordings.
   - Close the [full-tick record](2026-10-25-full-tick-gameplay.md) as
     superseded.
   - Update the guides, STATUS and CHANGELOG.

### Decisions for the owner

- **D1 — Long goals.** 2048 to 128, two Sokoban puzzles, Snake to length
  10, Pong to two points, Tetris to 20 pieces and 3 lines. Each is sized to
  about 2 hours or less. Larger goals (2048 to 256, Snake to 16 to light
  every LED) would take roughly twice as long.
- **D2 — Short screen.** 32×24 (planned; about a quarter of the drawing
  cost) or the full 64×48 (same picture as the long version, slower
  scenario).
- **D3 — Full-tick capture.** The plan publishes long games as frame
  recordings only. Full-tick capture, every signal change, took 1 h 45 min
  for 146 Visual Snake frames. Keep it as an optional tool, or retire it with
  the old campaign.

Nothing in this section has been run.

## 2026-11-03 — Owner direction

The owner said "go" and asked to revise the weak device I/O scenarios on the
way. D1–D3 were not answered; the plan's defaults were taken: the long goals
above, a 32 × 24 screen for short versions, and full-tick capture kept as an
option but not used.

## 2026-11-03 — Phase 1 measurement (replaces the plan's cost estimates)

`RecordedGuestProbe 2048-merge ff 500000 100000 compact 32 24` (old fixture,
unit timing, frame capture, no history): 99,198 cycles, 23,117 instructions,
29.1 million events, 12.5 million wire changes, 274 s. At 64 × 48 the same
fixture took 135,080 cycles and 33,680 instructions. The machine takes about
4.2 cycles per instruction; the native simulation about 360 cycles per
second. The screen size was not the main cost.

A small RV32IM interpreter with stub devices (scratch tool, not committed)
reproduced the 33,680 instructions exactly and showed where they went: `rect`
20,830 (64-bit comparisons for every word written), `render` 3,557, the
fingerprint 1,789, `game_init` 1,369 (byte-wise clear) and `game_cell` (one
rule query per cell, which scans the snake's body or the Tetris piece).

## 2026-11-03 — Implementation

Guest (`tools/rv32im/guest`):

- `rect` clips once with 64-bit arithmetic and then loops on 32-bit
  addresses, with an on-screen fast path; `zero` clears in words; `render`
  uses the new one-pass `game_board`; the 2048 digits are written straight
  to video memory. Pixels are unchanged (`GuestDisplayGeometryTest` passes at
  all four shapes; the host frames of every fixture match the circuit's).
- Cell sizes come from the display's registers, so one program fits 32 × 24
  and 64 × 48.
- One `main.c` with a short mode (presets 1–5) and a session mode
  (`GAME_SESSION`, from the retired `long_gameplay/guest.c`). Short runs
  skip the rule fingerprint (about 1,800 instructions).
- `bringup.c` lights and blinks LEDs when built for the LED machine.

Result on the 2048 merge at 32 × 24: 23,117 → 7,649 instructions.

Players (`long_gameplay/plans.py`) stop at goals instead of fixed counts:
2048 84 updates (tile 128), Sokoban 72 (two puzzles), Snake 123 (length 10,
then the wall), Pong 369 (two points, 12 bounces), Tetris 144 (20 pieces,
8 lines). Pong's rally threshold returned to 160 updates: at 120 the
session had only 8 bounces.

Fixtures (`build-guests.py`, manifest version 2): `bringup` and
`bringup-leds` (8 × 8), six `<game>-short` (32 × 24, presses at fixed
cycles), six `<game>-long` (64 × 48, batches after each frame, button queue
depth 7 because a Pong batch can hold six changes). The ten 3-press and
acceptance fixtures were removed. Host references (`reference-host.c`, now
one driver for both kinds) pass for all 14; the long ones reach the
players' predicted score, state and fingerprint.

Runtime: `runRecordedGuest` is split into building and a checked loop;
`runRecordedGuestOn` runs the loop on a caller's machine and simulator (with
history), and `on_event` reports reset release, sampled inputs, console
lines, frames and the halt. Fixtures carry their screen, queue depth and
batches; `retime` moves inputs to another period.

Scenarios: `GuestScenarioTest` runs a fixture as a visualizer scenario with
every check. The five `ComputerSystem*ScenarioTest` now run the bring-up
program; six `Game*ScenarioTest` run the short versions (group
**Computer System / Games**). Games refuse Logic and Visual, every whole
computer refuses Gate (a new `SimulationTest::unavailableFidelityPresets`
hook used by the visualizer and the Python binding). The old program-free
example remains as `ComputerSystemStructureScenarioTest`
(`computer-system-structure`, Visual), with the old Visual layouts copied.

Device scenarios strengthened: the display draws a 4 × 4 picture, changes
one pixel by byte mask, presents twice and is refused past its storage and on
a setting write; the timer runs with divisor 4 and a declared 222 MHz; the
button queue overflows on a fifth press and the flag is cleared; the LED
panel's pins blink for eight cycles, then go steady.

Retired: `long_gameplay/guest.c`, `long_gameplay/reference.c`,
`long_gameplay/demonstrations.json`, `prepare-long-gameplays.py` and
`accept-recorded.py` (its four-variant campaign targeted the retired
fixtures; `ComputerSystemLockstepTest` covers the variants).
`LongGameplayProbe` and `run-long-gameplays.py` were migrated (fixture by
name, frozen timing profile and period, compact default, publication under
`assets/system/long-games/`).

## 2026-11-03 — Native verification, first build

`build-milestone` (Release), unit timing, each under a 1.5–2 GB cap:

| Test | Result | Cycles | Instructions | Native time | Peak memory |
| --- | --- | ---: | ---: | ---: | ---: |
| Timer, Button, Framebuffer, LedPanel scenarios | pass | | | | |
| `RV32IMRecordedInputValidationTest`, `RV32IMRecordedEvidenceBudgetTest` | pass | | | | |
| `ComputerSystemScenarioTest` (bring-up, 32 × 24) | pass | 7,634 | 1,947 | 22 s | 180 MB |
| `ComputerSystemTwoBusScenarioTest` | pass | 7,674 | 1,947 | 22 s | 179 MB |
| `ComputerSystemRegisteredBridgeScenarioTest` | pass | 7,754 | 1,947 | 22 s | 177 MB |
| `ComputerSystemLedsScenarioTest` | pass | 7,656 | 1,953 | 24 s | 178 MB |
| `ComputerSystemCheckedScenarioTest` | pass | 7,674 | 1,947 | 36 s | 314 MB |
| `GamePongScenarioTest` | pass | 13,324 | 3,189 | 35 s | 187 MB |
| `GameSnakeScenarioTest` | pass | 28,194 | 6,679 | 67 s | 322 MB |
| `GameSnakeLedsScenarioTest` | pass | 25,813 | 6,184 | 80 s | 323 MB |
| `Game2048ScenarioTest` | pass | 32,012 | 7,649 | 90 s | 348 MB |
| `GameSokobanScenarioTest` | pass | 43,273 | 9,741 | 114 s | 457 MB |
| `GameTetrisScenarioTest` | pass | 50,860 | 12,057 | 119 s | 560 MB |

Native time includes the wire history the visualizer keeps. The machine
scenarios were then moved to an 8 × 8 screen (891 instructions in the
profiler) so that a Visual run stays at minutes.

## 2026-11-03 — Second build, 8 × 8 machines, layouts and experiments

After the hook, the structure scenario and the 8 × 8 bring-up screen:

- `ComputerSystemScenarioTest` 3,714 cycles, 891 instructions, 14 s;
  `ComputerSystemLedsScenarioTest` 3,736 / 897, 15 s;
  `ComputerSystemStructureScenarioTest` passes.
- `ComputerSystemCheckedScenarioTest` under `CIRCUITSIM_TIMING_PROFILE=embedded`
  at 4.5 ns: 3,754 cycles, no setup violation. The checked machine has no
  certificate of its own (§ 5.7); this run is evidence for this program only.
  It is the two-bus machine with checkers, so it takes the two-bus cycle
  count (an earlier § 5.5 draft said "same cycles as one bus"; corrected).
- `VisualizerScenarioCatalogTest` and `VisualizerFidelityProfileTest` pass
  after two fixes: the catalog was briefly rewritten with `\u` escapes,
  which the native JSON reader rejects (rewritten as UTF-8), and the group
  list gained `games`.
- Layouts: `recalculate_layouts.py --automatic-only --scenarios` (12
  scenarios) changed 18 automatic entries and kept 163 custom ones; the
  display's Compact entries for 8 × 8 and 32 × 24 were copied from its 4 × 4
  entry (same five children). `check-static-layouts.py` on the 12 scenarios:
  0 problems. The Python server must load the fresh `build-milestone` module;
  the deployed `visualizer-v2/circuit_backend*.so` is stale.

Experiments (`build-milestone/system-experiments-2026-11-03/`, summaries
copied to `site-docs/assets/system/results/`):

- Embedded, 4.5 ns, each fixture on its own machine: the same cycle counts
  as the unit-timing scenario tests (bring-up 3,714; 2048 32,012; Sokoban
  43,273; Snake 28,194; Pong 13,324; Tetris 50,860; Snake with LEDs 25,813),
  native batch times 13–102 s.
- Unit, three machines, six fixtures: identical frames, console and reports;
  two buses add 32–40 cycles and the registered bridge three times as many,
  one and three cycles per device access (Pong: 37 accesses, +37 and +111).
- The old-machine baseline table keeps its retired-fixture data
  (`unit.json` renamed `unit-retired-fixtures.json`).

## 2026-11-03 — WebAssembly, browser checks and the campaign launch

- `circuitsim_wasm` rebuilt (`build-wasm-milestone`); example rows
  regenerated for the five changed scenarios (display 20 rows, LED panel 21,
  timer 9, structure 4, buttons) in isolated processes on top of the
  2026-11-02 bundle; the strict 8893 preview build passes.
- Browser and WASM checks (`tools/run-web-tests.py`, 8893, system Python
  with Playwright):
  - `GameScenarioBrowserTest` (new): Pong's short version runs in the page
    in 47 s, with its 9 checkpoints, Compact only.
  - `DeviceSystemFidelityBrowserTest` (now on `computer-system-structure`):
    pass.
  - `DocumentationDiagramsBrowserTest`: 30 figures pass, after the
    `game-versions` box titles were shortened (cramped labels).
  - `RV32IMDeviceBrowserTest`: 24 pages and 25 sessions pass, after its
    expectations followed the new display picture and button queue (one
    expected hex string first had the word's bytes reversed), and after the
    machines that run a program left its example-table list.
  - `RV32IMDeviceWasmParityTest`: the 24 example scenarios and, new, the
    bring-up machine and Pong's short version match native exactly
    (topology, timestamps, checkpoints, state, final display).
  - Timed in the browser while the campaign shared the CPU: `game-tetris`
    182 s, `computer-system` 27 s.
- CTest: `RV32IMRecorded_*` (8 short fixtures, circuit frames, console and
  report equal the host run; the check first asked for 20 M cycles, above
  the 10 M unpaced limit), `RV32IMReference_*` (6 long fixtures),
  `DocumentationDiagramsTest` (after listing the new figures) and 17 other
  related tests pass.
- Layouts: the timer's divisor 4 adds a clock divider and new register-table
  keys; refreshed (9 entries) and the counter's Logic entry seeded from its
  saved variant. `check-static-layouts.py` over all 387 scenarios: 0 problems.

The first campaign (`build-milestone/long-games-campaign-2026-11-03`) ran
Sokoban to completion (671,744 cycles, 73 frames, 31 min) and failed at
publication: the probe ran Compact as the unnamed compact-children profile,
the campaign expected the named `compact` preset. The probe now passes the
named preset (the one the short versions open in). A dry run of publication
on that Sokoban output passed every frame, report, input and per-frame
button-level check; it also showed each 16-frame part carried an identical
885 KB layout copy, so publication now keeps the first part's layout and
points the others at it (the viewer reads only the first): 8.4 → 5.0 MB.
The campaign was stopped and relaunched as
`build-milestone/long-games-campaign-2026-11-03b`
(unit `circuitsim-long-games-1793660643`); the old one is marked superseded.

## 2026-11-03 — First long game published

`sokoban-long` passed and is published under
`site-docs/assets/system/long-games/sokoban-long/` (5.0 MB: topology index,
one scope, five parts of up to 16 frames, one shared layout, evidence):
673,842 cycles, 159,174 instructions, 3.03 ms simulated at 4.5 ns, 25 min
native. Every frame, the report, the input log and the button levels at
each frame equal the host reference. `RecordedCircuitBrowserTest` (default
now this playlist) passes: topology, all checkpoints' pins and wires,
display and keyboard, inspector and playback. The § 5.6 long table is
generated from the public campaign manifest (`results-tables.py`).

The other five games run serially in
`circuitsim-long-games-1793660643`; at about 300–400 cycles per second the
estimate is 6–7 more hours (Snake, Snake with LEDs, 2048, Tetris, Pong).
When it finishes: rerun `tools/system/results-tables.py`, check
`results-tables.py --check`, then close this record.

## 2026-11-03 — Campaign completed

`circuitsim-long-games-1793660643` completed; all six long games passed the
exact host-reference checks and are published under
`site-docs/assets/system/long-games/` (58 MB in all):

| Game | Frames | Cycles | Instructions | Native time | Parts | Peak RSS | Size |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `sokoban-long` | 73 | 673,842 | 159,174 | 25 min | 5 | 174 MiB | 5.0 MB |
| `snake-long` | 124 | 966,300 | 224,327 | 35 min | 8 | 172 MiB | 7.7 MB |
| `snake-leds-long` | 124 | 975,228 | 226,684 | 39 min | 8 | 175 MiB | 8.1 MB |
| `2048-long` | 85 | 1,954,776 | 473,442 | 70 min | 6 | 177 MiB | 5.6 MB |
| `tetris-long` | 145 | 2,147,750 | 502,831 | 75 min | 10 | 177 MiB | 8.8 MB |
| `pong-long` | 370 | 2,180,301 | 511,921 | 77 min | 24 | 177 MiB | 23 MB |

Total native time 5 h 23 min (the estimate of 6–7 hours was pessimistic).
The § 5.6 long table is regenerated from the public manifest.
`RecordedCircuitBrowserTest` was run on Sokoban only.

## 2026-11-03 — Why the games refuse Logic and Visual (corrects the reason)

The owner asked why Logic and Visual are unavailable in the short games and
wants to see the cores working. The earlier reason ("tens of minutes") was
an estimate from older Visual speeds and is replaced by measurements
(`build-milestone`, embedded timing, 4.5 ns, bring-up program, no history):
Compact 485 cycles/s and 287 events per cycle; Visual 104 cycles/s and
16,000 events per cycle. With history (the visualizer's loader,
`server.SessionStore`), the bring-up machine took 9 s / 274 MiB in Compact
and 55 s / 386 MiB in Visual. The short games with the restriction lifted
(`VisualizerSessionProbe topology … --fidelity`): Pong Visual, Pong Logic
and Tetris Visual were each killed at the 3 GB cap after about 95 s,
unfinished. A browser tab holds at most 2 GB, so the games keep refusing
Logic and Visual; the message and § 5.6 now give the memory reason and
point to `computer-system` in Visual (the bring-up program) to watch the
core's stages work.

A Visual frame snapshot is over 12 MB (`RecordedCircuitProbe bringup …
visual` exhausted its 24 MB budget after two frames; Compact: 25 KB per
frame), so long recordings in Visual would take gigabytes per game; they stay
in Compact.

## 2026-11-03 — Recording viewer made like the live visualizer

The owner asked for the recording interface to look almost the same as the
original visualizer. Recordings already open in the visualizer's canvas;
the differences were fixed:

- The header's recording line had no style and pushed the time readout out
  of view; the time, statistics and recording lines are now one line each
  (ellipsis, full text in the tooltip), as in the live view.
- A recording no longer opens with `DISPLAY` selected and the inspector open.
- The scenario menu: a recording now offers the full scenario catalog (from
  `scenarios.json`, without loading WebAssembly) and both views list the six
  published long games under **Computer System / Games (long versions,
  recorded)**, read from the campaign's public manifest. Choosing a scenario
  from a recording, or a recording from the live view, reloads the page in
  the other mode.
- The separate frame-only viewer `games.html` (with `games.mjs`,
  `games_worker.mjs` and `RecordedGameViewerBrowserTest`) is retired; the
  visualizer is the one interface for recordings.

Checks: switching both ways passes in the browser with no page errors;
`RecordedCircuitBrowserTest` (Sokoban) and `VisualizerScenarioBrowserTest`
(updated for the extra group) pass; `VisualizerDeviceViewTest` and
`RecordedCircuitTransportTest` pass. The root of a recording is still named
`MACHINE`, the name the native runner gives it.

Follow-up the same day: the games' Gate message said "Choose Logic or
Visual", which they also refuse. The games now give their own Gate reason,
and `VisualizerSession` lets a scenario's reasons replace its root
contract's (`std::map::merge` had kept the contract's). Native and WASM
rebuilt; the browser shows the new messages; `GameScenarioBrowserTest`
(Pong in 57 s) and `DeviceSystemFidelityBrowserTest` pass.

Correction (same day): the "> 12 MB per Visual frame" figure recorded above
counted the Visual topology too; a Visual frame is 1.02 MB raw, 15.6 KB
compressed and 5.6 KB as compressed changes only. Measurements are in
[the close-ups plan](2026-11-03-long-game-closeups-plan.md).

## 2026-11-10 — Closed

The plan was implemented and committed in `acaa085`; this record had kept
its first status line. Closed. The long games have since been recorded in
Visual, given machine saves, and re-recorded on the SRAM main memory (see
the records linked in the summary); the cycle counts quoted above are the
FF machine's.
