# Realistic leaf delays and gate-level memory control

Started: 2026-10-16. Status: open. Code state: `alpha` at `1d861c0` with
uncommitted cache and memory-container-retirement work in the worktree.

## Summary

Follows the closed [memory timing realism review](2026-10-16-memory-timing-realism-review.md).
The owner decided to give every **leaf** part realistic delays, with every
behavioral part still taking its timing from its structural form, and to
rebuild the memory control now hidden in C++ as gates. Owner decisions: one
fixed delay per gate type (1A) and self-timed memory devices signalling
completion (2A); every other decision was delegated and is recorded below.
Profiles `unit`, `embedded` (65 nm, default) and `desktop` (32 nm) live in a
C++ timing-profile file. Execution is on branch `realistic-timing` and
reported in the [progress log](2026-10-16-realistic-timing-progress.md).
Phases 1–6 and 7a–7b are done (7b added burst reads so that cache lines
fill in one burst, a change made in phase 7 after the cache plan's Step 1
showed word-by-word fills). The latest plan check (2026-10-17, in the
progress log) found Step 1's order holding with burst fills; the cache
experiment comes next, using `fill=burst`, then phase 8 (documentation,
follow-ups, full suite).

## 2026-10-16 — Scope and plan

### Rule

Only leaf parts carry hand-set delays. A part with a structural form is never
given its own timing; its behavioral twin's `PartDelays` table is regenerated
by `part_delay_tables` from the gate form, as the `AGENTS.md` fidelity rules
require.

### Leaf inventory

Found by listing every `BasicComponent`/`memory_device::Primitive` subclass
and checking whether its family has a structural form.

| Group | Parts | Current delay | Plan |
| --- | --- | --- | --- |
| Logic gates | `NOTGate`, `ANDGate`, `NANDGate`, `ORGate`, `NORGate`, `XORGate` (`src/modules/basic/Gate.cpp`) | 1 each | Realistic per-type delays (decision 1) |
| Wiring | `BitSplitter` (1), `BitJoiner`, `Rewire`, `ConstantValue`, `ClockGenerator` (0) | as shown | 0: wires carry no logic delay. Check for zero-delay ordering problems that the splitter's 1 tick may have hidden |
| Memory electrical stand-ins | `SRAMCellAccess`, `SRAMBitlinePair`, `SRAMSenseAmplifier`, `StaticReadColumn`, `MemoryOutputBuffer`, `DRAMCell`, `DRAMSenseColumn`, `NORFlashCell`; ROM cells are constants | 1 each | Delays depending on geometry: wordline length (columns), bitline length (rows); DRAM sensing slower than SRAM |
| X/Z detection | `RequestKnownBits`, `KnownCacheRequest` | 1 | Simulator metadata; give the delay of the equivalent gate depth |
| Hidden digital logic | `SRAMAccessControl` (0), `DRAMCommandControl`, `FlashControl`, `ROMAddressControl`, `NativeSequencer` | 1 or 0 plus C++ timers | Replace by gate-level circuits; their timing then comes from gates |

Every other `BasicComponent` is a behavioral twin (ALU, adder, shifter, mux,
decoder, register, state bit, FF word/bank/device, SRAM/DRAM/ROM/NOR
compact devices, memory controller and fused endpoint, RV32I stages and
cores). `ClockedStateBit`, `Mux2to1`, decoders and `DFlipFlop` are already
gate-built. `RV32IReferenceCore` is the instruction oracle and has no timing
role.

### Open decisions (recommendation first)

1. **Gate delay model.** Recommended: one fixed delay per gate type from the
   logical-effort method at a fan-out of four, with a finer tick (one
   logical-effort unit). Rough ratios: inverter 5, NAND2 about 7, NOR2 about 9,
   AND2 about 8 (NAND plus inverter), OR2 about 9 (NOR plus inverter), XOR2 to
   be sourced. Alternative: delay grows with the number of driven inputs,
   which then needs buffer trees on broadcast nets throughout the CPU and
   memories. Values must be cited (for example Sutherland, Sproull and
   Harris, *Logical Effort*) before adoption.
2. **Memory timing source.** Recommended: each device times itself
   structurally (gate delay chains; an SRAM replica bitline sized to the row
   count) and raises a completion signal; the gate-level controller waits for
   it and hands the result to the next clock edge. Alternative: the controller
   counts a configured number of cycles per step, like a real DRAM controller.
3. **Calibration reference.** Recommended: a small embedded system with
   on-chip SRAM, external DRAM and NOR flash (main memory tens of CPU cycles
   away). Alternative: modern desktop ratios (hundreds of cycles; far longer
   simulations). Target ratios to be taken from named datasheets.

### Phases

1. **Leaf gate and wiring delays.** Change the six gates and the wiring
   parts; regenerate `DFlipFlopTiming` constants, all `PartDelays`,
   `DelayRuleTest` floors and certified clocks. Function must not change;
   only timing. All published period and tick results become historical.
2. **Gate-level memory control.** Shared delayed controller (reusing the
   gate request check, a one-hot command state machine, DRAM open-row
   register and comparator, refresh counter, response capture), then
   `DRAMCommandControl`, `SRAMAccessControl`, `FlashControl` and
   `ROMAddressControl`. Remove the duplicated fixed waits and the flat guards.
   Waits for the container-retirement work to land, since it edits the same
   files.
3. **Geometry-dependent arrays.** Wordline, bitline and sense delays from
   rows and columns; the self-timing in phase 2 follows them.
