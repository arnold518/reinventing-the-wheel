# § 4.3.3. ALU32

`ALU32` is the complete combinational arithmetic/logic reference. RV32I Execute uses its `ALU32Core` variant: the same arithmetic, logic, shifting, and comparison network without the result-flag outputs that Execute never reads. Its five-bit operation encoding is **not** the [ALU8 encoding](../../alu8/2.6.md). There is no carry input, multiplication, division, rotation, or invalid-operation output.

The [single-cycle Execute stage](../4.4/execute.md) has one instance at `CORE.EXECUTE.ALU`, shared by arithmetic, address calculation, branch comparison, and JAL/JALR target calculation. PC+4 and the concurrent branch-target addition use separate adders; Writeback supplies jump links.

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| `AddSub32` | Shared addition/subtraction with carry and overflow | Yes | Yes |
| `Logic32` | Parallel AND, OR, and XOR | Yes | Yes |
| `Shifter32` | Logical left/right and arithmetic right shifts | Yes | Yes |
| `ALU32ControlDecoder` | Opcode split and local controls | Yes | No |
| `ALU32CompareUnit` | Comparison flags and Boolean result words from subtraction | Yes | No |
| `ALU32ResultSelector` | Selects the operation result | Yes | No |
| `ALU32FlagUnit` | Result flags and arithmetic-flag qualification | Yes | No |
| `ALU32` | Complete unit with the RV32I ALU interface | Yes | Yes |
| `ALU32Core` | CPU datapath variant with result and branch-comparison outputs | Yes | Yes |

Columns describe registered simulator implementations, not the C++ examples. The examples describe settled binary behavior, omit delays and `X`/`Z`, and can be compiled together in page order. Arithmetic uses unsigned 32-bit words and wraps modulo $2^{32}$.

## 4.3.3.1. `AddSub32`

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `A`, `B` | Input | 32 each | Operands |
| `SUB` | Input | 1 | 0: addition; 1: subtraction |
| `OUT` | Output | 32 | Wrapped result |
| `CARRY_OUT` | Output | 1 | Addition carry; subtraction **no borrow** |
| `OVERFLOW` | Output | 1 | Signed overflow |

### Behavior

The shared adder computes `A + (B XOR {32{SUB}}) + SUB`. Subtraction therefore sets carry when unsigned `A >= B`. Overflow is independent of unsigned carry.

#### C++ example

```cpp
#include <array>
#include <cstdint>
namespace alu32_example {
struct Arithmetic { uint32_t OUT; bool CARRY_OUT, OVERFLOW; };
Arithmetic addsub(uint32_t a, uint32_t b, bool sub) {
    const uint64_t wide = uint64_t(a) + (sub ? ~b : b) + sub;
    const uint32_t out = uint32_t(wide);
    const bool overflow = ((sub ? (a ^ b) : ~(a ^ b)) & (a ^ out) & 0x80000000U) != 0;
    return {out, bool(wide >> 32), overflow};
}
}
```

#### Unknown inputs and timing

Each bit follows XOR/AND/OR full-adder logic. A known controlling zero or one can mask an unknown; the entire word is not automatically discarded. Structural carries propagate through a prefix network; the behavioral implementation emits settled values after one abstract delay tick.

### Implementation

#### Structural implementation

Each B bit passes through an XOR with `SUB`; `SUB` also supplies carry-in to the shared prefix adder. A joiner collects sums. XOR of the carry into and out of bit 31 produces overflow. The optional ripple reference instead uses 32 chained `FullAdder` stages.

The default structural implementation uses `ParallelPrefixAdder32`: HalfAdder bit generate/propagate signals, five grouped carry-prefix levels (distances 1, 2, 4, 8, 16), then carry-in application and sum XORs. `CarryCombine` contains only AND/OR gates. Overflow remains carry into bit 31 XOR carry out. `Adder32` uses the same prefix component, so all cores receive the arithmetic improvement. The original ripple structure remains constructible with `Component::create<AddSub32>("RIPPLE", false)` (and likewise `Adder32`) for comparison.

The default layout follows connectivity. Prefix levels are expandable groups rather than one flat gate diagram. `ArithmeticTimingOptimizationTest` compares both structures on binary and four-state inputs; the five-level carry network changes propagation time, not the settled arithmetic contract.

#### Behavioral implementation

