# § 4.3.3. RV32I ALU

`RV32IALU` is the arithmetic and logic unit of every RV32I and RV32IM core in this project: the single-cycle core, both pipelines and the RV32IM core. It performs the 13 operations that RV32I instructions need and reports the three comparisons that branches test. It is a fixed 32-bit circuit, because RV32I fixes the register width, and it always uses the [parallel-prefix adder](../../alu/2.1.md#214-parallelprefixadderw).

It differs from the Chapter 2 [`ALU<W>`](../../alu/2.6.md#263-aluw) in the ways RISC-V differs from a classic flag-based processor: there is no carry input, no flag outputs and no invalid-opcode output, and comparisons are output pins rather than flags. Multiplication and division are not part of it; the RV32IM execute stage places `MulDivUnit<32>` beside it and selects that unit's result for M instructions.

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| `RV32IALUControl` | Turns the five-bit operation code into selector and control bits | Yes | Yes |
| `RV32IALUAddSub` | 32-bit addition or subtraction, with SUB as the carry input | Yes | Yes |
| `RV32IALULogic` | AND, OR and XOR of two words, all at once | Yes | Yes |
| `RV32IALUShifter` | Logical left, logical right and arithmetic right shifts | Yes | Yes |
| `RV32IALUCompare` | Equality and signed/unsigned less-than from a subtraction | Yes | Yes |
| `RV32IALUResultSelect` | Picks the result word for the operation code | Yes | Yes |
| `RV32IALU` | The complete RV32I ALU | Yes | Yes |

The six parts are documented first, in the order the ALU uses them; § 4.3.3.7 assembles them. Execute builds the ALU with `b.add(circuit::families::rv32iALU(), "ALU")`, so a build profile chooses its fidelity; its contract is `rv32i.alu`, and the parts are `rv32i.alu.control`, `rv32i.alu.add-sub`, `rv32i.alu.logic`, `rv32i.alu.shifter`, `rv32i.alu.compare` and `rv32i.alu.result-select`. Each part is chosen by the build profile like any other family, so a structural `RV32IALU` may hold compact parts; each part's compact form follows its own gates bit for bit, including X and Z. Python creates the ALU with `create_rv32i_alu(name, fidelity)`. The operation codes are named in `rv32i::ALUOp` in `include/modules/rv32i/RV32IALU.hpp`.

## 4.3.3.1. `RV32IALUControl`

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `OP` | Input | 5 | Operation code |
| `LOCAL_OP` | Output | 3 | `OP[2:0]`, the index within a result group |
| `HIGH_HALF` | Output | 1 | `OP[3]`, selects the upper result group |
| `OUT_OF_RANGE` | Output | 1 | `OP[4]`, forces the result to zero |
| `SUB` | Output | 1 | Subtract, and enable the comparisons |
| `SHIFT_LEFT`, `SHIFT_ARITHMETIC` | Output | 1 each | Shifter direction and sign fill |

### Behavior

`SUB` is 1 for SUB (1), SLT (8) and SLTU (9). The shift controls are plain functions of the opcode bits, not shift enables: they can be high while another result is selected, which is harmless because the result selector ignores the shifter then. `OUT_OF_RANGE` detects codes 16–31; the result selector also maps 13–15 to zero.

#### C++ example

These binary references share the namespace `rv32i_alu_example`. They omit propagation delays and X/Z values.

```cpp
#include <array>
#include <cstdint>
namespace rv32i_alu_example {
constexpr uint64_t MASK = 0xffffffffu, SIGN = 0x80000000u;
struct Control {
    unsigned LOCAL_OP;
    bool HIGH_HALF, OUT_OF_RANGE, SUB, SHIFT_LEFT, SHIFT_ARITHMETIC;
};
Control control(unsigned op) { // OP is a five-bit input.
    const bool arithmetic = !(op & 16) && !(op & 4) && !(op & 2);
    return {op & 7, bool(op & 8), bool(op & 16),
            arithmetic && bool(op & 9), !(op & 2), bool(op & 1)};
}
}
```

#### Unknown inputs

`LOCAL_OP`, `HIGH_HALF` and `OUT_OF_RANGE` are wiring and pass X and Z through unchanged. `SUB` and the shift controls follow their NOT/AND/OR gates.

### Implementation

#### Structural implementation

A splitter and a joiner expose the opcode bits. NOT gates on bits 1, 2 and 4 and two AND gates detect the arithmetic codes 0, 1, 8 and 9; an OR of bits 3 and 0 and a final AND give `SUB`.

#### Behavioral implementation

The compact form evaluates the same gates bit by bit with the gate rules (a low input forces AND low, a high input forces OR high, other X or Z inputs give X; wiring passes Z unchanged), so it gives the same outputs as the structural circuit for every four-state input, and it runs on delays proven on that circuit ([delay model](../../basic/1.2.md)). `CompactFormEquivalenceTest` runs both forms on binary, dense four-state and single-unknown vectors.

### Examples

<!-- simulator-examples: rv32i-alu-control RV32IALUControlTest -->

All 32 binary opcodes are recorded, each checking every output.

#### Embedded simulation

<div class="simulator-embed"><iframe src="../../../simulator/?scenario=rv32i-alu-control&amp;embed=1" title="RV32IALUControl and all binary opcodes" loading="lazy" allowfullscreen></iframe></div>

<a href="../../../simulator/?scenario=rv32i-alu-control" class="md-button">Open RV32IALUControl in the simulator</a>

### Tests

`RV32IALUControlTest` checks every binary opcode and every output. Source: [`RV32IALU.cpp`](https://github.com/arnold518/reinventing-the-wheel/blob/alpha/src/modules/rv32i/RV32IALU.cpp).

## 4.3.3.2. `RV32IALUAddSub`

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `A`, `B` | Input | 32 each | Operands |
| `SUB` | Input | 1 | 0: addition; 1: subtraction |
| `OUT` | Output | 32 | Wrapped result |
| `C_OUT` | Output | 1 | Addition carry; for subtraction, 1 means no borrow |
| `OVERFLOW` | Output | 1 | Signed overflow |

### Behavior

The adder computes `A + (B XOR {32{SUB}}) + SUB`. Unlike the Chapter 2 [`AddSub<W>`](../../alu/2.1.md#215-addsubw), `SUB` is also the carry input, so subtraction needs no separate carry pin. Subtraction sets `C_OUT` when unsigned `A >= B`. Overflow is independent of the unsigned carry.

#### C++ example

```cpp
namespace rv32i_alu_example {
struct Arithmetic { uint32_t OUT; bool C_OUT, OVERFLOW; };
Arithmetic addsub(uint32_t a, uint32_t b, bool sub) {
    const uint64_t wide = uint64_t(a) + (sub ? (uint64_t(b) ^ MASK) : b) + sub;
    const uint32_t out = uint32_t(wide & MASK);
    const bool overflow = ((sub ? (a ^ b) : ~(a ^ b)) & (a ^ out) & SIGN) != 0;
    return {out, bool(wide >> 32), overflow};
}
}
```

#### Unknown inputs

Each bit follows the adder's XOR/AND/OR logic, so a known controlling value can mask an unknown one; the whole word is not automatically discarded.

### Implementation

#### Structural implementation

The word gate `B_XOR_SUB` (`XORGate<32>`, one XOR per bit) inverts B when `SUB` is high; the wiring part `SUB_WORD` spreads `SUB` across the word ([word gates](../../basic/1.3.md#137-andgatew)). `SUB` also drives the carry input of one [`ParallelPrefixAdder<32>`](../../alu/2.1.md#214-parallelprefixadderw) named `PREFIX_ADDER`. Overflow is `C_SIGN XOR C_OUT`: the carry into the sign bit against the carry out of it.

The carry crosses 32 bits in five prefix levels instead of 32 ripple stages:

<!-- timing-rows
Prefix adder, operand to carry out | word.parallel-prefix-adder.32.behavioral | A → C_OUT
Ripple adder, operand to carry out | word.ripple-carry-adder.32.behavioral | A → C_OUT
-->

#### Behavioral implementation

The compact form evaluates the same gates bit by bit with the gate rules (a low input forces AND low, a high input forces OR high, other X or Z inputs give X; wiring passes Z unchanged), so it gives the same outputs as the structural circuit for every four-state input, and it runs on delays proven on that circuit ([delay model](../../basic/1.2.md)). `CompactFormEquivalenceTest` runs both forms on binary, dense four-state and single-unknown vectors.

### Examples

<!-- simulator-examples: rv32i-alu-add-sub RV32IALUAddSubTest -->

The recorded cases cover addition and subtraction at the carry, borrow and signed-overflow boundaries, and full-length carry chains.

#### Embedded simulation

<div class="simulator-embed"><iframe src="../../../simulator/?scenario=rv32i-alu-add-sub&amp;embed=1" title="RV32IALUAddSub" loading="lazy" allowfullscreen></iframe></div>

<a href="../../../simulator/?scenario=rv32i-alu-add-sub" class="md-button">Open RV32IALUAddSub in the simulator</a>

### Tests

`RV32IALUAddSubTest` checks arithmetic boundaries and all three outputs; `RV32IALUSliceTest` checks a representative four-bit add/subtract slice exhaustively. Source: [`RV32IALUAddSub.cpp`](https://github.com/arnold518/reinventing-the-wheel/blob/alpha/src/modules/rv32i/RV32IALUAddSub.cpp).

## 4.3.3.3. `RV32IALULogic`

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `A`, `B` | Input | 32 each | Operands |
| `AND_OUT`, `OR_OUT`, `XOR_OUT` | Output | 32 each | The three bitwise results |

### Behavior

All three outputs are always active; there is no opcode. The result selector picks one of them.

#### C++ example

```cpp
namespace rv32i_alu_example {
struct Logic { uint32_t AND_OUT, OR_OUT, XOR_OUT; };
Logic logic(uint32_t a, uint32_t b) { return {a & b, a | b, a ^ b}; }
}
```

#### Unknown inputs

AND with zero gives zero and OR with one gives one; other cases with X or Z give X, including XOR. Each bit is independent.

### Implementation

#### Structural implementation

Three word gates, `AND` (`ANDGate<32>`), `OR` (`ORGate<32>`) and `XOR` (`XORGate<32>`), take the whole operands and drive the three outputs. Each is 32 one-bit gates, grouped as a word, so the three paths are independent per bit.

#### Behavioral implementation

The compact form evaluates the same gates bit by bit with the gate rules (a low input forces AND low, a high input forces OR high, other X or Z inputs give X; wiring passes Z unchanged), so it gives the same outputs as the structural circuit for every four-state input, and it runs on delays proven on that circuit ([delay model](../../basic/1.2.md)). `CompactFormEquivalenceTest` runs both forms on binary, dense four-state and single-unknown vectors.

### Examples

<!-- simulator-examples: rv32i-alu-logic RV32IALULogicTest -->

The recorded cases include complementary patterns and four-state inputs.

#### Embedded simulation

<div class="simulator-embed"><iframe src="../../../simulator/?scenario=rv32i-alu-logic&amp;embed=1" title="RV32IALULogic" loading="lazy" allowfullscreen></iframe></div>

<a href="../../../simulator/?scenario=rv32i-alu-logic" class="md-button">Open RV32IALULogic in the simulator</a>

### Tests

`RV32IALULogicTest` checks all three outputs, including four-state cases. Source: [`RV32IALULogic.cpp`](https://github.com/arnold518/reinventing-the-wheel/blob/alpha/src/modules/rv32i/RV32IALULogic.cpp).

## 4.3.3.4. `RV32IALUShifter`

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `A` | Input | 32 | Word to shift |
| `B` | Input | 32 | Shift amount; only `B[4:0]` is used |
| `LEFT` | Input | 1 | 1: left; 0: right |
| `ARITHMETIC` | Input | 1 | Sign fill for right shifts; ignored for left shifts |
| `OUT` | Output | 32 | Shifted word |

### Behavior

The distance is 0–31. Left shifts and logical right shifts fill with zero; arithmetic right shifts fill with the original sign bit. Distance zero keeps the word. There is no rotation and no shifted-out carry, unlike the Chapter 2 [`BarrelShifter<W>`](../../alu/2.3.md).

#### C++ example

```cpp
namespace rv32i_alu_example {
uint32_t shift(uint32_t a, uint32_t b, bool left, bool arithmetic) {
    const unsigned n = b & 31;
    if (left) return uint32_t((uint64_t(a) << n) & MASK);
    uint32_t out = a >> n;
    if (n && arithmetic && (a & SIGN)) out |= uint32_t(MASK << (32 - n));
    return out;
}
}
```

#### Unknown inputs

`B[31:5]` is unused even when unknown. The distance bits drive gate-built muxes, so an unknown distance bit does not necessarily keep equal data. Left shifting disables sign fill.

### Implementation

#### Structural implementation

One right-shift network selects distances 1, 2, 4, 8 and 16 in five stages, `SHIFT_S0` … `SHIFT_S4`. Each stage is one 2:1 word mux (`Mux<2,32>`, one 2:1 mux per bit) choosing between the word and the word moved down by its distance, with the fill bit entering at the top; the moved word is wiring only (a splitter and the joiner `SHIFT_Sk_MOVED`). The word muxes `INPUT_REVERSE` and `OUTPUT_REVERSE`, selected by `LEFT`, reverse the bit order at the input and output, so the same network performs left shifts. The fill bit is `A[31] AND ARITHMETIC AND NOT LEFT`.

#### Behavioral implementation

The compact form evaluates the same gates bit by bit with the gate rules (a low input forces AND low, a high input forces OR high, other X or Z inputs give X; wiring passes Z unchanged), so it gives the same outputs as the structural circuit for every four-state input, and it runs on delays proven on that circuit ([delay model](../../basic/1.2.md)). `CompactFormEquivalenceTest` runs both forms on binary, dense four-state and single-unknown vectors.

### Examples

<!-- simulator-examples: rv32i-alu-shifter RV32IALUShifterTest -->

The recorded cases cover both directions, sign fill, every interesting distance and ignored upper amount bits.

#### Embedded simulation

<div class="simulator-embed"><iframe src="../../../simulator/?scenario=rv32i-alu-shifter&amp;embed=1" title="RV32IALUShifter" loading="lazy" allowfullscreen></iframe></div>

<a href="../../../simulator/?scenario=rv32i-alu-shifter" class="md-button">Open RV32IALUShifter in the simulator</a>

### Tests

`RV32IALUShifterTest` checks directions, sign fill, distances, ignored upper amount bits and four-state inputs. Source: [`RV32IALUShifter.cpp`](https://github.com/arnold518/reinventing-the-wheel/blob/alpha/src/modules/rv32i/RV32IALUShifter.cpp).

## 4.3.3.5. `RV32IALUCompare`

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `DIFF` | Input | 32 | `A − B` from the add/subtract part |
| `CARRY_IN`, `OVERFLOW_IN` | Input | 1 each | The subtraction's no-borrow and signed overflow |
| `ENABLE` | Input | 1 | Enables the comparisons |
| `EQ`, `LT_SIGNED`, `LT_UNSIGNED` | Output | 1 each | Comparison results |
| `SLT_RESULT`, `SLTU_RESULT` | Output | 32 each | The less-than results as words: bit 0 holds the result, bits 31:1 are zero |

### Behavior

The part reads an existing subtraction instead of comparing two operands itself. Signed less-than corrects the sign of the difference with the overflow bit. When `ENABLE` is low, every comparison is zero, so `EQ` is not an always-valid equality test; branches select SUB to obtain all three.

#### C++ example

```cpp
namespace rv32i_alu_example {
struct Compare { bool EQ, LT_SIGNED, LT_UNSIGNED; uint32_t SLT_RESULT, SLTU_RESULT; };
Compare compare(uint32_t diff, bool carry, bool overflow, bool enable) {
    const bool s = enable && (bool(diff & SIGN) != overflow);
    const bool u = enable && !carry;
    return {enable && diff == 0, s, u, uint32_t(s), uint32_t(u)};
}
}
```

#### Unknown inputs

A low `ENABLE` masks unknown subtraction inputs. When enabled, the outputs follow four-state zero detection, XOR and inversion. The upper bits of the result words stay 0.

### Implementation

#### Structural implementation

A Chapter 2 [`ZeroDetect<32>`](../../alu/2.4.md#241-zerodetectw) named `ARITHMETIC_ZERO` gives equality. A sign splitter, XOR, NOT and AND gates give the less-than results, and joiners widen them into words.

#### Behavioral implementation

The compact form evaluates the same gates bit by bit with the gate rules (a low input forces AND low, a high input forces OR high, other X or Z inputs give X; wiring passes Z unchanged), so it gives the same outputs as the structural circuit for every four-state input, and it runs on delays proven on that circuit ([delay model](../../basic/1.2.md)). `CompactFormEquivalenceTest` runs both forms on binary, dense four-state and single-unknown vectors.

### Examples

<!-- simulator-examples: rv32i-alu-compare RV32IALUCompareTest -->

The recorded cases combine every enable, carry and overflow value with zero, one and sign-boundary differences, then add four-state inputs with the comparisons disabled.

#### Embedded simulation

<div class="simulator-embed"><iframe src="../../../simulator/?scenario=rv32i-alu-compare&amp;embed=1" title="RV32IALUCompare" loading="lazy" allowfullscreen></iframe></div>

<a href="../../../simulator/?scenario=rv32i-alu-compare" class="md-button">Open RV32IALUCompare in the simulator</a>

### Tests

`RV32IALUCompareTest` checks enable masking, equality, the sign/overflow correction and the result words. Source: [`RV32IALUCompare.cpp`](https://github.com/arnold518/reinventing-the-wheel/blob/alpha/src/modules/rv32i/RV32IALUCompare.cpp).

## 4.3.3.6. `RV32IALUResultSelect`

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `ARITHMETIC_RESULT`, `AND_RESULT`, `OR_RESULT`, `XOR_RESULT`, `SHIFT_RESULT` | Input | 32 each | Part results |
| `SLT_RESULT`, `SLTU_RESULT`, `A`, `B` | Input | 32 each | Comparison words and pass-through operands |
| `LOCAL_OP` | Input | 3 | Index within a group |
| `HIGH_HALF` | Input | 1 | Selects the upper group |
| `OUT_OF_RANGE` | Input | 1 | Forces zero |
| `OUT` | Output | 32 | Selected word |

### Behavior

The lower group, by `LOCAL_OP`, is arithmetic, arithmetic, AND, OR, XOR, shift, shift, shift. The upper group is SLT, SLTU, A, B, and zero for indices 4–7. `OUT_OF_RANGE` overrides both groups with zero.

#### C++ example

```cpp
namespace rv32i_alu_example {
uint32_t select(uint32_t arithmetic, uint32_t and_result, uint32_t or_result,
                uint32_t xor_result, uint32_t shifted, uint32_t slt, uint32_t sltu,
                uint32_t a, uint32_t b, unsigned local, bool high, bool invalid) {
    const std::array<uint32_t,8> low{arithmetic,arithmetic,and_result,or_result,xor_result,shifted,shifted,shifted};
    const std::array<uint32_t,8> upper{slt,sltu,a,b,0,0,0,0};
    return invalid ? 0 : (high ? upper : low).at(local);
}
}
```

#### Unknown inputs

Known controls mask the unselected words; unknown controls follow the selector's AND/OR gates. The repeated arithmetic and shift choices keep separate gate terms, so an unknown selector bit gives the same outputs as the equivalent gate-built mux.

### Implementation

#### Structural implementation

Opcode minterms qualify nine result sources. Nine word gates `MASK_0` … `MASK_8` (`ANDGate<32>`) mask the nine source words with their selects, each spread across the word by wiring, and word ORs (`ORGate<32>`) combine them in a balanced tree (`RESULT_…`): per bit the same nine ANDs and OR tree as a bit-by-bit circuit. Reserved codes select no source and give zero. `RV32ILogicOptimizationTest` compares this circuit with a gate-built mux reference for all 1,024 four-state selector combinations, each with four data patterns.

#### Behavioral implementation

The compact form evaluates the same gates bit by bit with the gate rules (a low input forces AND low, a high input forces OR high, other X or Z inputs give X; wiring passes Z unchanged), so it gives the same outputs as the structural circuit for every four-state input, and it runs on delays proven on that circuit ([delay model](../../basic/1.2.md)). `CompactFormEquivalenceTest` runs both forms on binary, dense four-state and single-unknown vectors.

### Examples

<!-- simulator-examples: rv32i-alu-result-select RV32IALUResultSelectTest -->

The recorded cases use distinguishable source words with every binary opcode, then four-state source words.

#### Embedded simulation

<div class="simulator-embed"><iframe src="../../../simulator/?scenario=rv32i-alu-result-select&amp;embed=1" title="RV32IALUResultSelect" loading="lazy" allowfullscreen></iframe></div>

<a href="../../../simulator/?scenario=rv32i-alu-result-select" class="md-button">Open RV32IALUResultSelect in the simulator</a>

### Tests

`RV32IALUResultSelectTest` checks every local index with both groups and the zero override. Source: [`RV32IALUResultSelect.cpp`](https://github.com/arnold518/reinventing-the-wheel/blob/alpha/src/modules/rv32i/RV32IALUResultSelect.cpp).

## 4.3.3.7. `RV32IALU`

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `A`, `B` | Input | 32 each | Operands |
| `OP` | Input | 5 | Operation code |
| `OUT` | Output | 32 | Result |
| `EQ`, `LT_SIGNED`, `LT_UNSIGNED` | Output | 1 each | Comparisons, valid for SUB, SLT and SLTU; otherwise 0 |

### Behavior

| OP | Name | OUT | Comparisons |
| ---: | --- | --- | --- |
| 0 | ADD | `A + B` | 0 |
| 1 | SUB | `A − B` | Valid |
| 2 | AND | `A & B` | 0 |
| 3 | OR | `A \| B` | 0 |
| 4 | XOR | `A ^ B` | 0 |
| 5 | SLL | `A << B[4:0]` | 0 |
| 6 | SRL | Logical `A >> B[4:0]` | 0 |
| 7 | SRA | Arithmetic `A >> B[4:0]` | 0 |
| 8 | SLT | 1 if signed `A < B`, else 0 | Valid |
| 9 | SLTU | 1 if unsigned `A < B`, else 0 | Valid |
| 10 | PASS_A | `A` | 0 |
| 11 | PASS_B | `B` | 0 |
| 12 | CLEAR | 0 | 0 |
| 13–31 | Reserved | 0 | 0 |

The decoder selects SUB for conditional branches and reads all three comparisons; it selects ADD for addresses and jump targets, PASS_B for LUI and CLEAR for instructions that write no ALU result. Instruction legality is the decoder's job, so reserved codes simply give zero.

#### C++ example

This reference combines the part references above.

```cpp
namespace rv32i_alu_example {
struct Result { uint32_t OUT; bool EQ, LT_SIGNED, LT_UNSIGNED; };
Result alu(uint32_t a, uint32_t b, unsigned op) {
    const auto c = control(op);
    const auto ar = addsub(a, b, c.SUB);
    const auto l = logic(a, b);
    const auto sh = shift(a, b, c.SHIFT_LEFT, c.SHIFT_ARITHMETIC);
    const auto cmp = compare(ar.OUT, ar.C_OUT, ar.OVERFLOW, c.SUB);
    const auto out = select(ar.OUT, l.AND_OUT, l.OR_OUT, l.XOR_OUT, sh,
        cmp.SLT_RESULT, cmp.SLTU_RESULT, a, b, c.LOCAL_OP, c.HIGH_HALF, c.OUT_OF_RANGE);
    return {out, cmp.EQ, cmp.LT_SIGNED, cmp.LT_UNSIGNED};
}
}
```

#### Unknown inputs

The structural ALU can mask unknown values in parts whose result is not selected. The compact form instead makes **every output unknown if any bit of `A`, `B` or `OP` is X or Z**, even an unused operand. Its equivalence with the gates therefore covers known (0/1) inputs only.

### Implementation

#### Structural implementation

The root contains `CONTROL`, `ARITHMETIC` (the add/subtract part), `LOGIC`, `SHIFT`, `COMPARE` and `RESULT_SELECT`. The subtraction result, carry and overflow feed the compare part, and every part's result feeds the selector.

#### Behavioral implementation

The compact form computes the result and comparisons with the RISC-V value rule that the compact cores also use, then publishes them on delays proven on the gates, one set per operation code. The [instruction oracle](../4.2/verification/index.md) checks the same operations independently.

**Timing.** Paths of the complete ALU in each timing profile:

<!-- timing-rows
Operation to result | rv32i.alu.behavioral | OP → OUT
Operand to result | rv32i.alu.behavioral | A → OUT
Operand to equality | rv32i.alu.behavioral | A → EQ
-->

### Examples

<!-- simulator-examples: rv32i-alu RV32IALUTest -->

The recorded cases cover every operation at its boundaries and every reserved code.

#### Embedded simulation

<div class="simulator-embed"><iframe src="../../../simulator/?scenario=rv32i-alu&amp;embed=1" title="RV32IALU and its recorded cases" loading="lazy" allowfullscreen></iframe></div>

<a href="../../../simulator/?scenario=rv32i-alu" class="md-button">Open RV32IALU in the simulator</a>

### Tests

`RV32IALUTest` checks all outputs on the structural and compact forms. `RV32IALUSweepTest` adds an 8 × 8 boundary grid on every operation and 1,024 seeded random vectors over all 32 operation codes, against C++ integer results on both forms. `ALUHierarchyTest` checks the six named parts and the single shared prefix adder. `RV32IBuildingBlocksDocumentationTest` compiles the C++ examples on this page and replays them against the recorded binary cases. Sources: [`RV32IALU.hpp`](https://github.com/arnold518/reinventing-the-wheel/blob/alpha/include/modules/rv32i/RV32IALU.hpp), [`RV32IALU.cpp`](https://github.com/arnold518/reinventing-the-wheel/blob/alpha/src/modules/rv32i/RV32IALU.cpp).
