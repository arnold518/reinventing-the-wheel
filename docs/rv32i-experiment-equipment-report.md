# RV32I experiment equipment and clock validation

Date: 2026-09-23. Scope: native simulator experiment infrastructure, not a
physical-frequency claim or the planned long external-benchmark study.

## Implemented

- A common `runRV32IExperiment` harness for single-cycle, baseline, and advanced
  cores. Same images, zero-wait FF endpoint defaults, reset exclusion, absolute
  clock schedule and retirement accounting.
- All five advanced predictor policies and configurable table sizes.
- Shared programs 1–22, RV32I ELF input, and configurable dependent/alternating/
  nested loops.
- Whole-program and inclusive/exclusive retirement-PC windows.
- Separate construction/reset, execution, verification, simulator-work and
  pipeline/prediction/port metrics. Final architectural/data-image fingerprints
  accompany timed runs.
- Headless simulator and memory execution, bounded register snapshots,
  cycle/event/allocation limits, serial subprocess timeouts.
- A JSON command-line runner and serial campaign driver with correctness
  preflight, optional structural comparison, warmups, repeated measurements,
  deduplicated manifests, source/binary fingerprints and CSV/JSON summaries.
- Section 4.7 documents the measurement contract and runnable examples.

The legacy ELF fixture now uses absolute clock timestamps and excludes reset
events on both architectures. The new harness stops ELF workloads at retirement
of the verdict store, not at its earlier visibility in the advanced pipeline's
Memory stage. An explicit regression checks this distinction.

## Clock model

The pin-level analyzer replaces the former whole-component boundary heuristic.
It keeps asynchronous register/memory read paths, separates capture from launch,
follows Rewire's actual pin mappings and detects self/disconnected cycles.
Clocked black boxes without a timing model are rejected.

Fall-through endpoints require constrained operating modes: known-binary legal
Read/Write fields, stable reset, active-read payload, ISSUE asserted for fault
qualification, and SENT asserted for held requester responses. Four empty/held
instruction/data endpoint combinations are analyzed separately. An unrestricted
protocol graph can still report a combinational loop; that report is not
silently accepted as a clock estimate.

The common reference retains compact FF cells/registers/register-file storage
and memory macros while expanding combinational logic and CPU hierarchy.

| Configuration | Maximum constrained path | Experiment period |
| --- | ---: | ---: |
| Single-cycle | 182 | 458 |
| Baseline pipeline | 127 | 320 |
| Advanced, each of the five policies at 16 entries | 156 | 394 |

Period is `2 × (D + floor(D/4) + 2)`: a conservative half-cycle settling
allowance, not minimum safe period or physical Fmax. Both execution fidelities
use the same reference period. Historical visualizer fixture periods and
checkpoint tables remain unchanged.

### Subsequent clock-safety API

`computeSafeClockPeriod` and `rv32i_experiment --safe-clock` now distinguish
`safe-under-contracts`, `timing-violation`, and `unknown`. Unlike the historical
candidate calculation, this analysis requires explicit all-state component
contracts and checks full-/half-cycle setup, hold, and clock pulse widths.
It returns no period when coverage is incomplete. Caller-supplied contracts
remain trusted premises, and the result is not a physical timing certificate.

Fused behavioral FF instruction/data memory endpoints now have all-state
contracts (8/16/32-bit, endpoint and scalable families). Focused validation
checks 2,304 request/state cases and 20,805 timed transitions, as well as static
arc/capture coverage; delayed technology endpoints remain unsupported.
State-aware analysis now checks the actual requester gates over four-state
control inputs: six reachable states / 384 transitions per FF interface,
36 combined cases in advanced. Timed value-domain propagation retains mode
transition delays; hold checks retain all original arcs. The default advanced
reference core returns `safe-under-contracts` at **212 ticks** (formerly 232;
per-bit routing and conditional FF validator rules tightened the bound). Single/baseline
still return `unknown` for data-fault/store-operation feedback. Their always-ready
response slots are proven empty, but that alone does not resolve store control.
The JSON includes a closed `dependency_cycle` when unresolved, plus state-case
counts and evidence. A conservative cycle does not prove oscillation. No legacy
protocol-mode cuts are silently imported. `MemoryFeedbackTimingTest` now
demonstrates a real four-state counterexample on both structural cores: load
uninitialized data, AND with 1, then store a byte through that address. An
unknown byte mask causes write rejection, which switches the request to read,
which clears the error and re-enables write. Both cores continue toggling with
the clock frozen. This cannot be fixed by choosing a slower period. Ordinary
experiments zero-initialize memory; the separately declared binary-startup
contract below now covers that narrower invariant. The calculator has
not silently narrowed its four-state guarantee. Core clocks and wiring are unchanged.
The pilot below remains historical evidence at the chosen
periods; neither its completion nor its structural checks prove those periods
safe for every program. Details and supported cases are in section 4.7.

