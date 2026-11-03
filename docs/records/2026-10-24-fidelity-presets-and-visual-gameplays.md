# Fidelity presets and visual gameplay campaign

Started: 2026-10-24. Status: closed. Code state: `alpha` at `5549ee0`, preserving
the pending component/layout/menu work and existing recording evidence.

## Summary

The normal native/WASM visualizer offers shared named fidelity presets with
isolated custom overrides. Compact device compositions and bounded topology
chunks support structural game recording; scoped implementation checks pass.
The approved 146-frame checkpoint campaign published Snake (10), Pong (45)
and Sokoban (34), all with exact native/reference checks: 89 frames total.
2048 stopped under host memory pressure and Tetris remained incomplete.
The owner then requested intervening simulation ticks. Both the frame-only
unit and its deferred retry are stopped, with all saved evidence preserved.
The [full-tick follow-up](2026-10-25-full-tick-gameplay.md) supersedes this
campaign and recaptures the same approved scripts. Unfinished frame-only
games are not claimed complete.

The follow-up device-system preset repair is complete and verified on live
8893: Scenario default/Logic/Visual/Compact open, saved geometry survives,
and strict Gate is disabled with an early readable capacity reason. The
preset repair remains in place for the full-tick follow-up.

## 2026-10-24 — Scope and implementation decisions

- `gate`: structural selections, including storage; unsupported capacity or
  a conflicting required behavioral child is an error.
- `logic`: structural computation/control/interconnect; behavioral register
  words, state bits and register file, with behavioral large memory storage.
  Memory interfaces/controllers stay structural.
- `visual`: behavioral implementations for registered full visual
  abstractions; other selectable components structural. Primitive symbols,
  layouts, zoom and the Device views toggle do not alter the build policy.
- `compact`: prefer verified behavioral implementations, explicitly record
  fixed structural-only components. Existing scenario defaults remain an
  available selection to preserve current reference examples and saved work.

Four peripheral families have views but no compact factory. Reuse their
existing structural control builders and proven smaller models to create
compact control implementations; retain observer boundaries needed by the
runtime and inspector. Add equivalence coverage before using these forms in
the new queue. The named preset and actual selections must identify each
new recording; earlier compact captures are never relabeled.

The native and WASM selectors must rebuild the selected circuit, carry the
choice in the URL, preserve manual explorer choices without silently
mislabeling a custom build, and keep recorded replay fixed to its capture.
Builds and tests stay serial and bounded. The background queue starts only
after a real bounded visual-profile preflight; launch is not full-game success.

Stopped `circuitsim-long-gameplays-1792818961` through its existing CLI.
The earlier campaign record and `build-milestone/long-gameplays-20261024`
remain evidence for that compact run. New visual runs use a separate output
and fresh publication identity, preserving the old partials and short captures.

## 2026-10-24 — Native checks and export capacity

The shared C++ policy, native session cache/API and WASM selector carry preset
identity independently from timing. Explorer overrides are isolated per preset
and marked custom. Four compact peripheral compositions reuse their existing
control blueprints and characterized smaller models. The catalog retains its
leaf requirement except for explicitly validated `BehavioralComposition` roots.

Six focused native checks pass: console, timer, buttons, framebuffer, device
system lockstep (including a visual structural-CPU guest) and initial preset
policy checks. Total 110.10 seconds, peak 489,532 KiB RSS, no swap under the
768 MiB address-space cap. Initial composition validation/factory-order failures
were corrected before that pass; failed logs remain in the evidence directory.

A full 64×48 visual game has 26,833 components: 10,522,260 topology-header bytes,
51,595,174 scope bytes across 53 files (largest 1,566,229 bytes), and 798,161
checkpoint bytes. The legacy inline packet cannot fit. The revised exporter
streams scopes and shares one graph across schema-2 checkpoint parts. Files
stay below 32 MiB, native retained states below 24 MiB and 16 presentations per
part; the browser keeps two scopes and one state window. No CPU detail or
presentation is sampled away. The updated capacity test passes at 640 MiB.

A real two-PRESENT RV32IM ELF with presentation-paced button input passes
`LongGameplayProbe ... 2 1 visual`: 10.83 seconds wall time, 280,468 KiB peak
RSS, zero swaps, both presentations retained, zero setup violations. Both parts
package successfully against the external topology. This is a runtime/export
smoke check, not evidence for full long-game completion. Evidence lives under
`build-milestone/fidelity-visual/`; key measurements are retained here.

