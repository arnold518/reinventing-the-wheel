# Device I/O and redundant bit-adapter review

Started: 2026-10-27. Status: open. Code state: `alpha` at `5549ee0`, with the
existing uncommitted component, visualizer, profile and recording changes
preserved. Review only: no source, test, layout or recording was changed.

## Summary

The redundant one-bit and round-trip adapters come from the bit-vector
helper `circuit::structural::Logic` (`include/components/StructuralLogic.hpp`),
used by every Device I/O recipe, the RV32IM M-unit/core extensions and
`Decoder`. `out(src,sink)` is `join(split(src))` by construction, `drive(v)`
always joins, and `add/sub` always join operands and split results. A census
of eight small deployed scenarios finds 1,072 adapter components among 7,529
(14%), 112 one-bit-to-one-bit Rewires, 111 full split-then-join round trips
and 179 partial (byte/slice) round trips. Each Logic split publishes every
output bit on any input change. Recipes also repeat gates: `mux` re-inverts
its select per bit, constant comparisons use XOR+NOT per bit, and the button
update builds 32 private comparators instead of the existing `Decoder<5>`.

No Device I/O correctness defect was found. The integrated system never uses
the router/guard per-target CLK/RST outputs, yet each is driven through two
one-bit Rewires. There is dead helper code and a duplicated default address
map. Most public pages match the code, with a few wording gaps.

Proposed next step (not started): a Logic peephole/bridge change that keeps
generated names stable, then recipe cleanups, regenerated timing tables, and
layout/recording migration. Owner decisions are listed in the plan below.

## 2026-10-27 — Scope and method

Read all of `src/modules/device/*`, `include/modules/device/*`,
`include/components/StructuralLogic.hpp`, `StructuralNetBuilder.hpp`,
`BitAdapter.{hpp,tpp}`, `Rewire.{hpp,cpp}`, `WordSliceBuilder.hpp`,
`WordBuilder.hpp`, `src/modules/rv32im/{ArithmeticGroups.hpp,MulDiv.cpp,
RV32IMCore.cpp,RV32IMDeviceSystem.cpp}`, the twelve `site-docs/device-io/`
pages and their registrations. Saved layouts were counted from
`visualizer-v2/layout.json` (`type_layouts`, names matching Logic's `SPLIT_n` /
`JOIN_n`).

Live structure census: the deployed Python module
`visualizer-v2/circuit_backend.cpython-313-x86_64-linux-gnu.so` (built
2026-10-24 23:12, so it may lag the working tree) under
`prlimit --as=2147483648`, one process, 3.0 GiB host memory available.
`create_test_by_name(...).setup_circuit()` only; no simulation. Peak RSS
56 MB, 1.1 s. The script is in the session scratchpad (`census2.py`) and is
not tracked. No build, CTest, timing-table generation or browser check was run.

## 2026-10-27 — Redundant adapters: where and why

### Root causes in `Logic`

| Helper | Line | What it builds | Redundancy |
| --- | --- | --- | --- |
| `split(P)` | 77 | One Rewire `IN[w] → B0..B(w-1)` | A 1-bit source still gets a 1→1 Rewire. Splitting the same source again builds another Rewire. |
| `join(V)` | 85 | One Rewire `B0..B(n-1) → OUT[n]` | A 1-element vector gives a 1→1 Rewire. Joining exactly the outputs of one split rebuilds the original word. |
| `drive(V,sink)` | 116 | `join(V)` | Round trip whenever V came from `split`. |
| `out(P,sink)` | 118 | `drive(split(P))` | Always two Rewires. For a child or gate output, a direct wire is legal and enough. Only a parent input → parent output needs one bridge. |
| `add/sub` | 92/107 | Joins both operands, adds, splits the result | Operands from `split` round-trip. `sub` inverts every bit with separate NOT gates before the join. |
| `constant(w,v)` | 47 | `ConstantValue<w>` plus split | Often only feeds `equal()` or `drive()`. |
| `mux`/`choose` | 67/64 | Per bit: NOT(select), 2 AND, OR | The select is re-inverted for every bit. |
| `equal` | 73 | XOR + NOT per bit, then an AND tree | Against a constant, each bit could be a wire or one inverter. |