`AddSub32Behavioral` evaluates that same full-adder recurrence with four-state bit values, without constructing its internal gates.

### Examples

<!-- simulator-examples: addsub32 AddSub32Test -->

#### Embedded simulation

<div class="simulator-embed"><iframe src="../../../simulator/?scenario=addsub32&amp;embed=1" title="AddSub32" loading="lazy" allowfullscreen></iframe></div>

<a href="../../../simulator/?scenario=addsub32" class="md-button">Open AddSub32</a>

### Tests

`AddSub32Test` checks arithmetic boundaries and flags in both implementations. The table contains every checkpoint of the displayed scenario, with its recorded time.

## 4.3.3.2. `Logic32`

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `A`, `B` | Input | 32 each | Operands |
| `AND_OUT`, `OR_OUT`, `XOR_OUT` | Output | 32 each | Three simultaneous bitwise results |

### Behavior

All three outputs are active together; there is no opcode or enable.

#### C++ example

```cpp
namespace alu32_example {
struct Logic { uint32_t AND_OUT, OR_OUT, XOR_OUT; };
Logic logic(uint32_t a, uint32_t b) { return {a & b, a | b, a ^ b}; }
}
```

#### Unknown inputs and timing

AND with zero produces zero; OR with one produces one. Other indeterminate cases produce `X`, including XOR with `X` or `Z`. Operations are independent per bit.

### Implementation

#### Structural implementation

Two splitters, 32 gates of each operation, and three joiners provide three parallel paths.

#### Behavioral implementation

`Logic32Behavioral` applies the same four-state gate rules per bit with one abstract delay tick.

### Examples

<!-- simulator-examples: logic32 Logic32Test -->

#### Embedded simulation

<div class="simulator-embed"><iframe src="../../../simulator/?scenario=logic32&amp;embed=1" title="Logic32" loading="lazy" allowfullscreen></iframe></div>

<a href="../../../simulator/?scenario=logic32" class="md-button">Open Logic32</a>

### Tests

`Logic32Test` checks all three outputs and selected four-state cases against both implementations.

## 4.3.3.3. `Shifter32`

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `A` | Input | 32 | Word being shifted |
| `B` | Input | 32 | Shift amount is **only** `B[4:0]` |
| `LEFT` | Input | 1 | 1: left; 0: right |
| `ARITHMETIC` | Input | 1 | Sign-fill for right shifts; ignored for left shifts |
| `OUT` | Output | 32 | Shifted word |

### Behavior

Shift distance is 0–31. Left shifts and logical right shifts fill with zero; arithmetic right shifts fill with the original sign bit. A distance of zero preserves the word. There is no shifted-out carry output.

#### C++ example

```cpp
namespace alu32_example {
uint32_t shift(uint32_t a, uint32_t b, bool left, bool arithmetic) {
    const unsigned n = b & 31;
    if (left) return a << n;
    uint32_t out = a >> n;
    if (n && arithmetic && (a & 0x80000000U)) out |= ~uint32_t{0} << (32 - n);
    return out;
}
}
```

#### Unknown inputs and timing

`B[31:5]` are unused, even when unknown. The remaining bits follow the gate-based mux network; an unknown selector does not necessarily preserve equal one-valued mux inputs. Left shifting disables sign fill. The structural path includes five selectable shift stages.

### Implementation

#### Structural implementation

A shared right-shift network selects distances 1, 2, 4, 8, and 16. Input/output reversal muxes reuse it for left shifts. Fill is `A[31] AND ARITHMETIC AND NOT LEFT`.

#### Behavioral implementation

`Shifter32Behavioral` reproduces the reversal, each mux stage, and fill logic using four-state values, then schedules the result with one abstract delay tick.

### Examples

<!-- simulator-examples: shifter32 Shifter32Test -->

#### Embedded simulation

<div class="simulator-embed"><iframe src="../../../simulator/?scenario=shifter32&amp;embed=1" title="Shifter32" loading="lazy" allowfullscreen></iframe></div>

<a href="../../../simulator/?scenario=shifter32" class="md-button">Open Shifter32</a>

### Tests

`Shifter32Test` exercises directions, sign fill, distances, ignored upper amount bits, and selected four-state inputs.

