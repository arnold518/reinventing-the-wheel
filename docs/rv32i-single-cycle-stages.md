# Stage-wise non-pipelined RV32I core

Published component references: [§4.3 Single-Cycle Core](../site-docs/rv32i/4.3/index.md).

`RV32ISingleCycleCore` retains its public identity and both universal memory
ports. Its structural implementation is now six independently selectable
datapath stages plus `RV32IExecutionControlStatusUnit`. These are logical stages,
not pipeline registers or six execution cycles. Memory transactions can span
several clocks. Existing five-stage pipeline components are unchanged.

## Components and state ownership

| Component | Core child | Structural | Behavioral | State |
| --- | --- | --- | --- | --- |
| `RV32ISingleCycleFetchStage` | `FETCH` | Yes | Yes | PC and instruction-request progress/payload |
| `RV32ISingleCycleDecodeStage` | `DECODE` | Yes | Yes | The sole architectural register file |
| `RV32ISingleCycleExecuteStage` | `EXECUTE` | Yes | Yes | None |
| `RV32ISingleCycleMemoryStage` | `MEMORY` | Yes | Yes | Data-request progress/payload; no memory contents |
| `RV32ISingleCycleWritebackStage` | `WRITEBACK` | Yes | Yes | None |
| `RV32ISingleCycleNextPCStage` | `NEXT_PC` | Yes | Yes | None |
| `RV32IExecutionControlStatusUnit` | `EXECUTION_STATUS` | Yes | Yes | Halt/trap status and cause |

The stage families are `rv32i.single-cycle.fetch`, `.decode`, `.execute`,
`.memory`, `.writeback`, and `.next-pc`. Structural implementations compose the
existing tested components. Compact implementations directly model each stage;
they do not instantiate hidden gates or call the whole-instruction oracle.
Equivalence is at settled known-binary checkpoints after reset, not transient
gate delays or arbitrary X/Z controls.

## Stage interfaces

### Fetch

Inputs: `CLK`, `RST`, `ISSUE`, `TAKE`, `PC_WRITE`, `NEXT_PC<32>`, and the
`IMEM_` universal memory response/ready pins. Outputs: `PC<32>`,
`PC_PLUS_4<32>`, `INSTRUCTION<32>`, `DONE`, `FAULT`, `PC_MISALIGNED`, and
the `IMEM_` request/response-ready pins.

Contains `Register32`, `Adder32`, an alignment checker, and
`RV32IMemoryRequest`. Reset clears PC and requester state. A rising edge with
`PC_WRITE` loads `NEXT_PC`; otherwise PC holds. Fetch captures the old PC into
the requester. Request acceptance is not completion. `DONE` marks a matching
response; the endpoint holds its instruction until `TAKE` consumes it. `ISSUE`
only permits a new request and never retracts an already pending request.

### Decode

Inputs: `INSTRUCTION<32>`, `CLK`, `RST`, `REGISTER_WRITE`, `WB_VALUE<32>`.
Outputs: the complete `RV32IDecodeControlUnit` interface plus
`RS1_VALUE<32>` and `RS2_VALUE<32>`. The existing 4.2 decoder reference defines
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

Inputs: `CLK`, `RST`, `ISSUE`, `TAKE`, `MEM_WRITE`, `SIGN_EXTEND`, `ADDR<32>`,
`WRITE_DATA<32>`, `SIZE<2>`, and `DMEM_` response/ready pins. Outputs:
`READ_DATA<32>`, `DONE`, `FAULT`, `MISALIGNED`, and `DMEM_` request/response-ready
pins. SIZE 0/1/2 means byte/halfword/word; 3 is invalid.

Contains `RV32IMemoryLanes`, the alignment checker, and the data requester.
It formats a word transaction, holds the accepted payload stable, waits for a
response, and extracts/sign-extends the selected load lanes. Misaligned or
invalid-size requests retain the existing deliberately invalid bus-address
encoding, so the endpoint rejects them rather than writing an aligned address.
The execution controller determines the architectural fault. Memory contents
and technology controllers remain outside the core. `READ_DATA`/`FAULT` are
only consumed when `DONE` is asserted; idle bus contents have no ISA meaning.

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

Execution status authorizes PC/register writes only for a successful normal
attempt. Halt/trap attempts consume responses without those writes. Its
`FETCH_REQUEST_ACTIVE` output permits new fetches only while enabled, out of
reset, and not halted/trapped. Reset discards requester state; already launched
device side effects follow the memory protocol and are not rolled back.

All operands, targets, addresses, and link values use pre-completion PC and
register values. Requester registers are internal protocol state, not temporary
architectural registers. Stores await acknowledgment rather than reading back
their written data. There are no interstage latches, forwarding paths, or
instruction execution phase registers.

For example, at PC=16 with x5=24, `jalr x5,0(x5)` computes target=24 and link=20
before either state write. The completion edge writes PC=24 and x5=20. During a
stalled `lw x1,0(x1)`, x1 continues to hold the address until the load completes.

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

### Validation results

Focused validation on 2026-09-19 (not the repository-wide suite):

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
