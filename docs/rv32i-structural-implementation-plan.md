# RV32I Structural Implementation Plan

Last updated: 2026-07-24
Branch: `unified-component-migration`

## Goal

Build a visible structural RV32I single-cycle system without replacing the current behavioral RV32I system until the structural version passes the same program-level tests.

The first target is not a fully gate-level CPU. It is a structural datapath
made from visible components where that is practical. Each replaceable block
has one public family; a build profile can select its structural or behavioral
fidelity for fast testing, equivalence checking, and visualization:

- Keep `RV32IReferenceSystem` and `RV32IReferenceCore` as the current reference implementation.
- Add a new `RV32ISingleCycleCore` and `RV32ISingleCycleSystem`.
- Reuse the existing structural `ALU32`.
- Use the existing `RegisterFile32x32` structural shape in the educational
  structural core and keep its behavioral fidelity as the compact selection.
- Continue using `Memory64Kx32` for CPU-scale instruction and data memory; `Memory4x32`, `Memory32x32`, `Register32`, and `MemoryBit` are its lower-level teaching path.
- Give all five major core-block families structural and behavioral fidelities
  with one shared pin contract and one public type name.
- Keep lower-level or representative-slice tests beneath every behavioral scale abstraction.

## Behavioral Answer-Sheet Contract

The behavioral RV32I implementation was built as the executable answer sheet for the structural RV32I implementation.

The answer sheet consists of:

- `RV32IInstructionOracle`, which defines expected architectural state transitions for one instruction.
- `RV32IReferenceCore` and `RV32IReferenceSystem`, which expose those semantics through the circuit simulator's clock, memory, and system contracts.
- `RV32ISystemProgramCase` fixtures and the lockstep harness, which define shared inputs and observable checkpoints.

It answers what each committed instruction should do: next PC, register state, halt/trap state, logical memory access, and byte writes. It is not the structural implementation blueprint.

The independence boundary is mandatory:

- Production structural components must not call `RV32IInstructionOracle::step`, instantiate `RV32IReferenceCore`, or delegate architectural state updates to either one.
- The structural execution path must derive its results from visible control-flow, decode/control, register-file, ALU, execution-status, memory-interface, and writeback logic.
- Tests and test adapters may run the answer sheet and the structural device under test separately, then compare their commit checkpoints.
- Shared instruction encodings, decoded-control contracts, program fixtures, and trace data are reusable contracts. Hidden instruction execution is not.
- Behavioral fidelities are permitted as component-level answer sheets and
  fast substitutes, but they must never be hidden inside the production
  structural implementation of the same family.
- `Memory64Kx32` remains a permitted CPU-scale storage abstraction because it represents a separately tested memory contract; it does not decide whole-instruction RV32I semantics.

If the two implementations disagree, investigate the structural datapath, the answer sheet, and the ISA rule. Do not force structural logic to reproduce an answer-sheet defect. Once corrected, the shared regression case must preserve the resolution.

## Current Readiness

The codebase already has the main pieces needed to start this work:

- `ALU32` is a structural composite component with 32-bit inputs, 32-bit output, comparison flags, and a 5-bit operation input.
- `RegisterFile32x32` and the behavioral fidelity of `RegisterFile32x32` provide structural and behavioral implementations of the same register-file contract. The structural file uses `Register32BitCellArray` words internally, while the fully expanded `Register32` provides the representative lower-level storage word.
- the behavioral fidelity of `RegisterFile32x32` exposes register contents over time, which is useful for fast lockstep state comparison.
- `Memory64Kx32` supports program/data loading and memory-content introspection over time.
- `RV32IDecoder`, `RV32IControl`, and `RV32IInstructionOracle` provide tested pure-C++ instruction semantics.
- `RV32IInstructionLockstepTest` already compares a component under test against the oracle instruction by instruction.
- The shared program cases in `RV32IProgramCases.hpp` drive structural,
  balanced-profile, and reference-system tests.
- The answer sheet uses independent Harvard instruction/data memories on both sides of lockstep comparison.
- Lockstep store checks observe real simulated memory-bus transactions, including zero bytes that do not change final memory contents.
- A deliberately wrong lockstep probe proves that the comparison path detects a corrupted architectural register.
- `RV32IReferenceSystemContractTest` covers reset, enable/hold, public outputs, Harvard separation, halt, and reset recovery.
- Specification-derived oracle vectors cover wraparound, immediate sign extension, shift masking, JALR operand ordering, load-to-`x0` faults, backward branches, and precise control-transfer misalignment traps.
- Every numbered program has hard-coded final PC, instruction-count, register, halt/trap, and aggregate bus-write expectations in addition to per-instruction oracle lockstep.

See `docs/behavioral-rv32i-beginners-guide.md` for the detailed answer-sheet contract and explanations of all 16 program fixtures.

All five major block contracts, the structural single-cycle core, the structural system wrapper, shared lockstep reuse, direct memory wiring, and core/system visualization are implemented. Each structural and behavioral block also has its own single-device visualizer scenario. See `docs/rv32i-structural-core-report.md` for the completed integration record.

Verified baseline before structural implementation begins:

- focused hardened answer-sheet suite: 21/21 passed
- Release RV32I-related suite: 25/25 passed
- full repository regression: 109/109 passed

Current verification after structural core/system and visualizer integration landed:

- current unified-suite regression, including ELF/external validation:
  129/129 passed in 637.57 seconds
