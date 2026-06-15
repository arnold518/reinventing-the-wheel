# RV32I CPU Roadmap

Last updated: 2026-06-02

This roadmap replans the RV32I work after completing the two major foundations:

- Structural `ALU32`.
- Register and memory components, including `BehavioralRegisterFile32x32` and `BehavioralMemory64Kx32`.

The next goal is no longer "build ALU and memory". The next goal is to turn these components into a first runnable RV32I system.

## Target

The first CPU target is a correct, testable, educational RV32I machine:

- RV32I base integer ISA only.
- 32-bit `pc`.
- 32 architectural registers, `x0..x31`.
- `x0` hardwired to zero.
- Little-endian byte-addressed memory.
- Raw binary program loading first.
- Bare-metal assembly programs.
- Single-cycle CPU first.
- Halt through `EBREAK` or a documented project stop condition.
- Visualizer support after the CPU behavior is stable.

## Non-Goals For The First CPU

Do not implement these until first-pass RV32I works:

- RV64.
- `M` multiply/divide extension.
- `C` compressed instructions.
- Atomics.
- Floating point.
- Vector instructions.
- Privileged mode.
- Virtual memory.
- Operating system boot.
- Interrupts and timers.
- ELF loading as the first executable format.
- Caches, pipelines, hazards, and branch prediction.

## Current Baseline

The `rv32i` branch now has enough component foundation to start CPU integration.

Completed:

- Event-driven simulator and hierarchical component system.
- Native multi-bit `Pin<WIDTH>` and `Wire<WIDTH>` support.
- Basic gates.
- Multi-bit adapters: `Rewire`, `BitSplitter`, `BitJoiner`, and constants.
- Multi-bit muxes, including 32-bit mux variants.
- 8-bit arithmetic and `ALU8` prototypes.
- Structural 32-bit datapath components:
  - `Adder32`
  - `AddSub32`
  - `Logic32`
  - `ZeroDetect32`
  - `Comparator32`
  - `Shifter32`
  - `ALU32`
- Structural storage path:
  - `SRLatch`
  - `GatedDLatch`
  - structural `DFlipFlop`
  - `MemoryBit`
  - `Register32`
- Register-file and memory components:
  - `RegisterFile4x32`
  - `RegisterFile32x32`
  - `BehavioralRegisterFile32x32`
  - `Memory4x32`
  - `Memory32x32`
  - `BehavioralMemory64Kx32`
- RV32I decode/control libraries and tests.
- Program preload and memory readback helpers for RV32I test fixtures.
- Functional RV32I instruction oracle and per-instruction trace.
- Tests and visualizer scenarios for the ALU and memory foundations.

Detailed status reports:

- `docs/rv32i-components-report.md`
- `docs/rv32i-milestone-1-report.md`
- `docs/rv32i-milestone-3-report.md`
- `docs/rv32i-milestone-5-report.md`
- `docs/rv32i-milestone-6-report.md`
- `docs/rv32i-milestone-7-plan.md`
- `docs/memory-components-report.md`
- `docs/structural-dff-report.md`

Current verified baseline:

```bash
cmake --build build -j 8
ctest --test-dir build --output-on-failure
```

Most recent full-regression result:

- Full regression passed: 90/90 in 1095.44 seconds.

## Architecture Direction

The first runnable RV32I design should be a single-cycle system built from mixed structural and behavioral components.

Use these components in the first CPU-scale runner:

- `ALU32`: structural execution ALU.
- `BehavioralRegisterFile32x32`: CPU-scale architectural register file.
- Two `BehavioralMemory64Kx32` instances:
  - instruction memory
  - data memory

Why two memory instances:

- A single-cycle CPU fetches an instruction and may perform a data load/store in the same cycle.
- A single single-port memory would require arbitration or a multi-cycle CPU.
- Two instances keep the first runner simple and make instruction/data flow easier to visualize.

The lower-level structural components remain the educational source of truth:

