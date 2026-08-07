# RV32I Timing Model and Performance Counter Report

Date: 2026-08-05  
Branch: `rv32i-pipeline-performance`

## Outcome

CircuitSim now measures three separate things:

1. **Hardware work:** cycles, retired instructions, CPI, IPC, stalls, flushes,
   and pipeline occupancy.
2. **Circuit timing:** the longest normalized-delay path through the circuit
   that was actually instantiated.
3. **Simulator cost:** event count, component evaluations, value changes,
   maximum event-queue depth, and host wall-clock time.

The counters only observe existing events and output signals. They do not add
counter registers to the CPU or change its critical path.

## Measurement Model

`SimulatorPerformanceCounters` records scheduled and processed wire/component
events, effective wire and pin changes, and maximum queue depth. RV32I tests
reset it after initialization, so reported work belongs to the program window.

Every lockstep program reports hardware cycles, retired instructions, CPI,
and IPC. The five-stage core also exposes one-cycle observations for load-use
stall, data-memory readiness stall, single-data-port conflict, younger-work
flush, and occupancy of its four inter-stage registers. The structural
coordinator builds these signals from gates; the behavioral core computes the
same external contract. Program tests observe them before each active edge.

`CircuitTimingAnalyzer` walks the instantiated component/wire hierarchy and
uses each `BasicComponent` delay as a weight. Structural D flip-flops terminate
one path and start the next. Compact stateful memories, register files, and
sequential behavioral implementations are one-delay timing boundaries;
selecting structural fidelity exposes their internals. Combinational cycles
are detected and produce an invalid report rather than a fake large delay.

This is a normalized educational model, not nanoseconds. It excludes
transistor and routing delay, setup/hold, clock skew, voltage, synthesis, and
physical placement.

## Workloads

Programs 17-20 remain focused microbenchmarks. Two application-style kernels
were added:

- **Program 21:** sum a 16-word array with a scalar load/add/pointer/branch
  loop.
- **Program 22:** copy eight words while computing a checksum, combining
  loads, stores, dependencies, pointer updates, branches, and independently
  checked memory writes.

After the initial internal experiment, the full CRC32 program from the online
Embench 1.0 suite was built as a pinned RV32I ELF and executed by both
behavioral cores. It retired 6,130,443 instructions on each core. The
five-stage core used 7,181,789 cycles (CPI 1.1715), and the normalized timing
model predicts a 1.494x speedup. See `rv32i-external-benchmark-report.md` for
source provenance, independent Sail validation, reproducible commands, and
scope limits. The six smaller programs remain useful because they identify
the causes of individual stalls and flushes and are practical at structural
fidelity.

## Hardware Results

The behavioral five-stage implementation produced the table below. Programs
21 and 22 were also run structurally and matched every cycle, stall, flush,
retirement, architectural state, and memory effect.

| Program | Focus | Instructions | Single cycles | Five-stage cycles | CPI | Load-use stalls | Flushes |
|---:|---|---:|---:|---:|---:|---:|---:|
| 17 | Independent ALU chains | 73 | 73 | 77 | 1.055 | 0 | 1 |
| 18 | Dependent ALU chain | 66 | 66 | 70 | 1.061 | 0 | 1 |
| 19 | Unrolled load-use pairs | 67 | 67 | 103 | 1.537 | 32 | 1 |
| 20 | Branch-heavy loop | 51 | 51 | 85 | 1.667 | 0 | 16 |
| 21 | 16-word array sum | 84 | 84 | 134 | 1.595 | 16 | 16 |
| 22 | Copy plus checksum | 61 | 61 | 87 | 1.426 | 8 | 8 |
| **Total** | | **402** | **402** | **556** | **1.383 weighted** | **56** | **43** |

All six observed zero memory-readiness stalls because memory answers
immediately. They also observed zero single-port conflicts; their schedules do
not place an older store and younger load on the port in the same cycle. Both
causes are independently covered by the coordinator contract test.

Interpretation:

- Forwarding works: programs 17 and 18 have almost the same CPI despite very
  different dependency distance.
- A dependent load costs one bubble: program 19 reports 32 stalls for 32
  load-use pairs.
- Taken branches cost two younger instructions with EX-stage resolution:
  program 20 takes 15 backward branches and pays 30 redirect cycles. The final
  terminal instruction is the extra flush observation.