## 4.3.3.4. `ALU32ControlDecoder`

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `OP` | Input | 5 | Complete opcode |
| `LOCAL_OP` | Output | 3 | `OP[2:0]` |
| `HIGH_HALF`, `OUT_OF_RANGE` | Output | 1 each | `OP[3]`, `OP[4]` |
| `ARITHMETIC_ENABLE` | Output | 1 | Qualifies carry and overflow |
| `SUB` | Output | 1 | Requests subtraction and enables comparisons |
| `SHIFT_LEFT`, `SHIFT_ARITHMETIC` | Output | 1 each | Shifter controls |

### Behavior

Arithmetic flags are enabled for 0, 1, 8, 9; subtraction for 1, 8, 9. Shift controls are raw Boolean functions, **not** shift-operation enable flags. They can be high when another result source is selected. `OUT_OF_RANGE` only detects 16–31; selectors also map reserved 13–15 to zero.

#### C++ example

```cpp
namespace alu32_example {
struct Control {
    unsigned LOCAL_OP;
    bool HIGH_HALF, OUT_OF_RANGE, ARITHMETIC_ENABLE, SUB, SHIFT_LEFT, SHIFT_ARITHMETIC;
};
Control control(unsigned op) { // OP is a five-bit input.
    const bool arithmetic = !(op & 16) && !(op & 4) && !(op & 2);
    return {op & 7, bool(op & 8), bool(op & 16), arithmetic,
            arithmetic && bool(op & 9), !(op & 2), bool(op & 1)};
}
}
```

#### Unknown inputs and timing

Rewired selector bits preserve `X`/`Z`; logic-derived controls follow their gates. Outputs are combinational and need time to settle.

### Implementation

#### Structural implementation

A splitter and selector joiner expose opcode bits; NOT/AND/OR gates derive the enables. No behavioral implementation is registered for this helper.

### Examples

<!-- simulator-examples: alu32-control ALU32ControlDecoderTest -->

#### Embedded simulation

<div class="simulator-embed"><iframe src="../../../simulator/?scenario=alu32-control&amp;embed=1" title="ALU32ControlDecoder" loading="lazy" allowfullscreen></iframe></div>

<a href="../../../simulator/?scenario=alu32-control" class="md-button">Open ALU32ControlDecoder</a>

### Tests

`ALU32ControlDecoderTest` covers every binary opcode and every output control.

## 4.3.3.5. `ALU32CompareUnit`

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `DIFF` | Input | 32 | `A - B` from the shared subtractor |
| `CARRY_IN`, `OVERFLOW_IN` | Input | 1 each | Subtraction no-borrow and signed overflow |
| `ENABLE` | Input | 1 | Enable comparisons |
| `EQ`, `LT_SIGNED`, `LT_UNSIGNED` | Output | 1 each | Comparison flags |
| `SLT_RESULT`, `SLTU_RESULT` | Output | 32 each | Corresponding less-than flag in bit 0; bits 31:1 zero |

### Behavior

This helper consumes an existing subtraction; it is not an independent two-operand comparator. Signed comparison corrects the difference sign using overflow. All comparisons are zero when disabled. The 32-bit outputs are ALU result words, not 32-bit flags.

#### C++ example

```cpp
namespace alu32_example {
struct Compare { bool EQ, LT_SIGNED, LT_UNSIGNED; uint32_t SLT_RESULT, SLTU_RESULT; };
Compare compare(uint32_t diff, bool carry, bool overflow, bool enable) {
    const bool s = enable && (bool(diff >> 31) != overflow);
    const bool u = enable && !carry;
    return {enable && diff == 0, s, u, uint32_t(s), uint32_t(u)};
}
}
```

#### Unknown inputs and timing

Disabled AND outputs mask unknown arithmetic inputs. Enabled flags use four-state zero-detection, XOR, and inversion. Upper Boolean-result bits remain constant zero.

### Implementation

#### Structural implementation

`ZeroDetect32` forms equality; a sign splitter, XOR, NOT, and AND gates form less-than flags. Joiners extend those flags to words. No behavioral implementation is registered for this helper.

### Examples

<!-- simulator-examples: alu32-compare ALU32CompareUnitTest -->

#### Embedded simulation

<div class="simulator-embed"><iframe src="../../../simulator/?scenario=alu32-compare&amp;embed=1" title="ALU32CompareUnit" loading="lazy" allowfullscreen></iframe></div>

<a href="../../../simulator/?scenario=alu32-compare" class="md-button">Open ALU32CompareUnit</a>

