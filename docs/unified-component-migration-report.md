# Unified Component Migration Report

Status: complete

Branch: `unified-component-migration`

Starting checkpoint: `f519350` (`Complete structural RV32I core and visualization`)

Last updated: 2026-07-24

## Result

CircuitSim now has one public family identity for every replaceable component.
Profiles choose structural or behavioral fidelity recursively without naming a
C++ implementation class.

The foundational hierarchy is:

```text
Component -> IOComponent -> BasicComponent
```

Structural/behavioral fidelity is instance metadata, not inheritance. The
earlier experimental `EvaluatedComponent`, `StructuralComponent`,
`BehavioralComponent`, and `PrimitiveComponent` layers were removed.

## Main Changes

### Component families

`ComponentFamily` groups the contract ID, public type name, shared pin
initializer, factories, and default fidelity.

Shared pin definitions now live beside their component implementation. For
example, `defineALU32Pins()` and the `families::ALU32` definition are in
`ALU32.cpp`. Both implementations reuse that initializer, so constructors no
longer duplicate the interface.

Parents now request families:

```cpp
builder.add(circuit::families::ALU32, "ALU");
```

### Profiles and catalog

Profiles contain only fidelity choices. Exact implementation IDs remain catalog
and manifest details and cannot be written into a profile.

The catalog still enforces lower-level-first evidence. Every normal behavioral
implementation cites lower-level proof, a contract test, and equivalence or
representative-slice coverage.

### Tests

The public test names now describe verification purpose:

```text
MemoryBitStructuralContractTest
MemoryBitBehavioralContractTest
MemoryBitEquivalenceTest
```

The same convention is used for register, register-file, ALU, control-flow,
decode/control, and execution/status families.

The two fidelity contract entries reuse the same scenario or row table. They
remain separate CTest entries so failures identify the selected fidelity.
Equivalence tests continue to run independent simulators, and instantiate both
sides through the public family/profile path rather than naming implementation
classes.

### Capabilities and bindings

`RegisterStateView` replaces concrete register-file casts in the RV32I core and
visualizer.

Internal direct-evaluator implementations are no longer exported as Python
classes. Python receives the stable public type, contract, selected fidelity,
profile fingerprint, pins, and wires through the common component surface.
Specialized register state is exposed through the capability function
`get_register_state_at_time(component, time)`.

### Visualizer and layouts

Visualizer drawing and state overlays use the public type/capability rather
than a concrete behavioral class name.

All 146 visual scenarios were regenerated with layout schema version 2:

```text
99 representative topologies
188,520 traversed component instances
43,021 parent topologies
130 active type layouts
146 root layouts
0 overlapping child layouts detected
```

Profile-specific child overrides are currently empty because the generated
placements are shared successfully. The sparse `profile_layouts` layer remains
available. Profile-built roots use fingerprinted root keys.

## Verification

The final source builds successfully, and the exact final binary passes all
168 CTest entries in a no-contention serial run:

```text
100% tests passed, 0 tests failed out of 168
Total Test time (real) = 1560.13 sec
```

Coverage includes:

- catalog/profile construction and recursive selection;
- catalog production inventory and evidence identifiers;
- structural and behavioral family contract tests;
- RV32I block contract tests;
- all structural/behavioral equivalence tests through the public family path;
- all 16 structural, 16 balanced-profile, and 16 reference RV32I programs;
- registry/CMake synchronization;
- Python binding and visualizer layout regression.

The final regression also caught and fixed two edge cases:

- `BasicComponent` initialization must not auto-evaluate input-driven leaves or
  double-start a `ClockGenerator`; constants keep their explicit override.
- A structural `RegisterStateView` observes child pins at the simulator's
  selected snapshot, so capability callers/tests now select the requested time
  before reading. Compact implementations may reconstruct hidden state from
  their internal history.

The systemd service on port 8765 was restarted after the final build. A live
`rv32i-structural-program9` probe returned:

```text
54,049 components
180,155 pins
103,361 wires
layout key: rv32i-structural-program9
state encoding: indexed-v1
```

## Design Consequences

- Users see one `ALU32`, not `ALU32` plus a public behavioral twin.
- A profile says “behavioral here,” not “instantiate class X.”
- Structural blueprints remain ordinary readable component code.
- Behavioral evaluators remain independently testable implementation details.
- Layouts, inspectors, and capabilities survive a fidelity change because they
  use the public family identity.
- Adding a third implementation of a family does not require a new profile
  vocabulary.

See `docs/unified-component-model-migration-plan.md` for the final model and
`docs/unified-component-profiles.md` for profile usage.
