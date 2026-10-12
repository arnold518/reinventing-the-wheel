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

172 of the 174 compact implementations run on a table (`part_delay_tables
--coverage` lists every one that does not, with the reason).

- Unclocked logic: the adders, muxes, decoders, shifters, the composite logic
  units, the ALU and its units, the 8-bit ALU parts, decode control,
  forwarding, hazard detection, the predictor parts and the unclocked stages.
- Clocked parts: the three cores, the fetch and decode stages, the pipeline
  registers, the memory requester, execution status and the prediction path.
- Storage: every register width, the state bit, the flip-flop cell and the
  register file.
- Memory: flip-flop words, banks, devices, controllers and endpoints; ROM
  banks, devices and endpoints; the cells, rows, banks, devices and endpoints
  of SRAM, DRAM and NOR flash; the controllers of every technology; the
  scalable form of each memory; the single-cycle system.

The two without a table are the ROM cell and row, whose outputs never change.

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
106 (single-cycle), 70 (baseline) and 86 (advanced) ticks, before the memory
sizes below were taken into account. After changing a
storage part, regenerate the tables twice: the clocked parts are proven on
top of the storage tables.

A table treats reset as held at 0, so a reset change is timed at each output's
worst case. A harness must therefore leave a whole cycle between releasing
reset and the first working edge.

### Memories larger than can be built

A gate-level memory is limited to 4,096 cells, so a 65,536-word memory has no
gates to prove anything on. Every memory device and endpoint that can be
analysed is therefore also proven at 64 and 128 words, and a memory built with
a word count gets a table in which each delay keeps growing per doubling as it
did between those two sizes (`provenPartDelays`). For the flip-flop memory
that is 2 ticks per doubling on the read paths and nothing on the setups, at
every size from 2 to 128 words.

Such a table is an estimate, not a proof, and says so in its evidence:
"ESTIMATE for 65536 words: proven at 64 and 128 words, growth per doubling
continued". The scalable form of a memory uses the same tables. The flip-flop
endpoint whose consumer is always ready (the fixed-cycle cores) is a different
circuit and is proven as its own variant. A ROM with a specific image, and a
memory with a non-default row layout, get no table.

The memory endpoint keeps its empty/holding behaviour in the analysis and
takes its delays path by path from its table.

With the cores' real memory sizes the certified periods are 166
(single-cycle), 80 (baseline) and 98 (advanced) ticks.

### How the analysis reaches memory parts

- A part may pass its clock on unchanged (a contract's `clock_outputs`, or a
  zero-delay `Rewire`). The memory controller does this for its device.
- A memory primitive that never wakes itself for a later timed step
  (`memory_device::Primitive::respondsOnlyToInputs`) is described as "every
  input reaches every output in one delay", which is sound whatever it stores.
  Any primitive that is not made of gates can say so with
  `BasicComponent::primitiveTiming`: `InputsOnly`, or `SelfTimed` if it also
  wakes itself (the DRAM and NOR-flash cells and command controls, the SRAM
  access control).
- The memory sequencer is clocked and also wakes itself between clock edges
  to step through device phases (`selfTimedClockedPart`). Its contract is
  marked `self_timed`: its launch delays count from whatever caused the
  change. The delays of the logic around it can then be proven, which is what
  a compact controller needs. `safeClock` refuses to certify a period around a
  self-timed part and says so, so a timed memory is never taken for a
  zero-wait one.

### SRAM and DRAM

The gate-level SRAM cell keeps its bit in a latch, a loop of two gates.
`SRAMCellTimingTest` tries every case: both stored values, every old and new
value of its four inputs in 0, 1, X and Z, and a second change arriving while
the first is still settling. Its outputs change 2 to 3 ticks after the last
input change (`include/modules/memory/SRAMCellTiming.hpp`). One change always
settles; a second one arriving 1 tick later can leave the latch oscillating,
so the contract holds for input changes at least 2 ticks apart. The SRAM
access control spaces its phases by at least 32.

Two apparent loops are not loops, and are described precisely instead of as
"every input reaches every output":

- What the SRAM bit-line pair sends back to the cells is set from the write
  side only, never from the cells' own outputs.
- The DRAM cell's sample follows the word line only, and the sense column's
  restore signal follows precharge, sense and the word lines only.

### Inputs that end at X or Z

