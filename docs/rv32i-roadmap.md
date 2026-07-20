# RV32I CPU Roadmap

Last updated: 2026-07-19

This roadmap replans the RV32I work after completing the component foundations and the behavioral answer sheet:

- Structural `ALU32`.
- Register and memory components, including `BehavioralRegisterFile32x32` and `BehavioralMemory64Kx32`.
- Behavioral `RV32ISystem`, its instruction oracle, and instruction-lockstep program suite.

The next goal is to build the structural single-cycle RV32I system and grade it against that answer sheet.

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
- Behavioral `RV32ISystem` answer-sheet implementation with 16 numbered program cases.
- Per-commit instruction lockstep checks for PC, registers, halt/trap state, logical memory access, and byte writes.
- Tests and visualizer scenarios for the ALU and memory foundations.

Detailed status reports:

- `docs/rv32i-components-report.md`
- `docs/rv32i-milestone-1-report.md`
- `docs/rv32i-milestone-3-report.md`
- `docs/rv32i-milestone-5-report.md`
- `docs/rv32i-milestone-6-report.md`
- `docs/rv32i-milestone-7-plan.md`
- `docs/behavioral-rv32i-beginners-guide.md`
- `docs/rv32i-structural-implementation-plan.md`
- `docs/rv32i-control-flow-pair-report.md`
- `docs/rv32i-decode-control-pair-report.md`
- `docs/rv32i-register-file-pair-report.md`
- `docs/rv32i-alu-pair-report.md`
- `docs/rv32i-execution-status-pair-report.md`
- `docs/memory-components-report.md`
- `docs/structural-dff-report.md`

Current verified baseline:

```bash
cmake --build build -j 8
ctest --test-dir build --output-on-failure
```

Most recent full-regression result:

- Full regression passed: 142/142 in 525.19 seconds in the final serial verification run.
- All 16 structural and all 16 behavioral program tests passed their shared lockstep and hard-coded outcome checks.

## Architecture Direction

The first structural RV32I design should be a single-cycle system whose five major core-block contracts each have an educational structural implementation and a compact, clearly named behavioral reference/bring-up counterpart.

Use these components in the structural CPU-scale runner:

- `ALU32`: structural execution ALU.
- `RegisterFile32x32`: structural/hierarchical architectural register file. Its compact pair is `BehavioralRegisterFile32x32`.
- Two `BehavioralMemory64Kx32` instances:
  - instruction memory
  - data memory

Organize the core around five major peer-block contracts:

| Contract | Structural implementation used by the structural core | Behavioral counterpart |
| --- | --- | --- |
| Control flow | `RV32IControlFlowUnit` | `BehavioralRV32IControlFlowUnit` |
| Decode/control | `RV32IDecodeControlUnit` | `BehavioralRV32IDecodeControlUnit` |
| Register file | `RegisterFile32x32` | `BehavioralRegisterFile32x32` |
| ALU | `ALU32` | `BehavioralALU32` |
| Execution control/status | `RV32IExecutionControlStatusUnit` | `BehavioralRV32IExecutionControlStatusUnit` |

All five pairs now exist and have direct pair tests, including stateful execution-control/status priority, memory-wait, halt/trap, and reset-recovery coverage. Known binary CPU inputs are the interchangeability contract; conservative partial-unknown boundaries are documented in the per-block reports. The next step is structural core integration.

Generic operand/writeback muxes and direct data-memory connections wire these blocks. The current memory already handles widths, sign extension, range/alignment fault reporting, and faulting-store rejection, so a separate load/store forwarding box is unnecessary. `RV32ISingleCycleCore` and `RV32ISingleCycleSystem` are hierarchy containers rather than additional execution units.

The complete inventory, responsibilities, existing primitives, and datapath diagram are in `docs/rv32i-structural-implementation-plan.md` under **Structural CPU Building Blocks**.

Why two memory instances:

- A single-cycle CPU fetches an instruction and may perform a data load/store in the same cycle.
- A single single-port memory would require arbitration or a multi-cycle CPU.
- Two instances keep the first runner simple and make instruction/data flow easier to visualize.

The lower-level structural components remain the educational source of truth:

- `MemoryBit`, `Register32`, `RegisterFile32x32`, `Memory4x32`, and `Memory32x32` explain storage behavior.
- `RegisterFile32x32` keeps the selection/routing hierarchy visible but uses compact behavioral register words internally; the fully structural `Register32` is its representative storage-word proof.
- `BehavioralRegisterFile32x32` is the compact same-contract register-file counterpart.
- `BehavioralMemory64Kx32` remains the CPU-scale memory abstraction because a fully expanded 256 KiB memory is impractical; smaller structural memories establish the lower-level contract.