- `RV32IExecutionControlStatusUnitEquivalenceTest`: 30 labeled state/priority checkpoints passed
- all 16 structural, all 16 balanced-profile, and all 16 reference-system
  program tests pass their shared lockstep and hard-coded final outcomes
- browser topology construction: all ten standalone block aliases load one
  structural or behavioral root each; after unified profile migration the live
  structural Program 9 topology exposes 54,049 components, 180,155 pins, and
  103,361 wires

Known validation boundary:

- A strict ELF32 loader and external `tohost` runner are implemented. One
  externally assembled smoke ELF has passed Sail 0.13 and both CircuitSim
  system fidelities without using the internal instruction oracle.
- The pinned official ACT4 `I` workflow generated 39 self-checking ELFs. All
  39 pass Sail 0.13 and the behavioral CircuitSim system. A fully structural
  sample passes, but the full structural sweep remains resource-bound at a
  measured estimate of about 25 serial hours and roughly 1.1 GiB for the
  smallest test. See `docs/rv32i-external-validation-report.md`.
- This does not change the deciding rule: when a structural result,
  answer-sheet result, external model, and ISA reading disagree, the ratified
  RV32I specification remains the source to investigate against.

## Current Phase Status

| Phase | Status | Meaning |
| --- | --- | --- |
| Answer sheet and generic test infrastructure | Complete | The oracle, behavioral system, Harvard lockstep, real write observation, public contract, mismatch detector, and 16 independent outcomes are ready. |
| Phase 1: control-flow family | Complete | Structural and behavioral fidelities share one contract and pass `RV32IControlFlowUnitEquivalenceTest`. |
| Phase 2: decode/control family | Complete | The gate/composite decoder and direct answer-sheet evaluator agree for all 40 supported instructions, 12 directed illegal encodings, and 64 deterministic raw words. |
| Phase 3A: register-file and ALU equivalence | Complete for known binary CPU inputs | The isolated register-file equivalence test and 128-row ALU equivalence test pass; conservative partial-unknown behavioral boundaries are documented. |
| Phase 3B: execution control/status family | Complete | Structural gates/muxes/storage and the same-contract direct evaluator pass isolated runs of the same stateful waveform. |
| Phases 4-5: structural core and system | Complete | The five blocks, mux layer, and separate memory interfaces execute all 16 shared program cases in per-instruction lockstep and against hard-coded final outcomes. |
| Phase 6A: block visualizer | Complete | Ten browser scenarios exercise the two fidelities separately. Structural roots are expandable; behavioral roots show the compact answer-sheet boundary. |
| Phase 6B: core/system visualizer | Complete | The structural system and core have committed hierarchy layouts; the unified fully structural topology contains 54,049 components. |
| Phase 6C: lossless visualizer performance | Complete | Indexed bulk snapshots preserve every pin/wire value, gzip reduces transport, idle redraw stops, session caching is bounded, and hierarchy descent stops only below one physical pixel. See `docs/rv32i-visualizer-performance-report.md`. |
| External RV32I validation boundary | In progress | Strict ELF loading and the smoke fixture pass both fidelities. The complete 39-test ACT4 `I` set passes Sail and behavioral CircuitSim; a full structural run awaits a safe low-history execution path. |

## Immediate Next Deliverable

The first structural single-cycle CPU milestone and its first lossless
browser-performance pass are implemented. External validation now has a
Sail-proven smoke fixture and a complete 39-test ACT4 Sail/behavioral pass.
The remaining hardening is a safe low-history full-structural ACT4 run, an
optional deliberate nonzero reset-vector contract, and conservative
cold-start/topology-sharing improvements for the 54,000-component hierarchy.

`RV32IExecutionControlStatusUnit` now centralizes the decisions that the first four blocks deliberately do not own: whether the current instruction may update PC or a register, whether a memory request stays active while waiting, which halt/trap state latches, and which fault wins when several requests are present together. Its detailed implementation and verification record is in `docs/rv32i-execution-status-equivalence-report.md`.

The 16 program builders, final-result validator, and structural lockstep adapter should be extracted when the first structural core/system test needs them. They are reuse/integration tasks, not missing generic testing infrastructure.

## Structural CPU Building Blocks

The structural CPU is a network of small blocks. No single production block should secretly execute a whole RV32I instruction.

### Datapath overview

```text
                         ┌─────────────────────────────┐
                         │ execution control/status    │
                         │ enables PC/RF and requests  │
                         └──────────────┬──────────────┘
                                        │
                                        v
┌────────────┐   ┌─────────────┐   ┌───────────────┐
│ Program PC │──>│ instruction │──>│ fields +      │
│ register   │   │ memory      │   │ control + imm │
└─────┬──────┘   └─────────────┘   └───────┬───────┘
      ^                                      │ register addresses/control
      │                                      v
      │                              ┌───────────────┐
      │                              │ register file │
      │                              └───────┬───────┘
      │                                      │ operands
      │                                      v
      │                              ┌───────────────┐
      │                              │ operand muxes │
      │                              └───────┬───────┘
      │                                      v
      │                              ┌───────────────┐
      │                              │ structural    │
      │                              │ ALU32         │
      │                              └───┬───────┬───┘
      │                                  │       │ flags
      │                         address  │       v
      │                                  │  ┌───────────────┐
      │                                  │  │ branch        │
      │                                  │  │ decision      │
      │                                  │  └───────┬───────┘
      │                                  v          │
      │                           ┌────────────┐    │
      │                           │ direct     │    │
      │                           │ data mem   │    │
      │                           └─────┬──────┘    │
      │                                 │ load      │
      │                                 v           │
      │                           ┌────────────┐    │
      │                           │ writeback  │    │
      │                           │ mux        │    │
      │                           └─────┬──────┘    │
      │                                 └──> register file
      │                                             │
      └──────────────── next-PC logic <─────────────┘
```

