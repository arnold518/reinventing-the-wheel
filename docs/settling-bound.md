# Settling bound: a proven "no later than" for every output

`CircuitTimingAnalyzer::settling` (`include/analysis/SettlingBound.hpp`) answers one question
about a structural circuit: after its inputs stop changing, what is the last
tick at which each output can still change? The answer is an upper bound that
holds for every input, found from the wiring and the gate rules, without
trying inputs.

It replaces the earlier longest-path estimate (`CircuitTimingAnalyzer`, now
removed).

| Analyzer | Gives | Works on |
| --- | --- | --- |
| `CircuitTimingAnalyzer::safeClock` | A clock period with setup and hold checks | Clocked circuits whose parts all have contracts |
| `CircuitTimingAnalyzer::settling` | A proven latest change per output and per clocked capture | The same parts as the safe-clock analysis |

## Source layout

| File (`src/analysis/`) | Holds |
| --- | --- |
| `TimingCircuit` | Shared by the safe-clock and settling analyses: the built-in contract table, the walk that finds leaf parts, and the walk that finds which pin drives which |
| `SettlingGraph` | The circuit as one node per pin bit |
| `SettlingBound` | Timed evaluation of a case, and the search over cases |
| `SafeClockTiming` | The clock-period analysis; its memory-mode part is a sequence of named steps (`collectMemoryGroups`, `startMode`, `settleMode`, ...) |

## Why the result can be trusted

Every evaluated part publishes `f(inputs at t)` at exactly `t + delay`
(`BasicComponent::_updateOutputWire`). So if a part's inputs are constant from
tick `s`, its output is constant from `s + delay`. Adding these up along the
wiring is the plain longest-path bound. Three refinements lower it, and each
only removes paths that no input can use.

1. **Per bit.** A 32-bit pin is 32 separate signals. A path that enters a bus
   on one bit and leaves on another is not a path.
2. **Unchanging sources.** A constant, a held input or an undriven pin never
   changes, and neither does anything that depends only on those.
3. **Controlling values.** An AND input that ends at 0 fixes the output as
   soon as that input is fixed, whatever the other input does. The same holds
   for 1 on an OR. X and Z never control, exactly as in the gate code.

The third needs to know the value an input ends at, so it works by cases.

## Cases

With `binary_sources` the analysis may split on the final value of a bit: the
bit ends at 0, or it ends at 1. The two cases together cover every input, so
the largest case is still a bound for all of them.

A case may fix a source bit or an internal signal. What follows from it is
added (AND = 1 needs both inputs 1; XOR = 0 with one input known gives the
other), and a case that contradicts itself is dropped, because no input can
produce it. The search always works on the case with the largest bound and
stops when one complete input vector reaches that bound (then it is marked
`tight`) or when the budget runs out.

Stopping early is safe. The reported number is then the largest case not yet
excluded, which is higher than necessary, never lower.

## What is assumed

The report lists its premises. They are:

- Parts behave as their code does: one fixed delay, output a function of the
  inputs one delay earlier.
- The contracts of non-gate parts are right. Built-in contracts exist only for
  gates, the bit adapters, `Rewire`, constants and the compact storage parts.
  Anything else without a contract makes the report invalid.
- Start-up has passed.
- One clock, taken straight from a root input, rising edge only.
- **Only with `binary_sources`:** root inputs and clocked state end at 0 or 1.
  Earlier values and all internal wires are unrestricted. Without this option
  no case split is possible, because an X select keeps both sides of a choice
  live; the result is then refinements 1 and 2 only.

It bounds the latest change only. It says nothing about hold or the earliest
change, and it rejects combinational loops (latches built from gates), derived
clocks and falling-edge contracts instead of guessing.

## Options

| Option | Meaning |
| --- | --- |
| `binary_sources` | Allow the case search (see the premise above) |
| `sources` | `All`, `InputsOnly` (state holds) or `StateOnly` (inputs hold); separates input-to-output, clock-to-output and register-to-register delays |
| `fixed_inputs` | A root input that never changes, for example `RST = 0` |
| `final_inputs` | A root input that changes but is known to end at a value, for example one operation code |
| `input_arrival` | The tick of a root input's last change, if not 0 |
| `search_budget` | Timed evaluations per endpoint; more can only lower the bound |
| `search` | `Variables`, `Signals` or `Both` (default) |

## How it is checked

`tests/cpp/SettlingBoundTest.cpp` (CTest `SettlingBoundTest`):

- **Hand-built circuits** with a known answer: a false path through two
  selectors (10 ticks by path, 5 proven), constants, a bus that whole-pin
  analysis sees as a loop, clocked boundaries, and the rejections.
- **Every gate-level part in the catalog** (93 today). For each one the three
  numbers must be ordered (proven ≤ per-bit ≤ whole-pin path), and the real
  simulator is driven with random, complemented and still-switching input
  changes, four-state and binary. No output may ever change after its bound.
- **Small parts** (33 today, up to 12 input bits): every final vector is
  evaluated on its own. The search may not report less than the largest of
  them, and a `tight` result must equal it. Both search modes are checked.
- **A real false path**: the ALU's `EQ` output, below.

Run one part with details:

```bash
build-cleanup/tests/SettlingBoundTest --verbose --budget=500000 rv32i.alu32
```

## What it found

For most parts the longest path is already exact: 88 of 93 are proven `tight`
at the test's small budget, and the simulator reaches the bound.

| Part and output | Longest path | Proven | Seen in the simulator |
| --- | ---: | ---: | ---: |
| `rv32i.alu32` `EQ` | 33 | 30 (tight) | 30 |
| `rv32i.baseline.execute` `TERMINAL` | 47 | 46 (tight) | not targeted |
| `rv32i.baseline.execute` `OUT_EXECUTION_STATUS` | 44 | 43 (tight) | not targeted |
| `alu.width8` | 66 | 65 (tight) | 65 |
| `shift.barrel.width8` | 22 | 21 (tight) | 21 |
| `arithmetic.adder.width32` | 20 | 19 (tight) | 19 |
| `rv32i.prediction.predecode` | 32 | 30 (tight) | 28 |
| `rv32i.prediction.predictor` | 5 | 4 (tight) | 4 |

The `EQ` case shows what a false path is. The longest path runs through the
subtractor's carry chain into the zero detector. To matter, every other
difference bit must be 0. That makes every bit position "propagate", and then
the carry is already decided by the short carry-in path, so the long generate
path never decides anything. No input uses it. The proof needs about 250,000
evaluations (about 10 seconds).

## Limits

- The search has no clause learning. Some outputs stay open at any practical
  budget; their bound is then the path bound, which is still correct. The
  execute stage's `OUT_NEXT_PC` (46) is one: 1.5 million evaluations neither
  lowered it nor found an input that reaches it. The two execute rows above
  needed about 760,000 evaluations each.
- Clocked parts are analysed (launch, capture, setup) but are not yet driven
  against the simulator by the test.
- A structural `DFlipFlop` built from gates has no contract here, so circuits
  must use the compact storage parts, as the safe-clock analysis requires.
- Nothing uses these numbers yet. Behavioral delays and the experiment clock
  are unchanged.