Logic has no pass-through helper. Three separate bridges already exist
elsewhere: `memory_detail::bridge` (`MemoryFFController.hpp:154-155`,
`Cache.cpp:760-761`), `addPassThrough` (`RV32IPipelinePacking.cpp:167`) and a
bridge in `RV32IZeroWaitMemoryPort.hpp:34-35`. The AGENTS.md rule (a
component input cannot be wired to its own output) needs one zero-delay
Rewire per parent-input → parent-output pass-through, not two.

### Live census (deployed module, construction only)

| Scenario | Components | Adapters | 1-bit→1-bit Rewires | Full split→join round trips | Partial (slice) round trips |
| --- | ---: | ---: | ---: | ---: | ---: |
| `MulDivUnitWidth32ScenarioTest` | 842 | 126 | 38 | 44 | 0 |
| `RV32IMPipelineCoordinatorScenarioTest` | 153 | 48 | 48 | 24 | 0 |
| `AddressRouterWidth32ScenarioTest` | 940 | 197 | 24 | 30 | 34 |
| `ConsoleDeviceScenarioTest` | 1,182 | 149 | 2 | 3 | 25 |
| `ButtonDeviceScenarioTest` | 2,124 | 254 | 0 | 2 | 48 |
| `TimerDeviceScenarioTest` | 749 | 98 | 0 | 3 | 24 |
| `FramebufferDisplayScenarioTest` | 1,210 | 168 | 0 | 5 | 48 |
| `DeviceRegisterPortWidth32ScenarioTest` | 329 | 32 | 0 | 0 | 0 |

"Full" counts a join (or 1→1 Rewire) whose inputs are all the outputs of one
split. "Partial" counts a join fed by part of one split, for example a byte
of a split address. No adapter had an unconnected output. Five sources were
split more than once.

Where the one-bit pairs come from:

- **`RV32IMPipelineHoldControl` (`RV32IMCore.cpp:167-171`):** 24 `out()`
  calls, 48 one-bit Rewires, 57% of that group's 85 children. Only 7 are fed
  by a parent input and need a bridge. The other 17 are gate outputs and need
  a direct wire.
- **`ArithmeticIterationControl` (`ArithmeticGroups.hpp:59-61`):** 5 one-bit
  `out()` per instance, 20 Rewires across the multiplier and divider.
- **Smaller sources in `MulDivUnit`:**
  - `MulDivUnit` READY/BUSY/DONE (`MulDiv.cpp:115-117`, 6 Rewires);
  - operand sign outputs (`ArithmeticGroups.hpp:97-98`, 4 + 4);
  - `MultiplyStep` CARRY and `DivideStep` QUOTIENT_BIT (`MulDiv.cpp:88,95`).
- **`DeviceRouteChannel` (`DeviceBus.cpp:124`):** TARGET_CLK/TARGET_RST via
  `out()`, 4 Rewires per destination (24 in the router, 4 in the guard).
  `RV32IMDeviceSystem.cpp` wires CLK/RST straight to every target, so these
  outputs drive nothing in the integrated system.
- **Console `BACK_READY` (`DeviceState.cpp:196`):** one bridge-type pair.

Width-1 splits in `IndexedPixelSelector<8>` (1-bit OFFSET) and
`RV32IMDecodeOverrides` (`RV32IMCore.cpp:103-107`) are also of this kind.
`RV32IMDecodeOverrides` has 337 saved-layout children (31 SPLIT, 21 JOIN).

Across all saved type layouts there are 263 Logic `SPLIT_n` and 196 `JOIN_n`
children. These are counted once per type, not per instance.

### Worked example: `MACHINE.CORE.EXECUTE.MULDIV.MULTIPLY.RESULT.SPLIT_0`

