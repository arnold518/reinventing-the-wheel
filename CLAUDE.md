# CLAUDE.md

This file provides guidance to coding agents working in this repository.

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
- One `BuildProfile` (ordered rules, last match wins) chooses structural or behavioral per path; a `BuildManifest` records what was built. See `docs/unified-component-profiles.md`.
- Behavioral classes are usually named `...Direct`; their headers are private, under `src/modules`.
- A behavioral rule has one owner. Larger behavioral implementations call the smaller component's function (for example `alu32Result`) instead of restating it. The instruction oracle in `src/rv32i` is deliberately independent.
- Contract IDs are stored in saved profiles and layouts. Renaming or removing one needs a compatibility plan; see the retired list in `src/modules/rv32i/RV32IBuildProfiles.cpp`.

### Tests and scenarios

- `src/tests/TestRegistry.cpp` is the single registry used by CTest (`SimulationTestRunner`), the Python binding (`create_test_by_name`) and the WebAssembly build.
- Component tests run every available fidelity and compare them. Program tests compare cores against the instruction oracle.
- See `docs/test-organization.md` and `docs/benchmark-system.md`.

### Visualizer

- `visualizer-v2/`: `server.py` (native backend and layout computation) and `static/` (canvas app). The same UI runs without a server through `src/wasm/WasmBindings.cpp` and `VisualizerSession`.
- `visualizer-v2/static/scenarios.json` defines menu titles and groups; `layout.json` holds saved layouts keyed by type, by profile fingerprint and by scenario root.
- `visualizer/` is the older Pygame viewer.

### Documentation

- `site-docs/` is the public MkDocs source (`mkdocs.yml`). Example tables are generated from recorded scenarios, not typed by hand.
- `docs/` holds internal reports and plans; many are historical.

## Adding New Components

Follow `docs/adding-a-component.md`. (`docs/complete-circuit-workflow.md` is a historical walkthrough of an older API.)

## File Organization

```
include/
  basic/          Pin, Wire, LogicValue
  components/     Component hierarchy, builders, selection/ (families, catalog, profiles)
  modules/
    basic/        Gates, muxes, decoders, latches, flip-flop, clock
    composite/    Adders, ALU8 units, ALU32 units
    memory/       Storage cells, devices, controllers, endpoints, registers
    rv32i/        Building blocks, stages, cores
    utility/      Rewire, bit adapters, constants
  rv32i/          Instruction decoder, oracle, ELF loader (reference model)
  analysis/       Timing analysis, workloads, experiments
  simulator/      Event, Simulator, SimulationTest
  tests/          Scenario and test class headers
src/              Mirrors include/, plus bindings/ (pybind11), wasm/, visualizer/
tests/            Standalone C++, Python and JavaScript checks; CMake test lists
tools/            Site build, benchmarks, validation and layout scripts
visualizer-v2/    Browser visualizer
site-docs/        Public documentation source
docs/             Internal reports and plans
```

## Memory Management

- `std::shared_ptr` for ownership; `std::weak_ptr` for back-references (pin to component, wire to component, component to parent).
- Never store raw pointers to components, pins or wires.

## Known Limitations

- No clock-domain modelling; everything runs in one global time.
- Structural (gate-level) cores simulate at tens of cycles per second; behavioral cores at thousands. Large workloads need the behavioral forms.
- Detailed memory devices are limited to 4,096 cells; larger memories use `MemoryContainer`.
- No VCD or waveform export.

---

## Strategic Roadmap: Gate-Level → CPU → GPU

**Project Evolution**: Gate-level learning tool → Hybrid CPU simulator → Performance-optimized multi-core/GPU

This roadmap is historical planning context, not a current inventory. Consult the code, component catalog, current site documentation, and measured benchmark artifacts for implemented features. Do not remove structural implementations or the visualizer as routine cleanup.

### Three-Year Vision

