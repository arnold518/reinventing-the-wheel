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

## Coverage hardening follow-up

Date: 2026-07-29

The post-migration audit checked the complete built-in component catalog
against the test registry. Every registered component contract still has one
logical test; the weakness was the depth of several scenarios, not missing
test registration.

The shared truth-table adapter now accepts exact vectors of `LOW`, `HIGH`,
`UNKNOWN`, and `HIGH_Z`. This lets an ordinary component scenario drive and
check four-state buses without a private test harness.

Coverage added by the audit:

- binary gates now exhaust all 16 pairs of four-state inputs;
- the half adder and full adder exhaust 16 and 64 four-state rows
  respectively;
- every splitter, joiner, mux family, decoder, 8-bit arithmetic block, and
  32-bit ALU sub-block has representative unknown/high-impedance propagation
  and controlling-value cases;
- latches, flip-flops, registers, register files, and memory arrays check
  high-impedance capture, ambiguous control/address behavior, reset recovery,
  and exact four-state words;
- `RV32IBitPatternMatcher` checks unknown/high-impedance bits in both masked
  and significant positions;
- `RV32IControlFlowUnitTest` covers every branch decision, JAL, JALR,
  alignment, hold, reset, and 32-bit PC wraparound;
- `RV32IDecodeControlUnitTest` uses 40 legal instructions, 12 deliberately
  illegal encodings, and 64 deterministic raw words;
- `RV32IExecutionControlStatusUnitTest` covers all four encoded sizes at all
  four low-address offsets, every trap/halt source, stalls, fault-response
  gating, sticky state, priority, and reset recovery; and
- the instruction oracle, lockstep harness, sixteen program scenarios, and
  recursive profile-toggle sweep were reviewed and retained as the
  higher-level coverage layer. They already compare every committed
  architectural state and memory effect rather than sampling only final pins.

The new cases exposed three real cross-fidelity defects. The behavioral
`AddSub32`, `Comparator32`, and `Shifter32` implementations previously
collapsed partially knowable results to all-unknown, or preserved
high-impedance where their structural gate networks produced unknown. Their
behavioral evaluators now model the same four-state ripple/mux semantics as
the tested structural contracts.

The audit also corrected the `RegisterFile32x32` catalog semantic domain from
`known-binary` to `four-state`, matching its implementations and test
contract.

Follow-up verification:

```text
cmake --build build -j2
Result: passed

ctest --test-dir build --output-on-failure -j2
Result: 127/127 passed
Wall time: 346.08 seconds
```

This regression includes all sixteen
`RV32ISingleCycleSystemTest/program-XX` scenarios and the complete
`RV32IProfileToggleSweepTest`.

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
