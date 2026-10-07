# Component Test Model Plan

Status: implemented and verified

Last updated: 2026-07-25

## Goal

Each public component contract owns one logical test, regardless of how many
implementation fidelities it has.

The test owns the scenario: inputs, checkpoint meanings, and expected public
observations. The selection system owns construction. A test must not need a
separate structural class, behavioral class, or fidelity-specific scenario.

## Core rule

For an ordinary component:

```text
one component contract
└── one logical test
    ├── one shared scenario
    ├── structural run, if available
    └── behavioral run, if available
```

Each run:

- uses its own component tree;
- uses its own simulator;
- receives one complete recursive `BuildProfile`;
- checks independent expected outputs; and
- produces immutable checkpoint observations.

Afterward, the runner compares public snapshots between runs.

If a contract has only one fidelity, the one run is still checked against its
expected result and the cross-fidelity comparison is simply absent.

## Why one profile is enough

The caller supplies a base profile for the whole experiment. For each DUT run,
the runner appends an exact-path rule selecting that DUT fidelity:

```cpp
auto run_profile = circuit::withExactFidelity(
    base_profile,
    spec.instance_name,
    fidelity);
```

That remains one ordinary recursive profile. All descendant rules in the base
profile still apply. There is no separate root-fidelity field, child profile,
or effective-fidelity calculation.

This also makes a future visualizer editor straightforward: edit one profile,
rebuild the scenario, and observe the resulting manifest and timeline.

## Observational testing

Tests should drive inputs and observe normal circuit state. They should avoid
special test-only state mutation after setup.

At each logical checkpoint, the runner captures every public output pin as a
four-state vector. Values retain `LOW`, `HIGH`, `UNKNOWN`, and `HIGH_Z`.

Time travel remains useful for the visualizer and debugging, but comparisons
do not rely on identical numeric times. Structural and behavioral
implementations may settle at different times.

Each checkpoint contains:

- stable checkpoint ID;
- semantic checkpoint kind;
- actual local simulator time; and
- public output snapshot.

Comparison aligns checkpoints by ID and kind, not by absolute time.

## Checkpoint types

### Truth-table components

Use `Settled(row-id)`.

For each row:

1. drive all row inputs;
2. drain the event queue until idle;
3. observe all public outputs;
4. check the row’s expected outputs.

`TruthTableComponentTest` adapts existing `TestRow` tables to this model.

### Sequential components

Use named `AfterEdge` or `Settled` checkpoints.

The scenario explicitly drives reset, clock, enables, and data. It observes
after the relevant edge has propagated and the queue is idle.

A free-running clock is the exception: it never becomes idle, so
`ClockGeneratorTest` keeps a specialized absolute-time timing scenario.

### Memory components

Use `TransactionComplete` checkpoints.

The scenario drives address, data, size, enables, and clock as ordinary pins,
then observes read data, ready/fault outputs, and declared state-view
capabilities where the public contract includes them.

Representative structural memory slices remain separate components with their
own tests; the full `Memory64Kx32` behavioral abstraction is checked against
those lower-level contracts and its own memory cases.

### Program components

Use `InstructionCommit` checkpoints.

Program tests are not ordinary “both sides against a pin table” tests. The
behavioral RV32I system is the executable answer sheet. Therefore:

1. run the answer sheet independently;
2. compare every committed instruction with the independent instruction
   oracle;
3. check hard-coded final program outcomes;
4. run the selected structural/profiled system independently; and
5. compare commit observations with the answer-sheet trace.

The sixteen programs are scenario instances of one logical system test.

## Test data structures

`ComponentTestSpec` identifies:

- logical test ID;
- contract ID;
- DUT instance name;
- parameters; and
- scenarios.

`ActionScenario` contains ordered `ScenarioAction` entries and bounded run
limits.

`ScenarioAction` contains:

- checkpoint ID and kind;
- input vectors;
- expected public outputs;
- whether to emit a checkpoint; and
- human detail.

`RunArtifact` contains:

- selected root fidelity, read from build metadata;
- root component;
- simulator history;
- exact profile;
- build manifest;
- checkpoints; and
- completion state.

## Safety and determinism

Every action uses:

- a maximum allowed simulator time;
- a maximum event count; and
- `Simulator::drainUntilIdle`.

A broken oscillating circuit reports deadline or event-limit failure rather
than hanging.

Checkpoint times within one run must increase strictly. Numeric times need not
match another fidelity.

## Registry and CTest

The registry stores:

- concrete CTest/scenario name;
- logical test ID;
- test kind;
- owned contracts;
- scenario ID;
- labels; and
- factory.

CTest keeps individual program scenario entries for parallel execution and
clear failures:

```text
RV32ISingleCycleSystemTest/program-01
...
RV32ISingleCycleSystemTest/program-16
```

Names do not encode structural, behavioral, balanced, hybrid, reference, or
equivalence implementation mechanics.

Python and the visualizer construct every scenario with:

```python
circuit_backend.create_test_by_name(name)
```

Individual test classes are not a second Python registry.

## Ownership of correctness

Each component test assumes independently tested children satisfy their own
contracts. The parent test checks the parent’s public behavior, including its
wiring and composition.

Cross-fidelity equality is useful but is not sufficient by itself: two
implementations can share the same mistake. Therefore ordinary component runs
also check independent truth-table rows, expected transactions, semantic
rules, or program outcomes as appropriate.

## Implemented migration

- [x] Add `ComponentTestSpec`, scenarios, actions, and artifacts.
- [x] Add bounded event-queue draining.
- [x] Capture all public output pins automatically.
- [x] Align comparisons by semantic checkpoint identity.
- [x] Run available fidelities in independent simulators.
- [x] Use one base profile plus an exact DUT override.
- [x] Remove `ComponentRunConfiguration`.
- [x] Remove separate child/descendant-profile APIs.
- [x] Consolidate fidelity-specific component tests.
- [x] Consolidate RV32I programs under one logical system test.
- [x] Store test metadata and factories in the C++ registry.
- [x] Make the visualizer use the registry factory.
- [x] Regenerate layouts after the final profile fingerprint change.
- [x] Pass all 125 tests on the final source state.

## Future visualizer work

Interactive profile editing is intentionally separate. The test model already
provides the required foundation:

- one scenario definition;
- one recursive profile;
- reproducible build manifest;
- independent run artifacts;
- normal simulator histories; and
- semantic checkpoints.

The visualizer can later rebuild a fixed scenario with an edited profile
without changing the CTest scenario design.