- `MemoryBit`, `Register32`, `RegisterFile32x32`, `Memory4x32`, and `Memory32x32` explain storage behavior.
- `BehavioralRegisterFile32x32` and `BehavioralMemory64Kx32` are scale abstractions for CPU execution.

## CPU/System Boundary

Keep the CPU core separate from the system wrapper.

Recommended split:

- `RV32ISingleCycleCore`
  - owns PC, decode/control, register file, ALU, branch/jump logic, load/store control, and writeback mux.
  - exposes instruction-memory and data-memory pins.
- `RV32ISingleCycleSystem`
  - instantiates the CPU core.
  - instantiates one instruction memory and one data memory.
  - wires clock/reset and memory connections.
  - is the main visualizer/test scenario.

This separation keeps the CPU reusable and lets future tests replace memory with fixtures, probes, or alternative memory models.

## Design Rules

Keep the project's lower-level-first rule:

1. Define the external contract.
2. Build the smallest useful structural or representative slice first when feasible.
3. Test that lower-level contract directly.
4. Use behavioral components only when structural expansion is too large, slow, or visually noisy.
5. Keep behavioral components documented as abstractions of proven lower-level behavior.

For the next RV32I work, this means:

- Decode/control may start as ordinary C++ logic or compact components, but the instruction-field contract must be exhaustively tested.
- PC and next-PC logic should be visible components because they are central to CPU learning.
- Memory preload/readback helpers are test infrastructure, not CPU hardware.
- The first CPU should favor clarity and correctness over cycle realism.

## Replanned Milestones

### Milestone 1: ALU32 Foundation

Status: complete.

Purpose:

Build the structural 32-bit execution unit needed by RV32I arithmetic, logic, comparison, shift, and address calculation.

Implemented components:

- `Adder32`
- `AddSub32`
- `Logic32`
- `ZeroDetect32`
- `Comparator32`
- `Shifter32`
- `ALU32`

Verification:

- Per-component 32-bit tests.
- `ALU32Test`.
- `RV32IALU32Test`.

Report:

- `docs/rv32i-milestone-1-report.md`

### Milestone 2: Register And Memory Foundation

Status: complete.

Purpose:

Build the storage components needed before CPU integration.

Implemented components:

- `SRLatch`
- `GatedDLatch`
- structural `DFlipFlop`
- `MemoryBit`
- `BehavioralMemoryBit`
- `Register32`
- `BehavioralRegister32`
- `Decoder2to4`
- `Decoder5to32`
- `RegisterFile4x32`
- `RegisterFile32x32`
- `BehavioralRegisterFile32x32`
- `Memory4x32`
- `Memory32x32`
- `BehavioralMemory64Kx32`

CPU-scale decision:

- Use `BehavioralRegisterFile32x32` for architectural registers.
- Use two `BehavioralMemory64Kx32` instances for instruction and data memory.

Verification:

- Full memory/register stack tests.
- Behavioral register-file unknown-state tests.
- Byte/halfword/word memory load-store tests.
- Fault and alignment tests.

Reports:

- `docs/memory-components-report.md`
- `docs/structural-dff-report.md`

### Milestone 3: RV32I Decode Library

Status: complete.

Purpose:

Decode raw 32-bit instructions into stable project structures before building the CPU datapath.

Recommended files:

- `include/rv32i/RV32IInstruction.hpp`
- `include/rv32i/RV32IDecoder.hpp`
- `src/rv32i/RV32IDecoder.cpp`
- `include/tests/RV32IDecoderTests.hpp`
- `src/tests/RV32IDecoderTests.cpp`

Deliverables:

- Instruction enum for all RV32I base instructions.
- Decoded fields:
  - `opcode`
  - `rd`
  - `rs1`
  - `rs2`
  - `funct3`
  - `funct7`
  - decoded immediate
  - instruction format
- Illegal-instruction result for unsupported encodings.
- Explicit handling for shift-immediate encodings.
- Unit tests for every RV32I instruction group.

