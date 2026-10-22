# Complete replacement of fixed-configuration components

Started: 2026-10-22. Status: closed (implemented and focused acceptance passed).
Code state: main `alpha`, `88c0a95`, with existing uncommitted width/device/game
work and the current ALU refactor. Unrelated changes remain in place.

## Summary

The owner requires actual retirement of fixed-configuration components,
including ALU8, ALU32 and named mux variants. `RegisterFile32x32` is an
explicit owner-approved RV32I architectural exception; its recipe shares the
configurable register-file implementation. The owner explicitly rejects
preserving old IDs: this replacement has no compatibility factories, old-ID
aliases or saved-layout redirects. This supersedes the compatibility policy
in the earlier [width audit](2026-10-22-width-component-audit.md).


The replacement passes 23 focused native checks, two Python API checks,
seven reference/metadata registrations, four selected catalog methods and six
portable/browser registrations. All 158 example groups pass; three regenerated
timing profiles have 809 entries each, with complete unit-file verification.
The strict public package is on 8893. Fresh Pong has 18 exact-reference frames
and identical retained pin/wire states to the prior capture. Full suites,
broader phase-6 game acceptance and phase 7 remain outside this closure.

## 2026-10-22 — Scope and execution

Replace public fixed word/count classes with their parameterized components;
remove their factories and contract IDs; migrate all repository callers,
behavioral implementations, registry ownership, scenarios, profiles, layouts,
bindings and public references. Mux fan-in is a parameter, so named two-,
four-, eight- and 32-input variants are included. Inspect decoder and
increment helpers as part of the same inventory. ISA-specific RV32I/RV32IM
contracts and elementary gates/cells retain their architectural identities.

Source inventory currently identifies the arithmetic/ALU wrappers, ten mux
variants, the fixed decoder, register-file wrapper, M arithmetic wrappers,
and nine device wrappers. Existing generic implementations cover most word
and bus functions; remaining device ABI constraints must be made explicit
and handled in the implementation rather than hidden by renaming.

Execution order:

1. Finish the current focused ALU validation and restore the coherent local
   preview package, then keep it available during the larger replacement.
2. Replace arithmetic/control/mux/decoder/register-file construction and
   migrate their callers to the generic contracts.
3. Replace M/device wrappers and their callers, preserving the implemented
   game/device behavior through parameterized construction.
4. Remove retired registrations/aliases; update tests, scenarios, saved
   repository layouts and public reference paths/names.
5. Regenerate affected timing artifacts from the structural circuits and
   run serial, bounded component/integration/documentation/portable checks.
   Record precisely which larger regressions were not run.

No broad suite or game campaign is authorized by this record. The owner
requests minimal tests. All builds stay serial and resource guarded.

### Mux replacement in progress

The ten named mux classes and their factories are removed from the source.
`Mux<Inputs,W>` and its runtime family are the only construction paths;
contracts are `logic.mux.<inputs>.<width>` with no old-ID mapping. Width-32
compact factories reuse the parameterized mux-tree evaluator. Repository
C++ callers and scenario/test class names have migrated to the generic types;
Python exposes a geometry-aware factory. Timing generation, complete metadata/
layout migration and the retirement-wide build/checks remain pending.

A compressed pre-retirement source snapshot is retained under
`build-milestone/alu-refactor/`; it includes prior dirty changes. Unchanged
file timestamps were restored after the scripted replacement to avoid
rebuilding untouched source files.

### Arithmetic/control replacement in progress

Fixed ALU/arithmetic/control classes and headers are removed. Every word
family now constructs the width-aware recipe directly, with no legacy-family
routing. Control contracts are `word.control.extended` and
`word.control.integer`. Shared compact implementations are parameterized by
word width/topology; parent cores call the generic integer value rule.
The compact integer ALU retains its declared known-binary domain. The public
parallel-prefix component and PC incrementer are parameterized, with their
fixed variants removed. Caller/test/binding migrations are under way.

