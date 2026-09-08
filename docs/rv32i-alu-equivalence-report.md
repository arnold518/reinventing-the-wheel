# RV32I ALU Equivalence Report

## Outcome

Block 4 now has two implementations of the same contract:

- `ALU32`: the existing structural implementation.
- the behavioral fidelity of `ALU32`: the new compact reference/fast substitute.

`ALU32Test` is the current registry-driven structural/behavioral check. The older separately named `ALU32EquivalenceTest` is no longer registered.

## Shared contract

Inputs:

- `A[31:0]`
- `B[31:0]`
- `OP[4:0]`

Outputs:

- `OUT[31:0]`
- `ZERO`: the selected result is zero.
- `EQ`: A equals B during SUB/SLT/SLTU; zero for other operations.
- `LT_SIGNED`: signed A is less than signed B during SUB/SLT/SLTU; zero otherwise.
- `LT_UNSIGNED`: unsigned A is less than unsigned B during SUB/SLT/SLTU; zero otherwise.
- `NEGATIVE`: bit 31 of the selected result.
- `CARRY_OUT`: carry for ADD; no-borrow indication for SUB, SLT, and SLTU.
- `OVERFLOW`: signed overflow for ADD; subtraction overflow for SUB, SLT, and SLTU.

The operation encoding is shared through `ALU32Op`: ADD, SUB, AND, OR, XOR, SLL, SRL, SRA, SLT, SLTU, PASS_A, PASS_B, and ZERO occupy values 0-12. Encodings 13-31 safely produce zero with inactive carry and overflow.

## Implementations

The structural `ALU32` has seven immediate children: `CONTROL`, `ARITHMETIC`, `LOGIC`, `SHIFT`, `COMPARE`, `RESULT_SELECT`, and `FLAGS`. There is one shared 32-bit add/subtract chain. The control gates, comparison predicates, original four result multiplexers, and flag gates are grouped into structural components; they have not been replaced by behavioral code. See [assembly boundaries](multi-bit-alu-implementation.md#alu-assembly-boundaries) for the type mapping and moved internal paths.

The behavioral fidelity of `ALU32` calculates the same known binary results directly in one compact component. The structural top level also retains selectable lower-level arithmetic, logic, shift, and zero-detection families for mixed profiles.

For four-state safety, the compact implementation uses a deliberately conservative policy: if any A, B, or OP bit is unknown or high-impedance, all outputs become unknown. This avoids inventing a definite CPU result from incomplete inputs.

## Verification

`ALU32Test` runs the registered implementations against the same 46 binary rows and expected results. These cover:

- all 32 possible OP encodings;
- carry, borrow/no-borrow, and signed-overflow boundaries;
- zero and negative results;
- patterned AND, OR, and XOR;
- shifts by 0, 31, 32, and 33, proving the RV32I five-bit shift mask;
- signed and unsigned comparison boundaries;
- PASS_A, PASS_B, and ZERO.

The six new integration-block scenarios in `ALUIntegrationTests.cpp` check both control decoders, both result selectors, and ALU32 comparison and flag blocks separately. `ALUHierarchyTest` guards the seven-block assembly and shared carry chains. `RecursiveBuildProfileTest` checks that the five selectable lower-level ALU32 families remain reachable through a subtree profile, including both nested zero detectors.

Command and result:

```text
ctest --test-dir build -R 'ALU32|ALUHierarchyTest|RecursiveBuildProfileTest' --output-on-failure
```

## Boundary

The structural/behavioral equivalence claim is limited to known binary inputs, with the test coverage above; it is not exhaustive over all operand combinations. Equivalence is not claimed for partial-unknown inputs: structural gates may preserve some definite bits while the behavioral model intentionally invalidates the whole result.
