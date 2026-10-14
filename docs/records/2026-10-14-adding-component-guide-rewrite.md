# Rewrite the component creation guide

Started: 2026-10-14. Status: closed. Code state: `github-pages` at `448e4e3`, with concurrent uncommitted changes.

## Summary

The component guide now traces the current path from public contract through
structural and behavioral implementations, scenarios, proven delay tables,
frontend exposure and the public page template. Linked timing instructions
now use the complete clocked-table regeneration command.

## 2026-10-14 — Scope

The source, build scripts, tests and published-page conventions are authority.
The archived December 2025 walkthrough is historical context only. Preserve
other uncommitted work in the shared worktree.

## 2026-10-14 — Evidence and decisions

- Traced `Adder32` and `ALU32` from public headers and families through the
  built-in catalog, shared scenario runner, registry, CTest list, visualizer
  catalog and chapter 4.3 pages. Confirmed the inventory test requires one
  logical owner and registered evidence for each contract. Direct Python
  class bindings are optional; scenarios use the shared registry.
- Read `PartDelays`, catalog attachment, `CircuitTimingAnalyzer::delays`,
  `PartDelayTables.cpp`, its CTest entries, `DelayRuleTest`, and the MkDocs
  example exporter and Pages build script. The generator's `--write` skips
  clocked candidates unless `--clocked` is supplied. `--check` covers
  compiled default tables, while `--verify FILE` recomputes the complete
  generated file and compares the no-gate-list inventory.
- Replaced the old checklist with ordered implementation, test, timing,
  frontend and site-documentation instructions. Corrected the affected
  command snippets in the delay guide and timing reference as well.

## 2026-10-14 — Verification

Checked 37 local links in the working instructions and 12 source example
paths: all resolve. `git diff --check` passed. No component code, generated
table, public site page or scenario changed, so no C++ test, table generation,
WASM package or site build was run for this documentation rewrite.