### Five major core-block contracts

The recommended first core uses five major peer-block contracts, not sixteen. Five is an organization choice, not an ISA requirement: a later design may merge or split blocks if their contracts remain visible and independently testable.

Each family provides two independent fidelities behind one public contract:

- The structural implementation is the educational source used by `RV32ISingleCycleCore`. It is an expandable composite whose lower-level children remain visible.
- The behavioral fidelity uses an internal childless `BasicComponent`
  evaluator as a reference/bring-up tool. It has the same external pins and
  must not instantiate, call, or read private state from the structural
  implementation. Parents and profiles select the family and fidelity; they do
  not name this internal evaluator.

| Block family | Structural fidelity | Behavioral fidelity | Current state |
| --- | --- | --- | --- |
| Control flow | `RV32IControlFlowUnit`: `Register32`, `PC+4`/target adders, JALR mask, branch gates, next-PC mux, alignment check | the behavioral fidelity of `RV32IControlFlowUnit`: direct PC/target/branch calculation with the same clocked contract | Complete; focused equivalence test passes |
| Decode/control | `RV32IDecodeControlUnit`: field rewires, I/S/B/U/J immediate wiring, shared opcode/function predicates, instruction-recognition gates, and control-output gates | the behavioral fidelity of `RV32IDecodeControlUnit`: maps known instructions through `RV32IDecoder`/`RV32IControl` | Complete for all 40 legal forms, 12 directed illegal encodings, and 64 deterministic raw words; partial-unknown boundary documented |
| Register file | `RegisterFile32x32`: visible decoder/read muxes/register-word hierarchy | the behavioral fidelity of `RegisterFile32x32`: compact direct state | Complete; isolated equivalence exercises x0 and every x1-x31 entry |
| ALU | `ALU32`: existing add/subtract, logic, shifter, comparator, and zero-detect composites | the behavioral fidelity of `ALU32`: direct operation/flag calculation using the identical pins and op encoding | Complete for known binary inputs; conservative partial-unknown boundary documented |
| Execution control/status | `RV32IExecutionControlStatusUnit`: alignment gates, status storage, trap-priority muxing, and permission gates | the behavioral fidelity of `RV32IExecutionControlStatusUnit`: direct state machine and priority calculation with the same pins | Complete; stateful priority/handshake equivalence test passes |

Generic operand and writeback muxes connect these blocks, but they do not need RV32I-specific component classes. Use existing `Mux4to1_32bit` instances inside the core unless a smaller reusable mux materially improves the circuit.

There is no separate `RV32ILoadStoreUnit` in the first design. With the current memory contract, the core connections are mostly direct:

```text
DMEM_ADDR        = ALU.OUT
DMEM_WRITE_DATA  = RS2_DATA
DMEM_SIZE        = decoded MEM_SIZE
DMEM_SIGN_EXTEND = decoded LOAD_SIGN_EXTEND
DMEM_READ_EN     = MEMORY_REQUEST_ACTIVE AND decoded MEM_READ
DMEM_WRITE_EN    = MEMORY_REQUEST_ACTIVE AND decoded MEM_WRITE
writeback load   = DMEM_READ_DATA
```

`Memory64Kx32` already selects byte/halfword/word lanes, sign-extends loads, checks alignment/range, reports ready/fault, rejects faulting stores, and records actual byte writes. The only CPU-specific memory logic is low-bit address-alignment classification so the correct load/store trap cause can be latched; that small logic belongs in `RV32IExecutionControlStatusUnit`.

`RV32ISingleCycleCore` and `RV32ISingleCycleSystem` are hierarchy containers, not two more execution units:

- `RV32ISingleCycleCore` wires the structural implementation of each of the five contracts, generic muxes, and direct memory-interface signals.
- `RV32ISingleCycleSystem` contains the core plus instruction and data memory.

The current monolithic `RV32IReferenceCore` remains the independent
program-level answer sheet. It does not count as a behavioral fidelity of the
five block families because it does not expose their individual pin contracts.

### Equivalence rules

The dual implementation strategy is useful only if the two sides remain genuinely independent:

1. Freeze one pin contract and one encoding table for both implementations.
2. Build and test the structural implementation or representative lower-level slice first.
3. Register a behavioral fidelity only after the lower-level contract is tested.
4. Run the same directed and randomized inputs through both fidelities via the
   public family/profile construction path.
5. For combinational blocks, compare after both implementations settle; exact internal event timestamps need not match.
6. For sequential blocks, drive the same reset/clock/input waveform and compare state and outputs at the same observation points.
7. Test known-value behavior exhaustively where practical and define separate conservative `UNKNOWN`/`HIGH_IMPEDANCE` behavior. A behavioral block must not simply turn the whole result unknown if the structural circuit preserves some known lanes.
8. Keep `RV32IInstructionOracle` and `RV32IReferenceCore` out of every structural component's production dependencies.

Pairwise equivalence complements, rather than replaces, independent expected-value tests. If both implementations share a mistaken encoding table, direct equivalence alone can let the same mistake pass twice.

### Responsibilities inside the five blocks

The following responsibilities are still required, but they are internals or wiring rather than sixteen peer components:

