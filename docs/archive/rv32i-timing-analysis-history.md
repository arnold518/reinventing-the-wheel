# Timing-analysis history

Archived from public §4.7 on 2026-10-14.

[Current experiment results](../../site-docs/rv32i/experiments.md) · [Current timing contract](../../site-docs/rv32i/4.2/timing.md)

This is the preserved development narrative from the former experiment page. **It is historical, not the current support matrix or clock preset.** In particular, statements below about unresolved fixed-cycle feedback and periods such as 176/112/212 describe earlier implementations. Query the current runner and retain its certificate for new experiments. No measurement was rerun for this archive.

## Clock reference

A behavioral implementation is faster host software, not faster hardware.
Both execution fidelities use the clock derived from the same reference
configuration.

The reference expands CPU and combinational structure while retaining compact
FF cells, registers, register-file storage and large FF memory macros.
Asynchronous register/memory read paths remain timing paths; only clocked
state updates terminate them.

Fall-through memory requires operating-mode constraints. The reference assumes
known-binary Read/Write requests, stable reset, payload observed during active
reads, ISSUE asserted for access-fault timing, and SENT asserted when a
requester's response is held. It analyzes the four empty/held instruction/data
endpoint combinations separately and takes the maximum path.

For path delay `D`, the candidate period is:

```text
period = 2 × (D + floor(D / 4) + 2)
```

Each half-cycle includes a settling margin. Dynamic structural execution must
then match the compact model and instruction oracle at this period.
`--period auto` derives a candidate; the campaign's `--reference-check`
records which workloads actually validated it.

This is **not an Fmax estimate**. It excludes routing, physical memory timing,
clock skew, voltage and technology characterization. A finite validation corpus
does not prove all possible input paths. Larger predictors may change the
modeled clock as well as cycle counts.

For cycle-efficiency comparisons, use one common safe period. For modeled
execution-time comparisons, report each configuration's own period and its
assumptions. For delayed-memory experiments, device latencies remain in ticks;
they are not silently rescaled when the CPU clock changes.

### Safe-period analysis

`--period auto` and `--timing` retain the candidate-period calculation above.
Their JSON timing object states `guarantee: "candidate-only"`. Passing the
structural corpus does not promote a candidate into a universal guarantee.

`computeSafeClockPeriod` is a separate, non-mutating analysis. Its result is a
**sufficient period under explicit component and environment contracts**, not
an exact minimum period, a physical frequency, or proof of ISA correctness.

| Status | `safe_period_ticks` | Meaning |
| --- | --- | --- |
| `safe-under-contracts` | Even number | Every modeled setup, hold and pulse-width requirement is satisfied under the stated assumptions |
| `timing-violation` | `null` | The conservative hold bound fails; increasing the period cannot establish safety for the same-edge path |
| `unknown` | `null` | Missing/incomplete contracts, unsupported clocking, unconstrained inputs/outputs, unresolved dependency cycles, or analysis limits |

The first implementation supports one directly distributed ideal clock with
50% duty cycle. Reset and initial settling precede operation. External inputs
must be fixed or have declared transition windows relative to a clock edge;
external outputs must have declared sampling requirements. Debug/image writes
must not mutate state during analyzed operation. Arbitrary clock gating,
multiple clock domains, latch time borrowing, and implicit multicycle or
false-path exceptions are unsupported and cannot receive a safe result.

For each captured input, the analyzer propagates both earliest and latest
arrival times separately from rising- and falling-edge sources. Let `late`
include clock-to-output and combinational delays, and `g` be the required
event separation (one tick by default):

| Requirement | Bound |
| --- | --- |
| Same-edge setup, captured on the next occurrence | `period >= late + setup + g` |
| Opposite-edge setup | `period >= 2 * (late + setup + g)` |
| Same-edge hold | `early >= hold + g`; slowing the clock does not help |
| Opposite-edge hold | `period / 2 + early >= hold + g` |
| Clock pulse width | `period / 2 >= max(minimum_high, minimum_low)` |

It takes the maximum required period and rounds upward to an even tick count.
The general analyzer can return two ticks; the experiment harness separately
requires at least four. A consumer must also respect its own scheduling limits.
There is no automatic doubling of full-cycle paths or arbitrary percentage
margin. Bus-level dependencies may conservatively include unrealizable paths,
so this sufficient bound need not be the fastest usable period. A hold-bound
failure likewise need not mean that a failing input pattern has been found.

Each component contract names its evidence/version, all combinational arcs,
clocked captures and launches, fixed inputs, constant outputs, and pulse-width
requirements. Contracts must bound **all states and transitions**, including
glitches, on simulator wires rather than immediate UI pin caches.
Caller-provided contracts are trusted premises, not automatically
proved by the analyzer.

