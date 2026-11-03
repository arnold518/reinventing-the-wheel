# Engine performance investigation

Started: 2026-10-26. Status: open. Code state: `alpha` at `5549ee0`, with
the existing component, visualizer, profile and recording changes preserved.

## Summary

Objective: improve native throughput while preserving four-state values,
delays, ordering, cancellation, bounded memory and complete tick recordings.
Primitive gate caches and headless-buffer reuse are implemented in the native
baseline with focused validation; their measured whole-guest improvement is
small. Private wire and selected-file compiler controls show roughly 3% lower
CPU time in limited stable brackets; wire allocation requests fall 32%.
Scratch reuse and exact-repeat memoization are rejected for the measured guest.

The latest candidate uses incremental clocked-twin evaluation with ordered
64-bit dirty masks. It skips 66.1% of modeled node evaluations, with about
79 KiB of cumulative dependency/mask metadata in the short guest. All 82 latest
guests, 58 focused checks and 41 new trace packages match their controls/golden
data. Retained stable `-O1` guest blocks show 4.9% lower CPU without recording
and 4.2% with it, but each mode has only one stable block and streamed spread
is material. No matched `-O2` block is stable. Larger register-kernel gains
are not whole-engine speedups. All failed/unstable results remain recorded.

Production stays at the cache-enabled baseline; later candidates are private
experiments. No experiment OOM/swap occurs. Verification ELFs are archived
losslessly with restoration hashes to preserve disk headroom. Broader native/
WASM/workload acceptance and the owner's 10-million-events/second target remain
open. Live modules, finished captures and the stopped gameplay queue are kept.

The 2026-10-27 review puts current cost at about 1 µs of CPU per event, 10–12×
from the target. Measured allocation savings explain only a few percent of
that. The unverified hypothesis is that cache misses in a large pointer graph
dominate. The review lists new source candidates: splitter, joiner and Rewire
name work; `getenv` and temporary vectors on the publish path; 4-byte
`LogicValue` bits; one-word packed values; and inline heap keys. It sets out
phased work: evidence, exact cleanups, packed values, incremental twins, typed
routes, fewer events, then an indexed execution form. Seven owner decisions
are pending.

## 2026-10-26 — Scope and experiment plan

The [full-tick record](2026-10-25-full-tick-gameplay.md) establishes Snake's
4.797 billion processed events, 1.581 billion effective wire changes and
15.405 million saved timestamps. It also records the terminated background
queue; this investigation does not resume that campaign.

Inspect the heap scheduler, vector/allocation paths, fanout and hierarchy
propagation, repeated component evaluation, retained-history bookkeeping and
streamed trace writer. Consult primary simulator/compiler documentation for
alternative designs and their semantic constraints. Measure queue-only,
wire/gate and representative RV32IM workloads, separating trace overhead and
build flags. Keep setup, execution and postprocessing times separate, retain
event counts/checksums and report repeated-run dispersion.

Run builds and measurements serially with explicit memory and time bounds.
Hardware `perf` access may be restricted (`perf_event_paranoid=4`); verify it
before selecting a profiling fallback. Current native build configuration is
Release with `-O1 -g0 -DNDEBUG`, not the compiler's usual Release `-O3`.
Treat that as a candidate factor rather than a measured explanation.

Artifacts will live beneath `build-milestone/engine-performance-20261026/`;
copy key results and limitations into this tracked record. Any experimental
change must be measured against the same baseline and retain an exact
semantic comparison appropriate to its scope before being recommended.

## 2026-10-26 — Resource controls and attempt recording

The owner explicitly requires OOM precautions and retention of all experiment
and optimization attempts. `tools/engine-performance/experiment.py` records
each command before launching it, then retains stdout, stderr, `/usr/bin/time`
resources, exit status and timestamps in a unique attempt directory. Completed
attempts also enter `ledger.jsonl`; interrupted/deferred receipts remain in
their directories. Compile and measurement jobs run serially in separate user
systemd services with 768 MiB MemoryMax, 640 MiB MemoryHigh, zero permitted
swap, a 768 MiB address-space limit, 90% of one CPU and 180-second wall limits.
Start requires 1,536 MiB host available; stop below 1,024 MiB. A 60-second
headroom wait becomes a recorded resource deferral rather than an indefinite
wait. No full library rebuild, background gameplay or browser job is started.

Freeze the current library, include/source trees and probe into the ignored
artifact directory with SHA-256 provenance. The existing shared benchmark
`RunRecord` envelope carries measurements. Workloads cover queue depths,
1/32-bit wire changes and no-ops, 64-way gate fanout and a complete freestanding
RV32IM multiply/divide/remainder/load/store/branch loop. The guest retains
instruction, memory and device oracle checking and asserts its final registers.
Compare no observer, a lightweight observation digest and the actual bounded
gzip tick writer. Three interleaved repetitions will check event counters,
final state, observer digests and byte-identical full traces across candidates.

Initial `perf stat -e task-clock -- true` fails with status 255 because host
`perf_event_paranoid=4`; no host permission setting is changed. A first console
summary used `host_seconds` instead of the existing envelope's `hostSeconds`,
raising KeyError after the queue measurement had passed and been saved. The
summary was corrected; both attempts are recorded in `initial-attempts.json`.

## 2026-10-26 — Primary-source research and first profile

