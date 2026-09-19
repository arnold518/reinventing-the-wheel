# § 4.2.4. Instruction Decoder

These components translate a 32-bit instruction into datapath controls. [4.1](../4.1/index.md) defines instruction semantics; this page specifies the actual circuit outputs, recognition rules, and implementation boundaries. Decoding does not read registers, access memory, or execute the instruction.

The [single-cycle Decode stage](../4.3/decode.md) places `RV32IDecodeControlUnit` at `CORE.DECODE.CONTROL` next to the register file. The stage adds operand values and the writeback port while forwarding all decoder outputs unchanged. `REG_WRITE` remains intent, distinct from the stage's qualified `REGISTER_WRITE` input.

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| `RV32IBitPatternMatcher` | Parameterized masked instruction recognition | Yes | No |
| `RV32IDecodeControlUnit` | Register fields, immediate, legality, and execution controls | Yes | Yes |

Columns describe registered implementations. C++ examples describe settled binary values, omitting gate delays and `X`/`Z`.

## 4.2.4.1. `RV32IBitPatternMatcher`

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `INPUT` | Input | 32 | Word to inspect |
| `MATCH` | Output | 1 | All selected bits equal the configured value |

`mask` and `value` are construction parameters, not pins. A one in `mask` selects a bit to compare. `value` is masked at construction; bits outside the mask are irrelevant. A zero mask is rejected.

### Behavior

The predicate is `(INPUT & mask) == (value & mask)`. For example, mask `0x0000707f` and value `0x00002003` recognize the opcode/funct3 of `LW`, regardless of its register and immediate fields.

#### C++ example

```cpp
#include <cstdint>
#include <stdexcept>
namespace decode_example {
bool match(uint32_t input, uint32_t mask, uint32_t value) {
    if (!mask) throw std::invalid_argument("mask must select a bit");
    return (input & mask) == (value & mask);
}
}
```

#### Unknown inputs and timing

Unmasked bits cannot affect `MATCH`. A known mismatch can mask an unknown selected bit. Selected zero-valued pattern bits use NOT gates; their `X`/`Z` inputs become `X`. With exactly one selected bit whose expected value is one, the output is a direct wire and can retain `Z`. Multi-term matches pass through AND gates. Timing depends on the number of selected terms.

### Implementation

#### Structural implementation

`INPUT_SPLIT` exposes the bits, NOT gates invert selected zero-pattern terms, and an AND chain combines them. There is no registered behavioral implementation.

### Examples

<!-- simulator-examples: rv32i-pattern-matcher RV32IBitPatternMatcherTest -->

#### Embedded simulation

<div class="simulator-embed"><iframe src="../../../simulator/?scenario=rv32i-pattern-matcher&amp;embed=1" title="RV32IBitPatternMatcher" loading="lazy" allowfullscreen></iframe></div>

<a href="../../../simulator/?scenario=rv32i-pattern-matcher" class="md-button">Open pattern matcher</a>

### Tests

`RV32IBitPatternMatcherTest` covers several mask/value configurations, ignored bits, selected `X`/`Z`, mismatch masking, and rejection of a zero mask. The table/embedded run show the selected scenario; the registered native test runs every parameterized scenario.

## 4.2.4.2. `RV32IDecodeControlUnit`

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `INSTRUCTION` | Input | 32 | Complete instruction word |
| `RS1_ADDR`, `RS2_ADDR`, `RD_ADDR` | Output | 5 each | Raw bits 19:15, 24:20, 11:7 |
| `IMM` | Output | 32 | Constructed immediate; zero if unused or illegal |
| `ALU_OP` | Output | 5 | [ALU32](alu32.md) operation |
| `ALU_A_SEL` | Output | 2 | 0: RS1; 1: PC; 2: zero |
| `ALU_B_SEL` | Output | 2 | 0: RS2; 1: immediate; 2: zero |
| `LEGAL` | Output | 1 | Recognized supported instruction |
| `REG_WRITE` | Output | 1 | Register write intent, not final commit enable |
| `MEM_READ`, `MEM_WRITE` | Output | 1 each | Load/store intent |
| `WRITEBACK_SEL` | Output | 2 | 0: none; 1: ALU; 2: memory; 3: PC+4 |
| `MEM_SIZE` | Output | 2 | 0: byte; 1: halfword; 2: word/default |
| `LOAD_SIGN_EXTEND` | Output | 1 | Signed byte/halfword load |
| `USES_RS1`, `USES_RS2` | Output | 1 each | Whether raw source fields are actual dependencies |
| `BRANCH_TYPE` | Output | 3 | 0: none; 1: BEQ; 2: BNE; 3: BLT; 4: BGE; 5: BLTU; 6: BGEU |
| `JUMP_TYPE` | Output | 2 | 0: none; 1: JAL; 2: JALR |
| `HALT_REQUEST` | Output | 1 | Project EBREAK convention |
| `TRAP_REQUEST` | Output | 1 | Illegal instruction or ECALL |
| `DECODE_TRAP_CAUSE` | Output | 4 | 0: none; 1: illegal instruction; 2: environment call |