The generic word behavioral/family/control sources pass a bounded native
syntax compilation. The intermediate mux build compiled its implementation
and downstream arithmetic/memory users, then was stopped intentionally to
avoid rebuilding again while the wider replacement proceeds. This is not a
passing full build. Timing tables, layout metadata, remaining decoder/register/
M/device wrappers, public references and final tests remain pending.

### Storage, iterative arithmetic and device replacement

The decoder is now `Decoder<AddressBits>` with an explicit zero-output policy.
The register file uses one optimized decoder/read-tree/storage-bank recipe at
all 30 supported width/count/zero-policy configurations. The compact write
rule also uses geometry parameters; RV32I parents share that rule. The fixed
register-file class, wrappers and IDs are removed. M arithmetic uses its
existing parameterized iterative recipe at 32 bits as well as smaller widths;
the compact gate-list evaluator takes a width. Only its previously characterized
32-bit configuration is selectable in compact fidelity.

Bus adapters and pixel selectors now construct their generic recipes at every
supported width, including 32. Console, timer, button and display devices retain
their defined 32-bit MMIO ABI and existing configurable geometry/queues/divisor;
their public names and IDs describe the device rather than that ABI width.
No widening of the guest register ABI is claimed. This follows the default
option of the earlier optional question; no answer was required for these
independent component retirements.

Bounded syntax compilation passes for the generic register-file, decoder, mux,
iterative arithmetic and device implementations. The initial device check ran
during an incomplete caller migration and failed on removed symbols; the repeat
passes after migrating those callers. Full linking, timing regeneration and
runtime checks still remain.

### Owner clarification: architectural register file exception

The owner explicitly permits the fixed 32-by-32 register file. This replaces
this record's earlier statement that its public class and contract are retired.
`RegisterFile32x32` and `rv32i.register-file` remain actual architectural
components, using the same generic decoder, storage and read-tree recipe.
Other geometries use the parameterized storage families. No alias redirects
a retired ID to that architectural contract. Its compact implementation also
uses the geometry-aware shared rule. Other retirements continue unchanged.

### Defaults, selectors and visualizer metadata

The generic configuration, ALU, adder and add/subtract constructors default to
parallel-prefix carry. Ripple configurations are explicit, including the
recorded comparison/reference scenarios. This preserves the optimized branch/
prediction/PC arithmetic when fixed 32-bit constructors are replaced.
Architecture profiles select the generic 32-bit integer contracts explicitly;
retired rules no longer hold profile fingerprints artificially stable.

The old contract-name normalization redirects are removed. Current memory/
storage callers and selectors use exact IDs; rejection checks cover retired
names. The allowed `rv32i.register-file` remains a real contract. Generic
visualizer layout policies now retain width parameters and parse mux geometry;
decoder zero-output policy has its own actual type. Current documentation is
staged under the local build directory pending the source/timing/example checks.

Native rebuilding is isolated from the deployed Python module. A local
compiler launcher uses the bundled native Clang for ordinary translation units
and retains the conservative GCC settings for large table/cache units; both
run serially under the same address-space and host-memory guard. This speeds
recompilation while retaining bounded allocations. It is build-local tooling,
not a repository toolchain requirement or a full GCC build claim.

### Build fixes and contract audit

The first full build found an omitted decoder header in the new width cases;
a subsequent build found a stale renamed test-header reference. Both were
corrected. All current source includes now resolve. A source inventory finds
no remaining fixed arithmetic/mux/decoder/device classes; the private cache tag
matcher also uses a width template while the memory protocol continues to use
32-bit byte addresses. Negative API/rejection assertions deliberately name
retired components. The architectural register-file exception remains.

The standalone width sweep is explicitly hidden from the visualizer menu;
recorded width scenarios remain available. Scenario routes replace the old
fixed-component routes, and 37 saved root route keys have migrated without
adding redirects. The serial build is still in progress; no current-source
runtime pass or timing-table verification is claimed yet.

