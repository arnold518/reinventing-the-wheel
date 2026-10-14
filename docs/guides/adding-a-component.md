# Adding a component

This is the current path for a component that can appear in a circuit, a
recorded test scenario, and the public site. Start with the structural design;
add a behavioral form when scale or visual clarity calls for one. A public
contract may have one structural implementation and at most one behavioral
implementation. The [fidelity reference](../reference/unified-component-profiles.md)
explains profile selection; the
[timing reference](../reference/circuit-timing-analyzer.md) explains the delay
model. The [older complete workflow](../archive/complete-circuit-workflow.md)
describes a retired API.

For a small example of the current path, trace `Adder32` from
`include/modules/composite/Adder32.hpp` and
`src/modules/composite/Adder32.cpp` through
`BuiltinComponentCatalog.cpp`, `Adder32Test` in
`src/tests/ALU32LowerLevelSliceTests.cpp`, and
[`site-docs/rv32i/4.3/alu32.md`](../../site-docs/rv32i/4.3/alu32.md).

## 1. Decide the contract and lower-level design

Write down the component's stable contract before implementing it:

- Its catalog ID, public `TypeName`, purpose, pin names, directions and
  supported widths. Check neighboring families before choosing an ID. IDs are
  saved in profiles and layouts; a later rename needs a compatibility plan.
  Memory contracts use `memory.<technology>.<what>.<width>`; state without an
  address uses `storage.*`.
- Settled outputs for normal and boundary inputs, including `X` and `Z` where
  applicable. For stateful parts, specify startup, reset, clock edge, enable,
  retention, and illegal or unknown controls. State any parameter limits and
  clock or request/response protocol.
- The gates, slices or smaller components that establish the behavior. If a
  complete gate-level design is impractical, implement and test a representative
  cell, word or slice before using a behavioral abstraction at scale. Keep a
  temporary functional oracle separate from the product implementation.

Choose the existing `include/modules/<area>/` and `src/modules/<area>/` that
own this contract. Use a primitive gate only for a true terminal primitive;
ordinary composites inherit `IOComponent`. A directly evaluated leaf inherits
`BasicComponent`. Fidelity is a catalog choice, not a C++ base class.

## 2. Build the structural component

Declare the public class in `include/modules/<area>/<Name>.hpp` and its
`circuit::families::<Name>` when it is a selectable family. Define the shared
pin initializer and family in `src/modules/<area>/<Name>.cpp`. The family owns
the public ID, display type, pin initializer and factories. Its structural
factory normally uses `circuit::structuralFactory<T>()` or
`Component::createWithContext<T>(context, name)`. A family with only a
structural form leaves its behavioral factory empty.

Implement `buildInternals(ComponentBuilder&)` using smaller components. Add
children through the builder; use `builder.add(families::Child, "CHILD")` when
the child is selectable by profile. A direct `addNewComponent<T>` is suitable
when that exact structural child is intentional. Use
`circuit::detail::StructuralWiring` from
`include/components/StructuralNetBuilder.hpp`: connect sources to sinks, then
call `finish()` to create deterministic nets. A component input cannot be
wired directly to its own output; use `Rewire` for a pass-through. Include
`ComponentBuilder.tpp` in the `.cpp`, not the public header.

Create components through `Component::create<T>()`,
`Component::createWithContext<T>()` or the builder. A plain
`std::make_shared<T>` bypasses pin and internal construction. Keep names and
wire identities stable when changing an existing component because saved
layouts and profile paths use them.

Add new `.cpp` files, including scenario sources, to `CORE_LOGIC_SOURCES` in
the root `CMakeLists.txt`. Header-only templates need their concrete
instantiations or owning source wired into the build as appropriate.

## 3. Register the contract and make a structural scenario

Register the contract in
`src/components/selection/BuiltinComponentCatalog.cpp`, or in its existing
area-specific catalog (memory does this). Use `addStructural` for a fixed
composite and `addPrimitive` only for a terminal primitive. A two-fidelity
family uses `addSelectableFamily` where its parameters fit that helper;
parameterized families use explicit descriptors. A registered implementation
needs truthful verification evidence. The catalog checks pin schemas on
construction and rejects duplicate fidelities for one contract.

Write one logical component scenario in `include/tests/` and `src/tests/`:

- For a simple sequence of settled input/output rows, follow
  `TruthTableComponentTest` and its `TestRow` expectations.
