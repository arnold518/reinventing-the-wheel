# Non-device improvement plan

Started: 2026-10-27. Status: open. Code state: `alpha` at `5549ee0` with the
uncommitted gate-cache, trace and constant-word changes in the working tree.

## Summary

One ordered plan for the work the owner selected on 2026-10-27, excluding the
Device I/O architecture (tracked in the
[device architecture review](2026-10-27-device-architecture-review.md)).
Order: test sweeps, then the `Logic` adapter fix, then word-wide gates with a
gate-symbol visual, then engine work. Done: phase A (test sweeps), B1
(`Logic` adapters), C (word gates and their visual), the zero-delay `unit`
splitter with Chapter 2 word gates, engine steps D1–D4 and D2 (run-time pin
width), the unused splits in the multiply/divide steps, the second `unit`
table pass (the first used stale storage tables), pooled events, the
regenerated example tables (161 groups), the WASM rebuild with parity checks
(16 of 16, all ten recorded guests exact) and the strict site build. Open, all
owner decisions: the published §4.8 results at the old `unit` periods and a
full native suite before committing. The stale game recordings wait for the
device I/O work (owner decision, 2026-10-28); see the dated sections below. It brings
together the [test strength review](2026-10-27-test-strength-review.md), the
[bit-adapter findings](2026-10-27-device-io-review.md) and the
[engine plan](2026-10-26-engine-performance.md).

## 2026-10-27 — Plan

### Phase A: test sweeps (first)

Safety net for every later refactor. Details and case counts are in the test
strength record.

- A1. One shared settled-sweep helper (`include/tests/SettledSweep.hpp`)
  replacing the four duplicated settle harnesses. References are independent
  arithmetic, never the production value functions.
- A2. `AdderWidthSweepTest`: ripple and prefix adders at 8/16/32 bits, carry
  chains at every bit position, 4,096 seeded random vectors, single-bit X/Z
  injection, against `a + b + C_IN` and the behavioral form. Extend
  `ALUWidthContractTest` at 16/32 bits (edge grid plus 512 random) and repoint
  the 16/32-bit owners. Add `RV32IALUSweepTest`.
- A3. Gate-level multiply/divide: strict transaction checks for Multiplier,
  Divider and MulDiv<32>; an exhaustive 4-bit M unit as the gate-level slice
  (labelled slow if over about 30 s).
- A4. Exhaustive RV32I forwarding (about 2,600 vectors) and hazard (about
  1,000) units against small independent rules; ROM cell/row parameter extras.

Checks: each new test is mutation-checked once (a deliberate local fault must
make it fail, then is reverted). New coverage is headless, so
`DocumentationExamplesTest` and the regenerated example groups stay
byte-identical. One serial full build and suite at the end of the phase.

### Phase B: `Logic` adapter fix (second)

Root cause and census are in the bit-adapter record: `StructuralLogic.hpp`
works on bit vectors, so `out()`, `drive()`, `add()` and `sub()` build
split/join round trips, and one-bit splits/joins still create Rewires.

- B1. Inside `Logic` only: one-bit split/join return the pin itself; split each
  source once; a join of a complete split in order returns the original word;
  add `passThrough` (one Rewire only where a parent input feeds a parent
  output); invert a mux select once. Skipped constructions still use up their
  `id()` numbers, so surviving child names stay stable.
- B2. Word-level `add`/`sub` through the existing `AddSub<W>` family; rewrite
  the MulDiv result groups (`MultiplyResultCorrection<32>`: 82 children to
  about 6–8).
- Scope: device recipes also use `Logic`. Recommended: device recipes keep the
  current construction (an explicit legacy option) until device phase P1, so
  device layouts and game recordings migrate once, together with P1.

Checks: B1 removes only zero-delay parts, so regenerated delay tables must be
identical (that is the acceptance test). Regenerate and migrate saved layouts
for changed types; native/WASM parity; RV32IM component and program scenarios
against the oracle; phase A sweeps; full suite.