### Current contract families

| Hardware | Current identity |
| --- | --- |
| Word arithmetic / ALU slices | `word.<unit>.<W>[.<operation-contract>][.<topology>][.core]` |
| Operand-independent ALU control | `word.control.extended`, `word.control.integer` |
| Multiplexer | `logic.mux.<Inputs>.<W>`; fan-in 2/4/8/16/32 |
| Decoder | `logic.decoder.<AddressBits>.full` or `.zero-disabled`; 1–5 address bits |
| Configurable register file | `storage.register-file.<W>.<Count>.zero` or `.all` |
| RV32I architectural register file | `rv32i.register-file`, `RegisterFile32x32` (approved exception) |
| Iterative M arithmetic | `arithmetic.iterative.multiplier/divider/mul-div.<W>` |
| Device register frontend | `device.word.register-port.<W>` |
| Bus routing / guard / absent target | `bus.word.address-router/instruction-guard/error-target.<W>` |
| Packed pixel selection | `video.pixel-selector.<W>` |
| Configured MMIO devices | `device.console`, `device.timer`, `device.buttons`, `device.framebuffer` |

The decoder/address ranges, word widths and queue/display parameters are
separate dimensions. A 32-bit RGB output or byte-address bus does not imply
that the transported data word or framebuffer geometry has the same width.
The fixed MMIO ABI belongs to the configured peripheral contract, and the
CPU's ISA/register geometry is explicit. Behavioral availability stays limited
to configurations with structural/equivalence and timing evidence.

### Shared structural arithmetic and scenario identities

The M builders use the common structural helper directly; the compatibility
include shim is removed. Word additions at 8/16/32 bits all request the generic
prefix family, while smaller counter/control additions use full-adder cells.
Width no longer singles out a fixed 32-bit adder path. Recorded mux/decoder/
arithmetic roots use explicit fan-in/width/operation identities as well as the
new test names and routes. Current callers and layout references migrate to
those roots. CTest names/order are updated alongside the shared registry.

### Native build and reference consistency

The migrated component library, scenario runner, timing generator, visualizer
probe, arithmetic timing test, register-file integration test and retirement
contract check have linked in the isolated native build. Python registrations
are being rebuilt without deploying the module. This is build evidence, not
yet a current-source runtime pass. A subsequent runner link includes the
scenario helper naming cleanup.

The staged public reference uses `/alu/`, describes ripple and parallel-prefix
adders as configurations of the word adder, and makes parallel-prefix carry
the ALU default. Decoder control has no data-width parameter. MMIO peripheral
ABI width and configurable helper data widths are documented separately.
Final timing regeneration, focused checks and portable package refresh remain
open. Existing generic memory templates and ISA-specific components remain
general or architectural contracts; no fixed arithmetic/device compatibility
constructors are retained.

### Current native build, timing pass and layout identities

The isolated native component targets and Python module link successfully under
the 768 MiB address-space cap. The binding build exposed a missing fifth
argument annotation on the existing device-state observer; `current=false` is
now declared explicitly, preserving its ordinary four-argument use. The
resource-constrained local launcher uses bundled Clang for bounded native
compilation and GCC with conservative collection for the cache translation
unit; this is not evidence of a complete default-toolchain build. The deployed
Python module is untouched.

Full unit-profile timing generation with `--clocked` produces 809 entries in
1123.133 seconds, maximum RSS 368172 KiB. Embedded/desktop generation is still
running; no partial timing set is installed. Rebuild, recomputation and focused
runtime checks remain pending. The extra default compact table belongs to the
16-input width-32 mux.

A topology-only migration inspected 204 representatives and migrated 31
verified default root-layout identities. It inspected 27 profile identity
mappings and copied the corresponding layout data to current scopes; it adds
no runtime alias/redirect. Unidentified custom profiles are preserved. The
automatic-geometry dry run is still in progress. Its source-data backup and
migration inventory are retained in the ignored evidence directory.

