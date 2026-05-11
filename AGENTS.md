# AGENTS.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

CircuitSim is a digital logic circuit simulator with event-driven simulation engine (C++20) and interactive visualization (Python/Pygame). It simulates circuits at the gate level with accurate timing and supports hierarchical component composition.

**Core Architecture**: Event-driven simulation with priority queue processing WireUpdateEvents and ComponentEvalEvents. Components are composed hierarchically (composite pattern) where complex circuits are built from simpler gates.

## Architectural Rule: Lower-Level First

This project prioritizes learning hardware by building lower-level structures before hiding them behind behavioral code.

- Implement gates, slices, and composite components first whenever feasible.
- Add behavioral modules only when the equivalent lower-level implementation would be too large, too slow, or too visually noisy to use everywhere.
- A behavioral module must be treated as an abstraction of an existing lower-level component or representative lower-level slice, not as the original source of truth.
- Before adding a product-facing behavioral module, add tests for the lower-level implementation and equivalence tests proving the behavioral module matches the same external contract.
- If a full lower-level implementation is impractical, build a representative slice first, such as a 1-bit ALU cell, 4-bit ALU slice, one register cell, one register word, or tiny memory array.
- Temporary functional or behavioral code may be used as a reference oracle or bring-up tool, but it must be clearly named and must not replace the lower-level design path.

## Build System

### Initial Build
```bash
# From project root
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

### Rebuild After Changes
```bash
# C++ changes only
cd build && make -j$(nproc)

# CMakeLists.txt changes
cd build && cmake .. && make -j$(nproc)
```

### Build Outputs
- `build/circuit_backend.cpython-*.so` - Python module (pybind11)
  - **Automatically copied to `visualizer/` after build** (via CMake POST_BUILD command)
- `build/sim` - C++ standalone executable
- `build/tests/FullCircuitTest` - CTest executable
- `build/tests/FullAdderTest` - CTest executable

### Running Tests
```bash
# All C++ tests via CTest
cd build && ctest --verbose

# Single test directly
cd build && ./tests/FullAdderTest
```

### Running Visualizer
```bash
# From project root (Python module is auto-deployed after build)
cd visualizer
python main.py
```

**Important**: The `circuit_backend` Python module is automatically copied from `build/` to `visualizer/` after every successful build. No manual copying needed! The visualizer loads `circuit_backend` module and runs `FullAdderTest` by default (see main.py:106).

## Code Architecture

### Component Hierarchy (Core Abstraction)

```
Component (base)
  └─ IOComponent (adds pins)
      ├─ BasicComponent (adds evaluate() and delay)
      │   ├─ Logic Gates: NOTGate, ANDGate, ORGate, XORGate, etc.
      │   ├─ DFlipFlop (sequential)
      │   └─ ClockGenerator
      └─ Composite Components (use buildInternals())
          ├─ HalfAdder (XOR + AND)
          └─ FullAdder (2 HalfAdders + OR)
```

**Key Distinction**:
- `BasicComponent`: Implements `evaluate(time, simulator)` - gate-level logic executed during simulation
- Composite (plain `IOComponent`): Implements `buildInternals(builder)` - constructs internal circuit from other components

### Component Creation Pattern

**CRITICAL**: Components must be created via `Component::create<T>(args...)`, NOT direct `std::make_shared<T>()`.

```cpp
// CORRECT - handles pin initialization and buildInternals
auto adder = Component::create<FullAdder>("FA1");

// WRONG - pins won't exist, buildInternals won't run
auto adder = std::make_shared<FullAdder>("FA1");
```

The `create<T>()` template (Component.tpp:8-27):
1. Constructs the component
2. Calls `initPins()` if IOComponent (creates external interface pins)
3. Calls `buildInternals()` to construct internal hierarchy
4. Returns fully-initialized component

### Wire Connectivity Model

Each Pin has TWO wire connections:
- **external_wire**: Connects to parent component's scope (upward in hierarchy)
- **internal_wire**: Connects to child components (downward in hierarchy)

Example in HalfAdder:
```cpp
// External pin "A" has:
//   - external_wire: from parent's circuit to HalfAdder boundary
//   - internal_wire: from HalfAdder boundary to internal XOR1.A and AND1.A