- For clocked parts, multi-step transactions, parameter sets or named
  checkpoints, use `ComponentScenarioTest` with `ComponentTestSpec`,
  `ActionScenario` and `ScenarioAction`. Drive reset and preparation changes
  explicitly; record checkpoints after the relevant edge or transaction.

Give the scenario independent expected outputs. Check every public output at
useful checkpoints, normal and boundary values, selected `X`/`Z` cases,
reset/retention, invalid requests and parameter boundaries as relevant. A
settled checkpoint checks final values; add a focused timing test when an
exact transition time, setup, hold or intermediate pulse matters. The shared
`ComponentTestRunner` builds separate trees for available fidelities, checks
expectations, and compares their public output snapshots.

Add its `entry<...>` in `src/tests/TestRegistry.cpp` and its name in the
matching `SIMULATION_TESTS` list in `tests/CMakeLists.txt`, in the same order.
`TestRegistryCoverageTest` checks this CTest/registry pairing.
`BuiltinComponentCatalogInventoryTest` requires one logical test owner per
contract, registered evidence IDs and every production `TypeName` in its
expected inventory. Update that inventory for a new public type. A test used
only as lower-level or timing evidence is separate from that logical owner.

## 4. Add a behavioral form when justified

Keep the behavioral class private to `src/modules/<area>/`, usually named
`<Name>Behavioral`. It normally derives from `BasicComponent`, has the same
public `TypeName` and exact pin schema, and implements `evaluate()` without
building child gates. Publish results with `_updateOutputWire`. Reproduce the
structural four-state and state-transition rules, including unknown controls
and clock edges; a binary arithmetic shortcut alone does not establish that
contract. Let larger behavioral parts call the smaller part's shared
behavioral rule instead of copying it. The RV32I instruction oracle stays
independent for comparison.

Add the behavioral factory and catalog descriptor only after the structural
test or representative slice exists. Record the lower-level design, contract
test and equivalence or representative test in the descriptor's
`VerificationEvidence`; catalog `freeze()` requires them. Run the scenario in
both fidelities and add targeted equivalence cases for the states or values
that a single scenario cannot cover. State any restricted equivalence domain
in the contract and public page.

The constructor's scalar delay is only a fallback when there is no applicable
proven table. Do not choose that number as the new component's timing model.

## 5. Generate and validate behavioral delays

For a catalog-backed behavioral `BasicComponent`, the catalog looks up a
`PartDelays` table by implementation ID and parameters and attaches it with
`BasicComponent::setDelays`. `CircuitTimingAnalyzer::delays` derives the
table from the built structural form. It contains earliest and latest
input-to-output arcs, and, for clocked parts, clock-to-output launches and
setup requirements. Its earliest values matter to hold checks; its latest
values bound publication. A supported delay twin can time current values
from a flattened gate list within that table; the table remains the contract.
The simulator counts setup violations when a watched input changes too near
the edge. See `include/components/PartDelays.hpp` and
`src/components/selection/ComponentCatalog.cpp`.

After the new family and tests build, use the generator from the isolated
native build. Generate the **complete** checked-in table file with `--clocked`;
without that flag `--write` skips clocked candidates and would replace the
file with an incomplete one. Rebuild so the catalog compiles the new file,
then verify it:

```sh
cmake -S . -B build-cleanup -DCIRCUITSIM_DEPLOY_PYTHON=OFF
prlimit --as=4294967296 cmake --build build-cleanup -j1
build-cleanup/part_delay_tables --write src/components/selection/ProvenPartDelays.generated.inc --clocked
prlimit --as=4294967296 cmake --build build-cleanup -j1
build-cleanup/part_delay_tables --verify src/components/selection/ProvenPartDelays.generated.inc
build-cleanup/part_delay_tables --coverage
```

Inspect the generated diff and the coverage line for the new implementation.
`--check <contract-id>` is a shorter check of an already compiled, stored
default table. `--verify FILE` recomputes the whole generated file, including
parameter variants, and compares the separately generated no-gate-list
inventory. If gate-list availability changed, regenerate
`src/components/selection/NoGateList.generated.inc` with
`part_delay_tables --gate-lists FILE`, review it and rebuild. Never hand-edit
either generated file.