The building-block layout helper now uses `create_layout_topology` directly;
its former scenario-setup path actually replayed stimuli. It and the pruning
tool honor an isolated backend at the front of `PYTHONPATH`. Layout pruning
uses all visualizable registry descriptors, including generated M/device/word
scenarios, rather than searching only the central registry source. The layout
guide documents these commands.

### Automatic geometry and pin footprints

The 47-scenario dry run and write pass visit 88945 component instances. Both
report 218 automatic scopes, 58 changed scopes and 117 preserved custom
floorplans, with 33 unmatched and 688 unvisited saved scopes retained.
Subsequent default-topology reconciliation exposed overlapping pin footprints
in the advanced CPU memory-stage floorplan; that failed attempt saved nothing.

The CPU floorplan provider now receives each child's actual stub footprint.
A slot keeps its existing placement size when it fits; oversized horizontal
and vertical pin footprints shrink to the allocated slot. This replaces the
assumption that a single fixed clearance covers every child style. The
regression exercises the actual memory-stage topology with three gate-pin
sizes. Its execution and the second reconciliation are pending; no passing
geometry result is yet claimed for this correction.

### Second timing profile and complete scenario URL migration

Embedded-profile generation completes with 809 entries in 1186.244 seconds;
desktop generation is running. All timing profiles remain staged until the
complete set succeeds. No current timing-rebuilt runtime result is claimed.

The second topology reconciliation passes and migrates 67 default scopes.
The URL audit then exposed seven mux aliases that were still using generated
fallback names and the combined M unit's fixed-name URL. All ten mux URLs now
follow fan-in/data-width names; the combined unit uses `muldiv-w32`. Seven
additional root fingerprints and nine default topology scopes migrate. Every
staged example URL resolves to its declared current registered test. Python
source parsing and whitespace checks pass. The pruning review finds no retired
type/profile entries and only two nonvisual infrastructure root records; they
are unrelated to the component replacement and retained.

The pruning tool recognizes bracketed structural/configuration layout keys
when extracting their live base type, preventing valid cache, predictor and
configured-display entries from being falsely retired. No broad pruning was
applied. Site installation and visual/browser validation remain pending.

### Focused topology validation

Saved static placements pass in 21 selected scenarios: extended ALU at all
three widths, ripple/prefix adders, integer full/core ALUs, representative mux
fan-in/data widths, the decoder and architectural register file, M units,
device port/router/guard, display and integrated device system. The first
command covered 20 scenarios; its misspelled guard argument was silently
skipped by the checker. The checker now rejects unknown aliases, and the
correct guard alias passes in a separate run. This is topology/placement
evidence only, not a simulated game or browser pass.

The memory-stage footprint regression passes for all three gate-pin sizes
(1 test, 1.589 seconds). This verifies the earlier floorplan correction. The
shared unit-profile table comparison also finds all 780 retained-ID entries
unchanged; the remaining 29 current entries are replacement IDs and the new
16-input mux. Final native runtime validation is queued after the complete
three-profile installation and rebuild.

### Three timing profiles generated

Desktop generation completes with 809 entries in 1110.059 seconds. Unit,
embedded and desktop have identical key sets; the generated gate-list exclusion
inventory is in progress before atomic installation of the complete set. The
serial native rebuild and 22 focused component/integration checks plus two
binding checks are queued. The direct sign-bit comment and an add/subtract
allocation reservation now use width-neutral wording and `W + 1`; neither
changes circuit topology.

### Timing installation and focused native acceptance

All three generated profiles and the regenerated exclusion inventory are
installed. The 780 retained-ID tables are identical to their previous values
in unit, embedded and desktop profiles; every profile has 809 entries. The
exclusion inventory is unchanged. No timing keys were manually renamed.

