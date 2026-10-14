# Documentation audit against the implementation

Started: 2026-10-13. Status: open. Code state: `github-pages` at `448e4e3`, with an uncommitted documentation reorganisation in the working tree.

## Summary

The public component pages now describe compact unknown-value behavior and
gate-derived behavioral delays, including the optional value-dependent delay
twin and constructor fallback. The maintained timing guide/reference and
experiment runner's clock-contract README reflect the current 166/80/100
FF and 102 timed-memory certificates. The audit remains open for the public
landing/visualizer pages and other non-component findings. Dated findings
below retain the original `docs/` paths and assessments; maintained pages
now live under `docs/guides/` or `docs/reference/` (see the
[layout record](2026-10-13-guides-reference-layout.md)).

## 2026-10-13 — Method

- Every Markdown file under `docs/`, `site-docs/`, `tools/*/README.md`,
  `README.md` and `AGENTS.md` (archive included for links only): relative
  links and quoted file paths resolved; every quoted identifier, command-line
  option and contract ID looked up in `include/`, `src/`, `tests/`, `tools/`
  and `visualizer-v2/`.
- The internal guidance pages were read in full and their claims checked by
  search or by running the tool. Site pages were read where the recent timing,
  memory and unknown-value changes could affect them; the remaining site pages
  were covered by the mechanical checks and by the documentation tests, which
  last passed in the full suite at `34abf09`.
- Not done: the site was not rebuilt, and archive contents were not compared
  with the code (they are historical by definition).

## 2026-10-13 — Findings

Agreeing with the code: no broken link, no missing file path, archive index
complete (61 files); contract IDs, test names and options exist; the zero-wait
cycle table in `docs/rv32i-memory-integration.md` (77, 70, 103, 85, 134, 87)
reproduces; workload counts (22 programs, 15 generated families) hold;
`site-docs/memory/3.3.md` matches the sequencer; `site-docs/rv32i/4.2/timing.md`,
`execution.md`, `campaigns.md` and `4.6/memory-waits.md` hold.

Out of date:

1. Compact timing described as "one abstract delay tick" although the part now
   runs on a delay table proven on its gates: `site-docs/alu8/2.1.md` to
   `2.6.md` (13 places), `site-docs/rv32i/4.3/alu32.md` (2),
   `instruction-decoder.md`, `register.md` (2), `register-file.md`,
   `memory-access.md` (lanes and requester), `execution-status.md`,
   `4.4/fetch.md`, `decode.md`, `writeback.md`, `next-pc.md`,
   `site-docs/memory/3.1.1.md` (cell, word, bank), and
   `docs/rv32i-single-cycle-documentation-notes.md`. Only
   `rv32i.pipeline.memory-alignment` was not confirmed either way by name
   (`memory-access.md` line 126).
