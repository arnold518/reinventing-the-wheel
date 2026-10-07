# Benchmark campaigns

This is the common serial entry point for processor and memory campaigns. It
does not hide domain behavior: the RV32I runner retires instructions, while the
memory runners execute request/response scenarios. Both emit benchmark result
schema 1 records in `benchmark-runs.jsonl`.

```sh
python3 tools/benchmarks/run.py rv32i --output build/experiments/programs \
  --preset program-regression -- \
  --cores single,baseline,advanced --warmups 0 --repetitions 1

python3 tools/benchmarks/run.py rv32i --output build/experiments/generated \
  --preset generated-regression -- --iterations 8 --size 8

python3 tools/benchmarks/run.py rv32i --output build/experiments/embench-crc32 -- \
  --elf build/rv32i-embench/artifacts/crc32/crc32.elf \
  --max-cycles 15000000 --max-events 1000000000 --timeout 3600 \
  --cores single,baseline,advanced --warmups 0 --repetitions 1 --reference-check

python3 tools/benchmarks/run.py memory --output build/experiments/memory
```

Every output directory is new and receives provenance plus `COMPLETE` only
after all selected processes succeed. Runs are serial. Structural fidelity is
never substituted automatically.

## Middle-scale compiled equivalence

The `compiled-middle` preset bridges the tiny compiled fixture and the full
Embench campaign. Build the three real algorithms (CRC32, SHA-256, SGLIB) with
their original inputs/verifiers, one complete kernel invocation and no warmup:

```sh
python3 tools/rv32i-benchmarks/build-embench-multilib.py --profile middle \
  --compiler build/toolchains/xpack-riscv-none-elf-gcc-12.4.0-2/bin/riscv-none-elf-gcc \
  --output build/rv32i-embench-middle/artifacts
python3 tools/benchmarks/run.py rv32i --preset compiled-middle \
  --output build/experiments/middle-equivalence
```

It runs **three programs × three cores = nine structural/behavioral paired
cases**, with advanced/two-bit by default. It uses the existing ELF loader,
`--reference-check`, certified clocks and result envelope. Five advanced
policies can be selected explicitly for 21 cases. The preset verifies the exact
reduced-work manifest and ELF hashes; it rejects full-sized images and PC-window
overrides. `--compiled-dir` selects another build of the same suite.

Default budgets are 512 MiB per process, 500,000 cycles, 20 billion events and
7,200 seconds per subprocess, serially. Complete structural comparisons can
take hours; do not mistake successful builds/preflights for those results.
No ordinary CTest automatically compiles or runs this long panel. These
`embench-middle-*.elf` programs are separately labelled reduced workloads,
not official/full Embench results. See [the reference page](../../site-docs/rv32i/4.2/compiled/middle-equivalence.md).

To run the nine-case panel independently in the background:

```sh
python3 tools/benchmarks/run-full-embench-background.py start --profile middle \
  --artifacts build/rv32i-embench-middle/artifacts \
  --output build/experiments/middle-equivalence
```

This uses the same frozen-source, serial, failure-isolated launcher described
below, with three core configurations, **two checked executions per case**
(behavioral and structural-reference), and one behavioral measurement. Its
queue and logs are independent of any full Embench campaign; it does not wait
for or modify that campaign. The structural-reference fidelity retains compact
FF storage. The middle preset's cycle/event/timeout caps also apply here.

## Full Embench background run

Use the [multilib port](../rv32i-benchmarks/README.md#full-suite-multilib-port)
to build all 19 workloads first. Build the small budget-planning helper with
`cmake --build build --target rv32i_cycle_estimate -j1`.

```sh
python3 tools/benchmarks/run-full-embench-background.py start \
  --output build/experiments/full-embench

python3 tools/benchmarks/run-full-embench-background.py status \
  --output build/experiments/full-embench
```

The launcher returns immediately. The detached worker runs **19 workloads ×
7 configurations = 133 cases**: single-cycle, baseline, and advanced with
not-taken, taken, backward-taken, one-bit, and two-bit prediction (16 entries).
Each case has one oracle-checked behavioral circuit execution and one unchecked
measurement whose result must match: **266 full executions**. This does not
repeat the already separate bounded structural-equivalence panels, and is not
a three-repetition statistical study or an official Embench score.

Before circuit execution, a fast instruction oracle checks each ELF's own
verdict and counts instructions. The baseline scoreboard estimator includes
RAW stalls and control-flow gaps. These numbers size cycle/event budgets;
they are **not measured circuit performance**. Generous per-workload wall-clock
timeouts scale from previous host costs. Unknown input or a failed verifier
is never converted to a passing measurement.

Execution is serial, with a 512 MiB address-space limit per native simulator,
no waveform history, and reduced worker scheduling priority. The frozen source,
ELFs, and native executables under `snapshot/` isolate the run from later edits.
Per-case results retain the existing common schema. Matching architectural
fingerprints are checked across all seven configurations of each workload.
Failures are recorded and later cases continue.

- `campaign.json`: queue, preflight counts, budgets, per-case status and rough host-time estimate.
- `campaign.log`: top-level progress; `cases/<workload>/<configuration>.log`: detailed errors.
- `worker.pid` / `worker.exit`: worker process and final exit code.
- `FINISHED`: all queued cases were considered, possibly with failures.
- `COMPLETE`: every case passed, including cross-configuration checks.

This is a multi-day serial job. Detachment survives closing the terminal, not
a machine reboot; after interruption, do not mistake a stale `running` field
for a live process. The status command also checks the PID and exit marker.
Existing result directories are never overwritten.
