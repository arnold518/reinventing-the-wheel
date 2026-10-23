# RV32IM scenario naming and fidelity audit

Started: 2026-10-23. Status: closed (audit complete; presentation changes not requested).
Code state: main `alpha` at `41dc41b`; initial tracked worktree clean.

## Summary

All ten RV32IM menu scenarios hard-code the root instance name `DUT`. Their
shared `phase3-recorded` profile prefers behavioral fidelity at every path,
falling back to the sole available implementation. Every scenario has a
structural root implementation. Only `muldiv-w32` and `rv32im-core` have a
behavioral root, and both default to that single component. Other roots are
structural with compact children selected where available. Native probes for
all ten scenarios and fresh-browser expansion checks for both compact roots
confirm that structure is available. The shared compact-first profile and
generic naming make these defaults poorly suited to component inspection.
No scenario, public contract or UI behavior is changed.

## 2026-10-23 — Scope and source findings

The owner asked why the RV32IM scenarios show `DUT` and mostly behavioral
blocks in the existing scenario visualizer. Read the current status/index,
open foundation/game records, public RV32IM pages, scenario specs, shared
component runner, native session, explorer and WASM profile application.

`Sequence` in `src/tests/RV32IMPhase3Scenarios.cpp` explicitly constructs every
spec with `instance_name="DUT"`. This means device under test; it is separate
from the component type. `ComponentTestRunner::run` passes that name directly
to the catalog root request. The explorer displays `component.name`, so the
fixture label appears on both the canvas and explorer. This is not an ISA or
component-contract requirement. The eleven device component scenarios share
the same helper; the separate integrated system spec also uses `DUT`.

`scenarioProfile()` appends a behavioral rule for `ProfileSelector::any()`
and permits sole-implementation fallback. `ComponentScenarioTest::setupCircuit`
honors that explicit decision. Consequently this rule replaces the normal
structural-first catalog choice at every selectable path. The ten root
contracts and their source types all retain structural factories; compact
root forms are registered only for 32-bit MulDivUnit and the full IM core.
Those compact forms run their own gate lists with characterized state.

## 2026-10-23 — Fresh native scenario inventory

Built no new code. Ran the current `VisualizerSessionProbe topology` serially
for each of the ten menu scenarios, using no saved overrides. All root IDs
and names are `DUT`.

| Scenario | Default root | Components | Important compact descendants |
| --- | --- | ---: | --- |
| multiply-step | structural | 43 | None |
| divide-step | structural | 60 | None |
| multiplier-w32 | structural | 875 | Registers and prefix adders |
| divider-w32 | structural | 1428 | Registers and prefix adders |
| muldiv-w32 | behavioral | 1 | Root hides the assembly |
| rv32im-decode-control | structural | 382 | INTEGER decoder |
| rv32im-decode-stage | structural | 638 | REGISTER_FILE and HAZARD |
| rv32im-execute-stage | structural | 1354 | INTEGER executor and MULDIV |
| rv32im-coordinator | structural | 87 | INTEGER coordinator |
| rv32im-core | behavioral | 1 | Root hides the pipeline |

Exact `DUT=structural` overrides succeed: MulDivUnit becomes 2,715 components;
the IM core becomes 3,929. These expand the root only; the shared behavioral
rule continues selecting compact descendants. Native topology and selected
child metadata are retained in `/tmp/rv32im-scenario-audit.json` and
`/tmp/rv32im-scenario-expanded-audit.json`; the key results are copied here.

The explorer enables its fidelity arrow only when both implementations exist
and the profile is editable. The arrow records an exact-path draft; Apply
rebuilds and simulates it. This does not recursively expand a subtree. WASM
also stores per-scenario overrides in IndexedDB, so an existing browser may
show choices beyond the fresh defaults. Browser verification is pending.

## 2026-10-23 — Browser verification and assessment

Fresh Playwright contexts against the actual port-8893 WASM viewer reproduce
both compact root defaults, each named `DUT`, with a single visible component.
Both expose structural/behavioral metadata and an enabled triangle titled
"Expand into structural fidelity". Clicking it and pressing Apply succeeds:

| Scenario | Before | After exact root expansion |
| --- | ---: | ---: |
| muldiv-w32 | 1 behavioral component | 2,715 components, structural root |
| rv32im-core | 1 behavioral component | 3,929 components, structural root |

There are no page errors in these two checks. The expanded core still has
compact FETCH, pipeline registers, MEMORY, WRITEBACK, EXECUTION_STATUS and
PREDICTION children; DECODE, EXECUTE and COORDINATOR are structural. The
expanded MulDiv has structural MULTIPLY/DIVIDE and compact OPERATION state.
This agrees with native exact-path probes. The browser report is retained at
`/tmp/rv32im-visualizer-browser-audit.json`; those important results are above.

Findings and recommendations for a subsequent implementation topic:

1. Replace test-fixture root labels with descriptive component names. Migrate
   affected exact profile paths, saved layouts and generated example identity
   alongside the names; changing a menu title alone will not rename the root.
2. Give inspection scenarios a structural root and visible named functional
   children. Keep deeper compact storage/arithmetic choices where useful for
   bounded recording size and responsiveness. A global behavioral preference
   is suitable for batch throughput, but hides the hardware being inspected.
3. Preserve selectable compact forms and explicit per-component overrides.
   Root expansion must not be described as recursively expanding descendants.
4. The standalone core scenario still records reset/fetch backpressure only;
   a running-program scenario is a separate missing presentation feature.

This is a completed read-only source/runtime audit with automatic record and
status updates. No production source, profiles, scenarios, layouts, package
or service changed. No compilation, full suite or all-structural full-core
run was needed. Browser verification covers the two compact roots, while
native metadata/recorded topology covers all ten RV32IM menu scenarios.
