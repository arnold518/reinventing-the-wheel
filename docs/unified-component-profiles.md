# Unified Structural/Behavioral Component Profiles

## The idea in plain words

A component contract says what a block looks like from the outside: its pins, signal widths, timing observation point, and optional capabilities. An implementation says how that block works inside.

For example, `rv32i.alu32` has two implementations:

- structural: the ALU is assembled from add/subtract, logic, compare, shift, and selection circuits;
- behavioral: one evaluated black box directly computes the same documented outputs.

The parent asks for an `rv32i.alu32`; it does not name either C++ implementation. A frozen build profile makes the choice at that instance path. Structural implementations still own their exact child-and-wire blueprints. A profile selects implementations recursively; it does not describe the wiring.

Only two fidelities are selectable: `structural` and `behavioral`. A primitive gate evaluates directly because recursion must stop somewhere, but it is recorded as a terminal structural leaf rather than a third fidelity. `mixed` is a report about a completed subtree, not a selectable fidelity.

## Main pieces

| Piece | Purpose |
|---|---|
| `ContractDescriptor` | Stable outside interface and semantic/timing promise |
| `ImplementationDescriptor` | Concrete structural or behavioral implementation plus evidence |
| `ComponentCatalog` | Frozen registry and deterministic resolver |
| `BuildProfile` | Immutable selection rules, available handwritten or generated |
| `ProfileGenerator` | Convenience for repeatable families of profiles |
| `BuildContext` | Carries the profile while structural recipes recursively build children |
| `BuildManifest` | Records every resolved and explicit node and its effective subtree fidelity |

## Built-in profile generators

- `strictAllStructural()`: requires structural fidelity everywhere requested by contract; it fails if any request has no structural implementation.
- `maximallyStructural()`: prefers structural fidelity and records every necessary behavioral fallback.
- `strictAllBehavioral()`: requires behavioral fidelity at every contract request.
- `structuralThroughDepth(k, policy)`: keeps requested nodes at depths `0..k` structural and requests behavioral implementations below that boundary.
- `presetProfile("education" | "balanced" | "fast" | "reference")`: named convenience policies.
- `withOverrides(base, rules)`: adds exact path, subtree, contract, depth, or parameter overrides to a generated base.

Handwritten profiles remain supported through `BuildProfileBuilder`. Equal profiles serialize identically and have the same fingerprint. Conflicting equally specific rules fail instead of depending on insertion order.

## C++ example

```cpp
auto catalog = circuit::createBuiltinComponentCatalog();
auto request =
    circuit::families::RV32ISingleCycleSystem.request("SYSTEM");

auto generator = circuit::structuralThroughDepth(
    1,
    circuit::UnavailableFidelityPolicy::UseOnlyAvailableAndRecordException);
auto profile = generator->generate(*catalog, request);
auto build = catalog->createRoot(request, std::move(profile));

auto system = std::dynamic_pointer_cast<RV32ISingleCycleSystem>(build.root);
const std::string reproducible_record = build.manifest->serialize();
```

For this raw depth request, the system and core are structural while requested descendants prefer behavioral implementations. A release profile may add subtree overrides when only stable-value rather than transport-delay equivalence has been proven. The resolved manifest records the actual mixture instead of hiding that distinction.

## Choosing a profile

Use structural fidelity when the internal signal flow is the experiment: carry propagation, decode logic, storage construction, routing, or a new lower-level implementation.

Use behavioral fidelity after the lower-level implementation or a representative slice exists and equivalence evidence defines the safe observation boundary. It is useful when the hidden structure is visually noisy, too large, or not the subject of the current experiment.

For cache, pipeline, and eventual GPU experiments, make the component under study structural and keep unrelated large storage or already-proven blocks behavioral. Save the profile serialization and build manifest with results; a vague label is not reproducible enough.

## Tests and the visualizer

Structural, behavioral, reference, and profile-built scenarios remain separate. Equivalence tests are non-visual checks that run each implementation in an independent simulator. All 16 RV32I programs also have balanced scenarios named:

```text
RV32IBalancedSystemProgram1Test
...
RV32IBalancedSystemProgram16Test
```

The browser aliases are `rv32i-balanced-program1` through `rv32i-balanced-program16`. These release scenarios keep the system, core, and timing-critical control/decode/ALU/status subtrees structural while selecting the register file and large memories behaviorally. Selecting a component shows its fidelity and implementation ID. Profile-built root layouts use `scenario@profile-fingerprint`, so two topologies from the same scenario cannot silently overwrite each other's root layout.

### Layout storage

`visualizer-v2/layout.json` uses schema version 2 and separates three concerns:

- `type_layouts` stores reusable defaults and styling for a concrete visual type plus topology-affecting parameters such as bus width or matcher mask/value.
- `profile_layouts` is a sparse map from profile fingerprint to per-type child-placement overrides. It is used only when a profile genuinely needs different geometry or the user intentionally edits that profile's composite layout.
- `root_layouts` stores scenario roots by the API-provided layout key. Explicit builds use the scenario alias; profile builds use `scenario@profile-fingerprint`.

The browser must use the server's `layoutKey`, not reconstruct it from the selected scenario. This keeps root edits isolated while avoiding thousands of duplicate leaf layouts. Because selectable implementations have the same contract pins, their generated parent placements are normally identical; the profile override layer therefore remains empty until a real difference exists.

Recalculate every default after topology or layout-algorithm changes with:

```bash
python3 -u visualizer-v2/recalculate_layouts.py
```

The command builds every visualizable topology without running simulation timelines, collapses the 16 identical program topologies in each system family, checks every generated parent for overlapping child rectangles, removes stale layout types/aliases, preserves type colors, and replaces the layout file only after the full pass succeeds.

The required testing layers are:

1. lower-level gate, slice, and composite tests;
2. structural and behavioral family contract tests using shared scenarios;
3. independent structural/behavioral equivalence tests at the declared observation point;
4. profile resolution and manifest tests;
5. full-system lockstep tests for every release profile.

The current inventory test also requires every production module `TypeName` to appear in the built-in catalog. Adding a new module without catalog registration therefore fails CI.

## Explicit construction

`Component::create<T>()` remains available for one-form components and small
teaching circuits. It has no profile, so its selected fidelity is reported as
`unspecified`.

Selectable parent blueprints use:

```cpp
builder.add(circuit::families::ALU32, "ALU");
```

Without a build context, `builder.add()` creates the family's declared default.
With a build context, it resolves the family recursively through the profile.

The only component bases are `Component`, `IOComponent`, and `BasicComponent`.
New directly evaluated leaves inherit `BasicComponent`; new composites inherit
`IOComponent`. Fidelity comes from catalog/profile metadata rather than the
base class.