Instruction groups:

- R-type ALU.
- I-type ALU.
- Loads.
- Stores.
- Branches.
- `JAL`.
- `JALR`.
- `LUI`.
- `AUIPC`.
- `FENCE`.
- `ECALL`.
- `EBREAK`.

Done when:

- Every RV32I base instruction decodes into a stable enum.
- Illegal encodings are rejected.
- Tests cover representative and edge encodings for every instruction group.

Report:

- `docs/rv32i-milestone-3-report.md`

### Milestone 4: Immediate Generator And Control Contract

Status: complete.

Purpose:

Define the control signals that connect decoded RV32I instructions to existing components.

Recommended files:

- `include/rv32i/RV32IControl.hpp`
- `src/rv32i/RV32IControl.cpp`
- `include/tests/RV32IControlTests.hpp`
- `src/tests/RV32IControlTests.cpp`

Deliverables:

- Immediate generator for:
  - I-type
  - S-type
  - B-type
  - U-type
  - J-type
- Control structure containing:
  - ALU operation select.
  - ALU operand source selects.
  - register write enable.
  - memory read enable.
  - memory write enable.
  - memory access size.
  - load sign-extension control.
  - branch type.
  - jump type.
  - writeback source.
  - trap/halt flags.

Important mapping:

- RV32I instruction fields are not the same as `ALU32.OP`.
- The control unit maps decoded instructions to the project-local `ALU32.OP` values.

Done when:

- Immediate generation matches RV32I bit layouts.
- Every instruction enum maps to expected control signals.
- Illegal instructions produce trap control.

Report:

- `docs/rv32i-milestone-3-report.md`

### Milestone 5: Program Loader And Test Memory Utilities

Status: complete.

Purpose:

Make it easy to run real program bytes without simulating thousands of setup writes.

Implemented files:

- `include/rv32i/RV32IProgram.hpp`
- `src/rv32i/RV32IProgram.cpp`
- `include/tests/RV32IProgramTests.hpp`
- `src/tests/RV32IProgramTests.cpp`

Deliverables:

- Raw binary loader.
- Helper to write program bytes into `BehavioralMemory64Kx32` before simulation.
- Debug/test readback helpers for memory contents.
- Small hand-authored binary fixtures.
- Focused tests for endian behavior, bounds checks, alignment checks, and memory clearing.

Important boundary:

- Preload and readback helpers are simulator/test infrastructure.
- They are not CPU pins and not CPU hardware.

Done when:

- Tests can preload instruction memory.
- Tests can inspect data memory after execution.
- At least one tiny program fixture is loadable.

Report:

- `docs/rv32i-milestone-5-report.md`

### Milestone 6: RV32I Instruction Oracle

Status: complete.

Purpose:

Provide a compact correctness oracle for instruction semantics and program-level expected results.

Implemented files:

- `include/rv32i/RV32IState.hpp`
- `include/rv32i/RV32IFunctionalMemory.hpp`
- `include/rv32i/RV32IInstructionTrace.hpp`
- `include/rv32i/RV32IInstructionOracle.hpp`
- `src/rv32i/RV32IFunctionalMemory.cpp`
- `src/rv32i/RV32IInstructionOracle.cpp`
- `include/tests/RV32IInstructionOracleTests.hpp`
- `src/tests/RV32IInstructionOracleTests.cpp`

Deliverables:

- Architectural state:
  - `pc`
  - `x[32]`
  - byte-addressed memory view
  - halt/trap status
- One-instruction oracle for all RV32I base instructions.
- Program runner until halt, trap, or instruction limit.
- Per-instruction golden trace rows.
- Reference tests for instruction, memory, halt, trap, and trace semantics.

Role:

- This instruction oracle is a test oracle and bring-up tool.
- It is not the final product CPU component.
- It is not a visualizer scenario because it has no circuit topology.
- The future simulation-vs-oracle abstract test harness is planned, but intentionally not implemented yet.

