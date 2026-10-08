# Clock Analysis Contract

[Execution and measurement](execution.md) · [Timing tests](verification/timing.md) · [Historical development](../4.7/timing-history.md)

## Three clock choices

| Choice | Meaning | Evidence to retain |
| --- | --- | --- |
| Explicit `--period N` | User-selected scheduling period | N and the reason it is safe for every compared target |
| `--period auto` / `--clock-mode safe` campaign | Sufficient period for every four-state start | Per-target `--safe-clock` certificate |
| `--clock-mode binary-safe` campaign | Sufficient period under a checked binary-startup contract | Per-target `--safe-clock --binary-startup` certificate |

The earlier candidate clock, `2 * (D + floor(D / 4) + 2)` over a longest-path estimate, is retired together with its `--timing` option. Every modeled period now comes from `computeSafeClockPeriod`.

`computeSafeClockPeriod` is a non-mutating analysis. A successful result is sufficient **under explicit simulator contracts**, not a physical frequency, exact minimum clock, or proof of ISA correctness. Both fidelities use the clock from the same structural reference profile.

## Sufficient-period calculation

The analyzer follows combinational arcs between clocked launches and captures, retaining earliest and latest arrival bounds. Asynchronous register/memory reads remain combinational paths. An explicit component contract describes arcs, captures, launches, fixed inputs and pulse-width requirements.

For arrival bounds `early` and `late`, setup/hold constraints and event separation g:

| Requirement | Bound |
| --- | --- |
| Same-edge setup for next occurrence | period ≥ late + setup + g |
| Opposite-edge setup | period ≥ 2 × (late + setup + g) |
| Same-edge hold | early ≥ hold + g; a longer period cannot fix it |
| Opposite-edge hold | period / 2 + early ≥ hold + g |
| Pulse width | period / 2 ≥ maximum required high/low pulse |

Take the maximum period bound and round upward to an even tick count. The general API can return two ticks; the experiment scheduler additionally requires four. Supported feedback is analyzed with bounded state/domain reasoning rather than deleting a dependency just because it looks inconvenient. Primitive delays and state restrictions must reach wires before they can constrain downstream settling.

## Scope and fail-closed behavior

The supported environment uses one ideal, directly distributed clock, 50% duty cycle, declared input transition windows/fixed values, and declared output sampling constraints. Reset settles before operation. Runtime image mutation, unsupported clock gating, multiple domains and implicit multicycle exceptions are not silently accepted.

| Status | Period | Interpretation |
| --- | --- | --- |
| `safe-under-contracts` | Number | All modeled requirements satisfied under the reported premises |
| `timing-violation` | null | A required conservative constraint fails |
| `unknown` | null | Missing contracts, unsupported logic/environment, unresolved analysis or exhausted budget |

An unresolved dependency is not automatically a proven physical oscillator. Likewise, a conservative hold failure is not necessarily a concrete failing program. Report diagnostics and assumptions instead of substituting a guessed clock.

## Binary startup

The experiment's normal zero-filled memory and binary reset state support a narrower certificate:

1. Every supported endpoint image, including sparse default-fill storage, is binary before evaluation.
2. Reset and initial settling establish binary storage state.
3. Changing external inputs carry the required binary-at-edge and timing promises.
4. Settling/setup/hold reasoning establishes binary captures in the next cycle, preserving the premise inductively.

This proof is not specialized to a chosen program's data values. Four-state/X/Z experiments require their own valid scope; binary startup cannot certify them. Caller-supplied or unsupported device timing contracts are not automatically endowed with the built-in binary-state proof.

## Reproduction

```sh
build/rv32i_experiment --core single --safe-clock --binary-startup --allocation-limit-mib 512
build/rv32i_experiment --core baseline --safe-clock --binary-startup --allocation-limit-mib 512
build/rv32i_experiment --core advanced --policy two-bit --entries 16 --safe-clock --binary-startup --allocation-limit-mib 512
```

Recompute after circuit, predictor capacity, endpoint, fidelity or contract changes. Delayed technology latencies are device ticks; they do not automatically rescale with the CPU period. For predictor cycle-efficiency comparisons, use a common safe period. For modeled total time, use and disclose each target's own certified period.

The [historical timing narrative](../4.7/timing-history.md) preserves previous implementation limitations and numeric bounds. It is not the current support table.

## Source and tests

`include/analysis/SafeClockTiming.hpp`, `src/analysis/SafeClockTiming.cpp`, `src/analysis/MemoryTimingState.cpp`, and `include/analysis/ExperimentClock.hpp`. The [timing test reference](verification/timing.md) separates contract checks, sufficient-bound analysis, and dynamic program regressions.