4. **Calibration.** Fit device ratios to the chosen reference; model memories
   above the 4,096-cell structural limit as banks of small arrays plus global
   wiring instead of the current per-doubling extrapolation.
5. **Cache.** Drive tag and data devices directly instead of through full
   endpoints; parallel tag/data read and parallel ways.
6. **Follow-up.** Regenerated tables and clocks, removed self-timed delay-rule
   exceptions, public component pages, visualizer layouts, and reruns of the
   published RV32I and memory results. Optional DRAM banks, bursts and
   off-chip latency only if the cache study needs them.

Each phase: structural change first, behavioral twin checked against it,
focused checks between fixes, one full serial suite per batch (about 40
minutes, plus a similar rebuild after a core header change).

Verification this date: inventory from source (`grep` of all
`BasicComponent` and `Primitive` subclasses, family registrations). No code
changed; nothing built or run.

## 2026-10-16 — Owner decisions 1A and 2A; timing data in one place

The owner chose **1A** (one fixed delay per gate type, logical-effort based,
finer tick) and **2A** (each memory device times itself structurally and
raises a completion signal that the gate-level controller waits for).
Decision 3 (embedded versus desktop reference) is deferred. The owner
proposed keeping every constant delay in one configuration file or C++ data
file so that references can be switched.

Proposed design, for confirmation:

- A C++ data file of named **timing profiles**. Each profile lists every leaf
  delay: the six gates, wiring (0), the memory electrical stand-ins with
  per-row and per-column terms, and any off-chip delay part. Structure stays
  the same across profiles; only leaf numbers change, so behavioral tables
  can still be generated from the gate forms.
- Profiles: `embedded` and `desktop` for decision 3, and `unit`, which
  reproduces today's 1-tick timing so published results remain reproducible.
- A profile is chosen per build (through `BuildContext`, with a default for
  context-free construction) and recorded in the `BuildManifest` and
  experiment provenance, like the `BuildProfile` fingerprint.
- `part_delay_tables` generates tables per profile; certified clocks are kept
  per profile. Full tests run on the default profile, with focused checks on
  the others.
- A C++ data file is preferred over a runtime file: it builds into the native
  and WebAssembly targets unchanged, is type-checked, and the generated tables
  must be rebuilt for each profile anyway.

Gate delays do not depend on decision 3; phase 1 can start before it is made.

## 2026-10-16 — Further considerations before memory realism

Collected at the owner's request; none decided yet.

**Decide first**

1. *Clean baseline.* The worktree holds uncommitted work from several
   sessions (cache, container retirement, Pages readiness). Commit it before
   phase 1, so timing changes are reviewable and reversible.
2. *Unit mapping.* Each profile needs a stated tick length (picoseconds) and
   process node so datasheet nanoseconds convert to ticks; gate delays then
   follow from the node.
3. *Long times are not gate chains.* A DRAM access of tens of nanoseconds is
   thousands of gate delays. Building it as inverter chains would be
   artificial and slow to simulate. Under decision 2A, put long analog and
   off-chip times in electrical stand-in leaves (sense time from bitline
   length, an I/O pad/board-trace part) whose delays come from the profile;
   use gate chains only for short on-chip self-timing such as an SRAM replica
   bitline.
4. *Burst transfers and DRAM banks.* A cache line fill as one burst instead
   of one request per word changes results greatly. Adding bursts later would
   change the shared REQ/RSP contract and force recalibration, so decide
   before calibration.
5. *Large FF memory.* A 256-KiB zero-wait FF memory cannot be realistic.
   Either keep it as a labeled "ideal memory" or limit FF to small sizes; the
   published FF baseline depends on it.

**Technical risks to test early**

6. *Transport delay.* `BasicComponent::_updateOutputWire` schedules every
   output change at `t + delay` and never cancels an earlier one, so pulses
   of any width propagate. Real gates swallow pulses shorter than their delay.
   With unequal gate delays, glitches become common: more events, and
   possible misfiring of latches, pulse generators and change detectors.
   Decide between an inertial-delay option and glitch-free self-timed design.
7. *Self-timed to clocked crossing.* A device's completion signal arrives at
   any time relative to the clock. There is no metastability model; a
   realistic two-flip-flop synchronizer costs one or two cycles. The timing
   analyzer currently handles self-timed behavior only through primitives
   flagged `SelfTimed`; gate-level delay chains and loops need checking.
8. *Size scaling above the structural limit.* Real large memories are many
   small subarrays plus global wiring; a 64×64 subarray fits the 4,096-cell
   limit. Scalable devices need a size formula fitted on small structural
   sizes with a holdout, replacing per-doubling extrapolation.

**Accepted simplifications to state**

9. One nominal delay per leaf: no rise/fall difference, no process corners
   (hold checks use the same values); no wire delay in general logic, only in
   memory stand-ins.

**Bookkeeping**

10. Hard-coded timing: tests asserting exact ticks or cycles,
    `DFlipFlopTiming.hpp`, clock periods written into tests and scripts
    (100, 102, 166, 80, 4000), phase/guard minimums, and recorded site
    example tables. Derive them from the profile; choose a default test
    profile (`unit` for regression, a realistic profile for new checks).
11. Refresh and retention: the default `retention_ticks` of 10^9 means data
    never decays in practice; use realistic refresh-interval and retention
    ratios.