- PC register, `PC+4`, branch decision, jump targets, next-PC mux, and target-alignment check live in `RV32IControlFlowUnit`.
- Instruction fields, immediate generation, and raw instruction control live in `RV32IDecodeControlUnit`.
- Operand selection and writeback selection use generic mux components directly inside the core.
- Data-memory address/data/size/sign/read/write paths are direct core wiring; only address-fault classification and request permission live in `RV32IExecutionControlStatusUnit`.
- Halt state, trap state/cause, and final architectural write permissions live in `RV32IExecutionControlStatusUnit`.
- Instruction-memory access is system/core wiring: PC drives address, and memory returns data/ready/fault.
- Test catalogs, validators, and adapters remain test infrastructure rather than CPU blocks.

### Exact inputs and outputs of the five blocks

#### 1. `RV32IControlFlowUnit`

| Direction | Pins |
| --- | --- |
| Inputs | `CLK`, `RST`, `PC_WRITE`, `RS1_VALUE[32]`, `IMM[32]`, `BRANCH_TYPE[3]`, `JUMP_TYPE[2]`, `EQ`, `LT_SIGNED`, `LT_UNSIGNED` |
| Outputs | `PC[32]`, `PC_PLUS_4[32]`, `NEXT_PC_CANDIDATE[32]`, `BRANCH_TAKEN`, `PC_MISALIGNED`, `TARGET_MISALIGNED` |

This is sequential plus combinational: it stores PC and calculates all possible following PCs. `PC_WRITE` is the final permission from the execution-control/status unit.

#### 2. `RV32IDecodeControlUnit`

| Direction | Pins |
| --- | --- |
| Input | `INSTRUCTION[32]` |
| Register/immediate outputs | `RS1_ADDR[5]`, `RS2_ADDR[5]`, `RD_ADDR[5]`, `IMM[32]` |
| ALU outputs | `ALU_OP[5]`, `ALU_A_SEL[2]`, `ALU_B_SEL[2]` |
| Raw side-effect outputs | `LEGAL`, `REG_WRITE`, `MEM_READ`, `MEM_WRITE`, `WRITEBACK_SEL[2]` |
| Memory outputs | `MEM_SIZE[2]`, `LOAD_SIGN_EXTEND` |
| Control-flow outputs | `BRANCH_TYPE[3]`, `JUMP_TYPE[2]` |
| Stop/exception outputs | `HALT_REQUEST`, `TRAP_REQUEST`, `DECODE_TRAP_CAUSE[4]` |

These are raw instruction intentions, not final enables. For example, JAL may request register writeback, but the execution-control/status unit removes that permission if the target is misaligned.

#### 3. Register-file implementations

| Direction | Pins |
| --- | --- |
| Inputs | `RS1_ADDR[5]`, `RS2_ADDR[5]`, `RD_ADDR[5]`, `WRITE_DATA[32]`, `REG_WRITE`, `CLK`, `RST` |
| Outputs | `RS1_DATA[32]`, `RS2_DATA[32]` |

This contract already exists in both `RegisterFile32x32` and the behavioral fidelity of `RegisterFile32x32`. The core connects final—not raw—register-write permission to `REG_WRITE`.

#### 4. `ALU32`

| Direction | Pins |
| --- | --- |
| Inputs | `A[32]`, `B[32]`, `OP[5]` |
| Main output | `OUT[32]` |
| Flag outputs | `ZERO`, `EQ`, `LT_SIGNED`, `LT_UNSIGNED`, `NEGATIVE`, `CARRY_OUT`, `OVERFLOW` |

This contract already exists in structural `ALU32`. the behavioral fidelity of `ALU32` must copy these pins and the `ALU32Op` encoding exactly. Generic core muxes select `A` and `B`.

#### 5. `RV32IExecutionControlStatusUnit`

| Direction | Pins |
| --- | --- |
| Clock/control inputs | `CLK`, `RST`, `ENABLE` |
| Raw decode inputs | `LEGAL`, `REG_WRITE`, `MEM_READ`, `MEM_WRITE`, `HALT_REQUEST`, `TRAP_REQUEST`, `DECODE_TRAP_CAUSE[4]` |
| Address/control inputs | `ALU_ADDRESS[32]`, `MEM_SIZE[2]`, `PC_MISALIGNED`, `TARGET_MISALIGNED` |
| Memory inputs | `IMEM_READY`, `IMEM_FAULT`, `DMEM_READY`, `DMEM_FAULT` |
| Final permission outputs | `PC_WRITE`, `REGISTER_WRITE`, `MEMORY_REQUEST_ACTIVE` |
| Status outputs | `HALTED`, `TRAPPED`, `TRAP_CAUSE[4]` |
| Debug/contract outputs | `DATA_ADDRESS_MISALIGNED`, `INSTRUCTION_ATTEMPT` |

This block is a real independent module, not just wiring. It contains state and trap-priority logic. It decides whether the current instruction completes normally, waits, halts, or traps, and it suppresses inappropriate PC/register changes.

Decision priority for one instruction attempt:

1. reset clears status
2. previously halted/trapped state holds
3. unavailable instruction/data response waits without an attempt
4. current PC misalignment or instruction-memory fault traps
5. illegal instruction or ECALL traps; EBREAK halts
6. load/store address misalignment or aligned data-memory fault traps
7. taken branch/JAL/JALR target misalignment traps
8. otherwise permit the instruction's normal PC/register effects

### Direct core wiring around the blocks