**Year 1**: Option B - Educational 32-bit CPU with gate-level visualization
**Year 2**: Option B→C Transition - Performance-optimized pipelined CPU
**Year 3**: Option C - Multi-core CPU + Full GPU simulator

### The Two-Model Strategy

**Option B (Hybrid)**: Mix gate-level + behavioral components
- Goal: Educational CPU simulator with visualization
- Keep: Gates for ALU, registers, control logic (visible in UI)
- Behavioral: Multipliers, caches, memory only after lower-level contracts or representative slices are tested
- Timeline: Months 1-18
- Target: 32-bit RISC-V single-cycle → pipelined CPU

**Option C (Performance)**: Pure functional/behavioral model
- Goal: Research-grade performance analysis
- Remove: Event-driven simulation, gate-level components, Pygame visualizer
- Add: Cycle-accurate microarchitecture models, statistical analysis
- Timeline: Months 19-36
- Target: Out-of-order CPU → multi-core → GPU

---

## OPTION B: Hybrid CPU Simulator (Months 1-18)

**Philosophy**: "Build lower-level first, then use behavioral models only as tested abstractions for scale"

### Component Architecture Strategy

| Component | Implementation | Reason |
|-----------|---------------|--------|
| **4-8 bit ALU slices** | Gate-level | See carry propagation, educational |
| **Register file** | Gate-level D-FlipFlops | Visualize state storage |
| **Instruction decoder** | Gate-level ROM + logic | Understand control flow |
| **Program counter** | Gate-level register + adder | Visualize sequencing |
| **32-bit Multiplier** | Behavioral abstraction after lower-level slice tests | Too many gates (~1024) |
| **Caches (L1/L2)** | Behavioral abstraction + stats overlay after contract tests | Complex state machine |
| **Memory** | Behavioral abstraction after tiny memory/slice tests | Impractical at full gate-level |

**Rule of Thumb**:
- Gate-level: <200 gates, educational value, fits in visualizer
- Behavioral: >1000 gates, performance critical, complex algorithms, and backed by lower-level slice/equivalence tests

### Critical Infrastructure: Multi-Bit Buses (Required First!)

**Historical prerequisite (implemented)**: multi-bit pins and wires already support the current RV32I cores. The sketch below records the original roadmap, not a new implementation requirement.

```cpp
// Phase 1 requirement: Bus<WIDTH> template
template<size_t WIDTH>
class Bus {
    std::array<std::shared_ptr<Wire>, WIDTH> lanes;

    void setValue(uint64_t value) {
        for (size_t i = 0; i < WIDTH; i++) {
            lanes[i]->setValue((value >> i) & 1 ? HIGH : LOW);
        }
    }

    uint64_t getValue() const {
        uint64_t result = 0;
        for (size_t i = 0; i < WIDTH; i++) {
            if (lanes[i]->getValue() == HIGH) result |= (1ULL << i);
        }
        return result;
    }
};

// Usage: 32-bit register file
class RegisterFile : public IOComponent {
    std::array<Bus<32>, 32> registers;  // 32 × 32-bit registers
};
```

### Behavioral Component Pattern

Behavioral components are allowed only after the lower-level behavior is already captured by gates, slices, composite modules, or representative-slice tests. They keep the same external contract while compressing implementation detail for scale:

```cpp
class Multiplier32 : public BasicComponent {
    std::queue<Operation> pipeline;
    static constexpr size_t LATENCY = 3;

    void evaluate(size_t time, Simulator& sim) override {
        // Direct computation, validated against lower-level slice tests
        if (start_signal) {
            uint32_t a = getInputBus<32>("A");
            uint32_t b = getInputBus<32>("B");
            pipeline.push({a, b, time + LATENCY});
        }

        // Pipeline completion (still respects timing)
        if (!pipeline.empty() && pipeline.front().ready_time == time) {
            uint64_t result = (uint64_t)pipeline.front().a * pipeline.front().b;
            setOutputBus<64>("RESULT", result);
            pipeline.pop();
        }
    }
};
```

