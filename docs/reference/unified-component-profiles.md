# Component Fidelity Profiles

## The simple model

A component family is one public kind of hardware. For example, the public
family is `ALU32`, whether its selected implementation is:

- structural: visible children and wires; or
- behavioral: one compact evaluated black box.

A parent asks for the family, not for an implementation class:

```cpp
builder.add(circuit::families::ALU32, "ALU");
```

One immutable `BuildProfile` is handed to the root. The same profile travels
through `BuildContext` to every selectable descendant. There is no second root
fidelity argument, descendant profile, effective fidelity, or mixed fidelity.

Only these two selections exist:

```cpp
enum class Fidelity {
    Structural,
    Behavioral,
};
```

A primitive gate may evaluate directly, but it is still the terminal leaf of a
structural design. “Mixed” is only a description of a manifest containing both
fidelities; it is not a third selectable value.

## What each object does

| Object | Responsibility |
|---|---|
| `ComponentFamily` | Public contract identity, shared pins, and implementation factories |
| `ComponentCatalog` | Contracts, available fidelities, evidence, validation, and construction |
| `BuildProfile` | Ordered rules that choose fidelity by path, subtree, contract, depth, or parameters |
| `BuildContext` | Carries the one profile during recursive construction |
| `BuildManifest` | Records what was actually selected at every created node |

The profile selects implementations. It does not describe internal wiring.
Each structural component still owns its exact child-and-wire blueprint in
`buildInternals()`.

## Rule behavior

Rules are read in order. The last matching rule wins. This is intentionally
simple: write a broad default first and a narrow override afterward.

```cpp
auto profile = circuit::BuildProfileBuilder("inspect-alu")
    .addRule(circuit::preferFidelity(
        circuit::Fidelity::Behavioral,
        circuit::ProfileSelector::any(),
        "compact default"))
    .addRule(circuit::preferFidelity(
        circuit::Fidelity::Structural,
        circuit::ProfileSelector::subtree("SYSTEM.CORE.ALU"),
        "show the ALU subtree"))
    .build();
```

That profile asks for behavioral components generally and structural
components at `SYSTEM.CORE.ALU` and below.

To keep a parent structural while making eligible descendants behavioral, use
a subtree rule followed by an exact-path override:

```cpp
auto profile = circuit::BuildProfileBuilder("visible-alu-shell")
    .addRule(circuit::preferFidelity(
        circuit::Fidelity::Behavioral,
        circuit::ProfileSelector::subtree("ALU")))
    .addRule(circuit::preferFidelity(
        circuit::Fidelity::Structural,
        circuit::ProfileSelector::exactPath("ALU")))
    .unavailablePolicy(
        circuit::UnavailableFidelityPolicy::
            UseOnlyAvailableAndRecordException)
    .build();
```

No special “descendants-only” selector is needed.

## Standard profile factories

`components/selection/StandardProfiles.hpp` provides functions returning normal
`BuildProfile` values:

- `strictAllStructural()`
- `maximallyStructural()`
- `strictAllBehavioral()`
- `structuralThroughDepth(k, policy)`

They are helpers, not a second profile system. Handwritten profiles and helper
profiles behave identically. `withProfileOverrides()` appends ordinary rules,
and `withExactFidelity()` appends one exact-path rule.

## Root construction

There is one catalog entry point:

```cpp
auto catalog = circuit::createBuiltinComponentCatalog();
auto request =
    circuit::families::RV32ISingleCycleSystem.request("SYSTEM");
auto profile = rv32i::balancedSystemProfile(*catalog, request);
auto build = catalog->createRoot(request, std::move(profile));
```

The root is selected by the same profile as its descendants. Code that needs a
particular root fidelity adds an exact-path rule to that profile:

```cpp
profile = circuit::withExactFidelity(
    std::move(profile),
    "SYSTEM",
    circuit::Fidelity::Structural);
```

This is important for experiments: one serialized policy completely explains
the build.

## Defaults and unavailable implementations

The canonical profile contains no forced rule. Resolution chooses structural
when available and behavioral otherwise.

There is no per-component hidden default. A strict profile reports an error
when its requested fidelity is unavailable. A permissive profile may use the
only available implementation, but the manifest records that exception.

## Capabilities

Consumers must not assume which concrete implementation was selected.
Implementation-independent optional APIs use capability interfaces:

- `RegisterStateView` observes register-file state.
- `RV32IStateView` observes architectural CPU state.

For example, the structural RV32I system holds its selected core as an
`IOComponent` and queries `RV32IStateView`. Therefore the same structural
system wrapper works with a structural or behavioral nested core.

Future memory images, cache statistics, pipeline state, and GPU inspection
should follow the same pattern.

## Tests

Each public contract owns one logical component test. Its scenario supplies
inputs and expected observations. `ComponentTestRunner`:

1. finds the fidelities available for the DUT contract;
2. derives one complete profile for each run by appending an exact DUT rule;
3. builds each run in an independent tree and simulator;
4. observes all public output pins at logical checkpoints;
5. checks independent expected outputs; and
6. compares snapshots across available fidelities.

The same base profile controls all descendants in both runs. There is no
`ComponentRunConfiguration` and no separate child profile.

Program tests are different: the behavioral RV32I system is the executable
answer sheet, so the program scenario compares independently executed
structural/profiled systems against the answer-sheet commit trace and
hard-coded program outcomes.

## Python and the visualizer

Python exposes the common component surface plus a registry:

```python
names = circuit_backend.get_registered_test_names()
test = circuit_backend.create_test_by_name(names[0])
```

Test classes are not individually bound. The visualizer uses the same registry
factory for ordinary and parameterized scenarios.

The HTML visualizer also exposes the recursive profile directly:

- the left explorer is the component tree;
- an expanded selectable row requests structural fidelity;
- a collapsed selectable row requests behavioral fidelity;
- fixed rows have only one implementation and cannot be toggled;
- `Apply & Run` saves the sparse exact-path overrides, creates a fresh
  simulation session, and returns the topology produced by that profile;
- clicking a row smoothly moves the camera to that component; and
- search and row virtualization keep the explorer usable for very large trees.

The explorer does not save a second copy of the component hierarchy. The
server stores only exact overrides in `visualizer-v2/profiles.json`; the C++
catalog rebuilds the real tree from the scenario's base profile plus those
overrides. Profile writes are revision-checked and atomic, and the existing
simulation remains active if validation or rebuilding fails.

`visualizer-v2/layout.json` uses:

- `type_layouts` for reusable type defaults;
- `profile_layouts` for sparse profile-specific overrides; and
- `root_layouts` for scenario roots keyed by
  `scenario@profile-fingerprint`.

The fingerprint describes topology-affecting policy. Human-facing profile
names and rule reasons are excluded, so renaming a profile does not duplicate
layouts.

Regenerate layouts after topology, profile, or layout-algorithm changes:

```bash
python3 -u visualizer-v2/recalculate_layouts.py
```

## Foundational classes

There are only three component bases:

```text
Component
└── IOComponent
    └── BasicComponent
```

- composites normally inherit `IOComponent` and implement
  `buildInternals()`;
- directly evaluated leaves inherit `BasicComponent` and implement
  `evaluate()`.

There is no `StructuralComponent` or `BehavioralComponent` base. Fidelity is a
catalog/profile choice, not a C++ inheritance category.
