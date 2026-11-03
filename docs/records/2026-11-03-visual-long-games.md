# Long games recorded in Visual

Started: 2026-11-03. Status: open (prepared; campaign not started). Code
state: `alpha`, the commit that adds this record.

## Summary

The six long games are re-recorded with the core built from its stages
(the Visual preset), one machine state per frame, so a reader can open the
core's stages at any frame of a whole session. The recorder now stores
recordings gzip-compressed, which makes Visual affordable: a Visual frame is
about 1 MB of JSON and 16 KB compressed. A two-update Visual smoke campaign
passed every check and opens in the viewer. The full campaign is ready to
launch (about 25 hours at about 100 cycles per second); it has not started.

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
under 2.8 GB). Not yet pushed.