builder.addNewWire("A_internal",
    builder.getInputPin("A"),          // Source: HalfAdder's external pin
    { builder.getInputPin<XORGate>("XOR1", "A"),
      builder.getInputPin<ANDGate>("AND1", "A") }  // Sinks: internal gates
);
```

### Simulation Event Flow

1. **Wire change** → `WireUpdateEvent` scheduled (Priority: WIRE_UPDATE = 0, highest)
2. **Wire processes** → Propagates new value to all sink pins
3. **Sink pin owners** (BasicComponents) → `ComponentEvalEvent` scheduled (Priority: COMPONENT_EVAL = 1)
4. **Component evaluates** → Logic executed, output wires updated
5. **Cycle repeats** with propagation delays

Events are processed in strict priority order: WireUpdateEvents always before ComponentEvalEvents at same time.

### ComponentBuilder Usage

The builder maintains scoped context for circuit construction:

```cpp
void MyComposite::buildInternals(ComponentBuilder& builder) {
    // Add internal components (automatically become children)
    builder.addNewComponent<XORGate>("xor1");
    builder.addNewComponent<ANDGate>("and1");

    // Connect wires (source pin → multiple sink pins)
    builder.addNewWire("wire_name",
        builder.getOutputPin<XORGate>("xor1", "OUT"),  // Source
        { builder.getInputPin<ANDGate>("and1", "A") }  // Sinks vector
    );

    // Access parent component's pins (no template arg)
    builder.getInputPin("A");   // This component's input pin A
    builder.getOutputPin("Sum"); // This component's output pin Sum
}
```

### Pin Initialization Lambda Pattern

Components define their external interface via lambda passed to IOComponent constructor:

```cpp
ANDGate::ANDGate(std::string name)
    : BasicComponent(std::move(name), 1,  // delay = 1
        [](IOComponent* self) {
            self->addPin("A", PinType::INPUT);
            self->addPin("B", PinType::INPUT);
            self->addPin("OUT", PinType::OUTPUT);
        })
{}
```

This lambda is stored and called later during `initPins()` phase.

## Adding New Components

**IMPORTANT**: For complete step-by-step workflow, see `docs/complete-circuit-workflow.md` - this is the SOURCE OF TRUTH for building circuits.

### Quick Reference

#### Adding a Basic Gate
1. Declare in `include/modules/basic/Gate.hpp` (or new header)
2. Implement constructor (pin initialization lambda) and `evaluate()` in corresponding `.cpp`
3. Add to CMakeLists.txt `CORE_LOGIC_SOURCES` if new file
4. Export to Python in `src/bindings/ModulesBinding.cpp`

#### Adding a Composite Component
1. Declare in `include/modules/composite/` with `buildInternals()` override
2. Implement constructor (external pins) and `buildInternals()` (internal circuit)
3. Use `ComponentBuilder::addNewComponent<T>()` and `addNewWire()` in buildInternals
4. Add to CMakeLists.txt and bindings
5. Create test executable in `tests/` directory

### Template Include Pattern

**IMPORTANT**: ComponentBuilder uses templates, causing circular dependencies.

- `ComponentBuilder.hpp`: Only declarations
- `ComponentBuilder.tpp`: Template implementations
- Include `.tpp` where templates are instantiated (e.g., HalfAdder.cpp:3), NOT in headers

## Python Bindings (pybind11)

Bindings split across multiple files in `src/bindings/`:
- `export.cpp`: Module definition (`PYBIND11_MODULE(circuit_backend, m)`)
- Each `*Binding.cpp`: Exposes specific classes (Component, Wire, Pin, etc.)
- All binding functions called from `export.cpp`

**Pattern**: Use `.def_property_readonly()` for getters, expose minimal interface needed by visualizer.

## Visualizer Architecture

- `main.py`: App class, event loop, LayoutManager (saves component positions to layout.json)
- `visual_component.py`: VisualComponent/VisualIOComponent (mirrors C++ hierarchy), VisualWire, VisualPin
- `camera.py`: Camera class (pan/zoom, world↔screen coordinate transforms)
- `ui_elements.py`: Button, Slider widgets

**Data Flow**:
1. Initialize C++ test (e.g., FullAdderTest)
2. Build visual hierarchy mirroring C++ components
3. Run simulation to completion (stores history)
4. Visualizer scrubs time via `simulator.set_circuit_state_at_time(t)`
5. Visual components query C++ pin/wire values for rendering

**Layout System**: Hierarchical JSON (layout.json) with type-based defaults and per-instance overrides. Components store positions relative to parent.

## Simulation History & Time Travel

`Simulator` records all wire value changes in `_log: map<Wire*, vector<pair<time, value>>>`.

`setCircuitStateAtTime(t)` replays history using binary search to find value at time t for each wire. This enables the time-scrubbing UI.

## Memory Management

- Extensive use of `std::shared_ptr` for ownership
- `std::weak_ptr` for back-references (Pin→Component, Wire→Component, Component→parent)
- All major classes inherit `std::enable_shared_from_this<>` to safely get shared_ptr to `this`

**Rule**: Never store raw pointers to Components/Pins/Wires. Always use shared_ptr or weak_ptr.

## File Organization

```
include/
  basic/          - Wire, Pin, LogicValue enum
  components/     - Component hierarchy (*.hpp + *.tpp for templates)
  modules/
    basic/        - Gates, DFlipFlop, ClockGenerator
    composite/    - HalfAdder, FullAdder
  simulator/      - Event, Simulator, SimulationTest base
  tests/          - Test class headers (HalfAdderTest, etc.)

