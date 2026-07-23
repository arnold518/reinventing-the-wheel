# RV32I Milestones 3 and 4 Report: Decode And Control

Last updated: 2026-07-19

## Status

Milestones 3 and 4 are implemented.

They are pure C++ rule libraries, not visual circuit components yet:

- Milestone 3: `RV32IDecoder`
- Milestone 4: `RV32IControl`

Together, they define the first trusted ISA-to-datapath contract:

```text
32-bit instruction word
  -> decoded RV32I meaning
  -> CPU control signals
```

Example:

```text
0xfff28293
  -> ADDI rd=x5, rs1=x5, imm=-1
  -> ALU A=rs1, ALU B=imm, ALU_OP=ADD, write rd from ALU
```

This is the semantic foundation for the later paired structural/behavioral decoder-control components, functional instruction oracle, and single-cycle CPU.

## Why This Is Not A Component Yet

The CPU will need a real decoder/control component with pins. That component should come later.

This milestone intentionally avoids `Component`, `IOComponent`, and `BasicComponent` inheritance because it is the rule layer:

- no pins
- no wires
- no simulator time
- no layout
- no visualizer state

This keeps the RISC-V bit rules easy to test before we debug datapath wiring.

Later layering should be:

```text
RV32IDecoder + RV32IControl libraries
  pure C++ rule source

RV32IDecodeControlUnit
  complete structural field/immediate/control component

RV32IDecodeControlUnit, behavioral fidelity
  same-contract reference added after structural slice tests

pairwise equivalence + independent expected rows
  prove both circuit implementations against the contract
```

The behavioral component stays separate; it must not be placed inside the structural decoder as a hidden bridge.

## References

Primary rule sources:

- RISC-V RV32I base integer ISA: `https://docs.riscv.org/reference/isa/unpriv/rv32.html`
- Official RISC-V opcode repository: `https://github.com/riscv/riscv-opcodes`

Project rule sources:

- `docs/rv32i-roadmap.md`
- `docs/rv32i-components-report.md`
- `docs/rv32i-milestone-1-report.md`
- `docs/memory-components-report.md`

Important project decisions:

- RV32I base only.
- No RV64 instructions.
- No compressed instructions.
- No CSR or privileged instructions.
- `FENCE` is accepted.
- `FENCE.I` is rejected as an unsupported extension.
- `ECALL` traps.
- `EBREAK` halts test programs.

## Implemented Files

Decode library:

- `include/rv32i/RV32IInstruction.hpp`
- `include/rv32i/RV32IDecoder.hpp`
- `src/rv32i/RV32IDecoder.cpp`

Control library:

- `include/rv32i/RV32IControl.hpp`
- `src/rv32i/RV32IControl.cpp`

Tests:

- `include/tests/RV32IDecoderTests.hpp`
- `src/tests/RV32IDecoderTests.cpp`
- `include/tests/RV32IControlTests.hpp`
- `src/tests/RV32IControlTests.cpp`

Build and registry:

- `CMakeLists.txt`
- `tests/CMakeLists.txt`
- `src/tests/TestRegistry.cpp`

## End-To-End Meaning

The decode/control path is:

```cpp
auto decoded = rv32i::RV32IDecoder::decode(raw_instruction);
auto control = rv32i::RV32IControl::fromDecoded(decoded);
```

Or:

```cpp
auto control = rv32i::RV32IControl::fromRaw(raw_instruction);
```

Easy mental model:

```text
Decoder:
  What instruction is this?
  Which registers and immediate does it name?

Control:
  Which datapath blocks should turn on?
  Which mux inputs should be selected?
  Where should the result be written?
```

## Milestone 3: Decode Library

### Purpose

Milestone 3 converts a raw 32-bit instruction into a stable decoded structure.

Example:

```text
0x00b50533
  opcode = 0x33
  rd     = 10
  rs1    = 10
  rs2    = 11
  funct3 = 0
  funct7 = 0
  result = ADD x10, x10, x11
```

### Public Decode API