## 2026-10-24 — Portable package and API checks

The serial O1 WASM build and strict Pages package pass. Four affected scenario
example groups were regenerated in isolated processes; unrelated verified rows
were retained from the previous bundle. Native profile persistence, timing,
new preset isolation and both recording-format contract checks pass (four
checks, 57.94 seconds). An existing exact-payload assertion was updated to
include the new explicit preset metadata; the earlier failed run is preserved.

The five reference-preflight sessions are frozen in
`build-milestone/long-gameplays-visual-20261024`, initially queued under
`circuitsim-long-gameplays-1792838150`. Preparation alone does not start native
play or establish completion. Browser/portable parity and actual launch remain
the next acceptance steps.

## 2026-10-24 — Acceptance and background restart

Eight native/WASM parity cases pass in 20.10 seconds: all four register presets
and visual console/timer/buttons/framebuffer scenarios. Actual component trees,
profile fingerprints, timestamps and three indexed states match. The generic
`VisualizerSessionProbe` accepts `--fidelity` for this equipment.

Browser profile acceptance passes in 42.92 seconds: all four actual choices,
URL state, per-preset custom isolation, independent embedded timing, 1440/800/400
widths and recovery from a genuinely unsupported gate build. The initial test
used a small native memory that correctly supports gate fidelity; that mistaken
expectation was replaced with the scalable endpoint's required compact child.
The failed test output is retained; no product restriction was added.

The real two-part visual guest replay passes in 14.60 seconds: all retained
pin/wire vectors match the native capture, display and buttons agree, inspector,
Play and rewind work, fidelity/timing are locked, and no WASM worker starts.
The legacy schema-1 path also passes contract checks. The strict preview build
passes in 94.81 seconds. Tested native modules and WASM/static files were
staged with complete backups under `build-milestone/fidelity-visual/predeploy`.

Started `circuitsim-long-gameplays-1792838150` with the frozen visual executor.
Systemd reports active/PID 3677723, MemoryHigh 640 MiB, MemoryMax 768 MiB and
MemorySwapMax zero. Pong is running (901 frames), followed serially by 2048
(513), Sokoban (303), Tetris (604) and Snake (769). Actual reference matches,
all frames and graph/manifest hashes are required before each publication.
The original unit remains inactive. Progress, attempts and evidence are under
`build-milestone/long-gameplays-visual-20261024/campaign.json`; the public index
is `assets/rv32im/long-gameplays/visual/index.html`. No full native game is
claimed complete at launch. Full suites and long native/WASM acceptance were
not run as part of the focused feature checks.

## 2026-10-24 — Measured budget correction and final launch

This section supersedes the initial launch's seven-day limit and service ID.
The first visual Pong attempt reached 4,096 cycles and 1,030 retired
instructions with no presentations yet, at roughly 20 cycles/second. Its
service peaked at 238,669,824 bytes with no swap. This fidelity can require
days or weeks per game; the earlier compact throughput does not apply.

The inherited seven-day wall and 100-billion-event limits could truncate the
requested long structural sessions. Increase native wall allowance to 90 days
(configurable, bounded to 1–180), total events to 10 trillion and retain the
100-million-cycle/instruction, frame, memory, CPU and disk limits. Packaging
retains its seven-day budget. No lower-fidelity substitution or frame sampling
is used. The final native probe repeats the actual two-frame guest in 11.91
seconds, 280,524 KiB RSS, zero swap. Circuit checkpoint JSON, pixels, guest
results and event counts match exactly; host-duration fields are excluded
from deterministic comparisons. An initial comparison included `hostSeconds`
and correctly differed; no guest/circuit discrepancy was found.

Preserve both startup attempts, stopped campaign manifests and publication
snapshots in their original output directories. Final immutable campaign:
`build-milestone/long-gameplays-visual-20261024-r3`, unit `circuitsim-long-gameplays-1792839335`.
Its frozen source manifest explicitly records 90 days, 10 trillion events,
100 million cycles/instructions, 16-checkpoint parts, 640 MiB native address
space, 768 MiB service memory, no swap and host-headroom/disk guards. Pong runs
first; 2048, Sokoban, Tetris and Snake remain queued. No full game is claimed
complete. The earlier compact and both startup units are stopped.

Live 8893 acceptance passes after deployment: seven JavaScript/CSS/WASM assets
match the tested package hashes (HTML is served with MkDocs reload injection),
actual Visual/Gate selections work with embedded timing, the existing 18-frame
Pong-rally capture locks fidelity/timing, both campaign routes return 200 and
there are no page errors. The receipt is
`build-milestone/fidelity-visual/live-receipt.json`.

