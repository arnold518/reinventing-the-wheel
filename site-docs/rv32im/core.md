# RV32IMFiveStageCore

New five-stage RV32IM family with the existing external memory protocol.

Catalog: `rv32im.core.educational-five-stage`. Structural logic remains available. A compact implementation is under validation:
it executes this core's own gates with characterized register state, and derives
its delays from that circuit. Register and retirement observers read the actual
modeled register bank and MEM/WB signals. The instruction oracle remains test
and batch-checking equipment.

## Pins

| Pin | Direction | Bits |
| --- | --- | --- |
| `CLK` | Input | 1 |
| `DMEM_REQ_READY` | Input | 1 |
| `DMEM_RSP_READ_DATA` | Input | 32 |
| `DMEM_RSP_STATUS` | Input | 3 |
| `DMEM_RSP_VALID` | Input | 1 |
| `ENABLE` | Input | 1 |
| `IMEM_REQ_READY` | Input | 1 |
| `IMEM_RSP_READ_DATA` | Input | 32 |
| `IMEM_RSP_STATUS` | Input | 3 |
| `IMEM_RSP_VALID` | Input | 1 |
| `RST` | Input | 1 |
| `BRANCH_MISS` | Output | 1 |
| `BRANCH_RESOLVED` | Output | 1 |
| `COMMIT_VALID` | Output | 1 |
| `DATA_PORT_STALL` | Output | 1 |
| `DMEM_REQ_ADDR` | Output | 32 |
| `DMEM_REQ_OP` | Output | 3 |
| `DMEM_REQ_VALID` | Output | 1 |
| `DMEM_REQ_WRITE_DATA` | Output | 32 |
| `DMEM_REQ_WRITE_MASK` | Output | 4 |
| `DMEM_RSP_READY` | Output | 1 |
| `EXECUTION_STALL` | Output | 1 |
| `EX_MEM_PC` | Output | 32 |
| `EX_MEM_VALID` | Output | 1 |
| `FETCH_PC` | Output | 32 |
| `HALTED` | Output | 1 |
| `ID_EX_PC` | Output | 32 |
| `ID_EX_VALID` | Output | 1 |
| `IF_ID_PC` | Output | 32 |
| `IF_ID_VALID` | Output | 1 |
| `IMEM_REQ_ADDR` | Output | 32 |
| `IMEM_REQ_OP` | Output | 3 |
| `IMEM_REQ_VALID` | Output | 1 |
| `IMEM_REQ_WRITE_DATA` | Output | 32 |
| `IMEM_REQ_WRITE_MASK` | Output | 4 |
| `IMEM_RSP_READY` | Output | 1 |
| `INSTRUCTION_ATTEMPT` | Output | 1 |
| `LOAD_USE_STALL` | Output | 1 |
| `MEMORY_STALL` | Output | 1 |
| `MEM_WB_PC` | Output | 32 |
| `MEM_WB_VALID` | Output | 1 |
| `PIPELINE_FLUSH` | Output | 1 |
| `PIPELINE_STALL` | Output | 1 |
| `PREDICTION_RECOVERY` | Output | 1 |
| `TRAPPED` | Output | 1 |
| `TRAP_CAUSE` | Output | 4 |

## Behavior

RV32I integer execution plus all eight M instructions, using the new decode, execute and coordinator families. EXECUTION_STALL identifies arithmetic waits. Other public pins retain the advanced I core interface; existing RV32I families remain available and reject M.

## Internal structure

The shared five-stage assembly blueprint retains original packet registers, forwarding, prediction, memory requester, WB and retirement observation. New M families are selected only by the IM core. RV32IPipelineStateView and RV32IRetirementView remain available.

The behavioral form runs a compiled list of this core's own gates with
characterized register state. Its delays are proven on that same structure;
the independent instruction oracle is used only for verification. This form
is characterized for known binary inputs after reset, with setup requirements
met and observation at instruction commits. It exposes current architectural
state and pending retirement. Historical register-file inspection requires
the expanded structure.

## Edge cases and integration

Older memory/WB terminals cancel M ownership. Backpressure retains memory/fetch ownership. Prediction parameters are prediction and prediction_entries, with the same accepted values and limits as the advanced I core. No privilege, interrupt, C, A or floating-point extensions are added.

## Standalone timing boundary

With binary external inputs, including ENABLE, changing one tick after falling
edges, RST=0, ideal 50% clock and rising-edge output sampling, the structural
safe-clock analyzer certifies minimum periods of 94 unit ticks, 3900 ps
(embedded) and 2716 ps (desktop). Functional unit checks use 100 ticks and
count setup violations. These bounds describe this standalone boundary;
actual memory/cache/device connections need their own analysis. They are
not host simulation throughput or physical silicon frequency estimates.

## Recorded example and checks

`RV32IMFiveStageCoreTest` owns the independent functional checks; `RV32IMFiveStageCoreScenarioTest` retains this smaller recorded example. Both use the same logical component owner.

<!-- simulator-examples: rv32im-core RV32IMFiveStageCoreScenarioTest -->

<div class="simulator-embed"><iframe src="../../simulator/?scenario=rv32im-core&amp;embed=1" title="RV32IMFiveStageCore" loading="lazy" allowfullscreen></iframe></div>

<a href="../../simulator/?scenario=rv32im-core" class="md-button">Open the recorded circuit</a>
