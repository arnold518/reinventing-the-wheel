# Engine performance: measurement and plan

Started: 2026-10-13. Status: open. Code state: `github-pages` at `448e4e3`, with an uncommitted documentation reorganisation in the working tree. No source file was changed by this work.

## Summary

Compact (behavioral) cores run at 3,700 to 13,400 cycles per second and the
gate-level advanced core at 51. The event queue is not the main cost. Each
event costs 0.7 to 5 µs, where a lean engine should need 50 to 100 ns, and
most scheduled wire updates change nothing. The time goes to pin lookups by
name, small heap allocations, values copied as vectors, and outputs re-sent
unchanged. An eight-phase plan is proposed below; nothing is implemented.
Next step: three decisions by the owner (see "Open decisions"), then phase 0.

## 2026-10-13 — Why this was looked at

The question was whether larger workloads (memory-mapped devices, a program
the size of DOOM) can run on compact cores while keeping the pins, the memory
components and the event queue. A frame of such a program is roughly 1 to 3
million instructions and a boot 100 million or more; these two figures are
estimates, not measurements. At today's speed that is minutes per frame.

## 2026-10-13 — Measurement

Binary: `build-cleanup/rv32i_experiment`, Release, built 2026-10-13 04:53
from the tree at `448e4e3`. Host: 4 cores, 7 GB. History is disabled by the
tool. Default flip-flop memory.

```sh
build-cleanup/rv32i_experiment --core <single|baseline|advanced> \
  --workload crc32 --iterations 200 --period auto --no-verify \
  --max-cycles 100000 --max-events 100000000 --allow-incomplete
# gate level: add --fidelity reference, --max-cycles 300, --max-events 2000000000
```

All runs stop at the cycle budget, so the program does not finish. Figures
come from `host_execution_seconds` and the simulator's own counters in the
JSON output. One run each; no repetitions.

| Run | Cycles | Seconds | Cycles/s | Events/cycle | Evaluations/cycle | Wire updates/cycle | Effective changes/cycle | Time/event |
|---|---|---|---|---|---|---|---|---|
| Single-cycle, compact | 100,000 | 7.485 | 13,360 | 59.3 | 10.0 | 49.3 | 8.0 | 1.26 µs |
| Baseline pipeline, compact | 100,000 | 27.114 | 3,688 | 51.8 | 8.4 | 43.4 | 6.4 | 5.24 µs |
| Advanced pipeline, compact | 100,000 | 8.493 | 11,775 | 55.8 | 9.6 | 46.2 | 7.6 | 1.52 µs |
| Advanced pipeline, gates | 300 | 5.902 | 51 | 28,898 | 8,440 | 20,458 | 9,692 | 0.68 µs |

Observations:

- Compact runs schedule about 46 wire updates per cycle and about 8 change a
  value: roughly 84% are discarded when processed. At gate level 53% are.
- The compact cores already advance one cycle in one function
  (`RV32IFiveStageCoreBehavioral::advanceOneCycle`). The cost is around it.
- Setup (building the system and deriving the clock) takes about 1 s of wall
  time for the advanced core and is outside the figures above.

## 2026-10-13 — Profile

`perf` is not usable on this host (`kernel.perf_event_paranoid=4`). Stacks
were sampled instead: the program ran under `gdb -batch`, was interrupted
about every 50 ms, and printed a 14-frame backtrace each time. 112 samples
(advanced, compact), 142 (baseline, compact), 133 (advanced, gates). The
binary has no debug information, so inlined code is charged to its caller.
Shares are inclusive, overlap, and are good to about ±5 points.

Advanced pipeline, compact:

| Share | Where |
|---|---|
| 78% | `BasicComponent::evaluateAt` and below |
| 40% | `RV32IFiveStageCoreBehavioral::driveOutputs` |
| 23% | `BehavioralMemoryEndpoint::evaluate` |
| about 20% | `operator new`, `malloc`, `free` |
| 14% | `memcmp` (pin-name string comparison) |
| 10% | `BasicComponent::publishTime` |
| 9% | `IOComponent::getOutputPinDynamic` |
| 5% | `IOComponent::getInputValue` |
| 3% | `Simulator::scheduleEvent` |

Baseline pipeline, compact:

| Share | Where |
|---|---|
| 74% | `rv32i::baseline::BehavioralCore::evaluate`, of which `comb` 39% |
| 28% | `std::map<std::string, …>` tree operations |
| 23% | `memcmp` |
| 11% | `BehavioralMemoryEndpoint::evaluate` |

This core keeps its signals in a `std::map<std::string, unsigned>`, which is
why it is three times slower than the other two compact cores.

Advanced pipeline, gates:

| Share | Where |
|---|---|
| 41% | `ComponentEvalEvent::process` and below |
| 34% | `WireUpdateEvent<1>::process` |
| 25% | `propagateWireChange` |
| 19% | `BitSplitter<32>::evaluate` and `BitJoiner<32>::evaluate` together |
| 17% | `Simulator::drainUntilIdle` itself (queue push and pop, inlined) |
| about 16% | allocation and release |
| 11% | `__dynamic_cast` |
| about 2% | `GateTwin::settle` |