The component pairs follow one rule: freeze identical pins, build/test the structural implementation or representative lower-level slice first, then add the behavioral version and drive both with the same vectors or clock waveform. Compare settled functional outputs and matching sequential observation points, not necessarily every internal propagation timestamp. The behavioral member is not automatically product-facing; it is promoted to a fast configuration only when measured structural cost justifies it after equivalence is proven.

## Behavioral Answer Sheet And Structural Independence

`RV32IInstructionOracle`, `BehavioralRV32ICore`, and `RV32ISystem` form the executable answer sheet for structural RV32I development. They provide expected architectural results and a simulator-facing reference system while the structural datapath is built.

The structural RV32I must be an independent implementation:

- It may reuse instruction encodings, decoded-control contracts, program fixtures, and expected checkpoint data.
- Its tests execute the answer sheet and structural system independently from the same fixtures and compare their observable contracts. Pairwise component equivalence remains a non-visual regression concern.
- Its production execution path must not call the instruction oracle, instantiate the behavioral core, or copy hidden whole-instruction state transitions behind a structural shell.
- The component-level behavioral counterparts remain outside the structural execution path; they are references and optional fast substitutes, not hidden children of structural shells.
- CPU-scale behavioral memory remains an allowed storage abstraction because instruction semantics are still produced by the visible datapath.

The answer sheet is a correctness reference, not the product destination or structural blueprint. A disagreement is investigated against the ISA and regression tests rather than automatically treating either implementation as infallible.

Beginner-oriented documentation of the complete answer-sheet contract, execution flow, instruction behavior, jump/trap rules, and all 16 program tests is in `docs/behavioral-rv32i-beginners-guide.md`.

## CPU/System Boundary

Keep the CPU core separate from the system wrapper.

Recommended split:

- `RV32ISingleCycleCore`
  - owns the five structural blocks, operand/writeback muxes, and direct data-memory control wiring.
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

- Instruction fields and immediates should be visible structural wiring.
- Decode/control starts with a representative lower-level opcode/instruction-family slice and grows into a complete structural decoder. Its behavioral counterpart stays separate rather than becoming an internal bridge.
- PC and next-PC logic should be visible components because they are central to CPU learning.
- Every major block gets independent expected-value tests plus a direct structural/behavioral equivalence test. Direct equivalence alone is insufficient because both sides could share the same incorrect encoding assumption.
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

- Use `RegisterFile32x32` in the structural educational core and keep `BehavioralRegisterFile32x32` for compact tests and future fast configurations.
- Use two `BehavioralMemory64Kx32` instances for instruction and data memory.

Verification:

- Full memory/register stack tests.
- Behavioral register-file unknown-state tests.
- Explicit `RegisterFile32x32` versus `BehavioralRegisterFile32x32` pairwise equivalence remains to be added before structural CPU completion.
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

- This instruction oracle supplies the architectural answers used to grade both the behavioral reference system and the structural CPU.
- It is not the final product CPU component.
- It is not a visualizer scenario because it has no circuit topology.
- The implemented `RV32IInstructionLockstepTest` runs the oracle separately from a component under test and compares per-commit snapshots.

Done when:

- Every RV32I base instruction has instruction-oracle coverage.
- Small programs produce expected register and memory results.
- Trace rows contain enough named semantic fields for future component-CPU checkpoint comparison.

Report:

- `docs/rv32i-milestone-6-report.md`

### Milestone 7: Behavioral RV32I System Component

Status: complete as the behavioral answer-sheet baseline.

Purpose:

Build the simulator-facing behavioral answer sheet that runs raw programs, stabilizes the CPU/system contract, and supplies expected results for the later structural implementation.

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

- compare the behavioral system against `RV32IInstructionOracle` after every committed instruction.
- compare PC, all registers, halt/trap state and cause, logical memory access, and actual byte writes.
- reuse the same program cases later for structural-system lockstep checks.

Done when:

- `RV32ISystem` runs the 16 numbered raw RV32I program cases.
- tests cover arithmetic, load/store sizes, branches, jumps, loops, reset, halt, and traps.
- system commit checkpoints match the Milestone 6 instruction oracle.
- visualizer exposes scenario aliases for the numbered behavioral programs.

Plan:

- `docs/rv32i-milestone-7-plan.md`