The timing rebuild passes in 172.785 seconds (maximum RSS 400712 KiB),
including the current library, native runner/probes and isolated Python
module. All 22 focused native checks pass in 274.45 CTest seconds (maximum
RSS 226704 KiB): retired-ID rejection, catalog ownership/inventory, exact
profile selection, registry order, generic width/ALU tests, four-state
structural/behavioral equivalence, arithmetic timing/incrementer cases, both
decoder policies, register-file FF integration, the 32-bit M units, compiled
RV32IM program, single-cycle Execute, display and device inspection.

Both Python binding checks pass in 20.56 CTest seconds: current generic
construction/type/pin APIs and device parameters, including larger display
geometry. The checks reject fixed arithmetic/mux/decoder symbols. Native
commands ran serially under the 768 MiB address-space cap; this is a focused
subset, not the full test suite or phase-6 game campaign.

The serial WASM rebuild has started, using the existing O1 link configuration
and bounded compiler/Node settings. Complete-file timing verification, public
reference installation, generated examples and portable/browser/preview
validation remain open.

### Portable build memory boundary and reference review

The first WASM rebuild stopped at the compiler's 640 MiB address-space limit
while compiling `Cache.cpp`; the log reports LLVM allocation failure rather
than a successful build. The isolated launcher now compiles only that source
at O0 with a 1 GiB address-space limit, keeping other sources at 640 MiB,
serial compilation, Node's 256 MiB heap and a 1500 MiB available-memory start
guard. That source succeeds on retry. The deployed native module is untouched.

Portable ALU parity coverage now includes the shared default-prefix generator
at 8/16/32 bits and both 16-bit adder scenarios. Behavioral selection is based
on the component's advertised available fidelities rather than a width in its
test name. These added portable cases have not run yet. Reference review also
corrects the hierarchy coverage description and removes historical wording
from the staged current ALU page. The palette test uses the decoder's actual
zero-disabled type rather than a retired layout suffix.

### Portable packaging preparation

The serial WASM retry has compiled the replacement arithmetic, device and
scenario sources and linked the simulator library. Its remaining generated
timing chunks compile at O0 under the same 640 MiB limit; gate sources and
the final O1 link retain their existing optimization settings. The first
chunks already completed at O1; this is a local mixed-optimization build,
not a release-toolchain performance measurement.

The public-source installation dry run finds 41 changed files and nine
removed files (the seven `alu8` pages, old ALU figure and RV32I `alu32` page).
Every live public file still matches the preserved pre-retirement backup.
Installation will require a successful staged strict build and will retain
the existing 8893 simulator directory as a backup. Fresh Pong recording,
package and provenance receipts are queued from the rebuilt native probe;
the old receipt will not be reused for the new binary.

### Portable build passed; capture and final metadata audit

The WASM retry passes in 23:11.30, maximum RSS 517168 KiB. The fresh native
Pong capture is running with the 768 MiB address-space cap, bounded
presentation-only observation and history disabled. No new game result is
claimed until its independent reference comparison passes.

The final palette inventory finds and removes 17 unused, nonexistent fixed
component entries (wide gate/comparator/arithmetic placeholders, two 16-input
mux names and `Decoder2to4`). None is a saved-layout type. Current parametric
prefix colors remain. The staged portable package receives the current
palette; the live preview is still the earlier coherent checkpoint.

### Fresh Pong capture and exact reference passed

The current capture completes 115,686 cycles and 29,005 instructions, with
18 presentations/checkpoints and zero setup violations. All 18 exact frames,
stable reports and input-consumption checks pass the independent host C
reference. The observer retains the existing non-mutation assertions.
Capture wall time is 623.797 seconds, execution 613.0546424 seconds, maximum
RSS 151348 KiB. The current native recording is produced under the 768 MiB
address-space cap; no second game execution is used for the reference check.

