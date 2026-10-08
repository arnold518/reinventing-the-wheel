# RV32I experiment equipment

This is the RV32I domain runner behind the common
[`tools/benchmarks/run.py`](../benchmarks/README.md) campaign entry point. Its
JSON keeps the historical top-level fields and also includes the shared
`benchmark_run` schema used by memory and future accelerator runners.

Build serially:

```sh
cmake --build build --target rv32i_experiment RV32IExperimentTest -j1
ctest --test-dir build -R '^RV32IExperiment' --output-on-failure -j1
```

One verified run, with a clock derived from the common structural timing reference:

```sh
build/rv32i_experiment --core advanced --policy two-bit --entries 16 --program 20 --period auto
```

One experiment campaign (new output directory required):

```sh
python3 tools/rv32i-performance/experiment.py \
  --output build/experiments/pilot --reference-check
```

For a campaign at the certified binary-initialized periods, use:

```sh
python3 tools/rv32i-performance/experiment.py \
  --output build/experiments/binary-safe-panel \
  --clock-mode binary-safe --allocation-limit-mib 512 --reference-check
```

`binary-safe` requires a successful `--safe-clock --binary-startup` certificate
for each core and predictor configuration. The campaign records that report in
`clocks.json`, runs at its sufficient period, and labels measured time with
`clock_basis: "binary-startup-safe-clock-v1"`. A common explicit `--period` is
accepted only if it is at least each selected certificate's bound; explicit
periods remain scheduling choices and have no `modeled_time_ticks` entry. The
default `--clock-mode safe` uses the `--safe-clock` certificate, which holds for
every four-state start. The earlier candidate clock is retired.

The default matrix is single-cycle, baseline and five advanced predictor
policies, programs 17–22, one host warmup and three measured repetitions.
Each process starts a fresh CPU and cold predictor. Host warmups do not warm
the next process's predictor. A verified run precedes every measured case.
`--reference-check` additionally compares the structural implementation's
cycles, instruction accounting and observations with the compact model.
Without it, automatically derived clocks are certified by analysis but not
dynamically validated.

The driver writes source/binary/ELF fingerprints, platform and dirty-worktree
provenance, timing paths and implementation manifests, raw JSONL, and JSON/CSV
summaries. `COMPLETE` is written only after all selected cases succeed and the
source/binary fingerprint is unchanged. Interrupted directories are partial
evidence and must not be treated as completed experiments.

Implementation manifests are deduplicated under `manifests/`; JSON rows name
the corresponding file. Final register/PC/status/data-memory fingerprints are
also compared between verification, reference and timed runs.

### Cycle-by-cycle equivalence diagnostics

`--trace-cycles` adds `cycle_trace_schema: 1` and an ordered `cycle_trace` array
to one execution's JSON. Supply `--max-cycles` of at most 4,096; timing-only
queries do not accept tracing. Each entry samples settled pre-edge control,
valid-stage PCs and active memory-port signals, then post-edge architectural
state for the retirement attempt at that edge. `retire.valid` includes terminal
halt/trap attempts, matching `instruction_attempts`, not `retired_instructions`.
Cycle numbers start at zero
after reset; the trace covers the whole run even when counters use a measurement
window. Inactive payload and unselected write-data bytes are intentionally omitted.

```sh
build/rv32i_experiment --core advanced --program 20 --period auto \
  --fidelity reference --max-cycles 1024 --trace-cycles
ctest --test-dir build -R '^RV32IDifferentialTest$' --output-on-failure -j1
```