The tables are proven for inputs that end at any of 0, 1, X or Z. Assuming 0
or 1 would remove a few false paths (the ALU's `EQ` is 30 instead of 33 that
way), but an X on a select line keeps both sides of a choice live, and a
compact part must not be faster than its gates then. The case search remains
available to `settling` through `binary_sources`; the tables do not use it.
`delay_estimate_experiment --compare --four-state` drives X and Z as well.
The flip-flop is measured with X and Z data too.

### Parts built with parameters

Tables are stored for a part's default parameters. A part built with any
parameter finds a table only where one was proven for that exact combination
(`variants` in the generator), or through the word-count estimate above. The
advanced core is proven for each of its five prediction policies at 16
entries, which is how the experiments and program tests build it.

### Parts built with other parameters, and delays per operation

- A memory built with another row layout has its own table (`@rows=K`).
- The advanced core has a table for each prediction policy and each
  predictor table size from 1 to 256 entries.
- `rv32i.alu32`, `rv32i.alu32-core` and `alu.width8` carry one set of delays
  per operation code (`control = "OP"`, `arcs_by_control`). While the
  operation is steady, a data input uses the delays of that operation. A
  change of the operation itself costs the output's largest delay.
- A ROM image does not matter: it only sets constants behind the read
  column, and the proven table is the same for every image.

### How the engine uses a table

`BasicComponent::evaluateAt` and `publishTime`:

- Every input pin that changes keeps the outputs it reaches busy for that
  pin's delay, whether or not the value computed at that moment differs. A
  later, faster input cannot publish a result that a slower one is still
  shaping.
- Only a rising clock edge counts as a clock change.
- An output restated at the value it already has is not a change.
- An output that changes with no listed path (reset, a value the gates
  never show) waits the output's largest delay.
- Setup watch: at a rising edge with the fixed inputs at their values, an
  input that changed less than its setup time before the edge is counted
  (`BasicComponent::setupViolations()`).

### Delays that follow the data

A compact part also carries its own gates as a flat list (a
`DelayTwin`, built once per kind of part from its gate-level form by
`buildDelayTwin`, attached by the catalog). Whenever its inputs change it
makes one pass over that list with the value each input bit has now and
the tick each bit last changed, and reads off when every output is final:

- a gate with an input at its deciding value (0 into AND, 1 into OR) is
  final when that input is, and does not wait for the other input;
- any other gate is final one delay after its later input;
- a bit that never changed delays nothing.

So the time follows the data: a carry that stops after two positions costs
two positions. The result holds for any earlier history of the inputs, so
changes that overlap are covered without the single-pin assumption of the
tables. The part publishes at the earlier of this time and the table's.

Parts with a clock have one too. The list then also carries the part's
flip-flops, and each instance keeps what they hold: at a rising clock edge a
flip-flop takes the value that stood at its data input before the edge (X if
it had not been quiet for the setup time), and its output counts as changed
one clock-to-output delay later. A reset in progress or an unknown clock
gives no answer, and the table applies. Before a time from the list is
used, the value the list ends on is compared with the value the part
computed; if they differ the table applies.

A leaf inside the gate-level form that is not a gate (a memory's address
control, read column or output buffer) is timed by its own delays: its
output follows each input that reaches it by that leaf's delay. A memory
leaf can say which input reaches which output (`Primitive::reaches`), which
also tightens the part's table (ROM output enable to data: 14 ticks to 2).
A leaf that is a plain function of its inputs can also describe itself
(`Primitive::settled`: its output values, and the inputs that decide each
output at those values; its own `evaluate` publishes exactly that). The
list then knows the leaf's values, and an output bit does not wait for
input bits that cannot change it: the latest input bits are taken first and
every value of them is tried (up to six bits). The ROM's three leaves do
this. A leaf that does not describe itself has its value left open, which
is safe but coarser.

