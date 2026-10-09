# CircuitTimingAnalyzer

One timing analysis for built circuits (`include/analysis/CircuitTimingAnalyzer.hpp`).
It reads the component tree, uses one contract per indivisible part, and
answers three questions from the same model and the same engine.

| Question | Call | Answer |
| --- | --- | --- |
| Settling | `CircuitTimingAnalyzer::settling` | The last tick at which each output, and each clocked capture, can still change |
| Clock | `CircuitTimingAnalyzer::safeClock` | A period under which every setup, hold and pulse-width requirement is met, or "unknown" |
| Part delays | `CircuitTimingAnalyzer::characterize`, `CircuitTimingAnalyzer::delays` | The delays of one part: per output, or as a full table per input-output pair. This is what a compact form of the part must reproduce |

All results are simulator ticks, not physical delays. Every answer is an upper
bound that holds for every input; none is found by trying inputs.

It replaces three earlier tools: the longest-path estimate with its candidate
clock, the safe-clock analysis with its own value engine, and the separate
settling analysis.

## Source layout (`src/analysis/`)

| File | Holds |
| --- | --- |
| `TimingCircuit` | Which parts are indivisible, the built-in contract table (matched by C++ type), and which pin drives which |
| `TimingGraph` | The circuit as one node per pin bit. Loops are allowed. The compact FF memory endpoint is a part with its own rule |
| `TimingEngine` | The one evaluation, and the search over cases |
| `CircuitTimingAnalyzer` | The settling and part-delay questions |
| `SafeClockTiming` | The clock question: contract and boundary checks, earliest transitions for hold, the period |
| `MemoryTimingState` | The reachable states of a memory handshake, used as covering cases |
| `TimedLogicDomain.hpp` | The FF request validator over sets of values |

## The engine

Each bit carries a fact: from tick `at` on its value lies in a set of
{0, 1, X, Z}, and it does not change after tick `settled`. Facts start with no
information and are only tightened, by rules that follow from how parts
behave:

- A part publishes `f(inputs at t)` at exactly `t + delay`
  (`BasicComponent::_updateOutputWire`). So an output is settled one delay
  after all its inputs are.
- A gate's output set is the image of its input sets. One possible value
  means the bit is settled.
- A controlling input (0 on AND, 1 on OR) decides the output alone, as soon as
  it is itself decided. X and Z never control, exactly as in the gate code.
- A bit that depends only on unchanging sources never changes.

Tightening repeats until nothing moves. Loops need no special treatment: a
loop settles when some value on it is decided, and otherwise stays open, in
which case no bound is claimed.

## Cases

Some paths exist in the wiring but no input uses them. To remove them the
engine splits a bit that is known to be 0 or 1 into its two cases. The cases
together cover every input, so the largest is still a bound for all.

A case may fix a source bit or an internal signal. What follows from it is
added (AND = 1 needs both inputs 1; XOR = 0 with one input known gives the
other), and a case that contradicts itself is dropped, because no input can
produce it. Stopping the search early is safe: the number reported is then the
largest case not yet excluded.

A caller can also supply a set of covering cases. The clock question does
this for memory: each compact FF endpoint is either empty or holding a
response, and `MemoryTimingState` enumerates the reachable combinations with
the handshake registers. Each is settled and checked on its own.

## Delay tables for compact parts

A compact (behavioral) part may declare a `circuit::PartDelays` table
(`include/components/PartDelays.hpp`, `BasicComponent::setDelays`): a latest
and an earliest delay per input-output pair, clock-to-output and setup for
clocked parts, and optionally one set of pairs per value of a control input.

- **The simulator** publishes each output at the latest delay of the input
  that caused it. A newer result never overtakes an older one still on its
  way. A part without a table keeps its single constructor delay.
- **The analyzer** reads the same table as the part's contract, and uses the
  earliest delays for hold.
- **`CircuitTimingAnalyzer::delays`** generates the table from the gate-level
  form of the part.

The compact part then gives the same values as the gates, never changes
before the gates have finished, and never after its table allows. It does not
copy input-by-input timing (an easy input still waits for the worst case) or
glitches. A circuit built from compact parts is never faster than the same
circuit in gates. `SettlingBoundTest` checks all of this on `Adder32`; two
chained adders come to 37 ticks in gates and 38 with compact adders.

`delay_estimate_experiment` compares proven delays with real gate-level
settling times over random input changes, per output, per input pin and per
control value.

### Which parts have one

`tools/timing/PartDelayTables.cpp` (`part_delay_tables`) proves a table for
every catalog part that has a gate-level form and a single-part compact form,
and writes them to `src/components/selection/ProvenPartDelays.generated.inc`.
The catalog gives each compact part its table when it builds it, for the
default parameters only.

```bash
build-cleanup/part_delay_tables --write src/components/selection/ProvenPartDelays.generated.inc
build-cleanup/part_delay_tables --check     # recompute every stored table from the gates
```

95 compact implementations run on a proven table (`part_delay_tables
--coverage` lists every one that does not, with the reason).

- Unclocked logic: the adders, muxes, decoders, shifters, the composite logic
  units, the ALU and its units, the 8-bit ALU parts, decode control,
  forwarding, hazard detection, the predictor parts and the unclocked stages.
- Clocked parts: the three cores, the fetch and decode stages, the pipeline
  registers, the memory requester, execution status and the prediction path.