src/
  (mirrors include/ structure for .cpp implementations)
  bindings/       - pybind11 Python exports
  main.cpp        - C++ standalone entry point

visualizer/       - Python/Pygame GUI
  main.py         - Application entry point
  layout.json     - Saved component positions (git-ignored typically)

tests/            - CTest C++ test executables
```

## Common Patterns

### Creating a Test Circuit
```cpp
class MyTest : public SimulationTest {
    std::string getTestName() const override { return "MyTest"; }

    void buildCircuit() override {
        // builder is already initialized in setupCircuit()
        auto gate1 = builder->addNewComponent<ANDGate>("gate1");
        auto wire1 = builder->addNewWire("w1",
            /* source */ nullptr,
            /* sinks */ {});
        wire1->setValue(LogicValue::HIGH);
        // ...
    }

    void setInitialState() override {
        // Set initial wire values, schedule first events
        auto wire = builder->getWire("w1");
        wire->propagateChange(*sim, 0);
    }

    void verifyResults() override {
        // Assert expected final state
        auto out = builder->getOutputPin("result");
        assert(out->getValue() == LogicValue::HIGH);
    }

    size_t getRunDuration() const override { return 100; }
};
```

### Debugging Simulation
- Enable verbose logging in Simulator.cpp (already has detailed logs)
- Use `root->format(0, true, true)` to print circuit state (wires + pins)
- Check event queue processing order (WireUpdate before ComponentEval)
- Verify pin connections: each pin should have correct internal/external wires

## Known Limitations (Current State)

- Single-bit wires only (no buses)
- No clock domain modeling (everything in global time)
- No X-propagation for uninitialized wires (defaults to UNKNOWN)
- Visualization breaks with >1000 components
- No VCD/waveform export (would be valuable addition)

---

## Strategic Roadmap: Gate-Level → CPU → GPU

**Project Evolution**: Gate-level learning tool → Hybrid CPU simulator → Performance-optimized multi-core/GPU

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

**Current limitation**: Only single-bit wires exist. Cannot build CPU without multi-bit data paths.

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

1. **Multi-bit buses first** - Single-bit wires cannot scale to CPU
2. **Lower-level first** - Gates, slices, and composites come before behavioral shortcuts
3. **Behavioral must be proven** - Necessary for scale, but only as tested abstraction of lower-level behavior
4. **Gate-level where educational** - ALU slices, registers, control logic
5. **Start simple** - Single-cycle before pipeline, pipeline before OOO
6. **Validate constantly** - Test every phase before moving forward
7. **Timeline is realistic** - 3 years from gates to GPU is achievable

---

**This roadmap is the strategic direction. Build gate-level understanding first (Option B), then scale with performance models (Option C). The journey from basic gates to working GPU is a 3-year commitment - plan accordingly.**
