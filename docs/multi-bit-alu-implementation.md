# Multi-Bit And ALU Implementation

This branch restores the native multi-bit simulation path and the 8-bit ALU module.

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
- Top-level: `ALU8` with `A<8>`, `B<8>`, `OP<4>`, `OUT<8>`, `ZERO`, `CARRY`, `OVERFLOW`, and `NEGATIVE`.

## ALU Opcodes

| Opcode | Operation |
| --- | --- |
| `0x0` | `ADD`: `A + B` |
| `0x1` | `SUB`: `A - B` |
| `0x2` | `AND`: `A & B` |
| `0x3` | `OR`: `A | B` |
| `0x4` | `XOR`: `A ^ B` |
| `0x5` | `NOT`: `~A` |
| `0x6` | `SHL`: logical left shift |
| `0x7` | `SHR`: logical right shift |
| `0x8` | `SRA`: arithmetic right shift |
| `0x9` | `INC`: `A + 1` |
| `0xA` | `DEC`: `A - 1` |
| `0xB` | `NEG`: two's-complement negation |
| `0xC` | `PASS_A` |
| `0xD` | `PASS_B` |
| `0xE` | `CMP`: subtract result and flags |
| `0xF` | `ZERO`: constant zero |

The restored implementation is behavioral internally, which keeps tests deterministic and the simulator API stable. The public direction remains compatible with a later structural/gate-level expansion because the native multi-bit pin and wire interfaces are now in place.