A part gets no gate list if its gate-level form has a loop, a stored state
that is not a plain flip-flop, more than one clock, a part that times
itself, or more than 60,000 gate nodes. Those parts are listed, with the
reason, in `NoGateList.generated.inc` (written by `part_delay_tables
--gate-lists`, checked by `--verify`), so that no run pays for building
their gates only to find that out: the three cores and the single-cycle
system (82,765 to 121,327 nodes) and 41 memory parts that time themselves
or contain cells that do (DRAM, NOR-flash, the SRAM device and what is
built on them). The register file is listed too, by measurement: on its
table it has the same shares as with its gate list, and because gate-level
cores keep the register file compact, its large list made every such run
21% slower. Every other compact part has one, the decode stages included.
A falling clock edge on its own costs no pass.
`CIRCUITSIM_NO_DELAY_TWIN=1` turns gate lists off,
`CIRCUITSIM_NO_DELAY_TWIN_FOR=prefix` turns them off for the contracts
starting with that prefix (to measure what one costs), and
`CIRCUITSIM_DELAY_TRACE=1` prints which parts got one and each time its
answer was not used.

### Showing X while an output may still move (option)

With `CIRCUITSIM_STRICT_SETTLING=1` a part with a gate list shows X on an
output from the earliest tick the table says it can move until the tick
its gates are final, and then the value. This copies the pulses gates
make. It is off by default: X on a line that never pulses can stop a part
that reads it, and the clocks are proven with pulses included either way.
`DelayRuleStrictSettlingTest` runs the whole comparison with it on: a
compact output must then not look settled before its gates, pulses
included. The pulses it leaves unmatched are all in parts without a gate
list.

### The delay rule and how far it holds

The rule: a compact part takes at least as long as its gates, and not much
longer. `DelayRuleTest` runs every part as gates and as its compact form
through the same situations (one pin, X or Z, a few pins, all pins, edge
patterns, two pins a few ticks apart, clock edges) and compares them output
by output.

Holds, in every comparison made (138 parts and sizes, about 117,000 situations, 140,000 output changes):

- a compact output never changes before the gates' output;
- a compact output on a table changes at exactly the delay the table gives;
  one timed from its gates changes no later than that;
- the gates stay within the table, also when several pins change at once or
  a few ticks apart, and also for gate-level parts sitting inside larger parts;
- the three cores, compact, run at the clock proven on their gates (166, 80,
  98) with no setup violation, and take exactly the gates' cycle count for
  every program, memory technology and prediction policy compared.

Does not hold, and why:

- "Not much longer" in every single situation, for parts without a gate
  list. Over all parts the gates take 95% of the compact time on average
  (90% before gate lists; the 8-bit ripple adder went from 38% to 92%,
  `rv32i.prediction.path` from 44% to 98%, the decode stages from 91% to
  99%, the ROM device from 18% to 74%). Below 70%, and listed in the test
  with their measured shares: the single-cycle and baseline cores as whole
  parts (54% and 59%; too large for a gate list), and the DRAM and
  NOR-flash banks (56% to 69%; their cells are not gates), and the SRAM,
  ROM and NOR-flash controllers and endpoints (57% to 67%: they time
  themselves, so they have no gate list, and after a clock edge they answer
  at the latest time their table allows, a few ticks after their gates).
  At a clock they are exact: same period, same cycles.
- Pulses. Gates can leave a value and come back to it; a compact part
  computes the final value once and stays still, so a parent built from
  compact parts can go quiet sooner than the same parent in gates. The
  strict-settling option above removes this for parts with a gate list.
- Several pins at once is tested, not proven, for parts on a table only. A
  table is proven for one pin changing while the others hold. A gate list
  has no such limit: its time holds for any history of the inputs. For the
  parts on tables the test also looks for slow cases on purpose (the pin
  with the largest delay to an output, changed alone from many starting
  values); no case was found where the gates exceed the table, but no
  proof says they cannot.
- Values after an X or Z, for the five-stage core only. With every input at
  0 or 1 from the start, the two forms never differ in value. After an X or
  Z on an input, every compact part but one now leaves X exactly where its
  gates do: the fetch and decode stages, the execution-status parts, the
  prediction path, `rv32i.memory-request`, the memory controllers and
  endpoints and the flip-flop memory device evaluate their registers and
  handshake gates by the same four-state rules as the gates (shared
  functions: `registerStep`, `rv32i::register_file::step`, `incrementBy4`,
  `ResponseBuffer`). The compact five-stage core keeps its state as plain
  numbers; when its reset, or its enable or a memory handshake bit at a
  clock edge, is not 0 or 1, it shows X on every output until a reset. So
  it never shows 0 or 1 where its gates hold X, but it shows X on outputs
  its gates still know (about 16,000 output values in the test, counted
  there by direction; before this, 54,000 values were 0/1 in the compact
  form only and 2,350 were opposite). Data with unknown bits read from a
  memory is still treated as 0 by the compact cores.