The coordinator verified this case; it is copied here. It is the user-visible
crowding case.

- **The round trip:** `ArithmeticGroups.hpp:145` builds
  `q=l.split(input("LOW"))`. In the multiply branch, line 156
  `l.drive(q,output("RESULT_LOW"))` re-joins it. SPLIT_0 → 32 one-bit wires →
  JOIN is a pure identity round trip. It exists only because a parent input
  cannot be wired straight to its own output; one Rewire would do.
- **Child counts:**
  - `type_layouts["MultiplyResultCorrection<32>"]` has 82 children: 6 SPLIT,
    6 JOIN, 64 NOT, 2 ADD32, 2 word muxes, ONE and ZERO_WORD.
  - `DivideResultCorrection<32>` has 116 children: 7 SPLIT, 6 JOIN, 64 NOT
    and 31 ANY-tree gates.
- **Which pairs are redundant:** four of the six multiply split/join pairs
  are pure round trips:
  - LOW → RESULT_LOW;
  - HIGH → adder A, through the join inside `Logic::add`;
  - adder 1 OUT, split then joined into adder 2 A;
  - adder 2 OUT, split then joined to RESULT_HIGH.
- **The 64 NOTs:** `Logic::sub` inverts each bit of `correct_a`/`correct_b`.
  The Chapter 2 `AddSub<W>` family (`circuit::word` `Unit::AddSub`) could
  replace NOT×32 + split + join + adder for each subtraction.

The same structure gives `ArithmeticIterationControl<32>` 91 children for a
5-bit counter:

- `split(count Q)` is built twice;
- the 5-bit `add` uses a ripple chain with a constant split;
- two 5-bit `mux` calls add 10 select inverters.

### Simulation cost

`Rewire::evaluate` (`Rewire.cpp:109-140`) builds a `std::map` of output
vectors on every evaluation. It schedules a wire update for every output,
changed or not. A 32-bit split→join round trip on a changing word therefore
costs one split evaluation, up to 32 wire updates, one or more join
evaluations and one more update. A direct wire costs none of these; a single
bridge costs one evaluation and one update. This matches the engine review's
adapter hotspot (`2026-10-26-engine-performance.md`, 2026-10-27 section).
The event saving is not measured here.

`BitSplitter`/`BitJoiner` (`WordSliceBuilder`, Chapter 2 and memories) are
not Logic adapters. `BitSplitter` carries a one-tick `unit` delay
(`TimingProfile.hpp:39`). Removing those changes `unit` timing and is
outside the first step.

## 2026-10-27 — Fix plan and migration impact

Ordered by risk:

1. **Logic peepholes, no recipe edits.**
   - `split` of a 1-bit pin returns `{pin}`. A 1-element `join` returns the
     element.
   - Memoize `split` per source pin.
   - `join(V)` returns the original source when V is exactly one split's
     outputs in order.
   - Add `Logic::passThrough(src,sink)`: one zero-delay identity Rewire when
     `src` is a parent input and `sink` a parent output of the same owner,
     otherwise a direct connect. Make `out()` and `drive()` use it.
   - Precompute `inv(select)` once per `mux`.

   To keep saved names stable, the skipped constructions still consume
   `id()` numbers. Otherwise every later `NOT_n`/`JOIN_n`/`SPLIT_n` in the
   scope renumbers, because `id()` uses one shared counter.
2. **Word-level helpers.** Let `add/sub` accept word pins. Use the
   `AddSub<W>` family (or an inverted word plus the prefix adder) instead of
   per-bit NOT. Add a single-Rewire `slice` / `zeroExtend` (Rewire already
   supports `UnmappedBitValue::LOW`/`HIGH`). Apply these to:
   - the route-channel address offset (`DeviceBus.cpp:126-127`);
   - `addressMatch` byte inputs and per-byte constants (`DeviceGroups.hpp:142-155`);
   - `Rgb332Expansion` (`DeviceDisplay.cpp:38-40`);
   - `ButtonEventEncoding` (`DeviceState.cpp:273-276`);
   - read-data padding.