```cpp
namespace rv32i {

class RV32IDecoder {
public:
    static RV32IDecodedInstruction decode(uint32_t raw);

    static uint8_t opcode(uint32_t raw);
    static uint8_t rd(uint32_t raw);
    static uint8_t funct3(uint32_t raw);
    static uint8_t rs1(uint32_t raw);
    static uint8_t rs2(uint32_t raw);
    static uint8_t funct7(uint32_t raw);
    static uint16_t imm12(uint32_t raw);
    static uint8_t shamt(uint32_t raw);

    static int32_t signExtend(uint32_t value, unsigned bit_count);
    static int32_t immediateI(uint32_t raw);
    static int32_t immediateS(uint32_t raw);
    static int32_t immediateB(uint32_t raw);
    static int32_t immediateU(uint32_t raw);
    static int32_t immediateJ(uint32_t raw);
};

}
```

### Decoded Instruction Structure

```cpp
struct RV32IDecodedInstruction {
    uint32_t raw;
    RV32IInstruction instruction;
    RV32IFormat format;
    RV32IDecodeStatus status;
    bool legal;

    uint8_t opcode;
    uint8_t rd;
    uint8_t funct3;
    uint8_t rs1;
    uint8_t rs2;
    uint8_t funct7;
    uint16_t imm12;
    uint8_t shamt;

    int32_t immediate;
};
```

Field meanings:

| Field | Meaning |
| --- | --- |
| `raw` | Original 32-bit instruction word. |
| `instruction` | Semantic enum such as `ADD`, `LW`, or `JAL`. |
| `format` | R, I, S, B, U, J, or invalid. |
| `status` | Decode success or rejection reason. |
| `legal` | True when this project accepts the instruction. |
| `opcode` | Bits `[6:0]`. |
| `rd` | Bits `[11:7]`. |
| `funct3` | Bits `[14:12]`. |
| `rs1` | Bits `[19:15]`. |
| `rs2` | Bits `[24:20]`. |
| `funct7` | Bits `[31:25]`. |
| `imm12` | Raw bits `[31:20]`. |
| `shamt` | Shift amount bits `[24:20]`. |
| `immediate` | Reconstructed signed or upper immediate. |

Important detail:

The decoder always extracts the raw fields. Some fields are not semantically used for some formats. For example, stores do not have a real `rd`; bits `[11:7]` are part of the store immediate.

### Instruction Enums

Supported RV32I instruction enum values:

```text
ADD, SUB, SLL, SLT, SLTU, XOR, SRL, SRA, OR, AND
ADDI, SLTI, SLTIU, XORI, ORI, ANDI, SLLI, SRLI, SRAI
LB, LH, LW, LBU, LHU
SB, SH, SW
BEQ, BNE, BLT, BGE, BLTU, BGEU
JAL, JALR
LUI, AUIPC
FENCE
ECALL, EBREAK
INVALID
```

### Instruction Formats

RV32I uses a few repeated bit layouts.

R-type:

```text
funct7 rs2 rs1 funct3 rd opcode
```

Used by register-register ALU instructions.

I-type:

```text
imm[11:0] rs1 funct3 rd opcode
```

Used by immediate ALU instructions, loads, `JALR`, `FENCE`, `ECALL`, and `EBREAK`.

S-type:

```text
imm[11:5] rs2 rs1 funct3 imm[4:0] opcode
```

Used by stores.

B-type:

```text
imm[12] imm[10:5] rs2 rs1 funct3 imm[4:1] imm[11] opcode
```

Used by branches.

U-type:

```text
imm[31:12] rd opcode
```

Used by `LUI` and `AUIPC`.

J-type:

```text
imm[20] imm[10:1] imm[11] imm[19:12] rd opcode
```

Used by `JAL`.

### Major Opcode Table

The decoder first rejects instructions whose low two bits are not `0b11`. Those are compressed-looking or otherwise invalid for this RV32I base decoder.

Then it dispatches by opcode:

| Opcode | Group |
| --- | --- |
| `0x37` | `LUI` |
| `0x17` | `AUIPC` |
| `0x6f` | `JAL` |
| `0x67` | `JALR` |
| `0x63` | branches |
| `0x03` | loads |
| `0x23` | stores |
| `0x13` | register-immediate ALU |
| `0x33` | register-register ALU |
| `0x0f` | `FENCE` |
| `0x73` | `ECALL` / `EBREAK` |

RV64 opcode groups such as `OP-IMM-32` and `OP-32` are rejected as unsupported extensions.