### Milestone 8: Trace Comparator And Simulation Checkpoints

Status: implemented in lockstep form; a separate named full-trace comparator remains optional.

Purpose:

Compare the behavioral system's sampled simulation checkpoints against Milestone 6 instruction oracle trace rows.

Implemented deliverables:

- reusable `RV32IInstructionLockstepTest`
- incremental simulator advancement with stable instruction checkpoints
- `RV32ISystem` state adapter
- logical memory-access and actual byte-write comparison

Done when:

- tests can run the same program independently through `RV32IInstructionOracle` and `RV32ISystem`.
- checkpoints compare PC, registers, memory effects, halt, and trap state.
- comparisons are checkpoint-based, not every simulator event timestamp.

### Milestone 9: Paired Control-Flow Unit

Status: implemented and directly pair-tested with directed cases plus 64 deterministic randomized control combinations. See `docs/rv32i-control-flow-pair-report.md`.

Purpose:

Build the visible control-flow path and its compact same-contract counterpart after the behavioral system is stable.

Recommended components:

- `RV32IControlFlowUnit`, an expandable composite built from the existing `Register32`, `Adder32`, muxes, bit adapters, and small branch-decision gates.
- `BehavioralRV32IControlFlowUnit`, a separate `BasicComponent` added only after the structural contract tests pass.

Expected behavior:

- internal `Register32` stores current `pc`.
- sequential path computes `pc + 4`.
- branch path computes `pc + immediate`.
- `JAL` path computes `pc + immediate`.
- `JALR` path computes `(rs1 + immediate) & ~1`.
- branch decision supports `BEQ`, `BNE`, `BLT`, `BGE`, `BLTU`, and `BGEU`.
- target alignment is reported after JALR clears bit zero and only for a taken branch/jump.

Done when:

- PC reset behavior is tested.
- branch taken/not-taken cases are tested.
- `JALR` low-bit clearing is tested.
- each implementation passes independent expected-value tests.
- identical directed and randomized clock/input waveforms produce equivalent sampled state and outputs after settling.

### Milestone 10: Paired Decode-Control Unit

Status: implemented and directly pair-tested for all 40 supported instructions, representative illegal encodings, and 64 deterministic pseudo-random raw words. See `docs/rv32i-decode-control-pair-report.md`.

Purpose:

Turn instruction bits into visible fields, immediates, and raw control intent, with a compact component reference using the same pins.

Recommended components:

- `RV32IDecodeControlUnit`: structural field wiring, immediate formation, instruction-recognition terms, and output-control gates.
- `BehavioralRV32IDecodeControlUnit`: direct mapping through the existing pure `RV32IDecoder` and `RV32IControl` rules for known instructions plus the same documented unknown-input policy.

Responsibilities:

- Extract `rs1`, `rs2`, and `rd`.
- Construct I/S/B/U/J immediates.
- Select ALU operation and operands.
- Emit raw register-write, memory, branch, jump, halt, and trap intent.
- Keep architectural state updates outside both decoder implementations.

Done when:

- Every supported instruction form and representative illegal encoding has independent expected rows.
- Immediate construction is tested at positive/negative boundaries.
- Structural and behavioral components agree on supported forms, constrained-random raw words, and the documented unknown-bit cases.
- The structural component does not instantiate or call the behavioral component.

### Milestone 11: Finish Existing Pairs And Pair Execution Status

Status: complete. See `docs/rv32i-alu-pair-report.md`, `docs/rv32i-register-file-pair-report.md`, and `docs/rv32i-execution-status-pair-report.md`.

Purpose:

Complete the ALU/register-file pair coverage, then build the stateful pair that handles precise halt, trap, memory-fault, and architectural-write policy.

Pair-completion work:

- Add `BehavioralALU32` with the exact `ALU32` pins and operation encoding, then compare all operations, flags, boundaries, randomized inputs, and explicit unknown cases.
- Add a direct `RegisterFile32x32` versus `BehavioralRegisterFile32x32` equivalence test over reset, all addresses, writes, holds, `x0`, and unknown-policy cases.

Recommended status components:

- `RV32IExecutionControlStatusUnit`: structural alignment gates, priority logic, permission gates, and status storage.
- `BehavioralRV32IExecutionControlStatusUnit`: direct state/priority implementation with identical pins.

Data-memory wiring stays outside a load/store module:

- ALU output drives address.
- `rs2` drives store data.
- decoded size/sign controls drive memory.
- memory read data drives the writeback mux.
- final request permission combines with raw read/write intent.

