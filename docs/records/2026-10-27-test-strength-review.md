# Test strength review

Started: 2026-10-27. Status: open. Code state: `alpha` at `5549ee0`, with the
existing uncommitted component, visualizer and documentation changes preserved.

## Summary

Read-only audit of the 464 registered simulation tests (288 named tests plus
176 program/policy variants) for vector count, reference independence, width
and fidelity coverage. Core programs, memories, the 8-bit ALU family, gates,
muxes, decoders and multiply/divide steps are well covered. The weak spots are
the 16/32-bit word arithmetic outside the cores' own ALU, fully structural
multiply/divide, and several small RV32I control blocks and bus routers.
`ParallelPrefixAdderWidth16Test` has 8 cases, and the same 8-row builder serves
`RippleCarryAdderWidth16Test`. The plan below adds headless sweep tests in four
phases and leaves every published example table unchanged. Phase 1 is
implemented: shared sweep equipment, `AdderWidthSweepTest`, a 16/32-bit grid in
`ALUWidthContractTest` and `RV32IALUSweepTest`, each shown to catch a
deliberate fault the old tests miss; a pre-existing catalog ownership conflict
is fixed. Phases 2–4 remain.

## 2026-10-27 — Method and limits

Sources read: `docs/guides/tests-and-scenarios.md`,
`docs/reference/test-organization.md`, `src/tests/TestRegistry.cpp`,
`tests/CMakeLists.txt` and the test sources under `src/tests/`. Case counts come
from the source loops. Published row counts come from the 24 October Pages
export `build-wasm/pages/simulator/example-rows.json` (132 scenario groups),
which may lag current source slightly. Runtimes are CTest averages from
`build-milestone/Testing/Temporary/CTestCostData.txt`, which mixes older test
names; treat them as rough estimates. The only command run was the cheap
`build-milestone/tests/SimulationTestRunner --list` (464 names). No build, CTest
run or test execution was performed. Timing and setup/hold checks (delay-rule
and part-delay-table tests) are outside this review. Settled rows check final
values only.

How the harnesses work: `ComponentScenarioTest` runs each scenario at every
root fidelity the catalog offers and compares them
(`src/tests/ComponentTestModel.cpp:420-438`). A test constructed without a
scenario ID runs all of its scenarios (`:542-553`), while site tables export
only the scenario named in the `simulator-examples` marker. A component
without a behavioral form is therefore compared only against its hand-written
expectations.

## 2026-10-27 — Findings by area

### Very weak

| Test | Cases | Evidence | Note |
| --- | ---: | --- | --- |
| ParallelPrefixAdderWidth16Test | 8 | `src/tests/WordALUTests.cpp:145-157` | Operands {0,0},{FFFF,1},{7FFF,1},{1234,4321} × C_IN. No carry-chain spans, no X/Z. Feeds a site table (8 rows). |
| RippleCarryAdderWidth16Test | 8 | same builder, `:168-173` | Same rows. Not on the site. |
| ROMCellTest / ROMRowTest | 1 / 1 | `src/tests/MemoryDeviceTests.cpp:132-141` | One parameter value only (`value=1`, `contents=1010`). Low risk: trivial parts, and `MemoryDeviceContractTest` compares fidelities. |
| RV32IForwardingUnitTest | 7 | `src/tests/RV32IPipelineBlockTests.cpp:195-247` | Small decision space that could be enumerated in full. Only programs and differential runs cover the rest. |
| RV32IHazardDetectionUnitTest | 7 | `src/tests/RV32IPipelineBlockTests.cpp:284` | Same pattern as the forwarding unit. |
| AddressRouter / InstructionAccessGuard / BusErrorTarget scenarios | 3 each | site export | `routing()` builds compact only (`src/tests/RV32IMPhase3Tests.cpp:364-365`) and checks one target region plus one fault. |

How much the 16-bit prefix adder is checked across all tests:
- About 78 binary vectors are checked against an independent reference: the 8
  rows above, 56 arithmetic rows in `ALUWidthContractTest`'s ALU<16> (4 operand
  pairs × 7 add/sub ops × 2 C_IN), and 14 in `ALUScenarioTest`.
- `ALUFidelityContractTest` runs 1,024 random vectors (about 341 of them
  binary) plus 132 single-bit X/Z vectors (`src/tests/WordFidelityTests.cpp:109-120`).
  These compare the structural and behavioral forms with each other only, not
  with a + b.
