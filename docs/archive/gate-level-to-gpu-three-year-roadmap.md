# Historical gate-level to GPU roadmap

This is the strategic roadmap formerly embedded in `AGENTS.md`. It records a
planning snapshot, not the current implementation inventory. Check the code,
component catalog, tests and current documentation before using its claims.

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