## 2026-10-24 — Final service verification

The final r3 queue reaches 4,096 cycles and 1,030 retired instructions with
Pong active and the other four jobs pending. Zero presentations at this early
checkpoint is expected; no full recording is published yet. Systemd confirms
PID 3684231 active, 110,854,144 current / 117,989,376 peak service bytes and
zero swap. The 8893 public manifest matches the final unit and visual preset;
all frozen executor hashes match their source manifest and all three earlier
units are inactive. The receipt is
`build-milestone/fidelity-visual/background-final-receipt.json`.
The final strict preview build passes in 160.59 seconds; live simulator route
returns 200. The source diff has no whitespace errors. Full long-game outcomes
remain pending in this intentionally continuing background campaign.

## 2026-10-24 — Demonstration length review

The owner questions the 901-frame Pong target and requests reviewing all games
for demonstration use. Review current C rules, render/input path and all frozen
scripts; replay every existing input batch through those actual C rules, then
prepare shorter candidates. Production game rules, fidelity, native binaries,
visualizer behavior and the active background unit are unchanged by this audit.

| Game | Long frames | Candidate frames | Host-verified content |
| --- | ---: | ---: | --- |
| Pong | 901 | 45 | Both paddles move, one paddle return, three wall bounces, one complete point |
| 2048 | 513 | 25 | 24 moves, 18 merging moves, tile 32, score 160 |
| Sokoban | 303 | 34 | First three-crate puzzle solved in 33 moves with 11 pushes |
| Tetris | 604 | 32 | Four locked pieces, seven rotations, nine lateral moves and the first line clear |
| Snake | 769 | 10 | Nine legal moves, six turns, three foods, length grows from three to six |

Counts include the initial PRESENT. Total falls from 3,090 to 146 frames
(95.3% reduction). Pong's original player delays a miss until after 160 updates,
then repeats rounds; only the first completed point is useful for this candidate.
The 2048 long script plays 512 moves across two boards; the first 24 already
show repeated merging and growth. Eight Sokoban puzzles and 88 Tetris pieces
repeat mechanics demonstrated by one solved puzzle and the first line clear.
Snake's long safe circuit waits for distant food; the candidate finds short
legal food paths, applying the same actual C movement/collision rules.
No time skipping, changed ball speed, injected food or altered game rules.

Build all five RV32IM ELFs with the pinned GCC 12.4 compiler; audit every text
instruction. Run their complete host guest/control/renderer paths, compare
score/status/fingerprint to the planned state, require one frame per input
batch plus startup, and check at least 85% of updates change pixels. All five
pass. This is host-reference/instruction-audit evidence, not native circuit
execution of the shortened guests. Candidate metadata, ELFs, disassemblies,
button scripts and full expected frames/reports are in
`build-milestone/demo-review-20261024/candidates/`; that plan is explicitly
marked `demonstration-candidates-reference-passed`, not a running campaign.
Read-only review output and reproducible preparation scripts are in its parent.

An indicative cost model uses observed long-Pong cycles per frame and earlier
short native captures' CPU-cycle deltas for the other games, rather than their
compact wall times. It estimates 3.57 million cycles / about 33–50 hours at
20–30 cycles/second. Different core fidelity and action costs make this only
a rough projection; allow roughly 1.5–3 days for the whole visual suite, with
hours per game. Reducing recorded frame count alone would not save execution;
these candidates actually end guest scripts after their selected milestone.
The current long queue continues while these recommendations are reviewed.

## 2026-10-24 — Approved shorter demonstrations and restart

The owner approves the reviewed candidates and explicitly requests cancelling
the running queue and starting the short games in the background. Stop
`circuitsim-long-gameplays-1792839335` through its CLI; systemd reports
inactive/PID zero. Its last report is 200,704 cycles, 58,515 retired instructions
and 12 presentations after 6,906 seconds. Preserve the campaign, logs and
written topology in `build-milestone/long-gameplays-visual-20261024-r3`.
No 16-checkpoint packet had flushed, so there is no saved complete state part
from that interrupted attempt; the 12 in-memory presentations are not a
published or retained frame suite. Existing short native recordings remain.