The bounded differential suite compares two such runs at the same certified
period, reports the first differing cycle/field, and checks the normal final
signature too. See [coverage and limitations](../../docs/benchmark-system.md#bounded-equivalence-checks).
Tracing is disabled by default and adds no simulated events, but its host-side
collection cost is included in execution time: do not use traced runs to compare
simulator speed. Campaign `--reference-check` remains an aggregate comparison.

Use `--period 4000` for a common-clock cycle-efficiency comparison. Such rows
have no claimed modeled execution time. Without an explicit period, each
configuration uses its own constrained-reference period. For predictor-only
comparisons, also run at one common safe period to isolate cycle savings.

## Other workloads

The expanded, performance-only panel leaves the existing 22 numbered scenarios
unchanged. List native workload names with `build/rv32i_experiment --list-workloads`.

```sh
python3 tools/rv32i-performance/experiment.py \
  --output build/experiments/core-gap-panel \
  --suite core-gap --iterations 8 --size 8 --seed 1 \
  --clock-mode binary-safe --allocation-limit-mib 512 \
  --reference-check --warmups 1 --repetitions 3
```

This runs 23 parameterized cases across seven configurations. It covers
independent/dependent arithmetic, equivalent load schedules, branch patterns
and index collisions, nested direct/indirect calls, streaming and pointer
chasing, unsigned insertion sort, CRC-32, and byte scanning. Sort and CRC have
at most two outer iterations in the panel; individual workloads can scale more.
Keep runs serial and raise budgets deliberately for larger workloads.

`--measure-body --training-iterations N` measures after N prefix iterations at
the same PCs. The prefix trains the real predictor and is still oracle-checked;
it is different from a host warmup. Results expose actual PC markers, program
branch PCs, parameters, and independently checked checksum values. Body windows
are retirement-based, so in-flight instructions can influence boundary counters.
The campaign enforces identical images, final results and retired counts across
cores. `obs_*` summary columns overlap; they are not additive CPI attribution.

See §4.7 (`site-docs/rv32i/experiments.md`) for every workload's exact contract.

```sh
build/rv32i_experiment --core advanced --policy one-bit --workload alternating --iterations 128 --period auto
build/rv32i_experiment --core baseline --workload nested --iterations 128 --period auto
build/rv32i_experiment --core advanced --elf tests/fixtures/rv32i/external-smoke/self-check.elf --period auto
```

Generated `dependent`, `alternating`, and `nested` workloads accept 1–1,000,000
iterations. They use only RV32I; no multiplication extension is required.
The cycle and event limits still apply. The driver accepts `--workload`,
`--iterations`, `--elf`, `--entries`, `--cores`, and `--policies` for separate
capacity/workload sweeps.

The pinned CRC32 ELF in `build/rv32i-embench/artifacts/crc32/` can be used with
`--elf`, but needs explicitly raised budgets and a longer process timeout.
Do not start with the multi-million-instruction workload at structural fidelity.

Before a long structural run, a bounded prefix can measure simulator throughput
without claiming benchmark completion:

```sh
build/rv32i_experiment --core advanced --fidelity reference \
  --elf build/rv32i-embench/artifacts/crc32/crc32.elf \
  --period 78 --max-cycles 10000 --allow-incomplete --progress-cycles 1000
```

Such a row has `completed: false`, `verified: false` and an `incomplete` common
status. `prefix_verified` only says every observed retirement matched the ISA
oracle; it is not an upstream benchmark pass.

## Windows

```sh
build/rv32i_experiment --core advanced --workload dependent --iterations 4 \
  --start-pc 12 --stop-pc 32 --period auto
```

The region begins immediately before the first retirement at `start-pc`
(inclusive), and ends before retirement at `stop-pc` (exclusive). Pipeline
fill before the first marker is outside the region. Instruction counters,
port/stall observations and simulator event counts share this window.
The prefix executes normally and trains the predictor; it is not reset at the
marker. Missing markers fail the run. Oracle verification and the final memory
comparison cover the whole program, not merely the selected region.

For Embench, use symbol addresses from the ELF/disassembly to select the
intended invocation. The original board hooks remain no-ops; whole-program
results include startup, benchmark warmup and self-verification. These are not
official Embench scores.

## Clock and timing contract

- `ExperimentClock` generates absolute rising/falling/sample timestamps.
  Reset and initial settling are excluded from measurements.
- The common reference expands combinational logic and CPU structure, retaining
  the same compact FF cells, registers, register file and memory macros for
  every core. It is a hybrid normalized model, not physical synthesis.
- The analyzer keeps asynchronous read-address → data paths, separates
  sequential capture/launch, follows Rewire mappings, and rejects unknown
  clocked black boxes and combinational cycles.
- Fall-through endpoint modes are analyzed separately. Constraints are:
  known-binary legal Read/Write fields, stable reset, payload observed on active
  reads, ISSUE asserted for access-fault timing, SENT asserted for a held
  requester response. These are explicit false-path/operating-mode assumptions,
  not a general four-state protocol timing proof.
- The maximum path over the four empty/held endpoint combinations selects
  `period = 2 × (path + floor(path/4) + 2)`. Each half-cycle has a settling
  margin. This is conservative scheduling, **not an Fmax estimate**.
- Dynamic oracle/equivalence tests at that period are required. Passing a finite
  program corpus does not prove every possible path or input pattern.
- Behavioral cores use the same structural-reference clock; collapsing a core
  does not earn it a fictitious frequency advantage.
- Historical visualizer fixture clocks and checkpoint timestamps are preserved.
  The new experiment harness is the clock authority for performance runs.

No nanoseconds, MHz, physical area, energy or real-chip accuracy is claimed.
Explicit periods are only scheduling values; `--period auto` reports
`modeled_time_ticks = cycles × period` under the stated reference assumptions.

### Fail-closed clock-safety report

```sh
cmake --build build --target rv32i_experiment SafeClockTimingTest -j1
build/rv32i_experiment --core advanced --safe-clock
```

This calls `CircuitTimingAnalyzer::safeClock` (`include/analysis/SafeClockTiming.hpp`)
without running a program. It does **not** call the legacy mode-constrained
estimator. JSON contains `safe_clock.status`, nullable `safe_period_ticks`,
limiting constraint/path, a closed `dependency_cycle` when found, contract
evidence, assumptions and diagnostics.
Exit codes: 0 = safe under contracts; 2 = no established safe period; 1 = bad
command configuration. There is no numeric fallback on failure.

Supported analysis: one ideal 50%-duty clock, explicit rising/falling launch
and capture edges, min/max propagation, setup/hold, pulse widths, synchronous
external input windows and external output sampling. No implicit false-path
cuts, arbitrary behavioral black boxes, gated clocks or latch borrowing.
Reset must be fixed/deasserted after initial settling. Unconstrained external inputs,
unresolved combinational cycles, incomplete contracts, overflow and resource exhaustion
fail closed. Custom component contracts are trusted all-state premises, not
proofs generated from C++ code.

Fused behavioral FF endpoints now have an all-state contract at widths 8/16/32,
shared by the endpoint and scalable families. It covers all eight combinational
arcs, rising-edge writes/response storage, and one-tick wire propagation, with
reset fixed LOW. Timing analysis does not expand the storage by capacity.
Delayed technology endpoints remain unsupported rather than borrowing the FF
contract based on matching pins.

State-aware analysis enumerates the actual requester control netlist, including
X/Z decisions: six reachable abstract states and 384 transitions per FF
interface. It checks all 36 IMEM/DMEM combinations using timed value domains,
so clock-to-output and gate delays are retained when a control value resolves
a feedback path. Hold checks keep all original arcs. An always-ready endpoint
instead has a proven empty response slot after reset. Enumeration defaults to
a maximum of 64 combined cases; unsupported/mixed-edge cases retain the
general dependency analysis, and exhausted budgets fail closed. Settling work
is also bounded (10 million evaluations by default).

In the default unrestricted four-state scope, the advanced reference core returns `safe-under-contracts` with a
**212-tick sufficient period**, not an exact minimum or physical Fmax. Per-bit
joiner/splitter/Rewire propagation and conditional FF validator rules tighten
the earlier 232-tick bound without changing the cores or assuming binary data.
Single/baseline still return `unknown` for data-fault/store-operation feedback.
`MemoryFeedbackTimingTest` now reproduces actual oscillation on both cores:
`lw x1,0(x0); andi x1,x1,1; sb x0,0(x1); ebreak`, with uninitialized FF data
memory, produces an unknown byte mask. Read ignores that mask, write rejects
it, and fault suppression repeatedly changes the operation. The test freezes
the clock and observes continued toggles. No finite unrestricted settling bound
exists for this case. Normal experiments clear memory to binary zero, but
the analyzer does not silently assume or certify that startup invariant.
`dependency_cycle` names an actual closed pin walk, not downstream blocked
pins, and is not proof of oscillation. `state_case_count`,
`checked_state_cases` and `state_evidence` expose the state analysis. Legacy
mode-specific cuts are not silently imported.
The earlier 458/320/394 candidate clocks and the `--timing` option are retired.
Do not reinterpret earlier campaign artifacts as safety certificates.

The safe-period function also analyzes supported smaller circuits, including a
structural FullAdder between compact FF cells. Remaining unknown results need
resolution of their dependency cycles or contracts for the selected technology;
another passing benchmark does not close that gap. The new feedback test also
checks 10,752 four-state validator cases and a provable 26-tick known-address
byte-store feedback fixture, including recovery after the unknown input clears.

Focused endpoint validation: `MemoryEndpointSafeClockTest` checks all three
widths, 2,304 state/request cases and 20,805 timed transitions against an
independent tiny-memory oracle, plus static setup/hold and fail-closed cases.
`MemoryStateTimingTest` additionally compares 256 coupled requester/endpoint
frames at its calculated 18-tick period with a 2,000-tick reference, and checks
four-state reachability, state budgets, hold hazards and unresolved oscillators.
The CLI regression runs the 22-program structural advanced corpus and a branch
workload under every predictor policy at the periods returned by the analyzer.

### Binary-initialized clock-safety scope

```sh
cmake --build build --target rv32i_experiment BinaryStartupTimingTest -j1
build/rv32i_experiment --core single --safe-clock --binary-startup
build/rv32i_experiment --core baseline --safe-clock --binary-startup
build/rv32i_experiment --core advanced --safe-clock --binary-startup
ctest --test-dir build -R '^(BinaryStartupTimingTest|RV32IBinaryClockCLITest)$' --output-on-failure -j1
```

This explicit mode uses the normal experiment harness's binary-zero image
preparation. The calculator inspects all provisioned FF memory for unknown
bytes, including sparse default fill, and rejects already-started endpoints.
The C++ API does not clear or mutate memory. The proof assumes reset and initial
settling, checks that all modeled captures preserve binary state, and includes
setup, hold and pulse-width checks. Controller decisions quantified by binary
state enumeration are also explicitly checked binary and timed at the edge.
Subsequent binary program/data loading is
permitted; runtime image mutation or X/Z input injection is not. Changing
external inputs require an explicit `ClockInputConstraint::binary_values`
promise. Custom replacement contracts and unsupported technologies fail closed.

Generic timed gate implications such as `A OR (A AND B) = A` prove redundant
enables even when the other input comes from feedback. The controlling signal's
propagation delay is included; no CPU-specific false-path list is added.

| Reference core | Sufficient period | Control cases | Binary capture checks |
| --- | ---: | ---: | ---: |
| Single-cycle | 176 | 1 | 229 |
| Baseline | 112 | 1 | 1,326 |
| Advanced, default 16-entry/not-taken configuration | 212 | 4 | 7,288 |

JSON identifies `analysis_scope: "binary-initialized"` and reports
`binary_startup_verified`, `binary_capture_bits` (capture/control bit checks
summed across cases), and `binary_evidence`. Verification is true only on successful proof.
Plain `--safe-clock` retains `analysis_scope: "unrestricted-four-state"`;
single/baseline remain unknown in that scope. None of these periods is an exact
minimum or physical Fmax, and clock defaults remain unchanged. To execute a
test at the returned bound, pass its value via `--period`.

The binary CLI regression checks all 22 programs on all three structural
reference cores at their returned periods, plus a branch workload under all
five predictor policies. Unit tests cover binary-image inspection, sparse
512 MiB capacity with bounded allocation, timed AND/OR feedback and fail-closed
rejection. The real uninitialized-memory counterexample is also rejected before
execution; finite program success is not substituted for the static proof.

## Metrics and resource boundaries

`instruction_attempts` includes terminal halt/trap attempts.
`retired_instructions` excludes them. CPI/IPC use the latter; whole-program
cycles still include reaching the terminal event. Older visualizer metrics
include the terminal attempt in their instruction denominator, so their CPI
must not be mixed with this schema.

Stall/port observations can overlap and are not an additive CPI breakdown.
Branch misses mean **next-PC** misses, not necessarily direction errors.
Pipeline occupancy counts the four inter-stage valid bits, not five stages.

Setup, execution and verification are timed separately. Execution includes
host observation/clock-driving overhead. `--no-verify` skips oracle checking;
the campaign driver uses it only after a verified run and compares deterministic
counters. It is not an independently verified result. Histories are disabled
in both simulator and memories, and register snapshots stay bounded.

Automatic timing derivation is a separate preflight. Setup time covers the
measured circuit instance, not the extra reference instance used to derive its
clock. A standalone `--period auto` process's peak RSS includes that preflight.
ELF completion is observed when the verdict store retires, not when the advanced
pipeline first makes its data visible in Memory.

Each subprocess has a default 1,536 MiB address-space allocation cap, adjustable
with `--allocation-limit-mib`. This is not an RSS cap. Peak RSS is reported on
Linux. The driver adds a process timeout; the engine enforces event and cycle
budgets. Runs are serial. There is no automatic service shutdown or swap change.

Delayed memory is a separate advanced-only experiment:

```sh
build/rv32i_experiment --core advanced --program 21 --memory sram --period auto
```

Both ports use the selected technology. Single-cycle/baseline reject delayed
devices. ROM and NOR flash are not interchangeable writable RAM; choose a
compatible program. Device latency stays in ticks when the CPU period changes.
Automatic core timing uses the FF reference, not a fabricated timing model of
the delayed technology. No caches are involved.
