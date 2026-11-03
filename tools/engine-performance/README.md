# Engine performance experiments

This is bounded native research equipment using the common benchmark result
envelope. Findings and every failed/rejected idea belong in the
[open performance record](../../docs/records/2026-10-26-engine-performance.md).
It does not change the live visualizer, frozen gameplay binaries or fidelity.

Start with an existing current native `SimulatorLib` build made with Python
deployment disabled. The script freezes its archive and source/include trees;
inspect the baseline build's compiler flags and ensure it matches those
sources before interpreting overrides. Selected-file experiments reuse the
remaining archive; `hot-o2` is not a whole-library optimization-level test.

```sh
python3 tools/engine-performance/experiment.py \
  --output build-milestone/my-engine-experiment --build-dir build-milestone \
  build baseline
python3 tools/engine-performance/experiment.py \
  --output build-milestone/my-engine-experiment build wire-fast
python3 tools/engine-performance/experiment.py \
  --output build-milestone/my-engine-experiment build hot-o2
python3 tools/engine-performance/measure.py \
  --output build-milestone/my-engine-experiment --name initial
```

For execution-loop CPU time separated from setup, build `baseline-cpu`,
`wire-array-cpu`, `profiled-o2-cpu` and `buckets-cpu`, then pass those names
with `--variants` and a fresh `--name` to `measure.py`. `profiled-o2-cpu`
requires the earlier `hot-o2` objects. CPU timing uses a common runtime object
across those variants and does not add fields to public structures.

Each command is logged before launch to its unique `attempts/*/receipt.json`.
Keep stdout/stderr, resource receipts, results, mismatches and partial matrices.
The compilation/measurement limits are 768 MiB RAM/address space, zero service
swap, 90% of one CPU and 180 seconds. Host guards require 1,536 MiB available
to start and stop jobs below 1,024 MiB. Jobs run serially; do not run separate
copies concurrently or compile while measurements are running. Use a fresh
output directory when changing probe/runtime source or the base archive.

Workload size and observation mode appear in each result. A completed guest
is independently checked at instruction/memory/device boundaries and against
explicit final register values. Candidate comparisons require identical
counters, state, observation digests and every full-trace file hash. Timings
do not enter those signatures. These short loops and gate microbenchmarks
do not replace full-game, all-timing, history, native/WASM or browser checks.

`sampled` links a diagnostic `-pg` driver against the existing archive for
gprof's executable PC histogram. It does not provide complete call counts or
shared-library CPU attribution. Keep its timings out of speedup comparisons.

`buckets` is a research prototype with per-thread side storage to avoid changing
Simulator layout while linking the frozen archive. That storage's lifetime is
not production-ready. Production adoption requires owned queue storage, a full
build and ordering/cancellation/history/lifetime coverage. No prototype is
installed into the normal engine by these commands.

## STL container review

`STLContainerProbe.cpp` compares immutable name lookups, ordered last-write-wins
change batches and packed/byte flags. `stl_experiment.py` also builds private
output-index/flag, disabled-history reuse and recorder-container prototypes.
It reuses the parent's frozen source/archive and resource/attempt logger.

```sh
python3 tools/engine-performance/stl_experiment.py --output build-milestone/my-stl-review build
python3 tools/engine-performance/stl_experiment.py --output build-milestone/my-stl-review micro
python3 tools/engine-performance/stl_experiment.py --output build-milestone/my-stl-review pool-micro
python3 tools/engine-performance/stl_experiment.py --output build-milestone/my-stl-review pic
python3 tools/engine-performance/stl_experiment.py --output build-milestone/my-stl-review engine-pic
```

`--parent` selects the original frozen experiment directory. The controlled
engine stage recompiles unchanged core objects as well as candidates with the
archive's `-fPIC` flags, separating a container change from compiler flag changes.
The initial `engine` stage is retained for historical exploratory reproduction;
use `engine-pic` for container attribution. A completed result cannot be
overwritten. Setup/finalization are outside execution-loop timings; full
process receipts remain available.

Each standalone result verifies its checksum. Integrated results require the
same counters/state and byte-identical traces. These checks do not establish
speedups outside the measured operation mix; the record preserves both size
crossovers and integrated regressions. Public ordered pin maps and the normal
engine's class layouts remain unchanged by the equipment.

## Gate input cache implementation controls

After changing IO/gate class layouts, rebuild the complete native archive and
its consumers before comparing implementations. Inspect
`CMAKE_CXX_COMPILER_LAUNCHER` as well as the cached compiler: this checkout's
native launcher substitutes Clang for most files. Pass that actual compiler
explicitly; the CMake compiler ID alone does not describe the archive.

```sh
python3 tools/engine-performance/gate_cache_experiment.py \
  --compiler /path/to/actual/clang++ build
python3 tools/engine-performance/gate_cache_experiment.py measure
```

These implementation-specific controls use the preserved pre-change files
and original frozen snapshot in `engine-performance-20261026`. The four
named/cache and original/reused-buffer combinations share the new headers,
archive, compiler, PIC and assertion flags. A separate `prechange` executable
uses the complete old headers and archive, allowing a net comparison that
includes cache object-size costs without mixing incompatible class layouts.
Keep both sources and archives with their hashes. Do not link old-layout
objects into a new-layout executable.

Measurements rotate variants over three repetitions, with and without full
tick capture, and reject any counter, state, timestamp or trace-file mismatch.
The execution-loop CPU clock excludes setup and final writer flush; process
resources include them. Build and measurement stages refuse to overwrite
completed results. Every subprocess uses the same serial resource logger;
a zero `systemd-run` exit alone is insufficient without a successful complete
timed-command receipt.