### Binary-startup sufficient periods

`--safe-clock --binary-startup` now explicitly requests the initialized-binary
scope. The runner provisions zero-filled images as in normal experiments; the
calculator itself only inspects images and does not mutate them or specialize
to a program. It checks binary capture preservation from reset-seeded state,
with timing, across all enumerated control cases. Quantified controller decisions
also receive explicit binary-at-edge and timing checks. External binary-input promises
and reset/settling are recorded premises. Unknown bytes, already-started memory,
unsupported models, incomplete proofs and exhausted budgets are rejected.

| Reference core | Sufficient ticks | Cases | Binary bit/case capture checks |
| --- | ---: | ---: | ---: |
| Single-cycle | 176 | 1 | 229 |
| Baseline | 112 | 1 | 1,326 |
| Advanced, 16-entry/not-taken | 212 | 4 | 7,288 |

The solver derives timed controlling-value implications from native gate wiring
to resolve the binary store-enable feedback. It retains all original arcs for
earliest-transition/hold analysis. Sparse memory-image validation examines the
default fill and resident bytes without allocating memory by capacity. JSON
separates the analysis scope and exposes successful binary verification and
evidence. These bounds are sufficient under the simulator contracts, not
optimal periods or physical frequencies. Historical presets are not changed.

Focused validation executes all 22 programs on all three structural reference
cores at their returned periods, plus the branch workload under every advanced
predictor policy. Unit regressions cover sparse image checking at 512 MiB
capacity, unknown capture data, hold violations, budget failures and timed
controlling-value feedback. The earlier oscillation counterexample remains
reproducible and is rejected by binary-startup analysis due to its unknown
initial data image.

Final validation for this addition: **12 focused CTest targets passed**, run
serially in 144.28 seconds with a measured peak RSS of approximately 118.1 MiB.
This includes both unrestricted and binary-startup CLI regressions, the timing
unit tests, experiment harness and focused FF endpoint contract tests. Strict
MkDocs and whitespace checks also passed. No full-suite or long external
benchmark campaign was run for this change.

## Historical candidate-clock pilot

Authoritative artifacts:
`build/experiments/20260923-core-pilot-final/`.

The directory has its `COMPLETE` marker. Its source/binary consistency check
passed. It contains **154 configurations**: 22 shared programs ×
(single-cycle + baseline + five advanced predictor policies).

For each configuration:

1. The compact run passes instruction-by-instruction architectural oracle
   checking and final full data-image comparison.
2. The structural reference matches cycle counts, instruction accounting,
   observations and final-state fingerprint at the selected period.
3. One host warmup and three timed oracle-free repetitions match deterministic
   counters and final-state fingerprints.

Totals: 154 structural runs, 154 oracle preflights, 154 warmups, and **462 timed
repetitions**. Maximum reported process RSS across all phases was approximately
**77.73 MiB**. No full simulator test suite or multi-million-instruction CRC32
run was launched.

Additional focused validation:

- Seven CTest targets passed: experiment unit/CLI tests, timing analyzer,
  bounded simulator drain, legacy ELF runner smoke, register file, and baseline
  Decode.
- CLI tests include all shared programs on all three cores, predictor goldens,
  generated loops, measurement windows, malformed configuration, budget
  exhaustion, ELF retirement, and delayed SRAM response-wait reporting.
- Eight additional structural/behavioral capacity cases passed: one-bit and
  two-bit predictors at 1, 4, 64 and 256 entries on a 16-iteration alternating
  branch workload. Artifacts are in `build/experiments/20260923-capacity-*/`.
- Strict MkDocs build passed and the local section 4.7 page returned HTTP 200.
- `git diff --check` passed.

Build configuration was native C++20 Release. The simulator library used
`-O3 -DNDEBUG -std=gnu++20 -fPIC -UNDEBUG`; the runner used
`-O3 -DNDEBUG -std=gnu++20 -fPIE`. Thus library assertions remained enabled.
The compiler version is recorded in each result. Host time is local simulator
software cost, not CPU hardware speed.

## Candidate-clock example: program 20

All configurations complete 50 nonterminal instructions plus one terminal halt
attempt. The old visualizer calls these 51 retired instructions; the new schema
keeps the terminal attempt separate.

| Core / policy | Cycles | Modeled ticks |
| --- | ---: | ---: |
| Single-cycle | 51 | 23,358 |
| Baseline | 138 | 44,160 |
| Advanced / not-taken | 85 | 33,490 |
| Advanced / taken | 58 | 22,852 |
| Advanced / backward-taken | 58 | 22,852 |
| Advanced / one-bit | 60 | 23,640 |
| Advanced / two-bit | 62 | 24,428 |

