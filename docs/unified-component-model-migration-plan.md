# Unified Component Model Migration Plan

Status: proposed architecture, ready for staged implementation

Audit date: 2026-07-20

Last plan revision: 2026-07-21

## 1. Decision

Adopt a three-part model:

1. **Contract**: the logical hardware block and its externally observable rules.
2. **Implementation**: one concrete way to realize that contract.
3. **Build profile**: the policy that selects implementations for a component tree.

This model cleanly supports the two selectable fidelities, structural and behavioral, while also representing terminal primitive leaves and mixed completed trees. It must not be reduced to a constructor flag such as `ALU32(name, Mode::Behavioral)`.

The public construction API should request a contract:

```cpp
auto alu = builder.add<contract::ALU32>("ALU");
```

The active build profile should resolve that request to an internal implementation such as:

```text
contract:       rv32i.alu32
implementation: structural.netlist
```

or:

```text
contract:       rv32i.alu32
implementation: behavioral.direct
```

Concrete implementations still need different C++ classes because a structural composite builds children while a direct behavioral component evaluates itself. Those classes should become implementation details, preferably with local names such as:

```cpp
namespace circuits::impl::alu32 {
class Structural final : public StructuralComponent { /* ... */ };
class Behavioral final : public BehavioralComponent { /* ... */ };
}
```

This removes `A` versus `BehavioralA` from normal call sites without pretending that one class can have two incompatible execution mechanisms.

### Verification conclusion

The proposed model is suitable for the whole repository, with four mandatory safeguards:

- Contract registration must validate pins, semantics, timing/observation rules, parameters, and capabilities—not just matching class names.
- Behavioral selection must require lower-level evidence under the repository's lower-level-first rule.
- Resolution must be explicit, recursive, immutable during a build, and written to a resolved-build manifest.
- Missing or incompatible implementations must fail clearly. A strict structural request must never silently become behavioral.

Without these safeguards, the model would incorrectly group the current RV32I cores and memories as interchangeable.

## 2. What Was Audited

The audit covered the complete component creation and simulation path, all production component declarations and implementations, RV32I functional services, test harnesses, bindings, and both visualizers.

Current inventory:

| Area | Audited inventory |
| --- | ---: |
| Component-derived production module declarations | 81 |
| Direct `BasicComponent` subclasses | 19 |
| Direct `IOComponent` subclasses | 62 |
| Simulation tests in `TestRegistry` | 142 |
| CTest entries including the binding test | 143 |
| Python visualizer classes | 13 |

The 81 production declarations are listed in section 7. Test-only helper components, anonymous implementation helpers, pybind trampolines, and non-component RV32I services were also reviewed, but they are not selectable hardware implementations.

### Focused verification performed during this audit

The current tree built successfully. The following eight tests passed:

```text
RV32IControlFlowUnitPairTest
RV32IDecodeControlUnitPairTest
RV32IRegisterFilePairTest
BehavioralALU32PairTest
RV32IExecutionControlStatusUnitPairTest
RV32ISingleCycleSystemContractTest
BehavioralRV32ISystemContractTest
VisualizerModuleBindingsTest
```

Representative arithmetic/control, trap, and instruction-fetch-fault programs also passed in both systems:

```text
RV32ISingleCycleSystemProgram1Test
RV32ISingleCycleSystemProgram9Test
RV32ISingleCycleSystemProgram16Test
BehavioralRV32ISystemProgram1Test
BehavioralRV32ISystemProgram9Test
BehavioralRV32ISystemProgram16Test
```

A runtime schema probe confirmed seven same-pin pairs:

- `ALU32` and `BehavioralALU32`
- `MemoryBit` and `BehavioralMemoryBit`
- `Register32` and `BehavioralRegister32`
- `RegisterFile32x32` and `BehavioralRegisterFile32x32`
- `RV32IControlFlowUnit` and `BehavioralRV32IControlFlowUnit`
- `RV32IDecodeControlUnit` and `BehavioralRV32IDecodeControlUnit`
- `RV32IExecutionControlStatusUnit` and `BehavioralRV32IExecutionControlStatusUnit`

It also confirmed that the two system wrappers have the same external pins, and that all three memory sizes use the same CPU-facing pin shape. Those facts alone do not prove substitution; the exceptions are documented below.

## 3. Why One Class With a Mode Is Not Clean

The current simulator makes a real execution distinction:

- `Component::create<T>()` constructs a concrete type, initializes its pins, and then calls `buildInternals()`.
- `ComponentBuilder::addNewComponent<T>()` calls that concrete factory directly.
- `Wire.cpp`, `Event.cpp`, and `SimulationTest::scheduleInitialEventsForTree()` identify evaluated nodes by dynamically casting to `BasicComponent`.
- A non-`BasicComponent` `IOComponent` is treated as a hierarchy boundary, so wire changes continue through its internal or external wire.

Changing a field inside one `A` class would therefore change more than an algorithm. It would change whether input events schedule `evaluate()` or cross a composite boundary. A mode-dependent inheritance relationship is impossible in C++, and emulating it with conditions in the simulator would spread selection logic into event dispatch, initialization, visualization, and tests.

A wrapper `A` containing either `StructuralA` or `BehavioralA` is also undesirable. It would add an artificial hierarchy level, duplicate boundary pins and wires, add visual noise, complicate timing, and increase component counts. The catalog should return the selected concrete node itself, annotated with its logical contract metadata.

## 4. The Two Fidelity Choices

The profile has only two user-selectable fidelity choices:

```cpp
enum class Fidelity {
    Structural,
    Behavioral,
};
```

- A **structural** implementation provides a child-and-wire blueprint and can be expanded in the visualizer.
- A **behavioral** implementation calculates the contract directly and is a black box at that contract boundary.

For selectable implementations, registration must enforce the mapping:

```text
structural fidelity -> structural component blueprint
behavioral fidelity -> direct evaluated component
```

Composite/evaluated is therefore not another profile dimension. It is an internal simulator consequence of the fidelity selection. Primitive gates and routing helpers are the terminal exception: they evaluate directly because structural recursion must end somewhere, but they are not behavioral alternatives exposed to the profile.

### Logical identity

What the block promises: for example, a 32-bit ALU, a write-enabled bit, or an RV32I decode/control unit.

### Internal simulator mechanism

The engine still needs two internal reactions when a signal reaches a node:

- A structural node passes the signal into its child wires.
- A behavioral or primitive node schedules direct `evaluate()` logic.
- **Passive/root**: grouping-only nodes with no pins or evaluation.