3. **Gate-level cleanups. These change delays.**
   - Constant-folded `equal` against constants.
   - `Decoder<5>` for `ButtonInputUpdate` (`DeviceState.cpp:256-266`): 32
     cells × (2 splits + constant + 10 XOR/NOT + 4 AND + 4 mux gates) become
     one shared decoder.
   - Size the divider phase register to `bit_width(divisor-1)` instead of 32
     bits (`DeviceState.cpp:79-93`).
   - Constant-folded decode overrides: `old & ~m` / `old | m` instead of a
     per-output mux (`RV32IMCore.cpp:106`).
4. **Contract decision.** Remove the router/guard `<REGION>_CLK/_RST`
   outputs, or keep them as documented pass-throughs with one bridge each.
   Removing them changes public pins, layouts and published docs.

Impact to plan for:

- **Layouts.** Saved paths that disappear: 263/196 SPLIT/JOIN type children
  and all instance placements beneath them. With the `id()` preservation
  above, surviving names stay stable; without it, every Logic-built type
  needs regeneration. Run `tools/check-static-layouts.py` and prune or
  migrate the affected type and profile keys.
- **Delay tables.** Logic Rewires have zero delay, so step 1 should leave
  every arc unchanged. Regenerate with `part_delay_tables` and require
  identical output; that is the equivalence check. Steps 2–3 change gate
  depth, so regenerate all three profiles and record the changed device,
  M-unit and core arcs.
- **Compact forms.** Gate-list device/M-unit compact models compile the same
  groups, so their tables and fingerprints follow the structural change.
- **Recordings and evidence.** Structural/visual recordings, recorded
  checkpoints (`site-docs/assets/rv32im/circuits/*`) and published evidence
  JSON carry component paths and event counts. Old self-contained
  recordings remain replayable, but exact-reference comparisons and event
  counts need recapture. Coordinate with the frozen gameplay queue.
- **Site examples.** Pin-level example rows should not change in step 1;
  regenerate and compare all example groups.
