# RV32IMExecuteStage

Forwarded I execution plus a multi-cycle M execution slot.

Catalog: `rv32im.pipeline.stage.execute`. The implemented form is structural; selectable lower-level children may be compact. No new whole-component behavioral form is registered.

## Pins

| Pin | Direction | Bits |
| --- | --- | --- |
| `CANCEL` | Input | 1 |
| `CLK` | Input | 1 |
| `ENABLE` | Input | 1 |
| `EX_MEM_RD` | Input | 5 |
| `EX_MEM_REG_WRITE` | Input | 1 |
| `EX_MEM_RESULT_READY` | Input | 1 |
| `EX_MEM_VALID` | Input | 1 |
| `EX_MEM_VALUE` | Input | 32 |
| `IN_CONTROL` | Input | 32 |
| `IN_FETCH_STATUS` | Input | 32 |
| `IN_IMMEDIATE` | Input | 32 |
| `IN_INSTRUCTION` | Input | 32 |
| `IN_PC` | Input | 32 |
| `IN_REGISTER_ADDRESSES` | Input | 32 |
| `IN_RS1_VALUE` | Input | 32 |
| `IN_RS2_VALUE` | Input | 32 |
| `IN_VALID` | Input | 1 |
| `MEM_WB_RD` | Input | 5 |
| `MEM_WB_REG_WRITE` | Input | 1 |
| `MEM_WB_VALID` | Input | 1 |
| `MEM_WB_VALUE` | Input | 32 |
| `RST` | Input | 1 |
| `TAKE` | Input | 1 |
| `BRANCH_TAKEN` | Output | 1 |
| `CONTROL_TAKEN` | Output | 1 |
| `CONTROL_TARGET` | Output | 32 |
| `EXECUTION_WAIT` | Output | 1 |
| `IS_BRANCH` | Output | 1 |
| `OUT_ALU_RESULT` | Output | 32 |
| `OUT_CONTROL` | Output | 32 |
| `OUT_EXECUTION_STATUS` | Output | 32 |
| `OUT_FETCH_STATUS` | Output | 32 |
| `OUT_INSTRUCTION` | Output | 32 |
| `OUT_NEXT_PC` | Output | 32 |
| `OUT_PC` | Output | 32 |
| `OUT_PC_PLUS_4` | Output | 32 |
| `OUT_REGISTER_ADDRESSES` | Output | 32 |
| `OUT_STORE_DATA` | Output | 32 |
| `OUT_VALID` | Output | 1 |
| `PRETERMINAL` | Output | 1 |
| `REDIRECT` | Output | 1 |
| `REDIRECT_PC` | Output | 32 |

## Behavior

For an eligible valid M packet, forwarded operands launch MulDivUnit<32> once both sources are available. A matching older EX/MEM producer with RESULT_READY=0 keeps the slot idle until the load value is ready; stale register-file values cannot be captured. Faulted/illegal terminal packets do not launch arithmetic. EXECUTION_WAIT holds until DONE; OUT_ALU_RESULT then carries the selected M result. TAKE accepts it, CANCEL discards it and ENABLE pauses arithmetic. The additional CLK/RST pins clock/reset the M state.

## Internal structure

`INTEGER` retains the original I execute stage. `M_OPERANDS` reuses the
shared forwarding assembly; `M_CONTROL` groups instruction recognition,
packet eligibility, pending-operand checks and launch/wait control. `MULDIV`
owns the iterative arithmetic. The named word mux `RESULT_SELECTION`
overrides only the M ALU result. These are structural subcomponents rather
than a flat collection of per-bit selection gates.

## Edge cases and integration

The coordinator must inhibit packet movement and commit while EXECUTION_WAIT is asserted. Invalid or canceled packets cannot start a new owned M operation. Original branch and fault fields stay on the existing I execution path.

## Recorded example and checks

`RV32IMExecuteStageTest` owns the independent functional checks; `RV32IMExecuteStageScenarioTest` retains this smaller recorded example. Both use the same logical component owner.
The recording covers multiply, signed divide, held completion, an unavailable
older load followed by forwarding, fault suppression and cancellation. The
[dedicated programs](programs.md) show the stage operating in a complete pipeline.

<!-- simulator-examples: rv32im-execute-stage RV32IMExecuteStageScenarioTest -->

<div class="simulator-embed"><iframe src="../../simulator/?scenario=rv32im-execute-stage&amp;embed=1" title="RV32IMExecuteStage" loading="lazy" allowfullscreen></iframe></div>

<a href="../../simulator/?scenario=rv32im-execute-stage" class="md-button">Open the recorded circuit</a>
