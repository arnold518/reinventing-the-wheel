# Component Fidelity Profiles

## The simple model

A component family is one public kind of hardware. For example, the public
family is the configured `ALU<W>`, whether its selected implementation is:

- structural: visible children and wires; or
- behavioral: a compact evaluated model, or an explicitly validated composition
  of proven compact models.

A parent asks for the family, not for an implementation class:

```cpp
builder.add(circuit::families::rv32iALU(), "ALU");
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

The profile normally selects implementations. It does not describe internal wiring.
Each structural component still owns its exact child-and-wire blueprint in
`buildInternals()`.

A parent that cannot use one child fidelity may put `required_fidelity` on its
`ComponentBuildRequest`. This explicit requirement takes precedence over a
profile rule and fails if that implementation is unavailable. The structural
scalable memory endpoint uses it to keep a visible controller while requiring
the native device's behavioral implementation at any capacity. The manifest
records the selected native device contract and fidelity.

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

## Named visualizer presets

`FidelityPresets.hpp` owns the shared native/WASM policy (version 1). These
are ordinary build profiles, independent of unit/embedded/desktop timing.

| Preset | Selection |
| --- | --- |
| `gate` | Strict structural; unsupported storage capacity fails |
| `logic` | Structural logic, behavioral state/register/RF storage; behavioral native memory above 4,096 logical bit cells |
| `visual` | Behavioral full memory/register/device visual abstractions; structural remaining logic |
| `compact` | Prefer behavioral; record structural-only exceptions |

Every visualizer scenario opens in one preset, its `fidelity` in
`visualizer-v2/static/scenarios.json` (components and programs). There is no
scenario-defined profile in the menu, URL or saved profiles: explorer
profiles are keyed `<scenario>::<preset>`, and older keys without a preset
move to the scenario's default when loaded. A fixed scenario (a test without
build-profile support) is shown as built, its preset only a label. Inside
C++, an empty preset keeps the registered test's own build profile; only
tests and the example-row generator use it (the WebAssembly bridge before a
page chooses a preset, and `VisualizerSessionProbe --fidelity test`, which
`tools/run-web-tests.py` selects for its parity checks).

`fidelityPresetRestrictions` describes contract constraints before expensive
construction. The device system's fixed 256 KiB RAM and scalable endpoints'
required behavioral devices exclude strict Gate. Scenario payloads expose
`profile.unavailablePresets`; controls disable those choices, while direct
requests reject them with a readable reason. Custom exact overrides remain
explicitly marked custom. Registered component scenarios are checked before
building internals; the recorded-guest runtime checks before ELF loading.

`visualAbstractionKind` classifies full widgets by public contract. Primitive
symbols, layout, zoom and opening/closing parts do not alter selection.
The actual manifest is audited after construction, including required-child
exceptions. Private internals below a behavioral abstraction retain that
abstraction's proven implementation. Manual explorer overrides explicitly mark
the result custom and are stored under `scenario::preset`; the default retains
its legacy scenario key. Payloads report `fidelityPreset`, `presetVersion` and
`custom`. Sessions/cache keys include timing and fidelity separately.

## Behavioral compositions

Ordinary behavioral factories must remain childless `BasicComponent` leaves.
An IO composition must explicitly implement `BehavioralComposition` and validate
its internals. The console, timer, buttons and framebuffer compact factories
reuse their structural control builders, compiled into private control models
with gate-derived timing, plus their existing behavioral register/FIFO/storage
families. Observer boundaries remain present. Their validator rejects ordinary
structural gate children and requires registered compact children. The root
has no duplicated behavioral rule or independent whole-device timing table;
its constituent models own the behavior and proven delays.

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
- an expanded selectable row requests structural fidelity and a collapsed one
  behavioral; the canvas draws a collapsed part closed before Apply, and the
  read-only S/B badge shows the built fidelity (see
  `visualizer-v2-profile-explorer.md`);
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

A type and profile key must name **one internal structure**, because the
static site draws only saved placements. Types whose children change with a
setting the type name does not carry get a structure key
(`_component_layout_type` in `visualizer-v2/server.py`, kept identical to
`componentLayoutType` in `src/visualizer/VisualizerSession.cpp`): SRAM and FF
memory devices append their storage bank (`SRAM<32>[SRAMStorageBank<32,8,2>]`,
`FFArray8[MemoryStorageBank<FFArray,8,4>]`); caches, predictor tables and
entries, the branch predictor, memory timers, clocked counters and the DRAM
and NOR-flash command controls append a hash of their child names
(`…[layout-<16 hex>]`). A device group compiled inside a compact device lays
out as `…[config-compact]` (its child is the compiled model). Placements saved under the bare type do not
apply to a structure key. `tools/check-static-layouts.py`
(`VisualizerStaticLayoutTest`) fails when a child placement is missing, when
a key is shared by parts with different children, when an SRAM floorplan is
squeezed into a narrow strip, or when a child lies outside its parent's
content area. The content area is what `contentRect` in `app.js` leaves: below
the title bar (`title_bar_ratio` of the parent's width) and inside the side
margins (`boundary_area_ratio`) where pins are drawn. Floorplans written for
a parent should place children inside it (`_content_placer` in
`visualizer-v2/server.py`).

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
