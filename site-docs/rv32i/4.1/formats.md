# § 4.1.2. Instruction Formats and Decoding

[Reference index](index.md) · [Instruction catalogue](instructions/index.md)

This page defines shared bit placement and immediate reconstruction. Each instruction entry supplies its own fixed field values and exact recognition mask.

## Fields

Bits are numbered from 31 (most significant) to 0 (least significant). `w[h:l]` denotes an inclusive bit range. A register field contains a **register number**, not its current value.

| Field | Bit positions | Role when present |
| --- | --- | --- |
| `opcode` | `6:0` | Instruction family |
| `rd` | `11:7` | Destination register number |
| `funct3` | `14:12` | Operation selection within a family |
| `rs1` | `19:15` | First source register number |
| `rs2` | `24:20` | Second source register number |
| `funct7` | `31:25` | Additional selection for register operations and immediate shifts |
| `shamt` | `24:20` | Immediate shift count, replacing the second register operand |

These positions are reused by immediate fields. A store has **no destination register** even though bits `11:7` can be numerically extracted. An immediate operation has no `rs2` dependency merely because bits `24:20` exist.

## Layouts

R/I/S/U are the main layouts; B/J are immediate-layout variants. Field widths in each row sum to 32.

| Format | Fields, from bit 31 toward bit 0 | Widths |
| --- | --- | --- |
| R | `funct7, rs2, rs1, funct3, rd, opcode` | `7, 5, 5, 3, 5, 7` |
| I | `imm[11:0], rs1, funct3, rd, opcode` | `12, 5, 3, 5, 7` |
| S | `imm[11:5], rs2, rs1, funct3, imm[4:0], opcode` | `7, 5, 5, 3, 5, 7` |
| B | `imm[12], imm[10:5], rs2, rs1, funct3, imm[4:1], imm[11], opcode` | `1, 6, 5, 5, 3, 4, 1, 7` |
| U | `imm[31:12], rd, opcode` | `20, 5, 7` |
| J | `imm[20], imm[10:1], imm[11], imm[19:12], rd, opcode` | `1, 10, 1, 8, 5, 7` |

FENCE and SYSTEM are classified as I format by this decoder, but their fields do not have ordinary I-format operand roles. See their [individual entries](instructions/system.md).

## Immediate reconstruction

In the formulas, `:` joins bit fields. `sext_n` sign-extends an `n`-bit pattern to a 32-bit word. Sign extension preserves a signed value: 12-bit `0xFFF` represents `-1` and becomes `0xFFFFFFFF`.

| Format | Decoded 32-bit value | Encodable offset/value |
| --- | --- | --- |
| R | No immediate | Not applicable |
| I | `I = sext_12(w[31:20])` | −2048…2047; shift-immediate instructions use `shamt` instead |
| S | `S = sext_12(w[31:25] : w[11:7])` | −2048…2047 bytes |
| B | `Boff = sext_13(w[31] : w[7] : w[30:25] : w[11:8] : 0)` | −4096…4094 bytes, even offsets |
| U | `U = w[31:12] : twelve_zero_bits` | Multiples of 4096; all 20-bit upper fields are available |
| J | `Joff = sext_21(w[31] : w[19:12] : w[20] : w[30:21] : 0)` | −1,048,576…1,048,574 bytes, even offsets |

A B/J immediate already includes its low zero bit after decoding: it is a **byte offset**, not an offset to multiply by two again. Encodable targets need not satisfy the current four-byte instruction alignment; taken misaligned transfers trap.

All non-shift I-format immediates are sign-extended, including ANDI/ORI/XORI/SLTIU. Immediate shifts instead use `shamt`; their upper seven bits are selectors. For SRAI with `shamt=31`, the raw I field is `0x41F` (1055), but the shift count is 31. U-format `imm` is already shifted left by 12, whereas assembly supplies only its upper 20-bit operand.

## Operation selection

First select a family using the seven-bit `opcode`. Then check that family's `funct3`, upper bits, or exact word as listed below. An opcode alone is usually **not** enough to recognize a legal instruction.

| Opcode (hex) | Family | Format | Additional check |
| --- | --- | --- | --- |
| `0x33` | Register arithmetic/logic | R | `funct3` and all seven `funct7` bits |
| `0x13` | Immediate arithmetic/logic | I | `funct3`; also upper seven bits for shifts |
| `0x03` | Loads | I | `funct3` selects width and extension |
| `0x23` | Stores | S | `funct3` selects width |
| `0x63` | Conditional branches | B | `funct3` selects comparison |
| `0x37` | LUI | U | None; remaining bits are operands |
| `0x17` | AUIPC | U | None; remaining bits are operands |
| `0x6F` | JAL | J | None; remaining bits are operands |
| `0x67` | JALR | I | Must have `funct3=000` |
| `0x0F` | FENCE | I | Must have `funct3=000`; other fields do not select a different base instruction |
| `0x73` | ECALL / EBREAK | I | Must match the complete word `0x00000073` / `0x00100073` |

