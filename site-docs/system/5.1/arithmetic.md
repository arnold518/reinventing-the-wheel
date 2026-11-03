# § 5.1.1. Multiply and Divide Units

RV32IM adds eight instructions to RV32I: four multiplies (`MUL`, `MULH`,
`MULHSU`, `MULHU`) and four divides (`DIV`, `DIVU`, `REM`, `REMU`). Their
hardware is built the way a person multiplies and divides on paper: one bit
per clock edge, with a small adder or subtractor doing each step. A 32-bit
operation takes one launch edge and then 32 step edges.

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| `MultiplyStep<W>` | One shift-and-add step: add the multiplicand when the current bit is 1 | Yes | Yes, explicit gate-list form |
| `DivideStep<W>` | One restoring-division step: subtract the divisor when it fits | Yes | Yes, explicit gate-list form |
| `Multiplier<W>` | W steps of `MultiplyStep<W>` with a launch/result handshake | Yes | No |
| `Divider<W>` | W steps of `DivideStep<W>` with the same handshake | Yes | No |
| `MulDivUnit<W>` | A multiplier and a divider behind one interface, selected by the instruction's funct3 | Yes | Yes (W = 32) |

All five support W = 4, 8, 16 and 32 (catalog `arithmetic.multiply-step.W`,
`arithmetic.divide-step.W`, `arithmetic.iterative.multiplier.W`,
`arithmetic.iterative.divider.W`, `arithmetic.iterative.mul-div.W`). The
core uses W = 32; the 4-bit forms are small enough to inspect bit by bit.

The step components' compact forms come from the shared gate-list registry.
They are selected by an explicit choice, such as collapsing that component
in the explorer; general compact preferences keep their gates. This does
not add compact standalone `Multiplier<W>` or `Divider<W>` roots.

### A worked example

`7 × 5` with W = 4 (`A` = 0111, `B` = 0101). Each step looks at one bit of
`B`, from the lowest, and adds `A` into the high half when that bit is 1;
then the pair (high half, low half) shifts right one place.

| Step | Bit of B | High half before | Added | Sum (carry, 4 bits) | After shift: high · low |
| ---: | :---: | ---: | ---: | ---: | --- |
| 1 | 1 | 0000 | 0111 | 0 0111 | 0011 · 1... |
| 2 | 0 | 0011 | 0000 | 0 0011 | 0001 · 11.. |
| 3 | 1 | 0001 | 0111 | 0 1000 | 0100 · 011. |
| 4 | 0 | 0100 | 0000 | 0 0100 | 0010 · 0011 |

The result is high 0010, low 0011: 0010 0011 = 35. Division runs the same
way backwards: shift one dividend bit into the remainder, and subtract the
divisor when it fits, writing a quotient bit of 1.

## 5.1.1.1. `MultiplyStep<W>`

### IO pins

| Pin | Direction | Width | Meaning |
| --- | --- | ---: | --- |
| `A` | Input | W | Multiplicand |
| `BIT` | Input | 1 | Current multiplier bit |
| `HI` | Input | W | Running high half |
| `CARRY` | Output | 1 | Carry out of the addition |
| `SUM` | Output | W | `HI + (BIT ? A : 0)`, W bits |

### Behavior

`SUM` and `CARRY` are `HI + (BIT ? A : 0)` as a W+1-bit result. The part is
combinational. `BIT = 0` adds nothing.

#### C++ reference

```cpp
#include <cstdint>
#include <utility>

template<unsigned W>
std::pair<uint32_t, bool> multiply_step_ref(uint32_t hi, uint32_t a, bool bit) {
    static_assert(W == 4 || W == 8 || W == 16 || W == 32);
    constexpr uint64_t mask = (uint64_t{1} << W) - 1;
    const uint64_t sum = (hi & mask) + (bit ? (a & mask) : 0);
    return {uint32_t(sum & mask), bool(sum >> W)}; // SUM, CARRY
}
```

This is a binary settled-value reference, with a wide temporary so the carry
is retained even at W = 32. It does not model gate delays or four-state values.
In the circuit, a known `BIT = 0` masks even unknown bits of `A`; uncertainty
in `HI` can still reach the sum. With `BIT = X` or `Z`, each AND lane uses
the usual four-state rules rather than choosing a host-language branch.

### Implementation

One `ANDGate<W>` masks `A` with `BIT` (the bit is spread to every lane by
wiring), and an adder adds the result to `HI`: full adders at W = 4, the
[`ParallelPrefixAdder<W>`](../../alu/2.1.md) of § 2 at 8, 16 and 32 bits.
No host multiplication runs inside the component.

