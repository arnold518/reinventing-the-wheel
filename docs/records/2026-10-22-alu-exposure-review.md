# ALU external interface and documentation review

Started: 2026-10-22. Status: open (review done; owner direction pending).
Code state: main `alpha` at `88c0a95` plus the uncommitted component
replacement described in the
[closed replacement record](2026-10-22-general-component-retirement.md).

## Summary

The owner is not satisfied with how `ALU<W>` and its units are exposed or
documented. The review finds that the replacement made width a parameter,
but `Operations` still selects two unrelated designs under one set of class
names: eight names have two different pin sets. Most problems in the
interface, contract IDs, menu titles and the Chapter 2 site pages follow from
this. Several factual errors in the pages are listed below. No code or
public page has been changed yet; the next step needs the owner's choice of
naming/contract direction.

## 2026-10-22 — Evidence

Read: `include/modules/composite/{ALU,WordDatapath,ParallelPrefixAdder,Incrementer}.hpp`,
`src/modules/composite/Word*.cpp`, `WordALUValueModel.hpp`, `IntegerALUValue.hpp`,
both Python binding entry points, `visualizer-v2/static/scenarios.json`,
`site-docs/alu/2.1.md`–`2.7.md`, `site-docs/components/word-components.md`,
`site-docs/rv32i/4.3/alu.md` and `mkdocs.yml`.

Probed every `create_word_component` configuration at W=8 (structural) and
behavioral availability at W=8/16/32 with the `build-milestone` Python
module (built after the newest source file). The script was kept in the session's temporary
scratch area; no simulation beyond construction was run.

### Pin sets per unit and operation contract (W=8 shown)

| Unit | `Extended` | `IntegerDatapath` |
| --- | --- | --- |
| `ALU` | A, B, OP[5], C_IN → OUT, ZERO, NEGATIVE, CARRY, OVERFLOW, INVALID_OP | A, B, OP[5] → OUT, EQ, LT_SIGNED, LT_UNSIGNED [, ZERO, NEGATIVE, CARRY_OUT, OVERFLOW] |
| `AddSub` | A, B, SUB, C_IN → OUT, CARRY_OUT, OVERFLOW | A, B, SUB → OUT, CARRY_OUT, OVERFLOW |
| `LogicUnit` | A, B, OP[3] → OUT | A, B → AND_OUT, OR_OUT, XOR_OUT |
| `BarrelShifter` | A, B, OP[3], C_IN → OUT, CARRY | A, B, LEFT, ARITHMETIC → OUT |
| `CompareSelect` | A, B, DIFF, OP[3], CARRY_IN, OVERFLOW_IN → OUT | DIFF, CARRY_IN, OVERFLOW_IN, ENABLE → EQ, LT_SIGNED, LT_UNSIGNED, SLT_RESULT, SLTU_RESULT |
| `ALUFlagUnit` | RESULT, ARITHMETIC, SHIFT, ARITH_CARRY, ARITH_OVERFLOW, SHIFT_CARRY → ZERO, NEGATIVE, CARRY, OVERFLOW | RESULT, ARITHMETIC_ENABLE, ARITH_CARRY, ARITH_OVERFLOW → ZERO, NEGATIVE, CARRY_OUT, OVERFLOW |
| `ALUResultSelector` | 4 results + A, B, 6 enables → OUT | 7 results + A, B, LOCAL_OP[3], HIGH_HALF, OUT_OF_RANGE → OUT |
| `ALUControlDecoder` | OP[5] → 3 selectors, 6 enables, INVALID_OP | OP[5] → LOCAL_OP, HIGH_HALF, OUT_OF_RANGE, ARITHMETIC_ENABLE, SUB, SHIFT_LEFT, SHIFT_ARITHMETIC |
| `Adder` | A, B, Cin → Sum, Cout (contract ignored) | same |
| `ArithmeticUnit` | A, B, OP[3], C_IN → OUT, CARRY, OVERFLOW, INVALID_OP | silently built as extended |
| `ZeroDetect` | A → ZERO (contract ignored) | same |

### Behavioral availability (catalog factories)

