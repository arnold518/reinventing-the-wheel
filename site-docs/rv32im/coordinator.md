# RV32IMPipelineCoordinator

Pipeline movement control during multi-cycle arithmetic.

Catalog: `rv32im.pipeline.coordinator`. The implemented form is structural; selectable lower-level children may be compact. No new whole-component behavioral form is registered.

## Pins

| Pin | Direction | Bits |
| --- | --- | --- |
| `DMEM_READY` | Input | 1 |
| `ENABLE` | Input | 1 |
| `EXECUTION_WAIT` | Input | 1 |
| `EX_PRETERMINAL` | Input | 1 |
| `EX_REDIRECT` | Input | 1 |
| `HALTED` | Input | 1 |
| `IMEM_READY` | Input | 1 |
| `LOAD_USE_HAZARD` | Input | 1 |
| `MEMORY_FAULT` | Input | 1 |
| `MEMORY_TERMINAL` | Input | 1 |
| `MEM_LOAD` | Input | 1 |
| `MEM_STORE` | Input | 1 |
| `RETIRE_TERMINAL` | Input | 1 |
| `TERMINAL_PENDING` | Input | 1 |
| `TRAPPED` | Input | 1 |
| `WB_TERMINAL` | Input | 1 |
| `ACTIVE` | Output | 1 |
| `COMMIT_ENABLE` | Output | 1 |
| `DATA_PORT_STALL` | Output | 1 |
| `DATA_REQUEST_ISSUE` | Output | 1 |
| `DMEM_READ_ENABLE` | Output | 1 |
| `DMEM_WRITE_ENABLE` | Output | 1 |
| `EXECUTION_STALL` | Output | 1 |
| `EX_MEM_FLUSH` | Output | 1 |
| `EX_MEM_WRITE` | Output | 1 |
| `FETCH_PC_REDIRECT` | Output | 1 |
| `FETCH_PC_WRITE` | Output | 1 |
| `FETCH_RESPONSE_TAKE` | Output | 1 |
| `FETCH_VALID` | Output | 1 |
| `ID_EX_FLUSH` | Output | 1 |
| `ID_EX_WRITE` | Output | 1 |
| `IF_ID_FLUSH` | Output | 1 |
| `IF_ID_WRITE` | Output | 1 |
| `IMEM_READ_ENABLE` | Output | 1 |
| `LOAD_USE_STALL` | Output | 1 |
| `MEMORY_STALL` | Output | 1 |
| `MEM_WB_FLUSH` | Output | 1 |
| `MEM_WB_WRITE` | Output | 1 |
| `PIPELINE_FLUSH` | Output | 1 |
| `PIPELINE_STALL` | Output | 1 |

## Behavior

EXECUTION_WAIT adds EXECUTION_STALL. During that hold, packet writes/flushes, fetch advancement/response consumption and COMMIT_ENABLE are suppressed. Occupied older WB state cannot repeatedly retire or write a register. Older terminal/fault state overrides the arithmetic hold.

## Internal structure

`INTEGER` retains the original pipeline coordinator; `ARITHMETIC_HOLD`
groups the structural hold, older-terminal override and movement gates.
The implementation conservatively holds older packets too; an already
accepted memory operation can complete into its retained response.

## Edge cases and integration

This design does not independently drain WB during arithmetic waits. No packet is retired twice. Ordinary ENABLE, reset, memory waits and terminal priorities retain their existing coordinator inputs.

## Recorded example and checks

`RV32IMPipelineCoordinatorTest` owns the independent functional checks; `RV32IMPipelineCoordinatorScenarioTest` retains this smaller recorded example. Both use the same logical component owner.

<!-- simulator-examples: rv32im-coordinator RV32IMPipelineCoordinatorScenarioTest -->

<div class="simulator-embed"><iframe src="../../simulator/?scenario=rv32im-coordinator&amp;embed=1" title="RV32IMPipelineCoordinator" loading="lazy" allowfullscreen></iframe></div>

<a href="../../simulator/?scenario=rv32im-coordinator" class="md-button">Open the recorded circuit</a>
