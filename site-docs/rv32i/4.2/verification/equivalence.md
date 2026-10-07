# Structural and Behavioral Equivalence

[Verification index](index.md) · [Fidelity definitions](../execution.md#fidelity)

## Comparison boundary

Compare two implementations of the **same core/configuration** with the same image, initialized memory, policy/table size, and certified period. Do not require different microarchitectures to have identical cycles or port traffic.

Reference expands the CPU datapath but retains selected compact storage abstractions. Thus the tests establish agreement with that reference profile, supported by separate lower-level contract tests; they do not claim every FF and memory bit is expanded in every run.

## RV32IDifferentialTest

| Case family | Configurations | Paired comparisons |
| --- | --- | --- |
| Seeded instruction sequences | Four seeds × three cores | 12 |
| Fault priority | Fixed programs 9, 11, 12, 13, 14, 15, 16 × three cores | 21 |
| Predictor history and aliasing | Five policies × bursts and small-table alias cases | 10 |
| Compiled stack/call fixture | Complete C image × three cores | 3 |
| **Total** | Each pair runs both fidelities | **46** |

The generated ELF sequences cover boundary operand values, all ALU operation kinds, dependency spacing, competing forwarding producers, byte/halfword lanes, signed/unsigned loads, x0, load-to-store, a load-dependent branch, wrong-path stores/illegal instructions, and JAL/JALR redirects including an odd target. Four seeds vary operands/scheduling; they are not exhaustive fuzzing.

Fault cases check precise terminal behavior. On misaligned accesses the advanced core suppresses the request; single-cycle/baseline may probe an aligned read while checking alignment locally. Those legal per-core differences are preserved. A rejected store must never become an active write.

The branch panel uses bursts long enough to exercise predictor counter changes, and opposing branches with a four-entry alias setup. The compiled fixture checks real compiler-generated calls/stack behavior, not merely hand-encoded instruction sequences.

## Trace contract

The test compares every settled cycle, including bubbles:

| Trace group | Compared information |
| --- | --- |
| Pre-edge control | Stall, flush, predictor/control observations exposed by that core |
| Stage state | Valid bits and active stage PCs |
| Memory interface | Request/response handshake and active operation/address/status/payload |
| Store data | Only selected byte lanes; irrelevant disabled lanes are excluded |
| Post-edge retirement | Attempt-valid, retired PC and architectural register/PC/status state |

Inactive payload is unspecified and omitted. Active X/Z is rejected rather than silently converted to binary. Terminal halt/trap attempts appear as retire.valid entries even though they are excluded from the successful-retirement metric.

After trace equality, aggregate signatures must also match. A separate negative test moves a retirement to the wrong cycle while preserving totals, and changes a memory payload; both must be detected. Trace-size/option validation and incomplete-prefix behavior are tested too. Enabling tracing must not alter simulated event counts.

Each subprocess has at most 1024 cycles, an event budget, a 90-second timeout, and a 512 MiB address-space cap. The runner's general trace limit is 4096 cycles. Processes are serial and no visualizer history is retained.

**Source:** `tests/RV32IDifferentialTest.py`; trace capture in `src/analysis/RV32IExperiment.cpp`.

## RV32IProfileToggleSweepTest

This audit derives the selectable contract inventory from the canonical RV32I tree and checks that the declared cases cover exactly that inventory before executing any program. Each case builds a mixed-fidelity profile, executes an appropriate program, and checks that every canonical instance of the target contract selected the requested fidelity without a fallback.

The inventory includes scalable FF memory selection, with behavioral and structural-controller choices where registered, `Register<32>` for register words and PC state, `Register<4>` for the trap cause, and `ALU32Core` for execution. Register-file dependencies run with Fibonacci; trap-state storage runs with an illegal instruction. Missing or retired selectable contracts fail the audit instead of silently escaping coverage.

**Boundary:** this is contract/profile integration coverage. It is not every possible combination of choices or every workload on every instance. Low-level component tests still establish each implementation's public contract.

**Source:** `src/tests/RV32ISingleCycleTests.cpp`.

## Campaign reference checks

`--reference-check` compares the selected case's aggregate signature with a reference-profile execution. It is useful for extending coverage to another small case. It does not enable cycle traces and is not a formal equivalence proof.

For large behavioral campaigns, retain the exact tested build/profile and select representative structural cases separately. More cases improve confidence in the tested scope; they cannot justify an untested timing contract or an unimplemented memory system.

The [middle-scale compiled suite](../compiled/middle-equivalence.md) fills the gap between the 64-instruction C fixture and the full Embench applications. Its `compiled-middle` preset applies these complete-program aggregate reference checks to CRC32, SHA-256 and SGLIB using one upstream kernel invocation and the original verifier. It does not silently enable full cycle traces or claim full-sized Embench equivalence.