Done when:

- Every one of the five block contracts now has both implementations.
- All five direct pairwise equivalence suites pass as well as independent expected-value suites.
- Misalignment, out-of-range faults, priority, halt/trap latching, reset recovery, and faulting-store rejection are tested.

### Milestone 12: Structural Single-Cycle CPU Core

Status: complete. See `docs/rv32i-structural-core-report.md`.

Purpose:

Wire the first CPU core from the structural member of all five pairs plus generic muxes and direct memory-interface wiring.

Implemented files:

- `include/modules/rv32i/RV32ISingleCycleCore.hpp`
- `src/modules/rv32i/RV32ISingleCycleCore.cpp`
- `include/tests/RV32ISingleCycleTests.hpp`
- `src/tests/RV32ISingleCycleTests.cpp`

Core blocks:

- `RV32IControlFlowUnit`.
- `RV32IDecodeControlUnit`.
- `RegisterFile32x32`.
- `ALU32`.
- `RV32IExecutionControlStatusUnit`.
- Generic operand/writeback muxes and direct instruction/data-memory wiring.

Done when:

- The core executes short arithmetic, memory, branch, and jump sequences.
- Register writeback works and `x0` remains zero.
- Illegal, halt, and fault paths suppress the correct side effects.
- A dependency guard proves no structural block invokes its behavioral counterpart, the oracle, or `BehavioralRV32ICore`.

### Milestone 13: Single-Cycle RV32I System

Status: complete. All 16 shared program cases pass structural instruction lockstep and their hard-coded final outcomes.

Purpose:

Create the first complete runnable RV32I machine.

Implemented files:

- `include/modules/rv32i/RV32ISingleCycleSystem.hpp`
- `src/modules/rv32i/RV32ISingleCycleSystem.cpp`
- `include/tests/RV32ISingleCycleTests.hpp`
- `src/tests/RV32ISingleCycleTests.cpp`

System blocks:

- One `RV32ISingleCycleCore`.
- One `BehavioralMemory64Kx32` instruction memory.
- One `BehavioralMemory64Kx32` data memory.
- Clock/reset wiring and program preload/test support.

Done when:

- A raw binary program can be loaded.
- The system runs until `EBREAK` or trap.
- All 16 shared behavioral-answer-sheet programs pass structural per-instruction lockstep and their hard-coded final outcomes.

### Milestone 14: Visualizer Integration

Status: complete for the first single-cycle system. Standalone block scenarios are separated, the structural core/system hierarchy has committed layouts, and the browser loads the 27,265-component contract scenario.

Purpose:

Expose the structural RV32I machine in the visualizer without overwhelming it.

Deliverables:

- Scenario alias such as `rv32i-single-cycle`.
- Layout defaults for CPU, instruction memory, and data memory.
- Expandable views of the five structural blocks.
- Summary labels for PC, instruction, register writes, memory access, and halt/trap state.

Done when:

- The visualizer can load the RV32I system scenario.
- The first demo program can be scrubbed through simulation history.

### Milestone 15: Assembly Workflow And External Validation

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
- Independent expected-value tests for both members of each major block pair.
- Direct structural/behavioral equivalence tests for all five major block contracts.
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
- All five major block contracts have structural and behavioral implementations with passing direct equivalence suites.
- The structural CPU reaches those results without invoking the oracle or behavioral core in its production execution path.
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

- Use the structural member of every major pair in the structural core.
- Keep behavioral counterparts separate and clearly named as references/fast substitutes.
- Treat behavioral memory as a tested scale abstraction backed by smaller structural memory components.
- Require direct pairwise equivalence and independent expected-value tests.
- Add program-level equivalence checks against the functional instruction oracle.

### Risk: Tautological Answer-Sheet Comparison

Mitigation:

- Execute the answer sheet and structural device under test independently.
- Keep `RV32IInstructionOracle` and `BehavioralRV32ICore` out of production structural dependencies.
- Reuse shared inputs and observable contracts, not hidden whole-instruction execution logic.

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

The first structural single-cycle milestone is complete. Immediate next steps are:

1. Differentially validate the answer sheet with Spike, Sail, or the official architecture tests.
2. Continue the 27,000-component browser work after the completed lossless indexed-state, event-driven-rendering, compression, and sub-pixel-only culling pass.
3. Decide deliberately whether to add a configurable nonzero reset vector.
4. Begin pipeline planning only after the single-cycle external-validation boundary is understood.