| Configuration | Behavioral |
| --- | --- |
| W=8, extended, ripple: every unit | Yes |
| W=32, integer, prefix: ALU (full and core), AddSub | Yes |
| W=32, integer: LogicUnit, BarrelShifter | Yes |
| W=32 prefix Adder; ZeroDetect at 8 and 32 | Yes |
| Any W=16 configuration; extended at 32; integer at 8/16; integer compare/flags/result/control | No |

The documented primary ALU (`word.alu.16.extended.prefix`) is structural-only.

### Interface findings

1. **One name, two designs.** The eight names above change pin sets with
   `Operations`. The integer `CompareSelect` has no A/B/OP and outputs flags;
   the integer `LogicUnit` has three outputs and no OP.
2. **Configuration accepts meaningless values silently.** `normalized()`
   drops adder topology for logic/shift/compare/flags/result, operations for
   adder/zero/arithmetic, and `full_flags` for every non-ALU unit. Only
   extended + `full_flags=false` is rejected. `full_flags=false` produces
   the `,core` / `.core` variant; the field name does not say that.
3. **Pin naming varies across one family:** HalfAdder `Carry`; FullAdder
   `Carry_in/Carry_out`; Adder `Cin/Sum/Cout`; AddSub `C_IN/OUT/CARRY_OUT`;
   extended ALU `CARRY`, integer ALU `CARRY_OUT`; flag units likewise.
   `ParallelPrefixAdder<W>` has a width-dependent pin name (`C7`/`C15`/`C31`).
4. **Opcode maps:** both contracts take a 5-bit OP. The integer map uses 13
   codes, has no INVALID_OP and gives zero for 13–31. The same operation is
   `CLEAR` (extended 0x1E) and `ZERO` (integer 0x0C).
5. **Redundant or uneven types:** `Adder<W>` with prefix topology is only a
   wrapper of `ParallelPrefixAdder<W>`. Extended addition nests
   AddSub → Adder → ParallelPrefixAdder; integer AddSub instantiates
   ParallelPrefixAdder directly. The extended-contract adder is built in
   `WordIntegerAdder.cpp`.
6. **Header exposure:** `ALU.hpp`, `Adder.hpp`, `AddSub.hpp` and
   `ZeroDetect.hpp` are two-line forwards to `WordDatapath.hpp`. The public
   header also exposes `Unit`, `build()`, `family(Unit,…)`,
   `registerComponents`, `buildControl`, `WordComponent`, `PrefixWord`
   and the carry cells in `arithmetic_detail`. The code is densely formatted
   (several declarations per line).
7. **Contract IDs** derive from lowercased class names
   (`word.arithmeticunit.8.ripple`, `word.aluflagunit.8`,
   `word.aluresultselector.32.integer`). `.extended` is explicit only on the
   ALU. 25 families per width plus two control maps: 77 contracts, most
   without a scenario or their own test owner.
8. **Python** exposes both `create_alu` (defined in `RecordedGuestBinding.cpp`)
   and `create_word_component("alu", …)`, with a positional `full_flags` bool.
9. **Parent usage:** RV32I branch-target adders use
   `addNewComponent<Adder<32>>` instead of a family, so a profile cannot
   select their fidelity. The baseline pipeline computes PC+4 with
   `Adder<32>`; the other cores use `Incrementer<32,4>`.
10. **Visualizer menu titles** look mechanically renamed:
    `ALU<8,extended,ripple>Flag Unit`, `ALU<8,extended,ripple>Control Decoder`
    (the decoder has no width), `Zero Detect8`, `Add Sub8`, `Add Sub32`,
    `Arithmetic Unit8`, `Logic Unit8`, `ALU<32> Core Datapath`.

### Site documentation findings

1. **Structure:** extended units are documented by function in 2.1–2.6;
   then 2.7 documents eight of the same names again with different pins.
   An unnumbered top-level "Word components" page before Chapter 2 repeats
   ALU pins/configuration and also covers mux, decoder, register file, M
   units and devices. 4.3.3 documents the core variant again. `ALU<W>` pins
   are therefore described in four places.