Add `prepare-long-gameplays.py --demonstrations` backed by the reviewed input
plans in `tools/rv32im/long_gameplay/demonstrations.json`. Rebuild with pinned
GCC 12.4, audit every text instruction and rerun all five complete host
control/render references plus the three-frame pacing fixture. Every check
passes. Each game's regenerated `guest.elf`, `batches.txt` and `expected.json`
hashes exactly match the reviewed candidate files. No game-rule, renderer,
engine, profile or native probe changes are needed.

The shared campaign CLI recognizes demonstration metadata, publishes under
`assets/rv32im/gameplay-demos/visual`, gives recordings `demo-*` identities and
leaves earlier long identities intact. Snake runs first to provide the first
complete example sooner. The original longer preparation mode remains available.
The part-size metadata is corrected to the existing 16-checkpoint export.

Launch the frozen short queue in `build-milestone/short-gameplays-visual-20261024`,
unit `circuitsim-gameplay-demos-1792846492`. Targets are 10/45/25/34/32 frames, 146 total.
Its binary, Node, packager/transport/layout and prepared input/reference files
are frozen with hashes. Keep serial execution, 768 MiB service / 640 MiB native
address-space caps, no service swap, CPU quota/priority and host-headroom/disk
checks. Publication still requires the actual full native frames, report and
input logs to agree with references; launch does not establish those results.
Evidence is under `build-milestone/short-gameplay-launch-20261024/`.

## 2026-10-24 — Short queue verification

The new unit reaches 16,384 cycles / 4,382 retired instructions with Snake
active and the four later games pending. Systemd confirms PID 3784368 active,
103,747,584 current / 114,900,992 peak service bytes, a 768 MiB hard cap and
zero current/allowed service swap. Frozen executor hashes and all 15 approved
ELF/input/reference hashes match. The earlier long unit remains inactive.

The strict preview build passes in 140.66 seconds. The new live 8893 index,
manifest and canonical directory route return 200; their unit, Visual preset,
146-frame targets and execution order match the campaign. The recording-library
link resolves to the new index, and the cancelled long index points there.
An initial link assertion expected literal `index.html`; MkDocs correctly
canonicalizes it to the directory URL. The corrected check accepts that valid
canonical form and passes without a product-code change. No new browser/WASM
suite or hardware build was needed for input-data/launcher/documentation work.

Receipt: `build-milestone/short-gameplay-launch-20261024/launch-receipt.json`.
Progress: `build-milestone/short-gameplays-visual-20261024/campaign.json`.
The host-reference checks and healthy launch do not claim full native games
have finished; all publication reference checks remain enforced at completion.

## 2026-10-24 — Device-system fidelity failures and repair

The owner reports all named selections failing on `rv32im-device-system`.
Reproduce each on live 8893 in fresh browser contexts. Scenario default works
with 22,275 components. Logic builds 36,341 but lacks a placement for
`MemoryController<ff,32>.NOTHING_HELD`; Visual (26,833) and Compact (610) lack
`ConsoleRegisterDecode.MODEL`. Gate instead reports `[object Object]` from a
C++ exception while attempting expensive structural construction. Evidence:
`build-milestone/device-system-fidelity-fix/live-errors.json`.

The worker previously merged only saved/default layouts; it never completed
new topology variants when a profile changed. Add a browser-side completion
pass that reuses compatible placements or generates bounded grids. Generated
variants are keyed by child structure, preserving original/manual/unrelated
family entries and preventing structural/private compact groups with the same
TypeName from clobbering each other's geometry. Generate matching defaults for
Reset Layout. Use actual pin-derived/user-configured aspects before placement.

Strict Gate cannot represent the fixed 256 KiB system RAM. Add shared contract
restrictions, native/WASM scenario prechecks and a recorded-runtime precheck,
before constructing a full structural core or loading its ELF. Expose disabled
preset reasons in payloads and the selector. Do not silently compact storage
under Gate. Enable Emscripten exception messages/stack traces and format caught
C++ messages rather than displaying `[object Object]`. The policy and fidelity
names remain version 1; no circuit or delay table changes are made.

The isolated native build passes. Preset/capacity/early rejection checks,
native API isolation and existing profile migrations pass. The new layout
regression initially had a test-data brace typo; corrected syntax and its
scope-reuse, bounds, idempotence and saved-layout checks now pass. Failed logs
remain retained. Portable/browser and strict packaging checks are next.
The native game service and all its frozen binaries/files remain untouched.

## 2026-10-24 — Device-system preset acceptance and deployment

