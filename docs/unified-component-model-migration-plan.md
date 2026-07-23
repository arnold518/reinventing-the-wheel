# Unified Component Model Migration Plan

Status: implemented and verified

Branch: `unified-component-migration`

Last updated: 2026-07-24

## 1. Goal

Give every circuit one public identity while allowing a build profile to choose
structural or behavioral fidelity independently at every selectable node.

The model must preserve the project's lower-level-first rule:

- build and test lower-level circuits first;
- admit a behavioral implementation only after equivalent lower-level behavior
  or a representative slice exists;
- keep behavioral implementations behind the same external contract;
- let the profile choose fidelity, never a C++ class name;
- keep the exact child-and-wire blueprint in the structural implementation.

## 2. Final Model

There are only three foundational component classes:

```text
Component
└── IOComponent
    └── BasicComponent
```

- `Component` owns hierarchy, wires, identity, and optional selection metadata.
- `IOComponent` adds typed input and output pins.
- `BasicComponent` is a directly evaluated leaf with delay and `evaluate()`.

There is no `StructuralComponent`, `BehavioralComponent`,
`PrimitiveComponent`, or `EvaluatedComponent` inheritance layer.

Structural and behavioral are selection metadata:

```cpp
enum class Fidelity {
    Structural,
    Behavioral,
};
```

A structural implementation is normally an `IOComponent` that builds child
components and wires. A behavioral implementation is a `BasicComponent` that
evaluates the same public contract directly. A gate is also a
`BasicComponent`, but when it terminates a structural circuit it is recorded as
a structural terminal leaf. Therefore C++ inheritance does not claim fidelity.

`mixed` remains useful only as a derived manifest result for a subtree that
contains both selected fidelities. It is not a selectable fidelity.

## 3. Public Component Family

A replaceable component is represented by one `ComponentFamily`.

The family owns:

- stable contract ID;
- stable visual/public type name;
- one shared pin initializer;
- structural factory, when available;
- behavioral factory, when available;
- default fidelity for explicit construction without a profile.

The implementation classes are mechanics behind the family. Parents, profiles,
contract tests, bindings, and the visualizer do not name them.

Example public declaration:

```cpp
namespace circuit::families {
extern const ComponentFamily ALU32;
}
```

Example family definition in `ALU32.cpp`:

```cpp
namespace {
void defineALU32Pins(IOComponent* self) {
    self->addPin<32>("A", PinType::INPUT);
    self->addPin<32>("B", PinType::INPUT);
    self->addPin<5>("OP", PinType::INPUT);
    self->addPin<32>("OUT", PinType::OUTPUT);
    // flags...
}
}

namespace circuit::families {
const ComponentFamily ALU32{
    "rv32i.alu32",
    "ALU32",
    defineALU32Pins,
    structuralFactory,
    behavioralFactory,
};
}
```

The pin definition belongs beside the component implementation. It is not kept
in a central pin-schema file. Both implementation constructors reuse
`families::ALU32.pinInitializer()`, so the interface is written once.

## 4. Construction

A parent asks for a family:

```cpp
builder.add(circuit::families::ALU32, "ALU");
```

It does not write:

```cpp
builder.addNewComponent<ALU32Direct>("ALU");
```

When a `BuildContext` exists, `ComponentBuilder::add()` sends a contract request
to the catalog. The profile selects fidelity at the full instance path, the
catalog chooses the verified implementation for that fidelity, and the manifest
records the result.

Without a build context, the family constructs its declared default. This keeps
small explicit teaching circuits convenient, although reproducible experiments
should use a profile.

## 5. Profiles

A profile rule contains:

- a selector by exact path, subtree, family/contract, depth, or parameters;
- only `Fidelity::Structural` or `Fidelity::Behavioral`;
- priority and a human-readable reason.

It contains no concrete implementation ID and no class name.

Handwritten profiles remain supported:

```cpp
auto profile = circuit::BuildProfileBuilder("inspect-alu")
    .addRule(circuit::preferFidelity(
        circuit::Fidelity::Behavioral))
    .addRule(circuit::preferFidelity(
        circuit::Fidelity::Structural,
        circuit::ProfileSelector::contract(circuit::families::ALU32)))
    .build();
```

Profile generators remove repetitive rules:

- `strictAllStructural()`;
- `maximallyStructural()`;
- `strictAllBehavioral()`;
- `structuralThroughDepth(k, policy)`;
- named presets;
- generated profile plus handwritten overrides.

The generator chooses visibility policy. The structural component still owns
its exact topology.

## 6. Catalog and Verification

The catalog records:

- external contract and semantic observation boundary;
- structural or behavioral fidelity;
- capabilities;
- deterministic priority;
- lower-level evidence;
- contract tests;
- equivalence or representative-slice tests;
- factory supplied by the component family.