2. **Factual errors:**
   - 2.6 overview lists `ALU<W>` behavioral as "No"; the 8-bit extended
     ripple family has one, and the same page's timing rows cite it.
   - 2.6 walkthrough says `ARITHMETIC → ADD_SUB → ADDER → PREFIX_ADDER`;
     the adder child is named `ADD`.
   - 2.7 names nonexistent classes `AddSub<32,integer,prefix>Behavioral`,
     `LogicUnit<32,integer>Behavioral` and `BarrelShifter<32,integer>Behavioral`.
   - Overview footnotes describe behavioral as "the recorded eight-bit ripple
     configuration" on pages whose units have no topology.
3. **Recorded examples mix configurations:** sub-units at 8-bit ripple, the
   ALU and prefix adder at 16-bit prefix, integer units at 32-bit prefix.
   §2.1.3 shows ripple first although prefix is the default; the prefix adder
   comes last (§2.1.6).
4. **Private and process language:** `ExtendedALUValueModel`,
   `WordBehavioral<W, Topology>`, "retains the compact-fidelity checks",
   "This is not an independent arithmetic oracle", long test-inventory tables
   with case counts.
5. **Duplication and length:** the `alu` example table is generated on both
   2.6 and the word-components page; the boilerplate W/behavioral paragraph
   and source footers repeat on every page. 2.1, 2.6 and 2.7 are 4.9k, 3.5k
   and 3.7k words. 2.7 uses `### Tests` without links; 2.1–2.6 use
   `### Source and tests` tables.

Out of scope here but seen: 12 iframes omit `embed=1`; the app detects
iframes itself, so this has no effect.

### Options for the owner

- **A (recommended): one name per contract.** Keep `ALU<W>` (extended,
  teaching) and introduce a separate integer ALU name; make its units
  separate types or private parts. Give each component only the parameters
  it uses, reject the rest, and use one pin-naming convention. Then
  restructure Chapter 2 around these components.
- **B: one ALU with a common unit interface.** Redesign the integer units so
  that only the opcode map differs. This is a larger hardware change and
  alters RV32I Execute timing.
- **C: documentation-only cleanup.** Fix the errors, titles and structure
  without changing the interface. The two pinouts per name remain.

No checks were run beyond the construction probe; nothing was built.

## 2026-10-22 — Is the extended contract needed?

Owner question: whether the extended ("teaching") ALU is needed at all.

- **Users:** outside `modules/composite`, tests, bindings and Chapter 2,
  nothing uses `Operations::Extended`, `ExtendedOp`, `ArithmeticUnit` or the
  extended compare/flag/result units. RV32I/RV32IM Execute uses
  `word.alu.32.integer.prefix.core`. The visualizer's RV32I layout names
  only integer types.
- **Difference:** extended has 29 codes in function groups, a carry input
  (ADC/SBC), INC/DEC/NEG, NOT/NAND/NOR/XNOR, rotations with shift carry,
  EQ/SLT/SLTU predicates, MIN/MAX, CLEAR and INVALID_OP, with a ZERO,
  NEGATIVE, CARRY, OVERFLOW flag set. Integer has the RISC-V 13 operations,
  no carry input, branch comparisons (EQ/LT_SIGNED/LT_UNSIGNED), and optional
  ZERO/NEGATIVE/CARRY_OUT/OVERFLOW (`full_flags`).
- **Footprint:** extended-only structure is `WordExtended.cpp` (195 lines)
  and the extended control map; 7 of the 18 characterized word delay
  contracts are extended-only (`alu.8.extended.ripple`, `arithmeticunit`,
  `logicunit.8`, `barrelshifter.8`, `compareselect.8`, `aluflagunit.8`,
  `aluresultselector.8`, `control.extended`; `addsub.8.ripple` and
  `adder.8.ripple` are shared building blocks). `ExtendedALUValueModel` also
  supplies the 32-bit adder and zero-detect behavioral rules, so it cannot be
  removed wholesale.