2. Unknown-value behaviour: `4.4/fetch.md` says unknown inputs fall back to
   zero (the compact PC is now a four-state register); `4.4/decode.md` says a
   nonbinary instruction writes no destination (writes now follow the register
   file's four-state rule); `4.5/fetch.md`, `4.3/memory-access.md` (requester)
   and `docs/rv32i-single-cycle-stages.md` limit equivalence to binary values
   where the compact form now follows its gates.
3. `tools/rv32i-performance/README.md`, sections "Clock and timing contract",
   "Fail-closed clock-safety report" and "Binary-initialized clock-safety
   scope": the retired formula `2 x (path + floor(path/4) + 2)`; periods 212
   and 176/112/212 (now 166, 80, 100 in both scopes); "single/baseline remain
   unknown" (both certify); "delayed technology endpoints remain unsupported"
   and "automatic core timing uses the FF reference" (timed memories certify at
   102).
4. `site-docs/index.md` is still the placeholder ("content will be added",
   "Browse the empty blog"); `site-docs/visualizer.md` only embeds the
   simulator and describes nothing.
5. Small: `docs/rv32i-memory-integration.md` labels its link to chapter 4.4 as
   "4.3"; `docs/circuit-timing-analyzer.md` says about 117,000 situations (the
   last run had 120,081); `README.md` opens with benchmark notes before saying
   what the project is and says only chapter 2 tables are generated;
   `docs/automatic-layout-refresh.md` hardcodes `/home/arnold/venv`; no public
   page mentions `setup_violations`, `--check-handshake-states`, or that timed
   memories cost one more cycle per access.

## 2026-10-13 — Components against the site pages

Question: has any circuit appeared, or changed, without the site saying so?

- Method: every `TypeName` declared in `include/` and `src/` searched for in
  `site-docs/`; the diff of `include/modules` and `src/modules` from `7e13cdf`
  (the last commit that brought the pages in line) to `448e4e3` read in full.
- Pin tables: the documentation tests compare each page's pin table with the
  built part's pins; they passed in the full suite at `448e4e3`.
- Type names with no mention on the site: `RV32IFetchStage`,
  `RV32IMemoryStage`, `RV32IWritebackBypass` (advanced pipeline),
  `ClockGenerator`, `NORGate`, `Mux2to1_4bit`, `Mux2to1_32bit`, `Mux8to1_8bit`,
  `Mux8to1_32bit` (basic parts), and two test-only types (`AddSub4Slice`,
  `RV32IProgramRoot`). Found by name search only; whether the 4.6 pages
  describe the three pipeline parts under other words was not read through.
- Gate-level behaviour changed since `7e13cdf` in one place: the memory
  sequencer shows a finished operation at the next clock edge, and a DRAM
  refresh starts only after the backend was seen ready for an edge. Page 3.3
  has both. No structural part was added, removed or rewired; no pin changed.
- All other module changes are in compact forms, made to follow their gates
  (unknown values, delay tables). Pages still describing the old compact
  behaviour are findings 1 and 2 above; `4.5/registers.md` "bitwise unknown
  merging" is correct but does not state the rule (0 only if both are 0).
- Not checked: cycle counts quoted on `4.6/memory-waits.md` and
  `4.2/timing.md` against the one-edge-later rule.

## 2026-10-13 — Component reference corrections

This section replaces the "nothing corrected yet" assessment in the summary
and resolves the component-page parts of finding 2. The published Fetch pages
now state that their compact PC stores four-state values rather than turning
unknown bits into zero. The single-cycle Decode page now distinguishes unknown
decode outputs from the register-file write, which still follows instruction
bits 11:7 and the register-file's uncertain-destination rule. The memory
requester page now distinguishes its own four-state registers from the binary
`MemoryRequestState` used by complete compact cores. The baseline packet
register page spells out the zero-only uncertain-write merge. Existing advanced
Fetch and Memory stage classes and the internal WB-bypass selector are named in
their component references. The maintained single-cycle stage reference now
names `IncrementBy4` as Fetch's PC+4 part and notes the compact stage rules.

Source checked: `RV32ISingleCycleStagesBehavioral.cpp`,
`RV32IBaselinePipelineBehavioral.cpp`, `RV32IMemoryRequest.cpp`,
`RV32IPipelineRegisters.cpp`, `RV32IPipelineStages.cpp` and its Fetch/Memory
implementations, `RV32IForwardingUnit.cpp`, `RV32IDecodeStage.cpp`, and the
register-file behavior. No component or pin was added.

Verification: all 11 selected documentation CTests passed serially; the strict
MkDocs build passed using the existing generated example-row bundle;
`git diff --check` passed. This was a documentation-only review; the full
native suite, WebAssembly package and browser checks were not rerun.
Finding 1 (behavioral delay descriptions), finding 3 (RV32I experiment
documentation), and RV32I test-case inconsistencies were deliberately left
outside this edit. The non-component findings 4 and 5 remain open.

## 2026-10-14 — Behavioral delay documentation revision started

- The user asked to fix the remaining compact-delay descriptions. A read-only
  pass found stale one-tick claims in ALU8, FF memory and RV32I component
  pages, plus table-only or old-period prose in maintained internal timing
  references. The current catalog attaches a gate-derived `PartDelays` table
  to matching behavioral parts and, where supported, a `DelayTwin` that
  computes settling from the actual input values. A part without a matching
  table retains its constructor delay; self-timed parts and large cores use
  tables without a delay twin. The `part_delay_tables --coverage` query
  reported 172 tabled compact implementations and two stable ROM parts
  without a table.
- This revision will keep valid one-tick statements for primitive gates and
  terminal digital memory parts, correct catalog-backed component timing,
  distinguish propagation delay from the fixed experiment clock period, and
  reconcile the internal timing reference. Checks and remaining gaps will be
  recorded after the edit.

## 2026-10-14 — Behavioral delay descriptions corrected

This section replaces the open timing assessment above and the applicable
parts of findings 1 and 3. ALU8, FF-memory, RV32I building-block, single-cycle
stage and pipeline overview pages now describe catalog-attached per-path
tables. They explain where a delay twin follows current values, where a
large or self-timed part uses the table alone, why pending results can hold a
new publication, and when constructor delay remains. True one-tick primitive
claims were retained. The maintained timing reference and guided explanation
now distinguish primitive fixed delays, table bounds, runtime gate-list
settling, clock-to-output/setup arcs, and the fixed CPU clock period. The
experiment runner README no longer gives the retired candidate-clock formula,
old periods or an FF-only automatic timing claim.

Source checked: `ComponentCatalog::create`/`delayTwin`, `BasicComponent::evaluateAt`/
`publishTime`, `ProvenPartDelays`, the generated tables and the timing tests.
`part_delay_tables --coverage` reported 172 tabled compact implementations;
the ROM cell and row are stable-output exceptions. Current FF periods are
166/80/100 and the advanced timed-memory period is 102. The direct SRAM
`--safe-clock` query returned `safe-under-contracts`, period 102, in the
unrestricted four-state scope.

Verification: all 15 documentation-labeled CTests passed serially; the strict
MkDocs build passed using the existing generated example-row bundle;
`git diff --check` passed. No C++ behavior was changed. The full native suite,
WebAssembly packaging and browser checks were not rerun for this documentation
change. The public landing/visualizer pages and minor audit findings remain
open; current RV32I result coverage and the Embench projection are recorded
separately in §4.7.
