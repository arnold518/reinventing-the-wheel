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
default `--clock-mode safe` uses the `--safe-clock` certificate in its
unrestricted four-state scope under the declared contracts. The earlier
candidate clock is retired.

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
signature too. See [coverage and limitations](../../docs/reference/benchmark-system.md#bounded-equivalence-checks).
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
  --period 100 --max-cycles 10000 --allow-incomplete --progress-cycles 1000
```

Such a row has `completed: false`, `verified: false` and an `incomplete` common
status. `prefix_verified` only says every observed retirement matched the ISA
oracle; it is not an upstream benchmark pass.

Every run reports `setup_violations`: how often a compact part saw an input
change inside its setup time. Anything above 0 means the clock is too fast
for that system (the run also warns on stderr), and the compact form may
then differ from its gates. `--period auto` certifies a clock for
`--memory sram|dram|rom|nor-flash` too (102 for the advanced core, against
100 with the flip-flop memory); that period also covers what the compact
core's table needs at its memory pins, so a compact run reports 0.
`--fidelity reference --check-handshake-states` (validation only) fails if
a memory port is ever in a handshake state the clock analysis did not cover.

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
- The common timing reference expands the selected core's structural logic
  while retaining the documented compact storage and memory macros. The
  analyzer checks propagation, setup, hold, pulse widths and reachable memory
  handshake states under its stated contracts.
- Catalog-selected behavioral parts use gate-derived per-path delay tables.
  Eligible smaller parts can use a delay twin to follow current input values;
  complete cores and self-timed memory parts use their tables. Propagation
  delay can vary during a run even though its chosen CPU period stays fixed.
- `--safe-clock` supplies sufficient periods under the selected timing
  contracts: 166 ticks for single-cycle/FF, 80 for baseline/FF, 100 for
  advanced/FF, and 102 for advanced with SRAM, DRAM, ROM or NOR-flash.
  The four timed technologies share a limiting request-ready setup path,
  while their access response latencies differ.
- Behavioral and structural forms use the same certificate for a selected
  core, policy and memory. Reference checks compare their completed cycles
  and state; a finite program corpus does not prove every input history.

No nanoseconds, MHz, physical area, energy or real-chip accuracy is claimed.
Explicit periods are only scheduling values; `--period auto` reports
`modeled_time_ticks = cycles × period` under the stated reference assumptions.

### Fail-closed clock-safety report

```sh
cmake --build build --target rv32i_experiment SafeClockTimingTest -j1
build/rv32i_experiment --core advanced --safe-clock
```

This calls `CircuitTimingAnalyzer::safeClock` (`include/analysis/CircuitTimingAnalyzer.hpp`)
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

Fused behavioral FF endpoints use their own timing contracts at widths
8/16/32, including combinational arcs and rising-edge storage. Supported
larger memories use delay estimates extended from smaller proven geometries;
the analysis does not expand every storage cell. Timed SRAM, DRAM, ROM and
NOR-flash endpoints have their own sequenced-state analysis, not an FF
substitution. Their response timing and phase sequencing remain
technology-specific.

State-aware analysis enumerates the requester and endpoint control netlist,
including X/Z decisions. FF endpoints use empty/held response states; timed
endpoints add their reachable sequencer and handshake states. Timed value
domains retain clock-to-output and gate delays when a control value resolves
a feedback path. Hold checks keep all original arcs. State and settling
budgets fail closed when exhausted.

The current FF systems certify 166, 80 and 100 ticks for single-cycle,
baseline and advanced cores in both the unrestricted four-state and
binary-initialized scopes. The advanced core with each supported timed
technology certifies 102 ticks in the unrestricted four-state scope. The
timed-memory proof enumerates reachable requester/endpoint handshake states;
`state_case_count`, `checked_state_cases` and `state_evidence` expose that
coverage. A certificate is sufficient under its declared contracts, not an
exact minimum or physical Fmax. The retired candidate-clock formula and old
campaign periods are not current timing certificates.

The safe-period function also analyzes supported smaller circuits, including a
structural FullAdder between compact FF cells. An unsupported configuration
still needs its own certificate; another passing benchmark does not close a
missing timing contract. The feedback tests also exercise four-state
validator and byte-store cases.

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
promise. Unsupported configurations fail closed. Timed-memory certificates
currently use the unrestricted four-state analysis rather than this mode.

Generic timed gate implications such as `A OR (A AND B) = A` prove redundant
enables even when the other input comes from feedback. The controlling signal's
propagation delay is included; no CPU-specific false-path list is added.

| Reference core with FF memory | Sufficient period (ticks) |
| --- | ---: |
| Single-cycle | 166 |
| Baseline | 80 |
| Advanced, default 16-entry/not-taken configuration | 100 |

JSON identifies `analysis_scope: "binary-initialized"` and reports
`binary_startup_verified`, `binary_capture_bits` (capture/control bit checks
summed across cases), and `binary_evidence`. Verification is true only on successful proof.
Plain `--safe-clock` uses `analysis_scope: "unrestricted-four-state"` and
certifies the same three FF periods. None of these periods is an exact
minimum or physical Fmax. To execute a test at the returned bound, use
`--period auto` or pass its value via `--period`.

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
Automatic core timing uses the selected timed endpoint's own contract. The
advanced core currently certifies 102 ticks with each supported timed
technology; their different response latencies can change cycle counts. No
caches are involved.

## Regenerating the §4.7 result figures

With Matplotlib available, run from the repository root:

```sh
MPLCONFIGDIR=/tmp/circuitsim-mpl python3 tools/rv32i-performance/plot-current-results.py
```

The generator pins the completed October 14 measured campaigns and the older
full Embench campaign. It checks completion, the seven-target grids, oracle
and structural phases where applicable, source/binary fingerprints, clock
periods, tick arithmetic, all 133 historical Embench cycle inputs, and 602
cells in the public §4.7 tables before writing four SVGs. Full Embench remains
a conditional clock-only projection. Add `--preview-dir /tmp/rv32i-chart-previews`
to write PNGs for visual inspection; those previews are not site assets.
The checked-in SVGs were generated with Matplotlib 3.10.3. Their data checks
are version-independent, but other Matplotlib versions can change SVG bytes
or spacing; inspect previews after regenerating with a different version.