12. Host cost: glitches, gate-level controllers and long flash program/erase
    waits (the clock keeps ticking) add events; set budgets.
13. Sources: cite the gate-delay reference and one named datasheet per
    technology in the profile file.
14. Coordination: wait for the concurrent memory and Pages sessions before
    editing shared memory or core headers, and never build in parallel.

## 2026-10-16 — Decisions on the considerations (delegated by the owner)

The owner asked the assistant to decide. These replace the open items in the
previous section; each can be revisited with evidence.

1. **Baseline.** Commit the current worktree before phase 1, once the
   concurrent Pages and container-retirement sessions have finished; do not
   commit other sessions' work while they are active.
2. **Unit.** Realistic profiles use **1 tick = 1 picosecond**. Gate delays
   follow from each profile's process node; datasheet nanoseconds convert
   directly. The `unit` profile keeps today's abstract tick. Parameter limits
   that assume small tick counts (for example `retention_ticks` ≤ 10^9) must
   be widened.
3. **Long times.** Analog and off-chip times live in electrical stand-in
   leaves: sense time grows with bitline length; a new I/O pad/board-trace
   leaf carries the off-chip delay. Gate delay chains are used only for short
   on-chip self-timing (SRAM replica bitline, pulse generation).
4. **Bursts and banks.** Deferred to the cache phase. This corrects the
   previous section: leaf delays do not depend on the protocol, so adding a
   burst read later reuses the open-row timing and needs reruns, not
   recalibration. One DRAM bank until contention is measured.
5. **Large FF memory.** No special "ideal" case in realistic profiles: FF
   memories follow the same size scaling as other technologies. The
   published FF baseline stays reproducible under the `unit` profile.
6. **Short pulses.** Leaf gates get **inertial delay**: a new output value
   replaces a pending, not-yet-published one, so pulses shorter than the
   gate's delay are swallowed (the usual HDL gate rule). With every delay 1
   tick no pulse can be shorter than the delay, so the `unit` profile is
   unaffected; the delay twin used by behavioral parts must apply the same
   rule. Verify on the unit profile first.
7. **Done into the clock.** The gate-level controller captures the device's
   completion signal through a two-flip-flop synchronizer and accepts its
   one-to-two-cycle cost. Metastability is not modeled; the timing analyzer
   must treat the first synchronizer flip-flop as an asynchronous capture.
8. **Size scaling.** A large memory is a grid of subarrays no larger than the
   structural limit (for example 64×64 cells) plus global wiring whose delay
   grows with the grid's side, represented by a wire leaf. Fit and check the
   formula on small multi-subarray structural memories with a held-out size;
   cross-check trends against the CACTI cache/memory model.
9. **Simplifications accepted.** One nominal delay per leaf; no rise/fall
   difference or process corners; wire delay only in memory stand-ins and the
   off-chip leaf.
10. **Defaults.** Existing tests run on `unit`, so phase 1 does not rewrite
    them; new memory checks and experiments use realistic profiles. Clock
    periods in tests and scripts come from per-profile certification. Site
    examples move to the default realistic profile after calibration.