- Carry patterns (2^k − 1) + 1, which exercise each prefix span, are tested only
  at 32 bits (`src/tests/RV32IALUTests.cpp:410-413`).

### Weak

- **ALU<16> and ALU<32>, structural.** `ALUWidthContractTest` checks 4 operand
  pairs × 32 ops × 2 C_IN = 256 rows, plus 160 shift rows and 30 X/Z rows, so
  446 per width (`WordALUTests.cpp:98-117`). ALU<8> by comparison gets 5,184
  edge-grid rows, 2,048 random rows and exhaustive AddSub/shifter checks
  (`src/tests/ALUUnitTests.cpp:201-280`).
- **16/32-bit ALU sub-units.** The 16/32-bit AddSub, Arithmetic, Logic, Shift,
  Compare, Flags and Result units have no standalone test. Their declared
  verification owner is `WidthComponentsTest`
  (`src/modules/composite/WordDatapath.cpp:111-115`), which checks ALU<W> with
  compact adder children on the same 4 pairs (`RV32IMPhase3Tests.cpp:565-596`).
  These units have no behavioral form at 16/32 bits
  (`src/modules/composite/WordBehavioral.cpp:108-114`), so none is left
  untested. Their arithmetic is just thinly sampled.
- **RV32IALUTest.** It has 46 rows: 27 directed plus 19 reserved opcodes
  (`RV32IALUTests.cpp:643-678`), with no random sweep. Every core uses this ALU,
  but program, lockstep and differential tests cover it indirectly.
- **Multiply/divide units.**
  - `MultiplierWidth32Test`, `DividerWidth32Test` and `MulDivUnitWidth32Test`
    run 48 operand pairs per operation against the independent
    `rv32im::referenceResult`. They use the compact profile
    (`RV32IMPhase3Tests.cpp:30-40,104`), so MulDivUnit<32> runs its
    behavioral root, and Multiplier/Divider run a structural root with compact
    word children.
  - The gate-level forms get 5–10 transactions in the scenario tests. Those run
    under `rv32imInspectionProfile`, which also keeps the word parts compact.
    The rest comes from comparing the gate-level and compact cores in
    `RV32IMFiveStageCoreTest`.
  - The 4/8/16-bit M units get 3 pairs × 8 ops (`RV32IMPhase3Tests.cpp:632-647`).
- **RV32IExecuteStageTest.** It has 7 rows. The ALU and branch logic inside it
  are covered elsewhere.

### Adequate or strong

- **Gates, half adder, full adder.** Exhaustive four-state: 16, 16 and 64 rows
  (`CorePrimitiveTests.cpp:96-110`, `ArithmeticLogicTests.cpp` half/full
  adder rows).
- **Muxes and decoders.** Every select value plus X/Z select and data rows. A
  generic sweep checks 25 mux shapes (structural against compact) and
  exhaustive 1–5-bit decoders (`RV32IMPhase3Tests.cpp:602-630`).
- **8-bit ALU family.**
  - Exhaustive AddSub<8> in both fidelities: 262,144 vectors.
  - Exhaustive shifter: 20,480 vectors.
  - ALU<8>: edge grid, random and X/Z isolation.
  - Fidelity equivalence for 16 families, at least 1,156 vectors each, with an
    exhaustive 65,536-vector four-state zero detector.
- **32-bit prefix adder.** 44 rows including 31 carry chains, plus
  RV32IALUAddSub (80 rows).
- **Multiply/divide steps.** Exhaustive at 4 bits (512) plus 128 at each wider
  width, in both forms (`RV32IMPhase3Tests.cpp:85-101`).
- **Memories.**
  - Device contract tests sweep 3 widths × 3 geometries plus mixed fidelity
    (`MemoryDeviceTests.cpp:427-445`).
  - Random 24–32-operation sequences are checked against reference arrays.
  - Endpoints have 243–818 rows.
  - The cache is checked in lockstep against reference replacement models.
- **FIFO.** Depth 1–7 × 2 widths × 128 edges, structural against compact.
- **Pixel selector.** Exhaustive 256 × 4.
- **Cores.**
  - 22 programs × 8 core/predictor configurations, each against the oracle.
  - Instruction lockstep.
  - Bounded random-ELF differential checks (`tests/checks/RV32IDifferentialTest.py`).
  - Recorded games against host references.
- **Latches and flip-flops.** Value sequences only: 8, 12 and 15 steps.

## 2026-10-27 — Improvement plan