### Phase C: word-wide gates and their visual

- C1. Families `logic.{and,nand,or,nor,xor,xnor,not}.<W>` with type names
  `ANDGate<W>` and so on, for existing pin widths. Structural form: W one-bit
  gates (its internal split/join is inherent). Behavioral form: one leaf using
  the proven delays of its gate form. Both inputs W bits; a scalar broadcast
  input is a separate decision.
- C2. Equivalence tests: exhaustive four-state at small widths; random plus
  staggered per-bit changes inside one gate delay at 32 bits, so per-bit
  timing (inertial cancellation) matches the structural form, not just the
  final value.
- C3. Visual: the canvas recognizes the base type (`ANDGate<32>` draws the AND
  symbol), adds a width badge and bus-weight pin strokes, and draws the same
  symbol at every fidelity. Opening a structural word gate shows its one-bit
  gates in an automatic column layout. Fidelity presets: `gate` structural,
  `compact` behavioral; `logic` and `visual` are an owner decision.
- C4. Adopt where the operation is truly word-wide: `LogicUnit` (word gates
  plus `Mux<8,W>`), the prefix adder's bit stage and final sum (the prefix
  levels keep their `CarryCombine` cells), the add/sub operand invert and
  equality comparisons.

Checks: regenerated delay tables (expected identical where the same gates
remain; `LogicUnit` may change if its select structure changes), layout
migration, Chapter 2 site pages describing internal structure, unchanged
pin-level example tables, strict site build, browser check of the new symbol.

### Phase D: engine (alongside, one heavy job at a time)

From the engine record's 2026-10-27 plan:

- D0. Evidence first: counters for no-op updates and duplicate evaluations, a
  Snake-prefix workload, a speed-versus-circuit-size sweep, and at least six
  ABBA timing blocks.
- D1. Exact cleanups: hoist `getenv` out of the output-publish path, cached pin
  handles in BitSplitter/BitJoiner/Rewire, no temporary vectors in timed
  evaluation, output-side pin caching, inline heap keys, `-O2` on hot files.
  Commit together with the existing gate cache.
- D2. Packed four-state values, then run-time pin/wire width: one `Pin`/`Wire`
  class with a packed value; `getInputPin<W>` kept as a checked typed handle;
  component templates, family IDs and type-name strings unchanged; widths
  become a per-family contract choice. Then remove the fixed width list.
- D3. Promote the incremental clocked-twin prototype.
- D4. Pooled events and typed fanout routes.
- D5. Event reduction and D6 indexed execution, after D0 and owner decisions.

Checks per step: exact trace and golden comparison, timing-table identity,
WASM parity, then the full suite at batch boundaries. Batch fixes before full
runs (each full build or suite takes about 40 minutes).

### Owner decisions

1. `Logic` fix scope: keep the legacy construction for device recipes until
   device P1 (recommended), or apply it everywhere now and recapture games.
2. Word-gate fidelity in the `logic` and `visual` presets.
3. Word-gate contract IDs/type names as above, widths offered, and whether a
   scalar broadcast input variant is wanted.
4. Engine: `perf` access, accepting event-count changes, simulator-owned
   index tables, lossless disk recovery (about 3.5 GiB free).
5. Commit the current mixed working tree as separate topic commits before
   phase A.

## 2026-10-27 — Progress: phase A1/A2 done

The shared sweep equipment, `AdderWidthSweepTest`, the 16/32-bit grid in
`ALUWidthContractTest` and `RV32IALUSweepTest` are implemented and pass, with
fault checks and a catalog ownership fix; details and limits are in the
[test strength record](2026-10-27-test-strength-review.md). Commits are held:
the working tree mixes about eight earlier topics plus 1.9 GB of untracked
gameplay demos and about 150 MB of other generated assets, which need an owner
decision (ignore, external storage or commit) before topic commits are made.
Next: A3 (gate-level multiply/divide), then A4.