### Tests

`ALU32CompareUnitTest` checks enable masking, zero, sign/overflow correction, and Boolean word outputs.

## 4.3.3.6. `ALU32ResultSelector`

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `ARITHMETIC_RESULT`, `AND_RESULT`, `OR_RESULT`, `XOR_RESULT`, `SHIFT_RESULT` | Input | 32 each | Operation-unit results |
| `SLT_RESULT`, `SLTU_RESULT`, `A`, `B` | Input | 32 each | Comparison results and pass-through operands |
| `LOCAL_OP` | Input | 3 | Index within a group |
| `HIGH_HALF` | Input | 1 | Choose high group |
| `OUT_OF_RANGE` | Input | 1 | Force zero |
| `OUT` | Output | 32 | Selected word |

### Behavior

Low-group sources are arithmetic, arithmetic, AND, OR, XOR, shift, shift, shift. High-group sources are SLT, SLTU, A, B, zero, zero, zero, zero. `OUT_OF_RANGE` overrides both groups with zero. Inputs are independently meaningful even when no decoder is attached.

#### C++ example

```cpp
namespace alu32_example {
uint32_t select(uint32_t arithmetic, uint32_t and_result, uint32_t or_result,
                uint32_t xor_result, uint32_t shifted, uint32_t slt, uint32_t sltu,
                uint32_t a, uint32_t b, unsigned local, bool high, bool invalid) {
    const std::array<uint32_t,8> low{arithmetic,arithmetic,and_result,or_result,xor_result,shifted,shifted,shifted};
    const std::array<uint32_t,8> upper{slt,sltu,a,b,0,0,0,0};
    return invalid ? 0 : (high ? upper : low).at(local);
}
}
```

#### Unknown inputs and timing

Known controls mask unselected paths; unknown controls follow the original mux gate equations. The decoded AND/OR network preserves those equations, including repeated arithmetic/shift choices with unknown selector bits. Outputs settle after gate propagation.

### Implementation

#### Structural implementation

Parallel opcode minterms qualify nine result sources. `BIT_0` through `BIT_31` are expandable `ALU32ResultBit` slices: each masks the nine source bits and combines them with a balanced OR tree. Invalid operations select no source and produce zero. Selection has no serial mux chain or stored state. No behavioral implementation is registered for this helper.

`RV32ILogicOptimizationTest` compares this circuit with the former mux tree across all 1,024 four-state selector combinations, each with four data patterns (4,096 rows).

### Examples

<!-- simulator-examples: alu32-result-selector ALU32ResultSelectorTest -->

#### Embedded simulation

<div class="simulator-embed"><iframe src="../../../simulator/?scenario=alu32-result-selector&amp;embed=1" title="ALU32ResultSelector" loading="lazy" allowfullscreen></iframe></div>

<a href="../../../simulator/?scenario=alu32-result-selector" class="md-button">Open ALU32ResultSelector</a>

### Tests

`ALU32ResultSelectorTest` checks all local indices and both group/override controls with distinguishable source words.

## 4.3.3.7. `ALU32FlagUnit`

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `RESULT` | Input | 32 | Selected final result |
| `ARITHMETIC_ENABLE` | Input | 1 | Enables arithmetic flags |
| `ARITH_CARRY`, `ARITH_OVERFLOW` | Input | 1 each | Shared adder flags |
| `ZERO`, `NEGATIVE`, `CARRY_OUT`, `OVERFLOW` | Output | 1 each | Result and qualified arithmetic flags |

### Behavior

Zero and negative always describe the final result. Carry and overflow are passed only when enabled. Comparison flags do not belong to this block.

#### C++ example

```cpp
namespace alu32_example {
struct Flags { bool ZERO, NEGATIVE, CARRY_OUT, OVERFLOW; };
Flags flags(uint32_t result, bool enable, bool carry, bool overflow) {
    return {result == 0, bool(result >> 31), enable && carry, enable && overflow};
}
}
```

#### Unknown inputs and timing

Negative directly exposes result bit 31, including `Z`; zero uses a gate reduction. Disabled carry/overflow are zero even if their arithmetic inputs are unknown.

### Implementation

#### Structural implementation