### Behavior

Register fields are always raw slices, even when unused or illegal. For example, `RS2_ADDR` in an immediate instruction contains immediate bits; `USES_RS2=0` says not to read it as a dependency. `REG_WRITE` remains high for instructions targeting x0: the register file suppresses that write.

#### Recognition and control groups

Numbers in the opcode/funct columns below are hexadecimal. All listed restrictions must match. Register and immediate fields are otherwise unrestricted.

| Group | Opcode / funct restriction | Recognized instructions | Main controls |
| --- | --- | --- | --- |
| Register ALU | `33`; funct7=`00`, funct3=`0,1,2,3,4,5,6,7`; additionally funct7=`20`, funct3=`0,5` | ADD, SLL, SLT, SLTU, XOR, SRL, OR, AND; SUB, SRA | RS1/RS2 → ALU; register write, ALU writeback |
| Immediate ALU | `13`; funct3=`0,2,3,4,6,7` | ADDI, SLTI, SLTIU, XORI, ORI, ANDI | RS1/I-immediate → ALU; register write, ALU writeback |
| Immediate shifts | `13`; funct3=`1`, funct7=`00`; or funct3=`5`, funct7=`00,20` | SLLI, SRLI, SRAI | Same as immediate ALU; ALU uses low five immediate bits |
| Loads | `03`; funct3=`0,1,2,4,5` | LB, LH, LW, LBU, LHU | RS1 + I-immediate; memory read and memory writeback |
| Stores | `23`; funct3=`0,1,2` | SB, SH, SW | RS1 + S-immediate; memory write; RS2 is store data |
| Branches | `63`; funct3=`0,1,4,5,6,7` | BEQ, BNE, BLT, BGE, BLTU, BGEU | RS1 − RS2; B-immediate and branch selector; no register write |
| PC-relative jump | `6f` | JAL | PC + J-immediate; jump=1; PC+4 writeback |
| Register-relative jump | `67`; funct3=`0` | JALR | RS1 + I-immediate; jump=2; PC+4 writeback |
| Upper immediate | `37` / `17` | LUI / AUIPC | Zero + PASS_B / PC + ADD; U-immediate; ALU writeback |
| Fence | `0f`; funct3=`0` | FENCE | Legal no-op in this simulator; I-immediate still exposed |
| Exact system word | `00000073` / `00100073` | ECALL / EBREAK | Trap cause 2 / halt request; I-immediate 0 / 1 |

These rules recognize 40 instruction forms. FENCE accepts all other fields under its opcode/funct3 mask; it does not implement ordering in a cache hierarchy. FENCE.I, CSR instructions, M-extension instructions, and other unmatched encodings are illegal here. EBREAK halts the simulator; this is not a privileged breakpoint exception implementation.

For an illegal word: `LEGAL=0`, `TRAP_REQUEST=1`, cause=1, `ALU_OP=12`, both ALU sources=2, `MEM_SIZE=2`; immediate and all other control outputs are zero. Raw register fields remain exposed. Control encodings not listed above are never emitted for binary instructions.

#### C++ example

The following reference includes every recognized group, immediate construction, and illegal fallback. Immediate values are 32-bit two's-complement bit patterns. SRAI exposes the complete I-immediate (including its function bits); only the low five bits reach the shift distance.

