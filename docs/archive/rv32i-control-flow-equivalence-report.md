# RV32I Control-Flow Equivalence Report

Last updated: 2026-07-19

## Status

Block 1 is implemented and its focused equivalence test passes.

Implemented components:

- structural `RV32IControlFlowUnit`
- behavioral reference the behavioral fidelity of `RV32IControlFlowUnit`

Both components expose the same pins. `RV32IControlFlowUnitEquivalenceTest` runs the same vectors in two isolated simulators and checks each trace against independent expected values.

## Structural Implementation

`RV32IControlFlowUnit` is an expandable `IOComponent`. Its visible children include:

- `Register32` for PC state
- three `Adder32` components for `PC+4`, `PC+IMM`, and `RS1+IMM`
- structural branch-condition muxing and inversion gates
- 32-bit branch/jump muxes
- `Rewire` for the JALR bit-zero mask
- bit splitters and gates for current-PC and selected-target alignment

The unit does not decide whether a trap should commit. It reports `PC_MISALIGNED` and `TARGET_MISALIGNED`; the later execution-control/status block supplies final `PC_WRITE` permission.

## Behavioral Reference

the behavioral fidelity of `RV32IControlFlowUnit` owns only its compact PC state and direct control-flow calculation. It is not instantiated inside the structural component.

Its known-value behavior matches the structural contract:

- reset PC to zero
- update only on a rising edge with `PC_WRITE=HIGH`
- calculate sequential, branch, JAL, and JALR candidates
- clear JALR target bit zero
- ignore an unused target for a non-taken branch
- expose current-PC and selected-target misalignment

## Verification

Focused command:

```bash
ctest --test-dir build -R '^RV32IControlFlowUnitEquivalenceTest$' --output-on-failure
```

Verified scenarios:

- reset and reset recovery
- normal `PC+4`
- `PC_WRITE=LOW` hold
- taken and non-taken branches
- all six branch conditions
- JAL
- aligned and misaligned JALR candidates
- JALR bit-zero clearing
- a deliberately written misaligned PC
- direct structural/behavioral comparison at every checkpoint
- 64 deterministic randomized combinations of branch/jump type, comparison flags, immediate, and rs1 value

Result: pass.

## Known Boundary

The focused equivalence contract currently grades binary-known inputs. Four-state partial-unknown equivalence is not yet claimed; that policy should be finalized before these components are promoted as interchangeable fast/product configurations.