These paths do not justify separate RV32I modules:

| Destination | Source/selection |
| --- | --- |
| ALU `A` | mux of `RS1_DATA`, `PC`, or zero using `ALU_A_SEL` |
| ALU `B` | mux of `RS2_DATA`, `IMM`, or zero using `ALU_B_SEL` |
| register `WRITE_DATA` | mux of `ALU.OUT`, `DMEM_READ_DATA`, or `PC_PLUS_4` using `WRITEBACK_SEL` |
| `IMEM_ADDR` | `PC` |
| `DMEM_ADDR` | `ALU.OUT` |
| `DMEM_WRITE_DATA` | `RS2_DATA` |
| `DMEM_SIZE` | decoded `MEM_SIZE` |
| `DMEM_SIGN_EXTEND` | decoded `LOAD_SIGN_EXTEND` |
| `DMEM_READ_EN` | `MEMORY_REQUEST_ACTIVE AND MEM_READ` |
| `DMEM_WRITE_EN` | `MEMORY_REQUEST_ACTIVE AND MEM_WRITE` |

### Existing primitives to reuse

These are already implemented and tested; do not rebuild them under RV32I-specific names without a contract reason:

- gates: NOT, AND, OR, XOR, NAND, NOR
- storage: `DFlipFlop`, `Register32`, structural register/memory slices
- routing: `Rewire`, `BitSplitter`, `BitJoiner`, constants
- selection: one-bit muxes and existing multi-bit mux variants, including `Mux4to1_32bit`
- arithmetic: `Adder32`, `AddSub32`
- execution: `Logic32`, `Shifter32`, `Comparator32`, `ZeroDetect32`, `ALU32`
- profile-selectable register storage: structural and behavioral fidelities of
  the `RegisterFile32x32` family
- scale memory: `Memory64Kx32`, backed educationally by the existing smaller structural storage path

### Blocks that are not CPU hardware

The following are necessary for development but must stay outside the production structural execution path:

- `RV32IInstructionOracle` and `RV32IReferenceCore`: whole-instruction/program answer sheet only
- the behavioral fidelities of `RV32IControlFlowUnit`,
  `RV32IDecodeControlUnit`, `ALU32`, and
  `RV32IExecutionControlStatusUnit`: same-contract references and optional fast
  selections, never children of their structural implementations
- shared 16-program case catalog: test input data
- hard-coded expected-result validator: test output checker
- `RV32IStructuralLockstepTestBase`: structural test adapter
- program/data preload helpers and memory-history readers: setup/observation tools

The structural CPU may be tested by these tools, but it must calculate its own result through the blocks in the inventory above.

## Important Design Decisions

New RV32I circuit components should live under:

- `include/modules/rv32i/`
- `src/modules/rv32i/`

This matches the current project layout. Older roadmap notes mention `modules/riscv`; use `modules/rv32i` for this implementation.

The reusable structural system should preserve the public system pins already used by the behavioral system:

- Inputs: `CLK`, `RST`, `ENABLE`
- Outputs: `PC[32]`, `HALTED`, `TRAPPED`

The structural core should use a stable memory interface, not copy the current behavioral core's internal memory-clock workaround:

- Instruction memory uses `IMEM_ADDR[32]`, `IMEM_READ_DATA[32]`, `IMEM_READY`, and `IMEM_FAULT`; the system wrapper supplies the constant word-read controls.
- Data memory uses `DMEM_ADDR[32]`, `DMEM_WRITE_DATA[32]`, `DMEM_READ_DATA[32]`, `DMEM_READ_EN`, `DMEM_WRITE_EN`, `DMEM_SIZE[2]`, `DMEM_SIGN_EXTEND`, `DMEM_READY`, and `DMEM_FAULT`.
- The system clock drives the PC, register file, and data memory.
- Combinational datapath signals settle before the active clock edge.
- The first system uses the current always-ready behavioral memories. `READY` remains in the core boundary so a later variable-latency memory can hold the instruction instead of silently committing incomplete data.

The current behavioral core emits separate `IMEM_CLK` and `DMEM_CLK` pulses from inside the core. That behavior is useful for the existing abstraction, but it is the wrong contract for the structural single-cycle datapath.

For testing, the structural system needs an adapter that can produce the same lockstep snapshot shape as the behavioral system:

- PC from the `PC` output pins.
- Registers from the selected register-file implementation's state/history adapter; the structural runner uses `RegisterFile32x32`.
- Data memory from `Memory64Kx32`.
- Instruction count from the test adapter or an internal debug/commit signal.
- Last memory access and writes from direct core memory-interface signals and memory introspection.

Precise side-effect policy:

- One enabled rising edge represents one instruction attempt, including an instruction that halts or traps.
- PC update and register write must be gated by the same valid-instruction decision.
- A faulting load must not write its destination.
- `DMEM_WRITE_EN` is a store request, not proof that memory changed. It may be asserted while the memory reports a fault; `Memory64Kx32` must then reject the request and record no byte-write transaction.
- Store requests must still be suppressed when reset, disabled, already halted/trapped, illegal, or not a store.
- A misaligned taken branch/JAL/JALR must keep the faulting PC; JAL/JALR must not write the link register.
- A non-taken branch must ignore alignment of its unused target.
- `EBREAK` sets the project halt latch without advancing PC; other traps latch a specific cause without advancing PC.
- Halt/trap state remains latched until reset, and `ENABLE=LOW` must suppress every architectural side effect.

## Implementation Phases