## 2026-10-27 — Progress: phase A done, B1 done, B2 replaced

Phase A is complete (test strength record). B1 is implemented and verified;
details are in the [bit-adapter record](2026-10-27-device-io-review.md). B2 as
planned (`Logic::sub` through `AddSub<W>`) was rejected: `AddSub` adds a
one-tick `BitSplitter` and XOR operand path, changing the core's critical path
and every core timing table and recording. The remaining per-bit inverters
(`MultiplyResultCorrection<32>` has 64) move to the word NOT gate in phase C,
which keeps the same gates and zero-delay adapters. Next: phase C.

## 2026-10-27 — Phase C scope decision: where word gates are timing-neutral

Word gates are timing-neutral only where they replace gates that sit between
zero-delay adapters. The `Logic` recipes (M units, RV32IM core groups) use
zero-delay Rewires, so `NOTGate<W>` replaces their per-bit inverters with
identical delay tables (checked in all three profiles). The Chapter 2 recipes
(`LogicUnit`, the prefix adder's bit stage and final sum) are built with
`WordBuilder`, whose `BitSplitter` costs one tick in `unit` timing. Feeding a
word gate directly from the word input removes that tick, and the prefix
adder's propagate bits are needed again bit by bit after the word stage. Both
adoptions change published Chapter 2 timing and the core's critical path, so
they are deferred for an owner decision rather than done silently.

## 2026-10-27 — Phase C implemented: word gates, visual and M-unit adoption

- C1: `include/modules/basic/WordGate.hpp` / `src/modules/basic/WordGate.cpp`
  add 24 families `logic.{and,nand,or,nor,xor,not}.<W>`, W = 4/8/16/32, types
  `ANDGate<W>` … `NOTGate<W>`. Structural form: W one-bit gates between
  zero-delay Rewires (`A_BITS`/`B_BITS`/`IN_BITS`, `OUT_WORD`). Compact form:
  one leaf using the shared one-bit gate helpers, on the delays proven on its
  gates. Registered after the word muxes; inventory updated.
- Delay tables: `part_delay_tables --write --clocked` for all three profiles
  gives 833 entries each: exactly the 24 new word-gate tables added; all 809
  existing entries byte-identical in unit, embedded and desktop. This is also
  the complete embedded/desktop verification for B1 and the adoption below.
- C2: `WordGateSweepTest` (headless, about 1 s): 13,872 vectors on both forms
  against independent one-bit rules written in the test (exhaustive binary at
  4 bits, boundary patterns, 256 random binary and 256 dense four-state words
  per family), and per-bit timing: bits changing one tick apart, output wire
  sampled every tick, compact equal to gates in every timing profile. A
  deliberate fault (compact NOR computing OR) failed it; reverted. The first
  timing draft sampled the output pin, which a basic part sets at evaluation;
  it now samples the wire, as the delay reaches it.
  Known limit, shared with every compact part: transport delay, so a pulse
  shorter than one gate delay can pass where inertial one-bit gates swallow it.
- C3: the canvas draws word gates with the one-bit symbol plus a `/W` badge;
  a gate-level word gate is drawn closed (children hidden for drawing and
  picking, like a device view) until it is selected. Selection changes that
  open or close one mark geometry dirty. Palette prefix colors match the
  one-bit gates; server layout defaults use a square body.
- C4: `Logic::sub` and `negate` (Minimal mode, 8/16/32 bits) use one
  `NOTGate<W>` for the operand inversion, keeping the `NOT_n`/`JOIN_n` numbers.
  The M-unit and core base tables are unchanged in all three profiles. Chapter
  2 adoption is deferred (previous section).
- Counts (construction only, scenario defaults): `MulDivUnit<32>` scenario
  843 → 731 (B1) → 507 components; five-stage core 15,247 → 15,025 → 14,801;
  `MultiplyResultCorrection<32>` 82 → 79 → 15 children. The RV32IM inspection
  profile prefers compact word parts, so these scenarios show each inverter
  word as one compact gate; the `gate` preset expands it.
- Layouts: a topology-only migration pruned removed children and relaid 10
  automatic group scopes (including the stale B1 leftovers) with the existing
  floorplanner; floorplan-code groups were left alone with nothing missing.
  The static layout check reports 0 problems on 14 scenarios.

Verification: 53 focused CTests pass (216 s), including all RV32IM/M/decoder/
device tests, the sweeps, `DelayRuleSelectionTest`, `PartDelayTablesTest`,
adapter, gate-cache and tick-recording checks; 19 of 21 visualizer CTests pass
with the same two pre-existing failures (no word gate lacks a palette color).
`node --check` passes for `app.js`. Not run: full suite, WASM/browser checks
(the canvas change is unverified in a browser), strict site build.

## 2026-10-27 — Word gate canvas check in a browser

A native server (`visualizer-v2/server.py` with the new `build-milestone`
module, layout/profile files redirected to scratch copies so nothing in the
repository was written) and a Playwright check on
`?scenario=muldiv-w32&fidelity=gate`: seven `NOTGate<32>` word gates, each
gate-level. Unselected, a word gate is closed (`closedWordGate`), drawn as the
NOT symbol, and its children are absent from the visible geometry; selected,
it opens and its 32 inverters, `IN_BITS` and `OUT_WORD` are drawn and
pickable. No page errors. Screenshots were inspected: the
`MultiplyResultCorrection<32>` group shows its two word inverters as symbols.
The `/W` badge is not drawn when the symbol is under 28 pixels wide.

Remaining clutter seen there: when `Logic` splits an adder result and a later
join of exactly those bits returns the word, the split Rewire stays with
unused outputs (`SPLIT_77` in that group). The builder cannot remove a child,
so fixing it needs lazy splits in `Logic` (build the split only when a bit is
first used). Not done.

## 2026-10-27 — Zero-delay splitter in `unit`; Chapter 2 word gates

Owner decision (option 1): a bus splitter is wiring, so `unit` timing no
longer charges it a tick (`TimingProfile.cpp`; it was already zero in
`embedded` and `desktop`). With all adapters free, a word gate is a pure
regrouping, and the Chapter 2 recipes use them:

- `LogicUnit<W>`: word gates `AND`, `OR`, `XOR`, `NOT`, `NAND`, `NOR`, `XNOR`
  and one `Mux<8,W>` (`SELECT`), replacing W× seven gates and W `Mux<8,1>`.
- `ParallelPrefixAdder<W>`: word gates `PROPAGATE`/`GENERATE` replace the W
  half adders; `CIN`, `CARRY`, `SUM` and the wiring part `CARRY_BITS` replace
  the per-bit final AND/OR/XOR chain (same gates and depth).
- `AddSub<W>`: one `XORGate<W>` (`B_XOR_SUB`) with `SUB` spread by
  `SUB_WORD`, replacing W XOR gates.
- New `WordSliceBuilder` helpers `wordGate` and `broadcast`.
- `ALUContractTest` and `ALUHierarchyTest` counted the W half adders; they now
  require no half adders and exactly one `GENERATE` word gate.

Delay tables regenerated with `part_delay_tables --write --clocked` in all
three profiles: `embedded` and `desktop` are byte-identical to the stored files
(833 entries each), which shows the regrouping is timing-neutral; `unit`
changes 782 of 833 entries because the splitter tick ran through memories,
endpoints and cores. The new `unit` file is installed.

Layouts: all affected scopes were `automatic`. A topology-only migration
re-laid the changed types (`LogicUnit<32>` 260 → 9 children,
`ParallelPrefixAdder<32>` 140 → 12, `AddSub<32>` 41 → 10, and the same at
8/16 bits, plus profile scopes) and gave the new word gate types layouts;
root layouts of `logic-unit-w8`, `add-sub-w8` and both prefix-adder scenarios
were pruned and refreshed with `recalculate_layouts.refresh_automatic`. The
static layout check over all 373 scenarios reports 0 problems.

Site pages updated: prefix adder, `LogicUnit` and `AddSub` structure text
(alu/2.1, 2.2), `timing.md` and the flip-flop page (splitter now free). The
`unit` delay figures quoted in alu/2.1 (ripple 18/34/66, prefix 13/16/19) and
other published `unit` timing numbers still describe the old tick; they are
updated after the timing tests below.

## 2026-10-27 — `unit` re-certification after the splitter change

Removing the splitter tick shortens `unit` paths; `embedded`/`desktop` are
unchanged. Updated stored and expected values (all from the analyzers):

| Quantity (`unit`) | Before | After |
| --- | --- | --- |
| Certified core periods, single / baseline / advanced (`ExperimentClock.hpp`) | 166 / 80 / 94 | 142 / 68 / 82 |
| RV32IM core, standalone M unit (`RV32IMTimingTest`, site pages) | 94 / 68 | 84 / 48 |
| Ripple adder A → C_OUT at 8/16/32 (alu/2.1) | 18 / 34 / 66 | 17 / 33 / 65 |
| Prefix adder A → C_OUT at 8/16/32 (alu/2.1) | 13 / 16 / 19 | 8 / 10 / 12 |
| Prefix adder 32 A → OUT (`SettlingBoundTest`) | 19 | 12 |
| `RV32IALU` EQ structural / proven (`SettlingBoundTest`) | 33 / 30 | 24 / 21 |
| Register-file read path (`RV32IExperimentTest`) | 12 | 11 |
| Advanced core + SRAM safe period (`RV32IExperimentCLITest`) | 174 | 170 |
| Settling chain of three splitter+inverter stages | 6 | 3 |

`tools/timing/documentation-timing.generated.json` was regenerated: the `unit`
splitter delay, description, periods, and three FF-advanced access counts
(5 → 6, 5 → 6, 9 → 10 cycles, because the clock is shorter while access times
in ticks are unchanged). `TimingTableDocumentationTest` expects the 8-bit
ripple row 2–17. The prefix adder at 32 bits is now about 5.4 times faster
than the ripple adder in `unit`, consistent with the realistic profiles.

All 26 non-slow timing CTests pass after these updates. Published §4.8
experiment results measured at the old `unit` periods remain dated results
(owner decision pending); the analyzer reference keeps its dated history.

## 2026-10-27 — Broad native suite after the splitter change

`ctest -LE 'slow|recorded-games|gameplay|phase6|benchmark' -j1` in
`build-milestone` (364 tests, 78 min, under a 2.5 GB memory scope):
358 passed, 6 failed.

- `FFMemoryBindingsTest`: the stored structural trace in
  `tests/fixtures/memory-array-timing.json` was one tick per splitter slower.
  The value sequence is identical (40 rows, same values, same order) and each
  transaction now settles one tick sooner. The fixture times were updated and
  its description records why.
- `RegisterLayoutGenerationTest` and `RV32ISingleCycleLayoutTest`: these failed
  already on the committed `layout.json` (the same empty `Register<32>` entry
  in profile `b3399052dc128d3a` and empty `RV32ISingleCycleCore` entry in
  profile `197f49957ffbeb1c` exist in the last four commits). They were fixed
  by running the repository generators (`tools/generate-register-layouts.py`
  and `generate-single-cycle-core-layout.py`'s `refresh_layout`). Only those
  two entries and the `rv32i-program3` root mode (automatic → floorplan)
  changed. Both tests pass; the static layout check reports 0 problems in 373
  scenarios.
- `DeviceParametersBindingsTest`: not caused by this work. The uncommitted
  device change of 2026-10-24 gave `device.framebuffer` a behavioral factory,
  so the test's expectation that a behavioral framebuffer is rejected no
  longer holds. Left for the deferred device I/O work.
- `VisualizerColorPaletteTest`, `VisualizerReferenceProfileTest`: failing
  before this work (recorded earlier); unchanged.

`DelayRuleTest` (slow) passed before the splitter change and is re-run after it.

## 2026-10-27 — `DelayRuleTest` after the splitter change

The slow `DelayRuleTest` (11 min) failed only on the recorded floors of the
three cache controllers: 0 parts break the delay rule, and all three cores are
verified with 0 setup violations at their new certified periods (142 / 68 /
82), compact clocks equal to the gates'. With the splitter free, the
controllers' gates settle sooner relative to their tables, so the sampled
shares fell:

| Part | Floor before (typical / worst) | Measured now |
| --- | --- | --- |
| `memory.cache.controller.8` | 84 / 61 | 83 / 58 |
| `memory.cache.controller.16` | 83 / 64 | 81 / 58 |
| `memory.cache.controller.32` | 85 / 75 | 83 / 75 |

The exception reason is unchanged (this sample does not reach the slowest
case), so `FLOORS` in `tests/cpp/DelayRuleTest.cpp` takes the values printed
by `DelayRuleTest --floors` for those parts.

## 2026-10-27 — D2 (run-time pin width) done

Implemented and measured in the
[engine performance record](2026-10-26-engine-performance.md) (section
"D2: packed values and run-time signal width"). Pins and wires take any
width 1–64; component families keep their own width lists. The broad
non-slow native suite after D2 passes 362/365 in 5,746 s; the three failures
are the ones that failed before D2 (`DeviceParametersBindingsTest`,
`VisualizerColorPaletteTest`, `VisualizerReferenceProfileTest`). Remaining
plan items: pooled events, the three unused splits in the multiply/divide
steps (`MultiplyStep<32>` 2, `DivideStep<32>` 1), and at the end the example
table and recording regeneration for the `unit` timing change, the WASM
rebuild with parity checks and the strict site build.

## 2026-10-27 — `unit` tables: second pass (the first used stale storage tables)

Correction to the "Zero-delay splitter in `unit`" section: that regeneration
was one pass. A part's table is proven on its gates with its storage and
pipeline registers kept compact, so those inner parts ran on the `unit`
tables compiled into the tool, which still charged the splitter tick. The
first pass was therefore conservative for every part that contains storage.
Found when `part_delay_tables --check arithmetic.iterative.mul-div.32` gave
setups one tick shorter than stored (A 22 vs 23, internal period 31 vs 32),
with and without the step change below; `embedded` and `desktop` were not
affected (their storage tables did not change, and their regeneration was
byte-identical).

Second pass (`--profile=unit --clocked --write`, 694 s): 117 of 833 entries
differ, every changed number smaller (415 numbers), none larger: 46 variants
each of the RV32I and RV32IM five-stage cores, the other cores and the
single-cycle system, fetch/decode/execution-status stages, the four pipeline
registers, the prediction path, the memory request, the M unit, and six
device-part tables (counter 4/32, FIFO 8/32, register port, address router,
instruction guard; generated data, no recipe change). Installed; a third
pass (730 s) is identical to the second, so the tables are a fixed point.

Re-certification after the second pass (full rebuild): `RV32IProfileSweepTest`
found the advanced core's `unit` certificate at 80 ticks instead of 82;
single-cycle (142) and baseline (68) are unchanged. `CERTIFIED_CORE_PERIODS`
now stores 80, and `documentation-timing.generated.json` was regenerated with
`documentation_timing_data --write`: only `certified_periods.advanced` and
`period_ticks` change (82 → 80); every memory access keeps its cycle count.
`site-docs/rv32i/experiments.md` and the analyzer reference name 80. All 26
non-slow timing CTests and `DelayRuleTest` pass afterwards. The RV32IM core
stays at 84.

## 2026-10-27 — Unused splits in the multiply/divide steps removed

The last three Rewires with no connected output were inside the steps:
`MultiplyStep<W>` split `HI` and its adder's sum only to join them again, and
`DivideStep<W>` split `DIVISOR` only for the word inverter to take back the
whole word. New `Logic` helpers keep those words whole, with the same gates,
adder operand order and child names (they use up the same `SPLIT_n`/`JOIN_n`
numbers): `addToWord(x, y, carry_in)` (= `join(add({split(x), y, bit(carry_in)}).first)`)
and `subtractWord(a, y)` (= `sub({a, split(y)})`). At 4 bits, without a word
adder, both fall back to the bit path, so the 4-bit steps are unchanged.

- Construction census: unused Rewires 3 → 0 in `MulDivUnitWidth32ScenarioTest`
  (29 Rewires) and in the five-stage core (173 Rewires).
- `part_delay_tables --check` for the steps (no tables of their own) and
  `arithmetic.iterative.mul-div.32`: `embedded`/`desktop` identical; the
  `unit` difference is the stale first pass above (it also appears with the
  original recipes) and is gone after the second pass.
- Layouts: `MultiplyStep<32>` (type and profile `763e0b8bac717d60` scopes,
  automatic) pruned `SPLIT_33`/`SPLIT_38` and re-laid 36 children;
  `DivideStep<32>` pruned `SPLIT_1` and re-laid 105. Static layout check: 0
  problems in 373 scenarios.
- 15 focused CTests pass, including the slow `MulDivGateLevelTest` and the
  four step tests and scenarios.

## 2026-10-27 — Pooled events done

`circuit::makeEvent` with a per-thread free-list allocator; the Visual guest
runs 6.3% faster with identical events. Details and checks are in the
[engine performance record](2026-10-26-engine-performance.md) ("Pooled
events"). Remaining plan items: example tables and recordings for the
`unit` timing change, the WASM rebuild with parity checks, the strict site
build.

## 2026-10-27 — WebAssembly rebuild, parity checks and site build

This closes the last three plan items that do not need an owner decision.

- WebAssembly module and verification runner rebuilt serially
  (`build-wasm-milestone`, the current working tree with pooled events, D2 and
  the second `unit` table pass). 16 tests pass in the WASM verification runner:
  `WordGateSweepTest`, `AdderWidthSweepTest`, `RV32IALUSweepTest`,
  `RV32IPipelineControlSweepTest`, `BuiltinComponentCatalogInventoryTest`,
  `MulDivUnitWidth32Test`, `MulDivGateLevelTest`,
  `RV32IMFiveStageCoreScenarioTest`, `RV32IMArithmeticProgramTest`,
  `RV32IMDivideEdgesProgramTest`, `SimulatorDrainUntilIdleTest`,
  `RewireValidationTest`, `RewireTest`, `MultiplyStepTest`, `DivideStepTest`
  and `RV32IMFiveStageCoreTest`.
- Example tables: the site packaging regenerates every
  `simulator-examples` table from its recorded scenario; all 161 example
  groups verify (each in its own process), including the multiply/divide
  groups at the new `unit` timing. No generated rows were edited by hand.
- Strict MkDocs build passes.
- Native/WASM parity suite (16 checks, 67 minutes): 14 passed on the first
  run. The two failures were stale test expectations, not simulation
  differences:
  - `ALUWasmParityTest` still counted 32 `HalfAdder` cells in the RV32I ALU's
    prefix adder; since the word-gate change its bit stage is one
    `XORGate<32>` (`PROPAGATE`) and one `ANDGate<32>` (`GENERATE`). The check
    now asserts those two gates; re-run passes.
  - `RecordedGuestWasmParityTest`: `RecordedGuestProbe` (native) now defaults
    to the `compact` fidelity preset, while the WASM bridge ran with no preset
    and so reported the legacy `recorded-device-compact-children` profile
    name and reasons. Event counts were identical (15,181,178). The test now
    selects `compact` on the bridge (`setFidelityProfile`). Re-checked on the
    `bringup` guest only: exact native/WASM parity (38,811 cycles, profile
    `fidelity-compact-v1`). The other nine guests and the 97×65 surface run
    were not re-run after the fix (about an hour); their native references
    were captured without error in the first run.
- Not regenerated: the game recordings and their site assets. They belong to
  the device work and the pending owner decision on the gameplay demos.

What remains of this plan is owner decisions only: the published §4.8 results
at the old `unit` periods, the gameplay recordings, and a full native suite
before committing.

## 2026-10-28 — Recorded-guest parity: all guests

The full `RecordedGuestWasmParityTest` re-run after the preset fix passes
(140 minutes): all ten scripted guests (`bringup`, `2048`, `sokoban`,
`snake`, `pong`, `tetris`, `2048-merge`, `tetris-line-clear`, `pong-rally`,
`snake-wall`) give exact native/WASM frames, reports, device samples, cycles
and event counts, and the 97×65 `bringup` run matches with full frames. The
parity suite therefore passes 16 of 16.

The committed game recordings under `site-docs/assets/rv32im/circuits/` are
stale for the current timing: `bringup` still runs 38,811 cycles, but its
evidence lists 15,336,236 processed events against 15,181,178 now (the
`unit` tables and the zero-delay splitter changed internal event timing).
They were not regenerated; that waits on the owner's decision about the
gameplay recordings.

## 2026-10-28 — Owner decision: recordings wait for device I/O

The owner chose to leave the ten committed game recordings as they are and
recapture them once after the device I/O work, which will change them again.
They still play back; only their event counts are stale. With that, the
non-device plan has no remaining work of its own. Still open as owner
decisions: the published §4.8 results at the old `unit` periods and a full
native suite before committing.

## 2026-10-28 — Visualizer browser checks

Run with `tools/run-web-tests.py` against the rebuilt site
(`build-wasm-milestone`, built after the step layout changes), browser Python
`/usr/bin/python3` (the docs venv has no Playwright; a first attempt with it
ran nothing).

- `VisualizerRewireBrowserTest` passes (unpackers, RV32I lanes, field
  extractor, control packer, routing).
- `VisualizerScenarioBrowserTest` passes (grouped catalog, legacy URL,
  core/prediction changes, fidelity and compact controls).
- `VisualizerWasmBrowserTest` failed on stale Chapter 2 embed component
  counts: the word-gate change regrouped the prefix adder, `AddSub` and
  `ArithmeticUnit`, and each word gate is one part containing its one-bit
  gates. New counts, identical in the native probe and the browser:
  `parallel-prefix-adder-w16` 320 → 319, `add-sub-w8` 156 → 167,
  `arithmetic-unit-w8` 897 → 908. Expectations updated; re-run passes
  (219 scenarios, 731 documented example rows).

## 2026-10-28 — Full native suite: partial run, one failure fixed in source

First attempt (`ctest -j2`, 591 tests): the first five recorded-game tests
(`RV32IMRecorded_<game>_Test`) failed with "Simulation event budget
exhausted". Cause: since the 2026-10-24 fidelity presets,
`tools/rv32im/run-recorded.py` defaults to the Visual preset, and
`tests/checks/RV32IMGameReferenceTest.py` did not pass a profile, so the games
ran at Visual detail past the 500 million event budget (the earlier passing
runs used compact). Fix: the test passes `--logic-profile compact`. The run
was stopped and restarted.

Second attempt: Claude Code stopped it for host memory pressure (other
projects on the host: code-server, quant-ai documentation builds) after 30 of
591 tests. 29 passed, including all ten recorded-game tests (406–900 s each)
with the fix. One failed: `RV32IProfileToggleSweepTest` — "RV32I
profile-toggle audit omitted selectable contracts: logic.and.32, logic.or.32,
logic.xor.32, word.parallel-prefix-adder.32". These are selectable inside the
RV32I ALU since the word-gate change; the audit list in
`src/tests/RV32ISingleCycleTests.cpp` now covers them (program 2, each
behavioral). Not yet rebuilt or re-run (host memory). The remaining 561 tests
were not run.
