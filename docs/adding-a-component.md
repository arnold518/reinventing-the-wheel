# Adding a component: current checklist

This is the working checklist for the code as it is today. The longer
[complete circuit workflow](complete-circuit-workflow.md) is a historical
walkthrough of the December 2025 API and is kept only for its explanations.
Background: [component fidelity profiles](unified-component-profiles.md) and
[test organization](test-organization.md).

## Rules that apply before you start

- **Lower level first.** Build the structural implementation (gates, slices,
  composites) before a behavioral one. A behavioral implementation is an
  abstraction of a tested structural design, never the source of truth.
- **One contract, at most one structural and one behavioral implementation.**
- **A behavioral rule has one owner.** If your behavioral implementation needs
  the result of a smaller component (for example the ALU), call that
  component's behavioral function (`alu32Result` in
  `src/modules/composite/ALU32Direct.hpp`); do not restate it. The instruction
  oracle in `src/rv32i/` stays independent on purpose.
- **Catalog contract IDs are stored in saved profiles and layouts.** Do not
  rename or remove one without a compatibility plan. Removing an `rv32i.*`
  contract also changes the RV32I preview profile's fingerprint unless its ID
  is added to the retired list in `src/modules/rv32i/RV32IBuildProfiles.cpp`.

## Steps

1. **Header** in `include/modules/<area>/`: a class deriving `IOComponent`
   with `buildInternals(ComponentBuilder&)`, plus
   `extern const ComponentFamily <Name>;` in `circuit::families`.
2. **Source** in `src/modules/<area>/`: the pin function, the
   `ComponentFamily` (contract ID, type name, pins, structural factory,
   behavioral factory), and `buildInternals`. `circuit::structuralFactory<T>()`
   (`components/selection/FamilyFactories.hpp`) is the standard structural
   factory. Wire with
   `circuit::detail::StructuralWiring`
   (`include/components/StructuralNetBuilder.hpp`). Always create components
   through `Component::create` / `createWithContext` or the builder, never
   `std::make_shared`.
3. **Behavioral implementation**, if any: a `BasicComponent` with `evaluate()`.
   Its header is private and lives next to the source in `src/modules/<area>/`.
4. **Build list**: add new `.cpp` files to `CORE_LOGIC_SOURCES` in the root
   `CMakeLists.txt`.
5. **Catalog**: register the contract and its implementations in
   `src/components/selection/BuiltinComponentCatalog.cpp` (`addStructural` or
   `addSelectableFamily`), or in the area's own `register…` function for
   memory parts. Evidence strings must name tests that exist.
6. **Scenario test**: a `ComponentScenarioTest` (or `TruthTableComponentTest`)
   in `include/tests/` and `src/tests/`. Then:
   - add an `entry<…>` to `src/tests/TestRegistry.cpp`;
   - add the test name to the matching list in `tests/CMakeLists.txt`;
   - add the type to the inventory in `src/tests/ComponentSelectionTests.cpp`.
7. **Python binding**: one `bindCompositeModule<…>` line in
   `src/bindings/ModulesBinding.cpp` (current convention; frontends otherwise
   reach components through the test registry).
8. **Visualizer**: give the scenario a title and group in
   `visualizer-v2/static/scenarios.json`, then regenerate layouts with
   `python3 -u visualizer-v2/recalculate_layouts.py`. Add a colour to
   `visualizer-v2/palette.json` if the type should stand out.
9. **Site documentation**: a page under `site-docs/` following
   *IO pins → behavior → implementation → examples → tests*. Example tables
   come from the recorded scenario through
   `<!-- simulator-examples: ALIAS TEST -->`; do not type rows by hand.
   Standalone browser or WASM checks go in `tests/web/` and are listed in
   `tests/web-tests.json`.

## Build and verify

Build and test serially in the isolated directory; this host runs out of
memory with parallel compiles.

```sh
cmake -S . -B build-cleanup -DCIRCUITSIM_DEPLOY_PYTHON=OFF
prlimit --as=4294967296 cmake --build build-cleanup -j1
ctest --test-dir build-cleanup -j2
```

`build/` holds experiment results and validation evidence; it is not a
scratch directory. The published site (including the WebAssembly simulator)
is built with `tools/build-pages.sh` and checked with
`tools/run-web-tests.py`.

A refactor that should not change any circuit can be proven by comparing the
component, pin and wire lists of every registered scenario before and after;
the test suite alone does not show that wire names and order are unchanged.