The old and current captures have identical complete event counters
(56,646,379 processed/scheduled events), cycles, instructions and topology
sizes (6,426 components, 17,688 pins, 8,070 wires). Wall time is higher than
the earlier 341.4513218-second execution; no equal-throughput or controlled
performance-regression conclusion follows from these different build/host
runs. The functional/event-count equivalence is direct recorded evidence.

Current runner SHA-256:
`21dfbd735702f6dbdb591740fd8ea24ae64bffeac15cdb1d9c69ee1f51fea867`.
Current sorted source-manifest SHA-256:
`10d166e0589824c09d856c9c8a0645a9c723b43b85eb0aa6a15f68cd6d822db0`.
Source, binary and fixture-manifest hashes are unchanged across the capture.
Fresh packet/layout/package/evidence files replace all four staged Pong assets.
Complete isolated-process WASM example generation is now running.

### Retained-state comparison with the pre-retirement Pong capture

All 18 complete indexed pin vectors, wire vectors and simulated checkpoint
times are identical before and after replacement. The guest record differs
only in setup/execution/capture host durations and each frame's `hostSeconds`;
all other frame metadata, pixels, masks and input snapshots agree. This
artifact comparison reuses the two actual captures rather than executing
another guest. Evidence: `pong-replacement-state-comparison.json` under the
isolated ALU evidence directory. Different component type/contract identities
are still freshly captured and packaged, not rewritten into the old trace.

### Complete examples, strict public package and live installation

All 158 unique groups pass isolated-process WASM generation in 611.912
seconds: 134 component example groups and 24 programs with reference PC/status
checkpoints. All 176 CPU program root layouts pass portable packaging checks.
The first strict-build attempt fails because the shell's Anaconda Python
lacks MkDocs. The docs virtual environment passes a 98.83-second strict build;
a final build with explicit staged snippet paths and `check_paths=true` passes
in 115.86 seconds (maximum RSS 193732 KiB). No example rows are hand-edited.

The replacement public sources/configuration and matching simulator/capture
package are installed coherently on 8893. All live public sources matched the
preserved backup before installation. The old preview package remains at
`build-wasm/pages/simulator-before-component-retirement`; native deployed
extensions are untouched. The service retains its 768 MiB/no-swap limit.

Live HTTP checks pass all six new document/simulator routes, including 262
adder-page rows and 162 complete-ALU-page rows. Thirty non-HTML artifacts match
exact hashes. An initial literal HTML hash assertion detects MkDocs' expected
live-reload injection; verifying that exact script separately proves both HTML
entry points otherwise match the installed source. Old ALU8/ALU32 document
routes return 404; no redirects are installed. Evidence: `retirement-live-http.json`.
Complete unit-file timing verification is running before the final focused
reference/catalog/portable/browser panel.

### Complete timing verification and final documentation corrections

Complete-file unit verification passes in 1307.470 seconds, maximum RSS
372524 KiB, after rebuilding the catalog with the regenerated file. It
recomputes the whole 809-entry unit file and checks the gate-list exclusion
inventory. Embedded/desktop were completely regenerated from gates but were
not separately rerun through `--verify`; no all-profile re-verification claim
is made.

Four of seven focused reference/metadata registrations pass initially:
DocumentationDiagrams, RV32IBuildingBlocksDocumentation, TimingTableDocumentation
and WebTestRunner. Three fail and are being corrected/rechecked:

- The single-cycle documentation test confuses the renamed ALU scenario alias
  with its Markdown filename. It now reads `rv32i/4.3/alu.md`.
- The arithmetic page still contains three explicit eight-bit anchor aliases.
  They are removed, preserving only current generated headings and the
  width-independent arithmetic-flags anchor. This corrects the earlier
  incomplete anchor migration; no old arithmetic anchors are kept.
- Generic adder prefix colors used an unrelated hue despite the arithmetic
  cell defaults. Adder and parallel-prefix families now use related purple
  colors at every width/topology. The color test checks all six adder choices.

