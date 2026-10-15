# Retire the memory container component

Started: 2026-10-16. Status: closed. Code state: `alpha` with the memory refactor uncommitted at closure.

## Summary

The redundant memory container component is retired. Scalable endpoints use a visible controller and the per-technology behavioral native device; retired contract IDs resolve to the native device contract. Focused native, Pages and web checks passed. Full CTest coverage and the remote published site were not verified.

## 2026-10-16 — Investigation and decision

`MemoryContainer` only selected an existing `memory.<technology>.device.<width>.behavioral` implementation and changed its display type. It introduced no storage or timing behavior. The structural endpoint previously selected `memory.<technology>.scalable-device.<width>` to keep storage compact. The native behavioral device already has the same pins and supports sparse large capacity. Existing tests compare device fidelities, endpoints and large sparse capacities.

Retire the distinct component and contract. The scalable endpoint explicitly requests behavioral fidelity for its native device child while its controller remains selectable. Old `scalable-device` contract IDs resolve to the native device contract for saved profiles and callers. The old visualizer URL remains an alias; active documentation uses the new native-device name.

## 2026-10-16 — Changes and verification

Pending final test results. Code, public memory pages, diagram source and generated figure, bindings and scenario names are being updated together. No claim of full-suite or browser verification is made yet.

## 2026-10-16 — Focused native validation

The container factory, display override and distinct `scalable-device` contracts were removed. Each scalable endpoint now asks for the behavioral implementation of the existing native device contract, while the controller remains visible and selectable. A parent-required fidelity field was added to the catalog request; retired device IDs canonicalize to the native contract. The Python factory is now `create_memory_device`. Public memory pages, diagrams, binding and scenario names were updated together.

The first focused run found missing visualizer registry metadata for `NativeMemoryDeviceTest` and a strict C++ example warning from the new aggregate field. Both were corrected. The serial `build-cleanup` build then passed; the 17 focused CTests covering cleanup, catalog selection, all five endpoints, native-device parity, timing, docs examples and visualizer integration passed in 52.03 seconds. Static JSON validation, diagram generation check, eight diagram tests and `git diff --check` passed. Delay-table regeneration and Pages/WASM/browser validation are still running; no full-suite or published-site claim is made yet.

## 2026-10-16 — Expanded native and site validation

The proven part-delay tables were regenerated for the current catalog. `part_delay_tables --verify` found 601 identical table entries and an identical gate-list inventory; coverage reports 157 compact implementations with a proven table and two unchanged ROM cell/row exceptions. The serial native build passed, followed by 28 focused CTests in 231.34 seconds, including memory endpoint, cache, RV32I memory-migration, scenario-catalog and documentation-example checks. This is focused coverage, not a full CTest run.

`tools/build-pages.sh build-wasm` completed successfully. It packaged the WebAssembly visualizer, generated 52 verified `native-memory-device` example rows and completed a strict MkDocs build. The browser and WASM parity checks are still running. The remote published URL is outside this local verification.

## 2026-10-16 — Closure

The focused web runner passed 7/7 memory documentation, endpoint, inspector and scenario browser/WASM parity checks against the packaged local site. This replaces the preceding pending-browser status. Local implementation and documentation verification are complete within the stated focused coverage; the record is closed. The full CTest suite and live Pages URL remain outside this validation.
