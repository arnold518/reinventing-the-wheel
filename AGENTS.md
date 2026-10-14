# Working in CircuitSim

This file is the working rule set for coding agents. Before changing code or
documentation, read [the current status](docs/STATUS.md), the
[internal documentation index](docs/README.md), and the relevant public pages
under `site-docs/`. Check current source and tests before relying on an older
plan or report. Read the open record for a topic before continuing its work.

## Project Overview

CircuitSim is a digital logic circuit simulator: an event-driven engine in C++20, a browser visualizer (native Python backend or serverless WebAssembly), and a documentation site. It simulates circuits at gate level with propagation delays and builds complex parts from simpler ones (composite pattern).

What exists today:

- **ALU8** (site chapter 2): a teaching 8-bit ALU built from gates.
- **Memories** (chapter 3): flip-flop, SRAM, DRAM, ROM and NOR-flash devices, each as a detailed design (cell -> row/word -> bank -> device) and as a scalable `MemoryContainer`; controllers; and a common request/response endpoint.
- **RV32I** (chapter 4): building blocks, a single-cycle core, a baseline five-stage pipeline (stall only) and an advanced pipeline (forwarding, load-use hazards, branch prediction, memory waits), plus an instruction oracle, benchmark and verification equipment.

## Architectural Rule: Lower-Level First

This project prioritizes learning hardware by building lower-level structures before hiding them behind behavioral code.

- Implement gates, slices, and composite components first whenever feasible.
- Add behavioral modules only when the equivalent lower-level implementation would be too large, too slow, or too visually noisy to use everywhere.
- A behavioral module must be treated as an abstraction of an existing lower-level component or representative lower-level slice, not as the original source of truth.
- Before adding a product-facing behavioral module, add tests for the lower-level implementation and equivalence tests proving the behavioral module matches the same external contract.
- If a full lower-level implementation is impractical, build a representative slice first, such as a 1-bit ALU cell, 4-bit ALU slice, one register cell, one register word, or tiny memory array.
- Temporary functional or behavioral code may be used as a reference oracle or bring-up tool, but it must be clearly named and must not replace the lower-level design path.

## Build System

Build and test **serially**; parallel compiles run this host out of memory.

```bash
# Isolated build that does not overwrite the live visualizer module
cmake -S . -B build-cleanup -DCIRCUITSIM_DEPLOY_PYTHON=OFF
prlimit --as=4294967296 cmake --build build-cleanup -j1
ctest --test-dir build-cleanup -j2

# One scenario
build-cleanup/tests/SimulationTestRunner FullAdderTest
build-cleanup/tests/SimulationTestRunner --list
```

- The default `build/` directory also holds experiment results and validation evidence. Do not delete it as scratch.
- Without `CIRCUITSIM_DEPLOY_PYTHON=OFF`, the build copies `circuit_backend*.so` into `visualizer-v2/` and `visualizer/`.
- The published site (docs plus WebAssembly simulator) is built by `tools/build-pages.sh` into `build-wasm/site`; browser and parity checks run through `tools/run-web-tests.py` (inventory in `tests/web-tests.json`).
- Pushing the `github-pages` branch deploys the site.

## Code Architecture

### Engine

- `include/basic`: `Pin<W>` and `Wire<W>` are width-templated and four-state (0, 1, X, Z). Supported widths are 1, 2, 3, 4, 5, 8, 16 and 32.
- `include/simulator`: a priority queue ordered by time, then wire updates before component evaluations, then scheduling order. Wire and pin history is recorded for time scrubbing and can be disabled for headless runs.
- Each pin has an external wire (parent scope) and an internal wire (child scope). A component input cannot be wired directly to its own output; a zero-delay `Rewire` bridges it.

### Components

```
Component
  └─ IOComponent        (pins; composites implement buildInternals())
      └─ BasicComponent (delay; leaves implement evaluate())
```