### Completed Foundation: Answer Sheet And Generic Testing

Status: complete. This is not a remaining structural implementation phase.

Completed infrastructure:

- The current behavioral system, oracle, and 16 numbered program results form the initial answer-sheet baseline.
- Oracle and component grading both use separate instruction and data memories.
- Per-instruction checks observe actual memory-bus writes.
- The generic lockstep mismatch self-test proves that corrupted state is rejected.
- Semantic checks remain active in Release builds.
- All 16 behavioral program tests have hard-coded final outcomes in addition to per-instruction lockstep.
- The full repository regression passes.

Structural-specific integration is now complete:

- `rv32iSystemProgramCase` exposes the same 16 program builders to both systems.
- `verifyRV32ISystemExpectedResult` grades both systems against the same independent final outcomes.
- `RV32ISingleCycleSystemProgramTestBase` adapts real PC, register, status, and memory-bus signals to the generic lockstep harness.
- Structural tests use a measured conservative 4,000-unit cycle rather than inheriting behavioral timing.
- Structural smoke and public-contract tests cover reset, enable/hold, halt suppression, and reset recovery.

### Phase 1: Control-Flow Implementations

Status: complete. See `docs/rv32i-control-flow-equivalence-report.md`.

Add structural `RV32IControlFlowUnit` as an expandable composite. After its structural tests pass, add compact the behavioral fidelity of `RV32IControlFlowUnit` with exactly the same pins.

Inputs:

- `CLK`, `RST`, `PC_WRITE`
- `RS1_VALUE[32]`, `IMM[32]`
- `BRANCH_TYPE[3]`, `JUMP_TYPE[2]`
- `EQ`, `LT_SIGNED`, `LT_UNSIGNED`

Outputs:

- `PC[32]`, `PC_PLUS_4[32]`, `NEXT_PC_CANDIDATE[32]`
- `BRANCH_TAKEN`, `PC_MISALIGNED`, `TARGET_MISALIGNED`

Visible internals:

- existing structural `Register32` stores PC
- `Adder32` computes `PC+4`
- another structural addition path computes `PC+IMM`
- JALR path computes `(RS1+IMM)&~1`
- small gates choose BEQ/BNE/BLT/BGE/BLTU/BGEU from ALU flags
- muxes choose fallthrough, branch/JAL, or JALR candidate
- low target bits report misalignment only for a taken branch or jump

Tests:

- reset to zero, final write permission, and hold behavior
- normal `PC+4`, backward/forward branches, JAL, and JALR
- all six branch decisions
- JALR bit-zero clearing and old-`rs1` input behavior
- precise misaligned taken branch/JAL/JALR reporting
- non-taken would-be-misaligned branch does not report a fault
- simulator time-travel state
- identical directed and randomized waveforms through both implementations, sampled after settling and at matching clock observations

### Phase 2: Decode-Control Implementations

Status: complete for the binary instruction contract used by CPU execution. See `docs/rv32i-decode-control-equivalence-report.md`.

Add structural `RV32IDecodeControlUnit` as one major composite with visible lower-level children for field splitting, immediate formation, instruction recognition, and control decoding. After the structural field/immediate path and at least one representative instruction-family control slice pass, complete the structural decoder and add the behavioral fidelity of `RV32IDecodeControlUnit` with the same pins.

Inputs:

- `INSTRUCTION[32]`

Outputs:

- `RS1_ADDR[5]`, `RS2_ADDR[5]`, `RD_ADDR[5]`
- `IMM[32]`
- `LEGAL`, `ALU_OP[5]`, `ALU_A_SEL[2]`, `ALU_B_SEL[2]`
- raw `REG_WRITE`, `MEM_READ`, `MEM_WRITE`
- `WRITEBACK_SEL[2]`, `MEM_SIZE[2]`, `LOAD_SIGN_EXTEND`
- `BRANCH_TYPE[3]`, `JUMP_TYPE[2]`
- raw `HALT_REQUEST`, `TRAP_REQUEST`, and `DECODE_TRAP_CAUSE[4]`

Build the field and immediate paths structurally from bit adapters, rewires,
constants, muxes, and gates. Build instruction-recognition terms and combine
them into the raw control outputs. The behavioral fidelity may use
`RV32IDecoder` and `RV32IControl` for known 32-bit inputs, but its evaluator
remains a separate family implementation under test; it is not an internal
bridge inside the structural `RV32IDecodeControlUnit`.

Tests:

- every field position and all I/S/B/U/J immediate formats
- every supported instruction's raw control outputs against `RV32IControl`
- illegal encodings, ECALL, EBREAK, and FENCE
- representative structural slice versus independent expected rows
- structural versus behavioral component equivalence for all supported instruction forms, representative illegal encodings, constrained-random raw words, and the documented unknown-bit policy

### Phase 3: Complete All Five Block Equivalence Suites

Status: complete. See `docs/rv32i-register-file-equivalence-report.md`, `docs/rv32i-alu-equivalence-report.md`, and `docs/rv32i-execution-status-equivalence-report.md`.

Before core integration, complete the two contracts whose structural implementations already exist:

- Add the behavioral fidelity of `ALU32` with the exact `ALU32` pins, operation encoding, output flags, and defined unknown-value behavior.
- `ALU32EquivalenceTest` now covers every operation encoding, boundary vectors, 64 deterministic randomized known vectors, direct output/flag equality, and the behavioral partial-unknown safety rule. Direct partial-unknown equality is intentionally not claimed because the structural gates can preserve definite sub-results while the compact model invalidates the whole result.
- `RV32IRegisterFileEquivalenceTest` runs the same reset/read/write sequence independently for each implementation, selects every architectural entry through both read paths, and checks both traces against the same expected architectural values.

