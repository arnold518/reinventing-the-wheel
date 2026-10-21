# Device parameters and visual abstractions

Started: 2026-10-21. Status: open.
Code state: isolated `rv32im-phase3` worktree from `60acda2`; uncommitted phases 0–6 implementation.

## Summary

General device parameters and display/keyboard abstractions are implemented
in the isolated worktree. Native checks cover FIFO depths 1–7 in both
fidelities, standalone displays through 320×200, CPU presentation at
128×96/97×65, recorded-time observation/rewind and exact sampled-input logs.
Python declarations, shared JS helpers and all 363 static scenario layouts
pass; the layout merge adds 52 entries and changes no old value. Final
WASM/browser/site and game-comparison acceptance remain pending. Phase 6 is
open; the owner authorized phase 7 after those gates, and it has not started.

## 2026-10-21 — Audit and scope

- Standalone framebuffer parameters currently accept power-of-two widths
  4–256 and at most 4,096 visible bytes. System geometry reaches the device,
  but router, reference model and viewer retain the 4 KiB limit.
- Counter/timer divisors and declared timer frequency are already parameters.
  Console/button queues instantiate a four-entry FIFO. Its public three-bit
  COUNT constrains compatible capacity extensions to seven entries.
- Extend display geometry/storage and propagate capacity through routing,
  independent reference, frame limits and viewing. Preserve default geometry,
  addresses, pins and component identities. Check a 128×96 surface and a
  non-power-of-two width with actual memory transactions and CPU presentation.
- Add validated queue-depth parameters within the existing COUNT contract;
  prove wrap, full/empty, simultaneous operations and fidelity agreement.
- Device visual abstractions must show simulated display pixels and keyboard
  levels/events, including recorded-time inspection. Recorded controls remain
  observational; phase 7 supplies live input through the same device boundary.
- Build/test serially under explicit memory limits. The ongoing native suite
  uses the preceding binary; retain its results as dated evidence and refresh
  affected checks after rebuilding. Primary checkout work stays untouched.