`BasicComponent` currently means “evaluated,” not “basic.” It includes gates, utility adapters, a 256 KiB memory, and the behavioral CPU. The target name should be `EvaluatedComponent`.

Plain structural modules currently inherit directly from `IOComponent`. The target should add `StructuralComponent`. A `BehavioralComponent` can derive from the internal `EvaluatedComponent` base, alongside terminal `PrimitiveComponent` implementations.

These classes are needed by the event engine, but a profile never chooses them independently.

### Primitive leaves and mixed results

Primitive, mixed/hybrid, and reference are not additional fidelity values:

- **Primitive** means structural recursion has reached a terminal gate, constant, splitter, joiner, or routing helper.
- **Mixed/hybrid** summarizes a completed subtree containing both structural and behavioral choices.
- **Reference** is a testing role, such as an oracle-backed system, and is recorded separately from fidelity.

The current `BehavioralRegister32` name violates the clean rule because that class is a composite made from 32 behavioral bit cells. In the target model it is either a legacy structural register recipe with behavioral descendants, or it must be rewritten as one direct evaluated 32-bit register before it can register as the behavioral implementation of the whole register contract.

`RegisterFile32x32`, `RegisterFile4x32`, `Memory4x32`, and `Memory32x32` show why the final manifest must summarize mixed subtrees: their visible composed organization intentionally contains compact behavioral storage descendants.

### Scale and completeness

An implementation may be a complete implementation of a contract or only a representative lower-level slice. `Memory4x32` and `Memory32x32` are structural teaching slices; they are not lower-capacity drop-in implementations of `BehavioralMemory64Kx32` for a request that requires 64K words and byte/halfword accesses.

### Verification domain

Known binary equivalence and full four-state equivalence are different claims. Several RV32I behavioral blocks conservatively turn all outputs unknown when an input contains `UNKNOWN` or `HIGH_Z`, while structural gates can preserve unaffected known bits. The current pair reports explicitly prove known `0/1` behavior, not complete partial-unknown equivalence.

### Timing model

Functional equality after settling is not event-by-event timing equality. Structural propagation delay depends on topology; most behavioral implementations use a one-tick delay. A contract must state whether conformance is observed:

- after quiescence;
- at a clock boundary;
- after a fixed latency;
- or with exact event timing.

Build profiles must not imply exact timing equivalence when only stable-value or cycle-boundary equivalence has been proven.

## 5. Target Architecture

### 5.1 Contract descriptor

Each selectable family gets one versioned descriptor containing:

```cpp
struct ComponentContractDescriptor {
    ContractId id;
    uint32_t version;
    std::string_view display_name;
    ParameterSchema parameters;
    PinSchema pins;
    SemanticDomain semantic_domain;
    ObservationContract observation;
    CapabilityRequirements required_capabilities;
};
```

The contract, not either implementation constructor, becomes the single source of truth for pin names and widths. Pin initialization should install the descriptor's schema. Registration must instantiate or inspect an implementation and reject any mismatch.

Contract parameters must be canonical and must affect identity. Examples include mux input count and word width, decoder address width, memory capacity and supported access sizes, splitter width, constant output width/value, and matcher mask/value.

`ConstantValue<OUT_WIDTH, LEGACY_WIDTH>` deserves an early cleanup: `LEGACY_WIDTH` does not affect its pins or behavior and should not remain part of canonical identity.

### 5.2 Implementation descriptor

Each concrete realization registers metadata and a factory:

```cpp
struct ImplementationDescriptor {
    ImplementationId id;
    ContractId contract;
    Fidelity fidelity;
    bool terminal_primitive;
    bool reference_only;
    ParameterPredicate supports;
    SemanticDomain conformance_domain;
    ObservationContract conformance_observation;
    CapabilitySet capabilities;
    VerificationEvidence evidence;
    ComponentFactory factory;
};
```

There is no separately selectable execution kind. Registration validates that a structural implementation factory produces a `StructuralComponent`, except for an explicitly marked terminal primitive, and that a behavioral implementation factory produces a direct `BehavioralComponent` without functional child topology. `reference_only` controls where an implementation may be used; it is not a third fidelity.

`VerificationEvidence` should record:

- lower-level implementation or representative slice;
- independent contract tests;
- direct equivalence suites where a same-parameter implementation exists;
- supported logic domain;
- timing/observation claim;
- status such as experimental, verified, or reference-only.

A behavioral implementation must not become an automatic product default until its evidence satisfies the lower-level-first policy.

### 5.3 Explicit catalog

Use one deterministic `ComponentCatalog`, populated by an explicit `registerBuiltinComponents(catalog)` call. Avoid mutable global selection state and static-initialization-order registration.

The catalog must:

- reject duplicate contract or implementation IDs;
- reject pin-schema and parameter mismatches;
- reject implementations missing required capabilities;
- expose all available implementations and their evidence to tests and the visualizer;
- be frozen before a circuit build begins.

### 5.4 Build request, profile, and context

A build request contains the contract, normalized parameters, instance name/path, required semantics/capabilities, and any constraints imposed by the parent recipe.

The immutable `BuildContext` contains:

- the frozen catalog;
- the selected `BuildProfile`;
- the current component path;
- the requested semantic/timing domain;
- a resolved-build manifest collector.

Suggested resolution precedence:

1. exact instance-path override;
2. nearest matching subtree rule;
3. contract-and-parameter rule;
4. profile fidelity preference;
5. explicit contract baseline.

Every resolution records the winning rule and rejected alternatives. An explicit override that cannot be satisfied is an error. A baseline choice is allowed only when it is declared and recorded; it must not be a hidden fallback.

Suggested initial profiles:

| Profile | Intent |
| --- | --- |
| `education` | Preserve the visible current RV32I topology and approved scale abstractions. |
| `balanced` | Keep meaningful composition while selecting proven behavioral children for expensive blocks. |
| `fast` | Prefer proven direct behavioral implementations and compact storage. |
| `strict-structural` | Require structural implementations, ending only at declared primitive leaves; fail where the repository lacks a structural realization. |
| `reference` | Use oracle-backed/reference components for answer generation and differential testing, not as lower-level proof. |

The current full CPU cannot honestly satisfy `strict-structural`: its two 64K-word memories are behavioral, and its composed register file uses `BehavioralRegister32` cells. The profile should report that limitation instead of silently weakening itself.

### 5.5 Profile generators

Profiles should remain immutable rule sets, but users should not have to write those rules by hand for every experiment. Add an abstract generator that produces a complete, serializable `BuildProfile`:

```cpp
class ProfileGenerator {
public:
    virtual ~ProfileGenerator() = default;

    virtual BuildProfile generate(
        const ComponentCatalog& catalog,
        const RootBuildRequest& root) const = 0;

    virtual ProfileGeneratorDescriptor descriptor() const = 0;
};
```

The generator does not create components, enumerate the final component tree, or describe child wiring. It compiles its settings into deterministic rules first; recursive construction then reads the frozen generated profile. Each selected structural implementation still owns its exact child-and-wire blueprint. This keeps builds reproducible and makes the profile fingerprint, manifest, CLI, and visualizer independent of generator object lifetime.

Unavailable-fidelity behavior is generator configuration, never an implicit fallback:

```cpp
enum class UnavailableFidelityPolicy {
    Error,
    UseOnlyAvailableAndRecordException,
};
```

Keep the abstract interface small, but avoid one subclass for every minor rule combination. Most built-ins can use one data-backed `RuleBasedProfileGenerator`, exposed through named factory functions. Reserve additional subclasses for genuinely different generation algorithms, such as a future component-budget or measured-performance generator:

```cpp
class RuleBasedProfileGenerator final : public ProfileGenerator {
public:
    explicit RuleBasedProfileGenerator(ProfileGeneratorDescriptor descriptor,
                                       std::vector<GenerationRule> rules);

    BuildProfile generate(const ComponentCatalog&,
                          const RootBuildRequest&) const override;
    ProfileGeneratorDescriptor descriptor() const override;
};

std::unique_ptr<ProfileGenerator> strictAllStructural();
std::unique_ptr<ProfileGenerator> maximallyStructural();
std::unique_ptr<ProfileGenerator> strictAllBehavioral();
std::unique_ptr<ProfileGenerator> structuralThroughDepth(
    size_t maximum_depth,
    UnavailableFidelityPolicy unavailable);
```

Initial generator factories/presets should include:

| Factory/preset | Generated policy |
| --- | --- |
| `strictAllStructural()` | Structural at every selectable node; error if a required structural implementation is absent. Primitive leaves are allowed as structural endpoints. |
| `maximallyStructural()` | Prefer structural wherever one exists; permit behavioral only through an explicit recorded-exception policy. |
| `strictAllBehavioral()` | Behavioral at every selectable contract; error where no behavioral implementation exists. |
| `structuralThroughDepth(k, unavailable)` | Structural for paths at depth `0..k`, behavioral below that boundary. Root depth is zero. |
| `preset(name)` | Produce named project presets such as `education`, `balanced`, `fast`, and `reference`. |
| `withOverrides(base, overrides)` | Decorate another generator with exact-path, subtree, contract, parameter, or variant overrides. |

Depth becomes a normal rule selector alongside path, subtree, contract, and parameters:

```cpp
ProfileRule{
    .selector = DepthRange{.minimum = 0, .maximum = k},
    .fidelity = Fidelity::Structural,
};

ProfileRule{
    .selector = DepthRange{.minimum = k + 1},
    .fidelity = Fidelity::Behavioral,
};
```

For a structural node at depth `k`, its blueprint exists and requests children at depth `k + 1`; those children resolve behaviorally, so expansion naturally stops at the requested boundary.

`strictAllStructural()` and strict depth configurations use `Error`. Convenience generators may use the second policy. The generated profile records that fallback is permitted; because descendant paths emerge only while structural blueprints expand, each actual exception is recorded by the resolver in the resolved manifest. This distinction prevents a profile named “all structural” from quietly inserting behavioral memory.

Example usage:

```cpp
auto base = profile::structuralThroughDepth(
    3, UnavailableFidelityPolicy::Error);

auto generator = profile::withOverrides(
    std::move(base),
    {
        behavioralAt("system.instruction_memory"),
        behavioralAt("system.data_memory"),
        structuralAt("system.core.alu"),
    });

const BuildProfile profile = generator->generate(catalog, root_request);
```

The generator descriptor and its arguments must serialize into the profile metadata, for example:

```text
generator: structural-through-depth
maximum_structural_depth: 3
unavailable_fidelity: error
overrides:
  system.instruction_memory: behavioral
  system.data_memory: behavioral
```

This allows the visualizer to offer simple controls such as “all structural,” “maximum structural,” or a structural-depth slider without weakening the underlying explicit profile model. Manual `BuildProfile` construction remains available for exact saved experiments and tests; generators are a convenience layer, not a second selection system.

Resolution therefore follows this sequence:

```text
generator settings -> frozen profile rules
                         |
component request(path, depth, contract)
                         |
                    resolver selects fidelity + implementation
                         |
        structural implementation expands its own blueprint
                         |
             new child requests repeat the same process
```

The generator describes selection policy, not the hardware layout.

### 5.6 Recursive selection

Composite implementation recipes must request child contracts, not child concrete types:

```cpp
auto register_file = builder.add<contract::RegisterFile>(
    "REGISTER_FILE",
    RegisterFileParameters{.register_count = 32, .word_width = 32});
```

The builder carries a child `BuildContext`, so the same parent source can produce educational, hybrid, or fast subtrees.

A recipe may add constraints when its identity depends on a topology. For example, a `structural.ripple` adder recipe may require full-adder cells, while a composed register word may allow either structural or behavioral bit cells. The resolver intersects recipe constraints with profile policy and fails on conflict.

Because child choices can change a composite's effective fidelity, the manifest—not the parent's class name—must answer questions such as “does this core contain behavioral grandchildren?”

### 5.7 Contract handles instead of concrete pointers

`ComponentBuilder::getInputPin<T>()` and `getOutputPin<T>()` currently ignore `T` for lookup and retrieve an `IOComponent` internally. This is a useful migration opportunity.

Logical construction should return a contract-typed handle:

```cpp
ComponentHandle<contract::ALU32> alu = builder.add<contract::ALU32>("ALU");
builder.connect(parent.input<32>("A"), alu.input<32>("A"));
```

The handle owns or references `std::shared_ptr<IOComponent>` and provides schema-checked pin access. Parent components should no longer store `std::shared_ptr<StructuralConcreteType>` merely to wire or inspect a child.

Keep `Component::create<T>()` temporarily as an explicit-implementation compatibility path for legacy tests and bindings. Product composition should migrate to contract requests.

### 5.8 Capabilities for non-pin APIs

Pins are only part of several current contracts. These direct concrete methods also matter:

