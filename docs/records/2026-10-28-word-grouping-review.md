# Word grouping review of every component

Started: 2026-10-28. Status: open. Code state: `alpha` at `5549ee0` plus the
uncommitted working tree (word gates in `LogicUnit<W>`, `ParallelPrefixAdder<W>`
and `AddSub<W>` already applied).

## Summary

The owner requested a survey of repeated scalar logic and adoption of
multi-width gates across components. The non-device batches below are
implemented: RV32I ALU and pipeline parts, Chapter 2 units, memory/cache
parts and shared helpers. Gate widths now cover every width 2–32; word mux
widths remain 1/4/8/16/32. Timing comparisons and focused checks for those
batches are recorded below and in the linked behavioral-gap record.

Device I/O was explicitly deferred to its redesign. That later workflow
adopted shared register front ends, word read muxes and a decoder/word-gate
button update, but did not complete device-wide word grouping or adapter
cleanup. The 2026-10-29 console audit at the end confirms remaining scalar
three-bit status muxes, a split/join byte slice and the surviving Legacy
helper. Device cleanup remains open in the architecture record.

## 2026-10-28 — Method

1. Census: `VisualizerSessionProbe preview <test> x --fidelity gate` (build
   only) for all 373 registered tests, one program per program family (205
   builds). Nine used a fallback preset: `logic` for the shared-memory and
   D0/DID arrangements, the device system and the scalable endpoint (gate
   rejects their large memories); `scenario` for the program loader, clock
   generator, two-level cache and `FullCircuitTest`. Result: 440 component
   types. For each type, direct children were grouped by child type and name
   with digits replaced (`INPUT_REVERSE_#`); 175 types have a group of four or
   more repeated one-bit parts (7,525 parts).
2. Wiring: `VisualizerSessionProbe slice` on one instance of each of the 175
   types (all 175 sliced; the three program-only types used `logic`). A group
   counts as a word group when no member feeds another member of the group,
   and each input is either one shared signal (a select or enable) or one bit
   each of a single word (one splitter, or one other bit group). Reduction
   trees, prefix networks, ripple chains and decoder minterms were separated
   from word groups.