The generator handles known parameter variants explicitly in
`tools/timing/PartDelayTables.cpp`; add a variant there when construction or
timing really depends on it. `provenPartDelays()` accepts only matching
parameters. Large memory tables may be estimates extrapolated from proven
64- and 128-word circuits; label them as estimates. For loops or self-timed
primitives that ordinary path analysis cannot prove, establish an explicit
lower-level timing contract and tests before catalog use. An empty table or a
`--coverage` gap needs a documented reason and a suitable verification path.

Run a focused delay comparison for the contract without `--report`, which is
diagnostic only:

```sh
build-cleanup/tests/DelayRuleTest <contract-id>
build-cleanup/tests/SettlingBoundTest --verbose <contract-id>
```

The full `PartDelayTablesCompleteTest` and `DelayRuleTest` are slower release
checks. For clocked parts, also run the relevant safe-clock, setup/hold and
system tests; verify `setup_violations == 0` in the intended operating
configuration. A table and sampled delay comparison do not by themselves
prove a system clock or an untested memory parameter set.

## 6. Make the scenario available to frontends

The Python binding and WebAssembly visualizer construct scenarios through
`TestRegistry`; test classes do not need individual Python bindings. Add a
direct `bindCompositeModule<T>` in `src/bindings/ModulesBinding.cpp` only if
callers need to instantiate that C++ class directly from Python.

For a visible scenario, add a stable alias, title and group in
`visualizer-v2/static/scenarios.json`. Check
`VisualizerScenarioCatalogTest` so the new registry entry is classified and
resolves by alias. When the topology or profile changes, generate and review
its saved layout using `visualizer-v2/recalculate_layouts.py`; preview with
`--dry-run` first when existing manual layouts could be affected. Add palette
styling only when the new type needs a distinct appearance. Browser or WASM
behavior outside the common scenario path gets a focused test under
`tests/web/` and an entry in `tests/web-tests.json`.

## 7. Write the public component page

Put the page in the relevant chapter of `site-docs/`, link it from that
chapter's index and add it to `mkdocs.yml` navigation when the chapter uses
explicit entries. Document the implemented component, not a design plan.
Existing component pages use this order; follow the neighboring page's
numbering and heading level:

```markdown
# § <chapter>. <Component group>

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| `<Name>` | <one-sentence purpose> | Yes | Yes |

## <number>. `<Name>`

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `A` | Input | 32 | <meaning> |

### Behavior

<Settled outputs, state/handshake, reset and edge rules, and edge cases.>

#### C++ example

<A small compilable use or reference example, when the chapter uses one.>

#### Unknown inputs and timing

<X/Z scope, timing contract, clock assumptions and limitations.>

### Implementation

#### Structural implementation

<Children and signal path.>

#### Behavioral implementation

<State/rule and validated abstraction, or state that none exists.>

### Examples

<!-- simulator-examples: <scenario-alias> <RegisteredTestName> -->

#### Embedded simulation

<Embed and open link using the same alias and the page's relative simulator path.>

### Tests

<Contract, equivalence, timing and integration tests, with their actual scope.>
```

Match the IO table to the catalog pin schema, including widths and directions.
Explain any structural/behavioral difference in transient signals or supported
four-state domain. Use actual test expectations for examples: the
`simulator-examples` marker is replaced with verified scenario rows by
`tools/mkdocs_examples.py`. `tools/build-pages.sh` regenerates rows from the
WASM scenario before the strict MkDocs build. Do not type or copy a second
example table by hand. Match the alias to `scenarios.json`, and use the exact
registered test name. Follow a neighboring page's simulator iframe/link
pattern and run its relevant documentation test if the chapter has one.

## 8. Verify the complete path

Build serially on this host. The default `build/` also holds experiment
evidence, so use the isolated directory above. First run the new scenario,
catalog inventory and registry checks; then the relevant timing and system
checks. For example:

```sh
build-cleanup/tests/SimulationTestRunner <RegisteredTestName>
ctest --test-dir build-cleanup -R 'BuiltinComponentCatalogInventoryTest|TestRegistryCoverageTest|VisualizerScenarioCatalogTest' --output-on-failure -j1
ctest --test-dir build-cleanup -j1 --output-on-failure
```

After a public page change, run a strict MkDocs build with current example
data. For a new marker or WASM-visible scenario, use `tools/build-pages.sh`
to regenerate and verify the WASM example bundle before claiming the site
works. Run `tools/run-web-tests.py` for browser or WASM changes. Check the
rendered page, its simulator link, pin table, generated rows and relevant
chapter documentation tests. Record which checks ran and their limits in the
topic record and changelog, as required by `AGENTS.md`.