`ZeroDetect32`, a `SIGN_BIT` rewire, and two AND gates implement the block. The rewire has one 32-bit input (`RESULT`) and one 1-bit output (`SIGN`), mapping only `RESULT[31]` to `NEGATIVE`; it does not expose 31 unused outputs. This is zero-delay routing, including for `X` and `Z`, rather than a logic gate. No behavioral implementation is registered for this helper.

### Examples

<!-- simulator-examples: alu32-flags ALU32FlagUnitTest -->

#### Embedded simulation

<div class="simulator-embed"><iframe src="../../../simulator/?scenario=alu32-flags&amp;embed=1" title="ALU32FlagUnit" loading="lazy" allowfullscreen></iframe></div>

<a href="../../../simulator/?scenario=alu32-flags" class="md-button">Open ALU32FlagUnit</a>

### Tests

`ALU32FlagUnitTest` checks result classes, enable masking, carry/overflow combinations, and all four sign-bit values with unrelated lower bits set to `X` or `Z`. `ALUHierarchyTest` verifies the single-bit rewire's interface and exact bit mapping.

## 4.3.3.8. `ALU32`

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `A`, `B` | Input | 32 each | Operands |
| `OP` | Input | 5 | Operation |
| `OUT` | Output | 32 | Result |
| `ZERO`, `NEGATIVE` | Output | 1 each | Result zero/sign |
| `CARRY_OUT`, `OVERFLOW` | Output | 1 each | Qualified arithmetic flags |
| `EQ`, `LT_SIGNED`, `LT_UNSIGNED` | Output | 1 each | Qualified subtraction comparisons |

### Behavior

| OP | Operation | OUT | Carry/overflow | Comparison flags |
| ---: | --- | --- | --- | --- |
| 0 | ADD | `A + B` | Addition | Zero |
| 1 | SUB | `A - B` | Subtraction | Valid |
| 2 | AND | `A & B` | Zero | Zero |
| 3 | OR | `A \| B` | Zero | Zero |
| 4 | XOR | `A ^ B` | Zero | Zero |
| 5 | SLL | `A << B[4:0]` | Zero | Zero |
| 6 | SRL | Logical `A >> B[4:0]` | Zero | Zero |
| 7 | SRA | Arithmetic `A >> B[4:0]` | Zero | Zero |
| 8 | SLT | Signed `A < B`, extended to 32 bits | Subtraction | Valid |
| 9 | SLTU | Unsigned `A < B`, extended to 32 bits | Subtraction | Valid |
| 10 | PASS_A | A | Zero | Zero |
| 11 | PASS_B | B | Zero | Zero |
| 12 | ZERO | 0 | Zero | Zero |
| 13–31 | Reserved | 0 | Zero | Zero |

For every opcode, `ZERO = (OUT == 0)` and `NEGATIVE = OUT[31]`. SLT/SLTU retain subtraction carry/overflow even though their final result is only zero or one. `EQ` is not an always-on operand-equality indicator. Branch instructions select `SUB` to obtain all three comparisons. Reserved operations produce `ZERO=1`, all other flags zero; instruction legality is the decoder's responsibility.

#### C++ example

```cpp
namespace alu32_example {
struct Result {
    uint32_t OUT;
    bool ZERO, EQ, LT_SIGNED, LT_UNSIGNED, NEGATIVE, CARRY_OUT, OVERFLOW;
};
Result alu(uint32_t a, uint32_t b, unsigned op) {
    const auto c = control(op);
    const auto ar = addsub(a,b,c.SUB);
    const auto l = logic(a,b);
    const auto sh = shift(a,b,c.SHIFT_LEFT,c.SHIFT_ARITHMETIC);
    const auto cmp = compare(ar.OUT,ar.CARRY_OUT,ar.OVERFLOW,c.SUB);
    const auto out = select(ar.OUT,l.AND_OUT,l.OR_OUT,l.XOR_OUT,sh,
        cmp.SLT_RESULT,cmp.SLTU_RESULT,a,b,c.LOCAL_OP,c.HIGH_HALF,c.OUT_OF_RANGE);
    const auto f = flags(out,c.ARITHMETIC_ENABLE,ar.CARRY_OUT,ar.OVERFLOW);
    return {out,f.ZERO,cmp.EQ,cmp.LT_SIGNED,cmp.LT_UNSIGNED,f.NEGATIVE,f.CARRY_OUT,f.OVERFLOW};
}
}
```

#### Unknown inputs and timing

