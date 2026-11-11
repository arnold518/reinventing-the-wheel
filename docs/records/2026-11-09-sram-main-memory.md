# SRAM as the games machine's main memory

Started: 2026-11-09. Status: closed (2026-11-11). Code state: `alpha` at `1ee0600` plus the
uncommitted work of [the main-memory comparison](2026-11-08-games-machine-main-memory.md)
and of this record.

## Summary

Decision (owner, 2026-11-09): the games machines' main memory becomes the
on-chip clocked SRAM macro, set up for the clock the games run at, instead
of the zero-wait flip-flop memory; a 256 KiB flip-flop main memory does not
exist in real chips. FF and DRAM stay available as options for comparison.
This record covers the SRAM timing check made before switching, the code
change, the reruns of the published chapter-5 results and the re-recording
of the six long games in Compact and Visual.

Done: SRAM (3.33 ns access, one cycle at 4.5 ns) is the default of every
games machine (`memory=sram|ff|dram`); all machines certify at 4,254 ps; the
SRAM machine saves; the § 5.6/5.7 results were rerun (SRAM 1.28–1.32× FF's
cycles, 5.3–5.7 cycles per instruction); the six long games were
re-recorded in Compact (5.9 h) and Visual (30.1 h) with the FF recordings'
frames and instruction counts; `PublishedSavesTest` and a frames-only
fallback guard the published saves. The SRAM's analog intervals remain
placeholders.

## 2026-11-09 — Plan

1. Check the SRAM access time the `embedded` profile gives a 256 KiB
   memory against published figures before re-recording on it.
2. Make SRAM the default of every games machine (descriptions and machine
   options), keep FF and DRAM as `memory=ff|dram`.
3. Certify the clocks of the SRAM machines as the games run them (SRAM set
   up for 4.5 ns in `embedded`, 1000 ticks in `unit`).
4. Rerun the published result sets of § 5.6 and § 5.7 (short games in
   `embedded`, the three machines in `unit`), keep the old-machine
   comparison as dated FF evidence, and add the memory comparison.
5. Re-record the six long games in Compact, then in Visual, with a save
   after every frame; check their frames against the FF recordings.
6. Update the pages, STATUS and CHANGELOG; full native suite and web checks;
   commit and push.

## 2026-11-09 — SRAM timing check

