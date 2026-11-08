# Long games recorded in Visual

Started: 2026-11-03. Status: closed (2026-11-06; all six games published in Visual). Code
state: `alpha`, the commit that adds this record.

## Summary

The six long games are re-recorded with the core built from its stages
(the Visual preset), one machine state per frame, so a reader can open the
core's stages at any frame of a whole session. The recorder now stores
recordings gzip-compressed, which makes Visual affordable: a Visual frame is
about 1 MB of JSON and 16 KB compressed. A two-update Visual smoke campaign
passed every check and opens in the viewer. The first full campaign stopped at
2048 on an event budget sized for Compact; with the budget raised, all six
games were recorded again and passed every check (2026-11-06 04:47 KST,
about 25 hours of simulation; 34 MB published under `long-games/visual/`).
Replays from saved machine states continue in
[the replayable recordings record](2026-11-06-replayable-recordings.md).

## 2026-11-03 — Preparation

- **Compressed recordings.** `run-long-gameplays.py` now stores a published
  game's topology index, scopes, parts, shared layout and build manifest as
  `.json.gz` / `.txt.gz` (`compress_ready`) and rewrites their references and
  hashes; the playlist stays plain JSON. `recorded_circuit_transport.mjs`
  recognizes gzip by its first two bytes and decompresses with
  `DecompressionStream` (64 MB inflated limit), so it also works when a
  server has already decoded the file. `compress-published` converted the six
  Compact games in place: 58 MB → 3.5 MB; the originals are kept in
  `build-milestone/long-games-uncompressed-2026-11-03/`. Full-tick packages
  keep their own uncompressed contract.
- **Viewer menu.** The menu also reads
  `assets/system/long-games/visual/manifest.json` and lists published Visual
  games with a "· Visual" suffix.
- **Smoke fixture.** `smoke-long`: Pong's session cut to its first two
  updates (3 frames), expected report replayed through the real rules. The
  campaign runs it only when named in `--games`; `--publication` sends an
  equipment check outside the site. The six games' fixtures are byte-identical
  to those of the published Compact recordings (ELF and script hashes equal
  their evidence).
- **Visual smoke campaign** (`--games smoke-long --fidelity visual
  --memory-mib 2048 --publication build-milestone/visual-smoke-publication`):
  passed. 54,519 cycles in 548 s (99 cycles/s), peak native RSS 293 MiB;
  frames, report, inputs and button levels equal the host reference. The
  publication is 2.9 MB, almost all of it the one-time topology (graph
  0.4 MB, 52 scopes 2.2 MB compressed; the build manifest 6.9 MB raw before
  it was compressed too). `RecordedCircuitBrowserTest` passes on it (every
  pin and wire of every frame); the core opens with its stages
  (26,587 components, 60,332 wires).
- Also checked: `RecordedCircuitBrowserTest` on the compressed Compact
  Sokoban and Pong (24 parts).

- Full native suite (`ctest -j2`, 94 min): 608 of 611 passed. The three
  failures were stale checks of this work and were fixed and rerun: a color
  for the timer's new divider parts (`VisualizerColorPaletteTest`), the guest
  scenarios reported as logical tests of `ComputerSystemTest` (one logical
  test per contract, `BuiltinComponentCatalogInventoryTest`), and the CTest
  order of the new scenarios (`TestRegistryCoverageTest`). Rerun with the
  related catalog, profile and scenario checks: 9 of 9 pass. The complete
  browser suite was not rerun; the browser checks this work touches were run
  (see the game scenarios record).

Estimate for the campaign: 8.9 million cycles at about 99 cycles/s, about
25 hours of native time; about 2.6 MB of topology per game plus about 17 KB
per frame compressed, about 30 MB in all.

## Launch

```sh
python3 tools/rv32im/run-long-gameplays.py launch \
  --runner build-milestone/tests/LongGameplayProbe \
  --output build-milestone/long-games-visual-campaign \
  --fidelity visual --memory-mib 2048
python3 tools/rv32im/run-long-gameplays.py status --output build-milestone/long-games-visual-campaign
```

It publishes under `site-docs/assets/system/long-games/visual/`. Afterwards:
add the Visual rows to § 5.6, rerun `RecordedCircuitBrowserTest` on one
Visual game, close this record.

## 2026-11-04 — Pages workflow failure after the push

The Pages workflow for `acaa085` (run 41647256071) built the site but failed
its smoke browser tests: `RV32IBuildingBlocksBrowserTest` reported a console
error, "Failed to load resource: 404", and the runner stopped after 4 of 14
checks. Reproduced locally; the 404 was the viewer menu requesting
`assets/system/long-games/visual/manifest.json`, which exists only once a
Visual campaign has been launched (every simulator page asks for it).

Fix: the menu reads `assets/system/long-games/recordings.json`, an index of
the published recording sets that always exists (now: the Compact set);
`run-long-gameplays.py launch` adds its set to the index when it publishes
under `long-games/`. The native server (no site assets) requests nothing.
After the fix the page loads with no failed request, and the whole smoke
suite passes locally on 8893 (14 of 14; the first local attempt at the last
ten was killed by a 2.2 GB memory cap of this host, not by a test, and passed
under 2.8 GB). Pushed as `96ef121`; its Pages run (41682914510) built,
passed the smoke checks and deployed.