```cpp
#include <array>
namespace decode_example {
struct Decoded {
    uint32_t RS1_ADDR=0, RS2_ADDR=0, RD_ADDR=0, IMM=0;
    unsigned ALU_OP=12, ALU_A_SEL=2, ALU_B_SEL=2, WRITEBACK_SEL=0, MEM_SIZE=2;
    unsigned BRANCH_TYPE=0, JUMP_TYPE=0, DECODE_TRAP_CAUSE=1;
    bool LEGAL=false, REG_WRITE=false, MEM_READ=false, MEM_WRITE=false;
    bool LOAD_SIGN_EXTEND=false, USES_RS1=false, USES_RS2=false;
    bool HALT_REQUEST=false, TRAP_REQUEST=true;
};
uint32_t sext(uint32_t x, unsigned width) {
    const uint32_t sign = uint32_t{1} << (width-1);
    return (x ^ sign) - sign; // x has already been masked to width bits.
}
Decoded decode(uint32_t w) {
    Decoded d;
    d.RS1_ADDR=(w>>15)&31; d.RS2_ADDR=(w>>20)&31; d.RD_ADDR=(w>>7)&31;
    const auto invalid=d;
    const unsigned op=w&127, f3=(w>>12)&7, f7=w>>25;
    const uint32_t i=sext(w>>20,12);
    const uint32_t s=sext(((w>>25)<<5)|((w>>7)&31),12);
    const uint32_t b=sext(((w>>31)<<12)|(((w>>7)&1)<<11)|(((w>>25)&63)<<5)|(((w>>8)&15)<<1),13);
    const uint32_t u=w&0xfffff000U;
    const uint32_t j=sext(((w>>31)<<20)|(((w>>12)&255)<<12)|(((w>>20)&1)<<11)|(((w>>21)&1023)<<1),21);
    d.LEGAL=true; d.TRAP_REQUEST=false; d.DECODE_TRAP_CAUSE=0;
    const auto reg = [&](unsigned alu, unsigned a, unsigned src_b, uint32_t imm) {
        d.ALU_OP=alu; d.ALU_A_SEL=a; d.ALU_B_SEL=src_b; d.IMM=imm;
        d.REG_WRITE=true; d.WRITEBACK_SEL=1;
        d.USES_RS1=a==0; d.USES_RS2=src_b==0;
    };
    const std::array<unsigned,8> alu{0,5,8,9,4,6,3,2};
    if (op==0x33) {
        if (f7!=0 && !(f7==0x20 && (f3==0 || f3==5))) return invalid;
        reg(f7 ? (f3==0 ? 1 : 7) : alu[f3],0,0,0);
    } else if (op==0x13) {
        if (f3==1 && f7!=0) return invalid;
        if (f3==5 && f7!=0 && f7!=0x20) return invalid;
        reg(f3==5 && f7==0x20 ? 7 : alu[f3],0,1,i);
    } else if (op==0x03) {
        if (!(f3==0 || f3==1 || f3==2 || f3==4 || f3==5)) return invalid;
        reg(0,0,1,i); d.MEM_READ=true; d.WRITEBACK_SEL=2;
        d.MEM_SIZE=f3&3; d.LOAD_SIGN_EXTEND=f3<2;
    } else if (op==0x23) {
        if (f3>2) return invalid;
        d.ALU_OP=0; d.ALU_A_SEL=0; d.ALU_B_SEL=1; d.IMM=s;
        d.MEM_WRITE=true; d.MEM_SIZE=f3; d.USES_RS1=d.USES_RS2=true;
    } else if (op==0x63) {
        const std::array<unsigned,8> branch{1,2,0,0,3,4,5,6};
        if (!branch[f3]) return invalid;
        d.ALU_OP=1; d.ALU_A_SEL=d.ALU_B_SEL=0; d.IMM=b;
        d.BRANCH_TYPE=branch[f3]; d.USES_RS1=d.USES_RS2=true;
    } else if (op==0x6f) {
        reg(0,1,1,j); d.JUMP_TYPE=1; d.WRITEBACK_SEL=3;
    } else if (op==0x67 && f3==0) {
        reg(0,0,1,i); d.JUMP_TYPE=2; d.WRITEBACK_SEL=3;
    } else if (op==0x37) reg(11,2,1,u);
    else if (op==0x17) reg(0,1,1,u);
    else if (op==0x0f && f3==0) d.IMM=i;
    else if (w==0x00000073) { d.TRAP_REQUEST=true; d.DECODE_TRAP_CAUSE=2; }
    else if (w==0x00100073) { d.HALT_REQUEST=true; d.IMM=1; }
    else return invalid;
    return d;
}
}
```

#### Unknown inputs and timing