- **Docs.** `site-docs/device-io/index.md` ("Bit rewires expose the
  individual bits where gates need them") then becomes accurate. Pages
  describing internal structure need child-count/name updates only after
  steps 2–4.

Validation per step: focused device/M-unit/coordinator CTests in every
fidelity; an unchanged pin-trace comparison against the current build for
step 1 (exact) and steps 2–3 (value-exact, timing within regenerated tables);
`part_delay_tables` verification; WASM parity for changed scenarios; static
layout check; strict site build. Measure event counts before and after on the
eight census scenarios and one short guest.

## 2026-10-27 — Device I/O component review

Pin names, directions and widths were checked against each page's table. All
twelve pages match the pin initializers (`portPins`, `routerPins`,
`counterPins`, `fifoPins`, `consolePins`, `buttonPins`, `displayPins`,
`pixelPins`). Each component has an independent functional test and a
recorded scenario owner (`RV32IMPhase3Tests.cpp:703-727`, appended through
`appendRV32IMPhase3Tests`). Test strength is reviewed separately in
`2026-10-27-test-strength-review.md`.

| Component | Contract | Fidelities | Main findings |
| --- | --- | --- | --- |
| `ClockedCounter<W>` | `device.counter.{4,8,16,32}` | Structural all; compact 4/32 | 32-bit phase register/comparator/adder for any divisor (Low). Page says "validation is in progress" (Low). |
| `SynchronousFifo<W>` | `device.fifo.{8,16,32}` | Structural all; compact 8/32 | `FifoReadSelect` builds one constant + split per slot comparison (Low). Same page wording. |
| `DeviceRegisterPort<W>` | `device.word.register-port.W` | Structural; compact at 32 | Clean: no 1→1 or round trips. Five 3-bit `statusMux` groups, each 12 gates with 3 duplicate select inverters (Low). |
| `BusErrorTarget<W>` | `bus.word.error-target.W` | Structural only | Matches page. |
| `AddressRouter<W>` | `bus.word.address-router.W` | Structural; compact at 32 | Unused target CLK/RST outputs via 4 Rewires each (Medium). Per-byte address match: split→join→split round trips (Medium). |
| `InstructionAccessGuard<W>` | `bus.word.instruction-guard.W` | Structural; compact at 32 | Same router assembly; MEM_CLK/MEM_RST unused by the system (Medium). |
| `ConsoleDevice` | `device.console` | Structural + behavioral composition | Each register offset gets its own full 32-bit `addressMatch` (Medium, shared). `BACK_READY` bridge pair (Low). |
| `TimerDevice` | `device.timer` | Same | `TimerAccess.ENABLE` is a constant group output (Low). Same decode duplication. |
| `ButtonDevice` | `device.buttons` | Same | 32 `ButtonLevelUpdate` comparators instead of `Decoder<5>` (Medium). Fidelity-dependent `LEVELS` construction path (`DeviceState.cpp:250-255`; Low). |
| `FramebufferDisplay` | `device.framebuffer` | Same | Frame counter `add` round trips (`DeviceDisplay.cpp:72`; Low). Six full 32-bit register matches (Medium, shared). |
| `IndexedPixelSelector<W>` | `video.pixel-selector.W` | Structural only | Not used by `FramebufferDisplay` or the device system; capture is host-side `captureFrame`. The page should say it is standalone (Low). W=8 builds a 1→1 split and a full byte round trip (Low). |
| `RV32IMDeviceSystem` | `rv32im.system.devices` | Structural | Wires CLK/RST directly to all targets; the router's per-target clocks are unused (see router). |

### Findings by severity

No High-severity (incorrect behavior) finding.

**Medium**

1. **Unused target clock/reset outputs.**
   - Router and guard `<REGION>_CLK/_RST` (`DeviceBus.cpp:124`,
     `routerPins:21-24`) are never connected by `RV32IMDeviceSystem.cpp`.
     They still evaluate two one-bit Rewires per target on every clock and
     reset change.
   - The compact router also forwards them (`GateListPartBehavioral.cpp:77-82`).
   - Decide whether to remove them (a contract change) or keep them as
     single bridges.
2. **Register and address decoding repeats the full 32-bit comparison.**
   - `registerDecode` (`DeviceGroups.hpp:161-173`) builds one complete
     `addressMatch` per register offset. Each one splits ADDRESS and a
     32-bit constant, re-joins 4 bytes of each, then splits them again inside
     four byte cells, with an XOR+NOT against constant bits. Saved
     `DeviceRegisterAddressMatch` has 18 children; 10 of them are adapters.
   - Display/buttons build 6 of these, console/timer 3.
   - A shared upper-bits zero detector plus a small low-byte `Decoder` would
     remove most of this structure. It changes gate depth, so the timing
     tables must be regenerated.
3. **Thirty-two private button comparators.** `ButtonInputUpdate`
   (`DeviceState.cpp:256-266`) has 32 cells. Each builds `ConstantValue<5>`,
   two 5-bit splits, five XOR + five NOT, a 4-gate AND tree and a 4-gate mux.
   `Decoder<5>` (`include/modules/basic/Decoder.hpp:16-20`) already exists.
4. **Logic adapter redundancy** as quantified above. Visual crowding in
   device groups (`DeviceRouteChannel`: 7 SPLIT + 6 JOIN in 26 children) has
   the same cause as the MulDiv example.

**Low**

5. **Dead helpers.** `DeviceLogic.hpp:57` `supportsRouter` is unused and
   duplicated by a lambda at `DeviceBus.cpp:264`. `DeviceLogic.hpp:85` `at()`
   and `DeviceLogic.hpp:96` `validCommand(Logic&,ComponentBuilder&,port)` are
   unused; the live `validCommand` is `DeviceGroups.hpp:190`.
6. **Default address map defined twice.** `DeviceConfiguration.hpp:37-40`
   (`AddressMap`) and `DeviceBus.cpp:19-20` (`regions`, used for pin
   construction). Region names are also restated in `AddressMap::validate`
   and `addressMap()`.
7. **Compact coverage is uneven.** Counter 8/16, FIFO 16 and all non-32-bit
   bus parts are structural only. The pages state this, but the `compact`
   preset then records structural exceptions for them.
8. **Docs.**
   - Counter and FIFO pages say "Fidelity and protocol validation is in
     progress" and "initially provide structural circuits". That is project
     status, not reference behavior.
   - The pixel-selector page does not say the selector is outside the
     framebuffer and device-system path.
   - The router page describes child clocks "pass through", but the
     integrated system does not use them.
9. **`statusMux`** (`DeviceGroups.hpp:126-137`) builds a 3-bit mux from 12
   gates with three identical select inverters. `WordMux` lacks width 3, so
   this needs the `mux` fix or a 4-bit pad.
10. **Lower-level-first.** Satisfied. The compact console/timer/buttons/
    framebuffer forms compile their own structural groups and keep behavioral
    storage families (`DeviceGroups.hpp:81-106`).
    `DeviceBehavioral::validateBehavioralComposition` allows only Rewire,
    ConstantValue, compact groups and memory primitives. No independent
    whole-device behavioral rule was found.

Not checked: run-time behavior beyond construction; WASM; timing-table
contents; test vector adequacy (separate record); public site build.

## 2026-10-27 — Fix step 1 implemented for non-device recipes

Owner direction: non-device work first; device recipes keep their current
construction until the device redesign (P1 of the
[architecture review](2026-10-27-device-architecture-review.md)), so their
layouts and game recordings migrate once.

Change (`include/components/StructuralLogic.hpp`): `Logic` gains
`Adapters::Minimal` (default) and `Adapters::Legacy`. Minimal returns the pin
itself for one-bit splits/joins, splits each source once, returns the source
word for a join of a complete split in order, bridges a parent input to a
parent output with one `PASS_<output>` Rewire (in `wire`, so `drive` and `out`
use it), and shares one select inverter per `mux`. Every skipped construction
still uses up its `id()` number, so the names of surviving children do not
change. `src/modules/device/DeviceLogic.hpp` defines a device-local `Logic`
that selects `Legacy`; no device recipe changed. Non-device users: the
1–4-bit `WordDecoder`, the RV32IM arithmetic groups and step parts, and the
RV32IM core extension groups.

Results (construction-only counts; before = the 2026-10-24 Python module):

| Scenario | Components | Rewires | Gates |
| --- | --- | --- | --- |
| `MultiplierWidth32ScenarioTest` | 288 → 251 | 56 → 27 | 188 → 180 |
| `DividerWidth32ScenarioTest` | 528 → 459 | 62 → 32 | 414 → 375 |
| `MulDivUnitWidth32ScenarioTest` | 843 → 731 | 126 → 61 | 615 → 568 |
| `RV32IMDecodeStageScenarioTest` | 2,671 → 2,608 | 95 → 77 | 2,178 → 2,133 |
| `RV32IMExecuteStageScenarioTest` | 7,994 → 7,876 | 148 → 77 | 6,048 → 6,001 |
| `RV32IMPipelineCoordinatorScenarioTest` | 154 → 113 | 48 → 7 | 103 → 103 |
| `RV32IMFiveStageCoreScenarioTest` | 15,247 → 15,025 | 336 → 206 | 11,803 → 11,711 |

The gate reductions are the shared mux select inverters.

Verification:

- Delay tables: `part_delay_tables --check` for
  `arithmetic.iterative.mul-div.32`, `rv32im.core.educational-five-stage`,
  `logic.decoder.5.full` and `word.alu-control-decoder` gives 0 differences in
  unit, embedded and desktop; the complete unit `--verify` recomputes all 809
  entries identically (653 s). The embedded/desktop complete verifications were
  not run.
- 37 focused CTests pass (197 s): all M-unit/RV32IM unit, scenario, core and
  program tests, `MulDivGateLevelTest`, `WidthComponentsTest`, decoders,
  `RV32IMTimingTest`, the device system scenario, five device tests, registry
  and catalog checks.
- Layouts: `tools/check-static-layouts.py` on the 14 RV32IM/M scenarios found
  12 missing placements (new `PASS_*` bridges) in four types. A one-off
  topology-only migration pruned the removed children and relaid the two
  automatic types (`MultiplyResultCorrection<32>`: 4 pruned;
  `RV32IMDecodeOverrides`: 66 pruned) with the existing floorplanner. In the two
  deliberate `floorplan` types it kept every surviving placement and only added
  the bridges (`RV32IMPipelineHoldControl`: 48 pruned, 7 added;
  `MultiplyOperands<32>`: 9 pruned, its bridge placed at a removed join's spot
  because the floorplanner's spot overlapped). No overlap or bound violation;
  the recheck reports 0 problems. Only those four `type_layouts` entries changed.
- 19 of 21 visualizer CTests pass, including the static and automatic layout
  tests. `VisualizerColorPaletteTest` (about 65 group types, mostly Device I/O
  and RV32IM groups from earlier work, lack palette colors) and
  `VisualizerReferenceProfileTest` (its `app.js` excerpt lacks
  `normalizeFidelityPreset`) fail for reasons that predate this change; neither
  depends on the Logic edit.
- A registry bug found by the layout tool is fixed: `AdderWidthSweepTest` owns
  a contract, which made it visualizable; it is now in the headless set.

Step 2 decision: `AddSub<W>` for `Logic::sub`/`negate` was drafted and rejected.
`AddSub` splits its operand with a `BitSplitter` (one tick in `unit` timing)
and inverts with XOR, so it would lengthen the divide step and the result
corrections on the core's critical path and change every core timing table and
recording. The 64 inverters in `MultiplyResultCorrection<32>` are instead to
become one word-wide NOT gate (non-device plan, phase C), whose structural form
keeps the same gates and zero-delay adapters.

Not run: full native suite, WASM parity, browser checks, strict site build and
the 8893 preview refresh. Recorded game packages are self-contained and were
not recaptured; their core internals now differ from a fresh build.

## 2026-10-27 — Unused splits removed from non-device recipes

A construction census found Rewires with no connected output: 25 of 54 in the
`MulDivUnit<32>` scenario and 28 of 199 in the five-stage core. They were
splits whose bits were only rejoined (the join returned the original word).
`Logic` cannot drop a child after creating it, and it cannot see bits a recipe
wires through another `StructuralWiring`, so removal is done at the source:

- `Logic::out` (Minimal) wires the word straight through, or through one
  bridge for a parent input, still using up the `SPLIT_n`/`JOIN_n` numbers.
- New word-level helpers `invertWord`, `subtractWords` and `negateWord` keep
  results as words with the same gates as `sub`/`negate` (word NOT plus prefix
  adder); widths without a word adder fall back to the bit path.
- `ArithmeticResult` (both corrections), the divide operands and the divide
  datapath's next remainder now use them.

Results: unused Rewires 25 → 3 (`MulDivUnit<32>`) and 28 → 3 (core), the
three left inside the multiply/divide steps; `MultiplyResultCorrection<32>`
has 9 children (82 originally) and `DivideResultCorrection<32>` 42 (116).
`part_delay_tables --check` for `arithmetic.iterative.mul-div.32` and the core
gives 0 differences in all three profiles; 31 focused RV32IM/M CTests pass;
six group layouts were migrated (floorplan groups only pruned) and the static
layout check reports 0 problems on 14 scenarios.

## 2026-10-27 — Last unused splits removed

The three unused Rewires left inside `MultiplyStep<32>` and `DivideStep<32>`
are gone (new `Logic::addToWord`/`subtractWord`); the census now finds 0 in
the `MulDivUnit<32>` scenario and the five-stage core. Details and checks are
in the [non-device improvement plan](2026-10-27-non-device-improvement-plan.md).