- memory preload, clear, readback, touched-word history, and bus-write history;
- register-file state at a selected simulation time;
- RV32I initial PC/register setup;
- architectural state snapshots;
- last memory access/write traces.

Introduce queryable capability interfaces such as:

```text
MemoryImage
MemoryHistory
RegisterStateView
RV32IInitialStateControl
RV32IArchitecturalStateView
RV32IInstructionTraceView
```

`RV32IProgram::loadInto()` should accept `MemoryImage`, not `BehavioralMemory64Kx32&`. `RV32ISingleCycleCore::snapshotState()` must use a register-state capability rather than walking children named `X1` through `X31`; the current traversal breaks if the compact register file is selected.

Capabilities should be exposed through the logical handle or a generic `queryCapability<T>()` mechanism. They must be included in selection validation.

### 5.9 Simulator terminology and dispatch

Migrate toward:

```text
Component
  IOComponent
    EvaluatedComponent
      PrimitiveComponent
      BehavioralComponent
    StructuralComponent
```

During compatibility, `BasicComponent` can be an alias or deprecated subclass of `EvaluatedComponent`.

`StructuralComponent` supplies `buildInternals()`. `BehavioralComponent` supplies direct `evaluate()`. `PrimitiveComponent` also evaluates directly but is a terminal leaf rather than a behavioral alternative. The catalog enforces this mapping; the profile never chooses an execution mechanism.

Event dispatch should use a virtual input reaction or the internal base class instead of treating a `dynamic_pointer_cast<BasicComponent>` and its misleading name as the architectural definition. This prevents implementation selection from leaking into `Wire.cpp`, `Event.cpp`, and test initialization.

### 5.10 Instance metadata and resolved manifest

Every component instance should expose:

```text
contract ID and version
normalized parameters
implementation ID
selected fidelity
terminal-primitive or reference-only markers when applicable
effective subtree result: structural, behavioral, or mixed
selection rule/reason
verification status/domain
```

The final manifest should list the full hierarchy and a profile fingerprint. It is required for reproducibility, debugging, performance experiments, and honest visualization.

## 6. Compatibility Findings

### 6.1 Eligible after existing known-binary evidence is represented

The five major RV32I block pairs have matching pins and passing direct pair suites:

| Contract candidate | Structural/composed implementation | Behavioral implementation | Current proof boundary |
| --- | --- | --- | --- |
| `rv32i.control-flow` | `RV32IControlFlowUnit` | `BehavioralRV32IControlFlowUnit` | Known binary inputs and clocked scenarios. |
| `rv32i.decode-control` | `RV32IDecodeControlUnit` | `BehavioralRV32IDecodeControlUnit` | Known 32-bit instructions; partial-X differs. |
| `rv32i.register-file` | `RegisterFile32x32` | `BehavioralRegisterFile32x32` | Known binary pair suite; behavioral X-policy tested separately. |
| `rv32i.alu32` | `ALU32` | `BehavioralALU32` | Stable known binary results; partial-X differs. |
| `rv32i.execution-status` | `RV32IExecutionControlStatusUnit` | `BehavioralRV32IExecutionControlStatusUnit` | Known binary cycle-boundary behavior; partial-X differs. |

These implementations should initially register only for the domain they have actually proven. Four-state interchangeability remains a separate milestone.

### 6.2 Same schema, but missing direct substitution proof

`MemoryBit`/`BehavioralMemoryBit` and `Register32`/`BehavioralRegister32` have matching pins and closely aligned standalone behavior. They do not yet have dedicated isolated direct-equivalence suites comparable to the five major RV32I pairs.

`MemoryBit` is the clean pilot: the structural implementation is a child-and-wire blueprint and the behavioral implementation evaluates the same state transition directly. `Register32` needs one normalization step first. Despite its name, the current `BehavioralRegister32` is a composition of 32 behavioral bit cells, so it is not a behavioral implementation at the register contract boundary under the target rules.

They are the best pilot for the selection framework because a write-enabled bit proves that one contract can resolve to either a composite or an evaluated implementation. Before allowing profile selection:

- add one shared independent state-transition contract suite;
- run each implementation in a separate simulator;
- compare reset, hold, edges, writes, and explicit unknown cases;
- characterize the current composed `BehavioralRegister32` as a legacy mixed recipe;
- implement a genuinely direct `BehavioralComponent` version of the 32-bit register before registering behavioral fidelity for that whole-register contract;
- run the same independent and isolated-equivalence treatment at the 32-bit register level.

### 6.3 Same memory pins do not mean same memory contract

`Memory4x32`, `Memory32x32`, and `BehavioralMemory64Kx32` expose the same pin names and widths. Their semantics differ:

- capacities are 4, 32, and 65,536 words;
- the structural teaching memories are word-oriented slices;
- the behavioral memory supports byte, halfword, and word accesses;
- address-valid and fault regions differ;
- only the behavioral memory exposes preload/readback/history APIs today.

The target contract must parameterize at least capacity, address unit, supported access sizes, alignment, reset behavior, latency/ready behavior, and inspection capabilities. An implementation is eligible only when it supports the exact request. The 4/32-word structures are lower-level evidence for the full memory abstraction, not direct substitutes for a 64K-word request.

### 6.4 The full RV32I cores are not the same contract today

The core schema probe found common inputs but different outputs.

Only `RV32ISingleCycleCore` has:

- `TRAP_CAUSE[3:0]`
- `INSTRUCTION_ATTEMPT`

Only `BehavioralRV32ICore` has:

- instruction-memory write data/enable, size, sign extension, clock, and reset outputs;
- data-memory clock and reset outputs.

More importantly, `BehavioralRV32ICore` does not execute from its memory response pins. It uses `attachMemories()` and calls the functional oracle against concrete `BehavioralMemory64Kx32` objects. Its memory bus outputs primarily publish the already-computed access for visualization and memory side effects. It is therefore an oracle-backed system adapter, not currently a bus-substitutable implementation of `RV32ISingleCycleCore`.

Do not register these two classes under one core contract yet.

The canonical future core contract should follow the modular structural boundary:

- retain the nine common clock/enable and memory-response inputs;
- retain the structural instruction read-only and data read/write ports;
- retain `TRAP_CAUSE` and `INSTRUCTION_ATTEMPT` observability;
- let the system wrapper distribute memory clock/reset and provide instruction-memory constants;
- eliminate hidden concrete-memory attachment from a selectable core.

Either rework the behavioral core to execute solely through that pin contract, or keep it explicitly named and registered as a reference-system implementation rather than a core implementation.