- Create components with `Component::create<T>()`, `createWithContext`, or a `ComponentBuilder`; never `std::make_shared`, or pins and internals are not built.
- `ComponentBuilder` templates live in `ComponentBuilder.tpp`; include it in `.cpp` files, not headers.
- Wire structural circuits with `circuit::detail::StructuralWiring` (`include/components/StructuralNetBuilder.hpp`): call `connect(source, sink)` and then `finish()`.

### Fidelity selection (`include/components/selection`)

- A `ComponentFamily` is one public contract: an ID, pins, a structural factory and a behavioral factory. There is at most one implementation of each kind.
- Parents ask for a family, not a class: `builder.add(circuit::families::ALU32, "ALU")`.
- One `BuildProfile` (ordered rules, last match wins) chooses structural or behavioral per path; a `BuildManifest` records what was built. See `docs/reference/unified-component-profiles.md`.
- Behavioral classes are usually named `...Behavioral`; their headers are private, under `src/modules`.
- A behavioral rule has one owner. Larger behavioral implementations call the smaller component's function (for example `alu32Result`) instead of restating it. The instruction oracle in `src/rv32i` is deliberately independent.
- A behavioral part runs on the delays proven on its gate-level form: a `PartDelays` table per input-output pair, generated by `part_delay_tables` and given to the part by the catalog. See `docs/reference/circuit-timing-analyzer.md`. After changing a structural circuit, regenerate the tables.
- Memory contracts are named `memory.<technology>.<what>.<width>`; registers and state bits are `storage.*`. Retired names are mapped by `circuit::canonicalContractId`.
- Contract IDs are stored in saved profiles and layouts. Renaming or removing one needs a compatibility plan; see the retired list in `src/modules/rv32i/RV32IBuildProfiles.cpp`.

### Tests and scenarios

- `src/tests/TestRegistry.cpp` is the single registry used by CTest (`SimulationTestRunner`), the Python binding (`create_test_by_name`) and the WebAssembly build.
- Component tests run every available fidelity and compare them. Program tests compare cores against the instruction oracle.
- See `docs/reference/test-organization.md` and `docs/reference/benchmark-system.md`.

### Visualizer

- `visualizer-v2/`: `server.py` (native backend and layout computation) and `static/` (canvas app). The same UI runs without a server through `src/wasm/WasmBindings.cpp` and `VisualizerSession`.
- `visualizer-v2/static/scenarios.json` defines menu titles and groups; `layout.json` holds saved layouts keyed by type, by profile fingerprint and by scenario root.
- `visualizer/` is the older Pygame viewer.

## Documentation and automatic recording

Maintain documentation as part of the work, without waiting for a separate
request. Keep each change and its documentation independently reviewable.

- `site-docs/` is the public MkDocs source. Describe the implemented behavior
  of components and the visualizer there. Component pages cover IO pin names,
  directions and widths, behavior, internal structure, examples, edge cases and
  tests. Update the relevant page in the same change as a public contract or
  behavior change. Rewrite outdated descriptions instead of appending project
  history to a current reference page. Example tables come from recorded
  scenarios through `<!-- simulator-examples: SCENARIO TEST -->` markers; never
  type their generated rows by hand.
- `docs/README.md` indexes internal documentation. Put procedures and guided
  explanations in `docs/guides/`; put maintained contracts, architecture,
  interfaces, and test or benchmark specifications in `docs/reference/`.
  Review a guide's steps and a reference's claims against current source when
  related behavior changes. State the code version on a guide that deliberately
  explains a specific revision. Put dated work evidence in `docs/records/`,
  and finished historical plans and reports in `docs/archive/`. Index archived
  material in `docs/archive/README.md`; preserve its original claims as dated
  evidence rather than presenting them as current behavior. Keep only the
  index, living status, and changelog at the root of `docs/`.
- `docs/STATUS.md` is the one living project-wide status. Keep a concise view
  of what exists, what is in progress and what remains open. Rewrite it when
  those facts change, and link every open topic record under "In progress".
  Move history to the changelog or a topic record instead of accumulating it
  here. Mark unverified claims and validation gaps explicitly.
