# Multi-Bit And ALU Implementation

This branch provides the native multi-bit simulation path and the structural 8-bit ALU module. The current `ALU8` replaces the original four-bit-opcode interface; it is not an additional `ALU8V2` type.

## Core Direction

- `Pin<WIDTH>` and `Wire<WIDTH>` support bus widths used by the ALU (`1`, `2`, `3`, `4`, `8`, and `16`) while preserving the single-bit visualizer-facing API.
- `PinBase` and `WireBase` provide width-erased access for builders, tests, simulation history, and Python bindings.
- `ComponentBuilder::addNewWireDynamic` connects runtime-width pins for truth-table tests and bus-based components.
- `TruthTableTest` accepts both single-bit `LogicValue` expectations and integer multi-bit expectations.

## Restored Modules

- Logic: `AND8`, `OR8`, `XOR8`, `NOT8`, `NAND8`, `NOR8`.
- Selection: `Mux2to1`, `Mux4to1`, `Mux8to1`, `Mux16to1`, plus 8-bit variants.
- Arithmetic: `Adder8`, `Subtractor8`, `SubtractorWithBorrow8`, `Incrementer8`, `Decrementer8`, `TwosComplement8`.
- Comparison and flags: `EqualityChecker8`, `Comparator8`, `SignedComparator8`, `ZeroDetect8`.
- Shifts: `ShiftLeftLogical8`, `ShiftRightLogical8`, `ShiftRightArithmetic8`.
- Arithmetic path: `AddSub8` and `ArithmeticUnit8`, sharing one `Adder8` between arithmetic and comparison operations.
- Additional units: `LogicUnit8`, `BarrelShifter8`, `CompareSelect8`, and `ALU8FlagUnit`.
- Top-level: `ALU8` with inputs `A<8>`, `B<8>`, `OP<5>`, `C_IN`; outputs `OUT<8>`, `ZERO`, `CARRY`, `OVERFLOW`, `NEGATIVE`, and `INVALID_OP`.

## ALU Opcodes

| Opcode | Operation |
| --- | --- |
| `0x00`–`0x06` | `ADD`, `ADC`, `SUB`, `SBC`, `INC`, `DEC`, `NEG` |
| `0x08`–`0x0E` | `AND`, `OR`, `XOR`, `NOT`, `NAND`, `NOR`, `XNOR` |
| `0x10`–`0x14` | `SLL`, `SRL`, `SRA`, `ROL`, `ROR` |
| `0x15`–`0x17` | `EQ`, `SLTU`, `SLT` (results 0 or 1) |
| `0x18`–`0x1B` | `MINU`, `MAXU`, `MINS`, `MAXS` |
| `0x1C`–`0x1E` | `PASS_A`, `PASS_B`, `CLEAR` |
| `0x07`, `0x0F`, `0x1F` | Reserved: zero result and `INVALID_OP = 1` |

All units are structural composites of gates, multiplexers, adapters, and full adders. Multiplication and division are not supported.

## Flags and compatibility

- `ADC = A + B + C_IN`; `SBC = A - B - (1 - C_IN)`.
- Subtraction `CARRY = 1` means no borrow; `NEG` therefore sets carry only when `A = 0`. Standalone legacy arithmetic helpers retain their existing contracts.
- Arithmetic `OVERFLOW` indicates signed overflow. For all other operations it is zero.
- Shift/rotate distance is `B[2:0]`; higher bits are ignored. Carry is the last shifted-out bit; at zero distance it forwards `C_IN`. Rotates do not rotate through carry.
- `ZERO` tests the final `OUT`, and `NEGATIVE` is its bit 7. Logic, predicates, min/max, pass, clear, and reserved operations have zero carry and overflow.
- Reserved operations produce `OUT = 0`, `ZERO = 1`, and the other arithmetic flags zero, even with unknown data inputs. Unknown inputs otherwise follow the structural gates' four-state semantics.
- The `alu.width8` catalog contract is version 2. Existing callers must update opcode encodings, the width of `OP`, and explicitly drive `C_IN`; there is no legacy-opcode interpretation switch.

The component reference is maintained in `site-docs/alu8/2.1.md` through `2.6.md`. Recorded scenarios cover the new units in both native and WASM visualizers. `ALU8ContractTest` provides large headless sweeps; `tests/ALU8WasmParityTest.mjs` compares portable native/WASM topology and state.

## ALU assembly boundaries

Both top levels now contain seven functional blocks. This is a structural regrouping, not gate-count optimization or a shared opcode contract.

| Role | ALU8 instance / type | ALU32 instance / type |
| --- | --- | --- |
| Control | `CONTROL / ALU8ControlDecoder` | `CONTROL / ALU32ControlDecoder` |
| Shared arithmetic | `ARITHMETIC / ArithmeticUnit8` | `ARITHMETIC / AddSub32` |
| Logic | `LOGIC / LogicUnit8` | `LOGIC / Logic32` |
| Shift | `SHIFTER / BarrelShifter8` | `SHIFT / Shifter32` |
| Comparison | `COMPARE_SELECT / CompareSelect8` | `COMPARE / ALU32CompareUnit` |
| Result selection | `RESULT_SELECT / ALU8ResultSelector` | `RESULT_SELECT / ALU32ResultSelector` |
| Flags | `FLAGS / ALU8FlagUnit` | `FLAGS / ALU32FlagUnit` |

ALU32 retains its RV32I opcode map, pin schema, behavioral alternative, and the existing four result multiplexers inside `RESULT_SELECT`. `ALU32CompareUnit` receives the shared subtraction result and metadata; its predicate outputs are masked except for SUB/SLT/SLTU. `ALU32FlagUnit` derives zero/sign from the selected result and masks carry/overflow with the arithmetic enable.

The selectable arithmetic, logic, and shift paths retain their instance names. The two selectable zero detectors move from `ARITHMETIC_ZERO` and `RESULT_ZERO` to `COMPARE.ARITHMETIC_ZERO` and `FLAGS.RESULT_ZERO`. Subtree/contract profile rules still reach them; custom exact-path rules targeting the old paths must use the new paths. Raw gate paths moved under the new blocks. Saved unrelated layouts and the placement of the ALU in its parent are not reset.

`ALUIntegrationTests.cpp` contains six standalone recorded component scenarios and `ALUHierarchyTest`. The existing ALU8 contract sweep, ALU32 structural/behavioral scenario checks, and CPU tests remain in place.