### 6.5 The two system wrappers are candidates, not yet transparent substitutes

`RV32ISingleCycleSystem` and `RV32ISystem` both expose:

```text
inputs:  CLK, RST, ENABLE
outputs: PC[31:0], HALTED, TRAPPED
```

Both pass the 16 shared program families through independent lockstep against the instruction oracle. However:

- their internal core buses and timing differ;
- setup/state/trace methods differ;
- the behavioral system relies on the core's hidden concrete-memory attachment;
- the structural state snapshot depends on child names/topology.

They can become implementations of one system contract after capabilities and timing are normalized. Until then, describe them as `educational.single-cycle.hybrid` and `reference.oracle-backed`, not simply `structural` and `behavioral` twins.

### 6.6 Current structural purity is intentionally hybrid

The current educational RV32I system is not fully gate-level:

- `RegisterFile32x32` composes decoders/muxes around 31 `BehavioralRegister32` word cells.
- `BehavioralRegister32` is itself a composite of 32 `BehavioralMemoryBit` cells.
- `Memory4x32` and `Memory32x32` also use `BehavioralRegister32` words.
- `RV32ISingleCycleSystem` uses two `BehavioralMemory64Kx32` instances.

This is consistent with the lower-level-first scale policy because structural bit/word and small-memory examples exist. The unified model should call the result hybrid and show exactly where abstraction boundaries occur.

## 7. Production Class Migration Map

This section accounts for all 81 component-derived module declarations.

### 7.1 Primitive/evaluated singleton contracts

| Current classes | Count | Target treatment |
| --- | ---: | --- |
| `NOTGate`, `ANDGate`, `NANDGate`, `ORGate`, `NORGate`, `XORGate` | 6 | Register primitive gate contracts/operations. Keep evaluated truth-function implementations as the lower-level leaves; do not invent behavioral twins. |
| `ClockGenerator` | 1 | One clock-source contract parameterized by half-period. Preserve initial/self-scheduling behavior. |
| `BitSplitter`, `BitJoiner` | 2 | Parameterized adapter contracts keyed by width. Current template instantiations cover 1, 2, 3, 4, 5, 8, 16, and 32. |
| `ConstantValue` | 1 | Parameterized constant-source contract keyed by output width and value; remove `LEGACY_WIDTH` from canonical identity. |
| `Rewire` | 1 | Configured routing primitive keyed by input/output schemas, mappings, and unmapped-bit policy. Preserve the supported-width validation. |

### 7.2 Structural primitive families

| Current classes | Count | Target treatment |
| --- | ---: | --- |
| `SRLatch`, `GatedDLatch`, `DFlipFlop` | 3 | Separate sequential contracts with structural netlist implementations. Their hierarchy is meaningful and should remain visible. |
| `Decoder2to4`, `Decoder5to32` | 2 | One parameterized enabled-decoder family, preserving current pin naming. |
| `AND8`, `OR8`, `XOR8`, `NOT8`, `NAND8`, `NOR8` | 6 | Parameterized bitwise-logic contracts or operation descriptors with width 8; one structural lane-composition implementation each. |

### 7.3 Multiplexer family

The following 14 classes are one natural parameterized family:

```text
Mux2to1
Mux4to1
Mux8to1
Mux16to1
Mux32to1
Mux2to1_4bit
Mux2to1_8bit
Mux2to1_32bit
Mux4to1_8bit
Mux4to1_32bit
Mux8to1_8bit
Mux8to1_32bit
Mux16to1_8bit
Mux32to1_32bit
```

The contract parameters are input count and word width. Preserve existing `A`/`B` names for 2-to-1 forms and `IN0...INn` for larger forms during migration, or version the contract if names are normalized later. The existing classes can remain implementation shims until call sites migrate.

### 7.4 Arithmetic, comparison, shift, and ALU composites

| Current classes | Count | Target treatment |
| --- | ---: | --- |
| `HalfAdder`, `FullAdder` | 2 | Separate canonical low-level contracts with structural gate compositions. |
| `Adder8`, `Adder32` | 2 | One width-parameterized ripple-adder family where pin semantics match. |
| `AddSub32` | 1 | One structural add/subtract contract. |
| `TwosComplement8`, `Subtractor8`, `SubtractorWithBorrow8`, `Incrementer8`, `Decrementer8` | 5 | Keep distinct logical contracts; share internal helpers rather than forcing unlike pins into one public contract. |
| `EqualityChecker8`, `Comparator8`, `SignedComparator8`, `Comparator32` | 4 | Keep distinct output contracts. Parameterize only where the complete pin/flag semantics match. |
| `ZeroDetect8`, `ZeroDetect32` | 2 | One width-parameterized zero-detect contract. |
| `Logic32` | 1 | Keep its combined three-output contract; it is not the same contract as each single-operation Logic8 block. |
| `ShiftLeftLogical8`, `ShiftRightLogical8`, `ShiftRightArithmetic8` | 3 | Fixed one-position 8-bit shift contracts; current `Rewire` implementation remains an evaluated routing leaf inside a composite. |
| `Shifter32` | 1 | Combined 32-bit barrel-shifter contract with three result outputs. Do not conflate it with the fixed 8-bit shifters. |
| `ALU8` | 1 | Structural 8-bit ALU contract. |
| `ALU32`, `BehavioralALU32` | 2 | Two implementations of `rv32i.alu32`, initially certified for stable known-binary behavior. |

### 7.5 Memory and register components

| Current classes | Count | Target treatment |
| --- | ---: | --- |
| `MemoryBit`, `BehavioralMemoryBit` | 2 | Two implementation candidates for a write-enabled rising-edge bit contract; add direct isolated equivalence first. |
| `Register32`, `BehavioralRegister32` | 2 | `Register32` is the structural candidate. The current behaviorally named word is a legacy mixed composition of behavioral bit cells; either retain it as a structural/mixed recipe or rewrite it as a direct `BehavioralComponent` before registering whole-register behavioral fidelity. Then add isolated equivalence. |
| `RegisterFile4x32` | 1 | Parameterized/composed representative register-file implementation for four registers; address width is part of the contract parameters. |
| `RegisterFile32x32`, `BehavioralRegisterFile32x32` | 2 | Two implementations of the full RV32I register-file contract, known-binary pair-certified. |
| `Memory4x32`, `Memory32x32` | 2 | Parameterized structural teaching-slice implementations for exact capacities/access features only. |
| `BehavioralMemory64Kx32` | 1 | Full 65,536-word behavioral memory implementation with preload and history capabilities. It is backed by lower-level representative slices, not directly interchangeable with them. |