Build the mux layer needed by the core.

Datapath selections:

- ALU A: register rs1 or PC.
- ALU B: register rs2 or immediate.
- Writeback: ALU result, load result, PC+4, or zero/unused.

The structural `RV32IExecutionControlStatusUnit` now provides precise instruction completion:

- latch `HALTED`, `TRAPPED`, and `TRAP_CAUSE[4]`
- combine raw decode intent with reset, enable, memory ready/fault, instruction fault, and target-misalignment signals
- produce final `PC_WRITE` and `REGISTER_WRITE`
- produce `MEMORY_REQUEST_ACTIVE` for memory requests, gated by reset/enable/previous halt/trap/legality and raw load/store intent
- produce an internal `INSTRUCTION_ATTEMPT` indication for the lockstep adapter

the behavioral fidelity of `RV32IExecutionControlStatusUnit` has the identical pins and state semantics and is checked directly against the structural unit.

PC and register permissions must agree on illegal instruction, ECALL, EBREAK, instruction fault, load/store fault, and control-target misalignment. A data-memory request is not itself an architectural write; actual memory history proves whether a fault-free store committed.

`MEMORY_REQUEST_ACTIVE` must not depend combinationally on the current `DMEM_FAULT`, because the memory needs an active request in order to report that fault. The fault feeds PC/register/status decisions, while the memory component itself rejects a faulting store at its active edge.

Wire the data-memory interface directly as documented above. Add only the low-bit address checker needed to distinguish load/store address-misaligned traps from access faults; keep it as visible internals of this unit. Use existing mux components for operands and writeback.

Tests:

- Selector truth tables.
- Stable output after input transitions.
- Writeback source selection for ALU, load, JAL, and JALR cases.
- Faulting JAL/JALR suppresses link writeback.
- Faulting loads suppress register writeback; faulting stores produce no actual memory byte write.
- `ENABLE=LOW` and previously halted/trapped state suppress PC/register changes and memory requests.
- Halt/trap cause latches and reset recovery.
- Direct LB/LBU/LH/LHU/LW and SB/SH/SW memory wiring matches the memory component contract.
- Structural/behavioral status-unit equivalence over identical reset, enable, ready, fault, halt, trap, and recovery waveforms.

### Phase 4: RV32ISingleCycleCore

Status: complete. The production hierarchy uses only structural members of the five block contracts plus generic muxes, gates, and constants.

Add `RV32ISingleCycleCore` as a composite component.

Internal children:

- `RV32IControlFlowUnit`
- `RV32IDecodeControlUnit`
- `RegisterFile32x32`
- `ALU32`
- `RV32IExecutionControlStatusUnit`
- generic operand and writeback mux instances
- direct instruction/data-memory interface wiring
- Constants and rewires as needed

External pins:

- Inputs: `CLK`, `RST`, `ENABLE`, `IMEM_READ_DATA[32]`, `IMEM_READY`, `IMEM_FAULT`, `DMEM_READ_DATA[32]`, `DMEM_READY`, `DMEM_FAULT`
- Outputs: `PC[32]`, `IMEM_ADDR[32]`, `DMEM_ADDR[32]`, `DMEM_WRITE_DATA[32]`, `DMEM_READ_EN`, `DMEM_WRITE_EN`, `DMEM_SIZE[2]`, `DMEM_SIGN_EXTEND`, `HALTED`, `TRAPPED`

`TRAP_CAUSE[4]` and the instruction-attempt indication may be internal debug outputs/getters for test and visualization adapters. They should not expand the six-pin public system surface.

Cycle behavior:

1. PC drives instruction memory address.
2. Instruction data/fault and register outputs settle before the active edge.
3. Instruction bits drive decode, immediate generation, ALU control, branch decision, target generation, and memory controls.
4. Load/store data/fault, writeback data, next PC, and the centralized side-effect decision settle.
5. On an enabled, ready active edge, exactly one instruction attempt updates the permitted PC/register/status/memory state.
6. Halt, trap, or a precise current-instruction fault suppresses the prohibited side effects described above.
7. Once `HALTED` or `TRAPPED` is latched, later edges hold architectural state until reset.

Tests:

- Single ADDI instruction.
- x0 remains zero.
- Register-register arithmetic.
- Branch taken and branch not taken.
- JAL and JALR.
- Load/store smoke.
- Illegal instruction, ECALL, EBREAK, fetch fault, load/store fault, and misaligned control-transfer behavior.
- Reset/enable/hold and public/internal status behavior.
- A source/dependency guard proving the production structural core does not invoke the oracle or behavioral core.

### Phase 5: RV32ISingleCycleSystem

Status: complete. All 16 shared programs pass structural per-instruction lockstep and independent hard-coded final expectations.

Add `RV32ISingleCycleSystem` as the user-facing structural system.

Internal children:

- `RV32ISingleCycleCore`
- `Memory64Kx32` named `INSTRUCTION_MEMORY`
- `Memory64Kx32` named `DATA_MEMORY`

Public pins:

- Inputs: `CLK`, `RST`, `ENABLE`
- Outputs: `PC[32]`, `HALTED`, `TRAPPED`

System helper APIs should mirror the current behavioral `RV32IReferenceSystem` where practical:

- `loadProgram`
- `loadInstructionBytes`
- `loadDataBytes`
- `loadDataWords`
- `clearInstructionMemory`
- `clearDataMemory`
- `setInitialPC` if nonzero reset PC becomes supported
- `snapshotState`
- `lastDataMemoryAccess`
- `lastDataMemoryWrites`

Memory wiring:

- Instruction memory address comes from core PC/IMEM address.
- Instruction memory read enable is always high.
- Instruction memory write enable is always low.
- Instruction memory size is word.
- Data memory uses the core load/store interface.
- Data memory clock is the system clock.

Tests:

- Reuse the extracted shared RV32I program-case catalog and hard-coded outcome validator through the structural lockstep test base.
- Start with short arithmetic programs.
- Add load/store, branch, jump, and loop programs after individual components pass.
- Before declaring the structural system complete, pass all 16 numbered cases, including every intentional trap case.
- Mirror `RV32IReferenceSystemContractTest` for reset, enable/hold, public PC/status pins, Harvard separation, halt, ignored clocks after halt/trap, and reset recovery.

### Phase 6: Visualizer Integration

Standalone block and core/system visualization are complete.

Work items:

- Done: add separate structural and behavioral aliases for control flow, decode, register file, ALU, and execution status. Each scenario owns one device under test; equivalence tests are intentionally excluded from the browser selector.
- Done: add scenario aliases for the structural smoke, contract, and all 16 structural RV32I programs.
- Done: add layout defaults for `RV32ISingleCycleSystem` and `RV32ISingleCycleCore`.
- Keep the top-level system readable: core, instruction memory, data memory.
- Let users expand the core to inspect control flow, decode/control, register file, ALU, execution status, muxes, and direct memory-interface wiring.

The structural visualizer should not hide the datapath behind one large behavioral core box. The point of this branch is to expose the actual single-cycle path.

## Testing Ladder

Each phase should land with focused tests before wiring the next layer.

Landed block equivalence tests:

- `RV32IControlFlowUnitEquivalenceTest`
- `RV32IDecodeControlUnitTest`
- `RV32IRegisterFileEquivalenceTest`
- `ALU32EquivalenceTest`
- `RV32IExecutionControlStatusUnitEquivalenceTest`

Landed integration tests:

- the shared 16-case catalog through `rv32iSystemProgramCase`
- the shared hard-coded final validator through `verifyRV32ISystemExpectedResult`
- direct memory-interface coverage for all RV32I load/store widths in the 16 program suite
- `RV32ISingleCycleCoreSmokeTest`
- `RV32ISingleCycleCoreTest`
- `RV32ISingleCycleSystemProgram1Test` through `RV32ISingleCycleSystemProgram16Test`

Program-level tests should continue to compare against `RV32IInstructionOracle`.

They must also apply the shared hard-coded final-result validator. Per-instruction oracle lockstep catches the first divergence; the independent final outcome prevents the program suite from relying only on shared oracle behavior.

The answer sheet and the structural device under test must execute separately. No production structural component may invoke the oracle or behavioral core to obtain the state being compared.

The structural system may need a larger `cycle_time_step` than the behavioral system because the visible datapath has real component delays. The lockstep harness should not assume one behavioral event is enough for one instruction.

## Acceptance Criteria For The First Complete Version

The first structural RV32I milestone is complete when:

- All new component-level tests pass.
- All five major block families have structural and behavioral fidelities, with
  explicit equivalence tests passing.
- The existing CTest suite still passes.
- The structural core commits instructions without invoking answer-sheet execution logic.
- The structural system passes the shared public system-contract test.
- The structural single-cycle system passes all 16 numbered program cases in per-instruction lockstep and against their hard-coded final outcomes.
- Those programs cover arithmetic/immediates, register-register ALU, loads/stores, all branches, jumps, loops, halt, illegal/ECALL traps, alignment traps, and access faults.
- Actual structural memory-bus writes match expected byte lanes, including zero-valued writes, and faulting stores produce no write transaction.
- The visualizer can load at least one structural RV32I scenario and show the core as real subcomponents.

## Risks And Watchpoints

`ALU32` and `RegisterFile32x32` are structurally large. The single-cycle system
may be much slower than a profile that selects behavioral fidelity, so early
tests should be short and timing should be conservative. Fast profiles make
isolated testing possible, but passing one is not evidence that the structural
assembly works.

The existing behavioral core's memory interface is not the model we want for the structural core. In particular, the separate `IMEM_CLK` and `DMEM_CLK` outputs should not be copied.

The full structural decoder may be visually large. Keep repeated instruction-recognition logic hierarchical and inspectable, and use the separate behavioral decoder for fast reference runs. Do not place the behavioral decoder inside the structural decoder, because that would make the structural shell misleading.

A lockstep test becomes tautological if the structural side obtains its result from the oracle or behavioral core. Keep answer-sheet execution in test/reference code and add a dependency check or source-level guard if this boundary becomes easy to violate.

Instruction count is not naturally a public hardware output. The lockstep test adapter should count committed enabled cycles, or the core should expose an internal debug commit signal used only by tests and visualization.

Nonzero reset PC is not required for the first structural milestone if all initial program cases run from address zero. If program cases need nonzero bases, add a reset-vector mechanism deliberately instead of patching tests around it.

External Spike/Sail/architectural-suite differential testing remains desirable answer-sheet validation. Treat it as a parallel hardening task, not a reason to couple structural execution to the behavioral implementation or to postpone the first visible components.
