# Structural constant words and arithmetic inspection

Started: 2026-10-24. Status: closed. Code state: `alpha` based on `5549ee0`,
with this constant-wiring change.

## Summary

Loose per-bit banks now use supported-width constant words, and scalar
zero/one sources are shared within structural wiring scopes. Arithmetic,
device and SRAM/ROM wiring retain their structural implementations. Both
32-bit arithmetic datapaths have 14 children instead of 46 and one zero-word
source each. Focused native, timing, layout, portable and browser checks pass;
the strict site and port-8893 preview are updated. Full suites were not run.

## 2026-10-24 — Investigation and scope

The divider's current `DATAPATH` contains 32 one-bit `ConstantValue` children
(`ZERO_4` through `ZERO_35`), joined to initialize the remainder on launch.
`Logic::constant` constructs one independent source per bit. The multiplier,
arithmetic operand/result/control groups and device components use the same
helper. Repeated `Logic::bit` calls also duplicate scalar tie sources.

Use the existing zero-delay `ConstantValue<W>` primitive and ordinary bit
rewires; arithmetic remains gates, registers, adders and muxes. Direct bus
consumers should receive the word without splitting and rejoining it.
`MultiplyStep` and `DivideStep` already support 4/8/16/32 bits and publish
width-qualified runtime types. Correct their stale public headings and width
lists; no catalog rename or C++ template migration is needed.

Keep tests minimal: existing arithmetic/width and a representative device
check, affected stored timing comparisons, saved-layout validation and focused
browser expansion/playback. The complete suites and game campaigns are outside
this change. Baseline topology counts are retained under
`build-milestone/constant-word-cleanup/topology-before.json`.

## 2026-10-24 — Implementation

`Logic::constantWord` uses existing constant primitives at all eight supported
pin widths; `Logic::constant` splits the word for gate inputs, and `bit`
reuses scalar zero/one sources per helper scope. Arithmetic bus consumers
avoid the split/join round trip. Native topology confirms both 32-bit
datapaths have 14 children instead of 46, including one 32-bit zero source.
The explicit SRAM/ROM sequencer constant bank also becomes one shared zero
tie, matching its existing command-sequencer counterpart.

The step-component pages, index and navigation use `<W>`, all supported widths
are listed, and their scenario titles show the recorded width `<4>`.
Compilation of the arithmetic/helper changes passed. The additional memory
source change and functional/timing/layout validation are pending.

## 2026-10-24 — Native, timing and layout verification

The final serial native build passes with Python deployment disabled. Nine
focused functional registrations pass: both step sweeps, width components,
the counter, both 32-bit arithmetic recordings, Fibonacci and the SRAM/ROM
endpoint scenarios. The arithmetic selection takes 43.97 seconds and the
two endpoint checks 6.39 seconds. The first catalog invocation overlapped a
relink and reported two subprocess permission errors in its topology-variant
method; the other seven methods passed. After all links finished, that
method passes alone in 26.46 seconds. This was a check-scheduling mistake,
not a product failure; subsequent checks use completed artifacts.

All 15 selected stored default tables recompute identically in each of unit,
embedded and desktop: 45 comparisons, zero differences. These cover the
compact MulDiv/core, router/guard/register port, counters/FIFOs and SRAM/ROM
controllers at 8/16/32 bits. No generated timing file changes. Unselected
parameter variants and complete timing/native suites were not run.

| Inspection topology | Components before → after | Constant sources before → after |
| --- | ---: | ---: |
| Multiplier<32> | 359 → 288 | 78 → 7 |
| Divider<32> | 720 → 528 | 208 → 12 |
| RV32IM execute stage | 8,257 → 7,994 | 291 → 24 |
| Recorded counter | 47 → 45 | 5 → 2 |

Refreshed 61 saved layout scopes for 22 affected component types, retaining
styles and migration snapshots for surviving children. A first generator
comparison also selected parameterized I bit-pattern matchers because its
baseline grouped raw types; their source topology is unchanged, so all 22
unrelated matcher scopes were restored exactly. Dropped removed/unchanged
children from the new migration snapshots and preserved existing child-key
order. All 30 selected component, core, memory and program layouts have zero
problems. The existing migration check passes 729 automatic entries,
including manual positions, styles and skipped revisions.

The serial WASM build and strict combined site build pass; MkDocs reports
122.26 seconds. Nine example groups were regenerated, retaining previously
verified unrelated groups. Native/WASM topology, recorded state and example
parity pass for divider, counter and console (11.16 seconds). The first browser
launcher used the documentation virtualenv, which has no Playwright; retry
uses the installed system-Python Playwright per the test guide. Browser
inspection and the port-8893 refresh are pending.

## 2026-10-24 — Final package and preview acceptance

All three isolated browser sessions pass in 89.06 seconds: multiplier,
divider and Fibonacci. Their grouped scopes have no overlaps, missing
placements or explorer errors; the program also passes instruction-memory
inspection, halt and rewind. A fresh divider session on port 8893 passes in
17.57 seconds, and its screenshot shows the current 528-component topology.

The preview's strict build passes in 90.42 seconds. Eight component/program/
device/memory/simulator routes respond, and 26 non-HTML assets match the
reviewed 28-file package exactly. The previous package is preserved under
`build-wasm/constant-word-cleanup-refresh/20261023T155426Z/simulator-before`.
The deployed native Python modules are untouched.

Correction to the preceding example-regeneration claim: the initial package
filter used scenario aliases, while this generator matches registered test
names, so it retained the previous bundle without regenerating groups. A
corrected filter subsequently regenerates all nine intended groups: eight
component examples (including SRAM/ROM) and Fibonacci's 82 program
checkpoints. The full bundle remains byte-identical, confirming the unchanged
data used by both successful strict builds. No generated rows were edited.

Restored the repository's standard compact layout formatting after the final
geometry review. Repository, isolated package and preview source layouts
contain identical JSON data; formatting does not change their tested
geometry or migration history. Unrelated matcher scopes remain identical to
the starting revision.

Evidence is retained under `build-milestone/constant-word-cleanup/`, including
native build/check logs, the catalog retry, timing comparisons, topology
snapshots, layout report/checks, migration checks, portable/browser results,
example regeneration and preview hashes. This is scoped acceptance: no full
native/web/timing suite, benchmark/game campaign, push or published-site
deployment was performed.