### 7.6 RV32I circuit components

| Current classes | Count | Target treatment |
| --- | ---: | --- |
| `RV32IBitPatternMatcher` | 1 | Parameterized structural matcher contract keyed by mask and value. |
| `RV32IControlFlowUnit`, `BehavioralRV32IControlFlowUnit` | 2 | Two known-binary implementations of `rv32i.control-flow`. |
| `RV32IDecodeControlUnit`, `BehavioralRV32IDecodeControlUnit` | 2 | Two known-instruction implementations of `rv32i.decode-control`. |
| `RV32IExecutionControlStatusUnit`, `BehavioralRV32IExecutionControlStatusUnit` | 2 | Two known-binary cycle-boundary implementations of `rv32i.execution-status`. |
| `RV32ISingleCycleCore` | 1 | Current selectable educational core implementation after its children migrate to contract requests. |
| `BehavioralRV32ICore` | 1 | Keep reference-only until its bus contract and hidden memory coupling are corrected. Do not group with the structural core yet. |
| `RV32ISingleCycleSystem` | 1 | Current educational hybrid system implementation. |
| `RV32ISystem` | 1 | Current oracle-backed reference system candidate; normalize capabilities/timing before grouping with the educational system. |

Total: 81 declarations.

## 8. Non-Component Classes and Why They Stay Separate

The following RV32I types are functional services/data, not event-driven hardware nodes:

- `RV32IDecoder`
- `RV32IControl`
- `RV32IFunctionalMemory`
- `RV32IProgram`
- `RV32IInstructionOracle`
- decoded instruction, control, architectural-state, and trace structs/enums

They should remain independent semantic oracles, loaders, and data models. They may implement or consume generic capabilities, but they should not be registered as circuit implementations. In particular, the oracle is valuable because it is independent of the structural topology; turning it into the hidden behavior of a structural contract would weaken differential testing.

Framework classes under the migration include `Component`, `IOComponent`, `BasicComponent`, `ComponentBuilder`, `WireBuilder`, `PinBase`, `Pin<WIDTH>`, `WireBase`, `Wire<WIDTH>`, event classes, `Simulator`, and simulation-test base classes.

Test-only/private helpers such as `AddSub4Slice`, `LogicNetlist`, `NonMutatingBehavioralMemory64Kx32`, binding trampoline classes, and expected-value structs remain helpers rather than catalog entries.

The 13 Python classes—`Camera`, the two `LayoutManager` classes, `App`, `Button`, `Slider`, `VisualPin`, `VisualWire`, `VisualComponent`, `VisualIOComponent`, `CircuitSession`, `SessionStore`, and `VisualizerHandler`—consume component/test metadata. They do not participate in C++ implementation selection.

## 9. Test Model After the Refactor

### 9.1 One scenario definition, several independent executions

Test vectors and expected values should be defined once per contract. The runner then executes each registered implementation in its own component tree and simulator.

```text
shared stimulus/expected data
        |
        +-- structural implementation -> independent trace -> expected checks
        |
        +-- behavioral implementation -> independent trace -> expected checks
        |
        +-- trace comparator
```

This preserves the user's desired separate visual scenarios and prevents equivalence from depending on two devices sharing one event queue or waveform topology. Pair/equivalence tests remain non-visual regressions.

### 9.2 Required test layers

For every contract:

1. **Schema test**: exact pins, widths, directions, parameters, version, and required capabilities.
2. **Independent contract suite**: every implementation checked against hard-coded expected values or an independent semantic oracle.
3. **Implementation-specific suite**: topology, timing, history, unknown propagation, or optimization behavior unique to that implementation.
4. **Isolated equivalence suite**: compare separately recorded observations only within the declared semantic and timing domain.
5. **Structural integrity suite**: verify claimed lower-level children and forbid hidden reference/oracle dependencies.
6. **Profile integration suite**: build representative mixed trees and verify the resolved manifest.
7. **Visualizer/layout suite**: topology metadata, stable layout identity, component-state capabilities, and scenario exposure.
8. **Performance suite**: component/event counts and simulation time, reported separately from correctness.
9. **Profile-generator suite**: deterministic generation/fingerprints, depth-zero and boundary behavior, strict failure, recorded exceptions, override precedence, and descriptor round trips.

### 9.3 Known values, four-state logic, and timing

Do not label an implementation fully interchangeable when only known-value tests pass. Test metadata should select domains explicitly:

```text
known-binary / stable-after-settle
four-state / stable-after-settle
known-binary / clock-boundary
exact-event-timing
```

`ComponentRowsTest` and `runRows()` already isolate rows to discover settling time before running persistent scenarios. Preserve that strength, but make it instantiate a contract plus explicit implementation/profile instead of a concrete class template.

### 9.4 The 16 RV32I programs

Keep one `RV32ISystemProgramCase` definition per program. Replace the duplicated structural/behavioral test-class families with parameterized registrations:

```text
program case x system profile
```

Visible scenarios should still be separate, for example:

```text
rv32i-system-program9?profile=education
rv32i-system-program9?profile=reference
```

or equivalent stable aliases. The visualizer must never show two answer/DUT systems in one scenario.

Avoid an uncontrolled Cartesian product. Run:

- all 16 programs for required release profiles;
- focused block/profile combinations for exact overrides;
- pairwise covering combinations for optional mixed children;
- exhaustive combinations only for small contracts.

### 9.5 One test/scenario catalog

Today the same test inventory is repeated in `TestRegistry.cpp`, `tests/CMakeLists.txt`, `TestsBinding.cpp`, and Python visualizer alias/exclusion tables. Replace this with one declarative test catalog containing:

```text
test ID
factory or parameterized factory
contract/program case
profile/implementation
visualizable flag
stable aliases and label
tags and expected cost
```

CMake test discovery, pybind exposure, and visualizer scenarios should consume generated/runtime catalog metadata. Python should not need a manual `NON_VISUALIZABLE_TESTS` list or class-name-derived aliases.

## 10. Visualizer and Layout Migration

The browser currently keys most defaults by `getTypeName()` and contains manual cases for splitter/joiner widths, constant width, and matcher mask/value. That is evidence that C++ class name alone is not enough identity.

Use this layout identity:

```text
contract-id@version
+ normalized topology-affecting parameters
+ implementation-id
+ optional topology-signature version
```

Use instance path only for per-instance overrides. Include the build-profile fingerprint in scenario/root identity so layouts from a compact behavioral tree do not overwrite structural layouts.