For arithmetic/logic, `funct3` has the following meanings. Function fields are written in **binary**, while opcodes are written in hexadecimal.

<!-- rv32i-alu-selectors -->
| `funct3` | Register family (`opcode=0x33`) | Immediate family (`opcode=0x13`) |
| --- | --- | --- |
| `000` | ADD / SUB | ADDI |
| `001` | SLL | SLLI |
| `010` | SLT | SLTI |
| `011` | SLTU | SLTIU |
| `100` | XOR | XORI |
| `101` | SRL / SRA | SRLI / SRAI |
| `110` | OR | ORI |
| `111` | AND | ANDI |

- In the **register family**, `funct7` must be `0000000`, except SUB and SRA require `0100000`. Other upper patterns are rejected, not ignored.
- For **immediate shifts**, those same seven positions must be `0000000` for SLLI/SRLI or `0100000` for SRAI. The remaining five immediate bits contain the shift amount.
- For **other immediate arithmetic/logic**, all 12 upper bits hold the immediate. Do not impose a `funct7` check on, for example, ADDI or ANDI.

For memory and branches:

<!-- rv32i-memory-selectors -->
| `funct3` | Load (`0x03`) | Store (`0x23`) | Branch (`0x63`) |
| --- | --- | --- | --- |
| `000` | LB | SB | BEQ |
| `001` | LH | SH | BNE |
| `010` | LW | SW | Invalid |
| `011` | Invalid | Invalid | Invalid |
| `100` | LBU | Invalid | BLT |
| `101` | LHU | Invalid | BGE |
| `110` | Invalid | Invalid | BLTU |
| `111` | Invalid | Invalid | BGEU |

In these three families, the upper instruction bits carry operands/offsets, not a `funct7` selector. FENCE's ignored fields and the exact ECALL/EBREAK encodings are discussed in [instruction reference](instructions/index.md).

## Mask/match notation

Each entry gives `mask` and `match`. The instruction is recognized when `(w & mask) == match`: mask bits of 1 are checked, and bits of 0 are ignored for **operation selection**. Ignored selector bits can still carry operands.

For example, SUB checks its opcode, funct3, and all seven funct7 bits, but not its three register numbers:

```text
0x407302B3 & 0xFE00707F = 0x40000033
```

The named instruction is `sub x5, x6, x7`. Checking bit 30 alone is insufficient to distinguish valid ADD/SUB encodings: all required upper bits must match.

<details class="note" markdown="1">
<summary>Complete recognition-mask index</summary>

<!-- rv32i-encodings -->
| Instruction | Format | Mask | Match |
| --- | --- | --- | --- |
| `ADD` | R | `0xFE00707F` | `0x00000033` |
| `SUB` | R | `0xFE00707F` | `0x40000033` |
| `SLL` | R | `0xFE00707F` | `0x00001033` |
| `SLT` | R | `0xFE00707F` | `0x00002033` |
| `SLTU` | R | `0xFE00707F` | `0x00003033` |
| `XOR` | R | `0xFE00707F` | `0x00004033` |
| `SRL` | R | `0xFE00707F` | `0x00005033` |
| `SRA` | R | `0xFE00707F` | `0x40005033` |
| `OR` | R | `0xFE00707F` | `0x00006033` |
| `AND` | R | `0xFE00707F` | `0x00007033` |
| `ADDI` | I | `0x0000707F` | `0x00000013` |
| `SLTI` | I | `0x0000707F` | `0x00002013` |
| `SLTIU` | I | `0x0000707F` | `0x00003013` |
| `XORI` | I | `0x0000707F` | `0x00004013` |
| `ORI` | I | `0x0000707F` | `0x00006013` |
| `ANDI` | I | `0x0000707F` | `0x00007013` |
| `SLLI` | I | `0xFE00707F` | `0x00001013` |
| `SRLI` | I | `0xFE00707F` | `0x00005013` |
| `SRAI` | I | `0xFE00707F` | `0x40005013` |
| `LB` | I | `0x0000707F` | `0x00000003` |
| `LH` | I | `0x0000707F` | `0x00001003` |
| `LW` | I | `0x0000707F` | `0x00002003` |
| `LBU` | I | `0x0000707F` | `0x00004003` |
| `LHU` | I | `0x0000707F` | `0x00005003` |
| `SB` | S | `0x0000707F` | `0x00000023` |
| `SH` | S | `0x0000707F` | `0x00001023` |
| `SW` | S | `0x0000707F` | `0x00002023` |
| `BEQ` | B | `0x0000707F` | `0x00000063` |
| `BNE` | B | `0x0000707F` | `0x00001063` |
| `BLT` | B | `0x0000707F` | `0x00004063` |
| `BGE` | B | `0x0000707F` | `0x00005063` |
| `BLTU` | B | `0x0000707F` | `0x00006063` |
| `BGEU` | B | `0x0000707F` | `0x00007063` |
| `JAL` | J | `0x0000007F` | `0x0000006F` |
| `JALR` | I | `0x0000707F` | `0x00000067` |
| `LUI` | U | `0x0000007F` | `0x00000037` |
| `AUIPC` | U | `0x0000007F` | `0x00000017` |
| `FENCE` | I | `0x0000707F` | `0x0000000F` |
| `ECALL` | I | `0xFFFFFFFF` | `0x00000073` |
| `EBREAK` | I | `0xFFFFFFFF` | `0x00100073` |