- Parts that time themselves (DRAM, NOR-flash, the SRAM device and their
  controllers and endpoints) are compared with their gates only, not with
  their tables, and only until they start working on their own. Their
  system-level check is the cycle count below.

### Memories that time themselves

SRAM, DRAM, ROM and NOR-flash finish an access after a number of ticks, not
at a clock edge. Their controller used the "done" signal as it came. Two
things went wrong, in the gates and in the compact form alike:

- A "done" rising a few ticks before a clock edge was acted on by the
  controller (response counted as taken) while the core had not yet seen
  "response valid". The response was lost and the core waited forever
  (program 20 on SRAM at periods 108 and 110, on DRAM at 98 and 102).
- The same late signal reached the core inside its setup time. The gates
  and the compact core then captured different things: at period 100 on
  DRAM the gates finished and the compact core computed a wrong result; at
  112 on SRAM the compact core finished and the gates hung.

The memory sequencer still times the device in ticks, but it now speaks
to the transaction port only at a clock edge: a finished operation shows
"done", its result and "backend ready" at the next edge (`NativeSequence`).
Everything the core sees from such a memory changes just after a clock
edge. The cost is one cycle per access. A DRAM refresh starts only once the
backend has been seen ready for an edge, so a waiting request is never
overtaken by one refresh after another.

Measured afterwards, advanced core, program 20:

| memory | period 100 | 108 | 120 | 166 |
|---|---|---|---|---|
| SRAM, ROM | 400 | 334 | 334 | 268 |
| NOR-flash | 334 | 334 | 334 | 268 |
| DRAM | 588 | 564 | 537 | 442 |

Gates and compact form give the same cycles, both verified against the
instruction oracle, in every comparison made (14 runs of each form over
the four memories, periods 100 to 166), and the compact core sees no setup
violation from period 100 up (periods 100 to 1000 tried per memory). At 98,
the core's own certified clock, it sees 51: these memories answer "request
ready" a little later after the edge than the flip-flop memory does.

Because of that rule the analysis can certify a clock for these systems.
A self-timed part's contract names the pins that still keep to the clock
(`clocked_pins`; for the sequencer everything but its device side,
`MEM_*`). When the graph is built, the analysis walks back from every root
output and every pin captured by the clock; if it never meets an output
that a self-timed part changes on its own time, the circuit keeps to its
clock as seen from outside (`clocked_interface` in the report and in the
part's delay table), and it is treated as an ordinary clocked part. A
memory endpoint passes: the sequencer's own-time outputs reach only the
memory device, and the device's outputs only the sequencer. A memory
controller alone does not: its device pins are its outputs.

That removes the first of two obstacles to certifying a clock for a core
with such a memory. The second remains: the core's requester and the
endpoint depend on each other within a cycle ("request ready" depends on
"response ready", which depends on "done", which depends on "request
ready"). For the flip-flop endpoint the analysis resolves this by going
through every reachable state of the handshake
(`MemoryTimingState.cpp`); that model does not cover an endpoint with a
sequencer yet, so the result for these systems is still "unknown"
(combinational dependency cycle), and `--period auto` refuses them and
asks for an explicit period.

An experiment reports `setup_violations` in its result and warns on stderr
when a compact part saw an input change inside its setup time, so a run at
a clock that is too fast is not taken for a clean one.

None of the first three reaches a clocked experiment. The clock is proven on the
gates of the whole system, pulses and simultaneous changes included, and a
run at that clock gives the same cycles in both forms.

## What is assumed

Every report lists its premises.

- Parts behave as their code does: output a function of the inputs, one
  fixed delay later for a gate, the table's delay for a compact part.
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
  They are 166, 80 and 98 with storage and memory on their real delays.

- `PartDelayTablesCompleteTest`: every stored table regenerated from the
  gates and compared.
- `DelayRuleTest`: the delay rule above, part by part and for the cores.
  `--report` prints without failing on the targets; `--floors` prints the
  measured shares of the parts below them.

```bash
build-cleanup/tests/DelayRuleTest --report rv32i.alu32
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