Done when:

- Every RV32I base instruction has instruction-oracle coverage.
- Small programs produce expected register and memory results.
- Trace rows contain enough named semantic fields for future component-CPU checkpoint comparison.

Report:

- `docs/rv32i-milestone-6-report.md`

### Milestone 7: Behavioral RV32I System Component

Status: implemented in first form.

Purpose:

Build the first reusable RV32I component that can run raw programs inside the circuit simulator.

Chosen shape:

```text
RV32ISystem
  contains:
    BehavioralRV32ICore
    BehavioralMemory64Kx32 instruction memory
    BehavioralMemory64Kx32 data memory

  exposes:
    CLK
    RST
    ENABLE
    PC
    HALTED
    TRAPPED
```

Recommended files:

- `include/modules/rv32i/BehavioralRV32ICore.hpp`
- `src/modules/rv32i/BehavioralRV32ICore.cpp`
- `include/modules/rv32i/RV32ISystem.hpp`
- `src/modules/rv32i/RV32ISystem.cpp`
- `include/tests/RV32ISystemTests.hpp`
- `src/tests/RV32ISystemTests.cpp`

Expected behavior:

- `BehavioralRV32ICore` is a clocked `BasicComponent`.
- `RV32ISystem` is a reusable composite containing the core and two memories.
- `CLK`, `RST`, and `ENABLE` are external pins supplied by a testbench or visualizer.
- `RST` resets the core, not the preloaded instruction memory.
- instruction memory can be preloaded with `RV32IProgram`.
- data memory can be cleared/read for tests.
- programs run until `EBREAK`, trap, or test instruction limit.
- public system pins stay compact: `CLK`, `RST`, `ENABLE`, `PC`, `HALTED`, and `TRAPPED`.
- rich state inspection uses instrumentation getters, not top-level debug pins.
- core memory-interface pins stay internal to the system but remain stable for a future structural core.

Timing policy:

- use a small visible core FSM.
- fetch, execute, load-wait, store-wait, halted, and trapped states are acceptable.
- do not rely on hidden same-edge external-memory behavior.

Verification strategy:

- compare final system state against `RV32IInstructionOracle`.
- keep full trace-comparison harness deferred until stable per-cycle checkpoints exist.

Done when:

- `RV32ISystem` runs at least one raw RV32I program and should be broadened to multiple programs.
- tests cover arithmetic, load/store, branch, jump, halt, and trap cases.
- system final state matches the Milestone 6 instruction oracle.
- visualizer exposes a reusable `behavioral-rv32i-system-program1` scenario.

Plan:

- `docs/rv32i-milestone-7-plan.md`

### Milestone 8: Trace Comparator And Simulation Checkpoints

Status: planned.

Purpose:

Compare the behavioral system's sampled simulation checkpoints against Milestone 6 instruction oracle trace rows.

Deliverables:

- `RV32IInstructionTraceComparator`
- simulation trace sampler for `RV32ISystem`
- reusable oracle-driven test helper
- checkpoint definitions for stable cycle states

Done when:

- tests can run one program through `RV32IInstructionOracle` and `RV32ISystem`.
- trace rows compare PC, instruction, writeback, memory access, branch decision, halt, and trap fields.
- comparisons are checkpoint-based, not every simulator event timestamp.

### Milestone 9: Visible Control-Flow Components

Status: planned.

Purpose:

Build the visible control-flow path after the behavioral system is stable.

Recommended components:

- `ProgramCounter32`
- `BranchDecision32`
- `NextPC32`

Expected behavior:

- `ProgramCounter32` stores current `pc`.
- sequential path computes `pc + 4`.
- branch path computes `pc + immediate`.
- `JAL` path computes `pc + immediate`.
- `JALR` path computes `(rs1 + immediate) & ~1`.
- branch decision supports `BEQ`, `BNE`, `BLT`, `BGE`, `BLTU`, and `BGEU`.