The structural decoder handles each predicate and rewired field separately: unknown unused bits need not destroy recognition, and known register bits can remain visible. `RV32IDecodeControlDirect` is deliberately conservative: any nonbinary instruction bit makes **all outputs `X`**. Its registered equivalence scope is binary, settled output values. No instruction is latched here; downstream state must wait for decoding to settle.

### Implementation

#### Structural implementation

The top level has five children, arranged by connectivity:

| Child | Responsibility |
| --- | --- |
| `FIELDS_AND_IMMEDIATES` | Rewire the register addresses and I/S/B/U/J immediate forms |
| `RECOGNITION` | Shared opcode/funct3/funct7 predicates and instruction AND terms; exact ECALL/EBREAK matches |
| `CONTROL_SIGNALS` | Eighteen named control-output fields, each containing its own bit equations |
| `IMMEDIATE_MUX` | Select the immediate format specified by `IMMEDIATE_SELECT` |
| `CONST_ZERO32` | Zero for unused immediate selections |

`Rewire` is a **fixed bit-to-bit wiring map**, configured when the component is
constructed. Each mapped output bit copies one specified input bit. It can
extract fields, rearrange bits, join or split buses, and fan one input bit out
to several destinations (including sign extension). Unmapped output bits use
the configured `0`, `1`, or `X` default; this instance uses `0`. It has no stored
state, runtime selection input, arithmetic, or added simulation delay. Mapped
`X` and `Z` values are copied unchanged. A destination bit cannot have two
mapping sources.

Here `FIELDS_AND_IMMEDIATES` constructs every immediate form in parallel;
`IMMEDIATE_MUX`, not Rewire, selects the form needed by the decoded instruction.
The visualization shows one `INSTRUCTION[32]` spine with separate field taps.
Tap expressions list output bits from most significant to least significant:
`[h:l]` selects input bits, braces concatenate fields, and `n×` repeats a bit.

| Output | Example tap expression | Meaning |
| --- | --- | --- |
| `RS1[5]` | `[19:15]` | Extract the source-register field |
| `RD[5]` | `[11:7]` | Extract the destination-register field |
| `IMM_I[32]` | `{20×[31], [31:20]}` | Copy the 12-bit immediate and repeat its sign bit |
| `IMM_U[32]` | `{[31:12], 12×0}` | Copy the upper 20 bits and fill the low 12 bits with zero |

`RV32IInstructionRecognition` has an `INSTRUCTION[32]` input and forty one-hot match lines, bundled as `MATCH_LO[32]` and `MATCH_HI[8]` in the source pattern-table order. These are instruction indicators, not an encoded instruction number. `RV32IDecodeControlSignals` consumes those bundles and produces the public controls plus the internal `IMMEDIATE_SELECT[3]`. Its `RV32IDecodeControlField<name>` children each expose the same match inputs and an `OUT` bus of the corresponding control width.

These helpers are fixed structural implementations, not additional behavioral variants. Expanding them reveals the shared pattern matchers, recognition AND gates, and the original OR/NOT control equations. The OR chains keep their gate depth and safe illegal defaults. Field layouts keep each bit equation together in numeric order; no logic is replaced by a lookup-table black box.

#### Behavioral implementation

`RV32IDecodeControlDirect` uses `RV32IDecoder::decode` and `RV32IControl::fromDecoded`, converts their enums to the public pin encodings, and schedules outputs after one abstract delay tick. It is registered under `rv32i.decode-control` alongside the structural implementation.

### Examples

<!-- simulator-examples: rv32i-decode-control RV32IDecodeControlUnitTest -->

#### Embedded simulation

<div class="simulator-embed"><iframe src="../../../simulator/?scenario=rv32i-decode-control&amp;embed=1" title="RV32IDecodeControlUnit" loading="lazy" allowfullscreen></iframe></div>

<a href="../../../simulator/?scenario=rv32i-decode-control" class="md-button">Open instruction decoder</a>

### Tests

`RV32IDecodeControlUnitTest` checks circuit outputs in both fidelities; `RV32IDecoderTest` and `RV32IControlTest` exercise the semantic helpers. `RV32IBuildingBlocksDocumentationTest` compiles the reference above and compares its complete output set with the declared binary circuit rows and an additional encoding sweep. The structural source is `src/modules/rv32i/RV32IDecodeControlUnit.cpp`; recognition tests are in `src/tests/RV32IDecoderTests.cpp`.
