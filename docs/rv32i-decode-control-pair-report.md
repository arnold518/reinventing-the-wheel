# RV32I Decode/Control Pair Report

## Outcome

Block 2 now has two circuit components with the same external contract:

- `RV32IDecodeControlUnit`: a structural, expandable implementation.
- `BehavioralRV32IDecodeControlUnit`: a compact reference implementation backed by `RV32IDecoder` and `RV32IControl`.

The focused pair test passes for every instruction currently supported by the behavioral RV32I answer sheet.

## Structural implementation

The structural decoder does not call the behavioral decoder or control library. Its visible internals are built from existing simulator components:

- `Rewire` extracts `rs1`, `rs2`, and `rd` and forms I-, S-, B-, U-, and J-type immediates.
- `RV32IBitPatternMatcher` splits the 32-bit instruction and compares masked bits with NOT and AND gates.
- Forty instruction recognizers cover the supported RV32I instruction set.
- Gate-level OR networks combine recognizer outputs into each control bit.
- `BitJoiner` components assemble multi-bit controls.
- `Mux8to1_32bit` selects the immediate required by the recognized format.
- Constants provide safe defaults for illegal instructions.

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

`RV32IDecodeControlUnitPairTest` drives the same instruction word into both implementations. At every row it performs two checks:

1. Each implementation is checked against `RV32IDecoder` and `RV32IControl`.
2. Every output bit vector is compared directly between the structural and behavioral components.

Coverage includes:

- all 10 register-register ALU instructions;
- all 9 immediate ALU instructions;
- 5 loads and 3 stores;
- all 6 branches;
- `JAL`, `JALR`, `LUI`, `AUIPC`, `FENCE`, `ECALL`, and `EBREAK`;
- invalid instruction length, unknown opcode, invalid JALR `funct3`, invalid R-type `funct7`, unsupported `FENCE.I`, and an RV64-only opcode.
- 64 deterministic pseudo-random raw instruction words, which exercise both legal recognizers and safe illegal defaults.

Command and result:

```text
ctest --test-dir build -R '^RV32IDecodeControlUnitPairTest$' --output-on-failure
RV32IDecodeControlUnitPairTest ... Passed
```

## Boundary

The proven equivalence contract currently covers fully known binary instruction words. The behavioral component deliberately returns unknown outputs when any instruction input bit is unknown. The structural gates naturally propagate unknowns only through affected logic, so partial-unknown equivalence is not claimed yet.