Done when:

- PC reset behavior is tested.
- branch taken/not-taken cases are tested.
- `JALR` low-bit clearing is tested.
- behavior matches `RV32IInstructionOracle` and `RV32ISystem` checkpoints.

### Milestone 10: Load/Store Unit

Status: planned.

Purpose:

Map RV32I load/store instructions onto `BehavioralMemory64Kx32`.

Recommended component:

- `RV32ILoadStoreUnit`

Responsibilities:

- Generate memory `SIZE`.
- Generate memory `SIGN_EXTEND`.
- Generate `READ_EN` and `WRITE_EN`.
- Route store data.
- Route load data to writeback.
- Surface memory `FAULT`.

Load/store mapping:

| Instruction | Memory size | Sign behavior |
| --- | --- | --- |
| `LB` | byte | sign-extend |
| `LBU` | byte | zero-extend |
| `LH` | halfword | sign-extend |
| `LHU` | halfword | zero-extend |
| `LW` | word | word |
| `SB` | byte | store low byte |
| `SH` | halfword | store low halfword |
| `SW` | word | store word |

Done when:

- All load/store controls are tested.
- Misalignment and out-of-range faults are tested.
- Results match `BehavioralMemory64Kx32Test` and the functional instruction oracle.

### Milestone 11: Single-Cycle CPU Core

Status: planned.

Purpose:

Wire the first CPU core from existing components and control logic.

Recommended files:

- `include/modules/riscv/RV32ISingleCycleCore.hpp`
- `src/modules/riscv/RV32ISingleCycleCore.cpp`
- `tests/RV32ISingleCycleCoreTest.cpp`

Core blocks:

- Program counter.
- Instruction decoder/control.
- Immediate generator.
- `BehavioralRegisterFile32x32`.
- `ALU32`.
- Branch/jump unit.
- Load/store unit.
- Writeback mux.
- Instruction-memory interface.
- Data-memory interface.

Cycle behavior:

1. `PC` selects the instruction.
2. Decoder/control reads instruction fields.
3. Register file outputs `rs1` and `rs2`.
4. Immediate generator produces the immediate.
5. ALU computes result or address.
6. Branch/jump logic selects next PC.
7. Data memory handles load/store.
8. Writeback mux selects register write data.
9. On clock edge, register file and PC update.

Done when:

- The core can execute simple non-branch ALU instruction sequences in C++ tests.
- Register writeback works.
- `x0` remains zero.

### Milestone 12: Single-Cycle RV32I System

Status: planned.

Purpose:

Create the first complete runnable RV32I machine.

Recommended files:

- `include/modules/riscv/RV32ISingleCycleSystem.hpp`
- `src/modules/riscv/RV32ISingleCycleSystem.cpp`
- `tests/RV32ISingleCycleSystemTest.cpp`

System blocks:

- One `RV32ISingleCycleCore`.
- One `BehavioralMemory64Kx32` instruction memory.
- One `BehavioralMemory64Kx32` data memory.
- Clock/reset wiring.
- Program preload/test harness support.

Done when:

- A raw binary program can be loaded.
- The system runs until `EBREAK` or trap.
- At least these programs pass:
  - arithmetic smoke test
  - load/store smoke test
  - branch smoke test
  - loop counter
  - fibonacci or simple sum loop

### Milestone 13: Visualizer Integration

Status: planned.

Purpose:

Expose the first RV32I machine in the visualizer without overwhelming it.

Deliverables:

- Scenario alias such as `rv32i-single-cycle`.
- Layout defaults for CPU, instruction memory, and data memory.
- Checkpoints for fetch/decode/execute/writeback moments.
- Summary labels for PC, instruction, register writes, memory access, and halt/trap state.

Visualizer policy:

- Show the CPU system at block level by default.
- Keep `ALU32` expandable for education.
- Keep instruction/data memory as compact behavioral blocks.

Done when:

- The visualizer can load the RV32I system scenario.
- The first demo program can be scrubbed through simulation history.

### Milestone 14: Assembly Workflow And External Validation

Status: planned.

Purpose:

Make program creation repeatable and validate behavior against known references when available.

Deliverables:

- `programs/rv32i/` assembly sources.
- `tests/fixtures/rv32i/` raw binaries.
- Script to build raw binaries from assembly when a RISC-V toolchain is installed.
- Documentation for rebuilding fixtures.
- Optional comparison against Spike or another RV32I reference.

Done when:

- Fixtures are reproducible.
- Tests do not require the external toolchain unless explicitly rebuilding fixtures.
- At least five assembly programs run to completion.

## Testing Plan

Every milestone must add tests before it is considered complete.

Required test categories:

- Decoder tests.
- Immediate generation tests.
- Control signal tests.
- Functional instruction oracle tests.
- PC and next-PC tests.
- Branch/jump tests.
- Load/store control tests.
- Register-file tests.
- Memory endian tests.
- Load sign-extension tests.
- Store byte/halfword partial-write tests.
- CPU single-instruction tests.
- CPU short-program tests.
- Visualizer import smoke tests after bindings are added.

Important edge cases:

- Writes to `x0`.
- Negative immediates.
- Signed overflow wrapping.
- Signed vs unsigned comparisons.
- Shift amount masking.
- Branch target calculation.
- `JALR` target bit 0 clearing.
- Byte and halfword load sign extension.
- Byte and halfword store lane updates.
- Misaligned loads/stores.
- Out-of-range memory access.
- Illegal instructions.
- `EBREAK` halt.

## Acceptance Criteria For First-Pass RV32I

The project can claim first-pass RV32I support when:

- All RV32I base instructions decode.
- All RV32I base instructions execute.
- `x0` behavior is correct.
- `pc` update behavior is correct.
- Loads and stores are little-endian and byte-addressed.
- Branches and jumps choose the correct next PC.
- `EBREAK` halts execution.
- Illegal instructions trap.
- Raw binary programs can be loaded.
- At least five assembly programs run to completion.
- The functional instruction oracle and component CPU agree on program results.
- C++ tests pass through CTest.
- The visualizer can show at least one RV32I CPU scenario.

## Later Extensions

After first-pass RV32I works, consider extensions in this order:

1. Improve CPU/system docs and visualizer ergonomics.
2. Add ELF loading.
3. Add `M` multiply/divide extension.
4. Add simple multi-cycle memory or unified-memory arbitration.
5. Add a pipelined CPU.
6. Add hazard detection and forwarding.
7. Add branch prediction.
8. Add cache models.
9. Add privileged mode and interrupts.

Do not start these before the single-cycle RV32I system passes program-level tests.

## Risks

### Risk: Decoder Ambiguity

Mitigation:

- Keep decoder tests exhaustive by instruction group.
- Use official RISC-V opcode references as the rule source.
- Document project policy for illegal and unsupported encodings.

### Risk: Behavioral Components Hide Too Much

Mitigation:

- Keep structural ALU and structural storage slices visible.
- Treat behavioral register/memory components as tested scale abstractions.
- Add program-level equivalence checks against the functional instruction oracle.

### Risk: Visualizer Clutter

Mitigation:

- Use block-level CPU visualization first.
- Keep large memory and register file components compact.
- Expand only educational pieces such as `ALU32` by default.

### Risk: Building CPU Before Semantics Are Stable

Mitigation:

- Finish decode/control and functional instruction oracle before full CPU wiring.
- Use short single-instruction tests before whole-program tests.

## Near-Term Plan

Immediate next steps:

1. Broaden numbered behavioral RV32I system program coverage.
2. Add reset, branch, jump, load/store-size, and trap programs.
3. Keep instruction-lockstep checks against `RV32IInstructionOracle`.
4. Add the future trace comparator once stable per-cycle checkpoints exist.