Structural gates can mask unused/unknown inputs. `ALU32Behavioral` instead makes **all outputs unknown if any bit of A, B, or OP is nonbinary**, including an unused operand. Its equivalence claim is settled **binary** behavior, not full four-state or transient equivalence. There is no clock: wait for propagation before sampling.

### Implementation

#### Structural implementation

The root contains `CONTROL`, `ARITHMETIC`, `LOGIC`, `SHIFT`, `COMPARE`, `RESULT_SELECT`, and `FLAGS`. Arithmetic results feed comparison; control selects a result and qualifies flags. Each group can be expanded independently in the visualizer.

#### Behavioral implementation

`ALU32Behavioral` computes the selected operation and flags directly with one abstract delay tick. The registered contract is `rv32i.alu32`; structural remains available, including selectable lower-level arithmetic/logic/shifter fidelities.

### Examples

<!-- simulator-examples: alu32 ALU32Test -->

#### Embedded simulation

<div class="simulator-embed"><iframe src="../../../simulator/?scenario=alu32&amp;embed=1" title="Complete ALU32" loading="lazy" allowfullscreen></iframe></div>

<a href="../../../simulator/?scenario=alu32" class="md-button">Open ALU32</a>

### Tests

`ALU32Test` checks the integrated binary contract in both fidelities. `ALU32RepresentativeSliceTest` validates the lower-level construction. `RV32IBuildingBlocksDocumentationTest` compiles the C++ examples above and checks their outputs against declared binary test actions. Source implementations, including `ALU32Behavioral`, are in `src/modules/composite/`.

## 4.3.3.9. `ALU32Core`

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `A`, `B` | Input | 32 each | Operands |
| `OP` | Input | 5 | Operation |
| `OUT` | Output | 32 | Selected result |
| `EQ`, `LT_SIGNED`, `LT_UNSIGNED` | Output | 1 each | Comparisons, valid for subtraction operations |

### Behavior

`ALU32Core` retains inputs `A[32]`, `B[32]`, `OP[5]` and outputs `OUT[32]`, `EQ`, `LT_SIGNED`, and `LT_UNSIGNED`. It omits `ZERO`, `NEGATIVE`, `CARRY_OUT`, and `OVERFLOW`, so its structural hierarchy has no `FLAGS` child. The arithmetic unit still computes carry and overflow internally because signed comparison needs them. The general `ALU32` retains all flags. Both variants use the same operation codes and settled binary result/compare rules. `ALU32CoreTest` checks the reduced interface in structural and behavioral fidelities against the same arithmetic vectors as `ALU32Test`.

#### C++ example

```cpp
namespace alu32_example {
struct CoreResult {
    uint32_t OUT;
    bool EQ, LT_SIGNED, LT_UNSIGNED;
};
CoreResult core(uint32_t a, uint32_t b, unsigned op) {
    const auto r = alu(a, b, op);
    return {r.OUT, r.EQ, r.LT_SIGNED, r.LT_UNSIGNED};
}
}
```

#### Unknown inputs and timing

The structural form may mask unknown values in unused logic; the behavioral form returns unknown outputs if any bit of `A`, `B`, or `OP` is nonbinary. Their equivalence claim covers settled binary inputs. The unit is combinational and has no clock.

### Implementation

#### Structural implementation

The `CONTROL`, `ARITHMETIC`, `LOGIC`, `SHIFT`, `COMPARE`, and `RESULT_SELECT` groups share the general `ALU32` construction. There is no `FLAGS` group or externally connected flag storage.

#### Behavioral implementation

The `rv32i.alu32-core` family uses `ALU32Behavioral` in its reduced-interface mode. It computes the same result and comparisons as the general behavioral ALU but does not expose the four omitted flags.

### Examples

<!-- simulator-examples: alu32-core ALU32CoreTest -->

#### Embedded simulation

<div class="simulator-embed"><iframe src="../../../simulator/?scenario=alu32-core&amp;embed=1" title="ALU32 CPU datapath" loading="lazy" allowfullscreen></iframe></div>

<a href="../../../simulator/?scenario=alu32-core" class="md-button">Open ALU32Core</a>

### Tests

`ALU32CoreTest` checks structural and behavioral results against the same arithmetic vectors. `RV32IBuildingBlocksDocumentationTest` replays the binary examples against the C++ reference above.