## Timing Results

The default profile expands CPU datapath/control logic while keeping large
memory and register-file storage behavioral.

| Circuit scope | Critical delay units |
|---|---:|
| Single-cycle core | 189 |
| Single-cycle scenario with memory boundary | 190 |
| Five-stage Fetch | 70 |
| Five-stage Decode | 53 |
| Five-stage Execute | 93 |
| Five-stage Memory | 10 |
| Five-stage Writeback | 67 |
| Complete five-stage core/scenario | 105 |

The complete pipeline path is longer than isolated Execute. It continues
through redirect/kill coordination, fetch selection, and IF/ID flush logic
before reaching IF/ID state. The redirect feedback loop is therefore the next
timing target, not the ordinary ALU result alone.

Using `execution time = cycles x modeled clock period`:

| Program | Single modeled time | Five-stage modeled time | Speedup |
|---:|---:|---:|---:|
| 17 | 13,870 | 8,085 | 1.716x |
| 18 | 12,540 | 7,350 | 1.706x |
| 19 | 12,730 | 10,815 | 1.177x |
| 20 | 9,690 | 8,925 | 1.086x |
| 21 | 15,960 | 14,070 | 1.134x |
| 22 | 11,590 | 9,135 | 1.269x |
| **Total** | **76,380** | **58,380** | **1.308x** |

This supports a **30.8% speedup in the current normalized timing model**. It
is not a claim about fabricated hardware or host simulation speed.

## Simulator-Cost Experiment

| Architecture/program | Host seconds | Events | Component evaluations | Max queue |
|---|---:|---:|---:|---:|
| Single / 21 | 18.64 | 2,666,410 | 640,083 | 1,769 |
| Five-stage / 21 | 13.12 | 2,020,524 | 591,609 | 1,182 |
| Single / 22 | 10.40 | 1,531,621 | 379,351 | 1,773 |
| Five-stage / 22 | 7.87 | 1,038,305 | 302,931 | 1,068 |

The cleaned pipeline now also runs these two simulator workloads faster than
the structural single-cycle model. This is a host-software result for these
fixtures, not a general hardware claim; hardware speed and simulator speed
remain separate optimization targets.

## Visualizer and Layout

- Scenario responses include `performanceMetrics`.
- The top bar shows CPU cycles and CPI; its hover text lists every metric.
- Program aliases are generated from the C++ registry rather than a fixed
  program count.
- Deterministic layout regeneration traversed 165,231 component instances and
  produced layouts for 154 scenarios, including both new programs on both
  cores.

## Server Safety

Builds used two jobs; structural tests and layout regeneration ran one process
at a time. Swap was already essentially full before this work. Sampled
available RAM stayed at or above about 4.0 GiB. The largest monitored task was
the Python visualizer binding test at about 445 MiB RSS; layout regeneration
was about 247 MiB RSS in sampled checks.

For the 2026-08-05 cleanup, the user-owned visualizer process was deliberately
terminated so the unit's `Restart=always` policy could load the rebuilt
backend. The final replacement PID is `1572954`, `NRestarts=10`, the unit is
`active/running`, and `/api/health` returns `{"ok": true}`. Live browser
captures fully rendered single-cycle and five-stage program 9 at 9,826 and
13,243 components respectively.

## Verification

- Full serial CTest suite after the external benchmark integration:
  **175/175 passed** in 988.78 seconds.
- New programs 21-22 full cross-profile subset: **4/4 passed** in 170.08
  seconds.
- Visualizer module/binding regression: passed in 66.81 seconds.
- Layout regeneration: 154 scenarios, 112 representative topologies, 165,231
  traversed components, and no overlap exception.

## Reproduction

```bash
cmake -S . -B build
cmake --build build -j2

./build/rv32i_performance five 21 structural
./build/rv32i_performance single 22 structural

ctest --test-dir build -j1 --output-on-failure \
  -R 'RV32I(SingleCycleSystemTest|FiveStageCoreProgramTest)/program-(17|18|19|20|21|22)$'
```

## Next Experiment

Add configurable memory latency and a small tested cache contract. The
existing memory-stall counter can then separate load-use bubbles from cache or
memory waits, and these six programs can show whether reduced traffic is worth
the cache's additional critical-path delay.