This small backward loop favors taken prediction and includes cold predictor
startup. It does not establish a general ranking. In particular, fewer cycles
than the baseline does not automatically mean faster modeled execution than
single-cycle. Hardware-cycle results, constrained timing results and simulator
wall times must remain separate.

Programs 1–16 are correctness/fault cases; do not average them into a headline
performance score. Programs 17–22 are the focused performance panel.

## Remeasurement with binary-startup certificates

The new campaign used `experiment.py --clock-mode binary-safe
--allocation-limit-mib 512 --reference-check`. It certified 176 ticks for
single-cycle, 112 for baseline, and 212 for each 16-entry advanced predictor
configuration. The previous 458/320/394-tick periods were candidate clocks;
changing the clock source did not change the circuits. Each configuration was
oracle-checked, compared with the structural reference, warmed up once on the
host, and measured three times with a fresh CPU and cold predictor each run.

The full 22-program run has **154 configurations, 924 total runs**, and a
`COMPLETE` marker. Paired against the older 154-case pilot, all cases have the
same cycle counts, instruction counts, final-state fingerprints and simulator
event counts. Modeled time is now computed from the certified periods. For the
six-program performance panel (17–22), running each independent program once
gives:

| Core / policy | Total cycles | Total modeled ticks |
| --- | ---: | ---: |
| Single-cycle | 402 | **70,752** |
| Baseline | 992 | 111,104 |
| Advanced / not-taken | 556 | 117,872 |
| Advanced / taken or backward-taken | 491 | 104,092 |
| Advanced / one-bit | 497 | 105,364 |
| Advanced / two-bit | 503 | 106,636 |

Baseline has the lowest modeled time on program 17; single-cycle is lowest on
programs 18–22. The advanced predictor choice affects programs 20–22 in this
small panel. The totals are a descriptive sum, not a weighted benchmark score.

Three longer generated workloads were also checked and measured across all
seven configurations. `dependent` and `alternating` used 128 outer iterations;
`nested` used 32 outer iterations with eight inner iterations each. The
structural reference processed up to 66.9 million events in the nested run,
under the 100-million-event per-process budget. Selected results:

| Workload | Single-cycle ticks | Baseline ticks | Best advanced ticks / policy |
| --- | ---: | ---: | ---: |
| Dependent 128 | **113,344** | 230,944 | 138,012 / taken or backward-taken |
| Alternating 128 | **102,080** | 209,440 | 151,580 / backward-taken |
| Nested 32 | **152,768** | 266,448 | 205,852 / taken or backward-taken |

The advanced core removes most dependency stalls but has a longer certified
period than single-cycle. On the dependent loop, its taken predictor reduced
misses from 127/128 for not-taken to 1/128 and cycles from 902 to 651. The
baseline observed 1,158 data-stall and 640 branch-wait events on that loop;
these counters may overlap. The complete policy-by-workload table is in
[§ 4.6](../site-docs/rv32i/experiments.md).

The compact timed software had a separate host cost: on the dependent loop,
median execution was approximately 0.189 s for single-cycle, 0.468 s for
baseline, and 0.035 s for advanced/taken. Its structural cross-checks took
roughly 14–24 s each. These local software times must not be interpreted as
hardware execution times.

Authoritative local artifacts:

- `build/experiments/20260923-binary-safe-22/`
- `build/experiments/20260923-binary-safe-dependent128/`
- `build/experiments/20260923-binary-safe-alternating128/`
- `build/experiments/20260923-binary-safe-nested32/`

All four have `COMPLETE` markers and the same recorded source and binary
fingerprints. Together they contain **175 verified structural runs, 175 verified
oracle runs, 175 warmups and 525 measurements**. Every measurement satisfies
`modeled_time_ticks = cycles × period_ticks`; peak process RSS across all phases
was 79,868 KiB (about 78 MiB), within the 512 MiB address-space limit. The
memory remains zero-wait FF without caches. Periods are normalized simulator
ticks, not calibrated chip frequencies.

## Remaining research scope

The equipment is ready for controlled simulator experiments, not a claim of
real-chip timing accuracy. The next campaign can expand realistic workload
lengths, predictor sizes and external ELF kernels. Cold versus
prefix-trained region windows must be named explicitly.

Final memory-image checking detects persistent state differences; it does not
prove the absence of every transient or same-value duplicate transaction.
Requester/controller contract tests remain necessary for exactly-once bus
semantics. Timing constraints also need revalidation after circuit changes.

No cache hierarchy, BTB/RAS, area/energy model, physical delay calibration,
official Embench score or native-versus-WASM host benchmark is claimed here.
Partially completed earlier output directories are not the authoritative
campaign.
