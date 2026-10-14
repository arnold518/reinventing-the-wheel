# Stage-wise non-pipelined RV32I core

Published component references: [§4.4 Single-Cycle Core](../../site-docs/rv32i/4.4/index.md).

`RV32ISingleCycleCore` has six independently selectable datapath stages plus
`RV32IExecutionControlStatusUnit`. Its native FF memories read asynchronously
and capture stores with architectural writes on one edge. There are no pending
transactions or inter-stage registers. The pipeline retains transaction ports.

## Components and state ownership

| Component | Core child | Structural | Behavioral | State |
| --- | --- | --- | --- | --- |
| `RV32ISingleCycleFetchStage` | `FETCH` | Yes | Yes | PC only |
| `RV32ISingleCycleDecodeStage` | `DECODE` | Yes | Yes | The sole architectural register file |
| `RV32ISingleCycleExecuteStage` | `EXECUTE` | Yes | Yes | None |
| `RV32ISingleCycleMemoryStage` | `MEMORY` | Yes | Yes | None; no request state or memory contents |
| `RV32ISingleCycleWritebackStage` | `WRITEBACK` | Yes | Yes | None |
| `RV32ISingleCycleNextPCStage` | `NEXT_PC` | Yes | Yes | None |
| `RV32IExecutionControlStatusUnit` | `EXECUTION_STATUS` | Yes | Yes | Halt/trap status and cause |

The stage families are `rv32i.single-cycle.fetch`, `.decode`, `.execute`,
`.memory`, `.writeback`, and `.next-pc`. Structural implementations compose the
existing tested components. Compact implementations directly model each stage;
they do not instantiate hidden gates or call the whole-instruction oracle.
The documented cross-fidelity checks compare settled known-binary checkpoints
after reset, not transient gate delays or every X/Z control combination. The
compact Fetch PC and Decode register file also apply their lower-level
four-state rules when inputs are uncertain; see their published component pages.

## Stage interfaces

### Fetch

Inputs: CLK, RST, ISSUE, PC_WRITE, NEXT_PC, IMEM_READ_DATA, IMEM_FAULT.
Outputs: PC, PC_PLUS_4, INSTRUCTION, DONE, FAULT, PC_MISALIGNED,
IMEM_WORD_ADDR, IMEM_READ_EN. See the published page for widths.

`Register<32>` owns PC; `IncrementBy4` computes PC+4. Direct wiring exposes
PC/4 as the word index and forwards the memory word/fault. DONE is one; ISSUE enables the
read. There is no TAKE input, request capture, or handshake.

### Decode

Inputs: `INSTRUCTION<32>`, `CLK`, `RST`, `REGISTER_WRITE`, `WB_VALUE<32>`.
Outputs: the complete `RV32IDecodeControlUnit` interface plus
`RS1_VALUE<32>` and `RS2_VALUE<32>`. The existing 4.3 decoder reference defines
the control encodings.

Contains the decoder and `RegisterFile32x32`. Read addresses and the write
destination come from the current instruction. Reads are combinational; reset
and enabled rising-edge writes follow the register-file contract. There is no
write-data bypass. x0 remains zero. The `register-state-view` capability exposes
all registers, including behavioral history, independently of fidelity.

### Execute

Inputs: `PC<32>`, `RS1_VALUE<32>`, `RS2_VALUE<32>`, `IMM<32>`, `ALU_OP<5>`,
`ALU_A_SEL<2>`, `ALU_B_SEL<2>`, `BRANCH_TYPE<3>`, `JUMP_TYPE<2>`.
Outputs: `ALU_RESULT<32>`, `CONTROL_TARGET<32>`, `BRANCH_TAKEN`.

Operand selection and ALU encodings are the existing decoder contract. The main
ALU handles arithmetic, effective addresses, comparisons, LUI/AUIPC, and jumps.
For branches it compares operands while a separate adder calculates PC+IMM.
For JAL the target is the ALU result; for JALR it is that result with bit zero
cleared. There is no duplicated RS1+IMM adder. Jump selection 3 produces target
zero; branch selections 0 and 7 produce false. Comparison flags are meaningful
for SUB/SLT/SLTU, exactly as in ALU32; the decoder selects SUB for branches.

### Memory

Inputs: ISSUE, COMMIT, MEM_WRITE, SIGN_EXTEND, ADDR, WRITE_DATA, SIZE,
DMEM_READ_DATA, DMEM_FAULT. Outputs: READ_DATA, DONE, FAULT, MISALIGNED,
DMEM_WORD_ADDR, DMEM_WRITE_DATA, DMEM_WRITE_MASK, DMEM_READ_EN, DMEM_WE.