### R-Type Decode Rules

| `funct7` | `funct3` | Instruction |
| --- | --- | --- |
| `0x00` | `0x0` | `ADD` |
| `0x20` | `0x0` | `SUB` |
| `0x00` | `0x1` | `SLL` |
| `0x00` | `0x2` | `SLT` |
| `0x00` | `0x3` | `SLTU` |
| `0x00` | `0x4` | `XOR` |
| `0x00` | `0x5` | `SRL` |
| `0x20` | `0x5` | `SRA` |
| `0x00` | `0x6` | `OR` |
| `0x00` | `0x7` | `AND` |

Example:

```text
ADD x10, x10, x11
  rd=x10
  rs1=x10
  rs2=x11
```

### I-Type ALU Decode Rules

| `funct3` | Instruction |
| --- | --- |
| `0x0` | `ADDI` |
| `0x2` | `SLTI` |
| `0x3` | `SLTIU` |
| `0x4` | `XORI` |
| `0x6` | `ORI` |
| `0x7` | `ANDI` |

Shift-immediate instructions also use `funct7`:

| `funct7` | `funct3` | Instruction |
| --- | --- | --- |
| `0x00` | `0x1` | `SLLI` |
| `0x00` | `0x5` | `SRLI` |
| `0x20` | `0x5` | `SRAI` |

Example:

```text
ADDI x5, x5, -1
  rd=x5
  rs1=x5
  imm=-1
```

### Load Decode Rules

Loads use I-type immediates. The address calculation later is:

```text
address = rs1 + immediate
```

| `funct3` | Instruction | Load behavior |
| --- | --- | --- |
| `0x0` | `LB` | byte, sign-extend |
| `0x1` | `LH` | halfword, sign-extend |
| `0x2` | `LW` | word |
| `0x4` | `LBU` | byte, zero-extend |
| `0x5` | `LHU` | halfword, zero-extend |

Example:

```text
LW x4, 20(x3)
  rd=x4
  rs1=x3
  imm=20
```

### Store Decode Rules

Stores use S-type immediates. The store address later is:

```text
address = rs1 + immediate
```

| `funct3` | Instruction | Store behavior |
| --- | --- | --- |
| `0x0` | `SB` | store low byte |
| `0x1` | `SH` | store low halfword |
| `0x2` | `SW` | store word |

Example:

```text
SW x8, -20(x7)
  rs1=x7
  rs2=x8
  imm=-20
```

Stores do not write `rd`.

### Branch Decode Rules

Branches use B-type immediates. The immediate is a PC-relative offset.

| `funct3` | Instruction | Meaning |
| --- | --- | --- |
| `0x0` | `BEQ` | branch if equal |
| `0x1` | `BNE` | branch if not equal |
| `0x4` | `BLT` | signed less-than |
| `0x5` | `BGE` | signed greater/equal |
| `0x6` | `BLTU` | unsigned less-than |
| `0x7` | `BGEU` | unsigned greater/equal |

Example:

```text
BNE x1, x2, -4
  rs1=x1
  rs2=x2
  imm=-4
```

The decoder does not decide whether the branch is taken. That is the branch unit's job.

### Jump And Upper-Immediate Decode Rules

`JAL`:

```text
rd = link register destination
imm = PC-relative jump offset
```

`JALR`:

```text
rd = link register destination
rs1 = base register
imm = signed offset
```

`LUI`:

```text
rd = destination
imm = upper immediate shifted left by 12
```

`AUIPC`:

```text
rd = destination
imm = upper immediate shifted left by 12
```

Execution will later add `AUIPC`'s immediate to `PC`.

### Immediate Reconstruction

I-type:

```text
imm = sign_extend(inst[31:20], 12)
```

S-type:

```text
imm = sign_extend({inst[31:25], inst[11:7]}, 12)
```

B-type:

```text
imm = sign_extend({inst[31], inst[7], inst[30:25], inst[11:8], 0}, 13)
```

U-type:

```text
imm = inst[31:12] << 12
```

J-type:

```text
imm = sign_extend({inst[31], inst[19:12], inst[20], inst[30:21], 0}, 21)
```

Plain explanation:

- I and S immediates are ordinary signed 12-bit numbers.
- B and J immediates are split across the instruction word.
- B and J immediates always have a zero low bit in the encoded offset.
- U immediates occupy the upper 20 bits and leave the low 12 bits zero.

### SYSTEM And FENCE Decode Rules

Accepted:

| Raw instruction | Instruction |
| --- | --- |
| `0x00000073` | `ECALL` |
| `0x00100073` | `EBREAK` |

Rejected:

- CSR instructions
- privileged instructions
- unsupported SYSTEM encodings

`FENCE` is accepted when opcode is `0x0f` and `funct3=0`.

`FENCE.I` is rejected because it is not part of the first RV32I base target.

### Invalid Decode Statuses

| Status | Meaning |
| --- | --- |
| `InvalidInstructionLength` | Low bits are not `0b11`. |
| `UnknownOpcode` | Major opcode is not supported. |
| `UnsupportedFunct3` | Opcode is known, but `funct3` is invalid for this group. |
| `UnsupportedFunct7` | `funct3` is known, but `funct7` is invalid. |
| `UnsupportedSystem` | SYSTEM instruction is not `ECALL` or `EBREAK`. |
| `UnsupportedExtension` | Encoding belongs to an unsupported extension. |

Examples:

```text
0x00000000 -> InvalidInstructionLength
OP-IMM-32  -> UnsupportedExtension
FENCE.I    -> UnsupportedExtension
CSRRS      -> UnsupportedExtension
```

## Milestone 4: Control Library

### Purpose

Milestone 4 converts a decoded instruction into CPU control signals.

Example:

```text
LW x5, 8(x1)

Decode:
  instruction = LW
  rd = x5
  rs1 = x1
  imm = 8

Control:
  ALU A = RS1
  ALU B = immediate
  ALU OP = ADD
  memory read = true
  memory size = word
  register write = true
  writeback source = memory
```

### Public Control API

```cpp
namespace rv32i {

class RV32IControl {
public:
    static RV32IControlSignals fromDecoded(const RV32IDecodedInstruction& decoded);
    static RV32IControlSignals fromRaw(uint32_t raw);
};

}
```

### Control Signal Structure

```cpp
struct RV32IControlSignals {
    bool legal;

    uint8_t alu_op;
    RV32IALUSourceA alu_a;
    RV32IALUSourceB alu_b;

    bool reg_write;
    RV32IWritebackSource writeback;

    bool mem_read;
    bool mem_write;
    RV32IMemorySize mem_size;
    bool load_sign_extend;

    RV32IBranchType branch;
    RV32IJumpType jump;

    bool uses_rs1;
    bool uses_rs2;
    bool uses_rd;
    bool uses_immediate;

    bool halt;
    bool trap;
    RV32ITrapCause trap_cause;
};
```

### Control Signal Meaning

| Signal | Easy meaning |
| --- | --- |
| `legal` | The decoded instruction is accepted by the RV32I base target. |
| `alu_op` | Which `ALU32.OP` value to use. |
| `alu_a` | Where ALU input A comes from. |
| `alu_b` | Where ALU input B comes from. |
| `reg_write` | Whether to write the register file. |
| `writeback` | Which value goes to `rd`. |
| `mem_read` | Whether data memory is read. |
| `mem_write` | Whether data memory is written. |
| `mem_size` | Byte, halfword, word, or none. |
| `load_sign_extend` | Whether load data should be sign-extended. |
| `branch` | Which branch condition to evaluate. |
| `jump` | Whether this is `JAL` or `JALR`. |
| `uses_rs1` | The instruction reads `rs1`. |
| `uses_rs2` | The instruction reads `rs2`. |
| `uses_rd` | The instruction has a meaningful `rd`. |
| `uses_immediate` | The instruction uses the decoded immediate. |
| `halt` | Stop program execution. |
| `trap` | Enter trap/error path. |
| `trap_cause` | Why the trap happened. |

## Instruction Behavior Summary

### Register-Register ALU

Instructions:

```text
ADD SUB SLL SLT SLTU XOR SRL SRA OR AND
```

Control behavior:

```text
ALU A          = RS1
ALU B          = RS2
writeback      = ALU result
register write = yes
memory         = unused
branch/jump    = none
```

ALU operation mapping:

