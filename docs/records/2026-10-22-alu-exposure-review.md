# ALU external interface and documentation review

Started: 2026-10-22. Status: closed (2026-10-23).
Code state: implementation and acceptance on main `alpha`, based on checkpoint
`971ab53`; the source, integration and documentation below are included in
this change. Git history supplies its commit identity.

## Summary

Chapter 2 exposes width-only components at 8/16/32 bits with prefix arithmetic;
`RV32IALU` is a separate fixed-width contract with six parts. The headers,
bindings, parents, scenarios, saved identities and public references follow
that interface. Focused native checks, serial native/WASM builds, all 156
example groups, strict package/preview builds and nine selected web checks
pass. The 18 changed delay-table families verify in all three profiles, and
compiled native/WASM snapshots match at 809 entries per profile. Migrated 44
layout scopes; 367 scenarios cleared the complete scan, and the remaining
fetch root passes its final focused recheck. Port 8893 serves the fresh
package with the timing-cache fix loaded. Full suites and remaining game
acceptance are outside this topic.

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

## 2026-10-23 — Owner decisions and implementation

The owner accepted every recommendation (fixed-width `RV32IALU` exception,
5-bit OP, carry pin renames, `ZERO` → `CLEAR`, no `ARITHMETIC_ENABLE` on the
RV32I control part, deferred `AddSub<32>` reuse, branch/PC adders through
families, `RippleCarryAdder<W>`/`ParallelPrefixAdder<W>` names, four new
compact adder forms, checkpoint commit first) and directed that the Chapter 2
section describe only `ALU<W>`. Minimal tests were requested.

The prior uncommitted work was committed locally as `971ab53` (one
checkpoint commit: STATUS, CHANGELOG and several sources mixed topics).

Implemented:

- **Chapter 2 interface.** One public header per component
  (`ALU.hpp`, `AddSub.hpp`, `ArithmeticUnit.hpp`, `LogicUnit.hpp`,
  `BarrelShifter.hpp`, `CompareSelect.hpp`, `ZeroDetect.hpp`,
  `ALUFlagUnit.hpp`, `ALUResultSelector.hpp`, `ALUControlDecoder.hpp`,
  `RippleCarryAdder.hpp`, `ParallelPrefixAdder.hpp`) plus the shared
  `WordComponent.hpp` base. `WordDatapath.hpp`, `Adder.hpp`, `ALUConfig`,
  `Operations`, `AdderTopology` and `full_flags` are gone; construction,
  registration and compact factories are private (`WordRecipes.hpp`). Each
  component takes only W. IDs: `word.alu.W`, `word.add-sub.W`,
  `word.arithmetic-unit.W`, `word.logic-unit.W`, `word.barrel-shifter.W`,
  `word.compare-select.W`, `word.alu-flag-unit.W`,
  `word.alu-result-selector.W`, `word.zero-detect.W`,
  `word.ripple-carry-adder.W`, `word.parallel-prefix-adder.W`,
  `word.alu-control-decoder` (34 contracts, previously 77).
- **Carry pins.** Word adders use `C_IN`/`OUT`/`C_OUT`, the prefix adder adds
  `C_SIGN`; `AddSub` `CARRY_OUT` → `C_OUT`. One-bit cells unchanged.
- **Prefix carry everywhere above the adder.** `AddSub<W>` now contains
  `ParallelPrefixAdder<W>` directly (`ADD`), so `ArithmeticUnit<W>` and
  `ALU<W>` use prefix carry. The 8-bit compact forms of these three were
  recharacterized; compact forms were added for the ripple adder at 16/32
  and the prefix adder at 8/16.
- **`RV32IALU`.** Fixed 32-bit `RV32IALU` with parts `RV32IALUControl`,
  `RV32IALUAddSub`, `RV32IALULogic`, `RV32IALUShifter`, `RV32IALUCompare`,
  `RV32IALUResultSelect` (`rv32i.alu`, `rv32i.alu.*`), moved from the former
  integer recipes with identical gates. Opcodes are `rv32i::ALUOp`
  (`CLEAR` = 12); the shared value rule is `rv32i::alu_value`. Python:
  `create_rv32i_alu`; `create_alu` and the configuration arguments of
  `create_word_component` are removed.