RV32IMemoryLanes formats data/masks. Alignment and invalid SIZE suppress WE;
COMMIT supplies final permission. READ_EN checks active loads and stores before
commit. DONE is one, FAULT forwards the native access fault, and MISALIGNED
separately reports bad alignment/size. No clock, reset, or storage exists here.

### Writeback

Inputs: `ALU_RESULT<32>`, `READ_DATA<32>`, `PC_PLUS_4<32>`, `WRITEBACK_SEL<2>`.
Output: `WB_VALUE<32>`. Selectors 0/1/2/3 choose zero/ALU/load/PC+4. This is a
combinational mux, not another register file. Decode owns the physical write.

### Next PC

Inputs: `PC_PLUS_4<32>`, `CONTROL_TARGET<32>`, `BRANCH_TAKEN`, `JUMP_TYPE<2>`.
Outputs: `NEXT_PC<32>`, `TARGET_MISALIGNED`. A taken branch or nonzero jump
selection chooses the target; otherwise PC+4 is selected. Target alignment is
checked only for a selected transfer. Fetch owns the physical PC register.

## Completion and no reading back

Execution status authorizes PC/register/store writes only for a successful
normal attempt. Halt/trap attempts set terminal status without ordinary writes.
FETCH_REQUEST_ACTIVE and MEMORY_REQUEST_ACTIVE act as direct read enables;
PC_WRITE also supplies Memory.COMMIT. Native FF reset is tied low. No request
state or in-flight operation needs draining on CPU reset.

All operands, targets, addresses, and link values use pre-completion PC and
register values. Stores capture with PC/register writes, without acknowledgment
or reading back their written data. There are no interstage latches, forwarding paths, or
instruction execution phase registers.

For example, at PC=16 with x5=24, `jalr x5,0(x5)` computes target=24 and link=20
before either state write. The completion edge writes PC=24 and x5=20. During a
`lw x1,0(x1)`, x1 holds the old address throughout settling, then captures the load at the edge.

## Verification and visualization

Each stage has a registered `RV32ISingleCycle<Stage>StageTest` scenario that
compares both fidelities. The integrated `RV32ISingleCycleStageIntegrationTest`
checks source/destination overlap in LW, ADDI, ADD, and JALR, exact store effects,
and unchanged PC/registers throughout non-completion clocks. It replaces each
stage individually, all six together, and the whole core with behavioral models;
it also exercises structural register-file wiring. Hierarchy assertions require
six stages plus status and exactly one main ALU plus one branch adder in Execute.

The complete core uses a stage-aware floorplan: Fetch, Decode, Execute, Memory,
and Writeback run left to right; Next PC and execution status sit below. Its six
stage interiors retain their existing connectivity-based layouts. The generic
automatic refresh preserves this core arrangement, as it does for the pipeline.
Saved
fidelity paths for moved decoder/register-file/ALU/requester components migrate
in both native and WASM clients. Removed control-flow arithmetic paths map to
their corresponding new computation; this is a best-effort migration, not a
claim that old and new internal hierarchies are identical.

### Historical validation results

The following results describe the earlier transaction-backed version on
2026-09-19, not current timing or hierarchy counts:

- 35 native CTest cases passed: all six stage contracts, the no-reading-back
  integration test, core/status checks, all 22 single-cycle programs, memory
  endpoint/register integration, recursive profiles, and fidelity-toggle audit.
- The fidelity audit covers all 20 selectable contracts and 1,294 instances in
  the canonical structural program tree. Stage contracts compare structural and
  behavioral outputs; mixed-core tests replace stages individually and together.
- Native/WASM topology, timestamps, checkpoints, state, and capability views
  agree for six stages × two fidelities, plus both complete CPU systems × two
  fidelities. Decode register history is checked at the initial and final times.
- Python bindings, native geometry, saved-profile migration in both clients,
  and saved-layout migration passed. Headless-browser checks passed for the new
  core and all six stages, and the existing pipeline views. Core, Execute, and
  Memory screenshots were also inspected.
- The strict MkDocs build passed; all 69 execution-status rows and the 22
  reference programs' checkpoints were regenerated from the published WASM.

Builds and CTest ran serially. The 35-case native run took 333 seconds with
952 MiB peak RSS; the full structural fidelity audit accounted for
251 seconds. The WASM build peaked below 403 MB RSS. These are build/test
resource measurements, not CPU throughput benchmarks or physical timing claims.