3. Recipe reading for every type the classifier could not decide (barrel
   shifter stages read as a chain because each stage feeds the next; the
   divider's mux is built by the RV32IM `Logic` helper).

Limits: a family that no registered test builds is not covered. The
classifier is name- and wiring-based; every entry below was confirmed in the
recipe source.

## 2026-10-28 — Findings

Word gates exist as `ANDGate<W>`, `ORGate<W>`, `XORGate<W>`, `NOTGate<W>`
and the NAND/NOR/XNOR forms at W = 4, 8, 16, 32 (`src/modules/basic/WordGate.cpp`).
Word muxes exist as `Mux<N,W>` (`logic.mux.N.W`) at W = 1, 4, 8, 16, 32.
`WordSliceBuilder::broadcast` spreads one bit over a word, so "mask a word
with one enable bit" is one word gate. Splitters, joiners and broadcast are
free in every timing profile, so a regrouping keeps the same gates and the
same delays.

### A. Same gates, regroup now (supported widths)

| Component (file) | Bit-by-bit today | As word parts |
| --- | --- | --- |
| `RV32IALULogic` (`rv32i/RV32IALULogic.cpp`) | 32 AND, 32 OR, 32 XOR | `ANDGate<32>`, `ORGate<32>`, `XORGate<32>` |
| `RV32IALUAddSub` (`rv32i/RV32IALUAddSub.cpp`) | 32 `B_XOR_SUB_#` | one `XORGate<32>` with `SUB` broadcast (as `AddSub<W>`) |
| `RV32IALUShifter` (`rv32i/RV32IALUShifter.cpp`) | 224 `Mux<2,1>` | 7 `Mux<2,32>`: input reverse, 5 stages, output reverse |
| `BarrelShifter<8/16/32>` (`composite/WordExtended.cpp` `buildShift`) | 192/96/40 `Mux<2,1>`, W `VALID_BIT` AND | `Mux<2,W>` for reverse in/out and each stage; fill as a word mux; `ANDGate<W>` for valid. The 3–5 `SHIFT_#_CARRY` muxes stay single (a chain) |
| `CompareSelect<W>` (`buildCompare`) | 4×W `Mux<2,1>` (MIN/MAX), W `Mux<8,1>` | 4 `Mux<2,W>`; one `Mux<8,W>` with flag words (bit 0 = flag, rest 0) |
| `ALUResultSelector<W>` (`buildResult`) | 6×W AND, 5×W OR | 6 `ANDGate<W>` with broadcast enables, 5 `ORGate<W>` in the same balanced tree |
| `ArithmeticUnit<W>` and every `maskBus` user (`WordBuilder::maskBus`) | W AND per mask | `ANDGate<W>` with broadcast; fix the helper once |
| `RV32IALUResultSelect` (`ALUResultBit` ×32) | 32 nine-way one-hot cells | 9 `ANDGate<32>` with broadcast selects and `ORGate<32>` levels; `ALUResultBit` retires |
| `RV32ILoadByte<0..3>` (`rv32i/RV32IMemoryLanes.cpp`) | 8-bit AND masks and OR merges | `ANDGate<8>`, `ORGate<8>` |
| `MultiplyStep<W>` (`rv32im/MulDiv.cpp`) | W AND with `BIT` | `ANDGate<W>` with broadcast |
| `DivideStep<W>` (`Logic::mux` on words) | 2W AND + W OR (a 2:1 word mux) | one `Mux<2,W>` |
| `MemoryPayloadMux<4/8/32>` (`memory/MemoryHandshakeWiring.hpp`) | a hand-built 2:1 mux per bit | the existing `Mux<2,W>` |
| `MemoryReadSelector<W,N>` (`include/modules/memory/MemoryArrayBlocks.hpp`) | per-bit mux tree over N words, W AND enables | `Mux<N,W>` over the words, `ANDGate<W>` with `READ_VALID` broadcast |
| `CacheLineState<4/8>` (`memory/Cache.cpp`) | CLEAR/MARK/KEEP/NEXT/NOT_CLEAR/SELECTED_VALID per line | word gates of 4/8 |
| `StaticMemorySequencer` counters (`memory/MemorySequencers.cpp`) | COUNT_LOW/RUNNING/WAITING/KEEP/LOAD/NEXT ×8 | word gates of 8; the borrow chain stays |
| `Decoder<5>` output enable | 32 `OUT#_ENABLE_AND` | `ANDGate<32>` with broadcast enable (minterms stay) |
| `RV32IPredictionTable<4/16>` write rows | `WRITE_ROW_#` AND ×N | `ANDGate<N>` with broadcast |
| RV32IM `Logic` helper (`include/components/StructuralLogic.hpp`) | `mux`, `land`/`lor` over bit vectors | emit `Mux<2,W>` and word gates at word widths; this fixes its callers at once |

### B. Equality compares: regroup plus an existing reduction

`XOR` per bit followed by an OR tree is "not equal"; it matches `XORGate<W>`
followed by the existing `ZeroDetect<W>` (same OR tree): `CacheTagMatch<32>`,
`RV32IRequestTagMatch`, and both compares in `RV32IBranchRecovery`. The DRAM
open-row compare in `CommandMemorySequencer` uses XNOR bits and an AND tree
instead (`NOT` per bit, then AND): regrouping keeps its gates as
`XORGate<32>` + `NOTGate<32>`, while moving to `ZeroDetect` would change its
gates and delays.

### C. Not supported at today's widths

Groups of 5, 6, 29, 30 and 31 bits: the forwarding, hazard and write-back
bypass register compares (5 bits), `Decoder<5,zero-disabled>` (31 enables),
the row decoders' address inverters (31 bits in most), `Incrementer<32,4>`
(30 increment XORs), and the range checks' OR trees (odd widths). They need
either more word widths (pins already allow 1–64; widths are a family
contract choice) or stay bit by bit.

### D. Not word groups (stay as they are)

Reduction trees (`ZeroDetect`, range checks, timers), prefix networks
(`CarryCombine`, `Incrementer` prefix), ripple chains (`RippleCarryAdder`,
counters, borrow chains), decoder minterms, decode control fields
(`RV32IDecodeControlField<…>`, `RV32IMDecodeOverrides`,
`RV32IMExecutionControl`, `ALUControlDecoder`) and storage arrays (cells and
rows already form words). The word muxes `Mux<N,W>` themselves are one
`Mux<N,1>` per bit by definition; `Mux<2,W>`'s own SELECT/COMBINE gates could
be three word gates, which adds one level and is optional.

### E. Pin contract is per bit

`SRAMColumnSelector` and `MatrixColumnMux` expose one-bit bitline pins
(`BL_n`, `BL_BAR_n`, `DRIVE_n`); grouping them means changing their pins to
words, which is a contract change for the SRAM device.

### F. Device I/O (deferred by the owner)

`DeviceAddressByteMatch`, `DeviceRouterRequest`, `FifoWriteDecode`,
`FifoReadSelect`, `PixelByteSelect`, `ButtonLevelUpdate`, the device register
and router controls. Listed for device phase P1; not changed now. The device
`Logic` derives from the structural `Logic` helper, so changing that helper
must keep device recipes unchanged until P1.

## 2026-10-28 — Owner choices needed

1. Scope: groups A and B now (recommended), C later or never.
2. `ZeroDetect` for the equality compares (B): yes for the XOR-then-OR ones;
   keep the DRAM XNOR-then-AND compare as regrouped gates.
3. Word widths 5, 6 and 31 (C): add, or leave those groups bit by bit.
4. `ALUResultBit`: retire it (its function becomes word gates) or keep it as
   the visible per-bit cell.
5. Whether `Mux<2,W>`'s own gates become three word gates.

## 2026-10-28 — RV32I ALU regrouped (group A, first family)

Started without waiting for the owner choices, which do not affect it (the
stop hook's standing goal is to keep the non-device plan moving).

- `RV32IALULogic`: three word gates `AND`, `OR`, `XOR` (`ANDGate<32>` …)
  replace 96 one-bit gates, two splitters and three joiners (101 → 3 children).
- `RV32IALUAddSub`: one `XORGate<32>` `B_XOR_SUB` with `SUB` spread by
  `SUB_WORD`, as in `AddSub<W>` (36 → 4 children).
- `RV32IALUShifter`: seven `Mux<2,32>` (`INPUT_REVERSE`, `SHIFT_S0`…`SHIFT_S4`,
  `OUTPUT_REVERSE`) replace 224 `Mux<2,1>`; reversed and moved words are
  splitters/joiners (230 → 25 children). `Mux<2,32>` shares one select
  inverter where 32 `Mux<2,1>` had one each; every path keeps its depth.
- New helper `WordSliceBuilder::wordMux`.
- Checks: 14 focused CTests (all `RV32IALU*` part tests, `ALUHierarchyTest`,
  `RV32ILogicOptimizationTest`, `WordGateSweepTest`,
  `RV32IBuildingBlocksDocumentationTest`, `DelayRuleSelectionTest`,
  `PartDelayTablesTest`) and `RV32IALUSweepTest` pass.
  `part_delay_tables --check rv32i.alu rv32i.alu.logic rv32i.alu.add-sub
  rv32i.alu.shifter` gives 0 differences in unit, embedded and desktop.
  `RV32IProfileToggleSweepTest` first failed because the core now contains
  the selectable `logic.mux.2.32`; an audit case was added and it passes.
- Layouts: `generate-rv32i-building-block-layouts.py --integer-alu-only` was
  run, but it also reset the revision label and the minimum aspect of shared
  types (`ParallelPrefixAdder<32>`, `ZeroDetect<32>`, `Mux<2,32>`,
  `RV32IALUCompare` …) that later tools had set. Only its output for the
  three changed types (type, root and profile scopes, 24 entries) was kept;
  everything else is byte-for-byte the previous file.
  `RV32IBuildingBlockLayoutTest` required every part inside the ALU scenarios
  to have no custom floorplan; `Mux<2,32>` keeps its RV32I floorplan, so that
  one assertion now applies to the ALU's own types only (positions are still
  checked for every part). It passes. Static layout check: 0 problems in all
  373 scenarios (before the merge) and in the seven ALU scenarios (after).
- Site: `rv32i/4.3/alu.md` structure text for the three parts.
- Not changed: the recorded game captures still name the old per-bit parts;
  they are fixed snapshots, to be recaptured after device I/O (owner decision).

## 2026-10-28 — Chapter 2 regrouped (group A)

- `BarrelShifter<W>` (`WordExtended.cpp` `buildShift`): word muxes
  `INPUT_REVERSE`, `SHIFT_d` per stage, `OUTPUT_REVERSE`; the wrapped/fill top
  bits of each stage are a word mux `SHIFT_d_FILL` from four bits up (one and
  two bits stay `SHIFT_d_FILL_i`); the stage carry muxes stay single (a
  chain); `VALID` is a word AND. Children: 8-bit 69 → 36, 16-bit 44, 32-bit 52.
- `CompareSelect<W>`: word muxes `MINU`, `MAXU`, `MINS`, `MAXS`; predicate
  words (flag in bit 0, wiring) and one `Mux<8,W>` `RESULT` (one `Mux<8,1>` per
  bit, as before).
- `ALUResultSelector<W>`: six word ANDs `RESULT_SELECT_i` with broadcast
  enables and five word ORs in the same balanced tree (`RESULT_OR_l_i`).
- `WordBuilder::maskBus` is one word AND (used by `ArithmeticUnit<W>`'s
  `RESULT_MASK` and the shifter's `VALID`); new `WordBuilder::reduceWords`,
  `splitBits`, `joinBits`.
- Checks: 15 focused CTests pass (8-bit unit tests, `ALUContractTest`,
  `ALUFidelityContractTest`, `ALUWidthContractTest`, the three ALU scenarios,
  `ALUHierarchyTest`, `ArithmeticTimingOptimizationTest`,
  `DelayRuleSelectionTest`, `WordGateSweepTest`). `part_delay_tables --check`
  of `word.alu.8`, `word.arithmetic-unit.8`, `word.barrel-shifter.8`,
  `word.compare-select.8` and `word.alu-result-selector.8`: 0 differences in
  unit, embedded and desktop (16/32-bit units are structural only).
- Layouts: `relayout_changed.py` re-laid the automatic type and profile
  scopes of the four unit types at 8/16/32 bits; the new `Mux<2,16>` got the
  server's mux floorplan stored; `refresh_roots.py` pruned and refreshed the
  8-bit scenario roots. Static layout check: 0 problems in the nine Chapter 2
  scenarios.
- Site: `alu/2.1.md` (result mask), `2.3.md` (shifter), `2.4.md` (compare
  select), `2.6.md` (result selector).

## 2026-10-28 — Second batch: RV32I load lanes, multiply/divide, memory parts

- `RV32ILoadByte<0..3>`: 8-bit word ANDs `BYTE`/`HALF`/`WORD` with spread
  size enables, broadcast fill words for the upper bytes, and two word ORs
  `RESULT_0`/`RESULT_1` (the same per-bit reduce order).
- `structural::Logic` (Minimal mode only; the Device I/O recipes use Legacy
  and are unchanged): `mask(bits, enable)` builds one `ANDGate<W>` with the
  enable spread by a Rewire, and `mux` at 4/8/16/32 bits builds one
  `Mux<2,W>` (`WORD_MUX_n`). Both use up the per-bit `NOT_n`/`AND_n`/`OR_n`
  numbers so later child names do not shift. `MultiplyStep<W>` uses `mask`;
  `DivideStep<W>`, the RV32IM decode overrides and any 4/8/16/32-bit `mux`
  call get word muxes.
- `MemoryPayloadMux<4/8/16/32>`: word gates `A`, `B` (spread select and its
  inverse) and `SELECT`; other widths keep the per-bit form.
- `MemoryReadSelector<W,N>`: a tree of `Mux<2,W>` (`READ_l_i`) over the
  words, zero word for missing leaves, word AND `ENABLE` with `READ_VALID`
  spread.
- `CacheLineState` / `CacheDirtyLineState` at 4–32 sets: the per-set
  next-state logic is word gates over the sets (`CLEAR`, `MARK`, `KEEP`,
  `NOT_CLEAR`, `NEXT`, `SELECTED_VALID`, and the dirty path); set decoding,
  the state bits and the final OR trees stay per set.
- `StaticMemorySequencer`: the wait counter's per-bit logic is 8-bit word
  gates (`COUNT_LOW`, `COUNT_RUNNING`, `COUNT_WAITING`, `COUNT_KEEP`,
  `COUNT_LOAD` with one `LOAD` constant word, `COUNT_NEXT`); the borrow chain
  and the zero test stay per bit.
- Not done in this batch: the `Decoder<5>` output enables (32 one-bit output
  pins; regrouping needs a join and a split around one word gate, and the
  decoder is used widely), the RV32I prediction table's write rows, and the
  equality compares (owner choice 2).
- Checks: 40 focused CTests pass (all memory endpoint, controller, arbiter
  and cache tests including `CacheReadTest` and `CacheControllerLockstepTest`,
  the RV32I memory lane/request/stage tests, the multiply/divide step, unit
  and gate-level tests, `RV32IMFiveStageCoreScenarioTest` and two RV32IM
  programs).
- Delay tables: a full `part_delay_tables --clocked --write` in unit is
  byte-identical to the stored file (833 tables). `part_delay_tables --check`
  (with or without `--clocked`) reports the 15 `memory.*.scalable-endpoint.*`
  tables as different in every profile even so: `--check` proves an entry
  differently from the generator for these clocked endpoints. It is a
  limitation of `--check`, not a timing change; the complete `--verify`
  tests are the reference.

## 2026-10-28 — Owner: "decide yourself"; remaining groups regrouped

The owner asked to finish the non-device work and to decide the open choices,
grouping by width where needed. Decisions:

1. Scope: groups A, B and C. Word gates are offered at every width from 2 to
   32 bits (`gateWidth`), so the 5-, 6-, 29- to 31-bit groups regroup too;
   `WordGateSweepTest` now covers all 186 word gates.
2. Equality compares: `CacheTagMatch<32>` (XOR, OR tree, inverter) is now
   `XORGate<32>` plus `ZeroDetect<32>`, exactly the same gates. The request
   tag match and branch recovery (XOR and OR tree, no inverter) and the DRAM
   open-row compare (XOR, inverter, AND tree) regroup their XOR and inverter
   rows only; their trees are reductions and stay.
3. Odd widths: covered by decision 1.
4. `ALUResultBit` is retired: `RV32IALUResultSelect` masks the nine source
   words in word ANDs `MASK_0` … `MASK_8` and combines them in a word OR tree
   `RESULT_…` (the same gates per bit).
5. `Mux<2,W>` keeps its per-bit AND/AND/OR insides (they define the part).

Other regroupings in this step:

- `StructuralNetBuilder::gateBits` / `invertBits`: a word gate between
  Rewires for any bit vector of two or more bits.
- `Decoder<5>`: the address inverters are one `NOTGate<5>` (`NOT_ADDR`) and
  the output enables one `ANDGate<32>` (`ENABLE`, 31 bits zero-disabled)
  between `MATCH_WORD` and `OUT_BITS`; the RV32I `Decoder<5>` floorplan
  places the new parts.
- Memory row decoders (`rowDecodeLines`): the 31–32 address inverters are one
  word NOT (`NOT_ADDRESS`).
- `Incrementer<W,Step>` (power-of-two step): each prefix level is one word
  AND (`PREFIX_<d>`) and the sum one word XOR (`INCREMENT`).
- The RV32I forwarding, hazard and write-back compares (`equal5`): an
  `XORGate<5>` and a `NOTGate<5>` per compare.
- Prediction table write enables: one word AND `WRITE_ROW`.

Verification of this step is recorded with the behavioral gap review's
generic compact forms, which were built and checked together
([behavioral gap review](2026-10-28-behavioral-gap-review.md)).

## 2026-10-29 — Device deferral after the redesign

The [console structural audit](2026-10-28-device-io-architecture.md)
confirms that the former Device I/O deferral is only partly resolved.
`REGISTERS` uses word read-data muxes but scalar three-bit status choices;
`TX_BYTE` contains two Rewires implementing one low-byte slice. The older
Device Logic helper still selects Legacy. Width-3 basic gates are supported,
so status regrouping is possible without extending the mux contract. These
are pending implementation/migration checks, not a fresh correctness finding.
Current WASM build-only counts and the workflow-scope evidence are retained
in that audit; no hardware or timing changes were made in this follow-up.