Timing follows the masking gate and the adder's carry paths. The explicit
compact step evaluates this same gate list, including four-state masking,
and derives its delays when first built. There is no stored step-delay
table in the current generated files; numeric path rows require an export
of that derived table for the chosen width. A compact parent unit also
includes these same gates in its compiled circuit.

### Examples

<!-- simulator-examples: multiply-step MultiplyStepScenarioTest -->

<div class="simulator-embed">
  <iframe src="../../../simulator/?scenario=multiply-step&amp;embed=1" title="MultiplyStep<4> simulation" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../simulator/?scenario=multiply-step" class="md-button">Open the full visualizer</a>

## 5.1.1.2. `DivideStep<W>`

### IO pins

| Pin | Direction | Width | Meaning |
| --- | --- | ---: | --- |
| `BIT` | Input | 1 | Next dividend bit shifted in |
| `DIVISOR` | Input | W | Divisor |
| `REMAINDER` | Input | W | Partial remainder |
| `NEXT_REMAINDER` | Output | W | Remainder after this step |
| `QUOTIENT_BIT` | Output | 1 | 1 when the divisor was subtracted |

### Behavior

Shift `REMAINDER` left by one place, bring in `BIT`, and compare the W+1-bit
result with `DIVISOR`. When it is at least the divisor, subtract and set
`QUOTIENT_BIT`; otherwise keep it. `NEXT_REMAINDER` is the low W bits. Every
binary input is defined, including a zero divisor (the step always
subtracts).

#### C++ reference

```cpp
#include <cstdint>
#include <utility>

template<unsigned W>
std::pair<uint32_t, bool> divide_step_ref(uint32_t remainder,
                                       uint32_t divisor, bool bit) {
    static_assert(W == 4 || W == 8 || W == 16 || W == 32);
    constexpr uint64_t mask = (uint64_t{1} << W) - 1;
    const uint64_t shifted = (uint64_t(remainder & mask) << 1) | bit;
    const uint64_t d = divisor & mask;
    const bool subtract = shifted >= d;
    return {uint32_t((subtract ? shifted - d : shifted) & mask), subtract};
}
```

The wide temporary matters: discarding the old remainder's top bit before
the comparison gives the wrong quotient bit. The result pair is
`NEXT_REMAINDER`, `QUOTIENT_BIT`. Unknown inputs in the circuit propagate
through subtraction and selection; this binary reference does not assign
an integer value to `X` or `Z`.

### Implementation

A subtractor computes the shifted remainder minus the divisor; the bit that
was shifted out of the remainder and the subtractor's carry together decide
whether the divisor fits, so no W+1-bit pin is needed. One `Mux<2,W>` chooses
the difference or the shifted remainder.

Its explicit compact form evaluates the same combinational gates, with
four-state values and width-specific delays derived at build time. As with
the multiply step, there is no current stored table to render here without
exporting the chosen configuration's derived paths.

### Examples

<!-- simulator-examples: divide-step DivideStepScenarioTest -->

<div class="simulator-embed">
  <iframe src="../../../simulator/?scenario=divide-step&amp;embed=1" title="DivideStep<4> simulation" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../simulator/?scenario=divide-step" class="md-button">Open the full visualizer</a>

## 5.1.1.3. `Multiplier<W>`

### IO pins

| Pin | Direction | Width | Meaning |
| --- | --- | ---: | --- |
| `A` | Input | W | First operand |
| `B` | Input | W | Second operand |
| `CANCEL` | Input | 1 | Synchronously discard the operation, even while paused |
| `CLK` | Input | 1 | Rising edges launch and step the operation |
| `ENABLE` | Input | 1 | Low pauses iteration and completion |
| `RST` | Input | 1 | Active-high reset: discards any operation |
| `SIGNED_A` | Input | 1 | Treat A as signed for HI |
| `SIGNED_B` | Input | 1 | Treat B as signed for HI |
| `START` | Input | 1 | Launch request |
| `TAKE` | Input | 1 | Consume a completed result |
| `BUSY` | Output | 1 | An operation is iterating |
| `DONE` | Output | 1 | A result is held |
| `HI` | Output | W | High W bits of the product |
| `LO` | Output | W | Low W bits of the product |
| `READY` | Output | 1 | A launch would be accepted |

### Behavior

| Edge (with `ENABLE` = 1) | Effect |
| --- | --- |
| `START` and `READY` | Capture `A`, `B` and the signedness; `BUSY` rises |
| Each of the next W edges | One `MultiplyStep<W>`: add, then shift the product pair |
| After the last step | `DONE` rises; `HI`, `LO` hold the product |
| `TAKE` while `DONE` | Release the result; `READY` rises |

