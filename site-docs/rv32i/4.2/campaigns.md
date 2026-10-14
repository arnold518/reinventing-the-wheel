# § 4.2.7. Campaigns and Reproducibility

[Benchmark reference](index.md) · [Execution contract](execution.md) · [Verification](verification/index.md)

## Campaign interface

`tools/benchmarks/run.py` runs a selected collection **serially**. RV32I execution delegates to `tools/rv32i-performance/experiment.py`; memory execution invokes the existing memory benchmark binaries. It does not compile external workloads automatically.

| Selection | Cases before core/policy expansion | Purpose |
| --- | --- | --- |
| `--preset smoke` | Programs 1, 17, 20 | Short correctness, arithmetic, and branch check |
| `--preset program-regression` | All 22 fixed programs | Instruction behavior, traps, and short workloads |
| `--preset generated-regression` | All 15 generated families | One representative configuration per family |
| `--preset core-gap` | 23 parameter configurations | Dependencies, scheduling, branch patterns/aliasing, calls, memory loops, and training |
| `--preset compiled-middle` | Three reduced-work Embench ELFs | Complete structural/behavioral comparisons of real compiled algorithms |
| `--workload NAME` after `--` | One parameterized case | Controlled experiment varying one input |
| `--elf FILE` after `--` | One compiled image | External application or checked-in fixture |
| `--elf-dir DIR` after `--` | Every recursively discovered `.elf` | A previously built external collection |

The default target panel is single-cycle, baseline, and advanced with each of five predictor policies: seven configurations, not seven different cores. The [compiled-middle preset](compiled/middle-equivalence.md) defaults to just advanced/two-bit, giving three targets and nine paired cases; it always enables structural reference checks. Restrict `--cores` and `--policies` explicitly for small checks. `--entries` sets predictor capacity. The core-gap preset caps sort and CRC at two iterations and includes two taken-branch body-window cases with zero/32 training iterations; it is not a uniform amount of work across cases.

## Small structural/behavioral check

Run from the repository root; each output directory must be new:

```sh
python3 tools/benchmarks/run.py rv32i \
  --output build/experiments/docs-smoke --preset smoke --timeout 300 -- \
  --cores single,baseline,advanced --policies not-taken \
  --clock-mode binary-safe --reference-check \
  --allocation-limit-mib 512 --max-cycles 4096 --max-events 10000000 \
  --warmups 0 --repetitions 1
```

For each case the driver first runs the compact core with oracle checking. `--reference-check` also runs the structural-datapath reference profile at the same period and compares aggregate signatures, including cycles and observations. This option is **not** the per-cycle comparison in [RV32IDifferentialTest](verification/equivalence.md).

## Larger behavioral campaign

```sh
python3 tools/benchmarks/run.py rv32i \
  --output build/experiments/docs-core-gap --preset core-gap --timeout 300 -- \
  --clock-mode binary-safe --iterations 128 --size 16 --seed 1 \
  --allocation-limit-mib 512 --max-cycles 100000 --max-events 100000000 \
  --warmups 1 --repetitions 3
```

Omitting `--reference-check` retains the checked behavioral run and measured repetitions; it does not silently execute every large workload structurally. Every repetition starts a fresh simulated machine. Host warmups improve host measurement stability but do not train a later simulated predictor.

For external workloads, follow [the ELF contract and build-status notes](compiled/index.md). Start with one ELF and a bounded native run. Native `--allow-incomplete` can validate a prefix for calibration; a campaign requires complete, validated cases and does not turn a prefix into a successful full benchmark.

## Memory campaigns

```sh
python3 tools/benchmarks/run.py memory \
  --output build/experiments/docs-memory --timeout 300 \
  --memory-binaries MemoryStorageBenchmark
```

The available executables are `MemoryStorageBenchmark`, `MemoryEndpointBenchmark`, `FFMemoryBenchmark`, and `SRAMBenchmark`. Their built-in device cases share the common result envelope, not the RV32I instruction runner. A future cache or accelerator runner can use that envelope without pretending to execute RV32I instructions.

## Recorded artifacts

| Artifact | Contents |
| --- | --- |
| `provenance.json` | Command, case arguments, host, source/binary fingerprints, Git state; ELF hashes and directory build manifest when present |
| `clocks.json` | RV32I timing analysis, chosen periods, certification scope, reference checks |
| `manifests/` | Deduplicated RV32I circuit/profile manifests |
| `runs.jsonl` | RV32I reference, oracle-check, host-warmup, and measurement records, with phase labels |
| `benchmark-runs.jsonl` | Common schema envelopes extracted after successful RV32I execution; written directly for memory |
| `summary.json`, `summary.csv` | Per-case/target aggregates; RV32I host-time median/min/max and maximum observed RSS |
| `COMPLETE` | All selected campaign checks and repetitions finished |

Memory provenance records binary hashes and start time, but currently does not include the full RV32I source/profile/clock evidence. Do not infer those missing fields from the shared envelope.

Measurement repetitions omit repeated oracle execution and must match the checked run's signature. Their individual `validated` field need not be true: read the phase and its checked counterpart together. Across cores the driver checks workload/final-state fingerprints and retirement counts, not equality of cycles. It rejects source/binary changes during an RV32I campaign.

## Interpreting campaign output

Compare the same image, input, measurement window, memory configuration, and startup contract. Use cycles/CPI for scheduling and modeled ticks for clock-adjusted simulated execution; use host seconds/events/RSS for simulator cost. Observation counters overlap and cannot be added as a CPI breakdown. An explicit common `--period` is a scheduling experiment, not a certified hardware-speed claim.

A missing `COMPLETE`, timeout, budget exhaustion, or failed verifier is an incomplete campaign. Existing artifacts remain useful for diagnosis but are not a complete result set. Increase budgets deliberately after a small run; do not parallelize large structural builds/runs on a memory-constrained host.

Completed campaign coverage, measured values, clock certificates and their
source snapshots are reported in [§ 4.7](../experiments.md). Keep the
recorded fidelity and completion artifacts with each run when applying this
setup to a new checkout.

## Source and tests

- `tools/benchmarks/run.py`: common entry point and memory artifacts.
- `tools/rv32i-performance/experiment.py`: RV32I selection, calibration, checking, and recording.
- `tools/rv32i-performance/benchmark_suite.py`: the 15-family and 23-case panels.
- `BenchmarkCampaignCLITest`, `RV32IBenchmarkCLITest`: bounded campaign/artifact regression coverage.
- `FullEmbenchCampaignTest`, `BackgroundCampaignResumeTest`: frozen-campaign
  and recovery checks using small or synthetic fixtures; see the
  [infrastructure test reference](verification/infrastructure.md).
