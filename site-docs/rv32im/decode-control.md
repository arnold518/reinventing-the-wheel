# RV32IMDecodeControlUnit

Additive decode contract for RV32I plus all eight M instructions.

Catalog: `rv32im.decode-control`. The implemented form is structural; selectable lower-level children may be compact. No new whole-component behavioral form is registered.

## Pins

| Pin | Direction | Bits |
| --- | --- | --- |
| `INSTRUCTION` | Input | 32 |
| `ALU_A_SEL` | Output | 2 |
| `ALU_B_SEL` | Output | 2 |
| `ALU_OP` | Output | 5 |
| `BRANCH_TYPE` | Output | 3 |
| `DECODE_TRAP_CAUSE` | Output | 4 |
| `HALT_REQUEST` | Output | 1 |
| `IMM` | Output | 32 |
| `JUMP_TYPE` | Output | 2 |
| `LEGAL` | Output | 1 |
| `LOAD_SIGN_EXTEND` | Output | 1 |
| `MEM_READ` | Output | 1 |
| `MEM_SIZE` | Output | 2 |
| `MEM_WRITE` | Output | 1 |
| `RD_ADDR` | Output | 5 |
| `REG_WRITE` | Output | 1 |
| `RS1_ADDR` | Output | 5 |
| `RS2_ADDR` | Output | 5 |
| `TRAP_REQUEST` | Output | 1 |
| `USES_RS1` | Output | 1 |
| `USES_RS2` | Output | 1 |
| `WRITEBACK_SEL` | Output | 2 |

## Behavior

M instructions use opcode 0x33 and funct7 0x01, with funct3 selecting the operation. They are legal register-register instructions that write the ALU result and use both sources. Memory, branch, halt and trap controls stay clear. Register addresses retain their existing encoding.

## Internal structure

`INTEGER` retains the original RV32I decode family. `EXTENSION` groups M
recognition and control-field overrides, keeping register-field passthrough
inside the same boundary. Existing ALU operation numbers and packet fields
are unchanged. The recording covers all eight M encodings, integer fallback,
x0 destination and illegal/reserved encodings.

## Edge cases and integration

Reserved funct7 patterns remain illegal. The original RV32I decoder still rejects M. X/Z instructions follow the constituent four-state decoders, rather than being treated as an arbitrary valid M operation.

## Recorded example and checks

`RV32IMDecodeControlTest` owns the independent functional checks; `RV32IMDecodeControlScenarioTest` retains this smaller recorded example. Both use the same logical component owner.

<!-- simulator-examples: rv32im-decode-control RV32IMDecodeControlScenarioTest -->

<div class="simulator-embed"><iframe src="../../simulator/?scenario=rv32im-decode-control&amp;embed=1" title="RV32IMDecodeControl" loading="lazy" allowfullscreen></iframe></div>

<a href="../../simulator/?scenario=rv32im-decode-control" class="md-button">Open the recorded circuit</a>