Built-in contracts cover the concrete primitive gates, supported bit adapters,
Rewire, constants, and the compact FF cell, Register<32> and register-file
implementations, plus the fused behavioral FF endpoint at widths 8, 16 and 32
(including the scalable endpoint family). A structural master/slave DFF does not inherit its behavioral
counterpart's timing contract. Structural composites are expanded; unsupported
leaf implementations fail closed. Asynchronous register-file read paths remain
visible even though the same component also contains clocked state.

```cpp
#include "analysis/SafeClockTiming.hpp"

// `root` is an initialized Register<32> behavioral component named "REG".
// This describes the environment; it does not drive or modify the circuit.
circuit::analysis::ClockSpecification clock;
clock.clock_pin = "REG.input.CLK";
clock.inputs = {
    {"REG.input.RST", std::nullopt, {}, 0},
    {"REG.input.WE", std::nullopt, {}, 1},
    {"REG.input.D", circuit::analysis::ClockEdge::Rising, {1, 3}, 0},
};
clock.outputs = {{"REG.output.Q", circuit::analysis::ClockEdge::Rising, 0, 0}};
auto report = circuit::analysis::computeSafeClockPeriod(root, clock);
// Under these contracts: safe-under-contracts, period 4 ticks.
// Never substitute a default clock when safe_period_ticks is empty.
```

#### FF endpoint timing contract

The FF endpoint contract covers both same-cycle responses and the saved-response
slot. It is tied to the concrete behavioral implementation, not inferred from
the port names. Reset must remain LOW after initialization. With internal state
held fixed, the all-state combinational dependencies are:

| Output | Inputs that can change it before the next clock edge |
| --- | --- |
| `REQ_READY` | `RSP_READY` |
| `RSP_VALID` | `REQ_VALID` |
| `RSP_READ_DATA` | `REQ_VALID`, `REQ_OP`, `REQ_ADDR` |
| `RSP_STATUS` | `REQ_OP`, `REQ_ADDR`, `REQ_WRITE_MASK` |

Every listed arc delivers its transition after **one simulator tick**. This is
the fused model's declared delay, not a physical SRAM/FF access time. In
particular, `REQ_OP` and unknown write-mask bits can change the status even when
`REQ_VALID` is LOW; those paths cannot be silently removed.

All six request/control inputs other than `CLK` and `RST` are captured on the
rising edge: writes and response capture/consume/replacement depend on them.
Each output can also change one tick after a rising edge changes the stored
state. This conservative union covers empty and held slots without assuming
that the receiver always accepts a response. The dependency of `REQ_READY` on
`RSP_READY` is active when the slot is full. `RSP_READY` is not a combinational
dependency of response data or validity: when the slot is full, those outputs
come from the saved response.

Capacity does not expand the timing graph: the fused implementation remains
one modeled component. SRAM, DRAM, ROM and NOR-flash endpoints do **not** inherit
this FF contract. Their command sequencing and autonomous timed events need
separate timing models; they currently fail closed.

`MemoryEndpointSafeClockTest` checks every listed arc, rising-edge capture and
opposite-edge constraints at all three widths. Its independent four-word oracle
covers 2,304 state/request cases and 20,805 timed transitions, including held
responses, consume-and-replace, writes, invalid operations/addresses and X/Z
fields. It observes delivered wire values at the input-change tick and one tick
later; all eight combinational arcs have a single-input dynamic witness.
These are regression checks supporting the implementation-derived contract,
not exhaustive proof over every memory image. Existing endpoint contract tests
also compare structural and behavioral functionality.

#### State-aware settling

The analyzer now checks reachable control states before treating every feedback
dependency as unbounded. This is not a snapshot of the currently running
program, and it does not assume that the response buffer is always empty.

For a fused FF endpoint connected to `RV32IMemoryRequest`, it evaluates the
**actual requester control gates** for every combination of `ISSUE`, `TAKE` and
tag mismatch, each allowed to be LOW, HIGH, X or Z. Starting from reset, it
enumerates the next values of BUSY, SENT, ABANDONED and the endpoint response
slot. Supported wiring changes are re-enumerated; unsupported control logic
is not silently given the original circuit's proof.

The current circuit has **six reachable abstract states and 384 transitions**
per interface. Binary controls alone would give two states, but that smaller
result is insufficient for our four-state simulator. Instruction and data
interfaces are combined conservatively: all **36** state combinations are
checked, even if the complete CPU cannot reach every combination.

For the single-cycle and baseline cores, `RSP_READY` is permanently HIGH. The
analyzer proves by induction that their response slots remain empty after
reset; this does not resolve their separate store-control feedback.

Within each case, the analysis tracks both:

- which logic values a signal may have, including X/Z;
- when that restriction, or the final settled value, is guaranteed to reach
  the wire.