Related: [phases 4–6 implementation](2026-10-21-scripted-games-phase4-6-implementation.md)
and [adopted roadmap](2026-10-19-device-io-games-plan.md#rv32im-scripted-roadmap).

### Extensions staged; verification pending

Added shared validated framebuffer configuration (positive dimensions up to
4096, at most 256 KiB), independent power-of-two backing capacity, automatic
system aperture propagation, and a matching independent device reference.
Default 64×48/4 KiB and existing component identities/pins remain. Added
FIFO depths 1–7, console/button depth parameters and system forwarding. The
default child parameter maps remain unchanged. Non-default compact parts
compile and characterize their own configured gates.

Added a portable read-only device observer for display, logical keyboard,
console, timer and FIFO, shared by Python/native and WASM viewer adapters.
Display pixels come from recorded storage, with a tiny expanded-word history
fallback; keyboard levels, retained head and overflow come from recorded pins.
A Device views control and inspector previews are staged. Recording frames
also carry independently checked keyboard snapshots and applied input events.
The viewer shares RGB332 conversion with the device abstraction and retains
old recordings without keyboard metadata. Captures have a 4 MiB aggregate
pixel budget; geometry may contain at most 256 KiB per frame.

New tests cover every supported queue depth in both fidelities, arbitrary
strides, 128×96/320×200 standalone storage, and CPU presentation above the old
4 KiB boundary at 128×96 and 97×65. These changes have not been built or run.
The preceding native binary's Tetris and 2048-merge exact reference checks
passed (809.23 s and 465.80 s). Source changes during the remaining old-binary
suite invalidate its post-run source map as build provenance; a frozen
pre-parameter source/binary receipt is retained in the isolated build. Final
current-source acceptance requires refreshed artifacts.

### Preceding native suite completed before cancellation

The intended cancellation found no owned CTest process: it had already
finished, and no signal was sent. The full capped subset completed 543 tests,
535 passing and the same eight documented failures, in 16061.15 seconds.
The final game boundary comparisons passed as well. The twelve Node-dependent
checks remain separate. These results concern the preceding native binary;
the newly staged parameter/visual changes require a rebuild and fresh checks.

### First native build and focused checks

The serial native build passes under AS 768 MiB: 3:12.36 elapsed, 369268 KiB
peak RSS. The first 28 focused checks complete in 43.36 s (150244 KiB peak):
26 pass, including all FIFO depths/widths/fidelities, display memory through
320×200 and CPU presentation at 128×96/97×65 with non-default queue depths.
The observer test exposed a structural bank type-name mismatch; the registry
checker exposed the new test's declaration-order mismatch. Both are corrected
and rebuilding. No passing observer result is claimed before the rerun.

### Native correction rerun

All 28 selected checks pass after the structural-bank type and registry-order
corrections, in 44.48 s with 149812 KiB peak RSS under AS 768 MiB. The passing
DeviceInspectorTest covers tiny expanded and compact-child pixel histories,
backward observation without events, disabled-history rejection and a tap on
logical input 31. Both larger CPU surfaces pass with zero setup violations.
JS/browser/native-WASM parity and the broader phase-6 acceptance are unrun on
this parameter/visual snapshot; those remain required before completion.

The two CPU geometry checks each retire the short 31-cycle presentation
sequence and write the last visible word above the old video range. They
measure 0.137045 s (128×96) and 0.164062 s (97×65) execution with 8320 resident
video bytes. These are sparse routing/capture checks, not whole-screen drawing
or delivered frame-rate benchmarks. The detached-descendant cleanup guard
also passes its bounded local self-check before Node/browser scopes use it.

### JavaScript and compatibility reruns

The separate 18-check scope completed in 255.25 s, peak 551928 KiB, with
sixteen passes. The new device JS check passes, including syntax for the app
and game workers, every RGB332 index, four-state keys and large/padded images.
The earlier WebTestRunner inventory, memory-documentation fixture, palette,
RV32I realistic-profile sweep (69.61 s) and memory-topology check (134.12 s)
all pass. The latter two retain their own AS 1024/2048 MiB child limits;
the outer guard no longer conflicts with them.

Two follow-ups also pass: RegisterFileProfileMigrationTest's Python child was
reported EPERM by the process sandbox despite producing output, and passes
unchanged outside it (0.41 s). VisualizerRewireGeometryTest's isolated VM had
omitted the existing `structureKeyed` helper; loading that real helper fixes
the harness without changing its geometry assertions or product behavior.
Browser rendering and final native/WASM parity remain pending.

### Larger-screen clock evidence and expanded-profile gap

The extended all-state analysis proves all three compact memory configurations
at 520 ticks and also proves compact 128×96 with console depth 2/button depth
7 at 520 ticks, within the 1000-tick operating period. Expanding the CPU
exposes six pre-existing missing contracts (fetch, four pipeline registers,
prediction path), so that profile remains `Unknown`, with no certified period.
The initial extended test failed when it required a certificate for this
already documented comparison-only profile. The revised check retains all
compact certificate assertions and explicitly verifies the exact six missing
contracts for expanded configurations. It does not accept Unknown as a safe
clock. Expanded binary runs still need independent instruction/device checks
and zero observed setup violations; their evidence is execution-specific.

### Configuration-specific floorplans staged

Added an optional ComponentLayoutView observation capability for new device
configurations. Default FIFO depth 4, router aperture 4096 and standalone 4×4
surface retain their existing layout keys. Other queue capacities, video
apertures and display geometry/storage combinations receive `[config-…]`
keys through the same native/Python/WASM topology path. Generic key handling
preserves type colors while ignoring default child placements for another
configuration. No hardware pins, type names or simulation behavior change.
Two palette prefixes cover configured router/display names; every old palette
value is retained (nine prefix and twenty-one exact-type additions in total).
Native/JS key assertions are staged for the next rebuild/rerun. The default
system's larger backing-store display needs its new saved variant floorplan;
all old saved values must remain unchanged during generation.

The new native Python `create_device_component(contract, name, parameters,
fidelity)` helper follows the existing cache/memory catalog construction path.
It is additive and rejects unsupported parameters/fidelities. A Python binding
check is staged for larger geometry, both queue fidelities, canonical/default
layout keys and invalid declarations. This helper has not yet been rebuilt.

### Native acceptance after layout-key changes

All 32 selected native checks pass (115.43 s), including the geometry/queue/
observer cases, explicit capture budget, invalid-selector regression and the
eight-configuration clock audit. Four compact configurations are certified;
four expanded configurations report precisely the documented six missing
contracts and are comparison-only. The following normal four-part delay test
also passes with all safety assertions and the newly recorded floors; the
combined serial scope peaks at 320684 KiB in 212.21 s under AS 768 MiB.
The new Python construction helper is rebuilding; its binding and generic
viewer-layout checks remain pending. Final browser/site/WASM work is open.

### Consecutive input-log correction staged

Review found that the new applied-input metadata used the last scheduled
index, even though the loop schedules an upcoming falling-edge transition
before sampling the preceding one at the current rising edge. Consecutive
transitions could therefore log the next event. Device state and captured
frames still used the actual input pins and independently checked correctly.

Added a separate sampled-input index and verify tick/button/down against the
pins at that edge. Successful completion also requires every scripted event
to be sampled, rather than merely scheduled for a future boundary. New tests
cover consecutive press/release/input-31 transitions, exact sequences above
2^53, and an event scheduled after the guest's final sampling edge. The host
reference checker now verifies this metadata when present, and requires it
for fresh native runner checks. These changes need a rebuild/rerun.

### Saved layouts generated and applied with preservation proof

The repository layout generator fills the device-system scenario's 6426
components, including the configured display variant. Merging accepts only
new type/root/profile entries relative to the saved pre-milestone baseline:
52 added entries and zero changed old values. Applied the merged file using
the repository formatter. Full static completeness remains to be rerun;
generation and preservation alone do not establish browser placement success.

### Input-log and complete-layout verification

The rebuilt native input validation passes in 14.82 s, including exact
consecutive transition metadata, sequences above 2^53 and rejection of an
input scheduled after halt but never sampled. DeviceInspectorTest passes in
0.85 s. VisualizerStaticLayoutTest passes all 363 aliases in 101.67 s after
the official generator/merge; the combined serial scope is 117.39 s under
AS 768 MiB. These checks do not establish rendered browser appearance.

The SRAM memory investigation rules out the proposed redundant preview in
the normal CTest path: its constructor does not set up a circuit, and the
runner calls run directly. No shared scenario framework change was made.
A retry retains all waveform recording, permits AS 1536 MiB, and stops only
our process tree if host availability falls below 512 MiB. Other processes
and the primary checkout remain untouched.

### Fresh bring-up, CLI failure handling and portable refresh

The current native binary completes bring-up in 23922 cycles, 72.01335986 s
execution, two exact host-reference frames and zero setup violations. Exact
scripted input metadata passes. The CLI records build flags/cache digest,
compiler-launcher bytes/digest, source/binary/fixture/frame digests and
separate viewer provenance. An existing successful output is rejected.
Forced one-second timeout, one-cycle budget and 128 MiB address-space cap
all fail explicitly and publish no recording or success evidence.

The large SRAM retry exits 75 at the 512 MiB host-availability guard. This
is a resource-limited check, not a passing regression or an observed logic
failure. The user was asked asynchronously about additional free memory;
smaller independent checks continue. The final portable targets are now
rebuilding serially with the same guardian and 1 GiB compiler cap.

### Current memory limit retained; imported keyboard boundary

The owner chose to continue with current memory limits. Keep the large SRAM
check explicitly resource limited and continue serial independent acceptance;
do not wait for or terminate unrelated processes to obtain more RAM.

The recording viewer now rejects malformed keyboard metadata before any
canvas allocation, including an input field forged as a large display.
Optional absent snapshots remain compatible with older recordings. Added JS
validation cases and a browser rejection assertion; these checks are queued
after the current serial portable build rather than run concurrently.

### Portable component/clock scope and process-sandbox boundary

54 portable checks pass in 320.55 s, combined peak 320612 KiB RSS. They cover
all registered phase-3 arithmetic/core/device scenarios, updated queue and
display parameters, device observation, standalone wide time, complete table
value identity, provisioning, shared JS validation and the eight-configuration
system clock audit. The audit retains the four certified compact cases and
four exact documented Unknown expanded cases.

The following bridge time-parity test is blocked by EPERM from Node's child
launch in the process sandbox, despite a reported child status of zero.
No parity pass is claimed for it. Rerun that check and device bridge parity
outside the sandbox with the same memory guardian. The local CTest selection
also gave all system checks a broad games label, so they were omitted by the
initial non-game filter; an explicit 11-test system scope is now running.

### Complete portable system and bridge parity results

All 11 explicit system checks pass in 259.41 s, peak 375976 KiB RSS:
FF/cached-FF/cached-DRAM execution, epochs above 2^53 and 2^63,
repetition/quantum/expanded/structural lockstep, larger displays, exact input
logs, capture/evidence limits, integrated faults and the recorded scenario.

Outside the sandbox, RV32IMDeviceWasmParityTest passes all 22 scenarios in
115.26 s: topology, examples, selected recorded states, device observations
and rewind match native. SimulationTimeWasmTest passes in 2.23 s, including
actual wasm32 wide-time execution and exact main-module/native transport.
The same 512 MiB host-availability stop guard remains active. Browser
appearance/site and full revised game comparisons are still unverified.

## 2026-10-22 — Current milestone boundary

The owner asked whether interactive work had started. Phase 7 has not
started: component/system/observer native-WASM checks pass, but packaged
browser/site and the full revised-game comparison campaign remain. The
complete in-process example run safely stops for headroom; the verified
fresh-process mode is rerunning every example under the current memory limit.
Do not describe an implementation draft or passing component parity as
completed interactive play.