11. **Refresh.** Use realistic ratios (for example a 7.8 µs refresh interval
    and 64 ms retention in the chosen DRAM's datasheet).
12. **Budgets.** Set event and host-time budgets per profile in the pilot runs;
    long flash program/erase runs get an explicit budget.
13. **Sources.** Gate delays: *Logical Effort* (Sutherland, Sproull, Harris).
    Arrays: CACTI for size trends; one named datasheet each for external SDRAM,
    NOR flash and asynchronous SRAM. Cite them in the profile file.
14. **Reference (decision 3).** Implement both `embedded` and `desktop`;
    make `embedded` the default realistic profile because it matches RV32I
    and the game goals and keeps simulations short.

Start order: commit baseline → timing-profile file with `unit` reproducing
today → inertial delay verified on `unit` → 1A gate delays in the realistic
profiles → phase 2 onward.

## 2026-10-16 — Reference sources for calibration

The owner asked which datasheets the calibration will rely on. Values below
were read from the cited documents' summary tables through a web search on
this date; the exact AC tables must be rechecked from the manufacturer's PDF
when the profile file is written. Derived figures are marked.

| Use | Source | Values to use |
| --- | --- | --- |
| Gate delays (decision 1A) | Sutherland, Sproull and Harris, *Logical Effort: Designing Fast CMOS Circuits*, Morgan Kaufmann, 1999 | Logical effort and parasitic delay per gate type; delay = parasitic + effort × fan-out of 4. Tick-per-picosecond mapping per process node still to be sourced |
| External DRAM, `embedded` profile | Micron 256Mb SDR SDRAM, MT48LC16M16A2 ([datasheet copy](https://xdevs.com/doc/_Datasheets/RAM.SDRAM/MT48LC16M16A2P.pdf); [course copy](https://classes.engineering.wustl.edu/cse362/images/6/60/Sdram.pdf)) | -6A grade: 167 MHz, CL = tRCD = tRP = 18 ns; -7E: 15 ns at 133 MHz; 4 banks; 64 ms / 8,192-row refresh (7.8 µs per row, derived) |
| External DRAM, `desktop` profile | Micron 8Gb DDR4, MT40A1G8 ([datasheet](https://mm.digikey.com/Volume0/opasdata/d220001/medias/docus/8734/MT40A1G8SA-062E.pdf); [Rev. L copy](https://classes.engineering.wustl.edu/cse362/images/0/0c/8Gb_DDR4_SDRAM.pdf)) | -062E grade: 3200 MT/s, CL = tRCD = tRP = 13.75 ns; 64 ms / 8,192-row refresh up to 85 °C |
| External NOR flash | Infineon S29GL-S MirrorBit, S29GL256S ([Infineon datasheet](https://www.infineon.com/assets/row/public/documents/10/49/infineon-s29gl01gs-s29gl512s-s29gl256s-s29gl128s-128-mb-256-mb-512-mb-1-gb-gl-s-mirrorbit-flash-parallel-3-datasheet-en.pdf)) | Random access 90 ns; page access 15 ns; buffer program 512 B at 1.5 MB/s typical (about 0.34 ms per buffer, derived); 128 KB sector erase at 477 kB/s typical (about 0.27 s, derived). Guaranteed tPP/tBE from the AC table |
| External asynchronous SRAM | Infineon CY7C1069GN30-10 ([product page](https://www.infineon.com/part/CY7C1069GN30-10ZSXIT)) | 10 ns access time (distributor listings agree; official timing table still to be read) |
| On-chip SRAM and ROM arrays; size scaling | CACTI analytical cache/memory model, versions 6.5/7 ([Utah CACTI 6 page](https://users.cs.utah.edu/~rajeev/cacti6); [README](https://gem5.googlesource.com/public/gem5/+/master/ext/mcpat/cacti/README)) | Access time versus capacity, rows and columns at a stated node; used for trends and the subarray-plus-wiring formula, not as exact values. On-chip mask ROM uses the SRAM array timing without the write path, labeled as a model choice |
| System ratios, `embedded` profile | STMicroelectronics STM32F7 (Cortex-M7, 216 MHz, FMC SDRAM/NOR controller; RM0385/RM0410) | Flash wait states and FMC SDRAM timing in CPU cycles, to check the end-to-end ratio. Exact table not yet read |

Gaps: the tick-to-picosecond mapping per process node, the STM32F7 latency
table and the official SRAM and flash AC tables. No value is entered into code
until its row is checked against the primary PDF.

## 2026-10-16 — Controller scope in the plan

Phase 2 replaces every hidden C++ control part with gates: the delayed
controller's `NativeSequencer` (shared by SRAM, DRAM, ROM and NOR flash),
`DRAMCommandControl`, `SRAMAccessControl`, `FlashControl` and
`ROMAddressControl`. The FF controller, transaction port and response buffer
are already gates and only change timing through leaf delays. This phase also
rewrites the behavioral twins that currently share the C++ sequencer
(`MemoryControllerBehavioral`, `BehavioralMemoryEndpoint`, and the compact
SRAM/DRAM/ROM/NOR devices) so that they copy the new gate circuits and are
checked against them, with the self-timed delay-rule exceptions removed. The
cache stops wrapping its banks in full endpoints in phase 5.

## 2026-10-16 — Completeness review and consolidated plan

A reread found the "Phases" list out of date with the later decisions and
missing several pieces. This section **replaces the earlier "Phases" list**
as the current plan; earlier sections remain as dated reasoning.

### Gaps found in the earlier plan

1. No phase for the timing-profile infrastructure itself, and no proof that
   the `unit` profile reproduces today exactly.
2. No acceptance gate per phase.
3. Timing-analyzer work (asynchronous synchronizer capture, gate-level delay
   chains, per-profile periods) and `part_delay_tables` per-profile generation
   were not tasks.
4. The gate-list delay twin inside behavioral parts (`DelayTwin.cpp`) must
   read profile delays as well as the inertial rule.
5. The SRAM replica bitline (phase 2) needs the geometry-dependent bitline
   delay of phase 3; the two must be one phase.
6. The large-memory model needs a **structural multi-subarray device** to be
   checked against; today each structural device is one bank. This is new
   structural work.
7. Designs may fail under unequal delays (latch races, D flip-flop setup,
   zero-delay wiring order, pulse widths). The plan had no step for finding
   and fixing them.
8. Removing `phase_ticks`/`guard_ticks` and widening `retention_ticks` changes
   endpoint parameters stored in profiles, manifests and layouts; a
   compatibility plan is needed.
9. Process nodes per profile were not chosen. CACTI 6.5 covers 90/65/45/32 nm.
10. Visualizer, WebAssembly, Python binding and documentation work were not
    listed: profile selection, time units in the timeline, regenerated site
    example tables, public gate/memory/timing pages and the adding-a-component
    guide.
11. Model limits to state: the DRAM device keeps a completion signal where
    SDRAM has none; NOR page-mode reads are not modeled.
12. The cache experiment plan must be restated after these phases.

### Consolidated phases

| Phase | Work | Acceptance gate |
| --- | --- | --- |
| 0. Baseline | Commit the worktree once the concurrent sessions finish | Clean `git status`; full native suite result recorded |
| 1. Timing profiles | C++ profile data file (`unit`, `embedded`, `desktop`); leaves read their delays from the active profile; selection through `BuildContext` with a default; profile recorded in `BuildManifest` and experiment provenance; `part_delay_tables` and certified clocks per profile; Python/WASM selection | Under `unit`, every test, generated table and certified clock is identical to phase 0 |
| 2. Inertial delay | Leaf gates replace a pending unpublished change; `DelayTwin` follows the same rule | Under `unit`, still identical to phase 0 |
| 3. Realistic gate and wiring delays | Logical-effort gate delays at the chosen nodes; wiring 0; analyzer and tables regenerated per profile; find and fix designs that fail under unequal delays | All scenario and program tests functionally pass under `embedded` and `desktop`; `DelayRuleTest` passes; clocks certified per profile |
| 4. Gate-level memory control | Delayed controller from gates (request check reuse, one-hot command machine, open-row register and comparator, refresh counter, two-flip-flop synchronizer, response capture); `DRAMCommandControl`, `SRAMAccessControl`, `FlashControl`, `ROMAddressControl` from gates; duplicated waits and flat guards removed; analyzer support for asynchronous capture | Memory contract, timing and endpoint tests pass on all profiles; self-timed delay-rule exceptions removed |
| 5. Geometry-dependent arrays and self-timing | Wordline/bitline/sense delays from rows and columns; SRAM replica bitline and other completion signals; off-chip pad/board leaf; structural multi-subarray device; behavioral twins rewritten to copy the gates | Behavioral equals structural at every structural geometry; access time grows with array size |
| 6. Calibration | Profile values from the cited sources; subarray-plus-wiring formula fitted with a held-out structural size and compared with CACTI trends; realistic refresh and retention; parameter compatibility for removed or widened settings | Each technology's modeled read, write, row and refresh times within a stated tolerance of its source; order SRAM < DRAM ≤ NOR read |
| 7. Cache | Banks driven directly; parallel tag/data and ways; optional bursts and DRAM banks if measured traffic justifies | Cache contract tests pass; hit cost re-measured |
| 8. Publication | Public gate, memory, controller, timing and cache pages; adding-a-component guide; visualizer time units; regenerated site examples; reruns of RV32I and memory results per profile; cache experiment plan restated | Strict site build, browser/WASM tests and full native suite pass; results recorded with profile names |

### Still open

- Process node for `embedded` and `desktop` (for example 90 or 65 nm and
  32 nm, within CACTI 6.5's range).
- Tolerance for phase 6.
- Compatibility rule for removed endpoint parameters.

Phases are serial; focused checks between fixes, one full serial suite per
batch, never two builds at once.

## 2026-10-16 — Remaining decisions (delegated by the owner)

1. **Process nodes.** `embedded` uses **65 nm**, a common microcontroller
   node inside CACTI 6.5's range; `desktop` uses **32 nm**, CACTI 6.5's
   smallest node, labeled as older than current desktop chips. Gate delays
   and array delays must come from one consistent source per node: take the
   fan-out-of-four inverter delay from a published per-node table and check it
   against CACTI's technology parameters; record the table before entering
   any value.
2. **Calibration tolerance.** External devices: each modeled datasheet
   parameter (access, CL, tRCD, tRP, refresh interval, program and erase)
   within **±10 %** of the cited value. On-chip arrays: within **±20 %** of
   CACTI at the same node and geometry, since CACTI is itself an analytical
   estimate. The order SRAM < DRAM ≤ NOR read must hold exactly. The
   end-to-end `embedded` system check against STM32F7 cycle counts is a
   plausibility check within **±25 %**, not a calibration target.
3. **Old memory settings.** `phase_ticks` and `guard_ticks` lose their meaning
   once the controllers are gates (phase 4). They are then rejected with an
   error naming the replacement (the timing profile). Saved profiles,
   manifests and layouts that carry them at their old default values are
   migrated by dropping them on load, in the same place as
   `circuit::canonicalContractId`; any non-default value is an error.
   `retention_ticks` and `refresh_interval_ticks` become 64-bit with ranges
   wide enough for picosecond ticks (64 ms = 6.4×10^10).

**Correction to earlier sections.** The `unit` profile reproduces today's
results only until phase 4: replacing the C++ controllers with gates and
adding size scaling changes memory timing under every profile. Exact
reproduction of published results is therefore guaranteed by a Git tag,
`timing-legacy`, on the last commit before phase 4. After that, `unit`
remains a regression profile for logic timing only. This replaces the
earlier claims that `unit` keeps all published results reproducible.

All plan decisions are now made; phase 0 waits for the concurrent sessions.

## 2026-10-16 — Testing rule: subsets per phase, not the full suite

The owner directed that the full native suite (about 40 minutes) must not
run at every phase; only the subsets each change needs. This replaces the
"full native suite" and "one full serial suite per batch" wording in the
consolidated phase table and earlier sections.

- Phase 0: no new full run; the baseline is the commit itself.
- Phases 1–2 (`unit` must stay identical): regenerate `part_delay_tables`
  and diff against the committed tables; run `DelayRuleTest`,
  `PartDelayTablesCompleteTest`, `DFlipFlopTimingTest`, the safe-clock tests
  and a few scenario runners. Identical generated tables are the main proof.
- Phase 3: delay, timing and safe-clock tests on each profile, plus a
  representative scenario and program subset (gates, flip-flop, ALU, one core
  per kind, memory endpoints).
- Phases 4–7: the memory, endpoint, cache and affected RV32I memory tests
  only.
- Phase 8: one full native suite plus the site and browser checks, before
  publishing. A full run earlier only if subsets show broad breakage.

## 2026-10-16 — Requirements from the cache experiment

The [cache plan's current section](2026-10-14-cache-system-plan.md) adds two
requirements here: phases 5–6 raise the 16-word DRAM row limit for the
scalable device so a datasheet row (512 words on a 32-bit bus) can be
configured; phase 7 raises the cache's 64-set and four-words-per-line limits
if the cache plan's sizing rule needs it, and decides bursts and banks before
that experiment freezes its panel.

## 2026-10-16 — Phase 3 acceptance revised during execution

See the [progress log](2026-10-16-realistic-timing-progress.md): under the
realistic profiles, phase 3 is accepted on contract tests, per-profile delay
tables verified against the gates, oracle-checked CPU runs (gate-level and
compact) at certified clocks, and a `DelayRuleTest` sample, instead of every
scenario test, whose clock phases are hard-coded in ticks. Timed memories are
excluded under realistic profiles until phases 4–6.

## 2026-10-16 — Datasheet values read from the primary PDFs (for phase 6)

Read with `pdftotext` from the manufacturer documents on this date (copies
kept only in the session scratch area). These replace the summary-table
values of "Reference sources for calibration" where they differ.

| Device | Document | Values |
| --- | --- | --- |
| SDR SDRAM, `embedded` | Micron 256Mb SDRAM MT48LC16M16A2, Rev. M 1/09, Table 19 (-6A) | tCK(3) 6 ns, CL 3 (18 ns), tAC(3) 5.4 ns, tRCD 18 ns, tRP 18 ns, tRAS 42 ns min, tRC 60 ns, tRFC 60 ns, tWR 1 CLK + 6 ns (12 ns), tREF 64 ms for 8,192 rows (7.8 µs per row, derived). x16 bank: 8,192 rows × 512 columns × 16 bits |
| NOR flash | Infineon GL-S MirrorBit S29GL01GS/512S/256S/128S, **military-temperature edition** (−55 °C to +125 °C), Table 16 and Table 57 | 256 Mb: tACC 120 ns, tCE 120 ns, tOE 25 ns, tPACC 15 ns (full VCC = VIO). Single-word program 150 µs typ / 400 µs max; 512-byte buffer program 420 µs typ / 1,050 µs max (1.64 µs per word effective); 128 KB sector erase 410 ms typ / 1,100 ms max |
| Asynchronous SRAM | Infineon/Cypress CY7C1069G/GE, doc 001-81539 Rev. *I (G family; the GN listing is the same 10 ns grade) | tRC 10 ns, tAA 10 ns, tACE 10 ns, tDOE 5 ns, tWC 10 ns, tSCE 7 ns, tAW 7 ns, tPWE 7 ns, tSD 5 ns |

Not yet read: the DDR4 MT40A1G8 table for `desktop`, and the STM32F7 latency
table. The NOR program and erase times are 10^8 to 10^11 ps: simulating them
literally would cost about 30,000 to 85 million clock cycles per operation at
the `embedded` clock. Phase 6 must decide how program/erase are treated
(literal with explicit budgets, or a labeled time scale), and
`program_ticks`/`erase_ticks` limits must widen if literal.

## 2026-10-16 — Plan check after phase 4 (assistant decisions)

Observations from phase 4 (see the progress log) and what they change:

1. **External devices do not scale with the modeled depth.** The placeholder
   per-row terms (sense, read column, bitline) made a 64 Ki-word experiment
   memory (32,768 rows at two words per row) take about 100 ns per DRAM
   activate and NOR read, tripling cycles. A real SDRAM, NOR flash or
   asynchronous SRAM chip has a fixed internal geometry; its datasheet times
   do not depend on how many words a program uses. **Decision:** for the
   external technologies (DRAM, NOR flash, and SRAM when it models an external
   chip) the realistic intervals are absolute values calibrated so the
   device's command timings equal the datasheet (per-row terms 0 in the
   realistic profiles). Geometry scaling (subarrays plus global wiring,
   decision 8) applies to **on-chip** arrays: SRAM used as cache tag/data
   banks and on-chip ROM. This narrows phase 5's structural multi-subarray
   work to on-chip SRAM, which is what the cache experiment needs.
2. **Device completion must follow data.** SRAM READY rises before the sense
   and output path (210 ps at `embedded`). Phase 5 extends the READY replica
   over that path, because phase 7 drives cache banks directly.
3. **Unequal delays find real races.** Three races hid under `unit`
   (change-detector pulse, DRAM sense vs wordline, DRAM precharge vs
   wordline). From now on each structural change is checked under `embedded`
   as well as `unit` before it is accepted.
4. **NOR program and erase are 10^8–10^11 ps.** **Decision:** keep them
   literal in the profile (150 µs program, 410 ms erase from the cited
   datasheet), widen `program_ticks`/`erase_ticks` limits accordingly, and
   give experiments that program or erase explicit cycle budgets. No hidden
   time scale. CPU workloads only read NOR, so the panels are unaffected.
5. **Command protocol cost.** Each DRAM/NOR command costs about five clock
   edges in this device design. Accepted as the model's protocol cost; the
   calibration targets the analog times, and results report cycles and
   modeled time separately.
6. **Delay twins can drift after X/Z.** A gate list that claimed an output
   settled before the change was published made a compact controller answer
   0 ticks after a clock edge. Fixed generally: such a contradiction falls
   back to the proven table (never earlier than the gates).

Revised phases 5–6 (replace the corresponding rows of the consolidated
table):

| Phase | Work | Acceptance gate |
| --- | --- | --- |
| 5. On-chip geometry and completion | READY replica over sense and output; structural multi-subarray on-chip SRAM (subarrays of at most 64 rows plus a global wire leaf); size formula fitted on 1, 2 and 4 subarrays and checked on a held-out 8; scalable on-chip SRAM uses it | Structural and compact agree at every structural size; held-out size within ±10 % of the formula; access grows with size; checks pass under `unit` and `embedded` |
| 6. Calibration | Realistic intervals from the PDF-checked tables (SDRAM tRCD/CL/tRP/tRAS/tWR/tREF, NOR tACC/program/erase, SRAM tAA/tWC) with per-row terms 0 for external devices; on-chip SRAM against CACTI 6.5 trends; refresh interval 7.8 µs in cycles at the certified clock | Each external parameter within ±10 % of its datasheet; on-chip within ±20 % of CACTI; SRAM < DRAM ≤ NOR read order holds |

## 2026-10-16 — Owner rule: realistic memories, not cache-favouring ones

The owner directed: do not shape memory behavior so that caches look
effective; model the memory technologies realistically. This governs phases
5–7 and the cache experiment. Consequences, decided by the assistant:

1. **Calibrate end-to-end latency, not only analog intervals.** Phase 6
   acceptance adds request-to-data latency in clock cycles at the certified
   clock, compared with real systems: SDRAM row hit about CL plus controller
   overhead, row miss about tRP + tRCD + CL (Micron -6A: 18 + 18 + 18 ns);
   NOR random read about tACC; asynchronous SRAM about tAA; and the
   STM32F7 FMC/flash wait-state figures as the system plausibility check
   (±25 %). Where the model's protocol adds cycles a real controller would
   not spend, that is a model defect to fix, not a result.
2. **Known protocol artifacts to review against real controllers.** (a) The
   DRAM/NOR command protocol costs about five edges per command (sampler,
   completion, release, controller sees BUSY). Real SDRAM has no BUSY: the
   controller counts tRCD/CL/tRP cycles. Phase 6 either justifies each edge
   or moves the DRAM controller to counted timing from the calibrated
   intervals, keeping the gate-level device. (b) The static controller's
   two-stage synchronizer adds two cycles to every SRAM/ROM access; that is
   right for an asynchronous external chip but not for an on-chip SRAM
   clocked with the core. (c) The 2026-10-16 latency probe measured a 14-cycle
   L1 hit against 4 cycles for a DRAM open-row read; a real small L1 hits in
   1–2 cycles. Phase 7 must bring cache hit latency to a physically justified
   value (synchronous on-chip banks driven directly), and the record must
   state the remaining cycle costs and why.
3. **No tuning toward an outcome.** Parameters come from cited sources or
   from the gate-level circuits; a result that favors or disfavors caches is
   reported as found, with its protocol and timing causes, and is never a
   reason to change a parameter.

## 2026-10-17 — Plan check after phase 6.3 (assistant decisions)

Evidence is in the progress log (6.3a and 6.3b sections). Findings that
change the plan:

1. **The core–endpoint handshake limits every certified clock, and the
   clocked macro makes it worse.** All timed memories were already limited by
   the IMEM handshake (REQ_READY full-cycle setup). With the clocked macro the
   endpoint presents an accepted request straight to the macro, so the
   core's stall and forwarding logic → RSP_READY → acceptance → the macro's
   CE_N setup becomes one path: certified clock `unit` 220 ticks against 102
   for the asynchronous chip, `embedded` 5,236 ps against 4,608, `desktop`
   3,680 against 3,210. In cycles the macro is better (programs 19–22:
   269/166/297/228 against 336/220/384/292 at `embedded`); in time it is
   9 % faster at `embedded` and `desktop` and 54 % slower at `unit`. A real
   bus does not do this: an AHB master cannot hold off read data, so an SRAM
   enable never waits for the core's decision to consume the previous
   response in the same cycle. **Decision: phase 6.4 starts with decoupling
   request acceptance from the same-cycle RSP_READY** (a two-entry response
   buffer, the standard skid buffer, for every technology), then measures
   the certified clocks again. The clocked macro stays the default `sram`.
2. **Macro calibration stops at the honest residual.** The clocked macro's
   proven access (edge to data, `embedded`) is 54 FO4 at 4 bytes and 69 FO4
   at 512 bytes, against CACTI 65 nm LOP 22.4 FO4 at 512 bytes (3.1×; 4.5×
   with the input setup, which holds the 32-bit range check and the gate-built
   register setup). The known artifacts are the gate-built flip-flops
   (clock-to-Q up to 231 ps), the access token's change detection (about
   300 ps), precharge in the access path, the replica margins, and the 32-bit
   address range check. At the realistic clocks (about 150–175 FO4 per
   cycle) the macro already completes in one cycle at every proven size, so
   a faster macro would change no cycle count and no certified clock. It is
   reported as found and not tuned. Revisit only if phase 7 cache banks
   (tag compare and way select after the array) become cycle-critical.
3. **New phase 8 items:** the delay twin's X timing (a gate-level flip-flop
   carries an X one tick later than the twin's binary clock-to-output;
   reached by the 30-round sample of `memory.sram.endpoint.16`, not by the
   CTest run), and CacheReadTest with a profile-derived period (with the
   other tick-driven tests already listed).
4. Test equipment fixed on the way, kept: DelayRuleTest steps from event to
   event and follows a part until it goes quiet (100× faster under the
   realistic profiles; the six DRAM/NOR endpoints it used to skip are now
   compared).

The phases otherwise stand: 6.4 (handshake decoupling, then end-to-end
latency against datasheets and STM32F7), phase 7 (cache on the clocked macro),
phase 8 (documentation, follow-ups, full suite).

## 2026-10-17 — CPU coverage and test timing (assistant decisions, after an owner question)

The owner asked whether all CPU core experiments run on the new models and
whether test timings need to change. Facts: each phase so far was checked
with one CPU configuration (advanced core, two-bit predictor, programs
19–22, every memory, all three profiles, oracle-verified); the single-cycle
and baseline cores, the other predictors and the other benchmark programs
have not been rerun. The test suite runs under `unit` (the code default,
1-tick gates), so its scenario expectations stay valid; tests with fixed tick
periods or deadlines (CacheReadTest, MemoryDeviceTimingTest,
MemoryEndpointTimingTest and possibly others) fail under realistic profiles.
The plan text says `embedded` is the default and "full tests run on the
default profile", which does not match the code.

Decisions:
1. **New step 6.5 (end of phase 6): core sweep.** Every core (single-cycle,
   baseline pipeline, advanced) × predictor × benchmark program, with every
   memory, under `embedded` at the certified clocks, oracle-verified; a
   smaller `desktop` and `unit` sample. Purpose: find breakage now rather than
   in phase 8. Phase 8 keeps the rerun and restatement of published results.
2. **Phase 8 test timing:** audit every test for fixed tick periods and
   deadlines and derive them from the active profile (its certified clock or
   multiples of proven delays); then the full native suite runs under both
   `unit` and `embedded`.
3. **Default profile, corrected:** tests and the code default stay `unit`
   (fast regression for logic timing); experiments and the published site
   use `embedded`. This replaces "full tests run on the default profile".

## 2026-10-17 — Plan check after phase 6 (assistant decisions)

Phase 6 ended accepted with stated residuals (progress log, "Phase 6
acceptance"). What it changes for phase 7:
1. **The comparison baseline is now fair.** Every memory, the FF reference
   included, certifies at one clock per realistic profile (4,104 ps
   `embedded`, 2,858 ps `desktop`), set by the core itself. Cache results
   can be compared in cycles and in time without one memory paying a
   handshake penalty the others do not.
2. **Same rules for the cache path.** The direct path keeps a registered
   response (6.4b) and one DRAM synchronizer edge per command (6.4c). The
   cache's core-facing interface must obey the same budget: no response
   bypass the direct path is denied, REQ_READY from registered state only,
   and banks on the clocked macro. A cache hit latency of 1–2 cycles must
   come out of that structure, not be assumed.
3. **DRAM residual stays visible.** DRAM latency is +23–33 % over a counted
   controller. The cache experiment will state the DRAM row-hit and row-miss
   latency it ran with, so a reader can judge the miss penalty. Converting
   the DRAM controller to counted timing remains an option if phase 7's
   results turn out sensitive to it (to be measured there, not assumed).
4. **Open:** STM32F7 system check (source table still unread), the delay
   twin's X timing, tick-driven tests under realistic profiles (phase 8).

Phase 7 proceeds as planned: cache banks on the clocked macro, then the
experiment restated with calibrated timing.

## 2026-10-17 — Plan change in phase 7: bursts before the campaign (assistant decision)

Phase 7a (direct banks) brought the L1 hit to two cycles, but the cache
plan's Step 1 order check failed because line fills move word by word (L1
miss 38 cycles against an 8-cycle DRAM read; L2 hit 14). The phase 7 row
listed bursts "if measured traffic justifies"; the measurement justifies
them. Phase 7b adds burst reads (op 4, `block_words` words per request) to
the backing interface: DRAM streams the next columns of the open row one
per clock after CL, static memories stream at `access_cycles` per word,
caches fill with bursts and serve them as the next level. Then Step 1 is
measured again, and the campaign follows only if the order holds. The Step 1
expectation "DRAM tens of cycles" is corrected for an MCU clock (an SDRAM row
hit is under ten cycles at 216–244 MHz).

## 2026-10-18 — Plan change in phase 8: test timing (assistant)

Replaces decision 2 of "CPU coverage and test timing" (2026-10-17), which
asked to derive every test timing from the active profile and to pass the
whole native suite under both `unit` and `embedded`. Evidence (progress log,
2026-10-18): under `embedded` 217 of 466 tests fail because they are written
in `unit` ticks, and the CPU harnesses alone hold about 30 tick constants;
scaling the clock without them fixed none. Decision: the native suite stays
a `unit` suite; realistic profiles are covered by tests that choose their
own profile and use certified clocks (`RV32IProfileSweepTest`,
`RV32ITopologyTest`, per-profile table checks, the CLI checks), together with
the phase 6.5 core sweep. Phase 8's remaining work: the delay-twin defect
found by the strict-settling check, clock evidence for cached roots, the
visualizer data, and the public site when the owner allows it.

## 2026-10-18 — Phase 8: cached-root clock evidence settled (assistant)

The plan asked for either analyzer support for cached roots or proven
timing for the cache's state bits. Evidence (progress log, 2026-10-18): the
state bits already have proven tables and setup checks; the analyzer's only
objections were a gated clock in the arbiter (a design fault, fixed) and the
caches' zero-delay clock wires (accepted, as in the delay-table analyzer).
No new memory-contract model was needed. All five topologies are certified
at the core's clock in both realistic profiles. Remaining phase 8 work: the
public site when the owner allows it, the documentation checks that read
it, the STM32F7 check and reruns of published results.