The focused native preset/capacity/precheck, API isolation, existing profile
migration and new layout tests pass. Native Python API builds also pass for
the actual system: Logic 36,341 components (structural core/devices), Visual
26,833 (structural core/behavioral devices), Compact 610 (behavioral core/devices).
These use an isolated temporary layout store, preserving the workspace layouts.

The serial O1 WASM package and strict Pages build pass (108.17 seconds for
documentation). Regenerate the one affected default system example group:
four rows verified, other recorded groups retained. Existing timing/custom
profile browser acceptance passes in 43.21 seconds and eight native/WASM
parity cases pass in 20.05 seconds. The new system browser check passes in
180.23 seconds, including all supported selections, geometry bounds for new
variants, manual root placement and unrelated saved-entry preservation,
readable direct-Gate rejection and recovery. Initial checks incorrectly used
Playwright's option `is_disabled()` predicate despite a true DOM disabled
property; use the actual option property. Those failed logs are preserved.
No product constraint or layout change was made to satisfy that test issue.

Back up the live simulator/native modules under
`build-milestone/device-system-fidelity-fix/predeploy`, then atomically replace
the tested WASM/static package and native modules. Strict preview startup
passes in 111.31 seconds. Live 8893 checks prove seven JS/WASM asset hashes
match the tested package, all three named supported selections load with the
expected core/device fidelities and disabled Gate, and a direct Gate link
shows the full 256 KiB reason without `[object Object]`. There are no page
errors. The receipt is `build-milestone/device-system-fidelity-fix/live-receipt.json`.

The frozen game executor hashes remain unchanged, its service is running
and Snake reaches 192,512 cycles, 50,254 instructions and six presentations
at the live check. No background queue restart, new simulation physics,
behavioral module or delay-table regeneration was needed. Full suites and
completed-game acceptance remain outside these targeted UI checks.

## 2026-10-25 — Published games and host-memory pressure

Read the live campaign, attempt logs, service state and published evidence.
Snake completes 10 frames / 282,854 cycles in 9,908.92 execution seconds;
Pong completes 45 / 604,024 in 13,962.44 seconds; Sokoban completes 34 / 647,304
in 12,272.37 seconds. All three have `exactHostReference`, `allFramesRetained`
true, Visual fidelity and zero setup violations. Their published playlists
return HTTP 200 on 8893. These are native/reference passes, not new per-record
browser acceptance or full phase-6 claims.

2048 reaches the existing three-attempt host-pressure limit and is marked
`resource-stopped`. Its first attempt retains one raw partial packet; none is
a published completed game. Tetris's first attempt reaches nine frames before
host pressure; its next attempt waits before spawning the simulator. The
nine-frame progress fields belong to the interrupted previous attempt and
must not be presented as current retained progress. No 16-frame Tetris part
has flushed. Host available memory is about 1,047 MiB, below the 1,536 MiB
start threshold and close to the 1,024 MiB stop guard. The current service is
active with about 61 MB resident, peak 671,612,928 service bytes and zero swap.
Do not lower the safeguards or stop unrelated workloads.

The owner's request to run all five games remains active. Arm a single
resource-bounded deferred retry under `circuitsim-gameplay-retry-20261025`
(64 MiB helper cap, no swap, low priority). It waits for the current round to
finish normally with failures, then resumes the existing frozen worker once
if 2048 remains resource-stopped. The worker rechecks and skips completed
records and keeps its existing serial/native limits. The helper exits without
resuming if the campaign is explicitly interrupted/cancelled or completes;
it does not modify frozen executor files or start a second concurrent game.
Source: `build-milestone/short-gameplay-retry-20261025/retry-after-queue.py`.
The retry helper is confirmed active. Full 2048/Tetris outcomes and a reliable
ETA remain unavailable until host memory headroom returns.

## 2026-10-25 — Superseded by full-tick capture

The owner requests every intervening simulation tick, superseding the
PRESENT-only export policy. Stop `circuitsim-gameplay-demos-1792846492` and
its deferred retry `circuitsim-gameplay-retry-20261025`; both are confirmed
inactive. Keep all completed publications and partial/log evidence. The
latest interrupted Tetris attempt had reached one PRESENT, without a complete
recording; the older nine-frame partial remains historical evidence.

Close this campaign as superseded, with three verified games and two
unfinished games, rather than claiming all five completed. The same approved
146-frame scripts move to the separately frozen
[full-tick campaign](2026-10-25-full-tick-gameplay.md). Its new publication path
preserves all earlier frame-only recordings and the existing preset repair.
