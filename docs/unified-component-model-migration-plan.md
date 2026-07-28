# Unified Component Model Migration

Status: implemented and verified

Branch: `unified-component-migration`

Last updated: 2026-07-25

## Goal

Every replaceable component has one public family and one external contract.
One recursively propagated profile chooses structural or behavioral fidelity
at the root and at every selectable descendant.

The model preserves the lower-level-first rule:

- build and test lower-level circuits first;
- add behavioral scale abstractions only after structural or representative
  lower-level evidence exists;
- keep both implementations behind the same pins and observation contract;
- let profiles choose fidelity, never C++ class names; and
- keep wiring blueprints in structural component implementations.

## Final architecture

### Foundational component classes

```text
Component
└── IOComponent
    └── BasicComponent
```

No fidelity-specific base classes exist.

- `Component` owns hierarchy, wires, identity, and selection metadata.
- `IOComponent` adds pins.
- `BasicComponent` adds delay and direct `evaluate()`.
- A normal structural composite inherits `IOComponent`.
- A behavioral implementation inherits `BasicComponent`.
- A directly evaluated gate remains a terminal structural primitive.

### One public family

`ComponentFamily` owns the identity shared by all implementations:

- contract ID;
- public/visual type name;
- one pin initializer;
- structural factory when present;
- behavioral factory when present.

Parents request a family:

```cpp
builder.add(circuit::families::ALU32, "ALU");
```

They never choose a concrete implementation class.

The shared pin initializer is defined beside the component, normally in its
main `.cpp` file. Both factories reuse it, so the interface is written once.
Internal evaluator classes are construction mechanics and are not bound to
Python or named by profiles and tests.

### One recursive profile

The only root API is:

```cpp
catalog.createRoot(request, profile);
```

`BuildContext` carries that exact profile recursively. There is no:

- `createRootExact`;
- root-fidelity side argument;
- descendant/child profile;
- effective fidelity;
- reference-only selection category;
- polymorphic profile-generator hierarchy; or
- string lookup that maps a preset name to an implementation.

A rule chooses only `Fidelity::Structural` or
`Fidelity::Behavioral`. Rules are ordered; the last matching rule wins.

Selectors support exact path, subtree, contract, depth, and parameters. A
broad rule followed by a narrow rule is an explicit override.

Standard helper functions return ordinary `BuildProfile` values:

```cpp
strictAllStructural()
maximallyStructural()
strictAllBehavioral()
structuralThroughDepth(k, policy)
```

`withProfileOverrides()` and `withExactFidelity()` also return ordinary
profiles.

### Catalog and evidence

The frozen catalog records:

- contract pins and observation semantics;
- available fidelity;
- optional capabilities;
- lower-level evidence;
- contract tests;
- equivalence or representative-slice evidence; and
- the family-backed factory.

It enforces one implementation per `(contract, fidelity)`. A behavioral
implementation cannot enter the built-in catalog without lower-level evidence,
a contract test, and equivalence or representative evidence.

Construction validation requires:

- a behavioral implementation to be a childless `BasicComponent`;
- a nonterminal structural implementation to be a composite;
- a terminal structural primitive to be marked explicitly; and
- every implementation to match its contract pins.

### Capabilities instead of concrete casts

Consumers that need more than pins depend on capability interfaces:

- `RegisterStateView`
- `RV32IStateView`

This was especially important for the RV32I system. Its structural wrapper can
now contain either selected core fidelity and observe it through
`RV32IStateView`.

Future cache statistics, pipeline state, memory images, and GPU inspection
should use the same pattern.

## Component-test model

Each public component contract owns one logical test. A scenario owns inputs,
checkpoint meanings, and independent expected observations.

`ComponentTestRunner::runAll()`:

1. finds the available DUT fidelities;
2. starts from one caller-supplied base profile;
3. appends an exact DUT rule for each run;
4. builds independent trees and simulators;
5. drives the same scenario;
6. observes every public output pin;
7. checks expected outputs; and
8. compares fidelity snapshots.

The DUT rule and all subtree choices are therefore part of one complete
profile. Tests do not carry a second configuration object.

Program scenarios use the behavioral RV32I system as the executable answer
sheet and compare independent commit traces and hard-coded final outcomes.

CTest names describe logical components or program scenarios, not
implementation mechanics. Parameterized programs remain:

```text
RV32ISingleCycleSystemTest/program-01
...
RV32ISingleCycleSystemTest/program-16
```

## Registry and visualizer

The C++ registry owns scenario metadata and factories. Python exposes only:

- `get_registered_test_names()`;
- `get_registered_test_descriptors()`; and
- `create_test_by_name()`.

The visualizer constructs every scenario through that registry. It no longer
depends on a manually duplicated Python class binding for each test.

Component inspection shows public type, contract, selected fidelity,
selection reason, and profile fingerprint. It does not branch on internal
behavioral class names.

Layout schema version 2 stores reusable type defaults, sparse profile
overrides, and scenario roots. Profile fingerprints ignore human names and
reasons, preventing layout duplication when only prose changes.

## Migrated selectable families

- `MemoryBit`
- `Register32`
- `RegisterFile32x32`
- `AddSub32`
- `Logic32`
- `Shifter32`
- `Comparator32`
- `ZeroDetect32`
- `ALU32`
- `RV32IControlFlow`
- `RV32IDecodeControl`
- `RV32IExecutionStatus`
- `RV32ISingleCycleCore`
- `RV32ISingleCycleSystem`

`Memory64Kx32` remains behavioral-only, backed by structural `Memory4x32` and
`Memory32x32` representative slices. One-form components remain cataloged but
do not need a second factory.

## Completion checklist

- [x] Keep only the three foundational component classes.
- [x] Give selectable components one family identity and shared pins.
- [x] Select only the two fidelities through profile metadata.
- [x] Propagate one profile from root through the complete tree.
- [x] Remove forced-root and descendant-profile APIs.
- [x] Replace generator objects with plain profile helper functions.
- [x] Remove reference-only and priority/specificity selection layers.
- [x] Make profile rule precedence ordered and explicit.
- [x] Make the RV32I core and system complete two-fidelity families.
- [x] Add fidelity-independent RV32I state observation.
- [x] Migrate component tests to one base profile and one logical scenario.
- [x] Route visualizer scenario creation through the test registry.
- [x] Regenerate all default layouts for the final profile fingerprints.
- [x] Pass all 125 CTest entries on the final source state.
- [x] Record final measured verification in the migration report.

## Non-goals

This migration does not:

- require every component to have both fidelities;
- make propagation delays equal between fidelities;
- turn a profile into a wiring blueprint;
- make behavioral code the structural source of truth;
- replace the independent RV32I instruction oracle; or
- implement interactive profile editing in the visualizer.