</details>

## Rejection and diagnostics

`decode(w)` first rejects `w[1:0] != 11`. The test is **necessary, not sufficient**, to recognize a base 32-bit instruction: unsupported longer instruction prefixes also fail the opcode table. The decoder consumes a supplied 32-bit word; it is not a variable-length instruction-stream decoder.

| Decoder status | Current selection rule | Example word |
| --- | --- | --- |
| `InvalidInstructionLength` | Low two bits are not `11` | `0x00000000` |
| `UnknownOpcode` | Opcode is not dispatched | `0xFFFFFFFF` |
| `UnsupportedFunct3` | LOAD, STORE, BRANCH, or JALR opcode with an unsupported `funct3` | `0x00003003` (`LD`-shaped encoding) |
| `UnsupportedFunct7` | OP or shift-immediate selection with unsupported upper bits | `0x027302B3` (`MUL`-shaped encoding) |
| `UnsupportedSystem` | SYSTEM with `funct3=000`, but not exact ECALL/EBREAK | `0x30200073` (`MRET`) |
| `UnsupportedExtension` | SYSTEM with nonzero `funct3`; MISC-MEM with nonzero `funct3`; OP-IMM-32 or OP-32 opcode | `0x0000100F` (`FENCE.I`) |

All these failures yield `instruction=INVALID`, `format=Invalid`, and `legal=false`; raw fields remain available for diagnostics. At execution they become the same project `IllegalInstruction` trap. These diagnostic categories describe this decoder, not a general taxonomy of RISC-V extensions: for example, an M-extension word reports `UnsupportedFunct7`, not `UnsupportedExtension`.

Unlisted encodings are not silently treated as NOPs. Reserved-encoding policy is partly environment-dependent in RISC-V; trapping rejected words is our policy. Conversely, a valid arithmetic instruction with `rd=x0` is not illegal. Ordinary no-effect forms and base HINT encodings accepted by these masks retain their architectural no-effect behavior; this core does not implement their performance hints.

## Decoder output examples

This table records the library's **raw extraction fields**, not an instruction's actual operand list. For example, SW has no rd operand and LUI reads no source register. Use each instruction entry's Operands section to determine dependencies.

<!-- rv32i-decode-examples -->
| Word | Instruction | Format | rd field | rs1 field | rs2 field | Decoded immediate (signed decimal) | shamt field |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: |
| `0x407302B3` | `SUB` | R | 5 | 6 | 7 | 0 | 7 |
| `0xFFF30293` | `ADDI` | I | 5 | 6 | 31 | -1 | 31 |
| `0xFE732E23` | `SW` | S | 28 | 6 | 7 | -4 | 7 |
| `0xFE731CE3` | `BNE` | B | 25 | 6 | 7 | -8 | 7 |
| `0x123452B7` | `LUI` | U | 5 | 8 | 3 | 305418240 | 3 |
| `0xFF9FF2EF` | `JAL` | J | 5 | 31 | 25 | -8 | 25 |
| `0x41F35293` | `SRAI` | I | 5 | 6 | 31 | 1055 | 31 |

The raw `shamt` field is meaningful as a count only for immediate shifts. CircuitSim's disassembler currently prints the already shifted U-format value; that display is not a general round-trip assembler input.