- **Parents.** Execute requests `families::rv32iALU()`; the single-cycle
  branch-target adder, the baseline PC+4 adder and the prediction target
  adder request `families::parallelPrefixAdder(32)`. The architecture
  profile now selects `rv32i.*` only. Profiles that prefer compact parts
  everywhere (the RV32I architecture view and depth-limited test profiles)
  now get the compact 32-bit prefix adder for those three adders.
- **Retired:** the integer full-flag ALU and flag unit, integer 8/16-bit and
  ripple configurations, `ALUExtendedWidth8Test`,
  `ALUWidth16RippleScenarioTest`, `FlagsIntegerWidth32Test`,
  `ALUIntegerWidth32Test` and their aliases. Tests and aliases were renamed
  (for example `AddSubWidth8Test`/`add-sub-w8`, `RV32IALUTest`/`rv32i-alu`).
- **Found while building:** `tests/cpp/RV32ILogicOptimizationTest.cpp` did not
  compile at the checkpoint (it named a removed builder class); fixed.
  `tests/web/RV32IBuildingBlocksBrowserTest.py` used a page slug broken by the
  earlier mechanical rename; fixed.

## 2026-10-23 — Progress audit and remaining acceptance

This replaces the earlier summary and project-status statement that nothing
was implemented. The implementation section above matches the current
headers, recipes, parent wiring, registry, bindings and public pages. Chapter
2 now has pages 2.1–2.6; 2.7 is removed, and §4.3.3 specifies `RV32IALU` and
its six parts. `ALU<8>` has both fidelities; `ALU<16>` and `ALU<32>` remain
structural-only. Both named adders have compact forms at all three widths.

Existing evidence inspected, rather than rerun in this audit:

- `build-milestone/alu-exposure-build5.log` reaches 100%, including the native
  runner, Python module and timing tools. Its configuration has Python
  deployment disabled. Thirty individual ALU/component logs in the prior
  session scratch directory end in PASS, including width, exhaustive
  contract, fidelity and RV32I slice checks.
- The three `delays.{unit,embedded,desktop}.log` files report 18 characterized
  tables each; the three tracked generated profiles contain the new IDs.
  These logs establish scoped regeneration, not a fresh complete-file timing
  verification.
- The prior ten-check CTest selection passed eight checks (including both
  arithmetic timing checks, scenario catalog, RV32I documentation and native
  bindings) and failed `DocumentationExamplesTest` and
  `TimingTableDocumentationTest`.
- The prior MkDocs log reports a successful documentation build. The saved
  layout audits report 6,420 problems across 77 affected scenarios, both for
  the working layout and a checkpoint comparison (368 versus 372 scenarios
  total). The count alone does not establish which problems predate this
  refactor; that attribution and current layout acceptance remain open.

Fresh checks in this audit:

```sh
ctest --test-dir build-milestone \
  -R '^(ALUWidthContractTest|RV32IALUTest|ALUHierarchyTest|TimingTableDocumentationTest)$' \
  --output-on-failure -j1
```

The three ALU checks passed. `TimingTableDocumentationTest` failed at
`test_component_timing_stands_alone`: 16 timing paragraphs were found while
the test expects 17. Total time was 22.21 seconds; CTest exited 8.

A separate current native export of `ALUWidth8ScenarioTest` was passed through
`mkdocs_examples.render_table` with the metadata mutations from
`DocumentationExamplesTest`. Negative time, bad row index, empty checkpoint
and missing inputs were rejected. Changing the first checkpoint from
`case-0` to `row-8` was accepted, reproducing the failing negative test:
`row-8` no longer supplies the duplicate label that this fixture expects.

Remaining work:

- Resolve these two documentation checks and rerun their focused selection.
- Reconcile layout failures against the checkpoint, then verify affected
  current layouts and browser scenarios.
- Rebuild the WASM/site package and regenerate examples for the new registry
  and IDs; run the renamed ALU/RV32IALU parity and relevant browser checks.
  The inspected WASM module predates the current interface sources, so prior
  portable acceptance does not verify this refactor.
- Finish migration cleanup, including the old
  `ALU<32,integer,prefix>.OP` name in
  `docs/reference/rv32i-single-cycle-documentation-notes.md`, five trailing
  blank-line findings from `git diff --check HEAD`, and
  `CHECKS_PLACEHOLDER` in the pending changelog entry. Commit after acceptance.

