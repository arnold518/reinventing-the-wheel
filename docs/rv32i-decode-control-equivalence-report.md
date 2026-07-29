# RV32I Decode/Control Contract Report

## Outcome

Block 2 now has two circuit components with the same external contract:

- `RV32IDecodeControlUnit`: a structural, expandable implementation.
- the behavioral fidelity of `RV32IDecodeControlUnit`: a compact reference implementation backed by `RV32IDecoder` and `RV32IControl`.

The dedicated component contract test passes for every instruction currently
supported by the RV32I answer sheet. The unified test runner exercises both
fidelities and compares their public checkpoint snapshots.

## Structural implementation

The structural decoder does not call the behavioral decoder or control library. Its visible internals are built from existing simulator components:

- `Rewire` extracts `rs1`, `rs2`, and `rd` and forms I-, S-, B-, U-, and J-type immediates.
- Twenty-two shared `RV32IBitPatternMatcher` predicates decode the ten used
  opcodes, all eight `funct3` values, the two used `funct7` values, and the two
  exact SYSTEM instructions.
- Forty small recognition terms combine those shared predicates for the
  supported RV32I instructions.
- Gate-level OR networks combine recognizer outputs into each control bit.
- `BitJoiner` components assemble multi-bit controls.
- `Mux8to1_32bit` selects the immediate required by the recognized format.
- Constants provide safe defaults for illegal instructions.

This is a factored programmable-logic-array shape: shared field predicates,
instruction product terms, then a control-output plane. It avoids comparing
the same opcode and function bits independently for every instruction. The
structural hierarchy now contains 1,964 components, down from the previously
recorded 2,514.

Illegal instructions produce inactive side effects, `TRAP_REQUEST=1`, and `DECODE_TRAP_CAUSE=IllegalInstruction`. `ECALL` requests an environment-call trap, while `EBREAK` requests a halt.

## Shared pins

Input:

- `INSTRUCTION[31:0]`

Decoded data outputs:

- `RS1_ADDR[4:0]`, `RS2_ADDR[4:0]`, `RD_ADDR[4:0]`
- `IMM[31:0]`

Execution and side-effect controls:

- `ALU_OP[4:0]`, `ALU_A_SEL[1:0]`, `ALU_B_SEL[1:0]`
- `LEGAL`, `REG_WRITE`, `MEM_READ`, `MEM_WRITE`
- `WRITEBACK_SEL[1:0]`, `MEM_SIZE[1:0]`, `LOAD_SIGN_EXTEND`
- `BRANCH_TYPE[2:0]`, `JUMP_TYPE[1:0]`
- `HALT_REQUEST`, `TRAP_REQUEST`, `DECODE_TRAP_CAUSE[3:0]`

## Verification

`RV32IDecodeControlUnitTest` runs each fidelity in its own simulator with the
same instruction sequence. At every checkpoint it checks every public output
against the answer-sheet expectation and directly compares the structural and
behavioral snapshots.

1. Each fidelity is checked against `RV32IDecoder` and `RV32IControl`.
2. `RV32IDecoderTest` and `RV32IControlTest` independently check that answer
   sheet with explicit instruction expectations.
3. Every public output bit vector is compared directly between the structural
   and behavioral components.

Coverage includes:

- all 10 register-register ALU instructions;
- all 9 immediate ALU instructions;
- 5 loads and 3 stores;
- all 6 branches;
- `JAL`, `JALR`, `LUI`, `AUIPC`, `FENCE`, `ECALL`, and `EBREAK`;
- twelve directed illegal cases covering instruction length, unknown and RV64
  opcodes, invalid ALU/shift `funct7`, invalid branch/load/store/JALR `funct3`,
  `FENCE.I`, CSR, and unsupported SYSTEM encodings;
- 64 deterministic pseudo-random raw instruction words, which exercise both legal recognizers and safe illegal defaults.

The scenario therefore records 116 settled checkpoints: 40 legal instructions,
12 directed illegal instructions, and 64 deterministic raw words.

Command and result:

```text
ctest --test-dir build -R '^RV32IDecodeControlUnitTest$' --output-on-failure
RV32IDecodeControlUnitTest ... Passed
```

## Boundary

The proven equivalence contract currently covers fully known binary instruction words. The behavioral component deliberately returns unknown outputs when any instruction input bit is unknown. The structural gates naturally propagate unknowns only through affected logic, so partial-unknown equivalence is not claimed yet.
