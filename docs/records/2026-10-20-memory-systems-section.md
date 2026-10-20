# RV32I memory systems section and the split of the cache experiments

Started: 2026-10-20. Status: closed (2026-10-21). Code state: branch `realistic-timing` at
`60acda2`.

## Summary

The owner found that §3.8 read as a history (experiment 3 introduced as
what §3.7 "now" offers) and asked whether the program experiment over the
five memory arrangements belongs to the cache part. Decision: it belongs to
the RV32I chapter. The arrangements are systems built around the core, not
parts. A new §4.7 Memory Systems describes the five arrangements with a
visualizer scenario each. The former §4.7 results page becomes §4.8 and gains
one memory-systems section that merges the former experiments 2 and 3 as
final results. §3.8 keeps only the cache-alone experiment (access patterns).

## 2026-10-20 — Assessment and plan

- §3.8 problems found: the intro said "two experiments" over a list of three;
  experiment 3 opened with "§3.7 now offers other behaviors"; "In short"
  told the story in the order of the work; the setup table and the topology
  caption described write-through as the cache behavior.
- The three experiments: experiment 1 (scripted reads, no processor)
  isolates the cache's mechanics; experiments 2 and 3 are one experiment,
  programs over arrangements × cache behaviors, split only because they
  were run at different times. None is a core experiment; the core is held
  fixed as a request source.
- Placement: §3.5.3 keeps the arbiter as a component; §4.7 shows how it is
  connected in each arrangement, with scenarios: A is the advanced core's
  program-21 scenario, C-ID the existing `rv32i-shared-memory`, and three
  new scenarios for B, D-0 and D-ID. All run program 21 over the same
  flip-flop memory with small gate-level caches, so they load quickly and
  stay readable.

## 2026-10-21 — Done; record closed

**Code.**
- `RV32IProgramRoot` builds every shared arrangement. Each side reaches the
  arbiter through its L1 or directly, and an optional `SHARED_CACHE` (L2)
  sits between the arbiter and `SHARED_MEMORY`. `wireMemorySystem` takes a
  port prefix per side, so the core can connect straight to the arbiter's
  `I_` and `D_` ports.
- `RV32IMemoryArrangement` and `rv32iArrangementCaches` give the scenario
  caches: 4-set L1s, and a 4-set two-way L2.
- New registered scenarios: `RV32IMemoryArrangementBTest`, `…D0Test` and
  `…DIDTest` (aliases `rv32i-memory-b`, `-d0`, `-did`). They have no `/` in
  their names, because the catalog treats such names as program variants.
- The memory inspector shows one panel when a shared cache sits between the
  arbiter and the memory, since the grant no longer tells the sides apart.
- `scenarios.json` gains the group "RV32I / Memory systems" (with C-ID). The
  program-root floorplan covers the arrangements without L1s and with an L2;
  the saved C-ID floorplan is unchanged.

**Docs.**
- New §4.7 Memory Systems, with one part and one embedded scenario per
  arrangement, the address map, the two-panel view and its limits, and the
  tests.
- The results page becomes §4.8. Its memory-systems section merges the
  former experiments 2 and 3 as final results: setup, a speed table for A,
  one table of arrangements × behaviors, findings with the best system
  first, and every run's cycles.
- §3.8 keeps the access-pattern experiment only.
- Navigation, cross-references and §4.7 → §4.8 references are updated. The
  §4.8 figures move to `site-docs/rv32i/results/`.
- `cache-speedup` now shows the caches with a write buffer (bar scale 72 px
  per ×); `cache-topologies` moves to §4.7.
- All numbers were generated from the campaign data and the published A/B
  rows. Checking the findings against the tables corrected three claims
  before publication: the 4.0–5.4× range, "C-ID and D-0 within 3–9 % with
  write-through or a buffer" (not with any behavior), and the
  `pointer-chase` wording.

**Layouts.** Each new scenario's root placements were computed with the
floorplan and saved; the default session had kept the program root's
separate-memory placements and overlapped the parts. The browser
screenshots of B and D-ID show the parts left to right.

**Checks (all pass).**
- The four arrangement scenarios against the instruction-set reference.
- Native: `MemoryInspectorIntegrationTest` (new roles check: B two panels,
  D-0 and D-ID one), `VisualizerStaticLayoutTest`,
  `VisualizerScenarioCatalogTest`, `RV32IMemoryLayoutTest`, `CacheLayoutTest`,
  `VisualizerAutomaticLayoutTest`, `VisualizerSessionContractTest`,
  `VisualizerModuleBindingsTest`, `RV32ITopologyTest`, `RV32IBinaryClockCLITest`,
  `RV32IFiveStageCoreProgramTest/program-21`,
  `BuiltinComponentCatalogInventoryTest`, and the 18 documentation checks.
  The §4.8 plot script validates its 602 table cells.
- Pages build (strict).
- Browser: the three new scenarios load in WebAssembly with the expected
  parts and inspector roles; `DocumentationDiagramsBrowserTest` (after the
  figure scale fix), `RV32IDocumentationBrowserTest`,
  `VisualizerScenarioBrowserTest`, `VisualizerScenarioWasmParityTest`,
  `MemoryInspectorBrowserTest`, `MemoryInspectorWasmParityTest`,
  `MemoryDocumentationBrowserTest`, `VisualizerWasmBrowserTest`,
  `RV32IAdvancedBrowserTest`, `RV32IMemoryBrowserTest`,
  `MemoryEndpointBrowserTest`.

**Not run:** the full native suite.

**Incident.** The build waited about an hour for memory. `/tmp` (the
scratchpad) is RAM-backed and held 1.6 GB of old scratch output, which was
deleted; the owner's code-server file watcher had grown to 1.5 GB.