No implementation or public-page changes were made by this audit. Only this
record and the living status were updated. No full suite, current WASM/browser
run, deployment or published-site check is claimed.

## 2026-10-23 — Acceptance resumed

The owner directed completion after the progress report. Repaired the
metadata test using an actual duplicate checkpoint from the exported rows
instead of the retired `row-8` naming convention; changed the timing paragraph
inventory from 17 to 16 after removal of the duplicate ALU section. The two
documentation checks and `WebTestRunnerTest` pass (3/3, 87.04 seconds).
Corrected the internal single-cycle note's ALU and CLEAR names, the word-page
exception for the width-independent decoder, the adder browser check's section
names/anchors, and the five trailing blank lines reported by the audit.

The saved layout audit's 6,420 problem lines match its checkpoint comparison
exactly after normalizing profile fingerprints. Current topology and the
parent revision's actual type names identify stale keys from the earlier
width replacement: child types such as `Mux2to1`/`CacheTagMatch32` changed,
but the containing cache/prediction layout hashes still used those names.
The pruning tool also compacted bracketed strings as if they were arrays,
inserting spaces into SRAM/FF storage keys. Fixed that formatter; a temporary
fixture verifies that pruning leaves bracketed keys and string values intact.

Migrated 43 saved scopes using exact prior/current child signatures and
canonical storage keys, preserving existing geometry and current canonical
overrides. Added the missing fetch-stage scenario fallback from its prior
root placements and current type defaults. The migration report and original
working layout are retained under `build-milestone/alu-acceptance/`; important
findings are recorded here rather than relying on those ignored files alone.
Active layout/palette/profile/scenario JSON contains no retired ALU operation
or topology IDs. The broader layout checks and fresh package are in progress.

### Layout verification and package rebuild

`SRAMLayoutTest` and `CacheLayoutTest` pass (4.25 and 73.92 seconds). The
complete `VisualizerStaticLayoutTest` scanned all 368 public scenarios in
561.54 seconds and reduced 6,420 findings to two, both the fetch root's
`PC_PLUS_4`/`NEXT_PC_MUX` placements. The browser seeds a canonical root
fallback, while the native lookup and static audit read the exact profile
key. Materialized `rv32-i-fetch-stage@d6de8e32d1e34b33` using the restored
fallback, bringing the scope migration total to 44. A direct focused
`tools/check-static-layouts.py rv32-i-fetch-stage` recheck passes with zero
problems; the other 367 scenarios were unchanged after their complete scan.
The initial three-check CTest selection therefore exited 8; its two remaining
findings are resolved by this final focused check, rather than hidden by
claiming that the original full registration passed.

Native topology inspection also corrected the adder browser smoke test's
remaining ripple-era component counts: prefix-based `AddSub<8>` has 156
components and `ArithmeticUnit<8>` has 897. A serial native rebuild, scoped
18-table checks in each timing profile, and a full isolated WASM/site build
are running. The WASM build retains the configured O1 link setting and bounded
compiler launcher; example generation selects all groups in isolated processes.

## 2026-10-23 — Current package and local preview recovery

The serial native rebuild completed with Python deployment disabled. The 18
changed table families pass `part_delay_tables --check` in each of `unit`,
`embedded` and `desktop`, with zero differences. A fresh WASM build and full
isolated example export completed: all 156 current example groups verify, and
the combined strict MkDocs/site build passes. The compiled native and WASM
delay snapshots are byte-identical, with 809 entries per profile (SHA-256
`7f64ce04e95a9d1e8502034519e13717452bc097f2a0f771f6dd4c7d4c68c4dd`).
The 18-table checks are scoped structural recharacterization checks; the
snapshot comparison verifies all compiled entries, not a new gate analysis of
all 809 entries.

The three portable checks pass: `RV32IALUWasmParityTest`, `ALUWasmParityTest`
and `VisualizerScenarioWasmParityTest`. The first six browser attempts failed
before execution because `/home/arnold/venv/bin/python` lacks Playwright; they
are being rerun using `/usr/bin/python3`, which has the installed browser
package. The runner's initial localhost socket attempt was sandbox-blocked;
its retry runs outside the sandbox. These environmental failures do not count
as product acceptance.

