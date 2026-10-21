# RV32IMDecodeStage

RV32IM decode and register-read stage.

Catalog: `rv32im.pipeline.stage.decode`. The implemented form is structural; selectable lower-level children may be compact. No new whole-component behavioral form is registered.

## Pins

| Pin | Direction | Bits |
| --- | --- | --- |
| `CLK` | Input | 1 |
| `EX_CONTROL` | Input | 32 |
| `EX_REGISTER_ADDRESSES` | Input | 32 |
| `EX_VALID` | Input | 1 |
| `IN_FETCH_STATUS` | Input | 32 |
| `IN_INSTRUCTION` | Input | 32 |
| `IN_PC` | Input | 32 |
| `IN_VALID` | Input | 1 |
| `RST` | Input | 1 |
| `WB_RD` | Input | 5 |
| `WB_REGISTER_WRITE` | Input | 1 |
| `WB_REG_WRITE` | Input | 1 |
| `WB_VALID` | Input | 1 |
| `WB_VALUE` | Input | 32 |
| `LOAD_USE_HAZARD` | Output | 1 |
| `OUT_CONTROL` | Output | 32 |
| `OUT_FETCH_STATUS` | Output | 32 |
| `OUT_IMMEDIATE` | Output | 32 |
| `OUT_INSTRUCTION` | Output | 32 |
| `OUT_PC` | Output | 32 |
| `OUT_REGISTER_ADDRESSES` | Output | 32 |
| `OUT_RS1_VALUE` | Output | 32 |
| `OUT_RS2_VALUE` | Output | 32 |
| `OUT_VALID` | Output | 1 |
| `TERMINAL` | Output | 1 |

## Behavior

Uses the original pipeline input/output packet and writeback ports. It selects RV32IMDecodeControlUnit while retaining the original register file, immediate generator, WB bypass and hazard fields. RegisterStateView remains available.

## Internal structure

A shared private assembly blueprint is used by the I and IM stages; each chooses its own decoder family. Existing I child paths and pin schemas are retained.

## Edge cases and integration

WB writes, x0 and read bypass retain the existing decode-stage contract. No extra packet bit or renumbered instruction enum identifies M; the raw instruction is retained.

## Recorded example and checks

`RV32IMDecodeStageTest` owns the independent functional checks; `RV32IMDecodeStageScenarioTest` retains this smaller recorded example. Both use the same logical component owner.

<!-- simulator-examples: rv32im-decode-stage RV32IMDecodeStageScenarioTest -->

<div class="simulator-embed"><iframe src="../../simulator/?scenario=rv32im-decode-stage&amp;embed=1" title="RV32IMDecodeStage" loading="lazy" allowfullscreen></iframe></div>

<a href="../../simulator/?scenario=rv32im-decode-stage" class="md-button">Open the recorded circuit</a>
