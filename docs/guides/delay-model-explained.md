# Delays in CircuitSim, explained from the start

Written 2026-10-13 for the code at `448e4e3`; current documentation and
results status revised 2026-10-14. This is a guided explanation.
The exact reference for the analysis is
[circuit-timing-analyzer.md](../reference/circuit-timing-analyzer.md); the dated evidence
is in [the delay record](../records/2026-10-12-behavioral-delays-and-memory-clock-edge.md).
Every number here is copied from those two files or from the source, and is
in simulator ticks, not in physical time.

Contents:

1. [The problem](#1-the-problem)
2. [The rule](#2-the-rule)
3. [The pieces that were built](#3-the-pieces-that-were-built)
4. [The timing engine](#4-the-timing-engine)
5. [Delay tables](#5-delay-tables)
6. [Parts that cannot be handled by following paths](#6-parts-that-cannot-be-handled-by-following-paths)
7. [Gate lists: delays that follow the data](#7-gate-lists-delays-that-follow-the-data)
8. [Values: X where the gates have X](#8-values-x-where-the-gates-have-x)
9. [The safe clock](#9-the-safe-clock)
10. [Memories that time themselves](#10-memories-that-time-themselves)
11. [Experiments and results](#11-experiments-and-results)
12. [Tests](#12-tests)
13. [What does not hold, and what is open](#13-what-does-not-hold-and-what-is-open)
14. [Commands](#14-commands)
15. [History in order](#15-history-in-order)

## 1. The problem

Every part in CircuitSim exists in up to two forms.

- **Gate-level (structural).** The part is built from gates and smaller parts.
  Each gate publishes its result a fixed number of ticks after an input
  changes. A 32-bit adder is a few hundred gates; a whole processor core with
  its memory is 80,000 to 120,000 gate nodes.
- **Compact (behavioral).** The part is one block of C++ that computes the
  same outputs directly. It runs thousands of cycles per second where the
  gates run tens.

Large programs can only be run on compact parts. But the experiments in
chapter 4 report *time*: cycles multiplied by a clock period. So two questions
decide whether those numbers mean anything:

1. **How long does a compact part take to answer?** Before this work, each
   compact part had one typed-in delay, usually "one abstract tick". A compact
   32-bit adder answered in 1 tick where its gates need up to 19.
2. **How fast may the clock be?** The period must be long enough for the
   gate-level circuit to finish between two clock edges. The earlier answer
   was a longest-path estimate, which is neither a proof (it ignored loops and
   memory handshakes) nor tight (it counted paths no input can use).

A compact circuit that is faster than its gates gives results no real circuit
could give. A clock that is too short gives a circuit that silently captures
unfinished values. Both make an experiment meaningless.

## 2. The rule

The whole effort follows one rule:

> A compact part takes at least as long as its gates, in every situation, and
> not much longer if possible.

"At least as long" is the safety half: nothing built from compact parts may
ever be faster than the same thing built from gates. "Not much longer" is the
accuracy half: the delays are used directly as experiment values, so a compact
part that waits far longer than its gates distorts the result the other way.

Two more requirements came with it:

- **Same values.** The two forms must show the same output values, including
  the unknown value X.
- **Same cycles at a proven clock.** A whole system must run at a clock period
  that is *proven* on its gates, and at that period both forms must take the
  same number of cycles.

## 3. The pieces that were built

| Piece | Where | What it is for |
| --- | --- | --- |
| Timing engine | `src/analysis/TimingEngine.*`, `TimingGraph.*`, `TimingCircuit.*` | One evaluation that every timing question uses |
| `CircuitTimingAnalyzer` | `include/analysis/CircuitTimingAnalyzer.hpp` | The public entry: `settling`, `safeClock`, `characterize`, `delays` |
| `PartDelays` | `include/components/PartDelays.hpp` | A delay table: latest and earliest delay per input-output pair, clock-to-output, setup |
| Table generator | `tools/timing/PartDelayTables.cpp` (`part_delay_tables`) | Characterizes supported compact parts from their structural forms and writes the tables to a file |
| Stored tables | `src/components/selection/ProvenPartDelays.generated.inc` | Generated tables attached by the catalog when the implementation and parameters match |
| `provenPartDelays` | `src/components/selection/ProvenPartDelays.cpp` | Finds the table for a part and its parameters; extends memory tables to sizes too large to build |
| Simulator support | `BasicComponent::setDelays`, `evaluateAt`, `publishTime`, `setupViolations` | Selects table arcs, optionally uses a delay twin for current values, keeps pending outputs ordered, and notices late inputs |
| Gate list (`DelayTwin`) | `src/analysis/DelayTwin.cpp`, `NoGateList.generated.inc` | A compact part's own gates as a flat list, used only to time it from the actual data |
| Flip-flop and SRAM cell contracts | `include/modules/basic/DFlipFlopTiming.hpp`, `include/modules/memory/SRAMCellTiming.hpp` | Measured timing of the two latch loops that path analysis cannot handle |
| Primitive descriptions | `memory_device::Primitive::respondsOnlyToInputs`, `reaches`, `settled`, `BasicComponent::primitiveTiming` | Lets non-gate memory leaves tell the analysis how they behave |
| Shared four-state rules | `registerStep`, `rv32i::register_file::step`, `incrementBy4`, `ResponseBuffer` | One function per rule, used by the compact parts so they show X where gates do |
| Safe clock | `src/analysis/SafeClockTiming.cpp` | Proves a clock period: setup, hold, pulse width |
| Handshake states | `src/analysis/MemoryTimingState.*` | Lists every reachable state of a memory handshake, so the clock proof can go through them one by one |
| Clock-edge rule | `NativeSequence` in `src/modules/memory/MemoryEndpointEngine.cpp` | Timed memories speak to the core only at clock edges |
| Experiment runner | `tools/rv32i-performance/RV32IExperimentRunner.cpp` (`rv32i_experiment`) | `--safe-clock`, `--period auto`, `setup_violations`, `--check-handshake-states` |
| Measurement tool | `tools/timing/DelayEstimateExperiment.cpp` (`delay_estimate_experiment`) | Compares proven delays with what the simulator really shows |
| Tests | `tests/cpp/*Timing*`, `SettlingBoundTest`, `DelayRuleTest`, `PartDelayTables*` | See [section 12](#12-tests) |

## 4. The timing engine

### 4.1 One fact per bit

The circuit is turned into a graph with one node per pin bit
(`TimingGraph`). Each bit carries one fact:

> from tick `at` on, the value lies in this set of {0, 1, X, Z}, and it does
> not change after tick `settled`.

Facts start with no information ("any value, never settled") and are only
ever tightened, by rules that follow from how parts really behave:

- A primitive gate publishes `f(inputs at t)` after its fixed delay. A
  catalog-backed compact part uses its path table and may use its delay twin
  to time the current values; the analysis uses the table as its contract.
- A gate's possible output values are the image of its inputs' possible
  values. If only one value is possible, the bit is settled.
- A **controlling input** decides the output alone: a 0 into an AND gate, a 1
  into an OR gate. The output is then settled as soon as that one input is,
  without waiting for the other. X and Z never control, exactly as in the gate
  code.
- A bit that depends only on sources that never change never changes.

Tightening repeats until nothing moves. This is why **loops need no special
treatment**: a loop settles when some value on it gets decided, and otherwise
stays open, in which case the engine claims no bound rather than a wrong one.

### 4.2 False paths and cases

The longest path through the wiring is often a path no input can use. To
remove such paths the engine splits a bit known to be 0 or 1 into its two
cases and evaluates each. The cases together cover every input, so the largest
answer among them is still a bound for all inputs.

A case can fix a source bit or an internal signal. What follows from it is
added (an AND that is 1 needs both inputs 1; an XOR that is 0 with one input
known gives the other), and a case that contradicts itself is dropped, because
no input can produce it. Stopping the search early is safe: the number
reported is then the largest case not yet excluded.

The standard example is the ALU's `EQ` output. Its longest path runs through
the subtractor's carry chain into the zero detector: 33 ticks. For that path
to matter, every other difference bit must be 0. That makes every bit position
"propagate", and then the carry is already decided by the short carry-in path,
so the long path never decides anything. Proven bound: 30 ticks, and the
simulator reaches exactly 30. The proof takes about 250,000 evaluations.

| Part and output | Longest path | Proven | Seen in the simulator |
| --- | ---: | ---: | ---: |
| `rv32i.alu32` `EQ` | 33 | 30 | 30 |
| `rv32i.baseline.execute` `TERMINAL` | 47 | 46 | not targeted |
| `alu.width8` | 66 | 65 | 65 |
| `shift.barrel.width8` | 22 | 21 | 21 |
| `rv32i.prediction.predecode` | 31 | 30 | 28 |
| `rv32i.prediction.predictor` | 5 | 4 | 4 |

For most parts the longest path is already exact: 88 of 93 gate-level parts
are proven tight, and the simulator reaches the bound.

### 4.3 Three questions, one engine

| Question | Call | Answer |
| --- | --- | --- |
| Settling | `CircuitTimingAnalyzer::settling` | The last tick at which each output, and each clocked capture, can still change |
| Clock | `CircuitTimingAnalyzer::safeClock` | A period under which every setup, hold and pulse-width requirement holds, or "unknown" |
| Part delays | `characterize`, `delays` | The delays of one part, as a table per input-output pair |

Before this work there were three separate tools (a longest-path estimate
with a candidate clock, a safe-clock analysis with its own value engine, and a
settling analysis). They were merged into this one engine; the merge alone
left the certified periods unchanged (94, 58, 78), which was the check that
nothing was lost.

Every answer is an upper bound that holds for every input. None is found by
trying inputs.

## 5. Delay tables

### 5.1 What a table holds

A `PartDelays` table is the timing of one part as seen from outside:

- **Arcs.** For each input-output pair that can affect each other: the
  earliest and the latest delay.
- **Per control value.** Optionally a separate set of arcs for each value of
  one control input (for example per ALU operation).
- **Clocked parts.** Clock-to-output per output (`launches`), setup time per
  sampled input (`setup`), and the longest register-to-register path inside
  the part (`internal_period`).
- **Assumptions.** Inputs the table assumes are held (for example `RST = 0`),
  and where the numbers came from (`evidence`).

The table of the 32-bit adder, as generated:

```cpp
t.evidence = "proven on arithmetic.adder.width32.structural";
t.arcs = {
    Arc{"A", "Cout", {14, 19}},   // earliest 14, latest 19
    Arc{"A", "Sum", {3, 19}},
    Arc{"B", "Cout", {14, 19}},
    Arc{"B", "Sum", {3, 19}},
    Arc{"Cin", "Cout", {2, 2}},
    Arc{"Cin", "Sum", {1, 3}},
};
```

### 5.2 Who uses it

The same table is read by three users, which is what keeps them consistent:

- **The simulator.** A compact part selects applicable path delays for changed
  inputs and keeps older pending results in order. Its delay twin, when
  available and in agreement with the computed value, can publish earlier
  than the table's latest bound.
- **The analysis.** It reads the table as the part's contract, and uses the
  *earliest* delays for hold checks.
- **The generator.** `CircuitTimingAnalyzer::delays` produces the table from
  the gate-level form.

### 5.3 How the simulator applies it

(`BasicComponent::evaluateAt` and `publishTime`.)

- Every input pin that changes keeps the outputs it reaches busy for that
  pin's delay, whether or not the value computed at that moment differs. A
  later, faster input therefore cannot publish a result that a slower one is
  still shaping. A newer result never overtakes an older one.
- Only a rising clock edge counts as a clock change.
- An output restated at the value it already has is not a change.
- An output that changes with no listed path (a reset, a value the gates never
  show) waits the output's largest delay.
- **Setup watch.** At a rising clock edge, an input that changed less than its
  setup time before the edge is counted (`setupViolations()`). Experiments
  report this count, so a run at a clock that is too fast is not taken for a
  clean one.

### 5.4 Generating and storing them

`part_delay_tables` characterizes catalog parts that have both a gate-level
form and a single-part compact form, and writes their supported variants to
`ProvenPartDelays.generated.inc`. The catalog attaches a matching table when
it builds a compact part. Unsupported parameter combinations retain their
constructor delay; large-memory geometry extensions are estimates based on
smaller proven forms.

**172 of the 174 compact implementations run on a proven table.** The two
without one are the ROM cell and ROM row, whose outputs never change.

After changing any gate-level circuit, the tables must be regenerated. After
changing a storage part, they must be regenerated twice, because the clocked
parts are proven on top of the storage tables.

### 5.5 Choices made about the tables

- **Inputs may end at 0, 1, X or Z.** Assuming only 0 or 1 would remove a few
  more false paths (the ALU's `EQ` would be 30 instead of 33 in the table).
  But an X on a select line keeps both sides of a choice live, and a compact
  part must not be faster than its gates then. So tables are proven for all
  four values, and are a little more careful than strictly needed for clean
  inputs.
- **Per operation.** `rv32i.alu32`, `rv32i.alu32-core` and `alu.width8` carry
  one set of delays per operation code. While the operation is steady, a data
  input uses that operation's delays; a change of the operation itself costs
  the output's largest delay.
- **Per parameter.** A part built with a parameter finds a table only where
  one was proven for exactly that combination. The advanced core has a table
  for each of its five prediction policies and each predictor table size from
  1 to 256 entries. A memory with another row layout has its own table.
- **A ROM image does not matter.** It only sets constants behind the read
  column; the table is the same for every image.
- **Reset is treated as held at 0.** A reset change is timed at each output's
  worst case, so a harness must leave one whole cycle between releasing reset
  and the first working edge.

### 5.6 Memories too large to build

A gate-level memory is limited to 4,096 cells, so a 65,536-word memory has no
gates to prove anything on. The method:

- Every memory device and endpoint is proven at 64 words and at 128 words.
- A memory built with another word count gets a table in which each delay
  keeps growing per doubling as it did between those two sizes.
- For the flip-flop memory this is 2 ticks per doubling on the read paths and
  nothing on the setups, and it was checked at every size from 2 to 128 words.

Such a table is an **estimate, not a proof**, and its evidence text says so:
"ESTIMATE for 65536 words: proven at 64 and 128 words, growth per doubling
continued".

## 6. Parts that cannot be handled by following paths

### 6.1 The flip-flop

A flip-flop built from gates is a loop of latches. What it shows after a clock
edge depends on what it held before, so following paths gives no bound. The
gate-level `DFlipFlop` is therefore measured by trying everything
(`DFlipFlopTimingTest`): every stored value, every data value including X and
Z, and every offset of a data change around the clock edge. The result is its
contract (`DFlipFlopTiming.hpp`):

| | Ticks |
| --- | ---: |
| Setup (data quiet before the edge) | 6 |
| Hold (data quiet after the edge) | 1 |
| Clock to output | 3 to 4 |
| Shortest clock high / low phase | 2 / 6 |

Setup is defined as the point from which the output is also on time. Data
arriving 2 to 5 ticks before the edge is still captured, but then shows up as
late as tick 8.

With that one part known, the ordinary generator handles everything built from
it: a `Register<32>` needs 9 ticks of setup and answers in 3 to 4; the
register file reads in 3 to 12 ticks and answers 14 to 15 after the clock.

### 6.2 The SRAM cell

The SRAM cell keeps its bit in a loop of two gates. `SRAMCellTimingTest` tries
both stored values, every old and new value of the four inputs in 0, 1, X and
Z, and a second change arriving while the first is still settling. Result
(`SRAMCellTiming.hpp`): outputs change 2 to 3 ticks after the last input
change. One change always settles; a second arriving 1 tick later can leave
the latch oscillating, so the contract requires changes at least 2 ticks
apart. The SRAM access control spaces its phases by at least 32.

### 6.3 Loops that are not loops

Two places look like loops in the wiring but are not, and were described
precisely instead of as "every input reaches every output":

- What the SRAM bit-line pair sends back to the cells is set from the write
  side only, never from the cells' own outputs.
- The DRAM cell's sample follows the word line only, and the sense column's
  restore signal follows precharge, sense and the word lines only.

### 6.4 Memory leaves that are not gates

Some memory leaves are C++ primitives, not gates (address control, read
column, output buffer, DRAM and NOR-flash cells). Each can now tell the
analysis what kind of thing it is:

| Declaration | Meaning | Examples |
| --- | --- | --- |
| `InputsOnly` (`respondsOnlyToInputs`) | Outputs change only one delay after an input change; it never wakes itself | ROM address control, read column, output buffer, SRAM bit-line pair, sense amplifier |
| `SelfTimed` | It also wakes itself later, on its own schedule | DRAM and NOR-flash cells and command controls, SRAM access control |
| `reaches(input, output)` | This input can never change that output | ROM address control: the enables do not enter the row and column numbers |
| `settled(...)` | The leaf is a plain function: gives its output values and which inputs decide each output at those values | The ROM's three leaves |

`reaches` alone tightened the ROM table from 14 ticks to 2 on the path from
output enable to data.

## 7. Gate lists: delays that follow the data

A table has one weakness: it is a worst case per pin. An easy input still
waits as long as the hardest one. An 8-bit ripple adder whose carry stops
after two positions waits as if it ran through all eight. Measured, the gates
of that adder took only 38% of the compact part's time on average.

The fix: a compact part also carries **its own gates as a flat list**
(`DelayTwin`, built once per kind of part from its gate-level form). It is
used only to time the part; the values still come from the compact code.

Whenever the part's inputs change, it makes one pass over the list with the
value each input bit has now and the tick each bit last changed, and reads off
when every output is final:

- a gate with an input at its deciding value (0 into AND, 1 into OR) is final
  when that input is, and does not wait for the other;
- any other gate is final one delay after its later input;
- a bit that never changed delays nothing.

So the time follows the data: a carry that stops after two positions costs two
positions. The result holds for **any earlier history of the inputs**, so
overlapping changes are covered, which the single-pin tables cannot promise.
The part can publish at the earlier of this time and the table's bound when
the twin agrees with its computed value. An older pending result can still
hold the publication time back, so a newer result does not overtake it.

Details:

- **Clocked parts** have one too. The list then also carries the part's
  flip-flops and each instance keeps what they hold. At a rising edge a
  flip-flop takes the value at its data input from before the edge (X if that
  input had not been quiet for the setup time), and its output counts as
  changed one clock-to-output delay later.
- **Safety check.** Before a time from the list is used, the value the list
  ends on is compared with the value the compact code computed. If they
  differ, the table applies. A reset in progress or an unknown clock gives no
  answer, and the table applies.
- **Non-gate leaves** inside the list are timed by their own delays, using
  `reaches` and `settled` from section 6.4.

A part gets **no** gate list if its gate-level form has a loop, a stored state
that is not a plain flip-flop, more than one clock, a part that times itself,
or more than 60,000 gate nodes. Those are listed with the reason in
`NoGateList.generated.inc` so that no run pays for building their gates just
to find that out:

- the three cores and the single-cycle system (82,765 to 121,327 nodes);
- 41 memory parts that time themselves or contain cells that do;
- the register file, by measurement: it gained nothing from its list, and
  because gate-level cores keep the register file compact, the large list made
  every such run 21% slower.

What gate lists gained (share of the compact part's time that the gates
actually take; 100% would be exact):

| Part | Before | After |
| --- | ---: | ---: |
| All parts, average | 90% | 95% |
| 8-bit ripple adder | 38% | 92% |
| `rv32i.prediction.path` | 44% | 98% |
| Decode stages | 91% | 99% |
| ROM device | 18% | 74% |

Switches for measuring: `CIRCUITSIM_NO_DELAY_TWIN=1` (off),
`CIRCUITSIM_NO_DELAY_TWIN_FOR=prefix` (off for some contracts),
`CIRCUITSIM_DELAY_TRACE=1` (print which parts got one and when its answer was
not used).

**Strict settling (optional).** With `CIRCUITSIM_STRICT_SETTLING=1` a part
with a gate list shows X on an output from the earliest tick it can move until
its gates are final. This copies the pulses gates make. It is off by default,
because an X on a line that never really pulses can stop a part that reads it,
and the clocks are proven with pulses included either way.

## 8. Values: X where the gates have X

Timing was not the only difference. After an X or Z on an input, compact parts
used to turn the unknown into a 0 or reset themselves, while gates carry X
exactly as far as the logic lets it go. A test over all parts found 56,096
output values that differed between the two forms after an X or Z, of which
54,000 were a clean 0 or 1 in the compact form only and 2,350 were opposite
values.

The method: **one function per rule, shared**, written as the gates compute
it in four-state logic, and called by every compact part that has that piece
inside.

| Shared rule | What it models |
| --- | --- |
| `registerStep` | A register's reset, enable and capture, bit by bit |
| `rv32i::register_file::step` | The register file's write and read |
| `incrementBy4` | The PC incrementer: an unknown bit spreads exactly as far as through its gates |
| `ResponseBuffer`, `both`, `either`, `no`, `fade` | The memory handshake gates and the one-entry response buffer |

Typical rules that had to be matched exactly:

- A register bit whose write enable is unknown stays 0 only if the held bit
  and the incoming bit are both 0. The gates choose with AND and OR, which
  cannot tell that two 1s agree, so two 1s give X.
- A register losing its reset to an unknown value: a 0 stays 0, anything else
  becomes X.
- A comparison of two words is done bit by bit: one known differing bit is
  enough to say "different", even if other bits are unknown.
- Field codes that decode never produces (branch type 7, a trap cause above 2)
  pass through unchanged, as in the gates, instead of being replaced by a
  default.

Compact parts changed this way: the fetch and decode stages, the
execution-status parts, the prediction path, `rv32i.memory-request`, the
memory controllers and endpoints, and the flip-flop memory device.

Result: differences fell from 56,096 to 16,396, and **none remain where only
the compact form shows a 0 or 1**. What remains is the compact five-stage
core, which keeps its state as plain numbers and shows X on every output until
a reset when its reset, enable or a handshake bit is not 0 or 1 at a clock
edge. So it never claims a value its gates do not have; it is only more
pessimistic than they are. With every input at 0 or 1 from the start, the two
forms never differ in value at all.

## 9. The safe clock

### 9.1 What is proven

`CircuitTimingAnalyzer::safeClock` answers with one of three statuses:
`safe-under-contracts` (with a period), `timing-violation`, or `unknown`. It
never guesses: if it cannot prove a period, it says "unknown" and why.

For a period to be certified, for every clocked capture in the circuit:

- **Setup:** the data is settled at least the setup time before the next
  rising edge, in every situation.
- **Hold:** the earliest possible change after an edge comes no sooner than
  the hold time. This uses the tables' earliest delays.
- **Pulse width:** both clock phases are at least as long as the flip-flop
  needs.

The premises are listed in every report: parts behave as their code does,
contracts of non-gate parts are right, start-up has passed, there is one clock
taken straight from a root input.

### 9.2 Memory handshakes make loops

A core and its memory endpoint depend on each other within one cycle: "request
ready" depends on "response ready", which depends on "done", which depends on
"request ready". As plain wiring this is a loop, and a loop gets no bound.

The method is to stop treating the endpoint as wiring and to **go through its
states**. An endpoint is, at any moment, in one of a small number of states
(for the flip-flop endpoint: empty, or holding a response). In each state the
loop is cut somewhere, because some signal is fixed. `MemoryTimingState`
enumerates the states that can really be reached together with the handshake
registers, hands each one to the engine as a covering case, and the clock must
hold in all of them.

### 9.3 The periods and how they moved

| Step | Single-cycle | Baseline | Advanced |
| --- | ---: | ---: | ---: |
| Storage with a typed-in delay | 94 | 58 | 78 |
| Storage on its proven tables (setup 6 to 9, clock-to-output 3 to 4) | 106 | 70 | 86 |
| Memory estimated at its real size (65,536 words) | 166 | 78 | 96 |
| Compact parts timed from their gates; engine and stage fixes | 166 | 80 | 98 |
| Compact core's setup needs at its memory pins included | 166 | 80 | 100 |
| Advanced core with SRAM, DRAM, ROM or NOR-flash | | | 102 |

The periods went **up**, not down. That is the point: the earlier numbers were
optimistic because storage and memory had typed-in delays far shorter than
their gates.

### 9.4 Why the advanced core needs 100 and not 98

The gates of the advanced core are certified at 98 with the flip-flop memory
(92 with a timed memory, see below). But experiments run the *compact* core,
and its table cannot tell handshake states apart: it asks for
`IMEM_REQ_READY` 19 ticks before the edge on the fetch port and 30 on the data
port, in every state. The gates need less in the states where that signal
changes late.

So a period that is safe for the gates can still make the compact core count a
setup violation. The experiment therefore adds the compact core's own setup
needs at its memory pins to the same analysis
(`ClockSpecification::also_captured`). The certified period then covers both
forms: 100 with the flip-flop memory, 102 with a timed memory.

### 9.5 Binary start-up scope

`--binary-startup` asks for a certificate under one more premise: every source
ends at 0 or 1 after start-up. In that scope the periods are the same (166,
80, 100). It is not available for timed memories.

## 10. Memories that time themselves

### 10.1 What went wrong

SRAM, DRAM, ROM and NOR-flash finish an access after a number of *ticks*, not
at a clock edge. Their controller used to pass the device's "done" signal on
the moment it came. Two faults followed, in the gates and in the compact form
alike:

- **Lost responses.** A "done" rising a few ticks before a clock edge was
  acted on by the controller (response counted as taken) while the core had
  not yet seen "response valid". The response was lost and the core waited
  forever. Seen with program 20 on SRAM at periods 108 and 110, and on DRAM at
  98 and 102.
- **Different captures.** The same late signal reached the core inside its
  setup time. Gates and compact core then captured different things: at
  period 100 on DRAM the gates finished and the compact core computed a wrong
  result; at 112 on SRAM the compact core finished and the gates hung.

No choice of delay table can repair this. It is a real design fault: a signal
from another time base entering a clocked circuit unsynchronised.

### 10.2 The clock-edge rule

The memory sequencer (`NativeSequence`) still times the device in ticks, but
it now **speaks to the transaction port only at a clock edge**. A finished
operation shows "done", its result, its status and "backend ready" at the next
edge, like any other clocked state. Everything the core sees from such a
memory changes just after a clock edge.

- Cost: one more cycle per access.
- A DRAM refresh starts only once the backend has been seen ready for one
  edge, so a waiting request is never overtaken by one refresh after another.

A first attempt fixed this at the transaction port instead (capturing "done"
and "ready" there). It was replaced: putting the rule in the sequencer keeps
the port unchanged and makes the sequencer's outward behaviour simple enough
to analyse.

### 10.3 Telling a self-timed inside from a clocked outside

`safeClock` refuses to certify a clock around a part that times itself, so
that a timed memory is never mistaken for a zero-wait one. After the
clock-edge rule that refusal is too strict for a whole endpoint, so the
analysis now checks what a self-timed part can actually reach:

- A self-timed part's contract names the pins that still keep to the clock
  (`clocked_pins`; for the sequencer, everything except its device side,
  `MEM_*`).
- When the graph is built, the analysis walks back from every root output and
  every pin captured by the clock. If it never meets an output that a
  self-timed part changes on its own time, the circuit keeps to its clock as
  seen from outside. This is recorded as `clocked_interface` in the report and
  in the part's delay table (270 endpoint tables carry it).
- A memory **endpoint** passes: the sequencer's own-time outputs reach only
  the device, and the device's outputs reach only the sequencer.
- A memory **controller alone** does not pass: its device pins are its
  outputs.

### 10.4 The handshake states of a timed endpoint

The loop of section 9.2 exists here too, and the state list is larger
(`sequencedStates` in `MemoryTimingState.cpp`):

- **The endpoint's state:** PENDING (a request is in work), the response
  buffer's FULL (both four-state), and what the sequencer shows: idle,
  working, or done. Because the sequencer speaks only at clock edges, these
  are fixed between two edges.
- **Device timing is left open.** How many cycles the device works is not
  assumed, and a DRAM refresh may start by itself, so every device timing is
  covered.
- **The requester's three registers** (busy, sent, abandoned) are followed
  through its actual gates, with the core's decisions (issue, take, tag
  mismatch) free over 0, 1, X and Z.

That gives 30 reachable states for SRAM, ROM and NOR-flash and 42 for DRAM.
Tried directly, every pair of fetch-port and data-port states had to be
settled (13 x 13, then 34 x 34 cases), which ran out of the analysis budget.

The idea that made it practical: **merge states by what the endpoint shows**.
In each state the endpoint shows `RSP_VALID` as one value and `REQ_READY` as a
function of `RSP_READY` alone. States that show the same thing are one timing
case, which carries every value the requester's registers take among them.
That is a superset of the reachable states, so nothing is left out. Result: 6
cases per SRAM, ROM or NOR-flash endpoint and 8 per DRAM endpoint; 36 and 64
pairs for a core with two ports; about 2 seconds for SRAM and 3 for DRAM.

The engine got a `Sequenced` part mode for this: in one case, `RSP_VALID` is a
constant from the clock edge on, data and status are settled at their launch
delay, and `REQ_READY` is either a constant or follows `RSP_READY` by the
table's delay. The loop closes only in states where `REQ_READY` lets
`RSP_READY` through, and there the requester's registers already decide the
rest.

### 10.5 Checking the state list against reality

An enumeration can be wrong by leaving a state out. So the list is checked
against simulation: `rv32i_experiment --fidelity reference
--check-handshake-states` runs the gate-level advanced core and fails if, in
any cycle, a memory port is in a state that is not in its list.

| Memory | Port states seen (program 20, `--period auto`) | Outside the list |
| --- | ---: | ---: |
| Flip-flop | 170 | 0 |
| SRAM | 800 | 0 |
| DRAM | 1,166 | 0 |
| ROM | 800 | 0 |
| NOR-flash | 668 | 0 |

## 11. Experiments and results

### 11.1 Proven delay against real settling time

`delay_estimate_experiment` prints, for each gate-level part and each output,
the proven worst case next to the distribution of real settling times over
random input changes; then the same per input pin and per control value. This
is how per-operation tables were justified and how loose tables were found.

With `--compare` it drives the compact and the gate-level form of every
catalog part with the same single-pin changes (clock and reset included, and X
and Z with `--four-state`) and counts how often the compact form's last output
change came *before* its gates'. For every part with a table that count is
zero.

### 11.2 The delay rule, part by part

`DelayRuleTest` runs every part as gates and as its compact form through the
same situations (one pin; X or Z; a few pins; all pins; edge patterns; two
pins a few ticks apart; clock edges) and compares them output by output.

Result at `34abf09`: **138 parts and sizes, 120,081 situations, about 140,000
output changes, 0 rule breaks.**

- A compact output never changes before the gates' output.
- A compact output using only its table changes at the applicable table
  delay; a delay twin can publish earlier within that bound when it agrees
  with the compact result and does not overtake an older pending output.
- The gates stay within the table, also when several pins change at once or a
  few ticks apart, and also for gate-level parts sitting inside larger parts.
- On average the gates take 95% of the compact part's time.

24 parts are below the 70% target and are recorded in the test as exceptions,
each with its measured share and the reason:

| Parts | Share | Reason |
| --- | ---: | --- |
| Single-cycle and baseline cores as whole parts | 54%, 59% | Too large for a gate list |
| DRAM and NOR-flash banks | 56% to 69% | Their cells are not gates |
| 16 SRAM, ROM and NOR-flash controllers and endpoints | 57% to 67% | They time themselves, so no gate list; after a clock edge they answer at the latest time their table allows |

At a clock these parts are exact: same period, same cycles.

### 11.3 Chained parts

Two chained 32-bit adders settle in 37 ticks as gates and 38 with compact
adders: the compact circuit is never faster, and barely slower.

### 11.4 Whole systems at different clocks (timed memories)

After the clock-edge rule, advanced core, program 20, cycles to finish:

| Memory | Period 100 | 108 | 120 | 166 |
| --- | ---: | ---: | ---: | ---: |
| SRAM, ROM | 400 | 334 | 334 | 268 |
| NOR-flash | 334 | 334 | 334 | 268 |
| DRAM | 588 | 564 | 537 | 442 |

A longer period means fewer cycles because the device needs a fixed number of
ticks. Gates and compact form gave the same cycle count in all 14 runs of each
form, each verified against the instruction oracle. The compact core saw no
setup violation from period 100 up (periods 100 to 1000 tried per memory) and
51 at period 98.

### 11.5 Whole systems at the certified clock

Advanced core, program 20, `--period auto`:

| Memory | Certified period | Cycles, gates | Cycles, compact | Setup violations | Verified |
| --- | ---: | ---: | ---: | ---: | --- |
| SRAM | 102 | 400 | 400 | 0 | yes |
| ROM | 102 | 400 | 400 | 0 | yes |
| NOR-flash | 102 | 334 | 334 | 0 | yes |
| DRAM | 102 | 583 | 583 | 0 | yes |

The prediction policies `taken` and `two-bit` with SRAM also certify 102.

Certified periods for all cores:

| Core | Memory | Period |
| --- | --- | ---: |
| Single-cycle | Flip-flop | 166 |
| Baseline pipeline | Flip-flop | 80 |
| Advanced pipeline | Flip-flop | 100 |
| Advanced pipeline | SRAM, DRAM, ROM, NOR-flash | 102 |

### 11.6 Building compact cores from compact children

One idea tested and dropped: build a compact core out of compact stages
instead of as one whole-core model, so that it could follow its gates more
closely. It gave the same cycle counts but ran 14 to 136 times slower. The
compact cores stay whole-core models.

## 12. Tests

| Test | What it checks |
| --- | --- |
| `SettlingBoundTest` | Hand-built circuits with known answers; every gate-level part in the catalog (93) driven with random, complemented and still-switching inputs, where no output may change after its bound; every final vector of the 33 small parts; the ALU `EQ` false path; compact parts side by side with their gates |
| `DFlipFlopTimingTest`, `SRAMCellTimingTest` | The measured contracts of the two latch loops, by trying every case |
| `PartDelayTablesTest`, `PartDelayTablesCompleteTest` | Every stored table regenerated from the gates and compared |
| `DelayRuleTest`, `DelayRuleStrictSettlingTest` | The delay rule part by part and for the cores; the strict-settling variant with pulses included |
| `SafeClockTimingTest`, `BinaryStartupTimingTest` | The clock question, hold, budgets, every case that must end in "unknown" |
| `MemoryStateTimingTest`, `MemoryFeedbackTimingTest`, `MemoryEndpointSafeClockTest` | Handshake states, feedback through the memory store path, endpoints |
| `RV32IExperimentCLITest` | Certificates as reported by the tool: SRAM safe clock is `safe-under-contracts` at 102 with all state cases checked; a run at `--period auto` has 0 setup violations; the handshake check for flip-flop, SRAM and DRAM |
| `RV32IBinaryClockCLITest` | The binary start-up certificates (166, 80, 100) |

Full native suite at `448e4e3`, after a clean rebuild, one test at a time: 451
of 451 passed.

## 13. What does not hold, and what is open

Stated plainly, because the delays are used as experiment values.

- **"Not much longer" does not hold in every single situation** for parts
  without a gate list (the 24 exceptions above). They are never faster than
  their gates, and at a clock they are exact.
- **Pulses.** Gates can leave a value and come back to it; a compact part
  computes the final value once and stays still. So a parent built from
  compact parts can go quiet sooner than the same parent in gates. The
  strict-settling option removes this for parts with a gate list. It does not
  reach a clocked experiment, because the clock is proven on the gates with
  pulses included.
- **Several pins at once is tested, not proven, for parts on a table only.** A
  table is proven for one pin changing while the others hold. The test looks
  for slow cases on purpose and found none, but no proof says none exist. Gate
  lists have no such limit.
- **The compact five-stage core is coarse about X** (section 8): about 16,000
  output values in the test are X in the compact form where the gates still
  know the value. Data with unknown bits read from a memory is still treated
  as 0 by the compact cores.
- **Large memories are estimates** (section 5.6).
- **Self-timed parts are compared with their gates only**, not with their
  tables, and only until they start working on their own. Their system-level
  check is the cycle count.
- **The timed endpoint is analysed through its table, not its gates**, and the
  state model needs the endpoint wired to the core's requester
  (`RV32IMemoryRequest`).
- **No binary start-up certificate for timed memories.**
- **The search has no clause learning.** Some outputs stay open at any
  practical budget; their bound is then the longest path, which is still
  correct (the execute stage's `OUT_NEXT_PC`, 46, is one). The clock question
  splits cases only to close loops, not yet to lower a period.
- **Result coverage remains bounded.** The current [§4.7 report](../../site-docs/rv32i/experiments.md)
  records completed panels at the final periods and distinguishes the full
  Embench clock-only projection from measured current-source results. Other
  core, memory and policy combinations need their own checks.
- **Documentation coverage.** The public component references were revised
  to describe catalog delay tables, optional delay twins and the constructor
  fallback; see [the documentation audit](../records/2026-10-13-documentation-audit.md).

## 14. Commands

```bash
# Regenerate and check the tables
build-cleanup/part_delay_tables --write src/components/selection/ProvenPartDelays.generated.inc --clocked
prlimit --as=4294967296 cmake --build build-cleanup -j1   # compile the new tables
build-cleanup/part_delay_tables --verify src/components/selection/ProvenPartDelays.generated.inc
build-cleanup/part_delay_tables --check        # shorter check of compiled default tables
build-cleanup/part_delay_tables --coverage     # which compact parts have no table, and why
build-cleanup/part_delay_tables --gate-lists src/components/selection/NoGateList.generated.inc   # parts without a gate list

# Measure
build-cleanup/delay_estimate_experiment rv32i.alu32
build-cleanup/delay_estimate_experiment --compare --four-state
build-cleanup/tests/DelayRuleTest --report rv32i.alu32
build-cleanup/tests/SettlingBoundTest --verbose --budget=500000 rv32i.alu32

# Clocks
build-cleanup/rv32i_experiment --core advanced --safe-clock
build-cleanup/rv32i_experiment --core advanced --memory sram --safe-clock
build-cleanup/rv32i_experiment --core advanced --memory dram --program 20 --period auto
build-cleanup/rv32i_experiment --core advanced --memory sram --program 20 --period auto \
    --fidelity reference --check-handshake-states
```

## 15. History in order

| Commits | Step |
| --- | --- |
| `3a14511` | A proven settling bound per output, with false paths removed |
| `83f6ba5` to `8b3e46d` | One shared view of parts and contracts; the longest-path estimate retired; one public entry point; one engine for settling, the clock and part delays |
| `74bf765`, `823c4a9`, `8f29132` | Delay tables introduced; tables for 44 unclocked parts, then for the clocked parts: 63 in all |
| `8b03761` | Storage and flip-flop memory on proven delays; clocks recomputed (106, 70, 86) |
| `c908910` to `8661733` | Tables for the memory families one by one, until every compact part has one |
| `299234a` | Memory delays estimated at real size; clocks 166, 78, 96 |
| `a770f4e`, `162af57`, `ef98149` | Tables regenerated in one pass; per-parameter tables; inputs ending at 0, 1, X or Z |
| `b6ebe83` | The delay rule test; compact parts timed from their gates (gate lists); clocks 166, 80, 98 |
| `c475e19`, `3cfd0fd` | Gate lists for clocked parts, non-gate leaves, the decode stages and ROM leaves; strict-settling option |
| `c93be7c` | Compact parts leave X where their gates do; setup violations reported by experiments |
| `42d11d3`, `41cdf18` | The memory clock-edge rule (first at the port, then in the sequencer); self-timed inside told from a clocked interface |
| `34abf09` | Self-timed controllers and endpoints recorded as delay-rule exceptions |
| `509a601` | Safe clock certified for cores with SRAM, DRAM, ROM and NOR-flash (102) |
| `448e4e3` | The advanced clock covers the compact core for every memory (100); handshake states checked against simulation |
