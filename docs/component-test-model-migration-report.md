# Component Test Model Migration Report

Date: 2026-07-25

Branch: `unified-component-migration`

## Result

The migration now follows one rule throughout construction and testing:

> One recursive `BuildProfile` selects the root and every selectable
> descendant.

There is no separate root-fidelity argument or descendant profile. Each public
component contract has one logical test scenario, and that scenario runs every
fidelity actually provided by the catalog.

## Selection simplification

Removed:

- `createRootExact`;
- `ComponentRunConfiguration`;
- child/descendant-profile fields;
- descendants-only selectors;
- profile rule priorities and specificity contests;
- polymorphic profile-generator classes;
- named preset lookup;
- reference-only implementation state; and
- profile names/reasons from topology fingerprints.

The remaining model is:

1. Construct a `BuildProfile`.
2. Call `ComponentCatalog::createRoot(request, profile)`.
3. Let `BuildContext` carry that same profile recursively.
4. Resolve ordered rules with “last matching rule wins.”
5. Record every actual choice in `BuildManifest`.

Convenience functions in `StandardProfiles.hpp` return ordinary profile
values. They are not a parallel selection system.

## RV32I correction

The RV32I core and system are now complete two-fidelity families with one
family-local pin definition and both family-backed factories.

The structural system previously cast its selected `CORE` child to
`RV32ISingleCycleCore`. A profile could select the behavioral core, but the
cast then produced null and architectural state observation failed.

The fix adds `RV32IStateView`. Both core implementations and both system
implementations provide it. The structural system holds the selected core as
an `IOComponent` and observes state through the capability.

`RecursiveBuildProfileTest` now builds:

```text
system                   structural
└── CORE                 behavioral
```

and verifies that the wrapper can observe the selected core.

## Component test runner

`ComponentTestRunner::run()` now accepts one `BuildProfile`.

`runAll()` starts from the caller’s base profile and appends an exact-path DUT
rule for each available fidelity. Descendant choices stay in the same profile.
Each run receives:

- an independent component tree;
- an independent simulator;
- its exact profile;
- its build manifest; and
- semantic checkpoints containing every public output pin.

Each run checks independent expected outputs. Cross-fidelity comparison then
aligns checkpoints by stable ID and checkpoint kind. Numeric settle times may
differ.

The selected DUT fidelity in `RunArtifact` is read from actual root metadata,
not copied from a side configuration.

## Registry and Python

The registry now explicitly records whether a test has a meaningful circuit
for visualization.

Python no longer binds every C++ test class separately. It exposes:

```text
get_registered_test_names()
get_registered_test_descriptors()
create_test_by_name(name)
```

The visualizer uses the registry factory for every ordinary and parameterized
scenario. Explicit `visualizable` metadata replaces the accidental old rule
that “bound Python class means visualizable.”

## Layout regeneration

The final profile fingerprint excludes human profile names and rule reasons.
It changes only for topology-affecting policy.

Default layouts were regenerated after the fingerprint and registry changes:

| Metric | Value |
|---|---:|
| Visual scenarios | 110 |
| Representative topology builds | 95 |
| Traversed component instances | 217,804 |
| Unique parent topologies | 178 |
| Reusable type layouts | 131 |
| Profile-specific layouts | 0 |
| Root layouts | 110 |

All generated parents passed overlap validation. The sixteen RV32I program
scenarios received root layouts using the final profile fingerprint.

## Verification

Build:

```text
cmake -S . -B build
cmake --build build -j4
Result: passed
```

Focused selection/layout gate:

```text
BuildProfileTest
ComponentCatalogSelectionTest
BuiltinComponentCatalogInventoryTest
RecursiveBuildProfileTest
ComponentTestModelTest
VisualizerModuleBindingsTest
TestRegistryCoverageTest
Result: 7/7 passed in 9.84 seconds
```

Complete regression:

```text
ctest --test-dir build --output-on-failure -j2
Result: 125/125 passed
Wall time: 232.27 seconds
```

This includes all sixteen
`RV32ISingleCycleSystemTest/program-XX` scenarios.

The port-8765 visualizer worker was then relaunched through its systemd restart
policy. Its health endpoint passed, and its scenario API exposes the unified
registry names rather than the removed fidelity-specific names. A live
`rv32i-program9` request returned its 54,049-component topology under layout
key `rv32i-program9@66d12149b4bd5ced`, and the health endpoint remained green.

## Completed visualizer follow-up

Interactive fidelity editing now uses the same reusable scenarios and
recursive profiles described by this migration:

- the component explorer expands structural components and collapses
  behavioral components;
- `Apply & Run` rebuilds the same scenario with sparse exact-path overrides;
- simulation sessions keep topology and time-travel state together;
- profile persistence is atomic and revision checked; and
- large RV32I circuits load their exact pin/wire drawing scopes
  progressively.

See `docs/visualizer-v2-profile-explorer.md` and
`docs/rv32i-visualizer-performance-report.md`.