The access time the endpoint uses (`staticAccessTime`, clocked macro, from
the controller's edge to READY and the data, including the flip-flop launch)
for a 32-bit memory, and the access cycles it gives (`staticAccessCycles`
adds the capture margin):

| Words | `embedded` access | Cycles at 4.5 ns | `unit` access (ticks) | Cycles at 1000 ticks |
| ---: | ---: | ---: | ---: | ---: |
| 64 | 2,371 ps | 1 | 228 | 1 |
| 4,096 | 2,611 ps | 1 | 257 | 1 |
| 65,536 (the machine) | 3,331 ps | 1 | 353 | 1 |

The `embedded` SRAM intervals are FO4-multiple placeholders (bitline 90 ps
plus 3 ps per row, settle 90 ps, precharge 120 ps, global wire 15 ps per
subarray side); a 65,536-word memory gets delays extrapolated from tables
proven at 64 and 128 words.

Published reference: no access time of a 65 nm SRAM macro of this size was
found in open sources (compiler datasheets need registration). A whole-chip
reference point: NXP's i.MX RT1050 reads its 512 KB on-chip SRAM, configured
as tightly coupled memory, in a single cycle at 600 MHz, i.e. within
1.67 ns ([product page](https://nxp.com/imxrt1050),
[supporting information](https://www.nxp.com/docs/en/supporting-information/i.MX-RT1050-Supporting-Inf.pdf));
its process node was not confirmed by these sources. The model's 3.33 ns is
slower than that, so it is not optimistic.

Decision: no recalibration before switching. One access cycle at 4.5 ns
holds for any access time below about 4.1 ns (the period less the capture
margin), which covers both the reference and the model; a second cycle
would need an access slower than both. The intervals stay marked as
placeholders.

## 2026-11-09 — The change

- `GameMachineOptions` has one `memory` option (`MainMemory`: `sram`
  default, `ff`, `dram`) instead of the `dram`/`sram` flags; the machine
  parameter is `memory=sram|ff|dram`. Every built-in description
  (`systems/games-machine*.json`) names `sram`. The recorded runner passes
  the run's period as the memory's `clock_period` for SRAM and DRAM, so the
  SRAM takes its wait states from the clock (one at 4.5 ns in `embedded`
  and at 1000 ticks in `unit`).
- A run's identity mixes in the memory's name, so saves of the FF machine
  are refused by the new one (they would be anyway: the memory part
  differs). The run JSON's `configuration` reports `"memory"`.
- Callers updated: `system_experiment --memory`, `RecordedGuestProbe` (modes
  `sram|ff|dram|cached-…`, default `sram`), `runRecordedGuestFixtureJson`
  and its Python and WebAssembly bindings (a memory name instead of the
  `dram` flag), `run-recorded.py`, `RecordedGuestWasmParityTest`,
  `RV32IMGameReferenceTest` (now on the default SRAM machine) and the
  computer-system recorded tests.
- `ComputerSystemTimingTest` sets the memory up for the clock the games run
  at (4,500 in a realistic profile, 1,000 in `unit`) and now requires the
  certificate to fit that period in every profile, not only in `unit`.

## 2026-11-09 — Clock certificates

`ComputerSystemTimingTest`, SRAM and DRAM set up for the games' clock:

| Machine | Memory | `unit` (ticks) | `embedded` | Limiting path (`embedded`) |
| --- | --- | ---: | ---: | --- |
| `games-machine` | sram | 160 | 4,254 ps | arbiter `I_RESPONSE.HELD_DATA` write enable |
| `games-machine` | cached-sram | 156 | 4,254 ps | instruction cache pending bit |
| `games-machine-two-bus` | sram | 160 | 4,254 ps | as the one-bus machine |
| `games-machine-registered-bridge` | sram | 160 | 4,254 ps | as the one-bus machine |
| `games-machine-leds` | sram | 160 | 4,254 ps | as the one-bus machine |
| `games-machine` 128 × 96 | sram | 160 | 4,254 ps | as the one-bus machine |
| `games-machine` | ff | 92 | 4,464 ps | `CORE.IMEM_RSP_READ_DATA` |
| `games-machine` | cached-ff | 84 | 4,254 ps | instruction cache pending bit |
| `games-machine` | dram | 84 | 4,254 ps | arbiter held data |
| `games-machine` | cached-dram | 84 | 4,254 ps | instruction cache pending bit |

All are safe at the games' 4.5 ns (and 1000 ticks). In `unit` the SRAM
machines are limited by the RAM's write-mask input (160 ticks), not by the
core. `site-docs/assets/system/results/clock.json` holds these rows.

## 2026-11-09 — Review before re-recording (owner request)

The owner asked for every fix or update worth making before the long
recordings, not only memory ones. Findings:

- Device findings of the [device I/O review](2026-10-27-device-io-review.md)
  (32 button comparators, a full 32-bit match per register) were fixed in
  the chapter 5 rebuild: the buttons use one `Decoder<5>` and the register
  front ends decode with a `Decoder`. Nothing open there.
- **Published saves are fragile.** A save is refused when the machine's
  structure hash, its delay tables, timing profile or period differ, and the
  site's simulator is rebuilt from current source on every deploy. Any later
  change to a part the games machine uses (a shared gate, the ALU, a word
  mux), which GPU work is likely to make, would stop the recordings from
  opening, and nothing checked it: no native test opens a published save,
  and the game and recording browser checks are not in the deploy's smoke
  suite. Two updates, made now:
  - `PublishedSavesTest` (CTest): restores one published save of Pong from
    each set with `GameSessionProbe` (steps, views, jumps back, seeks; all
    paths must end in one state). It fails until the new recordings are
    published, since the published saves are of the FF machine.
  - The viewer falls back to the recorded frames when the simulator refuses
    a save ("Saved machine state does not fit"): it reloads with
    `saves=off` and says so in the status line. Checked by the new
    `GameSaveFallbackBrowserTest`.
- Choices that would change the recordings, defaults taken (owner may
  override before the campaigns): the display's 4 KiB pixel memory stays a
  flip-flop memory (noted as a known limit); the core keeps its static
  not-taken prediction (about 10 % of fetches are on a wrong path); the timer
  keeps declaring 0 Hz; the interconnect stays as it is.

## 2026-11-09 — Full native suite, first run

`ctest --test-dir build-milestone -j2` (616 tests, 95 min): 598 passed,
18 failed. The causes:

- **The SRAM model could not be saved.** `MemoryEndpoint<sram,32>` declared
  no state ("Cannot save a machine with parts that have not declared their
  state"): only the FF model had been audited for saves. Every save and
  resume of the new default machine failed (`MachineStateResume_*`,
  `GameReplayScenarioTest`); the long-game campaign would have failed at its
  first save. Fixed: a memory model can add its own state to the save, and
  the SRAM model saves the request it holds (address, data, mask, CE_N,
  WE_N, OE_N, the last clock level and whether it has seen a request); its
  access time follows from the configuration. DRAM still cannot be saved
  (it is not used for recordings).
- **The game and machine scenarios ran faster than certified.** The
  scenarios (`ComputerSystem*ScenarioTest`, `Game*ScenarioTest`) ran at 100
  ticks in `unit`, a clock chosen when the FF machine certified at 92; the
  SRAM machine certifies at 160, and its memory had been set up for the
  fixtures' 1000. They failed at the first instruction. Fixed: the scenarios
  run at the games' clock (1000 ticks in `unit`, 4.5 ns otherwise), which the
  clock certificate covers, and build the memory for it.
- **Older gaps found by the same run** (the last commit did not run the
  full suite): `TestRegistryCoverageTest` missed `GameReplayScenarioTest`
  in CMake's list of registry tests (it runs through a script); now listed
  in registry order. `VisualizerStaticLayoutTest` reports six missing
  layout entries for the 64 × 48 display in `game-replay`.
- `PublishedSavesTest` compared the hash of the uncompressed save; the index
  hashes the compressed file. Fixed (it also checks the length). It still
  fails, as expected, until the SRAM recordings are published.

## 2026-11-09 — Fixes verified; chapter-5 results rerun

After the fixes, the 16 failed tests other than `PublishedSavesTest` pass
(the eleven machine and game scenarios, the three `MachineStateResume_*`,
`GameReplayScenarioTest`, `TestRegistryCoverageTest`), and
`check-static-layouts.py game-replay` reports 0 problems: the 64 × 48
display's Visual and Compact entries were copied from the 32 × 24 ones (same
five children), as was done for 32 × 24 on 2026-11-03.

Result reruns (`tools/system/run-experiments.py`, serial, outputs in
`build-milestone/system-experiments-2026-11-09/`): 60 runs, all passing the
runner's checks.

- `embedded`, 4.5 ns, each program on its own machine, six memories:
  `memory.json` (the new memory table of § 5.7); the SRAM set is the new
  `embedded.json` (§ 5.6's short-game table).
- `unit`, 1000 ticks, the three machines on SRAM: the new `unit.json`.

| Program | `sram` | `ff` | `dram` | `cached-sram` | `cached-ff` | `cached-dram` | CPI (SRAM) |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Bring-up | 4,760 | 3,714 | 13,491 | 7,738 | 6,681 | 15,257 | 5.34 |
| 2048 | 41,322 | 32,012 | 124,268 | 56,490 | 50,986 | 99,961 | 5.40 |
| Sokoban | 55,533 | 43,273 | 169,589 | 82,368 | 72,691 | 150,923 | 5.70 |
| Snake | 36,963 | 28,194 | 118,134 | 54,649 | 48,234 | 100,326 | 5.53 |
| Pong | 17,268 | 13,324 | 55,711 | 25,069 | 22,221 | 46,885 | 5.41 |
| Tetris | 67,028 | 50,860 | 207,528 | 99,224 | 87,567 | 184,281 | 5.56 |
| Snake with LEDs | 33,960 | 25,813 | 108,985 | 49,268 | 43,689 | 89,670 | 5.49 |

- Every memory gives the same frames, console text, instruction counts and
  reports apart from the timer word. The FF column equals the published FF
  results exactly (the change did not alter the FF machine).
- FF needs 0.76–0.78× SRAM's cycles, DRAM 2.83–3.23×. The caches slow SRAM
  by 37–63 % and FF by 59–80 %; with DRAM they save 11–20 % in the games
  and cost 13 % in bring-up.
- `unit` cycles equal `embedded` cycles on SRAM. Two buses still cost one
  cycle per device access and a registered bridge three (Pong: +37, +111).
- The Visual bring-up scenario loads (with history, `server.SessionStore`)
  in 53 s and 385 MiB, Compact in 9 s and 274 MiB, as before the change.
- Pages: § 5.6 (CPI about five and a half; bring-up about 4,800 cycles) and
  § 5.7 (memory table and claims; six short games in the comparison) were
  checked against these numbers and corrected where they differed.

## 2026-11-09 — Smoke campaigns on the SRAM machine

`run-long-gameplays.py launch --games smoke-long --save-at-frames
--replay-window 1000`, published to `build-milestone/smoke-sram-*-pub`:

| Preset | Result | Cycles | Instructions | Host time | Peak memory | Saves |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| Compact (1 GiB cap) | passed (host reference, every check) | 66,966 | 11,067 | 113 s | 180 MiB | 3 |
| Visual (2 GiB cap) | passed | 66,966 | 11,067 | 545 s | 294 MiB | 3 |

`GameSessionProbe smoke-long SAVE PRESET` restored the middle save of each
(cycle 38,300) and stepped, viewed, jumped back and seeked to one state:
Compact 29 s / 183 MiB, Visual 23 s / 370 MiB.

## 2026-11-09 — Site build and browser checks

`tools/build-pages.sh build-wasm-milestone` (WebAssembly module, example
rows, strict MkDocs build) passed; preview 8893 serves it.

- `GameReplayBrowserTest` on the smoke recordings (served temporarily from
  `site-docs`, removed afterwards): Visual opened as a game session in 11 s,
  Compact in 13 s; event steps, memory reading and the next frame work.
- The fallback, for real: the published (FF) Visual Pong opened in the new
  simulator as its 370 recorded frames with "frames only: this version of
  the simulator cannot open the recording's machine saves", instead of an
  error. `GameSaveFallbackBrowserTest` passes.
- `run-web-tests.py`: the smoke suite (14 checks), `GameScenarioBrowserTest`,
  `RecordedCircuitBrowserTest`, `GameSaveFallbackBrowserTest` and
  `RV32IMDeviceWasmParityTest` pass. (A first attempt gave the simulator's
  address as the site root, so every browser check failed to load; rerun
  with the site root.)
- `RecordedGuestWasmParityTest` still named the ten fixtures retired on
  2026-11-03 ("Unknown recorded guest: 2048"); it now runs the eight current
  short fixtures.
- `RecordedGuestWasmParityTest` on the eight short fixtures: native and
  WebAssembly give exact frames, reports, device samples, cycles and event
  counts (and the 97 × 65 bring-up). § 5.6's browser times still hold on
  SRAM: Pong's short version runs in the page in 49 s, Tetris's in 158 s.

## 2026-11-09 — Compact campaign launched

The published FF set was copied to `build-milestone/ff-long-games-backup/`
(157 MB, Compact and Visual). The campaign keeps a game that is already in
its publication folder (and refuses one made by another runner), so the six
Compact game folders were removed from `site-docs` after checking each
against the backup; `visual/` stays until its own campaign.

    run-long-gameplays.py launch --runner build-milestone/tests/LongGameplayProbe \
      --output build-milestone/long-games-compact-sram --fidelity compact \
      --memory-mib 1024 --save-at-frames --replay-window 1000 \
      --games 2048-long,sokoban-long,snake-long,pong-long,tetris-long,snake-leds-long

(A first launch without `--games` was refused: "Long game publication
already exists".)

## 2026-11-10 — Compact campaign results

All six games passed (campaign `completed`), with no setup violations.
Every game has the same frames (`expectedSha256` and the exact host
reference), frame count and instruction count as the FF recording. Only
the cycle count changes:

| Game | Frames | Instructions | FF cycles | SRAM cycles | SRAM / FF | Host time | Peak memory | Saves |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Sokoban | 73 | 159,174 | 673,842 | 886,046 | 1.315 | 26 min | 176 MiB | 2.6 MB |
| Snake | 124 | 224,327 | 966,300 | 1,265,936 | 1.310 | 37 min | 174 MiB | 4.4 MB |
| Snake (LEDs) | 124 | 226,684 | 975,228 | 1,277,592 | 1.310 | 42 min | 170 MiB | 5.2 MB |
| 2048 | 85 | 473,442 | 1,954,776 | 2,503,400 | 1.281 | 74 min | 175 MiB | 3.0 MB |
| Tetris | 145 | 502,831 | 2,147,750 | 2,837,961 | 1.321 | 81 min | 174 MiB | 5.3 MB |
| Pong | 370 | 511,921 | 2,180,301 | 2,788,844 | 1.279 | 91 min | 177 MiB | 13.8 MB |

The ratios match the short games (FF needs 0.76–0.78× SRAM's cycles). The
whole campaign took 5.9 h of host time, serially.

Saves: a middle save of Pong (cycle 1,404,219) and of 2048 (cycle
1,138,710) restore in `GameSessionProbe` (Compact); the stepped, viewed,
jumped and seeked sessions end in the single run's state. Each took 35 s.

The host's swap was full during the campaign (4.0 of 4.0 GiB), held mostly
by the editor's file watcher (code-server, 1.4 GiB resident), not by the
campaign. 3.1 GiB of memory stayed available.

## 2026-11-10 — Visual campaign launched

The six Visual game folders matched the backup (`diff -rq`) and were
removed; `visual/index.html` and the manifests stay.

    run-long-gameplays.py launch --runner build-milestone/tests/LongGameplayProbe \
      --output build-milestone/long-games-visual-sram --fidelity visual \
      --memory-mib 2048 --save-at-frames --replay-window 1000 \
      --games 2048-long,sokoban-long,snake-long,pong-long,tetris-long,snake-leds-long

## 2026-11-11 — Visual campaign results

All six games passed (campaign `completed` at 11:10), with no setup
violations. Each has the FF Visual recording's frames and instruction
counts, and the same cycle count as the SRAM Compact recording:

| Game | Frames | SRAM cycles | FF host time | SRAM host time | Peak memory | Saves |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Sokoban | 73 | 886,046 | 1 h 50 min | 2 h 7 min | 307 MiB | 6.9 MB |
| Snake | 124 | 1,265,936 | 2 h 28 min | 3 h 8 min | 320 MiB | 11.9 MB |
| Snake (LEDs) | 124 | 1,277,592 | 2 h 33 min | 5 h 17 min | 320 MiB | 12.6 MB |
| 2048 | 85 | 2,503,400 | 5 h 26 min | 7 h 44 min | 297 MiB | 7.9 MB |
| Tetris | 145 | 2,837,961 | 6 h 16 min | 5 h 43 min | 314 MiB | 14.0 MB |
| Pong | 370 | 2,788,844 | 8 h 51 min | 6 h 8 min | 323 MiB | 35.2 MB |

In total 30.1 h at 107 cycles per second (Compact: 5.9 h, 548 cycles per
second). The host times vary with what else ran on the machine: another
session built and tested the chapter-6 accelerator in the same tree during
this campaign, and Snake with LEDs took 5 h 17 min against 3 h 8 min for
Snake. The campaign's memory guard (it pauses a game when the host has less
than 1 GiB available) stopped Tetris twice (00:36, 01:00) and Pong once
(09:01); each resumed from its last frame save in the same attempt, and the
finished recordings passed the same exact host-reference checks. The host's
swap stayed full, mostly held by the editor's file watcher (code-server,
1.4–1.7 GiB).

Published sizes: Compact 3.1 MB of frames and 34.5 MB of saves (37 KB per
save); Visual 33.9 MB and 88.8 MB (96 KB per save). The FF sets were 3.1/34.2
and 33.6/88.2 MB. § 5.6's tables were regenerated
(`tools/system/results-tables.py`; `--check` passes) and its size and speed
figures updated.

## 2026-11-11 — Checks on the published SRAM recordings

- `PublishedSavesTest.py --games pong-long 2048-long`: the saves of Pong and
  2048 at frame 1 open in `GameSessionProbe` for Compact and Visual, and the
  stepped, viewed, jumped and seeked sessions end in the single run's state
  (123 s, 370 MiB peak). The middle saves of Compact Pong and 2048 passed the
  same probe on 2026-11-10.
- Browser, on preview 8893: `GameReplayBrowserTest` with the published Pong
  (Visual: opened at frame 4 of 370 in 11 s, 60 event steps in 21 s; Compact:
  14 s, 7 s; memory readable, next frame, switching fidelity and back) and
  `GameSaveFallbackBrowserTest`: PASS.
- The FF sets remain in `build-milestone/ff-long-games-backup/` (not
  tracked).

## 2026-11-11 — Full native suite; closed

`ctest --test-dir build-milestone -j1` (616 tests, 2 h 26 min, under a
1,400 MiB memory cap; the build's sources are this record's state): 614
pass. The two failures are not this work's:

- `VisualizerScenarioCatalogTest`: 7 errors and 3 failures, all on the
  chapter-6 accelerator scenarios (`MacCellScenarioTest`,
  `MatrixUnitScenarioTest`, `AcceleratorMachineScenarioTest`, …) and the
  `accelerator` menu group that another session had added, uncommitted, to
  `visualizer-v2/static/scenarios.json`; the build does not contain those
  parts.
- `VisualizerColorPaletteTest`: the same session's accelerator types in
  `visualizer-v2/layout.json` have no palette colour.

Both tests pass on a clean checkout of this record's commit with the same
build. The record is closed: SRAM is the games machines' main memory, the
results and the long games are re-recorded on it and the pages say so.
Follow-up work, such as a faster bus (which would change the cycle counts
and so the recordings again), gets its own record.
