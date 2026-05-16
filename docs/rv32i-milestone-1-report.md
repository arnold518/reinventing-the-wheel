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
- Added a top-level structural `ALU32` with the documented external contract:
  - `A<32>`, `B<32>`, `OP<5>`
  - `OUT<32>`
  - `ZERO`, `EQ`, `LT_SIGNED`, `LT_UNSIGNED`, `NEGATIVE`, `CARRY_OUT`, `OVERFLOW`
- Added 5-bit `Pin`, `Wire`, `WireUpdateEvent`, dynamic builder, test helper, and `Rewire` support for ALU operation selection.
- Exposed the new 32-bit modules through pybind11 bindings.
- Registered per-component 32-bit CTest scenarios plus the RV32I ALU integration test.
- Exposed all new test scenarios through visualizer-v2 discovery:
  - `rewire-width5`
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
- Kept the implementation structural; no product-facing behavioral `ALU32Fast` exists.

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
- `include/tests/RV32IALU32Tests.hpp`
- `src/tests/RV32IALU32Tests.cpp`
- `src/tests/TestRegistry.cpp`
- `tests/CMakeLists.txt`
- `src/bindings/ModulesBinding.cpp`
- `src/bindings/TestsBinding.cpp`
- `visualizer-v2/server.py`

## Verification Coverage

Per-component tests now cover:

- `Adder32Test`: carry-in, carry-out, and sum edge cases.
- `Mux32to1Test`: one-bit 32:1 mux selection and unselected-input rejection.
- `Mux32to1_32bitTest`: 32-bit 32:1 result mux selection.
- `AddSub32Test`: 32-bit add/subtract edge cases, carry, and signed overflow.
- `Logic32Test`: bitwise AND, OR, and XOR patterns.
- `ZeroDetect32Test`: zero and non-zero inputs.
- `Comparator32Test`: equality, signed less-than, unsigned less-than, diff, carry, and overflow.
- `Shifter32Test`: SLL, SRL, and SRA for all shift amounts `0..31`, plus shift amount masking beyond 31.
- `ALU32Test`: every defined `ALU32Op`, default/reserved `OP` behavior selecting zero, and selected flag behavior.
- `RV32IALU32Test`: full structural ALU32 scenario plus exhaustive 4-bit add/sub slice coverage over every `A`, `B`, and `SUB` combination.

## Verification Results

Commands run:

```bash
cmake -S . -B build
cmake --build build -j 4
ctest --test-dir build --output-on-failure -R "Mux32to1Test|Mux32to1_32bitTest|Adder32Test|AddSub32Test|Logic32Test|ZeroDetect32Test|Comparator32Test|Shifter32Test|ALU32Test|RV32IALU32Test|RewireWidth5Test"
ctest --test-dir build --output-on-failure
```

Results:

- Configure passed.
- Build passed.
- Focused RV32I/32-bit subset passed: 11/11 tests in 133.41 seconds.
- Full CTest passed: 67/67 tests in 143.58 seconds.
- Hosted visualizer scenario discovery after restart includes `mux32to1`, `mux32to1-32bit`, `adder32`, `addsub32`, `logic32`, `zero-detect32`, `comparator32`, `shifter32`, `alu32`, and `rv32i-alu32`.
- Hosted `alu32` topology exposes `RESULT_MUX`, `CARRY_MUX`, and `OVERFLOW_MUX` as bound mux components with 34 visible pins each.

## Known Limits

- This milestone does not include an RV32I instruction decoder.
- No register file, program counter, instruction memory, data memory, load/store unit, branch/jump unit, or CPU top-level exists yet.
- No assembly programs or binary fixtures exist yet.
- Full structural `ALU32` visualization is large: the expanded topology currently contains 8,840 components, 17,074 wires, and 29,837 pins. Use the per-component scenarios for readable debugging, then use `alu32` or `rv32i-alu32` for end-to-end inspection.
- The structural ALU path is slow enough that a future behavioral `ALU32Fast` may be useful, but only after equivalence tests against this structural implementation exist.

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