- **Assessment given to the owner:** the extended contract has no consumer
  and the roadmap (RV32IM, games, AI accelerators) does not call for a
  flag-based ALU. Its unique teaching content is flags/carry chaining,
  rotations, min/max and invalid-opcode detection. Recommended: build
  Chapter 2 toward the integer ALU the CPU uses, keep the shared building
  blocks (half/full adder, ripple/prefix adder, add/subtract with carry,
  zero detect) and the integer full-flag variant, and retire the extended
  contract. Keeping it is reasonable only if a flag-based CPU or an ISA with
  carry is planned. Owner decision pending.

## 2026-10-23 — Owner proposal: extended ALU in Chapter 2, RV32I ALU in 4.3

The owner proposes keeping the extended ALU as the Chapter 2 ALU and
defining a smaller ALU for the CPU in the RV32I building-blocks section
(4.3). This replaces options A–C above. Proposed exposure, for owner review:

- **Chapter 2:** one operation contract. `ALU<W>(name, AdderTopology)` with
  W = 8/16/32; `Operations`, `full_flags` and `ALUConfig` are removed. Each
  unit takes only the parameters it uses. One header per public component;
  `Unit`, `build()`, `WordComponent` and registration move to `src/`.
  IDs lose the operation segment: `word.alu.<W>.<topology>`,
  `word.add-sub.<W>.<topology>`, `word.logic.<W>`, … (77 → 40 contracts).
  Proposed carry-port convention `C_IN`/`C_OUT`; `ParallelPrefixAdder`
  `C7/C15/C31` becomes `C_SIGN`.
- **RV32I:** `RV32IALU`, fixed at 32 bits and prefix addition, contract
  `rv32i.alu`, with structural and behavioral forms. Its pins are those of
  today's `ALU<32,integer,prefix,core>` (A, B, OP[5] → OUT, EQ, LT_SIGNED,
  LT_UNSIGNED), so Execute wiring and the decoder are unchanged. Its parts
  become RV32I types: control, add/subtract, logic, shifter, compare, result
  select. Identical gates mean identical delay values; only IDs, type
  names, saved layout keys and scenarios migrate.
- **Retired with the integer contract:** integer configurations at 8/16
  bits, ripple and full-flag variants, and the standalone behavioral forms
  of the integer add/subtract, logic and shifter.
- **Open owner choices:** a fixed-32-bit RV32I exception like
  `RegisterFile32x32`; the name (not "compact", which already means
  behavioral fidelity here); OP width (5 now, 4 suffices); reusing Chapter 2
  `AddSub<32>` (changes the overflow gates and RV32I timing); renaming the
  one-bit adder cells' pins.

## 2026-10-23 — Roadmap check: multiply/divide and future ALUs

Owner question: whether GPU/accelerator work would later fold multiply/divide
into the ALU, which would argue for keeping a general ALU.

- RV32IM keeps M outside the ALU: `MulDivUnit<W>` (W = 4/8/16/32) is a
  separate iterative unit with W cycles per operation
  (`docs/reference/rv32im-device-foundation.md`).
- The AI-accelerator plan (`2026-10-14-future-steps-plan.md`, Goal 3) starts
  with a multiply-accumulate processing element (for example signed 8-bit
  inputs, 32-bit accumulation) and a small systolic array: a separate unit,
  not an ALU extension.
- The GPU goal is unscheduled and has no chosen ISA; the plan names SIMT
  lanes, active masks and memory concurrency, and Vortex (RISC-V SIMT) as a
  reference. No ALU design is specified.
- Assessment given: no current plan places multiply/divide inside an ALU.
  A future lane datapath would combine add/logic/shift/compare blocks with
  a single-cycle or pipelined multiplier; the current iterative multiplier
  does not provide one result per cycle. That argues for general building
  blocks (including a future array multiplier) rather than a general ALU.
- Found in passing: `rv32im-device-foundation.md` still says legacy
  `ALU<8,extended,ripple>`/`ALU<32,integer,prefix>` wrappers keep their own
  maps and IDs; the replacement removed those wrappers. To correct with the
  ALU documentation work.

## 2026-10-23 — Owner question: one general ALU with restricted operations