General approach:
- Add headless `StandaloneVerificationTest` entries, registered with
  `visualizable=false` like `appendWordALUTests`. Do not grow the scenarios that
  site markers reference.
- Each new test compares the strict-structural form with the behavioral form
  (when one exists) and with an independent integer or reference model. It
  reports its vector count.
- Use fixed seeds. Use `drainUntilIdle` with history off.
- Check that a new test can catch a fault: introduce one deliberate local defect
  (for example, a wrong prefix span or a missed forwarding priority), confirm
  the new test fails while the old one passes, then revert. Do not commit the
  defect.

Shared equipment. The same settle harness exists four times under different
names:
- `Harness` (`ALUUnitTests.cpp:137-199`)
- `GateFixture` (`WordALUTests.cpp:76-96`)
- `Probe` (`WordFidelityTests.cpp:26-64`)
- `Rig` (`RV32IMPhase3Tests.cpp:35-80`)

Add one `include/tests/SettledSweep.hpp` covering: build a family under a
profile, drive named inputs, settle, read outputs, attach an optional
fidelity peer, inject X/Z per bit and count vectors. Reuse the existing
independent references: `expected()` in `WordALUTests.cpp`, `aluExpected` in
`ALUUnitTests.cpp`, `aluExpectedOut` in `RV32IALUTests.cpp`,
`rv32im::referenceResult`. Do not use production `alu_value` functions as the
expected output. Migrate the existing tests separately, later.

Runtime estimates, from the cost data above (-O1 build):
- ALU<16>/<32> in `ALUWidthContractTest`: about 3.5 ms per vector, averaged
  including setup.
- 8-bit word units in `ALUFidelityContractTest`: about 0.5 ms per vector.
- MulDiv<32>, compact: about 65 ms per transaction.
- Full CTest: about 12,900 s of averaged time, dominated by game recordings. The
  phases below add well under 1% to that.

Phase 1, arithmetic used by the cores (highest priority):
1. `AdderWidthSweepTest` for ripple and prefix adders at 8, 16 and 32 bits. It
   covers:
   - a carry chain (2^k − 1) + 1 for every k, at both C_IN values;
   - all-ones + 0/1;
   - 0x55…/0xAA… patterns and sign boundaries;
   - 4,096 seeded random binary vectors;
   - a single X/Z on every input bit.

   Results are checked against a + b + C_IN and against the behavioral form.
   Estimate: under 15 s in total. Exhaustive 8-bit (131,072 vectors × 2 forms ×
   2 adders) is optional, under the `slow` label.
2. Extend `ALUWidthContractTest` at 16 and 32 bits:
   - a 6 × 6 edge grid on the 14 arithmetic and compare ops × 2 C_IN (1,008
     vectors);
   - 512 random vectors across all ops.

   About 5 s more per width. Point the 16/32-bit owners in
   `WordDatapath.cpp:111-115` at tests that actually exercise those units.
3. `RV32IALUSweepTest`: 1,024 random vectors plus an edge grid against
   `aluExpectedOut`, in both forms. About 3 s.

Phase 2, multiply and divide:
- A strict-structural transaction check for Multiplier, Divider and MulDiv<32>
  (about 16 directed sign/zero/overflow transactions, costed before adoption).
- An exhaustive 4-bit M unit as the representative gate-level slice:
  256 pairs × 8 ops, labelled `slow` if it exceeds about 30 s.
- 4/8/16-bit random pairs against a width-generic reference.

Phase 3, control and bus blocks:
- Enumerate the forwarding unit: rs/rd from {0, 1, 2} × all valid/write/ready
  flags, about 2,600 vectors.
- Enumerate the hazard unit: about 1,000 vectors.
- Both are checked against a small independent C++ rule.
- Random execute-stage rows.
- Every router region, the guard and the error target, in both forms.

Phase 4, tidy:
- ROM cell `value=0`; several ROM row contents.
- Add these through `MemoryDeviceContractTest`, not the published scenarios.

Keep the published tables and the visualizer unchanged:
- Leave the `contract`/`rows` scenarios named by site markers untouched.
- Register the new tests in `TestRegistry.cpp` and `tests/CMakeLists.txt`.
  `TestRegistryCoverageTest` guards completeness.
- Verify with `DocumentationExamplesTest` and an isolated regeneration of the
  affected example groups. The output must be byte-identical.