For example, an AND gate whose controlling input becomes LOW does not clamp its
output immediately. The input must arrive, then the gate delay must pass. This
allows a bounded feedback-settling proof without ignoring mode-transition
glitches. Case-dependent register outputs retain their clock-to-output delays.
The result takes the worst bound over every checked state. **Hold analysis
retains every original all-state arc**, including feedback, and uses the
earliest possible arrival along those paths.

This state-aware solver currently handles rising-edge launches; mixed-edge
circuits retain the general dependency-graph analysis. Enumeration is bounded
by `ClockSpecification::maximum_state_cases` (64 by default) and
`maximum_state_evaluations` (10 million work-list evaluations by default). Unresolved cases,
unsupported implementations and exhausted budgets never receive a guessed
period. JSON exposes `state_case_count`, `checked_state_cases` and
`state_evidence` alongside the timing result; `state_evaluations` records work
performed against the bound.

The solver preserves **individual bit domains and settling times through
joiners, splitters and Rewire mappings**, including unmapped constant bits.
For FF response status, it evaluates the validator over opcode, address-error
and mask-knownness categories. An input is omitted from a particular status
bit's settling calculation only when every allowed value of that input gives
the same result with the other categories held fixed. The proof includes the
time when those restrictions become true, plus the endpoint delay. It does not
assume binary inputs or enumerate the memory's entire address space. Explicit
replacement timing contracts retain the general analysis, without silently
adding these built-in semantic rules.

`MemoryStateTimingTest` checks the six-state proof, resource-limit rejection,
hold violations and an unresolved oscillator. It also compares 256 coupled
requester/endpoint simulation frames at the calculated 18-tick fixture period
against a 2,000-tick reference, including X/Z controls, held responses, writes
and request errors. These regression runs support the code-derived bound; they
are not its source.

`RV32IExperimentCLITest` runs all 22 shared programs with the structural advanced
core at its returned period, checking instruction-by-instruction architectural
state and final memory against the oracle. It also checks all five predictor
configurations and runs the branch workload at each configuration's derived
period.

#### Earlier full-core result

For **unrestricted four-state analysis**, using the reference structural profile with compact FF storage, zero-wait FF
endpoints and the default 16-entry/not-taken predictor configuration:

| Core | Result | Explanation |
| --- | --- | --- |
| Single-cycle | `unknown` | Store feedback can actually oscillate for a reachable unknown byte address; no unrestricted four-state period is available |
| Baseline pipeline | `unknown` | The same unknown-byte-address feedback occurs in its memory stage |
| Advanced pipeline | `safe-under-contracts`, **212 ticks** | All 36 memory control-state combinations have bounded settling, setup and hold; per-bit rules tighten the earlier 232-tick bound |

For unresolved cases, `dependency_cycle` is a closed pin-by-pin walk through
the dependency graph, separate from `critical_path`. It does **not** establish
that the circuit oscillates: a conservative graph can combine paths that are
never active together. However, a separate executable counterexample now
demonstrates actual non-settling behavior in both fixed-cycle cores.

The 212-tick result is sufficient under the stated simulator contracts, not an
exact minimum or a physical CPU frequency. It is independent of the selected
program; ISA/oracle checks are separate. Rerun the analysis when the predictor,
memory, build profile or component implementation changes. Delayed-memory
endpoints still lack suitable timing contracts and return `unknown`.

#### Earlier unrestricted-period limitation

`MemoryFeedbackTimingTest` loads this program into the actual structural
single-cycle and baseline cores, with compact FF storage and **uninitialized
data memory**, which contains X under the existing device contract:

```asm
lw   x1, 0(x0)      # x1 becomes unknown
andi x1, x1, 1      # only bit 0 remains unknown; all higher bits become zero
sb   x0, 0(x1)      # the byte lane is unknown
ebreak
```

The word address sent to memory is zero, but the byte-write mask contains X.
For this request, a write returns `InvalidRequest`; a read ignores the mask
and returns `Ok`. The existing core uses that error to suppress write
permission, and uses write permission to select the operation. It therefore
switches back to read, clears the error, enables writing again, and repeats.
There is no stable write-permission value for this fixed request. The test
freezes the clock and still observes operation transitions: 47 in 500 ticks
for single-cycle and 67 for baseline. Increasing the clock period cannot make
this request settle.

This is not a claim that the ordinary benchmark runs oscillate: the experiment
harness explicitly clears its memory images to binary zero before execution.
The unrestricted analyzer runs before workload provisioning and promises no
such binary-memory invariant. The opt-in initialized-binary analysis below
declares and checks that narrower startup contract, including inductive
preservation and setup/hold checks; it does not replace four-state coverage.

The new regression also checks 10,752 concrete validator combinations against
the production implementation, proves a known-address byte-store feedback
fixture at 26 ticks, and verifies its unknown-address oscillation and recovery.
These tests distinguish a newly provable circuit from a real non-settling one.