The gate-timing pass (`DelayTwin::settle`) did not appear in the compact
profiles.

## 2026-10-13 — Causes in the source

- Pins are found by name on every use. `getInputValue("CLK")` and
  `_updateOutputWire(sim, "FETCH_PC", …)` search
  `std::map<std::string, std::shared_ptr<PinBase>>`
  (`include/components/IOComponent.hpp`), and `publishTime` hashes the same
  name again (`src/components/BasicComponent.cpp:275`).
- Outputs are re-sent unchanged. `_updateOutputWire` always schedules an
  event; the comparison happens only when the event is processed
  (`src/simulator/Event.cpp:24`).
- Values are copied as vectors. `evaluateAt` calls `getValueAsVector()` for
  every input on every evaluation (`BasicComponent.cpp:185`), and
  `WireUpdateEvent::process` builds another vector
  (`include/simulator/Event.tpp:36`). A 32-bit wire is 32 separate
  `LogicValue` elements.
- Each event is one `std::make_shared`, and the queue comparison calls the
  virtual `getPriority()` (`include/simulator/Event.hpp:27`).
- Fan-out is worked out on every change. `propagateWireChange`
  (`src/basic/Wire.cpp:63`) cleans the sink list, locks weak pointers and
  does up to two `dynamic_pointer_cast`s per sink.
- Each hierarchy level costs one more event: a composite's boundary pin
  schedules the value again onto its inner wire (same function).
- With history off, `recordPinChange` still writes to a `std::map` keyed by
  `shared_ptr` (`src/simulator/Simulator.cpp:213`).
- `ComponentEvalEvent::process` does a `dynamic_pointer_cast` per evaluation
  (`src/simulator/Event.cpp:13`).

## 2026-10-13 — Proposed plan

Every phase keeps pins, families, profiles, delay tables and wire histories
as they are. Gains are estimates, not measurements.

| Phase | Change | Helps | Estimated gain | Risk |
|---|---|---|---|---|
| 0 | Fixed benchmark set, a build with frame pointers for profiling, today's numbers and counters recorded | — | — | none |
| 1 | Pin handles resolved once; `publishTime` by index; name-based calls kept as a thin layer for tests and bindings | both | 1.5–2× | low |
| 2 | Do not schedule an output whose value equals the last one sent | compact mostly | 2–3× | medium |
| 3 | Baseline compact core and memory device models: plain fields in place of string-keyed maps | baseline core, memory | 3× on baseline | low |
| 4 | Packed four-state values (two bit masks per wire); vectors only at the Python, WebAssembly and history boundary | both | 2–3× | medium; touches every header |
| 5 | Events stored by value; fan-out lists built once; no `dynamic_cast` on the hot path | both, gates most | 2–3× | medium |
| 6 | Nets merged across composite boundaries; splitters and joiners as bit slices | gates | 2× | high; the visualizer reads per-wire history |
| 7 | Optional: link-time and profile-guided optimisation | both | 1.1–1.3× | low |

Expected after all phases (estimate): compact cores at 300,000 to 600,000
cycles per second, gate level at 300 to 500. One million needs a further
step: evaluating a clocked part only for the inputs it reacts to.

Checks per phase:

- Phases 1, 3, 4 and 5 must not change behaviour. `SimulatorPerformanceCounters`,
  final state fingerprints and `--trace-cycles` output must be identical
  before and after.
- Phases 2 and 6 change event counts on purpose. Wire histories and results
  must stay identical; the new counts become the reference.
- The tie-break order (time, wire updates before evaluations, scheduling
  order) must be preserved exactly; tests and the native-to-WebAssembly
  parity checks depend on it.
- Phase 2 must be checked against the setup-time watcher and
  `CIRCUITSIM_STRICT_SETTLING`; both depend on when outputs are published.
- Focused tests between fixes, the full suite once per phase. Phases 4 and 5
  each force a full rebuild. Web tests after phase 4.
- No structural circuit changes, so delay tables need no regeneration.

Rejected: a queue-free model of the core outside the component system. It
has no pins, so a profile cannot select it and the visualizer cannot show it;
it would break the rule that both fidelities look the same from outside. If
ever wanted it would be a named tool beside the instruction oracle, not a
fidelity.

## Open decisions

1. Phase 5 needs the simulator to refer to pins and components directly.
   `AGENTS.md` forbids stored raw pointers. Proposed: an index table owned by
   the simulator and rebuilt when the tree changes.
2. Phases 2 and 6 change event counts. Whether any published page or result
   quotes event counts was not checked.
3. Order. Proposed: phases 0 to 3 first (low risk, most of the compact gain),
   then 4 to 6.

## Limits

- One workload (`crc32`), one memory technology (flip-flop), one run per
  configuration. SRAM, DRAM, ROM and NOR-flash systems were not measured.
- Sampled profile with about 110 to 140 samples per run and no debug
  information; small shares are unreliable.
- The native visualizer path (history on) and the WebAssembly build were not
  measured.
- No test was run; nothing in the repository was built.