| Instruction | `ALU32.OP` |
| --- | --- |
| `ADD` | `ADD` |
| `SUB` | `SUB` |
| `SLL` | `SLL` |
| `SLT` | `SLT` |
| `SLTU` | `SLTU` |
| `XOR` | `XOR` |
| `SRL` | `SRL` |
| `SRA` | `SRA` |
| `OR` | `OR` |
| `AND` | `AND` |

### Register-Immediate ALU

Instructions:

```text
ADDI SLTI SLTIU XORI ORI ANDI SLLI SRLI SRAI
```

Control behavior:

```text
ALU A          = RS1
ALU B          = immediate
writeback      = ALU result
register write = yes
memory         = unused
branch/jump    = none
```

ALU operation mapping:

| Instruction | `ALU32.OP` |
| --- | --- |
| `ADDI` | `ADD` |
| `SLTI` | `SLT` |
| `SLTIU` | `SLTU` |
| `XORI` | `XOR` |
| `ORI` | `OR` |
| `ANDI` | `AND` |
| `SLLI` | `SLL` |
| `SRLI` | `SRL` |
| `SRAI` | `SRA` |

### Loads

Instructions:

```text
LB LH LW LBU LHU
```

Control behavior:

```text
ALU A          = RS1
ALU B          = immediate
ALU OP         = ADD
address        = RS1 + immediate
memory read    = yes
writeback      = memory data
register write = yes
```

Memory mapping:

| Instruction | Size | Sign extension |
| --- | --- | --- |
| `LB` | byte | yes |
| `LH` | halfword | yes |
| `LW` | word | no |
| `LBU` | byte | no |
| `LHU` | halfword | no |

### Stores

Instructions:

```text
SB SH SW
```

Control behavior:

```text
ALU A          = RS1
ALU B          = immediate
ALU OP         = ADD
address        = RS1 + immediate
memory write   = yes
store data     = RS2
register write = no
```

Memory mapping:

| Instruction | Size |
| --- | --- |
| `SB` | byte |
| `SH` | halfword |
| `SW` | word |

### Branches

Instructions:

```text
BEQ BNE BLT BGE BLTU BGEU
```

Control behavior:

```text
ALU A          = RS1
ALU B          = RS2
ALU OP         = SUB
branch type    = instruction-specific
register write = no
memory         = unused
```

Branch mapping:

| Instruction | Branch type |
| --- | --- |
| `BEQ` | equal |
| `BNE` | not equal |
| `BLT` | signed less-than |
| `BGE` | signed greater/equal |
| `BLTU` | unsigned less-than |
| `BGEU` | unsigned greater/equal |

Milestone 4 does not decide whether a branch is taken. It only says which branch condition to evaluate.

### Jumps

`JAL`:

```text
ALU A          = PC
ALU B          = immediate
ALU OP         = ADD
jump type      = JAL
writeback      = PC + 4
register write = yes
```

`JALR`:

```text
ALU A          = RS1
ALU B          = immediate
ALU OP         = ADD
jump type      = JALR
writeback      = PC + 4
register write = yes
```

The `JALR` low-bit clear happens later in the next-PC unit:

```text
target = (rs1 + imm) & ~1
```

### Upper Immediate

`LUI`:

```text
ALU A          = zero
ALU B          = immediate
ALU OP         = PASS_B
writeback      = ALU result
register write = yes
```

`AUIPC`:

```text
ALU A          = PC
ALU B          = immediate
ALU OP         = ADD
writeback      = ALU result
register write = yes
```

### FENCE

`FENCE` is accepted as a legal instruction.

First-runner behavior:

```text
ALU OP         = ZERO
register write = no
memory         = unused
branch/jump    = none
trap/halt      = no
```

In a simple single-cycle CPU with no cache or memory reordering, `FENCE` can behave like a no-op.

### ECALL And EBREAK

`ECALL`:

```text
trap       = true
trap cause = EnvironmentCall
halt       = false
```

`EBREAK`:

```text
halt = true
trap = false
```

This gives tests a simple program stop instruction.

### Illegal Instructions

Invalid decode results map to:

```text
legal      = false
trap       = true
trap cause = IllegalInstruction
ALU OP     = ZERO
```

This keeps the normal datapath quiet and lets the future CPU stop or report a trap.

## Example Walkthroughs