The API should provide contract/implementation metadata directly. The UI should display both, for example:

```text
ALU32
behavioral.direct
known-binary verified
```

or:

```text
RegisterFile32x32
composed.decoder-mux
effective subtree: mixed
```

Component-specific panels should query capabilities rather than compare type strings such as `BehavioralRegisterFile32x32` and `BehavioralMemory64Kx32`.

Existing layouts need an alias migration table from legacy type names and scenario keys. Do not delete old layouts; read old keys, translate when unambiguous, and save the new identity only after a successful load.

## 11. Staged Migration Plan

Each phase must leave the tree buildable and retain the legacy path until its call sites are migrated.

### Phase 0: Freeze contracts and close evidence gaps

Goal: characterize behavior before changing construction.

Work:

- Add machine-readable snapshots for all current production pin schemas.
- Document semantic domain and observation timing for every existing pair.
- Add direct isolated equivalence for `MemoryBit`/`BehavioralMemoryBit`.
- Characterize the current `BehavioralRegister32` composition, then add a direct whole-register behavioral implementation and isolated equivalence before treating that name as whole-register behavioral fidelity.
- Add characterization tests for side capabilities and state-at-time behavior.
- Record current component/event counts for representative visual scenarios.
- Keep the current 143 CTest entries green.

Exit gate: no behavioral implementation planned for selectable use lacks lower-level evidence and an independent contract test.

### Phase 1: Add metadata without changing behavior

Goal: make logical identity visible while construction still uses concrete classes.

Suggested new areas:

```text
include/components/contracts/
include/components/selection/
src/components/contracts/
src/components/selection/
```

Work:

- Add contract, parameter, implementation, capability, and verification metadata types.
- Attach optional legacy metadata to `Component` instances.
- Define canonical IDs and versions for every production family.
- Add metadata bindings and read-only visualizer display.
- Validate the 81-class inventory against the migration map.

Exit gate: every production instance can report a contract candidate, concrete implementation identity, parameters, selected fidelity, and any terminal-primitive/reference marker even though legacy constructors still choose it.

### Phase 2: Enforce the fidelity/mechanism invariant

Goal: represent structural blueprints, direct behavioral implementations, and terminal primitives honestly without adding another profile choice.

Work:

- Introduce `EvaluatedComponent` and compatibility-alias/deprecate `BasicComponent`.
- Introduce `StructuralComponent`, `BehavioralComponent`, and `PrimitiveComponent` with the mapping defined in section 5.9.
- Migrate direct structural subclasses mechanically and mark evaluated gates/utilities as terminal primitives.
- Flag the current composed `BehavioralRegister32` as a legacy naming mismatch; do not register it as whole-register behavioral fidelity until it is direct.
- Make event dispatch use explicit execution behavior rather than a `BasicComponent` naming convention.
- Add registration tests that reject behavioral implementations with functional children and structural implementations without a blueprint, except declared primitive endpoints.
- Preserve delays, initial scheduling, wire-boundary propagation, and history exactly.

Exit gate: all tests pass with no implementation-selection feature enabled.

### Phase 3: Centralize contract schemas

Goal: make the contract the source of truth for pins and parameters.

Work:

- Move duplicate pair pin definitions into shared contract descriptors.
- Replace constructor pin lambdas/macros gradually with descriptor installation.
- Add registration-time schema assertions.
- Canonicalize parameter serialization and hashing.
- Remove `LEGACY_WIDTH` from new constant requests while keeping source compatibility.

Exit gate: changing one pair's pin contract requires editing one schema, and either mismatched implementation fails its schema test.

### Phase 4: Add catalog, profile, context, and manifest

Goal: support logical construction without migrating all modules at once.

Work:

- Implement explicit catalog registration and freeze semantics.
- Implement immutable profiles and deterministic rule resolution.
- Implement the abstract `ProfileGenerator` interface, strict/maximal/depth/preset generators, and composable overrides.
- Serialize the generator descriptor, arguments, unavailable-fidelity policy, and generated-profile fingerprint.
- Thread `BuildContext` through `Component::create`/`ComponentBuilder` compatibility layers.
- Add contract-based `builder.add<Contract>()` and `ComponentHandle`.
- Record every selection in a resolved-build manifest.
- Add strict failure tests for missing implementation, parameter mismatch, capability mismatch, domain mismatch, conflicting rules, and unavailable fidelity at a depth boundary.
- Add generator tests for deterministic output, root-depth semantics, override precedence, explicit exception recording, and strict versus maximal structural behavior.

Exit gate: a test tree can mix legacy explicit construction and new logical construction, and its manifest exactly explains the result.

### Phase 5: Pilot structural/behavioral selection

Goal: prove that the architecture handles the hardest small case.

Order:

1. A singleton primitive gate, to validate the no-choice path.
2. `BitSplitter` or `Mux`, to validate parameterized identity.
3. `MemoryBit`, to validate structural-blueprint versus direct-behavioral selection and state.
4. `Register32`, after replacing or supplementing the current composed behaviorally named class with a direct behavioral whole-register implementation; use the legacy composition separately to validate recursive child selection.
5. `ALU32`, to validate a large combinational same-contract pair.

Exit gate: the same parent recipe and tests run with explicit structural and behavioral bit/register/ALU selections, with separate visual topologies and correct manifests.

### Phase 6: Migrate all singleton and parameterized families

Goal: apply the unified model to every current class, not just paired RV32I blocks.

Work:

- Migrate gates, clock, latch/DFF, utilities, decoders, muxes, logic, adders, arithmetic, comparators, shifters, and ALU8.
- Replace concrete child includes and typed pin lookup with contracts/handles.
- Keep legacy concrete names as deprecated construction aliases where external code needs them.
- Do not create artificial behavioral implementations for classes with one valid realization.

Exit gate: every non-memory/non-RV32I production module is catalog-backed.

### Phase 7: Migrate storage and capability APIs

Goal: make compact storage selectable without topology-dependent inspection.

Work:

- Migrate register and register-file families.
- Normalize `BehavioralRegister32` before registering it as the direct behavioral implementation of the register-word contract.
- Parameterize memory requests by exact capacity and access features.
- Add `MemoryImage`, `MemoryHistory`, and `RegisterStateView` capabilities.
- Update program loading, tests, bindings, and visualizer state panels to capabilities.
- Replace `RV32ISingleCycleCore::snapshotState()` child-name traversal.

Exit gate: switching the 32x32 register file implementation does not break snapshot, tests, or visualization; a 64K memory request cannot resolve to a 4/32-word slice.