Only those three failed registrations are queued for repetition. Current
native simulation source and generated timing values are unchanged by these
Markdown/test/palette corrections. The exact-reference Pong capture remains
current. Preview strict rebuilding and a final isolated strict package build
cover the corrected reference before browser acceptance.

### Final reference, catalog and browser acceptance

The three corrected registrations pass in 95.788 seconds, including all
compiled binary references in DocumentationExamples and single-cycle stages.
Together with the initial four passes, all seven selected documentation/
metadata registrations pass. Four selected scenario-catalog methods pass in
24.841 seconds: complete presentation, classification, static variant parity,
and the existing client/registry boundary. The final production-config strict
package build passes in 101.639 seconds using the unchanged 158-group bundle.
Current live HTTP/hash/retired-anchor checks pass again after the corrections.

Both ALU portable suites pass: IntegerALU in 27.191 seconds (nine scenarios),
ExtendedALU in 69.757 seconds (21 scenarios plus advertised behavioral
selections). This includes all three primary prefix-ALU widths and both
16-bit adder recordings. The first browser attempt fails before launch because
the docs virtual environment lacks Playwright. Selecting the installed system
browser Python resolves that environment error; passed parity checks are not
repeated.

The four remaining checks all pass serially (628.55 seconds total,
maximum time-reported RSS 347228 KiB):

- AdderDocumentationBrowser: 19.086 seconds; six sections/snippets, mobile/theme
  bounds, and 80 exact half/full-adder checkpoints in actual WASM embeds.
- RV32IMDeviceWasmParity: 318.442 seconds; all 22 recorded M/device scenarios,
  full topology/examples, sampled states, device observations and rewind.
- RV32IMDeviceBrowser: 226.663 seconds; all 23 component pages and three
  selected actual WASM sessions (primary ALU, display, buttons), every declared
  row, observer/inspector/toggle behavior, and mobile bounds.
- RecordedCircuitBrowser: 64.039 seconds; the fresh Pong topology and every
  retained pin/wire/display/input state, original inspector, rewind, playback
  and device toggle, without starting a WASM CPU for capture replay.

These are six distinct passing portable/browser registrations across the two
runs, not a full web suite. Screenshot review confirms the seven-block
16-bit prefix ALU and actual Pong screen in the original display/inspector.
Evidence: `retirement-acceptance-summary.json`, the preserved attempt/result
logs, and `retirement-browser-screenshots/` under the isolated evidence tree.

### Closure and remaining milestone boundary

Fixed-configuration component retirement is complete on main alpha: public
classes/factories, contract IDs, callers, scenarios, profiles, layout data,
bindings, timing artifacts and current references use the parameterized
implementations. No old-ID aliases or document redirects remain. The actual
`RegisterFile32x32` architecture exception shares the generic register-file
recipe. Configured peripherals retain the defined 32-bit MMIO ABI.

The source/ID audit, changed-Python parsing and `git diff --check` pass.
Builds and simulations were serial, resource guarded and isolated from the
native deployed extensions. Existing dirty work and the sole main checkout
are preserved. No commit, push or published-site verification is claimed.
The full native/web suites and broader phase-6 game campaign are unrun at this
replacement snapshot. Phase 6 and live input/phase 7 are separate open work;
this closure does not mark those milestones complete.


### Final larger-display CPU evidence

`RV32IMLargerDisplayTest` passes on the replacement binary in 23.55 CTest
seconds (23.70 seconds wall, maximum RSS 152668 KiB), under the 768 MiB cap.
It executes both 128x96 and 97x65 configurations, including CPU geometry
reads, a high video address beyond the former 4 KiB aperture, PRESENT,
exact capture geometry/pixel checks, all-known pixels and zero setup violations.
Console/button queue depths 2/7 are configured in the same actual systems.
This adds one current native registration to the earlier 22: final focused
acceptance is 23 native plus two API registrations. Current direct API tests
also cover 320x200; this does not claim compiled 320x200 game execution.
