# RV32I Mixed-Fidelity Toggle Audit

Date: 2026-07-28

## Goal

Verify that every profile-selectable component in the structural
`RV32ISingleCycleSystem` tree can be replaced by its behavioral fidelity while
its structural parents and the rest of the circuit continue to run correctly.

This is a contract-wide exhaustive sweep, not an enumeration of every possible
combination. The canonical structural tree contains 1,077 selectable instances,
so there are `2^1077` possible structural/behavioral combinations. Instead, the
test discovers every selectable contract in the built tree and, once per
contract, switches every instance of that contract to behavioral. It then runs
an instruction-by-instruction comparison against the RV32I answer sheet.

## Permanent regression test

`RV32IProfileToggleSweepTest` builds the canonical structural system, discovers
its selectable component inventory, and runs 15 mixed-fidelity scenarios. It
also verifies that:

- every discovered selectable contract has an audit scenario;
- the profile selected the expected number of instances;
- each target instance actually became behavioral;
- no target silently used an unavailable-fidelity fallback; and
- every architectural checkpoint and memory observation matches the answer
  sheet.

| Selectable contract | Instances | Program context |
|---|---:|---|
| `RV32ISingleCycleSystem` | 1 | Whole system |
| `RV32ISingleCycleCore` | 1 | Core inside a structural system |
| `RV32IControlFlowUnit` | 1 | Jump-and-link control flow |
| `RV32IDecodeControlUnit` | 1 | R-type and I-type decoding |
| `RV32IExecutionControlStatusUnit` | 1 | Illegal-instruction trap state |
| `RegisterFile32x32` | 1 | Fibonacci register dependencies |
| `Register32` | 32 | Register words and program counter |
| `MemoryBit` | 1,030 | Register, PC, halt, and trap-status cells |
| `ALU32` | 1 | Complete ALU operation coverage |
| `AddSub32` | 3 | Addition and subtraction |
| `Logic32` | 1 | Bitwise logic |
| `Shifter32` | 1 | Logical and arithmetic shifts |
| `Comparator32` | 1 | Taken and not-taken branches |
| `ZeroDetect32` | 2 | Equality and result-zero detection |
| **Total** | **1,077** | **14 contracts, 15 runs** |

`MemoryBit` intentionally has two scenarios because its instances occur in both
ordinary register/PC storage and halt/trap status storage.

## Bugs found and fixed

### Behavioral system did not expose memory-write history

The behavioral whole-system run executed stores correctly, but its
fidelity-independent observation API returned no written bytes. Direct
behavioral memory writes updated byte history without updating the memory
component's bus-write history.

The behavioral system now records its direct byte writes with timestamps and
implements the same exclusive-start, inclusive-end range query used by the
structural system.

### Behavioral core rejected transient unknown instruction bits

Inside a structural parent, instruction-memory data is temporarily unknown
during reset and event settling. The behavioral core tried to convert and
decode those bits immediately, throwing an exception before memory became
ready.

The behavioral core now previews an instruction only when all instruction bits
are known. While they are unknown, requests remain inactive and its public data
path outputs carry the same four-state unknown values as the structural core.
Known data-memory input is still required when an actual load response is
consumed.

## Validation

- `RV32IProfileToggleSweepTest`: passed all 14 contracts, 1,077 instances, and
  15 mixed-fidelity runs.
- Selectable component contract tests: 13/13 passed.
- RV32I single-cycle programs: 16/16 passed.
- `TestRegistryCoverageTest`: passed.
- `VisualizerModuleBindingsTest`: passed.
- `VisualizerProfileStoreTest`: passed.
- Complete repository regression: 127/127 passed in 535.15 seconds.
- `git diff --check`: passed.

The sweep is registered as a slow RV32I integration test, so CTest will keep
checking new selectable contracts and mixed-fidelity behavior in future work.
