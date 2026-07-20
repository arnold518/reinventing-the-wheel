# Structural RV32I Core And System Report

## Outcome

`RV32ISingleCycleCore` and `RV32ISingleCycleSystem` now execute the complete 16-program RV32I teaching suite through the structural datapath. The structural production path does not invoke `RV32IInstructionOracle`, `BehavioralRV32ICore`, or any of the compact behavioral block counterparts.

The behavioral implementation remains the answer sheet. Both systems run the same program cases independently and are graded after every committed instruction against the specification-oriented oracle and again against hard-coded final outcomes.

## Structural Core Hierarchy

`RV32ISingleCycleCore` contains:

- `RV32IControlFlowUnit`: structural PC state, PC+4, branches, jumps, JALR masking, and alignment checks.
- `RV32IDecodeControlUnit`: structural field extraction, immediate construction, instruction recognition, and raw control generation.
- `RegisterFile32x32`: visible write decoder, two read muxes, and 31 stored register words plus hardwired x0.
- `ALU32`: structural arithmetic, logic, shifting, comparison, and flags.
- `RV32IExecutionControlStatusUnit`: structural ready/fault priority, final write permissions, halt/trap state, and trap cause.
- Three generic `Mux4to1_32bit` components for ALU A, ALU B, and writeback selection.
- Two `ANDGate` components that combine the active memory request with raw load/store intent.
- Constants for zero operands and the always-active instruction read interface.

There is no monolithic behavioral executor hidden in this hierarchy. Every instruction result flows through the named blocks and their wires.

## Datapath Wiring

The important paths are:

```text
PC -> instruction-memory address -> instruction -> decoder
decoder register addresses -> register file -> rs1/rs2 values
decoder selectors + PC/rs1/rs2/immediate -> operand muxes -> ALU
ALU flags + decoder branch/jump controls -> control-flow unit
ALU result -> data-memory address
rs2 value -> data-memory write data
ALU result / load data / PC+4 -> writeback mux -> register file
memory ready/fault + decode intent + alignment -> execution/status
execution/status PC_WRITE -> PC state
execution/status REGISTER_WRITE -> register file
```

Instruction memory is always read as a 32-bit word. Data memory receives the decoded byte/halfword/word size and load sign-extension control. `MEMORY_REQUEST_ACTIVE` is combined with `MEM_READ` or `MEM_WRITE`, so a waiting or faulting request is visible to memory while prohibited architectural writes remain suppressed.

## Structural System

`RV32ISingleCycleSystem` contains one structural core and separate `BehavioralMemory64Kx32` instruction and data memories. Full-size memory remains a deliberate scale abstraction backed by the smaller structural memory slices and their tests.

The public system interface remains small:

- Inputs: `CLK`, `RST`, `ENABLE`
- Outputs: `PC[31:0]`, `HALTED`, `TRAPPED`

The core additionally exposes `TRAP_CAUSE` and `INSTRUCTION_ATTEMPT` for test and visualization adapters. The commit counter is test-owned rather than pretending that instruction count is an architectural CPU pin.

## Timing And Lockstep

The behavioral system can execute with a short event interval because one basic component performs an instruction step. The structural system needs time for decoder gates, register muxes, ALU paths, memory feedback, and status priority logic to settle before the active edge.

Structural lockstep therefore uses:

- an explicit reset warm-up;
- a 4,000-time-unit instruction period;
- a rising edge 1,000 units into the period;
- one committed instruction per active edge;
- bus capture immediately before the edge;
- architectural comparison after the edge and its propagation settle.

Initial component events are now scheduled idempotently. This lets structural tests initialize constants before advancing through reset without causing the normal test runner or browser visualizer to schedule the same initialization a second time in the past.

## Verification

Focused verification includes:

- `RV32ISingleCycleCoreSmokeTest`: structural ADDI followed by EBREAK.
- `RV32ISingleCycleSystemContractTest`: reset, disabled hold, enabled execution, halt, post-halt suppression, and reset recovery.
- `RV32ISingleCycleSystemProgram1Test` through `RV32ISingleCycleSystemProgram16Test`: all 16 shared program cases in per-instruction lockstep.

The 16 programs cover:

- register-register and immediate ALU operations;
- x0 write suppression;
- signed and unsigned comparisons and shifts;
- byte, halfword, and word loads/stores with sign/zero extension;
- all six conditional branch types;
- forward/backward control flow and loops;
- JAL and JALR link/target behavior;
- FENCE, EBREAK, and ECALL;
- illegal instructions;
- misaligned load/store and control-transfer traps;
- instruction, load, and store access faults;
- actual memory byte writes, including zero-valued bytes.

Every structural program passed both comparison layers:

1. The complete architectural state and memory transaction are compared after every instruction.
2. Final PC, instruction count, selected registers, halt/trap state and cause, and aggregate byte writes are compared with independent hard-coded outcomes.

The final repository-wide serial regression passed 142/142 tests in 525.19 seconds.

## Timeline And Visualization Fixes

Structural and behavioral block scenarios are separate, one-device tests. The five dual-DUT pair tests remain CTest-only equivalence regressions and are excluded from the browser selector.

The old control-flow pair test had randomized activity through time 44,500 but its last label stopped at 12,400. Its 64 deterministic random vectors now have checkpoints, so the last checkpoint is 44,400 and no recorded timestamp appears after it.

The browser exposes:

- `rv32i-system-structural`: the structural reset/enable/halt contract waveform.
- `rv32i-core-structural-smoke`: the short ADDI/EBREAK waveform.
- `rv32i-system-structural-program1` through `rv32i-system-structural-program16`.

The committed top-level layouts show the system as core plus two memories and show the core as the five major blocks plus its generic mux/gate wiring. The structural contract topology contains 27,265 components and 53,761 wires and ends exactly at its final 33,000 checkpoint.

The lossless browser performance pass is recorded in `docs/rv32i-visualizer-performance-report.md`. It keeps every structural signal exact while replacing repeated hierarchical state keys with indexed snapshots, rendering only after changes, compressing responses, bounding cached sessions, and stopping hierarchy descent only below one physical pixel.

## Current Boundary

- Reset PC is zero in this first structural version; the shared 16 cases all start at zero.
- Full-size memories are behavioral scale abstractions, not gate-level 256 KiB arrays.
- The internal instruction-attempt output is used by tests to confirm the datapath settled before each clock edge.
- External Spike, Sail, and official architecture-test differential validation remains a desirable next hardening step for the answer sheet and therefore for both systems.