- Storage: every register width, the state bit, the flip-flop cell and the
  register file.
- Memory: flip-flop words, banks, devices, controllers and endpoints; ROM banks
  and devices.

Two checks run on them. `SettlingBoundTest` drives each unclocked part side by
side with its gates. `delay_estimate_experiment --compare` does the same for
every part in the catalog, clocked ones included, with single-pin input
changes: it reports how often a compact form's last output change came before
its gates'. For every part with a table that count is zero.

### Storage and the flip-flop

A flip-flop built from gates is a loop of latches, and what it shows after a
clock edge depends on what it held before. Path analysis cannot bound that.
The gate-level `DFlipFlop` is therefore characterized by trying everything
(`DFlipFlopTimingTest`): every stored value, data value and offset of a data
change around the edge. Its numbers are in
`include/modules/basic/DFlipFlopTiming.hpp` and are its contract:

| | Ticks |
| --- | ---: |
| Setup | 6 |
| Hold | 1 |
| Clock to output | 3 to 4 |
| Shortest clock high / low | 2 / 6 |

Setup is the point from which the output is also on time. Data arriving 2 to
5 ticks before the edge is still captured, but then shows up as late as
tick 8.

With that one part known, the ordinary generator handles everything built
from it. A `Register<32>` needs 9 ticks of setup and answers in 3 to 4; the
register file reads in 3 to 12 ticks and answers 14 to 15 after the clock.

Because every structural view of a core keeps storage compact, the storage
tables enter every clock: the certified periods rose from 94, 58 and 78 to
106 (single-cycle), 70 (baseline) and 86 (advanced) ticks. After changing a
storage part, regenerate the tables twice: the clocked parts are proven on
top of the storage tables.

A table treats reset as held at 0, so a reset change is timed at each output's
worst case. A harness must therefore leave a whole cycle between releasing
reset and the first working edge.

### How the analysis reaches memory parts

- A part may pass its clock on unchanged (a contract's `clock_outputs`, or a
  zero-delay `Rewire`). The memory controller does this for its device.
- A memory primitive that never wakes itself for a later timed step
  (`memory_device::Primitive::respondsOnlyToInputs`) is described as "every
  input reaches every output in one delay", which is sound whatever it stores.

### Not covered

Their compact forms are today earlier than their gates by the amount shown.

- Sequenced controllers and endpoints of SRAM, DRAM, ROM and NOR flash (5 to 6
  ticks): the sequencer wakes itself for timed steps.
- SRAM cells, rows, banks and devices (2 ticks at the cell): the cell sits in a
  loop with its bit lines. DRAM and NOR-flash cells and devices: timed
  primitives without a contract.
- The single-cycle system (2 ticks) and every scalable device or endpoint: a
  scalable memory has no gate-level form at its size.
- Parts built with non-default parameters, and tables per control value.

## What is assumed

Every report lists its premises.

- Parts behave as their code does: one fixed delay, output a function of the
  inputs one delay earlier.
- The contracts of non-gate parts are right. Built-in contracts exist for
  gates, the bit adapters, `Rewire`, constants, the compact storage parts and
  the FF endpoint. Anything else without a contract gives no result.
- Start-up has passed.
- One clock, taken straight from a root input.
- Only with `binary_sources` (settling) or `binary_startup` (clock): changing
  sources end at 0 or 1. Without it, a case split is possible only where the
  circuit itself proves a bit binary.

## How it is checked

- `SettlingBoundTest`: hand-built circuits with known answers; every
  gate-level part in the catalog (93) driven in the real simulator with random,
  complemented and still-switching input changes, where no output may change
  after its bound; every final vector of the 33 small parts; the ALU `EQ`
  false path; part characterization.
- `SafeClockTimingTest`, `MemoryStateTimingTest`, `BinaryStartupTimingTest`,
  `MemoryFeedbackTimingTest`, `MemoryEndpointSafeClockTest`: the clock
  question, including feedback through the memory store path, hold, budgets
  and every fail-closed case. These were written for the earlier engine and
  pass unchanged on this one.
- The engine merge itself left the certified periods unchanged (94, 58, 78).
  They are 106, 70 and 86 since storage runs on its proven delays.

```bash
build-cleanup/tests/SettlingBoundTest --verbose --budget=500000 rv32i.alu32
build-cleanup/rv32i_experiment --core advanced --safe-clock
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
| `rv32i.prediction.predecode` | 31 | 30 (tight) | 28 |
| `rv32i.prediction.predictor` | 5 | 4 (tight) | 4 |

The `EQ` case shows what a false path is. The longest path runs through the
subtractor's carry chain into the zero detector. To matter, every other
difference bit must be 0. That makes every bit position "propagate", and then
the carry is already decided by the short carry-in path, so the long generate
path never decides anything. The proof needs about 250,000 evaluations.

## Limits

- The search has no clause learning. Some outputs stay open at any practical
  budget; their bound is then the path bound, which is still correct. The
  execute stage's `OUT_NEXT_PC` (46) is one.
- The clock question does not yet use the case search to lower a period; it
  splits cases only to close loops.
- A loop of latches gets no bound from path analysis. The gate-level
  `DFlipFlop` has a measured contract instead; any other latch loop does not.
- A compact part without a table still uses its typed-in single delay.