A behavioral implementation cannot enter the frozen built-in catalog without:

1. lower-level evidence;
2. its own contract test;
3. an equivalence test or representative-slice evidence.

Registration validates that:

- behavioral selections create a childless `BasicComponent`;
- nonterminal structural selections create a composite rather than a direct
  evaluator;
- structural terminal leaves are explicitly marked;
- all implementations match the family pin schema;
- named evidence tests exist in the test registry.

## 7. Capabilities

Consumers must not recover implementation classes with casts or type strings.
Optional APIs use capability interfaces.

Current example:

```cpp
class RegisterStateView {
public:
    virtual ~RegisterStateView() = default;
    virtual std::vector<std::vector<LogicValue>>
        getRegisterStateAtTime(size_t time) const = 0;
};
```

Both structural and behavioral register-file implementations provide this
capability. The RV32I core and Python visualizer query `RegisterStateView`
instead of checking a behavioral class name.

Future memory images, cache statistics, pipeline inspection, and GPU state
views should follow the same pattern.

## 8. Tests

The test layers are deliberately different:

1. Lower-level tests prove gates, slices, and composites.
2. A structural contract test runs the family with structural fidelity.
3. A behavioral contract test runs the same scenario logic with behavioral
   fidelity.
4. An equivalence test runs each fidelity in an independent simulator and
   compares only the declared observation boundary.
5. Profile tests verify recursive selection and manifest metadata.
6. System tests compare the structural/balanced systems with the independent
   RV32I answer sheet.

Separate CTest entries for the two fidelity contract runs are retained so a
failure identifies the selected fidelity immediately. They do not own separate
expected behavior. Shared bases and `ComponentFamilyRowsTest` reuse the same
waveform or row table.

Current naming is purpose based:

```text
ALU32StructuralContractTest
ALU32BehavioralContractTest
ALU32EquivalenceTest
ALU32LowerLevelSliceTest
```

Implementation-mechanism names such as `DirectTest` are not part of the test
surface.

## 9. Python and Visualizer

Python exports the foundational bases and public teaching components. It does
not export internal direct-evaluator implementation classes.

All returned components still expose the common IO discovery surface through
the base binding. Optional state is accessed through capability functions, such
as `get_register_state_at_time(component, time)`.

The browser uses:

- public type name for reusable layout and drawing;
- contract ID and selected fidelity for the inspector;
- profile fingerprint for reproducibility;
- capabilities for specialized overlays.

It does not branch on a concrete behavioral class name.

Layout schema version 2 stores:

- reusable `type_layouts`;
- sparse `profile_layouts` when one profile genuinely needs different geometry;
- `root_layouts` keyed by `scenario@profile-fingerprint` for profiled roots.

## 10. Migrated Families

The current selectable families are:

- `MemoryBit`;
- `Register32`;
- `RegisterFile32x32`;
- `ALU32`;
- `RV32IControlFlow`;
- `RV32IDecodeControl`;
- `RV32IExecutionStatus`.

Behavioral-only scale/reference families include:

- `Memory64Kx32`;
- `RV32IReferenceCore`.

The structural system and core are also families so parents can build them
through stable contracts:

- `RV32ISingleCycleCore`;
- `RV32ISingleCycleSystem`;
- `RV32IReferenceSystem`.

Simple gates and one-form composites remain normal components. They are
cataloged for inventory and evidence, but do not need a two-factory family when
there is no fidelity choice.

## 11. Migration Checklist

- [x] Preserve the pre-migration RV32I checkpoint and create a migration branch.
- [x] Collapse the component hierarchy to the three foundational classes.
- [x] Add family-local shared pin contracts and factories.
- [x] Remove concrete implementation selection from profiles.
- [x] Migrate recursive parent construction to `builder.add(family, name)`.
- [x] Migrate the built-in catalog and verification policy.
- [x] Migrate register-state consumers to a capability.
- [x] Normalize component type names across fidelities.
- [x] Normalize tests to structural contract, behavioral contract, and
  equivalence purposes.
- [x] Remove direct implementation classes from Python's public surface.
- [x] Make visualizer overlays use public type/capability information.
- [x] Regenerate schema-v2 layouts with overlap validation.
- [x] Run the complete 168-test regression on the final source state.
- [x] Restart and probe the port-8765 systemd visualizer.
- [x] Commit the completed migration.

## 12. Non-Goals

This migration does not:

- make structural and behavioral timing identical outside each documented
  observation boundary;
- require every one-form component to have a behavioral counterpart;
- turn profiles into topology blueprints;
- replace the independent RV32I instruction oracle;
- hide lower-level circuits behind behavioral code before lower-level evidence
  exists.
