# RV32I ALU Pair Report

## Outcome

Block 4 now has a same-contract pair:

- `ALU32`: the existing structural implementation.
- `BehavioralALU32`: the new compact reference/fast substitute.

`BehavioralALU32PairTest` passes all known-value equivalence rows and the behavioral unknown-input contract.

## Shared contract

Inputs:

- `A[31:0]`
- `B[31:0]`
- `OP[4:0]`

Outputs:

- `OUT[31:0]`
- `ZERO`: the selected result is zero.
- `EQ`: A equals B, independent of the selected result.
- `LT_SIGNED`: signed A is less than signed B.
- `LT_UNSIGNED`: unsigned A is less than unsigned B.
- `NEGATIVE`: bit 31 of the selected result.
- `CARRY_OUT`: carry for ADD; no-borrow indication for SUB, SLT, and SLTU.
- `OVERFLOW`: signed overflow for ADD; subtraction overflow for SUB, SLT, and SLTU.

The operation encoding is shared through `ALU32Op`: ADD, SUB, AND, OR, XOR, SLL, SRL, SRA, SLT, SLTU, PASS_A, PASS_B, and ZERO occupy values 0-12. Encodings 13-31 safely produce zero with inactive carry and overflow.

## Implementations

The structural `ALU32` visibly composes:

- ripple-carry add/subtract paths;
- a 32-bit logic unit;
- a five-stage barrel shifter;
- signed and unsigned comparison logic;
- zero and sign detection;
- result and flag muxes.

`BehavioralALU32` calculates the same known binary results directly in one compact component. It is a sibling used as an answer sheet or optional fast substitute; it is not hidden inside `ALU32`.

For four-state safety, the compact implementation uses a deliberately conservative policy: if any A, B, or OP bit is unknown or high-impedance, all outputs become unknown. This avoids inventing a definite CPU result from incomplete inputs.

## Verification

The pair test drives both components from the same wires and checks each against an independent expected-value function before comparing all outputs directly. Its 128 known-value rows cover:

- all 32 possible OP encodings;
- carry, borrow/no-borrow, and signed-overflow boundaries;
- zero and negative results;
- patterned AND, OR, and XOR;
- shifts by 0, 31, 32, and 33, proving the RV32I five-bit shift mask;
- signed and unsigned comparison boundaries;
- PASS_A, PASS_B, and ZERO;
- 64 deterministic pseudo-random operand/op combinations.

One additional row injects an unknown bit and verifies the conservative behavioral policy.

Command and result:

```text
ctest --test-dir build -R '^BehavioralALU32PairTest$' --output-on-failure
BehavioralALU32PairTest ... Passed
```

## Boundary

Direct structural/behavioral equivalence is proven for fully known binary inputs. It is not claimed for partial-unknown inputs: structural gates may preserve some definite bits while the behavioral model intentionally invalidates the whole result. That difference is an explicit contract boundary, not a known-value correctness gap.