#### Binary-initialized startup contract

`--safe-clock --binary-startup` selects an explicit, narrower scope. The
unrestricted `--safe-clock` command keeps its original meaning. The new mode
uses the experiment harness's existing zero-filled-memory preparation policy;
any binary program/data image can subsequently replace those bytes. The proof
does not specialize to one program, instruction sequence or memory value.

The calculator checks the following premises and induction step:

1. Every supported FF endpoint has a fully binary, provisioned image, before
   its first simulation evaluation. The check includes sparse default-fill
   bytes without allocating the entire address space.
2. The environment must assert reset on all supported storage and allow
   initialization to settle before normal operation. The recognized FF cell,
   register and register-file implementations reset to binary zero; endpoint
   response state also resets to zero.
3. Changing external inputs need explicit `binary_values` promises. Fixed
   inputs retain their stated constants. Runtime debug memory mutation and X/Z
   injection are outside this scope.
4. Assuming binary stored state at the start of a cycle, the analysis proves
   binary values at every modeled capture, with those restrictions arriving
   before capture. It also proves settling, setup, hold and pulse-width bounds.
   Quantified controller decisions (issue, take and tag mismatch) receive
   explicit binary-at-edge and timing checks before binary-only state
   enumeration is accepted.
   Thus a correctly timed cycle preserves the stored-state premise for the
   next cycle. Unknown constants reaching captures, unsupported state models,
   exhausted proof budgets and timing violations fail closed.

The structural enable correlation is checked from gates, not from component
names. For example, `A OR (A AND B)` settles to binary `A` even if `B` is part
of feedback. The proof includes the delay for a LOW `A` to propagate through
the AND path, followed by the OR delay; it does not pretend the redundant path
has zero delay. This applies to the fixed-cycle cores' request enables.

For the C++ API, opt in with `ClockSpecification::binary_startup = true` and
set `ClockInputConstraint::binary_values = true` on each changing external
input. Memory images must already be provisioned; the calculator does not
clear or modify them. Explicit replacement component contracts are not
supported by this binary-preservation proof.

JSON reports `analysis_scope: "binary-initialized"`,
`binary_startup_verified`, `binary_capture_bits` and `binary_evidence`.
`binary_startup_verified` is true only for a successful timing result, not
merely because the command-line option was supplied. The unrestricted mode
reports `analysis_scope: "unrestricted-four-state"` and keeps that flag false.
`binary_capture_bits` counts capture and quantified-control bit checks across
all cases, not unique storage bits or the capacity of memory.

For the same reference profile and default predictor configuration as above:

| Core | Sufficient period (ticks) | Control cases checked | Binary capture checks | Limiting capture |
| --- | ---: | ---: | ---: | --- |
| Single-cycle | **176** | 1 | 229 | Decode register-file write data |
| Baseline pipeline | **112** | 1 | 1,326 | IF/ID instruction register |
| Advanced pipeline | **212** | 4 | 7,288 | Fetch PC register |

All three return `safe-under-contracts` with `binary_startup_verified: true`.
These are sufficient simulator periods under the declared premises, not exact
minimum periods or physical clock frequencies. The advanced core's additional
control paths still limit its clock; pipelining does not by itself guarantee a
smaller period than every simpler core.

```sh
build/rv32i_experiment --core single --safe-clock --binary-startup
build/rv32i_experiment --core baseline --safe-clock --binary-startup
build/rv32i_experiment --core advanced --safe-clock --binary-startup
```

`RV32IBinaryClockCLITest` executes all 22 shared programs on each structural
reference core at its returned period, with instruction-by-instruction oracle
checks and final memory comparison. It also checks all five advanced predictor
policies and runs the branch workload at each policy's calculated period.
`BinaryStartupTimingTest` covers initialized and unknown memory images, sparse
512 MiB capacity without materializing it, unknown capture inputs, input
promises, hold failures, work budgets and timed controlling-value feedback.
`MemoryFeedbackTimingTest` confirms that the uninitialized-memory counterexample
is rejected by this mode before demonstrating its oscillation. Program tests
validate the implementation; finite test coverage is not the source of the bound.

No core clock defaults, layouts or instruction-cycle behavior were changed.
The legacy 458/320/394-tick auto-period calculation remains explicitly
`candidate-only`; historical campaign artifacts are not retroactive timing
certificates.

```sh
build/rv32i_experiment --core advanced --safe-clock
```

The command emits JSON and exits with status 0 for `safe-under-contracts`,
2 for an unavailable safe period (`unknown` or `timing-violation`), and 1 for
invalid command configuration. `--safe-clock` and `--timing` are mutually
exclusive. Analysis does not change core clocks, layouts, or checkpoints.