The owner reported port 8893 unavailable. The service was running, but HTTP
requests timed out while its rebuild loop failed on the newly named
`word.ripple-carry-adder.8.behavioral` table. The timing hook had cached the old
generated tables for the lifetime of `mkdocs serve`. Its packaged simulator
also predated this interface change. Refreshed `build-wasm/pages/simulator`
from the new package after verifying all 28 file hashes, retained the prior
package under `build-wasm/alu-preview-refresh/`, and restarted the existing
service without changing its unit or memory cap. Its strict build completes
in 89.45 seconds. Live HTTP now passes six reference/simulator routes and
26 non-HTML simulator asset hashes; HTML retains preview script injection.
The service's published GitHub Pages site was not changed.

The timing hook now clears its generated-table cache at each MkDocs build,
so subsequent preview rebuilds can follow renamed or regenerated tables.
A focused regression replaces the table files between two builds and checks
that the new key renders and the retired key is refused. Its first fixture
attempt omitted the no-gate inventory; after supplying that file, the timing
documentation registration passes all seven methods (4.22 seconds). Browser
acceptance remains in progress.

## 2026-10-23 — Browser acceptance follow-up

Five of the six browser registrations pass on the current package:
`AdderDocumentationBrowserTest` (24.68 seconds),
`RV32IBuildingBlocksBrowserTest` (295.71 seconds), `SRAMLayoutBrowserTest`
(37.02 seconds), `VisualizerScenarioBrowserTest` (22.76 seconds) and
`VisualizerTimingProfileBrowserTest` (8.83 seconds). The building-block check
verifies all six public pages and 15 live recordings, including all 447
shifter checkpoints, hierarchy geometry and the whole `RV32IALU`.

`VisualizerWasmBrowserTest` stopped on a stale camera fixture that still
requires a `HalfAdder` inside the now-prefix `ArithmeticUnit<8>`. Replaced
that fixture and the matching `ALU<8>` fixture with the named add/subtract,
prefix adder, carry-combine and XOR components. Reviewed later assertions
before retrying: the six arithmetic embeds now include the prefix recording,
all 731 Chapter 2 rows are counted, iframe selectors allow `embed=1`, and the
structural `ALU<16>` recording is checked separately from the page's compact
`ALU<8>` description. A focused retry is in progress.

### Profile-layout fallback correction

The first focused retry reached the live `ALU<8>` API probe after passing all
731 reference rows, camera bounds and structural/behavioral switching. It
then dereferenced an absent exact-profile root key. Source review of
`normalizeLayoutPayload` confirms that explicit fidelity overrides get a new
fingerprint and the UI seeds their root placement from the scenario-wide
layout. Updated the raw-payload assertion to use that same documented fallback
while still requiring all seven groups and their minimum widths. The final
focused retry is in progress; no runtime/layout behavior was changed for
these two fixture failures.

## 2026-10-23 — Final browser acceptance

`VisualizerWasmBrowserTest` passes in 197.03 seconds after the profile-fallback
fixture correction. Its report verifies all six arithmetic embeds and all
731 Chapter 2 example rows, 32 camera-bound checks, compact controls at
960/640/360/320 pixels, browser layout/profile persistence, live ALU fidelity
switching, one shared prefix adder with no ripple full-adders, the seven
Chapter 2 groups and six RV32I groups. There are no page errors. Combined
with the five earlier browser passes and three portable passes, all nine
selected web registrations now pass. No broader native or web suite is
implied by that selection.

The fresh package's layout, scenario metadata and worker source are byte-for-
byte equal to the tracked inputs. The preview is being restarted once more
to load the tested per-build timing-cache reset; its final strict build and
live route/asset check remain the last acceptance step.

## 2026-10-23 — Preview verification and closure

The preview restart loads the per-build timing-cache reset. Its final strict
MkDocs build passes in 53.05 seconds, and the existing service is active on
port 8893 with its 768 MiB/no-swap cap unchanged. The final HTTP smoke passes
six reference/simulator pages, verifies the iframe live-reload guard on their
HTML, and matches all 26 non-HTML simulator assets to the current package.
Its first extra HTML assertion incorrectly expected an internal timing key
that selected-row output does not expose; checking the rendered component
names corrects that smoke fixture, with no service or page change needed.

The status and changelog now describe the completed interface and its scoped
acceptance. All work requested for this topic is complete. No full native/web
suite, new complete-file gate reanalysis, remaining phase-6 game campaign,
push or published-site verification is claimed. The earlier failed checks
and their corrections remain above. This record is closed.