- The WASM registry will list the new names but not show them in the menu.
- Update the tests-and-scenarios guide and the adding-a-component guide so that
  each registered width needs a sweep against an independent reference.

Execution constraints: build serially. Rebuild only the test targets. Run new
tests individually before any full suite. One heavy job at a time.

## 2026-10-27 — Phase 1 implemented: adder, ALU and RV32IALU sweeps

Owner direction: start with the test sweeps, beware of OOM, avoid long tests
and run only the necessary checks.

Changes:

- `include/tests/SettledSweep.hpp` / `src/tests/SettledSweep.cpp`: shared
  `SettledSweep` (build one family under a profile, drive all inputs, settle,
  read all outputs) and `FidelitySweep` (strict gate-level form plus the
  behavioral form when present). With a reference, the gate-level outputs are
  checked against it before the forms are compared, so a wrong circuit is not
  reported as a form mismatch. `requireSoundUnknown` allows pessimistic `X` but
  rejects a known output that one unknown input bit could change. The four
  older harnesses are not migrated yet.
- `AdderWidthSweepTest` (`src/tests/ArithmeticSweepTests.cpp`): ripple and
  prefix adders at 8/16/32 bits. 1,225 / 1,397 / 1,933 binary vectors per
  adder (carry chains of every length both ways, a carry span of every length
  at every start position, patterns, 1,024 seeded random), against
  `A + B + C_IN` and `C_SIGN`; plus 68 / 132 / 260 single-X/Z vectors that also
  require sum bits below the unknown bit to stay exact. 7.0–7.4 s.
- `ALUWidthContractTest`: at 16/32 bits adds a 5 × 5 boundary grid on the 14
  arithmetic and compare codes (both carry-ins only for the two codes that read
  carry) and 128 seeded random vectors over all 32 codes: 528 vectors per width
  on the strict gate-level ALU. 21–26 s, up from about 4.8 s.
- `RV32IALUSweepTest` (`src/tests/RV32IALUTests.cpp`): a 6 × 6 boundary grid on
  all 13 operations and 256 seeded random vectors over all 32 codes, 724
  vectors, against C++ integer results on both forms. 5.7–6.6 s.
- The first draft used larger sets (512 ALU random, 8 × 8 grid and 1,024
  random for `RV32IALU`) and took 51 s and 19.6 s; it was reduced for the
  owner's runtime constraint. Both drafts passed on correct code.

Fault checks (deliberate local faults, reverted; no marker remains in source):