`LO` is the low half for every signedness. For `HI`, a signed operand that
is negative contributes a correction: the unsigned high half minus the other
operand. `ENABLE` = 0 pauses the steps and holds a finished result. `CANCEL`
discards the operation on the next edge even while paused; `RST` clears
everything at once. A `START` while not `READY` is ignored; changing `A` or
`B` after launch has no effect.

### Implementation

| Group | Contents |
| --- | --- |
| `CONTROL` | Busy and done state, a log2(W)-bit step counter, enables |
| `OPERANDS` | The captured operands and signedness |
| `DATAPATH` | High and low product registers, `MultiplyStep<W>`, next-value muxes |
| `RESULT` | The signed correction of the high half |

### Examples

The recording covers unsigned, signed and mixed-sign products, high-word
boundaries, a pause, a held result, consumption, cancellation while paused
and reset during an operation.

<!-- simulator-examples: multiplier-w32 MultiplierWidth32ScenarioTest -->

<div class="simulator-embed">
  <iframe src="../../../simulator/?scenario=multiplier-w32&amp;embed=1" title="Multiplier<32> simulation" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../simulator/?scenario=multiplier-w32" class="md-button">Open the full visualizer</a>

## 5.1.1.4. `Divider<W>`

### IO pins

| Pin | Direction | Width | Meaning |
| --- | --- | ---: | --- |
| `A` | Input | W | Dividend |
| `B` | Input | W | Divisor |
| `CANCEL` | Input | 1 | Synchronously discard the operation, even while paused |
| `CLK` | Input | 1 | Rising edges launch and step the operation |
| `ENABLE` | Input | 1 | Low pauses iteration and completion |
| `RST` | Input | 1 | Active-high reset: discards any operation |
| `SIGNED` | Input | 1 | Signed division |
| `START` | Input | 1 | Launch request |
| `TAKE` | Input | 1 | Consume a completed result |
| `BUSY` | Output | 1 | An operation is iterating |
| `DONE` | Output | 1 | A result is held |
| `QUOTIENT` | Output | W | Quotient |
| `READY` | Output | 1 | A launch would be accepted |
| `REMAINDER` | Output | W | Remainder |

### Behavior

The same handshake as the multiplier: one launch edge, W step edges, a held
result. `SIGNED` divides the magnitudes and then fixes the signs: the
quotient rounds toward zero and the remainder takes the dividend's sign.
Two cases follow the RISC-V M rules rather than trapping:

| Case | Quotient | Remainder |
| --- | --- | --- |
| Divide by zero | all ones (2^W − 1, or −1 signed) | the dividend |
| Signed overflow (most negative ÷ −1) | the most negative value | 0 |

### Implementation

| Group | Contents |
| --- | --- |
| `CONTROL` | Busy and done state, step counter, enables |
| `OPERANDS` | Divisor magnitude, dividend magnitude, captured signs |
| `DATAPATH` | Quotient and remainder registers, `DivideStep<W>`, next-value muxes |
| `RESULT` | Sign correction and the divide-by-zero rule |

### Examples

The recording covers nonzero remainders, each negative operand, division by
zero, signed overflow, pause, hold, consume, cancellation while paused and
reset during an operation.

<!-- simulator-examples: divider-w32 DividerWidth32ScenarioTest -->

<div class="simulator-embed">
  <iframe src="../../../simulator/?scenario=divider-w32&amp;embed=1" title="Divider<32> simulation" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../simulator/?scenario=divider-w32" class="md-button">Open the full visualizer</a>

## 5.1.1.5. `MulDivUnit<W>`

### IO pins

| Pin | Direction | Width | Meaning |
| --- | --- | ---: | --- |
| `A` | Input | W | rs1 value |
| `B` | Input | W | rs2 value |
| `CANCEL` | Input | 1 | Synchronously discard the operation, even while paused |
| `CLK` | Input | 1 | Rising edges launch and step the operation |
| `ENABLE` | Input | 1 | Low pauses iteration and completion |
| `OP` | Input | 3 | Operation: instruction funct3 |
| `RST` | Input | 1 | Active-high reset: discards any operation |
| `START` | Input | 1 | Launch request |
| `TAKE` | Input | 1 | Consume a completed result |
| `BUSY` | Output | 1 | An operation is iterating |
| `DONE` | Output | 1 | A result is held |
| `READY` | Output | 1 | A launch would be accepted |
| `RESULT` | Output | W | Selected result |

### Behavior

`OP` is the instruction's funct3:

| OP | Instruction | RESULT |
| ---: | --- | --- |
| 0 | `MUL` | Low half of the product |
| 1 | `MULH` | High half, both signed |
| 2 | `MULHSU` | High half, A signed, B unsigned |
| 3 | `MULHU` | High half, both unsigned |
| 4 | `DIV` | Signed quotient |
| 5 | `DIVU` | Unsigned quotient |
| 6 | `REM` | Signed remainder |
| 7 | `REMU` | Unsigned remainder |