Discussed a single ALU whose operation set, outputs and opcode map are
construction parameters, with the extended and RV32I ALUs (and a future GPU
lane) as named configurations. Findings for the decision:

- A reusable compact form already exists: `createGateListPart`
  (`src/components/GateListPartBehavioral.hpp`) runs a family's own
  structural gate list as one part. Any generated configuration could
  therefore get an exact four-state compact form without a hand-written
  value model.
- The two current designs select results differently (extended: local
  selector per group plus group enables; integer: one flat selector) and
  compute overflow differently. One generator means at least one of them
  changes gates, so its delay tables, recorded examples, and for RV32I the
  certified clock checks must be regenerated or rerun.
- The configuration space cannot all be characterized; only named
  configurations would get contract IDs, delay tables and scenarios.
- Pins would follow rules from the configuration (for example `C_IN` only
  when a carry-using operation is enabled), as `Mux` inputs follow fan-in.

## 2026-10-23 — Owner decision: `ALU<W>` and `RV32IALU`

The owner chose two named ALUs: the extended `ALU<W>` in Chapter 2 and a
separate `RV32IALU` in the RV32I building blocks (4.3). This replaces the
general configurable-ALU option. Checked facts for the interface:

- One ALU serves every core: `RV32IMExecuteStage` contains the unchanged
  `RV32IExecuteStage` (`INTEGER`), whose `RV32ISingleCycleExecuteStage`
  (`DATAPATH`) holds the ALU; `MulDivUnit<32>` sits beside it with a result
  mux and `EXECUTION_WAIT`.
- The RV32I decoder, control model, pipeline rules and oracle use the
  integer opcode constants (`IntegerOp::ZERO`, `PASS_B`, `SUB`, …); they move
  with the RV32I opcode namespace.
- The RV32I compare part uses Chapter 2 `ZeroDetect<32>`; its add/subtract
  part uses `ParallelPrefixAdder<32>` directly.

The proposed interface given to the owner (pending confirmation of the open
choices listed in the reply): Chapter 2 keeps today's extended pins and
codes, loses the operation/flag parameters, and uses IDs without the
operation segment; `RV32IALU` keeps today's core pins, gates and timing
values under `rv32i.alu`, with its six parts as structural-only RV32I
families. Retired: the integer full-flag ALU, integer flag unit, integer
configurations at 8/16 bits and with ripple carry, and standalone
behavioral forms of the integer add/subtract, logic and shifter.

## 2026-10-23 — Owner question: keep the adder-topology parameter?

- Outside `modules/composite`, tests and bindings, nothing selects
  `AdderTopology::Ripple`. Production adders (RV32I, M-unit helpers) use
  prefix.
- Ripple appears only in Chapter 2 recordings (`adder-ripple-w8`,
  `adder-ripple`, `addsub-extended-ripple-w8`,
  `arithmetic-extended-ripple-w8`, `alu-extended-ripple-w8`,
  `alu-extended-ripple-w16`) and in the 8-bit behavioral forms and delay
  tables, which are all ripple configurations.
- Pages 2.2–2.5 mention ripple only through the shared "recorded eight-bit
  ripple configuration" sentence, although their units contain no adder.
- `Adder<W>` with prefix topology is already a wrapper of
  `ParallelPrefixAdder<W>`, so the two designs exist as separate classes.
- Proposed to the owner: keep the ripple/prefix lesson at the adder level
  as two named components (`RippleCarryAdder<W>`, `ParallelPrefixAdder<W>`)
  and fix `AddSub`, `ArithmeticUnit` and `ALU` to prefix carry. Cost: the
  8-bit add/subtract, arithmetic-unit and ALU behavioral forms and delay
  entries are recharacterized at prefix; the ripple ALU recordings retire.

## 2026-10-23 — Owner decision: no adder-topology parameter

The owner dropped the topology parameter. The two adders are compared in
dedicated performance tests in their sections, and every ALU uses the fast
(parallel-prefix) adder. `AddSub<W>`, `ArithmeticUnit<W>` and `ALU<W>`
therefore have only W; ripple carry remains as its own adder component.
Remaining confirmations are listed in the reply of the same date.
