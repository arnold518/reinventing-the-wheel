# Test and scenario workflow guide

Started: 2026-10-16. Status: closed. Code state: `alpha` with the guide uncommitted at closure.

## Summary

The source-grounded test and scenario guide is linked from AGENTS.md and the guides index. Focused native, full local Pages build and seven relevant web checks passed.

## 2026-10-16 — Investigation and decision

The registry in `src/tests/TestRegistry.cpp` serves CTest, the Python binding and WebAssembly. `tests/CMakeLists.txt` lists CTest names, `visualizer-v2/static/scenarios.json` owns aliases and menu groups, and public example markers are generated from scenario checkpoints. The existing `docs/reference/test-organization.md` describes organization but is not a step-by-step working guide.

The new guide explains how to find coverage, run a focused native check, distinguish settled scenarios from timing tests and campaigns, and validate site/WASM changes. AGENTS.md now directs agents to read and maintain it.

## 2026-10-16 — Verification

Pending final documentation and site checks.

## 2026-10-16 — Focused guide validation

The guide was checked against the registry, CTest wiring, scenario catalog, example markers and Web test inventory. The missing explicit registry classification for the newly renamed native-device scenario was fixed while following the guide; `VisualizerScenarioCatalogTest` passed. `MemoryEndpointIntegrationTest` compiled and ran the documented examples, and all 17 focused native checks passed. The full Pages/WASM check is still pending.

## 2026-10-16 — Site packaging

The expanded focused native run passed 28 CTests, including the registry, documentation-example and visualizer scenario checks. `tools/build-pages.sh build-wasm` packaged the WebAssembly simulator, generated scenario example rows and completed strict MkDocs successfully. Browser verification is still running; the full native suite was not run.

## 2026-10-16 — Closure

The focused web runner passed 7/7 relevant browser and WASM parity checks. This replaces the preceding pending-browser status. The guide and its links are complete; the full native suite was not run.