The launch captures `OP` with the operands; the handshake, latency and
cancellation are those of the units inside.

`READY` describes an idle slot, not permission to ignore `ENABLE`: a launch
needs `ENABLE && START && READY`. While disabled, `TAKE` does not release a
finished result. `CANCEL` has priority over normal busy/done updates even
when disabled, but does not promise that the old result bits become zero;
only a result with `DONE = 1` is valid. Consuming a result and launching the
next operation require separate edges because `READY` is low while `DONE`
is high. Reset clears the stored state after its propagation delay.

The numerical instruction results and latency tests use known binary
operands and controls after reset. Four-state signals still flow through
the gates; no numerical multiply/divide result is promised for unknown
operands or ambiguous clock edges.

### Implementation

#### Structural implementation

A `Multiplier<W>` and a `Divider<W>` side by side, an operation register and
`RESULT_SELECTION`, the muxes that pick the product half or the quotient or
remainder. Exactly one engine takes each launch.

#### Behavioral implementation

At W = 32, the compact form runs this unit's own gates as a compiled gate
list with characterized register state, and a delay table proven on the same
circuit. It keeps the 33-edge latency and the handshake; it is not a host
multiplication.

**Timing.** Under a synchronous binary boundary, the structural unit
certifies a minimum clock period of 48 unit ticks, 2322 ps (embedded) and
1620 ps (desktop); `RV32IMTimingTest` derives each.

The selected paths below come directly from the stored default 32-bit table,
with reset inactive. Setup and clock-to-output values describe propagation
around an edge; the 33-edge algorithm latency is separate. The standalone
clock certificate also accounts for the falling-edge input constraints and
can exceed the table's internal register-to-register period.

<!-- timing-rows
Edge to result | arithmetic.iterative.mul-div.32.behavioral | CLK ↑ → RESULT
Launch setup | arithmetic.iterative.mul-div.32.behavioral | START before CLK ↑
Operand setup | arithmetic.iterative.mul-div.32.behavioral | A before CLK ↑
Cancel to ready | arithmetic.iterative.mul-div.32.behavioral | CANCEL → READY
-->

### Examples

The recording runs all eight operations, divide by zero, signed overflow,
and the pause, cancel and reset transitions.

<!-- simulator-examples: muldiv-w32 MulDivUnitWidth32ScenarioTest -->

<div class="simulator-embed">
  <iframe src="../../../simulator/?scenario=muldiv-w32&amp;embed=1" title="MulDivUnit<32> simulation" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../simulator/?scenario=muldiv-w32" class="md-button">Open the full visualizer</a>

## Source and tests

- [Interfaces and supported widths](https://github.com/arnold518/reinventing-the-wheel/blob/alpha/include/modules/rv32im/MulDiv.hpp)
- [Structural assembly](https://github.com/arnold518/reinventing-the-wheel/blob/alpha/src/modules/rv32im/MulDiv.cpp), [iteration, datapath and sign-correction groups](https://github.com/arnold518/reinventing-the-wheel/blob/alpha/src/modules/rv32im/ArithmeticGroups.hpp)
- [Compact gate-list implementation](https://github.com/arnold518/reinventing-the-wheel/blob/alpha/src/modules/rv32im/MulDivBehavioral.cpp), [functional tests](https://github.com/arnold518/reinventing-the-wheel/blob/alpha/src/tests/RV32IMPhase3Tests.cpp), [clock certificates](https://github.com/arnold518/reinventing-the-wheel/blob/alpha/tests/cpp/RV32IMTimingTest.cpp)
- [Shared explicit compact registry](https://github.com/arnold518/reinventing-the-wheel/blob/alpha/src/components/selection/GateListCompact.cpp) adds the step forms after module registration.

| Test | Coverage |
| --- | --- |
| `MultiplyStepTest`, `DivideStepTest` | Every 4-bit input combination and 128 spread input cases at each of 8, 16 and 32 bits (64 operand pairs × both BIT values), against an integer reference, structural and with compact children |
| `MultiplierWidth32Test`, `DividerWidth32Test`, `MulDivUnitWidth32Test` | Launch, latency, pause, hold, cancel, reset; signed and unsigned boundaries |
| `MulDivGateLevelTest` | The fully gate-level unit: an exhaustive 4-bit `MulDivUnit<4>`, boundary 8/16/32-bit operands |
| `WidthComponentsTest` | Latency and results at W = 4, 8, 16, 32 |
| `GateListCompactEquivalenceTest` | Explicit compact step forms versus their gates: binary, dense four-state and single-X/Z vectors |
| `*ScenarioTest` | The recorded examples above, both available fidelities |
