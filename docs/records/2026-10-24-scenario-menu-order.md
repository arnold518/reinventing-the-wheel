# Complete scenario menu order

Started: 2026-10-24. Status: closed. Code state: `alpha` at `5549ee0`, retaining
the earlier pending component/layout/gameplay changes.

## Summary

The complete catalog and visible selector now run from foundations through
ALU, memories, RV32I/programs, RV32IM/programs and Device I/O, with developer
circuits last. The owner explicitly selected this overall order in place of
the earlier temporary RV32IM-first prefix. Five focused catalog checks,
strict packaging and the live 8893 WASM menu pass. Gameplay continues in its
frozen background queue.

## 2026-10-24 — Investigation and decision

The shared Python/JS option builders honored catalog group order and explicit
row ranks, but the browser's `menuOptions()` alphabetized rows again. Prior
tests compared backend option arrays without asserting the final visible
menu, so multiply/divide steps, ALU subparts, stages and other rows still
appeared out of sequence. Only the M/device groups had explicit row ranks;
other groups depended on registry insertion order or browser alphabetization.

Use one canonical group array and explicit component/program ranks for every
scenario. Follow widths numerically within families, put supporting blocks
and stages before completed cores/devices, and retain program numbering.
Use deterministic core/prediction tie breaks for program variants. The final
browser selector must preserve the already-ranked options. Preserve aliases,
test names, contracts, saved profiles/layout identities and waveforms. Native
FF device scaling belongs with FF technology rather than controller wrappers.

## 2026-10-24 — Implementation and focused catalog checks

The catalog now ranks all 197 component entries and 22 RV32I program
definitions across 26 groups. The selector has 219 visible rows; separate
core/prediction choices retain all 373 registered options. Groups run through
logic/wiring, ALU, memory helpers/technologies/interfaces/caches, RV32I
registers/ALU/blocks/single/baseline/prediction/advanced/memory systems,
programs 01–22, RV32IM, programs 23–27, Device I/O and developer circuits.
Native FF scaling moves beside the smaller FF device. No aliases, titles,
test IDs, layout/profile identities, circuit code or waveforms change.

Python and JavaScript now apply group/component/program ranks with explicit
core/prediction tie breaks. The final browser menu no longer alphabetizes
the result. Catalog tests assert numeric widths, stage/core order, dense
component ranks, registry-order independence, Python/JS parity and final
menu ordering, while retaining all legacy alias/core/prediction checks.
The device browser expectation follows the new overall prefix.

Five focused catalog methods pass in 13.766 s. The first attempt exposed a
test-setup omission: the native descriptor list includes `RewireValidationTest`,
which the server intentionally excludes from the menu. Filtering descriptors
through the existing visibility rule fixes that new check; production
classification is unchanged. A before/after comparison proves presentation
identities unchanged except the intended native FF regrouping and ranks.
The live-menu/site checks remain pending. No native rebuild, new simulation,
timing/layout regeneration or full suite is needed for this metadata change.

The long campaign remains active in its separate frozen service, using about
154 MiB with a 768 MiB hard limit and zero swap at the catalog check. Only the
preview service is stopped during asset staging/building; gameplay is untouched.

## 2026-10-24 — Site and live menu acceptance

This completes the site/live checks left pending above. Reuse the existing
WASM engine and generated examples, back up the two packaged catalog files,
and stage their current source versions. The strict preview-config site
build passes in 136.52 s (138.21 s process wall time), with 191,200 KiB
maximum RSS and zero swaps. The preview service's rebuild passes in 98.44 s.
Public viewer/device references and maintained test guidance describe the
selected overall order and the catalog-only refresh procedure.

A fresh Chromium session at `http://127.0.0.1:8893/simulator/` starts the
existing full-adder scenario in WASM mode. Compare every optgroup label and
every row's key/title against independently exported Python expectations:
all 26 groups and 219 rows match exactly, with zero page errors. Both served
assets match source SHA-256 values:

- `scenarios.json`: `846aa21fe296dd4880f6c0f94d56b398b1b0e3213b0aec8dd7a9ecdd5f8e58dc`
- `scenario_catalog.mjs`: `48c0931c147a9772cf8c941d7317be2f45398a2fdaf673db1a2569005c66a63d`

The persistent scenario browser check now compares the complete DOM menu
with its ordered option payload; both edited browser scripts pass Python
syntax checks. The focused live session verifies this menu behavior without
running the full core-switching or device browser suites. No complete native,
browser or WASM regression suite, engine rebuild or new game run is claimed.
Catalog logs, original metadata/package backups and the full observed menu
are retained in `build-milestone/scenario-menu-order/`.

Final service inspection confirms the long-game campaign remains active
with its original PID `3307552`, about 153 MiB cgroup memory, 768 MiB maximum
and zero swap. The preview is active on 8893. No gameplay worker, frozen
artifact, queued session, saved circuit layout or profile was changed.