- `docs/CHANGELOG.md` records each meaningful completed code, site or
  documentation change, newest first, in the same change. State what changed
  and why, the checks actually run, their limits, and important decisions or
  deliberate omissions. Do not add the entry's own commit hash; Git history
  supplies it. Do not reconstruct old changes from memory.
- `docs/records/YYYY-MM-DD-topic.md` holds one substantive topic: a design,
  implementation, audit, validation run or investigation. Start or reuse its
  record automatically when work begins; a multi-day topic stays in one file.
  Include the start date, `open` or `closed` status, and code state. Keep a short
  current `Summary` at the top. Add dated sections for evidence, decisions,
  changes and verification; earlier dated sections are append-only. Correct an
  earlier assessment in a new section that says what it replaces. Close the
  record when the topic is finished and freeze it; follow-up work gets a new
  linked record. Copy key results into the tracked file instead of relying on
  generated output in `build/`. See [the record template](docs/records/README.md).
- For a substantive change, update its record as work proceeds, then update
  `STATUS.md` and `CHANGELOG.md` before finishing. For an audit or unfinished
  investigation, update the record and status; add a changelog entry when the
  repository itself changes meaningfully. A spelling or formatting correction
  only needs its affected page. Record failed or unrun checks accurately.
- After site changes, run the strict MkDocs build or `tools/build-pages.sh` as
  appropriate. The latter packages the WebAssembly simulator and generated
  example rows; use `tools/run-web-tests.py` for browser or WASM changes. Do
  not claim the published site was checked from a Markdown-only inspection.

## Adding New Components

Before creating or changing a component, read and follow
[the adding a component guide](docs/guides/adding-a-component.md). It covers
the current structural, behavioral, scenario-test, delay-table and public
documentation path. The older walkthrough is in
`docs/archive/complete-circuit-workflow.md`.

## File Organization

```
include/
  basic/          Pin, Wire, LogicValue
  components/     Component hierarchy, builders, selection/ (families, catalog, profiles)
  modules/
    basic/        Gates, muxes, decoders, latches, flip-flop, clock
    composite/    Adders, ALU8 units, ALU32 units
    memory/       Storage cells, devices, controllers, endpoints
    storage/      Register<W> and ClockedStateBit: state with no address
    rv32i/        Building blocks, stages, cores
    utility/      Rewire, bit adapters, constants
  rv32i/          Instruction decoder, oracle, ELF loader (reference model)
  analysis/       Timing analysis, workloads, experiments
  simulator/      Event, Simulator, SimulationTest
  tests/          Scenario and test class headers
src/              Mirrors include/, plus bindings/ (pybind11), wasm/, visualizer/
tests/            cpp/ (C++ test programs), checks/ (Python and JavaScript run by CTest),
                  web/ (browser and WebAssembly checks listed in web-tests.json), fixtures/
tools/            Site build, benchmarks, validation and layout scripts
visualizer-v2/    Browser visualizer
site-docs/        Public documentation source
docs/             Internal index, status and changelog; guides/, reference/,
                  records/ and archive/
```

## Memory Management

- `std::shared_ptr` for ownership; `std::weak_ptr` for back-references (pin to component, wire to component, component to parent).
- Never store raw pointers to components, pins or wires.

## Known Limitations

- No clock-domain modelling; everything runs in one global time.
- Structural (gate-level) cores simulate at tens of cycles per second; behavioral cores at thousands. Large workloads need the behavioral forms.
- Detailed memory devices are limited to 4,096 cells; larger memories use `MemoryContainer`.
- No VCD or waveform export.

The former three-year gate-to-GPU plan is preserved as
[historical context](docs/archive/gate-level-to-gpu-three-year-roadmap.md). It
is not an inventory of implemented features or a replacement for the
lower-level-first rule above.
