# Workload Correctness Tests

[Verification index](index.md)

## Fixed programs and ISA references

| Test | Inputs | Checks |
| --- | --- | --- |
| `RV32IInstructionOracleTest` | Instruction-level semantic cases | Register/PC/memory effects, terminal status and fault behavior |
| `RV32IInstructionLockstepHarnessTest` | Structural execution with oracle packets | Per-attempt architectural comparison |
| `RV32IInstructionLockstepMismatchDetectionTest` | Deliberately mismatching retirement information | Harness rejects discrepancies rather than silently accepting them |
| `RV32ISingleCycleSystemTest` | 22 fixed images and their explicit expectations | Expected final registers, memory writes, PC, halt/trap state and instruction attempts |
| `RV32IReferenceDocumentationTest` | Published instruction/program tables and compiled probes | Documentation matches executable semantics and fixture images |

The reference-documentation test checks 40 instruction encoding rows, all 128×8×128 opcode/funct3/funct7 selector combinations plus seeded words, the 40-row one-step answer sheet, and 14 boundary rows. It also checks the 22 program pages, their complete assembly/data/expected state, and exported checkpoint metadata. The fixed corpus contains 650 instruction attempts including terminal attempts; this is not a cycle count shared by all cores.

The native fixed fixture supplies explicit expected final values. The general experiment runner additionally/independently uses its shared instruction-oracle path; it does not transplant every fixture assertion into the common run record. The program pages retain embedded single-cycle recordings, whose checkpoint times are not benchmark clock measurements.

**Sources:** `src/tests/RV32IInstructionOracleTests.cpp`, `src/tests/RV32IInstructionLockstepTests.cpp`, `src/tests/RV32ISingleCycleTests.cpp`, `include/tests/RV32IProgramCases.hpp`, `tests/RV32IReferenceDocumentationTest.py`.

## RV32IBenchmarkTest

**Inputs:** all 12 parameterized generators, sizes 1/4/16/256, default and alternate seed, trained body-window cases, dependency distances 2/4/8, branch patterns crossing the 32-entry ring, and alias spacings 4/8/32/256.

**Checks:**

- Regenerating a case produces identical machine words.
- Body markers are ordered and visited for every training/measured iteration.
- Oracle execution reaches normal halt and x10 equals the independent host result.
- Expected output byte blocks match; calls restore sp; alias branch PCs have the requested spacing.
- Immediate/scheduled load variants have the same complete instruction multiset and result.
- Unknown family, invalid bounds, illegal distance/seed, and invalid training combinations are rejected.

**Boundary:** this runs emitted code in the semantic oracle, not a structural CPU. It is the algorithm/intended-work check underneath later core comparisons.

**Source:** `tests/RV32IBenchmarkTest.cpp`.

## RV32IBenchmarkCLITest

**Inputs:** small size-4/two-iteration cases for all 12 kernels on each of three cores in both fidelities, paired load schedules, whole/body/trained regions, invalid options, and a small two-target campaign.

**Checks:** reference and compact cycles, observations, retirement counts, image/final-state fingerprints, and checksums agree. Across core kinds the architectural fingerprints agree even when cycles differ. In the selected advanced load pair, 32 immediate load-use stalls become zero after scheduling, with 32 fewer cycles and identical work.

The region checks compare seven whole iterations with four training plus three measured iterations: same image and final state, smaller measured window, no terminal attempt in the window, and matching reference/behavioral observations. Prefix mode must report incomplete and not validated. The suite checks the 37-entry catalogue and 15-/23-case preset inventories, plus successful campaign artifacts.

**Boundary:** these are aggregate comparisons. Exact cycle placement is covered by [RV32IDifferentialTest](equivalence.md).

**Source:** `tests/RV32IBenchmarkCLITest.py`.

## ELF loading and external verdict

**RV32IElfLoaderTest:** synthetic valid/malformed ELF images check header, segment, BSS, entry and memory limits. Invalid format/ranges must fail before execution. Source: `src/tests/RV32IElfTests.cpp`.

**RV32IExternalValidationSmokeTest:** the checked-in [assembly smoke image](../compiled/smoke.md) exercises external loading and self-checking execution, using the existing core/fidelity combinations in its fixture. Its pass/fail result comes from the image rather than a prefilled oracle answer. Source: `src/tests/RV32IExternalValidationTests.cpp`.

**Compiled C:** the separate [compiled fixture](../compiled/compiled-equivalence.md) is checked completely by RV32IDifferentialTest, including ELF checksum and both profiles on each core.

**External Embench:** upstream verifier checks are described per [workload](../compiled/index.md). Availability in the source catalogue is not automatic CTest coverage or evidence of a successful full run.

**RV32IMiddleBenchmarkTest:** checks the [middle-suite](../compiled/middle-equivalence.md) inventory and preset, rejects full-profile/missing/duplicate/modified images, and ensures the common driver requests structural reference checks. With `--artifacts`, it also executes each complete ELF in the instruction reference, requiring its original verifier to pass within 200,000 instructions. It does not itself run the long structural campaign; that is the explicit `compiled-middle` preset.