| Fault | Old tests | New tests |
| --- | --- | --- |
| Prefix adder: skip the combine cell at distance 4, bit 9 (16/32 bits) | `ParallelPrefixAdderWidth16Test`, `RV32IALUTest`, `ALUScenarioTest` pass | `AdderWidthSweepTest` and `ALUWidthContractTest` fail; `RV32IALUSweepTest` passes (its random vectors rarely hit that span; the shared adder is the adder sweep's job) |
| RV32IALU logic: XOR bit 13 takes the OR result | `RV32IALUTest` passes; unit-level `RV32IALULogicTest` fails | `RV32IALUSweepTest` fails, reporting the gate-level form against the reference |

Catalog ownership correction. `BuiltinComponentCatalogInventoryTest` failed on
`word.alu.8` (and, by the same rule, the 16-bit adders): the catalog named
`ALUFidelityContractTest` while the recorded scenarios for those contracts are
grouped under `ALUWidthContractTest`. This predates this phase (the check was
last recorded on 2026-10-20, before the ALU interface change); `rv32i.alu`
surfaced first only because the first draft registered the new sweep with a
contract. `WordDatapath.cpp` now separates `contractOwner` (one logical owner
per contract) from `equivalenceOwner` (the test comparing forms):
`ALUWidthContractTest` owns `ALU<W>`, the 16-bit adders and the 16/32-bit
sub-units; `AdderWidthSweepTest` owns the 32-bit ripple adder and is the
equivalence evidence for the 16/32-bit adders; `ALUFidelityContractTest` stays
the equivalence evidence for `ALU<8>`. The sweeps declare no contract.

Verification (build `build-milestone`, `-O1`, Python deploy off; each build and
run alone under a user scope with `MemoryMax` 2 GiB for builds and 1–1.5 GiB for
tests, zero swap; incremental builds 39 s or less):

- The three sweeps pass on correct code with the counts above.
- 15 focused CTests pass: `ALUWidthContractTest`, `ALUScenarioTest`,
  `ALUWidth8ScenarioTest`, `ALUWidth32ScenarioTest`, both 16-bit adder
  scenarios, `ParallelPrefixAdderWidth32Test`, `RippleCarryAdderWidth8Test`,
  `ZeroDetectWidth32Test`, `ALUHierarchyTest`, `RV32IALUTest`,
  `WidthComponentsTest`, `BuiltinComponentCatalogInventoryTest`,
  `TestRegistryCoverageTest`, `ComponentCatalogSelectionTest`.
- `DocumentationExamplesTest` passed (42.7 s) after the site test-table edits,
  before the ownership correction; the pages did not change afterwards.

Not run: the full native suite, `ALUFidelityContractTest` and
`ALUContractTest` (code unchanged; only catalog metadata changed), WASM and
browser checks, and the strict MkDocs build. The strict build was skipped
deliberately: the site would copy the 1.9 GB of untracked gameplay demos with
3.4 GB of disk free. Site edits are new test-table rows on `alu/2.1.md`,
`alu/2.6.md` and `rv32i/4.3/alu.md`; no example marker or table changed.

Next: phase 2 (gate-level multiply/divide and the 4-bit M unit), then phase 3
(forwarding/hazard enumeration; the router part is deferred with Device I/O).

## 2026-10-27 — Phases 2–4 implemented

- `MulDivGateLevelTest` (`src/tests/RV32IMPhase3Tests.cpp`, label `slow`,
  about 44 s): the fully gate-level M units against a width-generic reference
  written in the test (`referenceM`). Exhaustive `MulDivUnit<4>` (2,048
  transactions) on the gate-level form (18 s) and the compact form (4 s);
  `MulDivUnit<8>` with 10 boundary and 6 seeded random pairs × 8 operations;
  `MulDivUnit<16>` with the 10 boundary pairs (each 16-bit transaction costs
  about 0.12 s, so random pairs stay at 8 bits); and `Multiplier<32>`,
  `Divider<32>`, `MulDivUnit<32>` through the existing `arithmetic()` check
  (two boundary pairs, then its enable/cancel/pause/reset control checks)
  with strict structural selection. The first draft with random 16-bit pairs
  took 52.7 s and passed.
- Correction to the review above: `MulDivUnitWidth32Test` exercises the
  structural 32-bit result groups (its compact profile keeps compact word
  children, not a behavioral root). A deliberate fault in
  `DivideResultCorrection` (quotient sign from `NEGATIVE_B`) made both
  `MulDivUnitWidth32Test` and `MulDivGateLevelTest` fail; `WidthComponentsTest`
  (4/8/16 bits, compact) passed. Reverted.
- `RV32IPipelineControlSweepTest` (`src/tests/PipelineControlSweepTests.cpp`,
  0.9 s): forwarding unit 8,502 vectors (registers x0/x1/x17/x30 in all four
  positions × 32 flag combinations, plus every one-bit register difference),
  hazard unit 2,358 vectors, against independent priority and load-use rules
  on both forms. A deliberate fault (MEM comparator ignores register bit 4)
  passed `RV32IForwardingUnitTest` and failed the sweep. Reverted.
- ROM extras in `MemoryDeviceContractTest`: `ROMCell` `value=0` and `ROMRow`
  contents `0000`, `1111`, `0110`, `1001` on both forms. The recorded
  `ROMCellTest`/`ROMRowTest` scenarios are unchanged.
- The router/guard/error-target part of phase 3 is deferred with the Device
  I/O work by the owner's direction.

Verification: each build/run alone under memory-capped user scopes; passing
CTests `MulDivGateLevelTest`, `MulDivUnitWidth32Test`,
`RV32IPipelineControlSweepTest`, `RV32IForwardingUnitTest`,
`RV32IHazardDetectionUnitTest`, `MemoryDeviceContractTest` (13.9 s on a loaded
host; cost data averaged 5.0 s before), `ROMCellTest`, `ROMRowTest`,
`TestRegistryCoverageTest`, `BuiltinComponentCatalogInventoryTest`. Site pages
(M units, forwarding, hazards, ROM) gained test descriptions only. Not run:
full suite, WASM/browser, strict MkDocs (disk headroom).

Phase status: 1–4 done except the deferred bus parts. The old harnesses
(`Harness`, `GateFixture`, `Probe`, `Rig`) are not yet migrated to the shared
helper; that is optional cleanup. This record stays open for that migration
and the deferred bus sweeps.