### Phase 8: Migrate the five RV32I blocks and profiles

Goal: build the educational core from logical children.

Work:

- Register the five existing block pairs with their honest known-binary/cycle-boundary evidence.
- Change `RV32ISingleCycleCore` to request the five contracts.
- Define and test generated `education` and `balanced` profiles, structural-through-depth profiles, and exact path overrides.
- Add integrity checks that structural recipes do not invoke the behavioral answer sheet or oracle internally.
- Run all 16 programs for required profile combinations.

Exit gate: the user can choose structural or proven behavioral implementations for each major child without editing core source, and the manifest/visualizer shows the actual mixture.

### Phase 9: Normalize core and system contracts

Goal: make full-core and system selection honest.

Work:

- Version and adopt the canonical core pin contract described in section 6.4.
- Rework or reclassify `BehavioralRV32ICore` so no selectable core bypasses pins through concrete memory pointers.
- Introduce RV32I initial-state, architectural-state, and trace capabilities.
- Converge the duplicated system wrappers onto one system recipe once core buses match.
- Register educational hybrid and reference system implementations with explicit timing and role metadata.
- Add isolated all-16-program differential runs between system profiles.

Exit gate: full system selection is a profile choice, not a choice between two unrelated public wrapper classes.

### Phase 10: Unify tests, bindings, and visual scenarios

Goal: remove concrete-class duplication from external surfaces.

Work:

- Replace the four current test/scenario lists with one catalog.
- Generate/discover CTest entries and pybind scenarios from it.
- Bind generic component construction/metadata and capabilities.
- Expose generator presets, the structural-depth control, strict/maximal behavior, and the generated descriptor through the bindings and visualizer.
- Keep separate visible implementation/profile scenarios.
- Migrate layout keys and remove type-string special cases where metadata replaces them.

Exit gate: adding one implementation requires one implementation registration plus its test evidence, not edits in CMake, C++ registry, bindings, and Python alias tables.

### Phase 11: Remove legacy names and paths

Goal: finish the refactor only after downstream code is migrated.

Work:

- Move concrete implementation classes into internal namespaces or `.cpp` scope where practical.
- Remove deprecated `BehavioralA` public construction APIs and legacy concrete child lookups.
- Remove `BasicComponent` terminology after all consumers use `EvaluatedComponent`.
- Retain aliases only for serialized-layout migration where needed.

Exit gate: production composition refers only to contracts, parameters, capabilities, and profiles.

## 12. Acceptance Criteria

The migration is complete when all of the following are true:

- Every one of the 81 current production component declarations is represented by a contract and one or more registered implementations, or is deliberately retained as a deprecated alias.
- Parent implementation source does not name selectable child concrete classes.
- Structural registration always maps to a child-and-wire blueprint (or declared terminal primitive), and behavioral registration always maps to a direct evaluated black box, without parent code choosing the mechanism.
- Contract registration catches pin, parameter, capability, semantic-domain, and observation-timing incompatibility.
- Behavioral defaults satisfy lower-level-first evidence requirements.
- `strict-structural` fails explicitly wherever only a behavioral scale abstraction exists.
- Strict, maximal, behavioral, preset, and structural-through-depth generators produce deterministic immutable profiles; generator settings and exceptions are serializable and fingerprinted.
- A resolved manifest can answer exactly which implementation was used at every path and whether the effective subtree is mixed.
- State preload, snapshot, history, and trace tools work through capabilities after implementation swaps.
- The five major RV32I block pairs pass independent contract suites and isolated equivalence in their declared domains.
- The 16 RV32I program cases run from one set of definitions across all required system profiles.
- Visual scenarios remain one DUT/system per scenario, and layouts are isolated by contract, implementation, parameters, and profile fingerprint.
- Existing tests are retained or replaced by equivalent catalog-driven tests, with no loss of four-state, timing, topology, or visualizer coverage.

## 13. Principal Risks and Controls

| Risk | Control |
| --- | --- |
| A profile silently changes hardware meaning | Exact parameter/capability matching, explicit baselines, no silent fallback, resolved manifest. |
| “Structural” is inferred from class name | Validate the registered fidelity against `StructuralComponent`/`BehavioralComponent`; calculate the effective structural/behavioral/mixed subtree from the manifest. |
| A convenient generator silently weakens its promise | Give unavailable fidelity an explicit `Error` or recorded-exception policy; reserve “strict” names for error behavior. |
| A generated experiment cannot be reproduced | Freeze and serialize the generated rules, generator descriptor/arguments, catalog version, and profile fingerprint before construction. |
| Behavioral model becomes the only source of truth | Require lower-level evidence and independent expected-value/oracle tests before product selection. |
| Partial-X differences are hidden | Register conformance domains and run separate four-state suites. |
| Faster implementation changes cycle timing | Version observation/timing contracts and validate clock/settling requirements per profile. |
| Concrete side APIs break substitution | Move preload/state/history/trace behavior to required capabilities. |
| Layouts overlap or overwrite one another | Key layouts by contract, implementation, parameters, topology version, and profile fingerprint. |
| Refactor becomes a big-bang rewrite | Keep `Component::create<T>()` and legacy bindings during staged migration; convert one family at a time. |
| Catalog startup depends on linker/static order | Explicit deterministic registration followed by catalog freeze. |
| Test combinations become unbounded | Required profiles plus pairwise covering combinations; exhaustive only for small blocks. |

## 14. Recommended First Implementation Slice

Do not begin with the full RV32I core. Begin with infrastructure plus `MemoryBit`:

1. Add contract/implementation/profile metadata and an explicit catalog.
2. Add the missing isolated `MemoryBit` equivalence suite.
3. Register `memory.write-enabled-bit` with `structural.mux-dff` and `behavioral.direct` implementations.
4. Add strict-all-structural, maximally-structural, strict-all-behavioral, and structural-through-depth generators plus an override decorator.
5. Build a tiny parent that requests the logical bit twice under different generated depth rules and exact path overrides.
6. Verify event dispatch, state/history, generator serialization, manifest output, separate visual scenarios, and layout identity.
7. Normalize the whole-register behavioral implementation, repeat at `Register32`, then migrate `ALU32`.

That slice exercises the central design claim—one logical contract choosing either a structural blueprint or a direct behavioral implementation—at low cost. It also proves that generators automate selection policy without owning the hardware blueprint. Once it works, migrating the five RV32I children is mostly systematic. The mismatched behavioral core and system cleanup should remain a later, explicit contract-normalization phase.