## 2026-11-04 — Campaign launched

Launched on the owner's go-ahead at 04:09 KST, with the command above, as
systemd user unit `circuitsim-long-games-1793732990` (2 GiB memory cap, no
swap). Runner `build-milestone/tests/LongGameplayProbe`, SHA-256
`a65179c6…5b27eb`, frozen into the campaign folder. Before starting, the
host reference passed for all six games (Sokoban 73 frames, Snake and Snake
LEDs 124, 2048 85, Tetris 145, Pong 370). Order: Sokoban, Snake, Snake LEDs,
2048, Tetris, Pong. The launch added the Visual set to `recordings.json`;
the viewer skips it until its manifest lists finished games.

## 2026-11-04 — First campaign stopped on the event budget; relaunched

Sokoban (673,842 cycles in 6,647 s, peak 293 MiB), Snake and Snake LEDs
passed every check. 2048 failed at 1,122,304 cycles (54 of 85 frames) with
"Simulation event budget exhausted": long sessions allowed 20 billion
simulator events, which covers Compact (about 300 events per cycle) but
Visual only to about 1.1 million cycles (about 17,800 events per cycle).
Tetris (2.15 M cycles) and Pong (2.18 M) would have failed the same way, so
the unit was stopped at 16:47 during Tetris (962,560 cycles); nothing
recorded was wrong.

Fix: `src/runtime/RecordedGuestFixtures.cpp` raises the long-session event
budget to 400 billion, enough for the 20 million cycle budget in Visual.
The runner was rebuilt (`LongGameplayProbe`, SHA-256 `bdc73886…11d9222`).

The worker refuses a published game made by another runner build, so the
owner chose to rerun all six from one build rather than keep the three
finished games. Those were moved to
`build-milestone/retired-visual-partial-2026-11-04/` (15 MB compressed for
three games). Relaunched at 16:51 KST as
`circuitsim-long-games-1793778661`, output
`build-milestone/long-games-visual-campaign-2`; the host reference passed
again for all six games.

## 2026-11-05 — Second campaign: Sokoban passed; Snake delayed by host memory

Sokoban passed again with the new runner (673,842 cycles in 7,704 s, peak
286 MiB). The first Snake attempt was stopped at 86,016 cycles at about
19:18 by the memory guard (host available memory below 1 GiB, from
processes outside the campaign); the retry then waited until 00:11 for
1.5 GiB to be available before starting (no OOM in the journal). Snake's
second attempt was at 692,224 of 966,300 cycles at 02:03, running at 65–90
cycles per second beside an editor file watcher using a full core.

## 2026-11-05 — Second campaign: five of six games passed

By 22:05 KST Snake (966,300 cycles, 9,701 s), Snake LEDs (975,228, 9,466 s),
2048 (1,954,776, 18,905 s) and Tetris (2,147,750, 19,850 s) had passed every
check, peaks 292–295 MiB; 2048 and Tetris ran past the old 1.1 million
cycle limit, so the raised event budget works. Snake LEDs' first attempt
was stopped at 802,816 cycles at 02:54 by the memory guard and restarted
after waiting until 05:45. Pong was running at 1,064,960 of 2,180,301 cycles
(181 of 370 frames).

## 2026-11-05 — Manual reboot during Pong; resumed

The owner rebooted the host at about 22:14 KST while Pong was at 1,093,632
of 2,180,301 cycles (185 of 370 frames). `run-long-gameplays.py resume` at
22:18 re-verified the five published games' hashes, skipped them, and
restarted Pong from cycle 0. The runner cannot resume mid-game: a part's
checkpoints hold pin and wire values for the viewer, not the simulator's
full state (pending events, memory and register contents of behavioral
parts), and the simulator has no state save and restore. Adding one would
also change the frozen runner, which the campaign refuses to mix with the
published games.

## 2026-11-06 — All six games passed; record closed

Campaign `build-milestone/long-games-visual-campaign-2`, unit
`circuitsim-long-games-1793778661`, completed 04:47 KST. After the manual
reboot Pong restarted from cycle 0 (22:18) and passed.

| Game | Cycles | Simulation | Peak memory |
| --- | ---: | ---: | ---: |
| Sokoban | 673,842 | 2 h 8 min | 287 MiB |
| Snake | 966,300 | 2 h 42 min | 293 MiB |
| Snake LEDs | 975,228 | 2 h 38 min | 296 MiB |
| 2048 | 1,954,776 | 5 h 15 min | 293 MiB |
| Tetris | 2,147,750 | 5 h 31 min | 293 MiB |
| Pong | 2,180,301 | 6 h 25 min | 293 MiB |

Every game passed the full-frame, report, input-log and per-frame
button-level checks against the host reference. Published: 34 MB under
`site-docs/assets/system/long-games/visual/`, listed in `recordings.json`.
§ 5.6 has the Visual table (`results-tables.py`, marker `long-games-visual`).
`RecordedCircuitBrowserTest` on the Visual Pong recording (preview 8893):
PASS (all checkpoints, pins and wires, display and keyboard, inspector,
playback).

Record closed 2026-11-06.