[Icarus's scheduler source](https://github.com/steveicarus/iverilog/blob/master/vvp/schedule.cc)
uses a time-step object containing event lists and slab allocators for several
event kinds. This motivates investigating shared timestamp buckets and bounded
event allocation pools here. It does not establish equivalent scheduling or
performance for CircuitSim; ownership, cancellation and wire-before-evaluation
ordering must be retained.

[Verilator's scheduling internals](https://github.com/verilator/verilator/blob/master/docs/internals.rst)
describe static ordering and trigger-based activation. [Yosys CXXRTL](https://yosyshq.readthedocs.io/projects/yosys/en/0.40/cmd/write_cxxrtl.html)
documents flattening and internal-wire localization/inlining while keeping
hierarchical debug names. These suggest an indexed execution representation
separate from the component/view hierarchy. Their compiled RTL evaluation
contracts are not a direct replacement for our delayed four-state gate events.

[GCC optimization documentation](https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html)
lists additional inlining and devirtualization at `-O2`, motivating a bounded
selected-file experiment against the current `-O1` library. Avoid rebuilding
large generated timing tables merely to investigate hot loop code.

Hardware sampling is unavailable, so a `-pg` diagnostic executable uses
[GNU gprof's PC histogram](https://sourceware.org/binutils/docs/gprof/Implementation.html).
The existing archive is not rebuilt with call hooks: call counts and inclusive
call graphs are incomplete, and dynamically linked library costs are outside
the executable's PC histogram. Sampling includes setup. Do not interpret flat
percentages as shares of complete process time or use the sampled binary as
the throughput baseline.

A 16-iteration Visual guest completes 2,000 cycles / 117 instructions and
14,302,358 events with the oracle checks passing. Its first flat profile has
13.02% of executable samples in GateTwin settling, 12.70% in string-keyed pin
map lookup, 10.57% in heap adjustment, 5.80% in BasicComponent evaluation,
5.03% in fanout propagation and 4.58% in heap insertion. These broaden the
initial vector/queue hypothesis: proven-delay refinement and pin-name lookup
also merit investigation. The profile's sampled self time totals about 15.5
seconds versus 22.03 process CPU seconds, so shared-library/unattributed time
is material. The diagnostic run peaks at 122,160 KiB with zero swaps.

Calibration queue-only throughput is about 3.58 million events/second at depth
4,096. A one-iteration Visual guest without observation processes 1,255,354
events in 2.20 execution seconds (setup 1.60 seconds). These short calibrations
are not repeated estimates or a full-game projection.

## 2026-10-26 — Candidate ledger

| Candidate | Current experiment | Correctness boundary |
| --- | --- | --- |
| Skip wire vector construction/comparison when unchanged | `wire-fast` overrides only event implementation/templates in an experimental binary; repeated measurements in progress | Preserve exact four-state changes, counters, order and full trace bytes |
| Optimize selected hot files at `-O2` | `hot-o2` rebuilds Simulator, Event, Wire, Pin, BasicComponent, Gate and explicit width instantiations; repeated measurements in progress | Same archive remainder, fidelity, timing and workload; no all-library or `-O3` claim |
| Shared timestamp buckets / cheaper priority handling | Research candidate, not implemented | Stable insertion order, zero-delay wire preemption, cancellation and quantum boundaries |
| Event pools / compact signal representation | Research candidate, not implemented | Shared ownership and pending-event lifetime, four states, bounded memory |
| Cached pin routes / pin indexes | Profile-supported candidate, not implemented | Preserve component identity, parent wiring and topology rebuild behavior |
| Incremental delay-twin evaluation | Profile-supported candidate, not implemented | Preserve data-dependent timing, setup checks and sequential timing state |
| Evaluation coalescing / redundant-update suppression | Source-supported candidate, not implemented | Must distinguish repeated no-op work from required intermediate transitions and edge-sensitive side effects |

## 2026-10-26 — First repeated matrix and measurement correction

Complete 54 interleaved measurements: six workloads, three variants and three
repetitions. All event counters, guest final state, observer digests and all
full-trace file hashes agree across variants/repetitions. Nine actual streamed
guest traces compare byte for byte. No measurement OOM, guard stop or swap is
reported in this matrix.

The initial per-bit virtual `wire-fast` check is not accepted as an optimization:
the unchanged 32-bit median is 1.739 seconds versus baseline 1.662 seconds, and
fanout median is 0.644 versus 0.600. Its RV32IM results do not establish a gain.
The selected-file `hot-o2` trace median is 7.578 versus 8.156 seconds, but this
small apparent improvement is not persuasive under the observed host variance.
Baseline guest/no-observer execution ranges from 4.083 to 6.841 seconds; full
streaming ranges from 5.440 to 8.555. Full-process CPU receipts also vary, so
host contention/processor conditions affect more than time spent descheduled.
Retain these results at `measurements.json`; do not promote median ratios to a
full-game speedup or select a winner from this phase.

Refine the next measurement phase with a common experimental runtime object
that samples process CPU time at the existing execution-loop boundaries and
prints it separately from wall time and setup. This changes no event work or
public structures. Use new `*-cpu` executable identities and a separate
measurement filename, preserving every first-phase binary/result.

Add three candidates to the second phase: `wire-array` compares the existing
typed wire array directly before allocating change values; `profiled-o2` adds
the sampled DelayTwin and IOComponent files to the earlier selected hot-file
build; `buckets` groups equal-time events in separate wire/evaluation FIFOs.
The bucket prototype preserves class layout by using research-only thread-local
side storage, rather than relinking old objects against a changed Simulator
layout. Its storage ownership/destruction is not production-ready; production
would need an owned queue member and full build/coverage. It remains a bounded,
single-simulator experiment even if the timing results look favorable.

The first bucket prototype compile fails because a source-rewrite helper also
replaced the `event_queue` substring inside `maximum_event_queue_depth`.
The attempt is retained (`1792984064977401629-buckets-cpu-compile-Simulator`,
exit 1, peak 159,016 KiB, zero swaps). Correct the rewrite to match the complete
identifier only; the subsequent compile/link passes. This is an equipment
generation failure, with no production source or simulation result changed.

## 2026-10-26 — Second matrix results

Complete 72 further interleaved comparisons: six workloads, four variants and
three repetitions. All event counters, final states and digest comparisons
pass; twelve streamed captures are byte-identical across the candidates.
The complete guest in each case is 512 CPU cycles, 33 retired instructions,
3,853,569 events, 2,739,521 wire updates, 1,114,048 evaluations and 1,195,599
effective wire changes. Streaming retains 16,950 timestamps. This is a small
M-operation/memory loop, not Snake or a full drawing/game workload.

The following are medians over three repetitions, in seconds. CPU time is
sampled inside the runtime execution loop. Construction, ELF loading,
VisualizerSession setup, final writer flush and result serialization are
outside that interval; full-process resource receipts retain those costs.

| Variant | Guest, no observer CPU / wall | Guest, digest CPU / wall | Guest, stream CPU / wall |
| --- | ---: | ---: | ---: |
| Baseline `-O1` | 5.563 / 6.235 | 5.319 / 5.918 | 7.047 / 7.948 |
| Direct array wire comparison | 5.595 / 6.257 | 5.148 / 5.718 | 6.068 / 7.669 |
| Nine selected files at `-O2` | 4.939 / 5.497 | 4.784 / 5.314 | 5.759 / 6.485 |
| Timestamp buckets, research storage | 5.156 / 5.733 | 4.634 / 5.182 | 5.879 / 6.618 |

Selected-file `-O2` reduces median execution CPU time by about 11% without
observation and 18% with streaming (about 13% and 22% more events per CPU
second). Each paired repetition improves, with substantial variation: CPU
reductions span 10.1–21.0% without an observer and 7.4–18.3% with streaming.
This is promising bounded evidence, not a confidence interval, whole-library
build result, full-game speedup or a new timing/fidelity setting. It recompiles
only Simulator, Event, Wire, Pin, BasicComponent, Gate, explicit width
instantiations, DelayTwin and IOComponent; generated delay tables stay untouched.

Direct array comparison improves the 32-bit unchanged-wire microbenchmark
median from 1.477 to 1.045 seconds and fanout from 0.677 to 0.608. Integrated
CPU-time gains are small/inconsistent without tracing. Retain it as a proven
allocation reduction with workload-dependent timing benefits rather than
claiming a universal engine speedup.

## 2026-10-26 — Allocation and sparse-queue diagnostics

`AllocationCounter.cpp` interposes C++ allocation operators only, enabled
inside the same execution-loop interval. Atomic diagnostic counters add
overhead, so these four runs are allocation measurements, not throughput
comparisons. `requestedBytes` is cumulative allocation traffic, not peak live
memory, and direct C allocations inside external libraries are not counted.

| Observation | Baseline C++ requests / bytes | Array comparison requests / bytes |
| --- | ---: | ---: |
| None | 13,307,473 / 575,543,039 | 9,024,030 / 503,170,927 |
| Stream | 14,547,309 / 644,147,481 | 10,263,866 / 571,775,461 |

The array candidate removes exactly 4,283,443 requests: 32.2% without a trace
or 29.4% with a trace. This equals twice the wire-update count minus effective
wire changes, matching the two temporary vectors per old update versus one
only for changed values in the candidate. This independently confirms both
the allocation mechanism and use of the candidate template in the linked
binary. Large allocation reduction alone does not establish equivalent wall
speedup; pin lookup, scheduling, propagation and delay-twin work remain.

Twelve queue-stress runs expose a decisive bucket tradeoff: two million events
with distinct timestamps, at two depths and three interleaved repetitions.

| Queue depth | Heap median seconds | Bucket median seconds | Bucket / heap time |
| --- | ---: | ---: | ---: |
| 64 | 0.414 | 0.923 | 2.23x |
| 4,096 | 0.443 | 1.516 | 3.42x |

The current bucket/map/deque implementation must not replace the general queue
on the strength of its dense guest results. Investigate a hybrid or pooled
time-slot representation and measure actual distinct-time occupancy first.
The research side-storage lifetime also remains unresolved for production.

## 2026-10-26 — Correctness, resources and decisions

For each of `wire-array-cpu`, `profiled-o2-cpu` and `buckets-cpu`, five existing
focused native checks pass: WireTemplateTest, SimulatorAdvanceAndRecordTest,
SimulatorUnwiredOutputPinHistoryTest, SimulatorDrainUntilIdleTest and NOTGateTest.
The existing TickRecordingTest also passes on each relinked SignalTraceProbe:
all 1,202 retained-history timestamps, four-state 8/32-bit buses, uint64 time,
pin-only future publications, cancellation, memory/register deltas and chunk
seek/rewind match decoded output. These fifteen native and three decoder runs
are scoped checks, not complete native/WASM/browser suites.

An additional independent ordering contract passes for both baseline and
buckets: FIFO among equal priorities, a newly generated zero-delay wire update
preempts an already waiting evaluation, a canceled event is skipped, uint64
time remains intact, single-event quantum frontiers are correct, and clear
resets pending work/time/counters. No scheduler class layout was changed.

The command audit contains 214 completed receipts: 213 successful commands
and the one retained prototype-generation compile failure. The denied perf
probe and initial console-summary error are separately retained as initial
attempts. No experiment job OOM, host-guard stop, resource deferral or swap is
observed. Maximum process RSS is 348,096 KiB (about 340 MiB), during a compile.
Node decoder checks retain the 768 MiB RAM / zero-swap service cap and host
guards but omit the address-space cap for V8's virtual reservations; that
exception appears explicitly as `addressSpaceMiB: 0` in those receipts.
Artifacts occupy about 851 MiB, including the frozen archive and raw traces.

Tracked equipment/procedures now live in `tools/engine-performance/` and the
benchmark reference links them. Raw receipts/results are in the artifact
directory's `attempts/`, `ledger.jsonl`, `attempt-index.json`, `audit-summary.json`,
`measurements.json`, `phase2-cpu.json` and `queue-stress.json`. Frozen executable
and source hashes, samples, failed logs and all trace bytes remain available.

Priorities for the next bounded implementation work:

1. Validate a dedicated performance build with stronger optimization on small
   hot engine files while retaining memory-safe compilation of generated tables.
2. Carry the allocation-free unchanged-wire comparison into a complete build
   and broaden width/history/inertial/timing coverage before production use.
3. Measure pending timestamp occupancy and evaluation multiplicity; investigate
   an owned hybrid scheduler and bounded event allocator with lifetime tests.
4. Replace repeated string pin lookup with stable cached pin/route handles, and
   investigate incremental/cached delay-twin evaluation while preserving
   data-dependent delays, sequential state and setup checks.
5. Compare on another complete program with different instruction/memory mix
   before projecting any result onto Snake. Use paired CPU and wall times and
   retain noisy or regressing results. Coalescing evaluations or suppressing
   future updates remains a semantic change needing dedicated evidence.

Production engine sources, live native/WASM modules, fidelity/timing contracts
and recorded games are not changed by these prototypes. The gameplay campaign
remains stopped. Full-game reruns, full suites, all timing profiles, optimized
WASM, event pools, pin caching, delay-twin changes and a 10-million-event/second
engine result remain unrun or unimplemented.

## 2026-10-26 — Final equipment verification

Equipment Python syntax and scoped diff checks pass. The two complete
matrices' trace hashes also agree with each other, confirming all 21 streamed
captures are byte-identical across both phases. Hash every frozen `include/`
and `src/` file against the current checkout and compare the baseline archive:
none changed during this investigation. Save that receipt and the final
equipment sources/hashes in `final-verification.json` and `equipment-final/`.
The two final equipment checks bring the command audit to 216 receipts:
215 passed and the one documented prototype compile failed. Resource maxima
and zero-swap findings are unchanged; no experiment services remain running.

Configuration wording: Release is the CMake configuration name. The verified
baseline flags are the cache's `-O1 -g0 -DNDEBUG`, with the library's `-UNDEBUG`
keeping assertions enabled. No claim depends on a compiler-defined Release
preset or an unmeasured comparison against a full `-O3`/LTO build.

## 2026-10-26 — STL container review: scope and new experiments

The owner requests a situation-by-situation review of STL choices. Continue
this open record and preserve every earlier experiment. Inspect scheduler,
pin/component maps, timing tables/flags, wire fanout, native memory, retained
histories, trace assembly and runtime/visualizer containers. Distinguish hot
lookups from build-only metadata and ordered inspection contracts before
proposing changes. A hash table is not a general replacement for a tree, and
a vector-based table can lose at large cardinality or frequent middle inserts.

Use a separate `stl-review/` artifact directory beneath the existing frozen
experiment. Retain the earlier 768 MiB RAM, no-swap, serial execution and host
headroom guards. Plan standalone repeated tests for small string lookups,
per-timestamp ordered change assembly and flags; test actual runtime variants
where private implementation data can change without changing class layout.
Candidates are a contiguous small output-index table, byte timing flags and
sorted vector trace changes. Preserve exact last-write-wins behavior, sorted
delta encoding, unique pending-count limits and trace bytes. Do not change
public pin-map types, sparse-memory layouts or Simulator layout while linking
against the frozen archive. No new speedup is claimed before measurement.

## 2026-10-26 — Container requirements and initial STL measurements

The source inventory indexes 139 declarations/usages in eleven engine,
component, memory, runtime and recorder files; it is an index, not a count of
139 distinct container objects. `stl-review/container-inventory.json` retains
paths and line numbers. The following decisions distinguish storage form from
required behavior. "Candidate" means an implementation experiment or future
change, not a production modification.

| Situation / source | Existing choice and requirement | Decision / replacement candidate |
| --- | --- | --- |
| Scheduler, `Simulator` | `priority_queue<shared_ptr<Event>, vector<...>>`; earliest time, wire priority, stable scheduling order, cancellation | Keep vector-backed heap as general fallback. Investigate an owned hybrid for dense same-time work; do not adopt the previously slower sparse-time bucket queue. `deque` does not supply priority ordering by itself. |
| Event ownership | Shared event objects, weak inertial-cancellation handles | Keep lifetime guarantees. A reusable slot/pool or compact event record needs generation/lifetime coverage; switching to `unique_ptr` breaks weak handles. |
| Pin/wire values | Fixed-width `array<LogicValue,W>`, frequently converted to vectors | Keep fixed arrays; compare/assign fixed payloads directly or use packed four-state values. Avoid temporary vectors in propagation. |
| Wire fanout, component children/wires | `vector<weak_ptr<PinBase>>` and vectors of owned children/wires; ordered scans | Keep contiguous vectors. Cache validated routes or move expired-sink cleanup out of the hot path with topology invalidation. `list` would add nodes and pointer traversal without a needed splice operation. |
| Public IO pin tables | `map<string,shared_ptr<PinBase>>`; getters expose exact map type and deterministic traversal | Keep public ordered tables initially. Cache pin handles/indexes for execution. A wholesale hash replacement requires API and topology/fingerprint-order migration. |
| Timing output names, private `TimedOutputs` | `unordered_map<string,size_t>`; built once, tiny immutable lookup table | Test contiguous linear/indexed lookup; hashing is not automatically cheapest for one or two outputs. No external map-reference contract blocks a private change. |
| Timing control cases | `map<uint64_t,Matrix>`; ordered construction and control-code lookup | Candidate sorted vector, or bounded dense lookup if actual codes are dense. Do not allocate up to an arbitrary 64-bit key. |
| Timing matrices and saved input bits | Nested vectors; rectangular repeated scans and dynamic-width input payloads | Candidate flat vectors plus row offsets/spans and reusable input buffers. Preserve all dimensions, control cases, setup state and delay-twin contracts. |
| Timing flags | Several private `vector<bool>` objects; repeated small updates | Test `vector<uint8_t>` for small hot flags. Packed bits remain appropriate for large bitsets where space/cache dominates. |
| Pending inertial changes | Small vector of output names and per-output pending vectors | Keep small vectors; candidate cached output index and expired-prefix cursor. Do not turn a handful of entries into a heap/hash table without measurement. |
| Wire/pin histories | `map<shared_ptr<signal>, vector<time,value>>` | Keep append-only time vectors for binary search. Candidate reserved hash/indexed outer lookup: key ordering is not needed for lookup, but state restoration must retain wire-then-explicit-pin phases and stable value ownership. |
| Headless pin state | Tree maps each pin to a one-entry history vector; clear/push destroys its nested value vector on change | First reuse the existing entry/value capacity. A future hash/indexed packed last-value table can remove history shape entirely: `signalHistory` returns empty while histories are disabled. Requires a complete Simulator layout migration. |
| Unique timestamp query | `set<time>` assembled from all retained histories | Keep ordering. A bounded collect/sort/unique or k-way merge is a candidate; blindly collecting every duplicate change in one vector can consume much more RAM than the current unique set. |
| Dense native RAM | `vector<PackedByte>` for data/known masks | Keep. Current game RAM uses this branch, so sparse-page map changes do not accelerate that storage path. |
| Sparse native RAM | `map<page,vector<PackedByte>>`; resident-only allocation | Reserved hash pages or a budgeted two-level page table are candidates. A capacity-sized page-pointer array can defeat sparse-memory limits for large address spaces. Page ordering is not required for ordinary byte lookup. |
| Memory history/touched words | Sparse byte-history maps, append-only entry/reset/write vectors and sorted touched-word set | Keep time vectors. Candidate hash history lookup and dense bounded dirty bitmap plus sorted-query view. Preserve ascending first-N touched-word output and reset/history-epoch semantics; sparse configurations need sparse indexes. These structures are bypassed with history disabled. |
| Trace pending times | Outer `map<time,...>`; consume earliest complete timestamp | Keep ordered time handling initially. An unordered outer map alone cannot provide the required next time. |
| Trace changes within a time | Inner `map<signalID,value>`; unique IDs, last-write-wins, ascending delta encoding, unique-entry budget | Compare sorted vectors for small rows; hash-plus-sort or append/merge for large rows. A duplicate-appending design also needs bounded raw-buffer/unique-entry accounting. Test a pooled ordered map separately. |
| Trace signal indexes | `unordered_map<shared_ptr<pin/wire>,size_t>`; large immutable point lookup | Keep hashing initially; consider reserve/load factor or stable execution indexes. Sorted iteration is unnecessary. Keep vectors for widths/state and bytes, and avoid huge address-sized direct pointer tables. |
| Trace register aliases | `map<string,size_t>`, reconstructing component IDs while walking parents | Candidate object-handle hash map with IDs saved once in auxiliary metadata. Avoid repeated path-string construction while retaining alias traversal and serialized IDs. |
| Builder/catalog/profile metadata | Builder name maps, ordered parameter maps and ordered profile-rule vectors | Mostly construction work. Hash name lookup may improve setup, but preserve canonical serialization and last-match-wins rule order; it is not an event-throughput optimization. |
| Runtime oracle queues/maps | Small `deque` device/transaction FIFOs and string/address maps | Deques fit front-pop/back-push. Cached host pin/port handles are possible; distinguish reference-equipment costs from the simulated gate/device implementation. |
| Visualizer indexes and sorted reports | Maps/hash maps/vectors for topology/inspection | Preserve reproducible IDs/order and bounded caches. Optimize separately from engine execution; do not attribute UI-only work to native event cost. |

The [standard associative requirements](https://eel.is/c++draft/associative.reqmts)
and [unordered requirements](https://eel.is/c++draft/unord.req) explain why
ordering, average versus worst-case lookup and iterator stability are separate
constraints. Hash-table rehash changes iteration order and invalidates iterators
while preserving element references. `std::flat_map` is not a C++20 drop-in:
the current GCC 14.2 toolchain predates libstdc++'s reported 15.1 implementation
in its [status table](https://gcc.gnu.org/onlinedocs/libstdc++/manual/status.html).
Prototypes therefore use ordinary vectors with explicit lookup/sort operations.

Complete 189 interleaved standalone comparisons over three repetitions, with
checksum validation and identical operation counts within each case. These
are short container microbenchmarks compiled at `-O2`, not full-engine speedups.
Representative median CPU times in milliseconds:

| Immutable name table | Tree | Reserved hash | Sorted vector | Linear vector | Already-resolved index |
| --- | ---: | ---: | ---: | ---: | ---: |
| 2 names, 1M lookups | 43.013 | 27.650 | 39.118 | 27.263 | 9.521 |
| 8 names, 1M lookups | 67.170 | 44.017 | 54.150 | 40.364 | 9.647 |
| 128 names, 1M lookups | 142.926 | 61.518 | 136.644 | 384.969 | 9.793 |
| 1,024 names, 0.2M lookups | 46.215 | 17.584 | 40.676 | 680.283 | 2.096 |

Resolving names once is a different execution API, so the indexed column is a
target for cached handles, not evidence that replacing map with vector alone
produces that speedup. Linear lookup is not suitable for the large-table case.

| Random per-time change rows | Tree | Hash + sorted output | Sorted insertion vector | Append + stable sort/dedup |
| --- | ---: | ---: | ---: | ---: |
| 8 distinct signals | 47.142 | 62.848 | 26.328 | 20.932 |
| 64 distinct signals | 52.293 | 66.182 | 25.868 | 26.339 |
| 512 distinct signals | 108.568 | 98.366 | 101.957 | 32.996 |
| 4,096 distinct signals | 165.201 | 104.758 | 395.902 | 80.215 |

Each row has approximately 1.05M updates, including a duplicate pass, and
ordered last-write-wins checksum verification. This demonstrates why a
sorted-insertion vector should not unconditionally replace every trace row.
The append/sort test bounds total callbacks explicitly; a production trace
writer must separately preserve its pending unique-entry and raw-storage caps.

Ten million flag updates take 123.689 versus 86.487 ms for 32 packed-bit/byte
flags, 135.717 versus 95.498 ms for 256, and 121.357 versus 92.827 ms for 4,096.
These results motivate testing byte flags inside BasicComponent; they do not
establish an integrated timing-kernel gain.

## 2026-10-26 — Compiler-control correction for container attribution

Inspection of the actual CMake library flags finds `-fPIC` on the frozen
archive's translation units, while the initial override compile commands omit
it. This is an additional code-generation difference, not an algorithm change.
The earlier prototype observations and correctness checks remain valid for
those binaries, but small differences cannot be attributed exclusively to the
container changes. In particular, the earlier "selected-file -O2" comparison
is a selected-file compiler-configuration comparison, not an isolated `-O1`
versus `-O2` experiment. This corrects that attribution in the earlier summary
and experiment sections; their original timings and all command receipts remain.

Standalone container comparisons all use the same `-O2` build and are not
affected. Retain the first STL integrated matrix as `stl-review/engine.json`
and mark it exploratory. Add `pic-control/` with unchanged BasicComponent and
Simulator control objects plus candidate objects compiled using the library's
`-std=gnu++20 -O1 -g0 -DNDEBUG -fPIC -UNDEBUG` flags. Every controlled binary
links the appropriate same-flag pair of core objects and the same execution
timer; trace-only variants differ only in their recorder driver/header. Rerun
the three-repetition matrix into `engine-pic.json` without overwriting earlier
artifacts. Use these controlled results for subsequent container decisions.

The installed libstdc++ implementation adds a useful nuance: `hash<string>`
is classified as a slower hash and `unordered_map::find` linearly scans tables
of at most 20 entries (`bits/hashtable_policy.h`, `bits/hashtable.h` and
`bits/basic_string.h`). A tiny `unordered_map` is therefore not necessarily
hashing on every lookup in this build. The linear-vector experiment primarily
changes layout/locality in that regime. This is an implementation observation,
not a portable C++ container guarantee. The installed GCC 14 include tree also
lacks `<flat_map>`, confirming the toolchain constraint directly.

Forty-eight additional paired same-binary tests compare ordinary tree changes
against `pmr::map` with a reused `unsynchronized_pool_resource`. The random-row
median CPU times (ordinary / pooled milliseconds) are 33.325 / 61.749 at 8
signals, 61.514 / 73.349 at 64, 124.274 / 150.579 at 512 and 205.854 / 215.187
at 4,096. The particular pool is slower in this standalone operation mix;
pooling is not accepted as a general container speedup. The integrated writer
remains a separate experiment because allocation order and simultaneous
future rows differ. Pool options cap chunks at 256 blocks and pool block sizes
up to 128 bytes; the writer retains its unique pending-entry limit and outer
time ordering. Its pool is declared before pending maps so all maps are
destroyed before their resource.

[Pool resource requirements](https://eel.is/c++draft/mem.res.pool) describe
reusable blocks and destruction behavior. A whole-recording
[monotonic resource](https://eel.is/c++draft/mem.res.monotonic.buffer) would
retain deallocated row nodes until release/destruction and is unsuitable for
the unbounded duration of the gameplay capture without explicit epochs.
Reusable pools still require a bounded upstream budget, safe release points
and event/reference lifetime discipline before production adoption. No
monotonic whole-run allocator is enabled by these experiments.

## 2026-10-26 — STL integrated controls, shape and final decisions

The STL review completes 42 initial exploratory and 42 matching-PIC integrated
comparisons, each with three rotated repetitions, seven variants, and no
observer / actual streaming observation. All counters and architectural state
match; the 42 streamed recordings agree byte for byte, including against the
earlier 21 baseline-matrix recordings. Use `engine-pic.json` for attribution;
the initial STL matrix is retained and superseded for that purpose.

Matched-PIC median execution CPU seconds are:

| Prototype | No observer | Stream | Stream / baseline time |
| --- | ---: | ---: | ---: |
| Unchanged core controls | 4.54817 | 5.02627 | 1.000 |
| Linear private output index | 4.56673 | 5.43649 | 1.082 |
| Byte timing flags | 4.49323 | 5.37163 | 1.069 |
| Sorted vector trace rows | 4.51128 | 5.21210 | 1.037 |
| Object-key trace register lookup | 4.37409 | 5.13699 | 1.022 |
| Reuse headless pin value buffer | 4.38657 | 5.40120 | 1.075 |
| Pooled ordered trace rows | 4.60475 | 5.04064 | 1.003 |

These data do not establish an integrated throughput win for any simple swap.
Even trace-only builds vary with observation disabled, demonstrating remaining
host/code-layout noise. For example, the stream baseline spans 4.97789–5.24032
CPU seconds, headless reuse spans 4.89378–5.42420 and pooled rows span
4.88720–5.04197. Retain improvements in individual runs and regressions alike;
do not turn the standalone lookup/flag speedups into an engine speed claim.

An allocation diagnostic confirms the headless reuse mechanism independently:
the same guest requests 13,307,473 C++ allocations in the control versus
13,282,636 with reuse, removing 24,837 requests and 112,116 requested bytes.
This is only 0.19% of this workload's C++ allocations, explaining why it is a
small cleanup opportunity rather than the main engine optimization. The
diagnostic's atomic counting overhead is excluded from throughput conclusions.

A separate baseline writer diagnostic records real container shape without
using its timings as performance evidence:

- 16,950 rows with 1,222,727 unique changes, mean 72.14 per row; maximum 6,453.
- Row counts in bins 0 / 1–8 / 9–32 / 33–128 / 129–512 / 513–4,096 / over
  4,096 are 0 / 3,296 / 5,267 / 6,313 / 1,781 / 290 / 3. Row frequency is
  not the same as weighted change-processing cost.
- At most six pending timestamps: keep the outer ordered-time map initially.
- Signal lookup tables contain 109,674 pins in 172,933 hash buckets and
  57,925 wires in 85,229 buckets; load factors about 0.63 and 0.68 do not
  indicate an obviously undersized table.
- There are two register aliases and two native memory stores. A tiny identity
  vector plus cached IDs is a plausible alternative to either string trees
  or hash maps here, but register/memory callbacks are a smaller cost than
  the bulk pin/wire path.

The diagnostic trace also matches the golden binary files. Its row-size
distribution supports an adaptive row representation: many rows are small,
while the large startup rows rule out assuming every row has at most 64 IDs.
Before adopting append/sort, preserve the unique-entry limit and add a separate
bound for duplicate raw callbacks. Before adopting a pool, bound retained
upstream memory and compare against the ordinary allocator on the actual mix.

Twelve focused native checks pass on matching-PIC output-index, byte-flag and
headless prototypes, including DFlipFlopTest, RegisterTest,
RV32IALUAddSubTest and the relevant simulator history/drain/width checks.
Three recorder variants pass the existing full TickRecordingTest on freshly
compiled candidate SignalTraceProbe fixtures, covering all 1,202 timestamps,
four states, wide time, memory/register changes and seek/rewind. These fifteen
checks are additional to the earlier focused checks, not a full suite or
native/WASM/timing-profile acceptance claim.

Current recommendation, by priority:

1. Preserve the public sorted pin maps and build cached pin/route handles or
   execution indexes so ordinary gate evaluation does not repeatedly resolve
   names. This addresses a sampled hot path and avoids changing deterministic
   API/serialization behavior. A full implementation/build is still required.
2. Remove transient value-vector copies through typed arrays or immediate
   spans where ownership allows, and reuse existing buffers. Delayed event
   payloads and retained histories must own snapshots; views into live signals
   cannot replace those copies safely.
3. Use size-aware container policies for recorder rows, control cases and
   sparse indexes. Keep the general heap fallback, healthy large signal hash
   indexes, dense RAM vectors and append-only history vectors.
4. Do not replace tiny output maps, all packed flags, all trees or all
   allocators globally: the controlled engine results do not support it.
   Sparse-memory and builder metadata changes should be measured on workloads
   that actually use those paths; they do not accelerate current dense game RAM.

All STL-review attempts, including the superseded non-PIC comparisons, remain
under `stl-review/attempts/` and `ledger.jsonl`; no completed capture is deleted
and no production engine or fidelity/timing contract is changed. The measured
container kernels explain choices; they do not demonstrate the owner's full
engine throughput target.

One further timing-container situation is distinct from the already contiguous
gate lists: `memory_device::Primitive::Values` is a name-to-bit-vector map and
`Deciders` is a name-to-set-of-input-names map (`DigitalMemory.hpp`).
`GateTwin::settleLeaf` repeatedly builds these dictionaries and may evaluate
several trial input assignments. Indexed port buffers and bounded input-depends
bit masks are candidates for that execution path, with an adapter retaining
the named public interface. Unknown/opaque values, per-bit settling decisions
and sequential timing state must remain equivalent. These containers were
source-reviewed, not changed or performance-tested in this STL phase. The
gate/node order and byte-state vectors already used by GateTwin remain suitable.

## 2026-10-26 — STL review final verification and resources

The new review logs 379 successful commands, including 237 standalone container
measurements, 84 integrated comparisons (42 exploratory and 42 matching-PIC),
twelve native checks, three decoder checks and diagnostic/build/verification
commands. All earlier failed attempts and superseded methodology results remain
in their original directories; no completed result is overwritten. Peak process
RSS is 339,184 KiB (about 331 MiB), zero swaps, with no experiment OOM or
resource-guard stops/deferrals. RAM/swap/headroom/serial policies remain as above.

Python syntax and scoped diff checks pass. Cross-check every streamed file
hash between both new matrices, the earlier golden matrix and the additional
shape diagnostic: they agree exactly. Recheck all frozen engine/include source
hashes and the baseline archive against the checkout: production files and
the archive remain unchanged. Store the new summary, all attempt receipts and
final equipment source hashes under `stl-review/audit-summary.json`,
`attempt-index.json`, `equipment-final/` and `equipment-hashes.json`.
Public site behavior, modules, recordings and fidelity profiles are unchanged;
no strict site/package build or broad native/WASM/browser suite is required or
claimed for this research-equipment/internal-documentation change.

## 2026-10-26 — Implementation authorized: gate input caches and buffer reuse

The owner's "go" authorizes the next bounded implementation step. Preserve
the public ordered pin maps, add a pin-table revision, and cache typed input
handles inside primitive gates. Refresh a cache when a pin is added/replaced;
ordinary rewiring keeps the same pin object and needs no input-handle refresh.
Retain null/wrong-width behavior and all four states. Reuse the disabled-history
pin state's existing value vector instead of destroying/recreating it.

The IO/gate object layouts change, requiring a complete native rebuild rather
than overrides linked against the old archive. Compare named-lookup and cached
gate objects compiled against the same new headers/archive and actual compiler
flags. Keep isolated Python deployment disabled and retain all pre-change
files, experiments and memory caps. Focused mutation, history, timing and exact
trace checks are required before accepting the change.

An additional compiler-provenance correction: the native build's
`CMAKE_CXX_COMPILER_LAUNCHER` points to `build-milestone/native-compiler.sh`.
It substitutes Clang for most translation units, with selected generated/test
units at `-O0` and a few GCC cases. The CMake GCC identifier alone did not
describe the frozen archive's compiler. Earlier standalone container probes
are GCC measurements; override experiments are mixed-toolchain comparisons.
The matching-PIC container controls used the same GCC compiler for control and
candidate objects, so their scoped comparisons remain reproducible, but they
are not an all-Clang production-build result. Record the launcher and actual
compiler invocation for this implementation phase. This further qualifies
the earlier compiler-level attribution; all prior data remain retained.

The first revision-only cache draft is superseded before use: retaining a
strong handle until the next evaluation could keep a replaced pin alive and
allow an obsolete weak wire sink to schedule extra work. Stop its partial
serial rebuild at 59%; preserve that attempt. Replace the revision counter
with a protected pin-table-change hook that immediately clears gate handles,
then resolve names lazily at the next evaluation. The mutation test checks
old-pin expiration before another evaluation, not only afterward. No changed
archive was published by the partial build.

That deliberate stop exposes an equipment issue: `systemd-run --wait` can
return zero after a service is externally stopped while its timed command
never completes. Correct the first receipt to `interrupted`, preserve the
wrapper's observed return code, and harden the logger to require a complete
successful `/usr/bin/time` receipt as well as wrapper success. Missing or
signal-terminated receipts are not passes. Restart the complete rebuild with
the corrected ownership behavior and final headers.

The final-layout serial `SimulatorLib` rebuild completes successfully. The
first attempt to build the newly registered standalone regression target
fails because the existing generated Makefile has not been reconfigured and
does not yet know that target. Preserve the failure, explicitly reconfigure
with Python deployment disabled, then build the regression and focused
native consumers. The new gate/cache regression passes its first execution.
The old delay-table snapshot executable is retained before relinking its
consumer, permitting a complete compiled-table comparison.

Thirteen focused native CTests pass, including all six gate scenarios,
history/drain, register, RV32I ALU, the new cache regression, and exhaustive
SRAM-cell/D-flip-flop timing checks. The full-tick decoder fixture also passes
all 1,202 timestamps, four-state/future pin-only changes and seek/rewind.

The first delay snapshot assertion fails: the saved executable dates from
October 23, while the frozen baseline archive dates from October 25. Its
17 differing lines therefore compare different pre-existing table revisions.
Retain both outputs and the diff; do not attribute them to input caching.
Compile the snapshot consumer anew against the complete frozen baseline
headers/archive. This output matches the newly rebuilt archive byte for byte
for every unit/embedded/desktop table field (SHA-256
`441a876856fff3d6e28f5db7a9f6e873079c1c6ef2f00dfff82c50bcb41aecb4`).
The canonical table source hash is also unchanged. The full snapshot compares
stored table data; SRAM/flip-flop checks separately exercise structural timing.

An initial dependency audit scans inactive objects from retired targets as
well as current objects and reports expected stale files. Restrict the audit
to the actual archive and selected consumer link commands: 172 objects depend
on the changed IO header and 109 on the gate header, with zero stale active
objects. Other executables/modules are deliberately not rebuilt or deployed.

The first control-build link fails because equipment canonicalized the
`clang++` symlink to `clang-24`, changing the link driver's mode and omitting
the C++ runtime. Preserve the partial snapshot/objects under
`implementation/control-build-failed-clang-driver/`, and retain its command
receipts. Correct the helper to preserve the compiler invocation name and
rebuild the controls with the actual native `clang++` launcher target. This
is an equipment failure, not an engine or memory failure.

The corrected control build completes. Separate complete old-layout and new
layout executables use the actual Clang invocation, `-O1 -fPIC -UNDEBUG`, and
the same execution CPU timer. Four new-layout combinations isolate named
lookup/cache access and original/reused headless buffers; a fifth frozen
pre-change executable measures the net effect including object layout.

Exercise the logger hardening with a real child service that stops its own
unit before the timed command completes: the wrapper exits zero, but the
receipt correctly says `interrupted`. Preserve that intentionally interrupted
attempt and `logger-regression.json`; it is an expected regression-test
outcome, separate from the earlier interrupted build.

All ten four-iteration calibration runs complete with identical counters,
architectural state and trace bytes across the five variants. Their signature
also matches the prior matching-PIC STL matrix and its golden traces. The
first cross-record assertion used the new `traceFiles` field name against
the older `files` field and failed on schema shape; normalize only that key
and the complete signatures match. Retain the normalized comparison receipt.
Do not infer a speedup from this single calibration: its differences are
small and mixed. Start the rotated three-repeat, 16-iteration matrix.

Source comparison against the original frozen include/src trees finds exactly
four changed production files: IOComponent, Gate.hpp/Gate.cpp and Simulator.cpp.
The rebuilt archive matches the new frozen copy. Keep that source/layout audit
with compiler, object and executable hashes. No component structure, delay
table source, public pin-map type, event priority or fidelity rule is changed.

The source review also explains the limited expected reach of primitive-gate
caching: `GateListPartBehavioral` already caches its input handles, while
`Primitive::read` still resolves names and `GateTwin::settleLeaf` builds named
`Values`/`Deciders` maps, trial output maps and per-bit scratch vectors.
Indexed primitive ports and reusable timing-model scratch storage remain a
separate candidate; preserving opaque/four-state values, dependency-based
publication times and sequential timing state requires its own equivalence
work. Do not attribute those costs to primitive gate input lookup alone.

## 2026-10-26 — Implemented caches: complete results and decision

The implementation preserves public ordered pin maps and caches typed owning
handles in the six primitive gate classes. A protected pin-change hook clears
handles immediately on input/output additions or replacements; cold evaluation
resolves them lazily. Missing/wrong-width inputs still read UNKNOWN, rewiring
uses the same pin object, and no component/pin/wire raw pointers are introduced.
Disabled-history pin changes reuse their existing one-value buffer. No retained
history or delayed event payload is changed into a view of mutable state.

The complete repeated matrix runs five variants, three repetitions each,
with and without streamed tick capture. Each run executes 117 instructions,
2,000 cycles and 14,302,358 processed events in the Visual fidelity profile
and unit timing. Streamed runs retain 66,162 timestamps. All counters,
architectural/profile fingerprints and all twelve trace-file hashes agree
across the fifteen streamed runs. Five earlier calibration traces also match
the previous golden traces. CPU seconds below measure execution only:

| Implementation | No recording, median CPU s | Streamed ticks, median CPU s |
| --- | ---: | ---: |
| Complete frozen pre-change build | 14.1463 | 16.5986 |
| New-layout named lookup/original buffer control | 14.2851 | 16.6186 |
| Gate cache only | 14.1428 | 16.7234 |
| Buffer reuse only | 14.5578 | 16.7571 |
| Gate cache and buffer reuse | 14.0536 | 16.3946 |

The combined net median reductions are 0.66% and 1.23%; execution wall medians
are 15.7179 -> 15.6143 s and 18.4496 -> 18.2168 s under the 90%-CPU cap.
Interpret cautiously: pre-change CPU ranges are 14.1116..14.2918 and
16.5377..19.5337 s; combined ranges are 13.9889..14.1820 and
16.3447..17.8210 s. The individual controls are not additive and buffer reuse
alone is slower in this matrix. Earlier allocation instrumentation establishes
its reduction in allocation churn, not a standalone CPU speedup. Retain every
result and avoid attributing the combined difference entirely to one change.

The separate 64-inverter fanout comparison uses 50,000 four-state drive rounds
and three rotated repetitions per variant/mode, eighteen runs total. Counters,
modeled times and full change digests match. Net execution wall medians are
2.3756 -> 2.2971 s without observation (3.3% lower) and
2.3382 -> 2.3087 s with a digest (1.3% lower). The compatible named-lookup
control has medians 2.4194 and 2.2506 s: even this gate-heavy case does not
establish a universal cache gain. Keep the run ranges and whole-process CPU
measurements in `fanout/results.json`; do not present them as full-core CPU
measurements or gameplay acceptance.

Measured native object sizes are IOComponent 432 -> 432 bytes, NOTGate
472 -> 496 bytes and a binary gate 472 -> 512 bytes. The cache therefore costs
24/40 bytes per gate, with no separate cache allocation. This bounded memory
tradeoff and simpler repeated input access are retained alongside buffer reuse;
the measurements support only a modest native improvement. Next work should
focus on primitive/timing-model execution data and wire payload allocations,
with exact timing/trace equivalence, rather than extrapolating this step to
the owner's throughput target.

## 2026-10-26 — Implementation verification, resources and remaining scope

The implementation phase retains 95 command receipts: 91 pass, two fail
(the unconfigured target and wrong compiler-driver link), and two are
intentionally interrupted (superseded build and logger stop regression).
Assertion-level comparison failures/corrections are documented above.
All 58 guest/calibration/fanout runs complete and match their semantic controls.
Thirteen focused native CTests and the complete tick decoder pass; the cache,
SRAM-cell and D-flip-flop checks exercise every timing profile. Complete stored
table snapshots match, but full gate-delay table regeneration/certification is
not claimed. There is no structural wiring or delay change requiring new data.

Maximum measured process RSS is 474,200 KiB (about 463 MiB), zero swaps.
No experiment OOM, guard stop or resource deferral occurs. Build/test/measure
jobs remain serial, capped at 768 MiB with zero service swap and the stated
host-headroom guard. The final rebuild takes 14m46s and never deploys Python.
All earlier completed recordings, artifacts and unrelated dirty changes remain.

Artifacts are under `build-milestone/engine-performance-20261026/implementation/`:
`results.json`, `calibration/`, `fanout/`, `gate-sizes.json`, `delay-check.json`,
source/dependency audits, `audit-summary.json`, `attempt-index.json`, per-phase
ledgers and every unique attempt directory. Freeze final equipment and hashes
under `equipment-final/` and `equipment-hashes.json`. Key results are copied
into this record so they do not depend on the ignored build artifacts.

Python syntax and scoped diff checks pass. This is isolated native validation;
no broad native suite, WASM rebuild/parity, browser suite, strict site build,
new live module or full-game recapture is performed. The public timing/fidelity
contracts and site behavior are unchanged. Broader platform/workload validation,
indexed timing-model data, bounded adaptive trace/queue policies and the
10-million-event target remain open. No gameplay campaign is restarted.

## 2026-10-26 — Remaining optimization priorities after the cache step

Reinspect current source and the completed controls in response to the owner's
request for more opportunities. This is source/record review only: no new
engine modification, build or benchmark is run. The measured small gate-cache
gain does not exhaust the remaining execution costs.

1. **Typed wire comparison and propagation.** `WireUpdateEvent::process` still
   constructs a vector, and `processWireUpdateEvent` obtains another vector
   merely to compare the current value. The retained direct-array prototype
   removes 32.2% of C++ allocation requests without observation and 29.4% with
   streaming on the short guest. This establishes allocation reduction, not
   a broad speedup. Port it to a complete current build first; then consider
   direct typed assignment and propagation without temporary value vectors.
   Delayed events/history must retain owned snapshots.
2. **Indexed timing-model ports and reusable scratch.** `GateTwin::settleLeaf`
   builds input/output maps, dependency sets, copied trial maps and scratch
   vectors during refinement. Use precompiled port indexes, bounded dependency
   masks and reusable per-evaluation storage, retaining the named interface
   at its boundary. This addresses a sampled hot path beyond primitive gates.
   Preserve opaque/unknown states, per-bit arrival times and sequential state.
   Incremental recomputation is a later extension: equal values alone do not
   imply equal arrival/setup timing.
3. **Controlled actual-Clang optimization levels.** Relevant native sources
   still compile at `-O1`. Compare `-O1`/`-O2` with the same actual compiler,
   PIC/assertion flags, source, layout and archive controls. Earlier selected
   compiler results have documented PIC/toolchain confounds and cannot supply
   an expected gain for this comparison. Keep generated tables outside the
   hot-file experiment and retain the compile memory cap.
4. **Duplicate pending evaluations.** Wire propagation schedules an evaluation
   for each changed input sink, and `evaluateAt` has no generic pending-event
   deduplication. Measure repeated pending `(component,time)` work before
   changing it. Restrict any initial prototype to explicitly pure parts and
   allow another evaluation after a zero-delay wire update. Never apply a
   blanket once-per-timestamp rule to edge-sensitive/self-timed parts. Event
   counts may intentionally fall, so acceptance must compare complete output
   timing/traces and architectural behavior rather than identical event counts.
5. **Bounded event allocation reuse.** Events are shared allocations with weak
   cancellation handles. A capped pool or compact owned event representation
   could reduce allocator/refcount/cache costs. It must preserve weak-handle
   generations and outliving component/event lifetimes, and release unused
   memory. No whole-campaign monotonic allocator or raw component/pin/wire
   handles are proposed.
6. **Hybrid current-time scheduling.** Keep the vector-backed heap for general
   future timestamps; investigate bounded wire/evaluation ready queues for
   dense work at the current time. The old bucket prototype is explicitly
   unsuitable as a global replacement because sparse queues regress 2.2–3.4x.
   Preserve wire preemption, insertion order, cancellation and event-quanta
   boundaries before measuring an owned hybrid.

Later candidates remain packed four-state signal/event payloads, cached fanout
routes and adaptive recorder rows. Recorder-only changes benefit capture;
plain sorted/pooled row replacements already failed to establish an integrated
gain. Keep the public ordered pin maps, dense memory vectors and general heap
fallback unless a workload-specific controlled comparison supports a change.
Recommend the wire allocation path first and indexed/reusable timing data next.
All future experiments retain serial execution, 768 MiB/no-swap service limits,
host-headroom checks, exact behavioral validation and complete attempt logs.

## 2026-10-26 — Continued experiments: wire payloads and timing scratch

The owner authorizes continued experiments. Freeze the current cache-enabled
engine as the new baseline, verify its archive/source hashes, and retain all
earlier results. This phase uses private native overrides rather than changing
the production engine or deploying modules. Recompile unchanged controls and
candidates with actual Clang, matching PIC/assertion flags and object order;
separate an explicit selected-file `-O2` factor from algorithm changes.

First test a direct typed wire comparison that avoids per-bit virtual calls
and constructs value vectors only after a real change. Then test per-settle
scratch-buffer reuse, removal of dead timing-input snapshots and bounded trial
output-map reuse for the two inspected concrete memory primitives that always
overwrite their output. Other primitive types retain fresh trial containers.
Keep generic primitive behavior, sequential timing state and fresh dependency
sets unchanged. This is an incremental timing-buffer experiment, not the full
indexed-port redesign. Validate independent timing-twin output/time signatures,
history/quantum boundaries and actual recorded ticks before performance claims.

Use serial 768 MiB/no-swap services, host-headroom guards and complete attempt
receipts. Distinguish allocation diagnostics from throughput measurements and
preserve failed, rejected and superseded attempts. No full-game run is started.

The first relocated timing-prototype compilation fails because its quoted
`TimingGraph.hpp` include depended on the original source directory. Preserve
that source/generator and receipt under `wire-timing/failed-relocated-include/`;
qualify the private header as `analysis/TimingGraph.hpp` through the existing
frozen source include path, and resume without rebuilding completed objects.
The baseline and wire-only control links had already passed. No engine or
resource failure is inferred from this equipment include-path error.

Before measurement, source review supersedes the unchecked typed-wire fast
path: a subclass can override the virtual value API, so qualified base-bit
access is valid only for exact `Wire<W>` instances. Preserve the first compiled
wire objects/binaries, source generator and build manifest under
`wire-timing/superseded-unchecked-wire/`. Add an exact-type guard with the
original generic fallback, then rebuild the affected private controls. No
performance result is attributed to the unchecked draft. Add a derived-wire
regression as well as ordinary four-state/width comparisons.

All six final controls build successfully. Thirty focused verification runs
pass: every variant agrees on exact timing/value signatures for 61,536 direct
leaf cases across unit/embedded/desktop profiles, and on all supported wire
widths with four states, short/long payloads, duplicate/no-op changes, null and
derived wires, history modes and uint64 times. The wire-only, timing-only and
combined `-O2` variants also pass gate-cache/mutation checks, scheduler quantum
and priority contracts, full-tick decoder/seek/rewind, and three selected
history/drain/ALU scenario checks. These are private override checks, not a
complete current-library rebuild, whole-suite or WASM claim.

Start a four-iteration allocation calibration for all six variants with and
without streaming. Its interposed atomic allocation counters are diagnostic;
do not use its CPU/wall times as throughput results. Compare its traces to the
earlier golden files before running the uninstrumented repeated matrix.

The twelve allocation-calibration runs match counters/state/trace bytes and
the earlier four-iteration golden signatures. Current baseline C++ request
counts are 13,282,636 without capture and 14,522,472 with streaming. The guarded
wire prototype removes exactly 4,283,443 requests (32.25% / 29.49%). Timing
scratch/trial reuse removes only 2,700 requests (about 0.02%); the combined
reductions are 4,286,143 requests. Selected `-O2` changes do not alter those
request counts. Cumulative requested bytes are not peak live memory, and these
instrumented durations are not throughput measurements. Start the separate
three-repeat, six-variant uninstrumented matrix at sixteen loop iterations.

This small timing-allocation reduction motivates a separate next experiment:
the current BasicComponent calls its delay twin even when no input changed.
Test exact-argument memoization only for stateless GateTwin models, caching
the input values, their arrival timestamps, evaluation time and resulting
output values/times per owner. Stateful twins must delegate unchanged: unknown
clock handling can intentionally return a different answered/fallback state on
a repeated call, so blanket memoization would be incorrect. Do not cache full
gate-node arrays or reuse one component owner's answer for another owner.

The uninstrumented matrix develops substantial baseline drift: sixteen-loop
no-capture baseline CPU changes from 13.8877 to 20.6606 s, and streamed cases
rise above 24 s. Live process/cgroup inspection finds unrelated CPU-intensive
workloads occupying this four-logical-CPU host. Leave those jobs untouched.
Stop the current experiment's service, retain completed rows, partial JSON,
all traces and `throughput-interruption.json`, and mark the matrix exploratory
rather than a causal speed estimate. This is an intentional measurement stop,
not an OOM, guard failure or semantic mismatch. Continue the stateless-memo
build/correctness/allocation diagnostics, then use fresh paired controls and
explicit host-load context for subsequent throughput work.

The per-owner memo prototype keeps only input values/times and output answers,
not full gate-node arrays. Stateful models delegate directly to the original
implementation. Twenty-five additional focused checks pass, including exact
70,752-case signatures for repeated calls, interleaved owners, mutated returned
buffers, unknown clocks and resets in all three timing profiles, plus register,
gate, ordering, decoder and selected scenario checks. Baseline, `-O1`/`-O2`
memo and diagnostic memo signatures agree exactly. The shared primitive probe
gets an explicit successful return so it can be reused as a helper; initial
verification sources remain frozen under `verification-sources-initial/`.

Run allocation and whole-process hit-count diagnostics for the memo prototype
before timing it. These counters are excluded from throughput builds. For the
next timing stage use baseline/candidate/candidate/baseline blocks, one fixed
allowed logical CPU, and start/end CPU-load/frequency/thermal snapshots. Define
endpoint baseline drift over 5% as unstable in advance and retain every block,
including unstable ones; do not claim an isolated speedup from unstable data.

Stateless memo allocation/hit diagnostics complete in eight golden-matching
runs: 135,331 whole-process twin calls, zero eligible hits, and 135,331 stateful
bypasses for both observation modes. Allocation counts are unchanged. Reject
this prototype as an optimization for the measured guest; do not spend a
throughput matrix on its bypass overhead. The actual active twins have state.

This replaces the blanket stateful exclusion with a narrower guarded
experiment, retaining the first prototype unchanged. Cache a successful full
GateTwin answer only when its clock is known and binary; never cache reset/
unknown-clock fallback or the special first falling-only early return. Compare
the full input values, their change times and evaluation time, and retain the
original inner state per owner. All other calls delegate and update that state.
An unchanged successful binary-clock computation has no further capture edge;
the focused probe must verify that assessment against the original algorithm.
Extend the probe with direct high-to-low transitions and a clock-pass output,
alongside unknown/reset cases, so an early-return/caller-buffer difference
cannot be hidden by testing only Q/Q_BAR. Preserve the initial probe source.

The guarded stateful memo passes 25 further focused checks, including identical
73,056-case baseline/candidate timing-value signatures in all three profiles,
direct high-to-low clock transitions, clock pass-through, unknown clocks/reset,
owner isolation, register scenarios and full-tick playback. Its eight golden-
matching diagnostics show only 409 hits in 135,331 calls (0.302%) and 58,772
noncacheable calls. It adds 1,023 execution-window allocation requests instead
of reducing them. Reject it for this guest and retain all code/results; broad
stateful equivalence is not claimed from these focused tests. Clock changes,
not redundant identical calls, account for most of the measured twin traffic.

Avoid spending another timing matrix on the two low-hit memo prototypes or
the negligible timing-copy reduction. Run 32 fixed-affinity ABBA measurements
for the remaining two factors, guarded wire updates and matched actual-Clang
`-O2`, with four-loop guests, capture/no capture and two blocks per factor/mode.
Every candidate is bracketed by a fresh baseline. Record all baseline drift,
candidate spread, host context, trace hashes and predeclared stability flags.

## 2026-10-26 — Paired results, rejected ideas and next evidence

All 32 fixed-CPU ABBA runs complete with matching counters, architectural state
and four-iteration golden traces. A block ratio is the geometric mean of its
two candidate CPU times divided by the geometric mean of its two fresh control
CPU times. Median ratios use only the predeclared endpoint-stable blocks below;
all unstable blocks, raw times and candidate spreads remain in `paired.json`.

| Factor | No recording | Streamed ticks |
| --- | --- | --- |
| Guarded typed wire comparison | 3.38% lower CPU, 1/2 stable blocks | 2.65% lower CPU, 2/2 stable blocks |
| Nine selected files at actual Clang `-O2` | 3.00% lower CPU, 2/2 stable blocks | 2.72% lower CPU, 2/2 stable blocks |

The two stable streamed wire blocks individually show 1.53% and 3.77% lower
CPU time. Stable compiler blocks span 2.24..3.75% without recording and
2.61..2.83% with it. One initial wire/no-recording block has 27.8% control drift
and is excluded from the estimate, not deleted. The retained stable wire
no-recording block has 4.32% candidate spread; remaining candidate spreads are
about 1.3..3.6%. These are small shared-host observations with one or two
bracketed blocks, not statistical confidence intervals, additive gains, a
whole-library compiler result or a full-game speed projection. The controlled
compiler result does not reproduce the earlier confounded 11..18% attribution.

The typed wire mechanism is independently established by eliminating exactly
4,283,443 C++ allocation requests on the short guest. That is about 32.25% of
requests but only about 12.6% of cumulative requested bytes without observation;
allocation counts alone cannot predict proportional CPU savings. A full current
build, broader timing/history/subclass validation and WASM parity are required
before production adoption. No production algorithm or compiler flags are
changed in this experimental phase.

Reject stateless and guarded stateful exact-repeat memoization for this guest:
all active twins have state, clock-driven calls rarely repeat the full key,
and the guarded cache hits just 409/135,331 calls while adding 1,023 allocation
requests. Do not promote leaf scratch reuse on the strength of the old profile:
the actual 13.02% gprof entry names the whole `GateTwin::settle` method, not
`settleLeaf` alone. Removing leaf/trial copies saves only 2,700 requests here.
All three prototypes remain useful negative evidence and retained equipment.

A more relevant next timing experiment is incremental recomputation of affected
nodes inside clocked twins, preserving the existing previous-node state and
arrival/setup timestamps. A clock event need not recompute unrelated logic,
but unknown/reset handling, capture before mutation, dirty-node memory bounds
and exact output publication must remain equivalent. Other remaining targets
are typed propagation, bounded event allocation and precompiled execution
routes for behavioral primitives. Broad container replacement, blanket
once-per-timestamp evaluation and the sparse-regressing bucket queue remain
unsupported.

## 2026-10-26 — Continued phase verification and resource closure

This phase retains 292 command receipts: 290 pass, one fails on the relocated
private include, and one is deliberately interrupted when the host becomes
unsuitable for the original timing matrix. The unchecked wire draft is
superseded by source review before measurement and retained in its own folder.
Eighty focused verification commands pass across the three prototype stages,
including exact three-profile timing signatures, width/four-state/derived-wire
cases, stateful owner/caller-buffer cases, register/scenario/order checks and
nine full-tick decoder checks. This is additional focused evidence, not a full
native or WASM/browser suite.

Twenty-eight diagnostic guests, 24 completed exploratory timing guests and 32
paired guests total 84 completed guest runs. Cross-audit every result against
the appropriate old four/sixteen-loop golden signature: all state/counters agree
and all 42 new streamed trace packages match byte for byte within workload.
The 25th exploratory job is interrupted; its incomplete output is not a result.

Peak process RSS is 268,428 KiB (about 262 MiB), zero experiment swaps and no
OOM, resource-headroom stops or deferrals. Serial 768 MiB/no-swap services and
host-memory guards remain active. Fixed-affinity paired runs additionally
retain load, CPU ticks/frequency and thermal context. Do not alter unrelated
host workloads to obtain a favorable benchmark.

Recheck every frozen include/src file and the native archive against the current
checkout: production is unchanged. Store `golden-audit.json`, `production-audit.json`,
`audit-summary.json`, `attempt-index.json`, source/build manifests, final
hashed equipment, initial probe sources, superseded prototypes, partial data,
per-command receipts and all traces under
`build-milestone/engine-performance-20261026/wire-timing/`. These tracked results
preserve the key facts independently of ignored artifacts. Live modules,
finished captures and the stopped gameplay queue are untouched. Python syntax
and scoped diff checks pass; no public site/package build or deployment is
performed or claimed. The larger engine optimization effort remains open.

## 2026-10-26 — Incremental clocked-twin experiment begins

The owner requests continuation. Keep the current engine/archive and earlier
controls frozen. Separate two private changes: reuse the clocked twin's already
owned node-value/time arrays in place, and recompute only affected gate nodes.
The first incremental prototype handles gate-only clocked twins; primitives/
leaf-containing models retain the full traversal. Track changes to both value
and arrival time, propagate through a shared compact dependency index, and
retain the original capture-before-update, unknown-clock, reset and falling-
only early-return behavior. No repeated-call memoization is introduced.

Use one dirty byte per existing state node and a shared CSR fanout index with
at most two dependencies per gate. Scan the existing topological order rather
than allocate an unbounded pending-node queue. Keep graph initialization/full
fallback for first calls, stateless twins and unsupported leaf configurations.
This limits extra memory and preserves generic primitive behavior. Measure
node visits, allocation requests, RSS, exact state/output times and actual trace
bytes before throughput. Retain the same serial 768 MiB/no-swap limits and
baseline-bracketed/fixed-affinity protocol. Available disk is about 3.3 GiB;
reuse the frozen archive instead of copying another build tree.

The four private owned-buffer/incremental builds pass. The first new probe
compile fails because nested source inclusion renames `main` twice. Preserve
the failing sources and receipt under `incremental/failed-probe-main/`, replace
the reused memo probe entry with an explicit guarded helper, and rerun reference
verification before candidates. This is an equipment composition error; the
engine algorithms have not yet been exercised or accepted.

Baseline-first verification now passes 33 focused checks for the in-place,
incremental `-O1`/`-O2` and node-counter variants. All agree on a 104,664-case
value/arrival-time signature across the three timing profiles. The new fixture
adds eight clocked feedback bits, reconvergent enable logic, arrival-only
changes, repeated calls, interleaved owners, cold-state replacement and a
leaf-containing fallback, with independent combinational truth checks. Existing
gate/mutation, ordering, register/history/drain/ALU and full-tick decoder checks
also pass. Start allocation and node-work diagnostics before timing; their
counter overhead is not a throughput measurement.

All ten four-loop allocation/node diagnostics match the old golden signatures
and trace bytes. The diagnostic model sees 135,331 calls: 57,973 original
falling-only early returns, 74,549 incremental calls and 2,809 full/cold calls.
Of 33,095,028 ordered node visits, it evaluates 11,230,576 and skips 21,864,452
(66.1%). The process constructs 76,012 bytes of CSR capacity and 41,193 dirty
bytes cumulatively; those counters are not a peak-live-memory measurement.
The in-place control removes six execution-window allocation requests; dirty
tracking adds 103 net requests (13,282,739 versus 13,282,636 without capture),
while cumulative requested bytes fall slightly. No large allocator saving is
claimed from this algorithm; its target is node work and copying.

Begin 32 fixed-affinity ABBA runs comparing in-place and incremental `-O1`
against the same unchanged `-O1` baseline. Extend the existing paired equipment
with an explicit control target so a later incremental `-O2` comparison can
use unchanged selected-file `-O2` controls, separating the compiler factor.
Preserve the existing 5% endpoint-drift rule, raw candidate spread, host context
and exact golden checks. Diagnostic counters are absent from timing builds.

All 32 `-O1` guest measurements preserve semantics, but none of the four
incremental blocks meets the 5% endpoint-stability criterion. Their ratios span
0.786..1.014 and cannot establish throughput. In-place alone has only one
stable block per mode (ratios 1.023 / 1.061); its streamed candidate spread is
14.2%, so that too is weak performance evidence. Retain the complete matrix;
do not substitute these numbers for an isolated algorithm gain.

Refine dirty traversal with ordered 64-bit masks instead of scanning one flag
per gate. CSR consumers become topological positions; repeatedly extract the
next set bit so newly dirtied consumers in the same word are processed too.
This preserves topological order while reducing metadata and clean-node branch
work. Keep the byte-flag prototype and its measurements intact.

Also prepare a within-process timing-model comparison: compile the unchanged
reference into a distinct namespace, and alternate control/candidate chunks
on separate owner states with identical input timelines. Validate every output
value/time and answer before timing, retain dense and sparse clocked-register
cases in all timing profiles, and compare state again at chunk boundaries.
This measures the timing kernel, not complete guest or gameplay throughput;
it cannot turn an unstable guest result into a demonstrated whole-engine gain.

The ordered-mask variants pass 25 further focused checks with the same
104,664-case signature and actual tick playback. Eight allocation/node
diagnostics retain all golden bytes and the same 66.1% skipped evaluations.
Mask storage is only 4,616 cumulative bytes versus 41,193 dirty bytes before;
CSR capacity remains 76,012 bytes. Both versions add the same 103 allocation
requests, so this refinement targets traversal and metadata, not allocator
request count.

The kernel protocol alternates ABBA and BAAB chunks to balance first/cold and
middle/warm input positions. Include an unchanged-reference-vs-reference
control, in-place, byte flags and ordered masks at matching `-O1`/PIC settings.
Validate every case before timing and each chunk endpoint during timing on
8/32-bit structural registers with sparse/dense input activity and all timing
profiles. Disk headroom is now about 1.7 GiB; reuse existing artifacts and keep
the remaining build/measurement footprint bounded, without removing evidence.

All four within-process kernel comparisons pass every value/time/answer check.
The unchanged-vs-unchanged control itself has CPU ratios 0.908..0.981, showing
residual code/data-layout or measurement bias. Against the separately compiled
reference, byte-flag ratios span 0.316..0.552 and ordered-mask ratios
0.139..0.355 over the twelve register width/activity/profile cases. In-place
alone is mixed (0.861..1.063). These large incremental kernel differences are
promising despite the control bias, but are not whole-engine speedups and
must not be projected onto game capture. Full guest timing remains necessary.

Archive 92 completed private verification ELFs losslessly, freeing about
2.388 GiB. Verify every gzip round trip by SHA-256 before removing its plain
copy, commit a per-file restoration index first, and preserve the original
mode/timestamps/hash plus compressed hash. All source, objects, receipts,
results and candidate/live executables remain available; no live module or
recording is archived. `verification-archives.json` under `wire-timing/` records
the locations. `archive_verification.py --restore RELATIVE_PATH` restores an
individual executable without overwriting a differing file. Verify restoration
by rerunning the reference timing probe and checking its recorded output hash.

The restored reference executable passes with its exact recorded output hash.
All sixteen ordered-mask guest measurements match the golden records. Stable
blocks show 4.91% lower CPU without recording and 4.20% with streamed ticks;
only one of two blocks in each mode meets the 5% baseline-drift rule. The
other retained ratios are 0.957 and 0.970. Even the stable streamed block has
7.53% candidate spread, so this remains limited shared-host evidence, not a
precise or universal speed estimate. Run a final sixteen measurements against
unchanged selected-file `-O2` controls to isolate the same algorithm at that
optimization level, without adding compiler and algorithm percentages.

## 2026-10-26 — Incremental results and decisions

The ordered-mask candidate is the strongest new timing-model candidate. It
evaluates 66.1% fewer nodes in the short Visual RV32IM loop, and uses
4,616 dirty-mask bytes plus 76,012 shared CSR bytes cumulatively (about 79 KiB).
Node metadata totals are not peak RSS. Request count rises by 103 during the
execution window; this is computational pruning rather than an allocator win.

| Guest comparison, same flags on both sides | Stable no-recording CPU ratio | Stable streamed CPU ratio |
| --- | --- | --- |
| Owned buffers only, `-O1` | 1.023, one of two blocks | 1.061, one of two blocks |
| Byte dirty flags, `-O1` | No stable blocks | No stable blocks |
| Ordered dirty masks, `-O1` | 0.951, one of two blocks | 0.958, one of two blocks |
| Ordered dirty masks, `-O2` | No stable blocks | No stable blocks |

Every raw ratio and predeclared 5% baseline-drift decision is preserved. The
retained ordered-mask `-O1` blocks mean 4.91% / 4.20% less execution CPU in these
bounded observations. The streamed block still has 7.53% candidate spread;
other blocks drift 5.3..11.6% at `-O2`. Do not relax the threshold or infer a
matched compiler-factor gain from those unstable blocks. Do not promote buffer
ownership reuse by itself: its limited stable measurements regress and other
measurements remain noisy. The private incremental variants include that reuse
as part of their algorithm; leaf-heavy workloads need separate assessment of
that fallback path before adoption.

Register-kernel ratios against a separately namespaced unchanged implementation
are 0.316..0.552 for byte flags and 0.139..0.355 for ordered masks. These are
8/32-bit sparse/dense register kernels across unit/embedded/desktop timing,
with all outputs, times and answered/fallback states checked. The unchanged
control's own 0.908..0.981 ratios expose residual layout/measurement bias, so
the exact factors are approximate. The large kernel improvement does not
contradict the smaller guest gain: much guest work is outside this refinement.
No full-game, structural-only-core or other-fidelity speedup is inferred.

Retain the ordered-mask implementation for wider validation and eventual
integration assessment. Stateful capture still precedes input/state mutation;
unknown/reset and falling-only behavior match the reference. Changed arrival
times propagate even when the value stays equal. Gate-only clocked twins get
incremental traversal; other models retain full traversal. No simulated ticks,
engine events, trace rows or component fidelity are removed or changed.

## 2026-10-26 — Incremental phase verification and resources

This phase retains 207 command receipts: 206 pass and one probe-composition
compile fails, is recorded and is corrected. Fifty-eight focused verification
runs pass, plus a restored reference-probe rerun and four kernel-comparison
processes (twelve profile/width/activity cases each). The direct timing suite
has 104,664 cases per implementation. Every within-process validation case and
chunk endpoint agrees with the unchanged reference.

Eighteen diagnostic guests and 64 uninstrumented paired guests total 82
completed guest runs. Cross-audit all results and all 41 streamed packages
against the old four-iteration golden signature: every counter, architectural
fingerprint and file byte agrees. The unchanged old archive and every frozen
include/src file still match the current checkout; no production source,
compiler flag, module, timing-table data, fidelity rule or recording changes.

Peak process RSS is 268,336 KiB (about 262 MiB), zero experiment swaps, with no
OOM, resource-guard stops or deferrals. Builds/tests/measurements stay serial
under 768 MiB services and host-memory guards. Lossless verification archiving
frees about 2.388 GiB; original binaries remain reconstructible with verified
hashes, modes and timestamps. Source/object/result/receipt/trace evidence stays
in place; primary probes and live modules are untouched. A real restoration
and reference execution passes its original output SHA-256.

Keeping that one restored reference in plain form leaves about 2.36 GiB net
disk savings; the archiving operation itself released 2.388 GiB before the
round-trip test. Its compressed copy and restoration metadata are retained.

Artifacts are under `wire-timing/incremental/`: source/build manifests, both
verification summaries, allocation/node diagnostics, kernel rows, all three
paired matrices and host snapshots, the archive restoration check, final
hashed equipment, `audit-summary.json` and every attempt receipt. The archive
index is at `wire-timing/verification-archives.json`. Key results and failures
are copied into this tracked record. Python syntax and scoped diff checks pass.
No full native suite, full-library candidate build, regenerated gate-delay
certification, WASM/browser suite, strict site build, deployment or game capture
is performed or claimed. The overall investigation remains open.

## 2026-10-27 — Review and optimization plan

The owner requests a thorough review of the records and current source, and a
plan. This review only reads records and source: no build, benchmark, test,
service or engine change is run. Code state: `alpha` at `5549ee0` with the
uncommitted working tree described below. Figures come from earlier sections
of this and the linked records. Estimates and hypotheses are labelled; none
is a measurement.

### Where the time goes

| Workload | Processed events | CPU seconds | CPU per event |
| --- | ---: | ---: | ---: |
| 16-loop Visual guest, no recording (cache + reuse median) | 14,302,358 | 14.05 | 0.98 µs |
| Same guest, streamed ticks | 14,302,358 | 16.39 | 1.15 µs |
| Snake full-tick capture (user CPU) | 4,796,953,473 | 5,655.66 | 1.18 µs |

The owner's 10 million events/second is 0.10 µs/event, a 10–12× gap. On
this guest, streamed recording adds about 17% CPU.

Derived per-event facts:

- The 4-loop guest makes 13,282,636 C++ allocation requests for 3,853,569
  events: 3.45 per processed event.
- The typed wire prototype removes 4,283,443 requests (1.11 per event) for
  about 3% less CPU. That is roughly 25–30 ns per removed request, including
  its vector work. This rests on one or two stable blocks, so it is rough.
  Extrapolating, removing every allocation would save on the order of 10%,
  not 10× (estimate).
- Most of the ~1 µs is therefore elsewhere. Hypothesis, unverified because
  hardware counters are blocked: per-event work is dominated by cache misses.
  The object graph is far larger than the host's 3 MiB L3 (Core i5-7200U, two
  cores with hyperthreads). The trace indexes 109,674 pins and 57,925 wires,
  and a binary gate object is 512 bytes. Each event touches its own
  allocation, the wire, every sink pin and its control block, the owner
  component and its timing data. A few DRAM misses per event would account
  for ~1 µs.
- The gprof profile cannot test this. It omits libc/libstdc++ (malloc, free,
  `__dynamic_cast`); about 30% of CPU was unattributed. The 2026-10-13 gdb
  sample attributed about 16% to allocation and 11% to `__dynamic_cast` at
  gate level.
- Snake's event mix differs from the guest's: 16,959 events/cycle versus 7,151
  (16-loop) and 7,527 (4-loop). No-op wire updates are 52.9% in Snake and
  56.3% in the 4-loop guest (1 − 1,195,599 / 2,739,521).

### Record findings

1. **Measurement.**
   - The headline incremental result has one stable ABBA block per mode. Its
     streamed candidate spread (7.53%) exceeds the effect (4.20%), and every
     `-O2` block is unstable. The typed-wire result has 1/2 and 2/2 stable blocks.
   - Pinning to one logical CPU still leaves contention from its hyperthread
     sibling. Unrelated workloads were found running during one matrix. The
     90% CPU quota throttles wall time, and laptop turbo and thermal limits
     vary.
   - The guests run 33 or 117 instructions, include large startup rows, and
     have a different event mix from Snake. Small differences in this
     record therefore stay indicative.
2. **The 2026-10-13 record is stale.**
   - It still says nothing is implemented and lists three pending decisions.
   - It estimated 1.5–2× from pin handles. Primitive-gate input caches measured
     0.7–1.2%, but they cover only the inputs of six gate classes: outputs,
     splitters, joiners, Rewire and compact parts still resolve names.
   - Its phase 1 (partly) and phase 7 (compiler) were explored here.
   - Close it as superseded by this record, carrying its decisions into the
     list below.
3. **"Production" means the source tree.**
   - The gate caches are uncommitted, and the live Python/WASM modules were not
     rebuilt.
   - The touched files mix three topics, which should be committed separately:
     - gate caches and headless-buffer reuse: `Gate.hpp/.cpp`,
       `IOComponent::pinsChanged`, the reuse lines in
       `Simulator::recordPinChange` and `GateInputCacheTest`;
     - trace-observer hooks in `Simulator.cpp`, from the full-tick record;
     - constant words in `StructuralLogic.hpp`, from the 2026-10-24 record.
4. **Event counts are published and exact.** Recorded-game evidence lists
   them (for example `bringup.evidence.json`: processed 15,336,236), and so
   does the Snake record. Any change that drops events requires owner approval
   and re-baselined evidence. This answers the factual part of the 2026-10-13
   decision 2.

### Opportunities in current source not yet in this record

| Item | Evidence | Expected effect | Contracts at risk |
| --- | --- | --- | --- |
| A. Splitter, joiner and Rewire name work | `BitSplitter<W>::evaluate` builds W `"OUT_i"` strings, does W map lookups and dynamic casts, and schedules W events when any bit changes (`BitAdapter.tpp:20-28`). `BitJoiner` also builds W strings and a vector (`:41-58`). `Rewire::evaluate` builds a `map<string,vector>` per call and looks up each mapped bit by name (`Rewire.cpp:109-140`). | Unmeasured. The 2026-10-13 gate-level sample put splitter+joiner at 19% inclusive. | None for cached handles. Suppressing unchanged bits changes event counts (item E of the plan). |
| B. `getenv` on the publish path | `publishTime` calls `getenv("CIRCUITSIM_DELAY_TRACE")` for every changed output that the gates do not time, or that no arc explains (`BasicComponent.cpp:307,311`). | Small; per call cost is an environment scan. | None if cached once like `transportDelayForced` (`:336`). No test sets it mid-process. |
| C. Temporary vectors in `evaluateAt` | Three literal `vector<LogicValue>{…}` per timed evaluation (`:184,204,228`), plus one copy per input (`:196`) and per published output (`:294`). | Small to moderate on timed parts. | None. |
| D. Output path still named | `_updateOutputWire` does a map lookup, a Pin cast, a weak lock and a Wire cast per output (`:349-351`, `IOComponent.hpp:84`). `publishTime` hashes the name (`:289`). The inertial path compares pin-name strings (`:354`) and allocates for one bit (`:370`). | Moderate on gates. | Pin replacement must invalidate handles, as the input cache does. |
| E. Four-state values use 4 bytes per bit | `enum class LogicValue` has the default `int` underlying type (`LogicValue.hpp:4`), so a 32-bit pin or wire value is 128 bytes. No `sizeof`/reinterpret dependency was found. | Less memory traffic; size unmeasured. | Layout change for bindings/WASM, not semantics. |
| F. One-word packed values | Every supported width is ≤32, so a two-bit-per-bit value fits one `uint64_t`. This is the same 0/1/X/Z encoding as the trace writer's `pack()` (`TickTraceWriter.hpp:139-142`). | Removes value vectors from events, compare, propagation and observer. | Delayed events and histories must still own snapshots; one word is an owned snapshot. |
| G. Heap compares chase pointers | The comparator dereferences both events and calls virtual `getPriority` (`Event.hpp:31-40`). `scheduleEvent` calls it twice more (`Simulator.cpp:127-131`). | gprof attributes 15.2% to heap adjust/insert. Inline keys (time, priority, order) keep the exact order and the general heap, so no sparse regression. | None if key fields are identical. |
| H. Fanout casts and allocations | Per change: `remove_if` over sinks, value-vector copy, per sink a weak lock, an owner lock, up to two dynamic casts and a `make_shared` evaluation event (`Wire.cpp:292-328`). `ComponentEvalEvent::process` casts again (`Event.cpp:135`). | Moderate. | Typed sink routes, rebuilt on topology change, and a typed `shared_ptr<BasicComponent>` in the event avoid raw pointers. |
| I. Hierarchy forwarding | A boundary pin queues another wire event at the same time (`Wire.cpp:322-326`). | Share of Snake's 3.36 billion wire updates unmeasured; count first. | Merging or immediate forwarding changes event counts and same-time order. |
| J. Recorder in the simulation thread | Chunks are gzipped synchronously at level 1 (`TickTraceWriter.hpp:173`). Register callbacks rebuild ancestor ID strings (`:107`). | Part of the ~17% recording cost. | The process has no threads today (no `std::thread` in src/include). With glibc 2.41, libstdc++ may then skip atomic refcounts; a writer thread could make every `shared_ptr` copy atomic. Measure before moving compression off-thread (hypothesis). |

### Gap estimate (unverified)

- **Constant-factor work at today's event semantics (phases 1–4 below).**
  Each measured piece is 1–5%, and the plausible unmeasured ones are likely
  similar. Combined, plausibly 1.3–2× (estimate), not 10×.
- **Fewer events.** Suppressing no-op publications and hierarchy forwarding
  raises guest cycles per second but may lower events per second. The target
  metric therefore needs an owner definition.
- **Reaching 10M/s.** If the cache-miss hypothesis holds, reaching 10M/s needs
  a compact indexed execution form: arrays of packed values, CSR fanout and
  plain events. The component and view hierarchy would stay for builders,
  histories and the visualizer.

### Plan

Run everything serially. Each service gets 768 MiB, no swap, the host-headroom
guards and a full attempt ledger. Run focused checks between steps; run one
full native suite and WASM parity per phase, not per change. Event counts must
stay identical in phases 1–4.

| Phase | Goal and scope | Validation | Stop / decide |
| --- | --- | --- | --- |
| 0. Evidence (no production change) | (1) Counter-only private build: no-op wire updates by driver type, hierarchy-forwarding events, duplicate (component, time) evaluations, same-time occupancy and allocations per site, on the 4-loop guest and a Snake prefix. (2) Snake-prefix workload checked against the published Snake trace chunks. (3) Event rate versus circuit size (inverter chains from 1k to 200k parts) to test the cache-miss hypothesis without perf. (4) Protocol: at least six ABBA blocks per factor and mode; require at least four stable; report every ratio; run with idle sibling hyperthread and a quiet host. | Counter builds reproduce golden counters and traces. | If the size sweep is flat, drop the cache-miss hypothesis and re-rank. |
| 1. Exact cleanups + gate caches | Items B, C, D, E, G, and cached handles for item A (still publishing every bit). Also selected hot files at `-O2` with generated tables kept at `-O1`. Commit the gate caches separately with this batch. Files: `BasicComponent.*`, `BitAdapter.tpp`, `Rewire.cpp`, `Event.hpp`, `Simulator.*`, `LogicValue.hpp`, build flags. | Identical counters, architectural state and trace bytes; timing-table snapshot; DelayRule/SRAM/flip-flop checks in three profiles; full native suite; WASM build plus gate/splitter/device parity checks. | Revert any piece that changes a counter or byte. |
| 2. Allocation-free value path | Port the guarded typed wire comparison. Then use packed one-word values (item F) in wire events, wire compare, propagation and a packed observer overload, keeping vector APIs at bindings and histories. Files: `Event.*`, `Wire.*`, `Pin.*`, `Simulator.*`, `SignalTrace.hpp`, trace writer. | As phase 1, plus derived-wire, width, X/Z, inertial-cancellation and uint64-time cases. | Expect allocations per event to fall well below 1; stop if CPU does not follow over six blocks. |
| 3. Incremental clocked twin | Promote the ordered-mask prototype, including in-place state. Keep full traversal for leaf models. File: `DelayTwin.cpp`. | 104,664-case signature in three profiles, golden traces, setup-violation counts, strict-settling scenarios, full suite, WASM parity. | Assess leaf-heavy workloads separately before claiming generality. |
| 4. Typed routes and pooled events | Typed sink routes and evaluation events (item H). Plain or pooled events with generation handles in place of `weak_ptr<Event>` for inertial cancellation. Needs owner decision 4. | Order contract (FIFO ties, wire preemption, cancellation, quanta); lifetime tests; full suite; WASM parity. | Pool memory must be capped and released. |
| 5. Fewer events (owner decision 3) | Drop a publication that equals the last value scheduled for that output, on fixed-delay non-inertial parts (splitters first). Remove or merge hierarchy-forwarding events (item I). | Identical wire/pin histories and trace bytes, architectural state and setup counts. Review `DrainResult.final_time` and event-limit stops, which depend on processed events. Re-baseline published counters. | Do not apply to edge-sensitive or self-timed parts. |
| 6. Indexed execution form (owner decisions 2, 4) | A design record first, then a bounded prototype on one subsystem with exact traces. | As phase 5, plus visualizer/history views. | Proceed only if phase 0 supports the cache-miss hypothesis. |

The recorder track runs independently and benefits capture only: packed
values to the observer, adaptive rows, object-keyed register lookup, and the
off-thread compression experiment from item J.

### Owner decisions needed

1. Allow `perf` during phase 0 (temporarily `kernel.perf_event_paranoid=1`)
   or install valgrind, so cache misses and shared-library costs can be measured.
2. Define the target: 10M of today's events per second, or guest cycles per
   second at fixed fidelity.
3. Allow event-count changes (phase 5) and re-baselining of published evidence.
4. Allow simulator-owned index tables (2026-10-13 decision 1) for phases 4 and 6.
5. Commit the gate caches now, or batch them with phase 1 (recommended: batch,
   then one full suite and WASM parity).
6. Provide quiet-host windows for timing (gameplay, preview and other heavy
   jobs paused).
7. Approve lossless disk recovery (below).

### Resources

At review time the disk has 3.5 GiB free (100% used). Experiment artifacts
total 4.6 GiB.

- **Archive copies.** Three `libSimulatorLib.a` copies are each 121,291,316
  bytes: `implementation/snapshot`, `wire-timing/snapshot` and
  `implementation/control-build-failed-clang-driver/snapshot`. Hard-link them
  if their SHA-256 values match (about 230 MiB; identity unverified).
- **Plain executables.** Probe executables of 40–90 MiB each remain under
  `wire-timing/` (for example the `memo-*` directories). They can be gzipped
  through `archive_verification.py`, as before.
- **Kept as-is.** Sources, receipts, results and traces stay.
- **Build guard.** Require at least 2 GiB free before any build; the space a
  full isolated rebuild needs is unmeasured.

## 2026-10-27 — D1 exact cleanups implemented

Changes, each behavior-preserving:

- `BasicComponent.cpp`: `CIRCUITSIM_DELAY_TRACE`, `_SETUP_TRACE` and
  `_STRICT_SETTLING` are read once on first use (same "present means set"
  meaning); `DelayRuleTest` sets its variable before building any part. The
  three one-element `std::vector<LogicValue>{…}` temporaries per timed
  evaluation are replaced by a size-and-value check.
- `Rewire`: bit routes are resolved to pin handles once and cleared by the
  `pinsChanged()` hook; evaluation uses indexed vectors instead of a
  name-keyed `std::map` and per-mapping pin lookups. Missing inputs still
  leave their bits unmapped; outputs are scheduled in the same order.
- `BitJoiner`: input handles cached the same way. `BitSplitter`: output names
  built once per width instead of per bit and evaluation.

Verification: 53 focused CTests pass (the RV32IM/M/decoder/device set, sweeps,
`RewireValidationTest`, joiner/splitter, `GateInputCacheTest`,
`TickRecordingTest`, delay-rule and table checks); regenerated delay tables
are identical for all 809 existing entries in all three profiles.

Measurement (same host, sequential, two runs each, user CPU seconds; the
"before" binary already had B1 and the word-gate adoption, so the difference
is D1 plus the 24 new tables, which these structural tests do not use):

| Test | Before | After | Change |
| --- | --- | --- | --- |
| `ALUWidthContractTest` | 19.19 / 19.28 | 15.26 / 14.97 | −21% |
| `MulDivGateLevelTest` | 38.71 / 38.47 | 36.30 / 36.63 | −5.5% |
| `RV32IMFiveStageCoreTest` | 88.97 / 88.66 | 87.50 / 87.03 | −1.7% |

Splitter/Rewire-heavy gate-level circuits gain most. This is not the record's
paired ABBA protocol and not a guest/Snake measurement. The gate-input cache
from 2026-10-26 is part of both binaries. Remaining D1 items: output-side pin
caching, inline heap keys and `-O2` hot files.

## 2026-10-27 — Inline queue keys and typed wire comparison

- `Simulator` keeps each queued event's sort keys (time, priority, scheduling
  order) beside its pointer (`QueuedEvent`), so heap comparisons no longer
  dereference events or call the virtual `getPriority()`. The order is the
  same comparison as `Event::EventComparator`; `scheduleEvent` reads the
  priority once. 51 focused CTests pass after this change, including the
  simulator advance/drain/history checks, gates, latches and flip-flops, the
  RV32IM/M/device/memory/cache/time tests and tick recording.
- Measurement attempt: an interleaved A/B run (two runs each) was invalid. The
  host was loaded by unrelated work (load average about 6 on 4 cores), and the
  unchanged binary itself ranged from 36 to 63 s on `MulDivGateLevelTest`. No
  speed is claimed for this change; re-measure on a quiet host.
- `WireUpdateEvent<W>::process` first compares its array with the wire's array
  (`Wire<W>::holds`) and returns on a no-op update, exactly the comparison
  `processWireUpdateEvent` makes, before building any vector. A changed update
  takes the existing path. This is the typed comparison measured privately on
  2026-10-26 (32% fewer allocation requests on the short guest), now in the
  production event.

## 2026-10-27 — Typed wire comparison verified; incremental twin in production

- Typed wire comparison (`Wire<W>::holds` in `WireUpdateEvent<W>::process`):
  51 focused CTests pass on that build.
- D3: `src/analysis/DelayTwin.cpp` now contains the ordered-mask incremental
  twin from `wire-timing/incremental/DelayTwin-bitset.cpp`, with the
  incremental path always on and the diagnostic counters removed. Gate-only
  clocked twins re-evaluate only gates whose input value or arrival changed,
  in topological order; first calls, stateless twins and twins containing
  leaves keep the whole-list evaluation. Node value/time arrays are owned by
  the twin state (no per-call copy back).
- Equivalence: `tests/cpp/DelayTwinReference.inc` keeps the previous
  implementation (namespace `circuit::analysis::reference`) for the new CTest
  `DelayTwinIncrementalTest`. It drives reference and production twins with
  identical clocked timelines (clock, periodic reset, sparse/dense data with X,
  arrival-only changes, two interleaved owners each) and requires the same
  answer, output values and arrival times at every call: 72,000 steps over
  `Register<8>`, `Register<32>`, `ClockedCounter<8>` and `MulDivUnit<4>` in
  every timing profile. A deliberate fault (one dirty position in 64 never
  marked) failed it at step 25; reverted.
- 52 focused CTests pass after D3 (536 s), including the equivalence test.
  Not yet run: `DelayRuleTest` (slow) and a guest/game timing measurement; the
  private 2026-10-26 measurements (66% of node evaluations skipped, about 4–5%
  less guest CPU in its stable blocks) are the only speed evidence.

## 2026-10-27 — Cached sink routes; DelayRuleTest after D3 and D4

- D4 (typed fanout routes, first part): `WireBase` keeps a `SinkRoute` per sink
  pin (evaluate its basic part, forward into/out of a composite, or nothing).
  `propagateWireChange` validates the cache against the live sink list on
  every call (same pins, same order, by control block) and rebuilds it when the
  list has changed, so the two `dynamic_pointer_cast`s and the owner lookup per
  sink and change are gone. A forwarding sink still checks that its owner is
  alive; a released basic part fails its weak lock and is skipped, as before.
  Scheduling order is unchanged. The value is still copied through a vector.
- 52 focused CTests pass on the D4 build (511 s), and `DelayRuleTest` (the
  slow compact-timing rule check, which exercises the incremental twin)
  passes in 1,073 s.
- Not done: pooled event allocation (no measured need yet; allocations were
  bounded at about 10% of CPU), typed value copies in fanout, `-O2` hot files,
  and run-time pin width (a staged refactor; not started). A paired guest
  measurement of D1–D4 on a quiet host is the next step.

## 2026-10-27 — Paired timing of D1–D4

One A/B/B/A block per test, pinned to CPU 3 (`taskset -c 3`), user CPU seconds
of the whole test process (setup included), load average about 1.4–1.8 from
unrelated work. Ratios are the geometric mean of the two B runs over the two A
runs. Endpoint (A–A) spread is at most 3.7%, inside the 5% stability rule.

| Comparison | Test | A runs | B runs | B/A |
| --- | --- | --- | --- | --- |
| D1–D4 vs before D1 | `ALUWidthContractTest` | 19.27 / 18.91 | 12.66 / 12.79 | 0.667 (−33%) |
| D1–D4 vs before D1 | `MulDivGateLevelTest` | 38.86 / 39.77 | 30.99 / 31.08 | 0.789 (−21%) |
| D3–D4 vs before D3 | `MulDivUnitWidth32Test` | 12.44 / 12.43 | 5.12 / 5.11 | 0.411 (−59%) |
| D3–D4 vs before D3 | `RV32IMFiveStageCoreTest` | 82.27 / 85.33 | 79.84 / 80.29 | 0.956 (−4.4%) |

"Before D1" already had B1 and the word-gate adoption; "before D3" had D1,
the queue keys, the no-op wire check and the word-gate tables. The gate-level
tests gain most from adapter caching, the queue and the wire check; the
compact 32-bit M unit gains most from the incremental twin. One block per test
is limited evidence, not a confidence interval, and none of these is the Visual
guest or a game capture; the owner's 10 million events/second target remains
unmeasured.

## 2026-10-27 — One-byte LogicValue

`enum class LogicValue : uint8_t` (it was int-sized), so every stored value
array and vector shrinks to a quarter. No code depends on its size or
reinterprets its bytes; trace packing uses explicit encodings. Full rebuild;
55 focused CTests pass, including the Python binding, session and pin-identity
visualizer checks.

One A/B/B/A block per test against the D4 binary (pinned CPU, load average
about 1.7): `ALUWidthContractTest` −1.4%, `MulDivGateLevelTest` −1.9%,
`MulDivUnitWidth32Test` −1.8%; endpoint spreads 0.5–3.2%. Small but
consistent; the larger memory benefit would show on large circuits, which were
not measured.

## 2026-10-27 — Typed fanout copies

`Wire<W>::propagateChange` now calls `propagateTypedWireChange<W>`
(`src/basic/Wire.cpp`, instantiated for the eight widths). It shares the
cached sink routes with the generic path, copies the wire's array into
same-width sink pins (`Pin<W>::setValueArray`) and builds same-width forwarding
events from the array; a different-width sink or target takes the previous
padded-vector path. Scheduling order is unchanged. 55 focused CTests pass.

One A/B/B/A block against the one-byte `LogicValue` binary (pinned CPU, load
about 1.5): `ALUWidthContractTest` −6.6%, `MulDivGateLevelTest` −1.9%,
`MulDivUnitWidth32Test` −1.1%; endpoint spreads 0.4–3.4%.

## 2026-10-27 — Visual guest: frozen 2026-10-26 baseline against the current build

The record's reference workload (`EnginePerformanceProbe guest 16 none`:
Visual RV32IM guest, 2,000 cycles, 117 instructions, oracle-validated) with
the frozen `baseline/probe` from 2026-10-26 (before the gate caches) and the
same probe source built against the current library (`-O1`, as before). One
A/B/B/A block, pinned to CPU 3, load average about 1.5.

| | A: 2026-10-26 baseline | B: current |
| --- | --- | --- |
| Execution seconds (`hostSeconds`) | 13.642 / 13.844 | 8.820 / 8.819 |
| Processed events | 14,302,358 | 13,740,072 |
| Events per second | 1.048 M / 1.033 M | 1.558 M / 1.558 M |
| Process user CPU | 14.39 / 14.63 | 9.59 / 9.62 |

Execution time falls 36% and events per second rise 49%; the endpoint
spread is 1.5%. The current build processes 3.9% fewer events for the same
guest work, because B1 removed adapters and phase C folds inverter words. The
gain combines the gate caches, D1–D4, the one-byte `LogicValue` and those
structural reductions; this run does not attribute it per step (the paired
test-level blocks above do). The 10 million events/second target remains far
off (1.56 M here); the remaining review candidates (run-time width/packed
values, pooled events, event reduction, an indexed execution form) are
untried, and D5/D6 need owner decisions.

## 2026-10-27 — WebAssembly check of the session's changes

`circuitsim_verification` (the registry test runner compiled with Emscripten in
`build-wasm-milestone`, link optimization 1) rebuilt in 14.5 min under a 3 GiB
cap. Run under Node, these pass: `WordGateSweepTest`, `AdderWidthSweepTest`,
`RV32IALUSweepTest`, `RV32IPipelineControlSweepTest`,
`BuiltinComponentCatalogInventoryTest`, `MulDivUnitWidth32Test`,
`MulDivGateLevelTest`, `RV32IMFiveStageCoreScenarioTest`,
`RV32IMArithmeticProgramTest`, `RV32IMDivideEdgesProgramTest`,
`SimulatorDrainUntilIdleTest` and `RewireValidationTest`. These check results
against references and both forms, not event-for-event native/WASM traces;
the browser module (`circuitsim_wasm`), web parity tests and the canvas change
were not rebuilt or run.

## 2026-10-27 — `-O2` on hot files

`CMakeLists.txt` adds `CIRCUITSIM_HOT_PATH_O2` (default ON). For non-Emscripten
builds whose release flags contain `-O1`, ten hot files (`Simulator.cpp`,
`Event.cpp`, `Pin.cpp`, `Wire.cpp`, `TemplateInstantiations.cpp`,
`BasicComponent.cpp`, `IOComponent.cpp`, `Gate.cpp`, `Rewire.cpp`,
`DelayTwin.cpp`) get `-O2` after the release flags, so `-O2` wins; `-O3`
builds and WebAssembly are unchanged. 30 focused CTests pass.

One A/B/B/A block against the same sources at `-O1` (pinned CPU, load about
1.9): `ALUWidthContractTest` −0.2%, `MulDivGateLevelTest` −0.3%,
`MulDivUnitWidth32Test` −2.9%; endpoint spreads 0.5–0.8%. Small, consistent
with the 2026-10-26 selected-file result being only about 3%; kept as an
option because it is cheap and exact.

Visual guest (`guest 16 none`) against the frozen 2026-10-26 baseline after
`-O2` and the unused-split removal: 13.759 / 13.552 s → 8.401 / 8.380 s
(−38.6%), 1.04 M → 1.64 M events/s, 14,302,358 → 13,729,079 events, both
validated.

## 2026-10-27 — D2: packed values and run-time signal width

Implemented in three stages, each rebuilt in `build-milestone`:

1. Packed values. `include/basic/PackedLogic.hpp` holds a four-state value of
   up to 64 bits in two words (`value`, `unknown`; LOW 00, HIGH 10, UNKNOWN 01,
   HIGH_Z 11 as value/unknown bits, the enum's numbers). Bits above the width
   are always zero, so equal signals compare as two words. Conversions keep
   the old rules: short vectors pad with UNKNOWN, long ones are cut; a sink or
   target of another width sees the value truncated or UNKNOWN-padded
   (`resized`), exactly as the padded-vector fanout did.
2. One pin/wire class and one event. `PinBase` and `WireBase` own the width
   and the packed value; their accessors are no longer virtual. `Pin<W>` and
   `Wire<W>` are stateless typed handles (`getInputPin<W>` still checks the
   width by type). `WireUpdateEvent` is one class for every width; the typed
   fanout (`propagateTypedWireChange<W>`) and the width dispatch in
   `makeWireUpdateEvent` are gone. `Rewire`, `BitJoiner`, the gate input
   caches and the delay-table path in `BasicComponent` work on packed values
   (the table path keeps value vectors only for the delay twin and refreshes
   them only when an input changed). `WordGateBehavioral` evaluates whole
   words with packed four-state rules (`circuit::logic_values::packed`), the
   same per-bit gate rules.
3. Run-time width. `circuit::dispatchWidth` covers every width 1–64 through a
   function table; `src/basic/TemplateInstantiations.cpp` and
   `kSupportedWidths` are removed. Which widths a component family offers
   stays that family's contract (`Register` now lists its eight widths
   itself). `Rewire` accepts any width 1–64.

Three files relied on `<algorithm>` arriving through the old `Pin.tpp`/
`Wire.tpp` and now include it. The research scripts under
`tools/engine-performance/` that name `TemplateInstantiations.cpp` describe
the 2026-10-26 build and are not updated.

Tests: new `SignalWidthTest` (packed round trip at 1/7/33/63/64 bits, padding
and truncation, dispatch of all 64 widths and rejection of 0 and 65, a
`Rewire` between 64-, 7- and 13-bit wires with random four-state values).
`RewireValidationTest` now expects 7 and 64 bits to be accepted and 65
rejected. After stages 1–2: 23 focused CTests and all 26 non-slow timing
CTests pass. After the packed hot paths: the same 23 focused CTests pass
(`WordGateSweepTest` compares the packed word gates with their gate-level
form at every tick). After stage 3: the rewire tests, `SignalWidthTest`,
`GateInputCacheTest`, `WordGateSweepTest`, the catalog inventory and the
Python module bindings pass.

Speed, user CPU on a pinned core against the pre-D2 runner (A/B/B/A):
after stages 1–2 alone (load about 1.4–2.3) `ALUWidthContractTest` +2.5%,
`MulDivGateLevelTest` +1.8%, `MulDivUnitWidth32Test` +2%,
`RV32IMFiveStageCoreScenarioTest` −1%: no gain, because hot code still
converted to and from vectors. A block after the packed hot paths ran at load
5–6 from other projects' jobs (times nearly doubled) and is discarded.

Re-measured at load 3–4 (other projects' jobs continued all evening), same
method. Fastest of each four runs, pre-D2 → D2: `ALUWidthContractTest`
11.11 → 10.87 s (−2%), `MulDivGateLevelTest` 27.58 → 26.78 s (−3%),
`MulDivUnitWidth32Test` 6.46 → 6.63 s (+2.6%),
`RV32IMFiveStageCoreScenarioTest` 6.91 → 7.36 s. The five-stage test was
repeated as five A/B/B/A blocks (ten runs each): median 6.82 s before, 6.76 s
after, fastest 6.49 / 6.55 s, so no change. Spread within one binary reached
50% in single runs; differences under about 5% are not resolved here.

Before the five-stage repeat, the twin's per-input change marking in
`BasicComponent::evaluateAt` was changed to walk the set bits of one changed
mask (two XORs) instead of comparing every bit; the same nine focused CTests
pass, including `DelayTwinIncrementalTest`.

Visual guest (`EnginePerformanceProbe guest 16 none`, oracle-validated),
two A/B/B/A blocks pinned to CPU 3, against the probe of 09:04 that produced
the 13.74 M-event figure above. That build predates typed fanout, the
Chapter 2 word gates and the zero-delay `unit` splitter as well as D2, so the
comparison spans all four.

| | A: 09:04 build | B: D2 build |
| --- | --- | --- |
| Execution seconds (`hostSeconds`) | 10.58 / 9.84 / 10.25 / 10.01 | 10.02 / 9.81 / 10.08 / 9.94 |
| Process user CPU | 11.48 / 10.77 / 11.20 / 10.96 | 10.82 / 10.76 / 11.01 / 10.95 |
| Processed events | 13,740,072 | 15,457,643 |
| Wire updates / evaluations | 9.73 M / 4.01 M | 11.30 M / 4.16 M |
| Effective wire changes | 4.31 M | 4.59 M |
| Events per second (mean) | 1.35 M | 1.55 M |

Execution time is 2% lower while the guest processes 12.5% more events, so
events per second rise about 15%. The extra events are not separated by
change. D2 keeps the event rules (the no-op check, UNKNOWN padding, the
constructors' bit rules), and the broad suite's timing and trace CTests pass
unchanged; the more likely source is the regrouped Chapter 2 parts and the
zero-delay splitter, which change when values arrive. Absolute times are
about 13% above the 8.82 s measured at load 1.5, so this block is not compared
with that one.

`DelayRuleTest` (slow) after D2 and the change-mask edit: passes in 784 s
(the floors set after the splitter change hold; no part breaks the rule).
The broad non-slow native suite after stage 3 passed 362/365 in 5,746 s; the
three failures are the ones that failed before D2.

## 2026-10-27 — Pooled events

`include/simulator/EventPool.hpp`: `circuit::makeEvent<T>` is
`std::allocate_shared` with a per-thread free-list allocator (16-byte size
classes up to 256 bytes, larger blocks straight from the heap). A finished
event's block (event plus control block, one allocation as with
`make_shared`) is reused instead of freed. Ownership, `weak_ptr` cancellation
of inertial changes and destruction order are unchanged. Every small block
has its class's full size, so a block freed on another thread may join that
thread's list; after a thread's pool is destroyed, its blocks go straight back
to the heap. Free lists do not shrink: they hold at most the peak number of
live events per thread. Used at the nine hot allocation sites (fanout
evaluations and forwards in `Wire.cpp`, the `BasicComponent` outputs,
`Rewire`, `makeWireUpdateEvent`); other `make_shared` event sites are
unchanged.

Checks: 17 focused engine/M-unit/binding CTests (scheduler, recording,
transport, gate caches, signal widths, twin, rewires, word gates, gate-level
M unit, five-stage core) and all 26 non-slow timing CTests pass.

Speed, pinned to CPU 3, load 2.6–2.9 (same tables and probe source, built
against the library without and with the pool):

| | A: no pool | B: pool |
| --- | --- | --- |
| Visual guest `hostSeconds` (6 runs each) | 8.33 / 8.47 / 8.30 / 8.43 / 8.35 / 8.22 | 7.83 / 7.89 / 7.78 / 7.82 / 7.77 / 7.82 |
| Process user CPU | 9.18 / 9.23 / 9.04 / 9.20 / 9.13 / 8.98 | 8.63 / 8.70 / 8.56 / 8.64 / 8.57 / 8.63 |
| Processed events | 15,457,313 (all runs) | 15,457,313 (all runs) |

Execution time −6.3% (mean 8.35 → 7.82 s), user CPU −5.6%; every pool run is
faster than every run without it; 1.85 M → 1.98 M events/s. Test-level
blocks: `MulDivGateLevelTest` −2.4%, `ALUWidthContractTest`,
`RV32IMFiveStageCoreScenarioTest` and `MulDivUnitWidth32Test` within ±1%
(one block each). The guest's absolute time is also lower than the D2 block
above because the second `unit` table pass shortened its compact parts'
delays (same event count before and after the pool here).

## 2026-10-27 — Pooled events in WebAssembly

The WASM build with pooled events (`build-wasm-milestone`) passes 16 tests in
the WASM verification runner and 14 of 16 native/WASM parity checks on the
first run; the two failures were stale test expectations, fixed and re-checked
(details in the [non-device plan record](2026-10-27-non-device-improvement-plan.md),
"WebAssembly rebuild, parity checks and site build"). Event counts match
native in the recorded-guest comparison. No WASM speed measurement was made.