### Visualization Strategy

Visualizer adapts based on zoom level:
- **Zoomed in** (gate-level): Show all gates, wires, carry chains
- **Zoomed in** (behavioral): Show black box + stats ("MUL | 3cyc | 98% util")
- **Zoomed out**: Block diagram only

---

### Option B: 18-Month Development Timeline

| Phase | Timeline | Deliverables | Key Components |
|-------|----------|--------------|----------------|
| **Phase 1: Multi-Bit Infrastructure** | Months 1-3 | Working multi-bit ALU | • `Bus<WIDTH>` template<br>• 4-bit ALU (gate-level)<br>• 32-bit ALU (behavioral)<br>• Register file (8-32 regs) |
| **Phase 2: Single-Cycle CPU** | Months 4-6 | First assembly program runs | • RISC-V RV32I decoder<br>• Program counter<br>• Instruction memory<br>• Data memory<br>• Assembler integration |
| **Phase 3: Pipeline** | Months 7-9 | 5-stage pipelined CPU | • IF/ID/EX/MEM/WB stages<br>• Pipeline registers<br>• Hazard detection<br>• Forwarding unit |
| **Phase 4: Memory Hierarchy** | Months 10-12 | Cache simulation | • L1 I-cache (16KB)<br>• L1 D-cache (16KB)<br>• Cache stats overlay<br>• Memory controller |
| **Phase 5: Optimization** | Months 13-15 | 100x speedup achieved | • Incremental evaluation<br>• Event batching<br>• Profiling tools<br>• Performance tuning |
| **Phase 6: Advanced Features** | Months 16-18 | Production-ready CPU sim | • Branch prediction<br>• Exceptions/interrupts<br>• 100+ test programs<br>• Documentation |

**Success Criteria (End of Month 18)**:
- ✓ 32-bit RISC-V CPU executes real assembly
- ✓ Visualizer shows gate-level ALU internals
- ✓ 100x faster than pure gate-level
- ✓ Can run bubble sort, fibonacci, matrix multiply

---

## OPTION C: Performance Model (Months 19-36)

**Philosophy**: "Cycle-accurate functional models - no gates, maximum speed"

### The Paradigm Shift

**What Changes**:
- Remove: Event-driven simulation, Wire/Pin classes, Pygame visualizer
- Add: Direct pipeline stepping, functional models, statistical analysis
- Speed: 1000x faster than Option B (can run SPEC benchmarks)
- Tools: matplotlib/pandas for performance analysis, not visualization

### Architecture: Pure Functional Model

```cpp
class PerformanceCPU {
    // Architectural state
    uint32_t PC;
    std::array<uint64_t, 32> registers;
    std::vector<uint8_t> memory;

    // Microarchitecture (not gate-level)
    std::deque<Instruction> fetch_buffer;
    ReorderBuffer rob;
    std::array<ReservationStation, 6> execution_units;
    BranchPredictor predictor;
    CacheHierarchy cache;

    void cycle() {
        // Process all stages in single function
        commit_stage();     // Retire instructions
        execute_stage();    // Functional execution
        issue_stage();      // Issue to execution units
        decode_stage();     // Decode instructions
        fetch_stage();      // Fetch from I-cache
    }
};
```

No events, no wires - just direct microarchitecture simulation.

### Option C: 18-Month Development Timeline