After the complete RV32IM matrix finishes, run
`python3 tools/engine-performance/gate_cache_fanout.py` for the bounded
64-inverter workload. It rotates the frozen, compatible named-lookup and
combined variants over three repetitions with no observer and with a full
change digest. It verifies counters, times and digests, and reports execution
wall time separately from whole-process CPU time. This isolates a gate-heavy
case; it cannot establish full-core or gameplay throughput.

## Current wire and timing experiments

`wire_timing_experiment.py` freezes the cache-enabled native baseline and builds
six private variants: unchanged, guarded typed wire comparison, timing scratch
reuse, both changes, and matched selected-file `-O2` controls. It uses actual
Clang with the same PIC/assertion flags and object order. No production source,
archive or live module is changed; template overrides require a full rebuild
before any production adoption.

```sh
python3 tools/engine-performance/wire_timing_experiment.py build
python3 tools/engine-performance/wire_timing_experiment.py verify
python3 tools/engine-performance/wire_timing_experiment.py \
  --name allocation-calibration --iterations 4 --repeats 1 allocations
python3 tools/engine-performance/wire_timing_experiment.py \
  --name throughput measure
```

`TimingTwinProbe.cpp` checks independent leaf truth and exact arrival signatures
across all timing profiles; `WirePayloadProbe.cpp` checks all supported widths,
four states, observation modes and derived-wire fallback. Verification also
uses existing gate, order, scenario and full-tick decoder equipment. The
allocation interposer adds overhead: its durations are never throughput data.

`twin_memo_experiment.py` tests per-owner exact-repeat memoization for stateless
models. `twin_stateful_experiment.py` adds guards for successful full clocked
answers, preserving unknown/reset fallback and the first falling-only early
return. Each exposes `build`, `verify` and `diagnostics` stages; the shared
`MemoTimingProbe.cpp` checks repeats, ownership, unknown clocks, resets and
clock pass-through. Retain negative hit-rate/allocation results and avoid
timing a cache with negligible eligible work.

For hosts with changing background load, use fresh baseline/candidate/candidate/
baseline blocks instead of interpreting a long unbracketed matrix:

```sh
python3 tools/engine-performance/paired_experiment.py --variants wire baseline-o2
```

This records fixed CPU affinity, CPU ticks/load/frequency/temperature snapshots,
execution CPU and wall time, complete semantic signatures and every raw result.
Endpoint baseline drift above the predeclared 5% threshold marks a block
unstable. Keep unstable blocks and candidate spread visible; stable endpoints
alone do not establish an otherwise isolated host. Use a fresh `--name` for a
new matrix. Never run compilation or another experiment alongside measurements.

## Incremental clocked timing models

`incremental_twin_experiment.py` reuses the current frozen archive and controls
under `wire-timing/`; it does not clone another native build. Its initial
variants separate owned node-buffer reuse from dirty propagation. Gate-only
clocked twins use shared CSR fanout indexes and per-owner dirty state;
stateless/leaf-containing models retain full traversal. Value **and arrival
time** changes propagate. Capture, reset, unknown-clock and falling-only
semantics retain their original order.

```sh
python3 tools/engine-performance/incremental_twin_experiment.py build
python3 tools/engine-performance/incremental_twin_experiment.py verify
python3 tools/engine-performance/incremental_twin_experiment.py diagnostics
python3 tools/engine-performance/incremental_twin_experiment.py build-bits
python3 tools/engine-performance/incremental_twin_experiment.py verify-bits
python3 tools/engine-performance/incremental_twin_experiment.py diagnose-bits
python3 tools/engine-performance/incremental_twin_experiment.py kernels
```

The ordered-mask version stores dirty topological positions in 64-bit words
and processes the next set bit, including consumers newly dirtied in the same
word. `IncrementalTimingProbe.cpp` adds feedback, reconvergent enable logic,
arrival-only changes, cold/interleaved owners and primitive fallback to the
existing three-profile timing checks. Diagnostics count logical node visits,
actual evaluations, cumulative CSR capacity and initialized dirty bytes; those
metadata totals are not peak RSS, and counter-build times are not throughput.

`TimingKernelCompare.cpp` places an unchanged reference in a distinct namespace
and alternates 32 ABBA/BAAB chunks within one process on structural 8/32-bit
registers. It validates every case before timing and each chunk endpoint.
An unchanged-vs-unchanged control exposes residual code/data-layout bias.
Kernel improvements cannot be presented as full-engine or game speedups.

Guest comparisons use explicit matching controls:

```sh
python3 tools/engine-performance/paired_experiment.py \
  --output build-milestone/engine-performance-20261026/wire-timing/incremental \
  --name paired-bitset --variants bitset
python3 tools/engine-performance/paired_experiment.py \
  --output build-milestone/engine-performance-20261026/wire-timing/incremental \
  --name paired-bitset-o2 --variants bitset-o2 --control baseline-o2
```

Completed private verification executables can be archived losslessly with
`archive_verification.py`. It excludes active/symlink files, checks every gzip
round trip, commits a hash/mode/timestamp index before unlinking the plain copy,
and preserves sources, objects, primary probes and all result/trace files.
Restore one indexed relative path with `--restore PATH`; restoration verifies
the original SHA-256 and refuses to overwrite a different file. All archiving
and restoration commands must use the same serial resource logger. This is
evidence storage management, not engine compression or throughput optimization.
