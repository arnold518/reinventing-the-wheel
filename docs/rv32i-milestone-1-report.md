# RV32I Milestone 1 Implementation Report

Last updated: 2026-05-16

## Status

Milestone 1 is complete for the structural RV32I ALU32 foundation.

The branch does not implement an RV32I CPU yet. It implements the lower-level 32-bit ALU stack that later decoder, datapath, branch, load/store, and CPU components will use.

## Scope Completed

- Added a structural `Adder32` built from 32 `FullAdder` components.
- Added a structural `AddSub32` built from 32 `FullAdder` components plus `B xor SUB` subtraction control.
- Added structural 32-bit logic, zero-detect, comparator, and barrel-shifter blocks.
- Promoted the ALU result/flag mux helpers to bound public modules:
  - `Mux32to1`
  - `Mux32to1_32bit`
- Bound and tested the width-specific constant sources used inside `ALU32`:
  - `ConstantValue1`
  - `ConstantValue32`
- Added a top-level structural `ALU32` with the documented external contract:
  - `A<32>`, `B<32>`, `OP<5>`
  - `OUT<32>`
  - `ZERO`, `EQ`, `LT_SIGNED`, `LT_UNSIGNED`, `NEGATIVE`, `CARRY_OUT`, `OVERFLOW`
- Added 5-bit `Pin`, `Wire`, `WireUpdateEvent`, dynamic builder, test helper, and `Rewire` support for ALU operation selection.
- Exposed the new 32-bit modules through pybind11 bindings.
- Registered per-component 32-bit CTest scenarios plus the RV32I ALU integration test.
- Exposed all new test scenarios through visualizer-v2 discovery:
  - `rewire-width5`
  - `constant1-high`
  - `constant32`
  - `mux32to1`
  - `mux32to1-32bit`
  - `adder32`
  - `addsub32`
  - `logic32`
  - `zero-detect32`
  - `comparator32`
  - `shifter32`
  - `alu32`
  - `rv32i-alu32`
- Kept the implementation structural; the behavioral fidelity of the `ALU32`
  family did not exist yet.

## Implemented Files

- `include/modules/composite/Adder32.hpp`
- `include/modules/composite/AddSub32.hpp`
- `include/modules/composite/Logic32.hpp`
- `include/modules/composite/ZeroDetect32.hpp`
- `include/modules/composite/Comparator32.hpp`
- `include/modules/composite/Shifter32.hpp`
- `include/modules/composite/ALU32.hpp`
- `src/modules/composite/Adder32.cpp`
- `src/modules/composite/AddSub32.cpp`
- `src/modules/composite/Logic32.cpp`
- `src/modules/composite/ZeroDetect32.cpp`
- `src/modules/composite/Comparator32.cpp`
- `src/modules/composite/Shifter32.cpp`
- `src/modules/composite/ALU32.cpp`
- `include/modules/basic/Mux.hpp`
- `src/modules/basic/Mux.cpp`
- `include/tests/ArithmeticLogicTests.hpp`
- `src/tests/ArithmeticLogicTests.cpp`
- `include/tests/UtilityComponentTests.hpp`
- `src/tests/UtilityComponentTests.cpp`
- `include/tests/ALU32LowerLevelSliceTests.hpp`
- `src/tests/ALU32LowerLevelSliceTests.cpp`
- `src/tests/TestRegistry.cpp`
- `tests/CMakeLists.txt`
- `src/bindings/ModulesBinding.cpp`
- `src/bindings/TestsBinding.cpp`
- `visualizer-v2/server.py`

## ALU32 Operation Decoding

`ALU32.OP` is a 5-bit project-local ALU operation select field. These values are not RISC-V instruction opcodes. A later RV32I decoder will translate instruction fields such as `opcode`, `funct3`, and `funct7` into these ALU control values.

Inputs:

- `A`: 32-bit first operand.
- `B`: 32-bit second operand. For shifts, `B[4:0]` is the shift amount.
- `OP`: 5-bit operation select.

Outputs:

- `OUT`: selected 32-bit result.
- `ZERO`: high when `OUT == 0`.
- `NEGATIVE`: copy of `OUT[31]`.
- `EQ`, `LT_SIGNED`, and `LT_UNSIGNED`: compare `A` and `B` regardless of selected `OP`.
- `CARRY_OUT` and `OVERFLOW`: selected add/sub/compare datapath flags; low for logical, shift, pass-through, zero, and reserved operations.

Defined operation codes:

| `OP` hex | `OP` binary | Name | Definition | Example |
| --- | --- | --- | --- | --- |
| `0x00` | `00000` | `ADD` | `OUT = A + B` modulo 32 bits. `CARRY_OUT` is unsigned carry. `OVERFLOW` is signed add overflow. | `A=0xffffffff`, `B=0x00000001` -> `OUT=0x00000000`, `CARRY_OUT=1`, `ZERO=1` |
| `0x01` | `00001` | `SUB` | `OUT = A - B` modulo 32 bits. `CARRY_OUT=1` means no unsigned borrow. `OVERFLOW` is signed subtract overflow. | `A=0x00000000`, `B=0x00000001` -> `OUT=0xffffffff`, `CARRY_OUT=0`, `NEGATIVE=1` |
| `0x02` | `00010` | `AND` | Bitwise AND. | `A=0xf0f0f0f0`, `B=0x0ff00ff0` -> `OUT=0x00f000f0` |
| `0x03` | `00011` | `OR` | Bitwise OR. | `A=0xf0f0f0f0`, `B=0x0ff00ff0` -> `OUT=0xfff0fff0` |
| `0x04` | `00100` | `XOR` | Bitwise XOR. | `A=0xf0f0f0f0`, `B=0x0ff00ff0` -> `OUT=0xff00ff00` |
| `0x05` | `00101` | `SLL` | Logical left shift by `B[4:0]`. Upper bits shift out, low bits fill with zero. | `A=0x00000001`, `B=0x0000001f` -> `OUT=0x80000000` |
| `0x06` | `00110` | `SRL` | Logical right shift by `B[4:0]`. Low bits shift out, high bits fill with zero. | `A=0x80000000`, `B=0x0000001f` -> `OUT=0x00000001` |
| `0x07` | `00111` | `SRA` | Arithmetic right shift by `B[4:0]`. Low bits shift out, high bits copy `A[31]`. | `A=0x80000000`, `B=0x0000001f` -> `OUT=0xffffffff` |
| `0x08` | `01000` | `SLT` | Signed less-than. `OUT=1` when `(int32_t)A < (int32_t)B`, else `0`. | `A=0xffffffff`, `B=0x00000001` -> `OUT=0x00000001` |
| `0x09` | `01001` | `SLTU` | Unsigned less-than. `OUT=1` when `A < B` as unsigned values, else `0`. | `A=0x00000000`, `B=0xffffffff` -> `OUT=0x00000001` |
| `0x0A` | `01010` | `PASS_A` | Pass-through of operand `A`. | `A=0x12345678`, `B=0x9abcdef0` -> `OUT=0x12345678` |
| `0x0B` | `01011` | `PASS_B` | Pass-through of operand `B`. | `A=0x12345678`, `B=0x9abcdef0` -> `OUT=0x9abcdef0` |
| `0x0C` | `01100` | `ZERO` | Constant zero result. | `A=0x12345678`, `B=0x9abcdef0` -> `OUT=0x00000000`, `ZERO=1` |

Reserved operation codes:

| `OP` range | Decode behavior | Example |
| --- | --- | --- |
| `0x0D` to `0x1F` | Reserved inputs are tied to `CONST_ZERO32`, so `OUT=0`. `ZERO=1`, `NEGATIVE=0`, `CARRY_OUT=0`, and `OVERFLOW=0`. Compare flags still reflect `A` versus `B`. | `OP=0x1f`, `A=0x12345678`, `B=0x9abcdef0` -> `OUT=0x00000000` |

Shift amount masking:

- `SLL`, `SRL`, and `SRA` use only `B[4:0]`.
- Example: `B=0x00000020` has `B[4:0]=0`, so shifting by `0x20` behaves as shifting by zero.
- Example: `B=0x00000021` has `B[4:0]=1`, so shifting by `0x21` behaves as shifting by one.

Intended RV32I mapping:

| RV32I instruction family | Intended ALU32 `OP` |
| --- | --- |
| `ADD`, `ADDI`, address generation for loads/stores, `AUIPC`, `JAL`, `JALR` target math | `ADD` |
| `SUB`, branch difference helper | `SUB` |
| `AND`, `ANDI` | `AND` |
| `OR`, `ORI` | `OR` |
| `XOR`, `XORI` | `XOR` |
| `SLL`, `SLLI` | `SLL` |
| `SRL`, `SRLI` | `SRL` |
| `SRA`, `SRAI` | `SRA` |
| `SLT`, `SLTI` | `SLT` |
| `SLTU`, `SLTIU` | `SLTU` |
| `LUI`, operand forwarding, debug helpers | `PASS_B` or `PASS_A`, depending on datapath wiring |
| Explicit zero/default/debug behavior | `ZERO` |

## Verification Coverage

Per-component tests now cover:

- `Adder32Test`: carry-in, carry-out, all-ones/alternating operands, signed edge cases, and carry propagation across every bit boundary.
- `ConstantValue1HighTest` and `ConstantValue1LowTest`: width-1 high and low constant sources.
- `ConstantValue32Test`: 32-bit constant source behavior.
- `Mux32to1Test`: one-bit 32:1 mux selection and unselected-input rejection.
- `Mux32to1_32bitTest`: 32-bit 32:1 result mux selection.
- `AddSub32Test`: 32-bit add/subtract edge cases, carry, signed overflow, carry propagation across every bit boundary, and borrow propagation across every bit boundary.
- `Logic32Test`: bitwise AND, OR, and XOR patterns plus walking-bit lane checks across all 32 bits.
- `ZeroDetect32Test`: zero, all-ones, sign-bit-only, and every single-bit non-zero input.
- `Comparator32Test`: equality, signed less-than, unsigned less-than, diff, carry, and overflow across a matrix of `0`, `1`, `2`, `INT_MAX`, `INT_MIN`, and `UINT_MAX`.
- `Shifter32Test`: SLL, SRL, and SRA for all shift amounts `0..31`, plus shift amount masking beyond 31.
- `ALU32StructuralContractTest`: every defined `ALU32Op`, every reserved `OP` from `0x0d` to `0x1f`, shift masking, and selected flag behavior.
- `ALU32LowerLevelSliceTest`: full structural ALU32 scenario plus exhaustive 4-bit add/sub slice coverage over every `A`, `B`, and `SUB` combination.

## Verification Results

Commands run:

```bash
cmake -S . -B build
cmake --build build -j 4
ctest --test-dir build --output-on-failure -R "ConstantValue1HighTest|ConstantValue1LowTest|ConstantValue8Test|ConstantValue32Test|ALU32StructuralContractTest|ALU32LowerLevelSliceTest"
ctest --test-dir build --output-on-failure -R "Mux32to1Test|Mux32to1_32bitTest|Adder32Test|AddSub32Test|Logic32Test|ZeroDetect32Test|Comparator32Test|Shifter32Test|ALU32StructuralContractTest|ALU32LowerLevelSliceTest|RewireWidth5Test"
ctest --test-dir build --output-on-failure -R "Adder32Test|AddSub32Test|Logic32Test|ZeroDetect32Test|Comparator32Test|Shifter32Test|ALU32StructuralContractTest|ALU32LowerLevelSliceTest"
ctest --test-dir build --output-on-failure
```

Results:

- Configure passed.
- Build passed.
- Focused constant/ALU subset passed: 7/7 tests in 109.59 seconds.
- Focused RV32I/32-bit subset passed: 11/11 tests in 133.41 seconds.
- Strengthened RV32I/32-bit ALU subset passed: 8/8 tests in 164.94 seconds.
- Full CTest passed: 69/69 tests in 186.94 seconds.
- Hosted visualizer scenario discovery after restart includes `constant1-from32`, `constant32`, `mux32to1`, `mux32to1-32bit`, `adder32`, `addsub32`, `logic32`, `zero-detect32`, `comparator32`, `shifter32`, `alu32`, and `rv32i-alu32`.
- Hosted `alu32` topology exposes `CONST_LOW`, `CONST_HIGH`, and `CONST_ZERO32` as bound constant components with visible pins.
- Hosted `alu32` topology exposes `RESULT_MUX`, `CARRY_MUX`, and `OVERFLOW_MUX` as bound mux components with 34 visible pins each.

## Known Limits

- This milestone does not include an RV32I instruction decoder.
- No register file, program counter, instruction memory, data memory, load/store unit, branch/jump unit, or CPU top-level exists yet.
- No assembly programs or binary fixtures exist yet.
- Full structural `ALU32` visualization is large: the expanded topology currently contains 8,840 components, 17,074 wires, and 29,847 pins. Use the per-component scenarios for readable debugging, then use `alu32` or `rv32i-alu32` for end-to-end inspection.
- The plan calls for a future behavioral fidelity of the same `ALU32` family.
  It may be useful for fast tests, but only after independent expected-value
  tests and direct equivalence against this structural implementation exist.

## Follow-Up Work

Next milestone: RV32I decoder library.

Immediate tasks:

1. Add `Instruction.hpp` and `Decoder.hpp` under `include/isa/rv32i/`.
2. Decode every RV32I base instruction into stable enums and fields.
3. Add immediate extraction and sign-extension tests.
4. Add illegal encoding tests.
5. Keep decoder independent of simulator state.

## Change Log

- 2026-05-16: Milestone 1 report created after structural ALU32 completion and CTest verification.
- 2026-05-16: Added per-component 32-bit tests and visualizer scenarios for `Adder32`, `AddSub32`, `Logic32`, `ZeroDetect32`, `Comparator32`, `Shifter32`, and `ALU32`.
- 2026-05-16: Promoted `Mux32to1` and `Mux32to1_32bit` from ALU-local helpers to bound modules so visualizer wires can anchor to their pins.
- 2026-05-16: Bound `ConstantValue<1,32>` and `ConstantValue<32,32>` so ALU constants show their trigger/output pins in the visualizer.
- 2026-05-16: Added complete `ALU32.OP` decoding documentation with definitions, examples, reserved-code behavior, and intended RV32I mapping.
- 2026-05-16: Hardened Milestone 1 tests with walking-bit, carry/borrow-boundary, comparator matrix, and full reserved-`OP` coverage.