| Phase | Timeline | Deliverables | Key Components |
|-------|----------|--------------|----------------|
| **Phase 1: Transition** | Months 19-21 | Functional CPU validated | • New codebase (no events)<br>• Direct pipeline stepping<br>• Port RISC-V ISA<br>• Validate vs Option B |
| **Phase 2: Out-of-Order** | Months 22-24 | Superscalar CPU | • Reorder buffer (ROB)<br>• Reservation stations<br>• Register renaming<br>• Tomasulo's algorithm |
| **Phase 3: Branch Prediction** | Months 25-27 | Tournament predictor | • Local history table<br>• Global history table<br>• Selector (tournament)<br>• BTB + RAS |
| **Phase 4: Multi-Core** | Months 28-30 | 4-core SMP system | • MESI coherence protocol<br>• Interconnect (crossbar)<br>• Shared L2/L3<br>• Memory controller |
| **Phase 5: GPU Simulator** | Months 31-33 | SIMT execution engine | • Streaming multiprocessors<br>• Warp scheduler<br>• Texture/compute units<br>• Memory hierarchy |
| **Phase 6: Validation** | Months 34-36 | Research-grade tool | • Hardware validation<br>• Calibrated timing<br>• 10+ MIPS speed<br>• Publication ready |

**Success Criteria (End of Month 36)**:
- ✓ Out-of-order multi-core CPU runs SPEC benchmarks
- ✓ GPU executes compute shaders (matrix multiply, convolution)
- ✓ Performance within 10% of real hardware timing
- ✓ 1000x faster than Option B

---

### Migration Strategy: Maintain Both Models

**Recommended directory structure** (Month 19+):

```
reinventing-the-wheel/
├── gate-simulator/          # Option B - Keep for education
│   ├── include/, src/       # Current codebase
│   ├── visualizer/          # Pygame visualization
│   └── README.md            # "Learn gate-level CPU design"
│
└── perf-simulator/          # Option C - Performance focus
    ├── include/
    │   ├── pipeline/        # Direct pipeline stages
    │   ├── microarch/       # ROB, RS, predictor
    │   └── analysis/        # Performance counters
    ├── tools/               # Python analysis (matplotlib)
    └── README.md            # "Cycle-accurate microarch sim"
```

**Use case separation**:
- Teaching/Learning → gate-simulator (visualize hardware)
- Performance research → perf-simulator (speed + analysis)
- Correctness validation → cross-check both

---

## Critical Success Factors

### Technical Milestones

| Metric | Pure Gate | Option B (Hybrid) | Option C (Performance) |
|--------|-----------|-------------------|------------------------|
| **Speed** | 1x baseline | 100x faster | 1000x faster |
| **Max complexity** | 8-bit CPU | 32-bit pipelined | Multi-core + GPU |
| **Visualization** | Full gate-level | Mixed (gates + stats) | Graphs/analysis only |
| **Use case** | Learning basics | Teaching CPU arch | Research/optimization |

### Year-End Checkpoints

**✓ Year 1** (Option B Complete):
- 32-bit RISC-V CPU executes real assembly
- Visualizer shows carry propagation in ALU
- 100x speedup achieved
- Programs: sorting, fibonacci, matrix operations

**✓ Year 2** (Option C Transition):
- Pipelined CPU with hazard handling
- Cache hierarchy with realistic timing
- Functional performance model validated
- SPEC CPU subset runs

**✓ Year 3** (Advanced Features):
- 4-core multi-processor with MESI
- GPU executes compute shaders
- Performance within 10% of real hardware
- Publication-ready research tool

### Key Architectural Principles

1. **Multi-bit buses first** - Already implemented; preserve the current multi-bit contracts
2. **Lower-level first** - Gates, slices, and composites come before behavioral shortcuts
3. **Behavioral must be proven** - Necessary for scale, but only as tested abstraction of lower-level behavior
4. **Gate-level where educational** - ALU slices, registers, control logic
5. **Start simple** - Single-cycle before pipeline, pipeline before OOO
6. **Validate constantly** - Test every phase before moving forward
7. **Timeline is realistic** - 3 years from gates to GPU is achievable

---

**This roadmap is the strategic direction. Build gate-level understanding first (Option B), then scale with performance models (Option C). The journey from basic gates to working GPU is a 3-year commitment - plan accordingly.**