### `ADD x3, x1, x2`

```text
Decode:
  instruction = ADD
  rd = x3
  rs1 = x1
  rs2 = x2

Control:
  ALU A = RS1
  ALU B = RS2
  ALU OP = ADD
  register write = yes
  writeback = ALU
```

### `ADDI x5, x5, -1`

```text
Decode:
  instruction = ADDI
  rd = x5
  rs1 = x5
  immediate = -1

Control:
  ALU A = RS1
  ALU B = immediate
  ALU OP = ADD
  register write = yes
  writeback = ALU
```

### `LW x5, 8(x1)`

```text
Decode:
  instruction = LW
  rd = x5
  rs1 = x1
  immediate = 8

Control:
  ALU computes x1 + 8
  memory reads one word
  register writes memory data to x5
```

### `SW x5, 8(x1)`

```text
Decode:
  instruction = SW
  rs1 = x1
  rs2 = x5
  immediate = 8

Control:
  ALU computes x1 + 8
  memory writes one word from x5
  register write is disabled
```

### `BEQ x1, x2, label`

```text
Decode:
  instruction = BEQ
  rs1 = x1
  rs2 = x2
  immediate = branch offset

Control:
  branch type = BEQ
  compare x1 and x2 later
  writeback is disabled
```

### `JAL x1, label`

```text
Decode:
  instruction = JAL
  rd = x1
  immediate = jump offset

Control:
  jump type = JAL
  target helper = PC + immediate
  writeback = PC + 4
```

## Test Coverage

### `RV32IDecoderTest`

Coverage:

- raw field extraction
- sign-extension helper behavior
- I/S/B/U/J immediate reconstruction
- every implemented RV32I base instruction
- shift-immediate `shamt` extraction
- invalid instruction length
- unknown opcode
- RV64-only opcode groups
- bad `funct3` for branch/load/store/JALR
- bad `funct7` for R-type and shift-immediate instructions
- `FENCE.I` rejection
- CSR instruction rejection
- unsupported SYSTEM instruction rejection

### `RV32IControlTest`

Coverage:

- every R-type ALU instruction
- every I-type ALU instruction
- every shift-immediate instruction
- every load instruction
- every store instruction
- every branch instruction
- `JAL`
- `JALR`
- `LUI`
- `AUIPC`
- `FENCE`
- `ECALL`
- `EBREAK`
- invalid instructions

For each control case, the test checks:

- ALU operation
- ALU input sources
- register write enable
- writeback source
- memory read/write enables
- memory size
- load sign-extension flag
- branch type
- jump type
- operand-use flags
- halt/trap flags
- trap cause

The tests use local instruction encoders so expected instruction words are built from fields. That makes the tests easier to audit than unexplained hex literals.

## Verification

Commands run:

```bash
cmake --build build -j 8
ctest --test-dir build --output-on-failure -R "RV32IDecoderTest"
ctest --test-dir build --output-on-failure -R "RV32IControlTest|RV32IDecoderTest"
ctest --test-dir build --output-on-failure -R "RV32IControlTest|RV32IDecoderTest|ALU32StructuralContractTest|ALU32BehavioralContractTest|Memory64Kx32Test|RegisterFile32x32BehavioralContractTest"
ctest --test-dir build --output-on-failure
```

Results:

- Build passed.
- `RV32IDecoderTest` passed: 1/1 in 0.01 seconds.
- `RV32IDecoderTest|RV32IControlTest` passed: 2/2 in 0.04 seconds.
- Focused control/decoder/ALU/memory subset passed: 6/6 in 168.65 seconds.
- Full regression passed: 86/86 in 291.62 seconds.

## What Milestones 3 And 4 Do Not Do

They do not:

- execute instructions
- update registers
- access memory
- update PC
- decide whether branches are taken
- clear the low bit for `JALR`
- expose visual control pins
- create a visual component
- load programs

Those belong to later milestones.

## Next Step

Next recommended work:

1. Add program preload/readback helpers for `Memory64Kx32`.
2. Add raw binary fixtures.
3. Add a small functional RV32I instruction oracle as the program-level oracle.
4. Build PC, branch-decision, and next-PC components.
5. Wrap decode/control as a behavioral component with pins.
6. Wire the first `RV32ISingleCycleCore`.
