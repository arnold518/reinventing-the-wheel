# Scripted device platform and games implementation

Started: 2026-10-21. Status: closed (2026-11-03).
Code state: main alpha at `88c0a95`, with accepted component replacement in the
working tree; game acceptance remains open.

## Summary

The 2026-10-24 gameplay-capture continuation completes all ten packaged
native baseline fixtures: 47 synchronized circuit/display/input checkpoints,
exact host reference checks and all ten original-canvas browser sessions.
Native runs were serial with histories disabled, a 768 MiB address-space cap
and at most 155.58 MiB peak RSS, without guard stops or OOM failures. The full
record set, layouts, source/build evidence and browser receipts are indexed
publicly. Strict packaging and the refreshed 8893 preview pass route/asset checks.
Separate repeat/quantum/expanded-core/cache and native/WASM acceptance remains.

The integrated CPU/device system, bounded batch runtime, five games, ten
fixtures and recording viewer are present on main alpha. Phase 6 is open.
The owner now authorized width generalization and completion of the remaining
acceptance work, using only necessary checks and bounded serial resources.
The [complete component replacement](2026-10-22-general-component-retirement.md)
now accepts the current generic builders, references and focused component
checks. Earlier campaign receipts still describe their own source/binary
snapshots; they do not complete the remaining game campaign.

The preceding width snapshot passed 555 selected native registrations and
72 new width/timing cases, WASM width execution, 15 portable checks and strict
packaging of all 158 example groups. Its native/WASM timing snapshots match.
The preceding frozen-model campaign stopped after seven passing runs: four
bring-up and three 2048 configurations. Expanded-core 2048 exhausted its
event budget. Those receipts are preserved. Remaining configuration comparisons,
game WASM parity, broader acceptance and final performance evidence
remain required before closing phase 6. Phase 7 remains unimplemented.

The owner has confirmed that the required game experience is in the original
circuit visualizer, with display/keyboard and other device abstractions on
the actual system components. All ten native fixtures now replay in the
original canvas with synchronized device state and retained pin/wire values.
Every retained frame and browser pin/wire vector is checked. The earlier
Pong-specific native/JS/live checks remain dated evidence below. Full phase-6
acceptance remains open; implementation evidence is below.

## 2026-10-21 — Scope and execution plan

1. Finish phase-3 compatibility/viewer validation and record its evidence. The exact-label native foundation panel passed 46/46; additional build/provenance checks are still in progress.
2. Review and implement the required wide-time prerequisite before long WASM recordings; verify times beyond 2^32 without changing pin/family identities.
3. Add RV32IMDeviceSystem using the existing/new component families: guarded instruction fetch and cacheable RAM route; separate uncached console/timer/buttons/control/video routes. Certify the assembled clock and verify guest-only MMIO.
4. Add a freestanding guest I/O library, reproducible RV32IM builds, scripted input scheduling, deterministic console/frame capture, independent device reference checks and bounded recording controls. Bring up console, timer/button echo, pixel patterns and a moving cursor.
5. Implement scripted 2048, Sokoban, Snake, Pong and Tetris with fixed inputs/seeds/levels, independent game assertions, frame hashes and final guest-state checks.
6. Run scoped and full required regressions, structural/compact-child and native/WASM comparisons, packaged-site/browser checks and integrated performance/memory measurements. Report failures and unrun checks distinctly; retain old modes.

Builds and simulations remain serial. Keep per-process resource caps where supported, headroom checks and a guardian that stops only our process group when host memory becomes tight. Use small workloads and bounded history/capture first; never allocate backing arrays spanning high MMIO addresses or load large media images as RAM.

### Device realism discussion

The shared address space selects separate RAM, FIFO, counter, button and video structures. A generic future block controller could add persistent loading/saves; two-core uncached shared-memory experiments could precede atomics/coherent caches. Neither is needed for the selected preloaded scripted games. These are unscheduled future candidates, not phase-6 prerequisites or implemented features.

### Integration concerns to resolve before acceptance

- The independent device reference must model timer/button read sampling at the backend execution edge and retained responses. Sampling time at instruction retirement would be wrong under stalls; do not use actual reply data as an unchecked oracle.
- Frame capture follows ordered uncached pixel writes and PRESENT. Any RAM report used for game assertions also needs verified cache visibility; initial supported cache policies should avoid an unproven write-buffer/write-back/FENCE interaction.
- Long game runs need bounded wire/memory history and frame storage, separate from existing full-circuit playback. Game playback rate must not be reported as measured live simulation FPS.
- Measure actual core/device run cost before choosing further compact forms. Existing lower-level proof permits justified abstractions, but an instruction oracle must never become the product CPU. Reuse established gate/behavior rules and prove cycle/latency agreement.
- Initial binaries/assets are preloaded. Multicore, persistent media/boot and live host input are not phase-6 prerequisites.

### Integrated system contract drafted (unverified)

Added a separate `rv32im.system.devices` / `RV32IMDeviceSystem` structural assembly source, not yet registered or built while the wide-time rebuild runs. External inputs are CLK/RST/ENABLE, TX_READY and logical INPUT_VALID/INPUT_BUTTON[5]/INPUT_DOWN. Outputs are TX_VALID/TX_DATA[8], PRESENT/FRAME_SEQUENCE[32], HALTED/TRAPPED/TRAP_CAUSE[4] and COMMIT_VALID. It composes the existing M core, guarded fetch, data router, shared RAM arbiter, console/timer/buttons and framebuffer. Optional I/D caches sit only on the RAM routes; the initial data cache is unbuffered write-through. The fixed RAM envelope is 256 KiB, matching the router and ELF loader; no high-address dense memory is allocated. FF and DRAM backings use existing endpoint families. No whole-machine behavioral implementation is introduced.

A `FrameStateView` observation contract returns an immutable indexed image plus a per-bit known mask, geometry, capture time and sequence. The caller must capture at a settled presentation edge before advancing to later writes. This observes the persistent surface and does not implement scanout, DMA or game logic on the host. Required system evidence is still pending: wiring/pin validation, MMIO instruction/device reference checks, timing, cache bypass, RAM visibility, frame ordering, native/WASM agreement and regression coverage.

The 21 selected new recorded scenarios passed saved-layout completeness (`0 layout problems`). The viewer baseline remains seven passes and one configured timeout; wide-time edits have not yet passed a build.

### Guest sources and checks drafted (unverified)

Added original freestanding C rules for five games and a guest-only MMIO library, startup/BSS/linker definitions, dirty-cell renderer, report page and hello/timer/button/pixel/cursor bring-up program under `tools/rv32im/guest`. No upstream game code/assets were copied. Game actions are logical down events, releases are consumed, Snake motion uses separate update events, and Pong samples both players' levels. Seeds and field-wise fingerprints are deterministic. Initial short scripts queue at most four transitions before polling, including a Snake tap; this bounds the bring-up without a timing-dependent host input callback. Broader rule boundary checks are drafted in `tests/cpp/RV32IMGameRulesTest.c`.

The new build helper scopes RV32IM/ILP32 flags, checks final disassembly, records compiler/source/ELF/script hashes and emits exact decimal input ticks. These sources and checks have not been compiled or executed yet. The wide-time native build first stopped at a mechanical parameter-export conversion error; that error was corrected and the serial rebuild resumed. No test success is inferred from these drafts.

### First guest build and rules evidence

The pinned GCC 12.4 RV32IM/ILP32 build succeeded for bring-up (205 instructions), 2048 (1,300), Sokoban (1,225), Snake (1,224), Pong (1,226) and Tetris (1,226). Final disassembly is restricted to supported 32-bit I/M instructions. Each game initially consumes three down events; Snake has an additional queued release to exercise a tap. This is a deliberately short deterministic batch, not evidence of extended play. Artifacts/manifests are under `tests/fixtures/rv32im/games`.

A strict `cc -std=c11 -O1 -Wall -Wextra -Werror` host rule-check build passed. Cases cover one-merge-per-tile and 2048 win/loss, Sokoban pushes/walls/goals, Snake direction/tap/food/tail/self/wall behavior, both Pong players/bounce/miss, Tetris rotation/wall/line-clear/top-out and pause/restart. Initial fixture errors were corrected: a vacating-tail move was legal, and an artificially full top row was cleared rather than causing top-out. These checks validate C rules only; guest circuit execution, rendered output and native/WASM parity remain unverified.

The larger native time rebuild was stopped by its own guardian below 384 MiB host availability. It did not touch other processes. Reconfigured compiler launcher to use lower-memory garbage collection and O0 for the large cache translation unit, retaining O1 for normal runtime/gate code; serial rebuilding resumed with a 768 MiB address-space cap.

### Batch runtime and recording viewer drafted (unverified)

Added an independent batch runner draft under `runtime/`: bounded cycles/instructions/events/frames, history off, preloaded ELF, exact-tick scripted button transitions, ISA retirement/register checks, device read sampling at backend execution and ordered pixel/PRESENT checks. It keeps independent RAM/video/button/timer/console models; recorded device reads are expected samples, not echoed CPU responses. Capture produces immutable RGB332 bytes and known masks. Exceptions/budget exhaustion are failures. The runner and native probe are not built/verified yet.

An additive `games.html` recorded-frame viewer draft runs the batch in its own worker or opens exported JSON. Replay controls change only frame playback; cancellation terminates that worker. Existing circuit playback paths are retained. No interactive guest input or persistent live simulator session is introduced. Browser packaging/runtime checks remain pending.

### Additional staged validation equipment

All six host C reference executables now build/run, producing two bring-up frames and four frames per game. Their reports/pixels provide a compiler/drawing reference; independent hard-coded rule checks remain the rule proof. Added the same ELF/input embedding for file-system-free WASM, native batch probe, optional Python/WASM adapters and exact-frame/report comparison helpers. The fixture helper was rerun after adding the explicit host-test boundary; its hashes cover the current guest source and the RISC-V disassembly remains restricted to I/M.

Registered draft system, input-validation, bring-up and game checks plus a small reset/disabled system component scenario. The system check uses a short synthetic ELF to exercise actual console/timer/M/pixel/PRESENT execution in structural/compact-child FF and cached FF/DRAM configurations. None of these circuit checks has passed yet. Source drafts and public references explicitly retain that limit.

Native compilation is temporarily stopped while the serial wasm32 compile audit finds remaining timestamp narrowing; compiled objects are retained. No other build/process is interrupted. Runtime histories include optional cache memory banks; frame/input/device-read storage has explicit bounds. Recorded inputs use future falling-edge-plus-one boundaries and stable sequence numbers. The batch uses the existing drain API in event quanta and observes only settled clock boundaries; this is not a live-session/frontier implementation.

### Fixed video window and observer contract

Integration review found that tiny fully structural framebuffer storage may lack a compact MemoryStateView, whereas the device-system root promises frame capture. Added an optional framebuffer `storage_bytes` capacity, retaining its existing default, and made the system reserve the full 4 KiB video aperture for every visible geometry. This keeps high-address routing local, supports padding bytes and guarantees scalable native storage for the system's observer. The representative tiny standalone framebuffer path remains available. Added pending capacity/padding/invalid-size checks; no new validation result is claimed.

Additional acceptance work remains after first bring-up: compiled Tetris line-clear and other costly boundary presets, longer deterministic game samples for measured throughput/visibility, explicit presentation versus changed-frame counts, and native/WASM/quantum/repetition comparisons. The current three-action fixtures alone cannot establish sustained game FPS.

### Pending runtime measurement boundary

Added per-presentation cycle/instruction/host-time samples, separate presentation and changed-image counts, construction/execution/capture times, actual RAM/video allocation and configuration/profile fingerprint fields. Presentations remain bounded even when frame export is disabled. Reported host execution includes reference checking and differs from viewer replay speed; sustained live pacing is still phase 7. The M C-smoke test now reads its exact audited embedded ELF bytes so the same verification can run without a WASM filesystem.

The short system ELF also places a console store on a taken branch's wrong path; its required output is only `A`. This extends the pending integrated check beyond the standalone speculative-fetch guard. The reference must not accept a device response as its own expected result. Fault-path integration remains separate from the default successful-guest batch contract.

### First portable execution results and assembly correction

The first portable verifier linked. SimulationTimeTest, SimulationTimePlaybackTest and RV32IMRecordedInputValidationTest pass in wasm32 under the memory guardian. These validate explicit time execution/history/overflow and input validation, not yet the packaged browser or native/Python parity. The catalog check first failed because the new system TypeName was not yet in its expected inventory; corrected that whitelist.

The first integrated system check failed at construction, before guest execution (`Invalid structural connection`). Inspection found that TwoPortMemoryArbiter has I_CLK/I_RST and D_CLK/D_RST, with no unprefixed CLK/RST. Corrected the new assembly to tie both existing protocol clock/reset pairs to the global inputs; no arbiter contract/source is changed.

Reference review also corrected the store check to compare only enabled byte lanes; inactive write-data bytes are don't-care in the existing protocol. Added a pending negative-value SB/readback to the short guest, alongside the wrong-path console store, M arithmetic and presentation. No integrated guest pass is claimed yet.

### First integrated execution and process-memory boundary

The catalog inventory now passes in WASM. The integrated tiny guest reached
PRESENT but first found unwritten video bytes unknown (byte 4 actual known mask
0, expected 255): forcing an expanded endpoint while selecting its compact
controller permits an unknown startup reset before the controller settles.
Removed the new display's forced endpoint fidelity so it follows the normal
profile, like RAM. No existing memory implementation is changed.

The next guarded run completed compact-child FF, all-structural logic with
scalable native FF storage, and cached FF configurations, including wrong-path
console suppression, M, timer, pixel capture and signed SB/readback. The guard
then stopped the command at the cached DRAM case when host available memory fell
below 512 MiB (exit 75). This is not a cached DRAM pass. Split the six system
configurations into separate registered checks/processes so WASM's retained
linear-memory high-water mark does not accumulate across configurations. Final
per-process and wide-time results remain pending.

### Isolated DRAM and wide-time system checks

The per-process cached DRAM check passes in WASM. Both complete short-guest
checks at start epochs 2^54+123 and 2^63+123 pass, including CPU, memory, devices,
retirement/register reference and frames. Logs are
`/tmp/circuitsim-device-cached-dram-wasm.log` and
`/tmp/circuitsim-device-wide-wasm.log`. This adds genuine unsigned-wide system
execution; it does not replace the pending binding/browser exact-transport checks.
The first compiled bring-up run is still in progress, with no completed result.

Added pending acceptance fixtures with curated guest-side starting states:
2048 merge, Tetris line clear, Snake wall loss and a 17-update Pong rally.
Ordinary game starts remain unchanged. The rally performs finite guest-controlled
updates after its initial scripted event; it does not add a live host or timer
pacing loop. Pong's one-rally terminal status is now defined from player one's
view: won for a right-side miss, lost for a left-side miss, one point per rally.
All affected guest images/embedded fixtures must be rebuilt and checked.

A compact MulDivUnit32 draft follows the existing cache-controller mechanism:
run its own structural gate list with characterized register state and delays
proven from that same circuit. It uses no host arithmetic shortcut or instruction
oracle. Its fidelity/equivalence, timing and performance checks remain pending;
no speed-up is claimed before measurement. Structural M units stay available.

### First compiled bring-up completion (preceding compact-M draft)

The original audited bring-up ELF completed in the actual WASM circuit:
23,922 cycles, 6,445 retired instructions, 95,062,233 processed/scheduled events,
maximum queue 5,350; console `hello-device\n`, two ordered known-pixel frames,
checked timer/button/report state and zero setup violations. Setup 2.3059 s,
execution 466.1282 s, capture 0.01222 s. `/usr/bin/time`: 468.87 s elapsed,
203,272 KiB peak RSS. First presentation at cycle 22,510 / instruction 6,092 /
439.6761 s; second at 23,852 / 6,428 / 464.5690 s. The moving-square update
therefore cost 1,342 cycles, 336 retired instructions and 24.8929 s (~0.0402
changed updates/s). Initial full clear/draw is separate from that update.
These are checked batch WASM measurements, not live display FPS.

Evidence is retained at `build-rv32im/pre-compact-bringup-wasm/recording.json`;
log `/tmp/circuitsim-bringup-wasm.jsonlog`, RSS `/tmp/circuitsim-bringup-wasm-rss.txt`.
ELF SHA256 `b355e8a466cea639d6b920923333d4d2a544c4afdb8ce8e037d75b8e569327f5`,
script SHA256 `ee1230add1f81997814b3e28f162ffe7cb3af19f2a8cbc49c3e8c919c55c23d2`.
The run returned success before an attempted interrupt was delivered; it was
not abandoned. New compact M sources and later guest changes were not in that
binary. Current-source rerun and exact host-reference/native comparison remain.

All ten revised guest ELFs now build/audit as RV32IM/ILP32, including four
curated acceptance fixtures. Strict C11 rule checks pass with both Pong winner
outcomes. Host reference builds for the new fixtures and rebuilt simulator
execution remain pending.

### Compact M execution and remaining event cost

The compact M draft passes WASM catalog inventory, MulDivUnit32ScenarioTest
(all available root fidelities), and MulDivUnit32Test (all eight operations,
48 directed/random operand sets each, latency/hold/pause/cancel/reset).
Elapsed/RSS: 3.65 s / 243,008 KiB scenario; 6.41 s / 228,152 KiB sweep. The
short integrated guest also passes: 126 cycles, 24 retired, 513,925 events,
2.5241 s execution, 7.30 s process / 245,296 KiB RSS. Log:
`/tmp/circuitsim-compact-m-first-wasm.log`. Standalone three-profile timing and
fresh native checks remain.

About 4,000 events/cycle remain in the assembled platform. Added a pending
whole-core gate-list form using the structural core (including its RF) with
characterized register/state-bit leaves. GateListModel gains optional read-only
node-index observations, so architectural and retirement capabilities read the
actual RF/MEM_WB signals; no instruction execution is duplicated in the adapter.
Existing cache callers use an empty observation list. Core compilation,
equivalence, timing and old cache regression checks are still required.

All ten host C reference builds pass, including the curated cases. Comparing
the first stored bring-up against the current manifest correctly rejected a
changed ELF hash. Inspection found the assembler had emitted a random temporary
object FILE symbol (`ccgywwKb.o`) for start.S; code/pixel behavior was unchanged.
A direct stored-output check matches both frames, all known bits and stable
report words with the host reference. Added an explicit `.file "start.S"`
directive; rebuild twice and compare every ELF before claiming reproducibility.
The first recorded ELF retains its original hash in the earlier evidence.

### Core observations and device compact forms

The first whole-core construction checks exposed two adapter gaps before any
instructions executed: enabled single-bit storage and composite boundary aliases.
GateListModel now supports the characterized FlipFlopCell state rule and resolves
read-only observations through the existing timing driver walk. Its program
retains only node indexes, not component/pin pointers. The 256-case four-state
cell/input/output-observation check passes in WASM, including composite aliases,
invalid observation rejection and unchanged graph size.

The full compact CPU then passes the short integrated guest with the same 126
cycles and 24 retirements: 232,586 events, 1.2920 s execution, 13.80 s process /
272,456 KiB RSS. Earlier compact-child CPU was 513,925 events and 2.5241 s.
Construction includes gate compilation and timing proof; these are not warmed
live-session measurements. Log `/tmp/circuitsim-gate-core-third-wasm.log`.

Added a shared private gate-list factory for counter 4/32, FIFO 8/32, register
frontend, address router and fetch guard. Counter/FIFO functional checks pass
in WASM with both structural and compact roots. The first frontend construction
failed because RequestKnownBits did not expose a functional value adapter.
Factored its existing X/Z check into one function used by evaluation and the
adapter; GateListModel calls that owner. Its existing conservative timing
contract/deciders are unchanged. Remaining device/scenario checks are running.
The batch now observes the frontend's public READ_STROBE/WRITE_STROBE instead
of inspecting its RESPONSE child's implementation.

A pending guest-only Pong drawing optimization updates old/new sprites rather
than rescanning all 512 logical cells per tick. Host reference equipment will
compare every presentation with an independent full-grid projection. New ELF
images and reproducible rebuild checks remain pending; no improved game FPS is
claimed from these source edits.

### Compact device verification and reproducible guests

The new register frontend, router and fetch guard functional checks pass in
WASM, including masks, sampled-response retention, stalls, and invalid/X/Z
fields. Recorded counter, FIFO, frontend, router and fetch-guard scenarios
pass across their available root fidelities. The full compact CPU's recorded
scenario also passes. The short integrated guest remains 126 cycles / 24
retirements, now 81,526 events and 0.379693 s execution (9.31 s process /
280,096 KiB peak RSS), versus 513,925 events / 2.52406 s with only compact M.
Log `/tmp/circuitsim-compact-devices-wasm2.log`. This is a short checked guest,
not a measured game-frame rate. Old timing/table/native checks still remain.

Rebuilt all ten guests twice in different output directories after the explicit
assembler FILE directive and the Pong sprite renderer. Every ELF/input byte and
the full manifests are identical. Evidence
`build-rv32im/guest-reproducibility.json`; builds 5/6 logs in `/tmp`. All ten
strict host reference builds pass again, with every Pong presentation checked
against a separate full-grid pixel projection. These current guest bytes are
now being embedded/relinked into the simulator; their circuit checks follow.

### Assembled FF operating-clock certificate

The first clock check requested a binary-startup proof and correctly returned
unknown: the binary-invariant analyzer does not recognize the new compact parts.
No weaker binary assertion was added. The unrestricted four-state timing check
then passes for the actual compact FF and cached-FF assemblies at 520 unit
ticks; the runner's period is 1,000. FF: 17,498 nodes and 109 setup / 109 hold
relationships. Cached FF: 31,709 nodes and 195 setup / 195 hold relationships.
Both limiting constraints are the external-button-to-FIFO PUSH full-cycle
setup path. Environment: RST fixed 0 after coordinated initialization,
ENABLE/TX_READY fixed 1, other external inputs at rising-edge+501 (equivalent
to falling-edge+1 with the actual 1,000-tick clock), rising-edge sampling and
one tick event separation. Cold check: 17.36 s / 258,068 KiB RSS in WASM.
Log `/tmp/circuitsim-system-timing-wasm2.log`. This certifies these two unit
configurations, not arbitrary external timing, physical profiles or DRAM.

### Full-core agreement and current compiled bring-up

RV32IMFiveStageCoreTest passes in WASM (114.23 s / 307,296 KiB RSS): all M
operations/dependencies, memory waits/faults, wrong-path suppression, reset
in flight, retirement/register/final-memory reference and structural/compact
protocol agreement. The reset fixture now observes the public execution-stall
boundary after four waiting edges instead of requiring a particular nested
C++ implementation type.

The current reproducible bring-up ELF passes actual WASM execution and exact
host C reference pixels/stable reports: 23,922 cycles / 6,445 instructions,
10,532,992 events, maximum queue 1,598, zero setup violations. Setup 8.4984 s,
execution 63.8039 s, capture 0.00582 s; process 72.55 s / 255,812 KiB RSS
(249.82 MiB). Frames remain at cycles 22,510 and 23,852, with exactly the same
reports/pixels as the earlier image. The 1,342-cycle / 336-instruction moving
update takes 3.42191 s, ~0.2922 changed updates/s, versus the preceding
24.8929 s / ~0.0402. Cold gate compilation/proof is included in setup, not
execution. This is measured checked batch throughput, not delivered live FPS.

Artifact `build-rv32im/compact-bringup-wasm/recording.json`, exact comparison
log `/tmp/circuitsim-bringup-current-reference.log`. Current ELF SHA256
`dace35a972617fa58f3cc18ee8a118f3b8809d8e4aee7000b045349317b4a0fd`;
input SHA256 remains `ee1230add1f81997814b3e28f162ffe7cb3af19f2a8cbc49c3e8c919c55c23d2`.
Earlier raw RSS evidence is 203,272 KiB, which is 198.51 MiB; raw KiB values
control where earlier prose approximated units differently.

The [mixed-profile FF startup issue](2026-10-21-memory-controller-startup.md)
is being checked independently before the fresh native regression build.

### Pending repetition and quantum check

Added RV32IMDeviceSystemLockstepTest: repeat the same short FF guest, change
event quantum from 100,000 to 97, and compare fully structural logic at the
same external clock. It checks cycles, retirements, final registers/report,
console, frame times/bytes/known masks, device samples and same-fidelity event
counts. Execution is pending. No new live or resumable-state API is introduced.

### Repetition, evidence limits and strict retirement comparison

RV32IMDeviceSystemLockstepTest passes in WASM: repeated compact execution,
97-event versus 100,000-event quanta, and fully structural logic agree on the
short guest's architectural state, samples, frames and cycles. Same-fidelity
event counts also agree. Process 67.19 s / 370,868 KiB RSS; log
`/tmp/circuitsim-system-lockstep-wasm.log`. The subsequent extra trap-cause
assertion still needs its refreshed run.

Added separate 65,536-entry maximums for console bytes and sampled device
reads, checked before appending; lower bounds are configurable. The viewer's
file budget is 32 MiB. A new check requires each evidence budget to fail with
its specific exhaustion reason. Stricter retirement checks now include
terminal status, memory size/sign/data/fault and architectural trap cause.
This caught a reference-only metadata error: LW was marked sign-extending
in the MMIO adapter. It now matches the existing independent I oracle: only
LB/LH extend a narrow sign. No CPU execution semantics changed. Execution of
these refreshed checks is in progress before the native build.

### Acceptance controls prepared during the native rebuild

The strict evidence-budget check now passes in fresh WASM after the LW
reference metadata correction (`/tmp/circuitsim-evidence-budget-wasm4.log`).
The native verification CLI accepts an event quantum and compact, expanded-core
or fully structural logic profile. Expanded-core selects the real CPU assembly
with compact children; it permits per-game fidelity checks without requiring
every long game to expand every register and gate. Profile name/fingerprint
are captured. The default fixture bindings remain unchanged. Short lockstep
and assembled-clock checks now include this comparison profile; validation
of these additions is pending.

The table generator now proves IM core tables against its expanded register
file with characterized storage leaves, matching the compact evaluator's
actual circuit. Its 45 supported predictor parameter combinations join the
existing I variants. Existing I/memory table construction is unchanged. Full
three-profile regeneration and old-entry comparisons remain required.

The recording CLI now launches the simulator directly instead of through a
timing wrapper: Python's wall-time timeout kills/reaps that direct child. Peak
RSS comes from child resource usage on this Linux host. This avoids leaving a
wrapper's simulator grandchild running after timeout. A short forced-timeout
check is required after the fresh native target is available. Native acceptance
orchestration is prepared under the ignored build directory and calls the
existing recording and independent reference CLIs serially for all ten guests,
repeated quanta and expanded CPU assemblies. No game acceptance pass is inferred
from preparation.

The full-core check now also executes its M/dependency/branch/store program
with taken, backward-taken, one-bit and two-bit prediction (four entries), in
addition to the default policy and default structural/compact comparison.
These newly added executions remain pending. The assembled clock panel is
prepared for compact/expanded CPU assembly with FF, cached FF and cached DRAM;
its earlier 520-tick certificate still applies only to the already checked
compact FF/cached-FF cases until that broader panel passes.

### Fresh native prerequisite panel and clock-analysis boundary

Fresh native provisioning and gate-list observation checks pass. The expanded
clock panel reports the compact FF, cached FF and cached DRAM configurations
as safe under contracts at 520 unit ticks (the actual period remains 1,000).
The attempted expanded CPU profile returns unknown: older compact fetch,
pipeline-register and prediction parts have no all-state contract in this
analysis. The panel consequently fails; it is not a six-configuration timing
pass. These diagnostics are retained in
`/tmp/circuitsim-native-foundation-refresh.log`.

The maintained timing test now targets the three compact machine configurations.
The expanded CPU profile is comparison equipment with per-execution setup and
reference checks, not an additional all-state static certificate. Its generic
binary-startup proof is not asserted or weakened. This narrows the optional
expanded timing attempt above; the adopted default-system certificate and
per-game fidelity comparisons remain required. Native core/time/system checks
are still running, and the three-configuration timing target needs rebuilding
and a passing rerun.

The native prerequisite panel finishes 8/9 passing in 181.15 s. Its one failure
is the documented optional expanded-profile clock-analysis attempt. Current
RV32IMFiveStageCoreTest passes in 93.00 s, including the four added predictor
policies and trap-cause assertions. SystemLockstep passes in 47.79 s, now also
including the expanded CPU profile; short system 7.04 s; explicit evidence
budget 14.09 s. Both wide-time tests, provisioning and gate-list observation
pass. The narrowed three-configuration timing target is rebuilding; do not
present this initial panel as 9/9.

The rebuilt timing test passes independently in 17.31 s for all three compact
configurations (`/tmp/circuitsim-native-system-timing.log`). Full unit table
generation with `--clocked` is now running under the 768 MiB cap. Old-entry
comparison and the remaining two profiles follow; this is separate from the
compiled-value partition identity already proven.

Core gate-program cache metadata is now capped at 128 entries, matching the
other new gate-list parts. Live instances own their immutable programs; an
evicted weak-cache key does not invalidate an instance. This bounds repeated
parameter spelling/configuration churn without changing execution or timing.
It requires the final rebuild; the running table generator predates this
metadata-only guard.

### Complete unit table regeneration

`part_delay_tables --write ... --clocked` completes with 808 unit entries.
All 754 preceding entries are byte-identical, none are missing, and the 54
additions are nine new default compact parts plus 45 M-core predictor variants.
Evidence `build-rv32im/unit-table-preservation.json`; log
`/tmp/circuitsim-unit-tables-generation.log`. The generation process peaks at
387,416 KiB RSS (~378.34 MiB) under the 768 MiB address-space cap. Embedded
then Desktop regeneration, no-gate-list inventory, rebuild and full verification
remain required. This is a generation pass, not yet a rebuilt-table test pass.

The game native/WASM parity script now captures all native records before
loading its WASM module. This keeps native children separate from WASM's
retained linear-memory high-water allocation. Native progress is forwarded
to the test log. The ten records remain bounded and actual game/parity
execution is still pending.

Embedded complete regeneration also passes: 808 entries, 54 additions, all
754 legacy entries byte-identical and none missing. Elapsed 9:48.68, peak
391,496 KiB RSS (~382.32 MiB), exit 0. Evidence
`build-rv32im/embedded-table-preservation.json`; log
`/tmp/circuitsim-embedded-tables-generation.log`. Desktop generation follows
serially under the same cap/guardian.

### Full-system fault-path acceptance prepared

Source review found two reference-boundary gaps: the register-window predicate
included the unmapped gaps, and a misaligned RAM instruction was required to
have a bus transaction before the independent oracle checked alignment. The
model now recognizes only the four actual 256-byte windows, and delegates
misalignment to the ISA oracle before consuming a transaction. Reference RAM
capacity is explicitly 256 KiB rather than relying on the shared default.
Outstanding data transactions are checked before reporting a terminal trap.

Added a registered full-system negative check for reserved encoding, attempted
MMIO instruction fetch, misaligned RAM load, a hole between register windows,
unmapped data and an address beyond video backing. Each must reach the checked
architectural trap with zero setup violations; premature model mismatches
do not count as passing faults. These new checks remain unbuilt/unrun until
the final native/WASM refresh. Normal guest execution is unchanged by design.

Desktop complete regeneration passes too: 808 entries, 54 additions, all
754 legacy entries byte-identical and none missing. Elapsed 9:52.73, peak
387,816 KiB RSS (~378.73 MiB), exit 0. Evidence
`build-rv32im/desktop-table-preservation.json`; log
`/tmp/circuitsim-desktop-tables-generation.log`. Thus the complete generated
files in Unit/Embedded/Desktop each preserve every old entry. The separate
no-gate-list inventory is being regenerated before the rebuild.

### Rebuilt-table system acceptance

All 5 selected current native checks pass in 80.47 s after the regenerated
tables and inventory are compiled: compact clock panel (17.65 s), normal
system (7.50 s), epochs above 2^53 (7.38 s) and 2^63 (7.28 s), and all six
full-system fault cases (40.65 s). The fault cases reach the independently
checked architectural traps with zero setup violations, rather than merely
throwing a model mismatch. Log
`/tmp/circuitsim-native-final-system-panel.log`. This is a focused native panel,
not a full-suite or game-circuit acceptance pass.

The isolated Python binding retry now uses a 1 GiB compiler address-space cap,
starts with at least 1,400 MiB available and stops its own process group if
host availability drops below 512 MiB. Deployment remains disabled.

### Current native compiled bring-up and binding panel

The current compiled bring-up passes actual native execution and the exact host
C reference: 23,922 cycles / 6,445 instructions, 10,532,992 processed events,
queue maximum 1,598, zero setup violations, two exact frames/reports and no lost
input. Setup 5.83845 s, execution 66.74602 s, capture 0.001638 s; peak RSS
146,288 KiB (~142.86 MiB). Presentations at cycles 22,510 and 23,852; the moving
update costs 1,342 cycles / 336 instructions and 3.39714 s (~0.29437 changed
updates/s). Artifact `build-rv32im/phase6-native-games/bringup/baseline/`. This
remains checked batch throughput, not delivered live FPS.

The full native target build passes (3:26.79, peak 272,644 KiB). The subsequent
fast binding panel is 4/5: registry coverage, strict game rules, partition
checks and exact Python/native wide-time/rewind pass. ModuleBindings fails the
existing rule that logical test names cannot contain implementation fidelity
words: the new standalone system variant used `Structural`. Its complete
coverage already exists in DeviceSystemLockstepTest (same tiny guest, compared
against the checked compact baseline). Removed that redundant unpublished
registration and kept all structural execution/comparison assertions. Other
configuration indices are now contiguous. The registry/source rebuild and
a passing binding-panel rerun are pending. Log
`/tmp/circuitsim-native-binding-panel.log`.

The binding-panel rerun passes 5/5 in 7.75 s after the naming cleanup, including
ModuleBindings, registry coverage and exact Python/native wide-time playback.
The final inventory has 555 registered tests. The complete suite is running
serially in two resource groups: 543 C++/Python checks capped at 768 MiB per
process, then 12 Node-dependent checks with V8 heap and host-memory limits.
This avoids applying a native address-space cap to V8's virtual reservation.
The three complete timing-table verification tests are included in the full
registered run; no duplicate full recomputation is claimed as extra coverage.

The full-suite Unit `--verify` test passes in 606.75 s: all 808 rebuilt entries
match recomputation, including parameter variants, and the generated no-gate-list
inventory matches. This is distinct from earlier generation/preservation and
compiled-partition snapshot checks. The suite now verifies Embedded; no full
suite completion is claimed yet.

The full-suite Embedded `--verify` test passes in 607.27 s too: all 808 entries
and the inventory match. Desktop verification is next; completed full-suite
checks are 8/8 at this boundary.

Desktop full `--verify` passes in 604.22 s. Complete regenerated/compiled table
verification therefore passes in all three profiles, including all 808 entries
and the generated inventory. DocumentationTimingDataTest (1.66 s),
MemoryLatencyGatesTest (14.58 s) and SRAMSubarrayFormulaTest (0.07 s) also pass.
The registered suite continues with delay-rule comparisons.

### Full-suite timing target failure under investigation

The first full DelayRuleTest reports zero timing safety rule breaks, but four
parts miss its conservatism targets: the three existing cache controller widths
(typical 84–85%, worst 62–75%) and the new RV32IM core (typical 30%, worst 26%).
Both use dynamically proven tables; the M core is intentionally too large for
a delay twin. The suite continues with strict settling. Baseline cache
reproduction and an evidence-based resolution remain pending; no full-suite
pass is claimed. Log: `/tmp/circuitsim-native-full-capped.log`.

The recording CLI now refuses a directory containing recording/evidence
output. This prevents an unsuccessful rerun from leaving a preceding successful
recording looking current. Timeout, cycle and address-space negative checks
remain pending after the serial suite.

Recording-viewer review found that an already queued callback from a terminated
worker could clear a newer worker's controls. Callbacks now check worker
identity; asynchronous file loads also carry a generation and stop the active
worker before reading. A later run/cancel supersedes an older pending file
read. Browser regression coverage for these races is being added; unrun.

The test guide now reflects 22 recorded component examples and ten guest
fixtures, the expanded-core/structural comparison, the complete probe CLI and
fresh output directories. Its full-suite example uses `-j1` for this host. The
public core reference now describes the compiled-gate behavioral form and its
known-binary, reset/setup/commit observation boundary; it does not promise
historical register-file inspection from that compact form.

Strict settling passes across the full part comparison in 1,197.43 s. The
ordinary delay-rule run took 1,204.53 s and failed only its four conservatism
targets; it reported zero safety rule breaks and its three old core clock/
program comparisons passed. At this boundary 13/14 full-suite checks pass;
the remaining suite continues. The recording CLI now streams progress to
`stderr.log` while retaining a direct, timeout-reaped simulator child.

The subsequent SRAMCellTimingTest (106.71 s) and DFlipFlopTimingTest
(12.63 s) pass. SettlingBoundTest is running.

SettlingBoundTest passes in 213.43 s. The complete native run continues with
RV32I logic/arithmetic optimization and safe-clock checks.

Native checks 18–26 pass: RV32ILogicOptimization, ArithmeticTimingOptimization,
SafeClockTiming, MemoryEndpointSafeClock, MemoryStateTiming, MemoryFeedbackTiming,
BinaryStartupTiming, BenchmarkModel and BenchmarkCampaignCLI. At this boundary
25/26 completed checks pass; the four delay-table conservatism targets remain
the only failure. These bounded benchmark checks are not a full benchmark
campaign.

The bounded FullEmbenchCampaign, BackgroundCampaignResume and
CleanupValidationRunner checks pass. WebTestRunnerTest fails because the
registered SimulationTimeWasmTest.mjs does not match the inventory check's
three historical filename patterns. Added the `*WasmTest.mjs` pattern and
assertions for selected portable-verifier and recorded-guest-probe artifact
paths. This corrects test inventory without removing any registered check;
its focused rerun awaits the serial suite. Legacy viewer smoke and the middle
benchmark/RV32I benchmark checks then pass.

RV32IBenchmarkCLITest passes in 124.94 s. The full run continues with
RV32IDifferentialTest; 32/34 completed checks pass, with the delay targets
and corrected-but-not-yet-rerun web inventory as the two outstanding failures.

RV32IDifferentialTest (65.52 s), RV32IExperimentTest (0.86 s) and
RV32IExperimentCLITest (56.67 s) pass. RV32IProfileSweepTest and
RV32ITopologyTest fail before hardware execution: their existing subprocess
commands request hard address-space limits of 1,024 and 2,048 MiB, which
cannot be installed beneath this run's inherited 768 MiB hard limit. This is
a validation-environment conflict, not yet a circuit regression result.
Rerun these two checks serially without the outer address-space cap; retain
their own explicit caps and host-memory guardian. No legacy experiment code
or test assertion is changed to accommodate this.

Checks 40–54 pass, including binary-clock CLI (68.36 s), memory inspector,
documentation diagrams, register/register-file FF integration and layout
generation, and all original RV32I building-block/single-cycle/baseline/advanced
example and documentation checks. Memory migration is next. The separate
resource-aware rerun script selects the 12 Node-dependent checks, the two
existing self-capped experiment checks and corrected WebTestRunnerTest.

RV32IMemoryMigrationTest passes in 192.07 s. Layout/visualizer regressions
follow. No additional circuit failure has been reported at this boundary.

Checks 56–61 pass: RV32I memory/single-cycle/building-block layouts, SRAM
bindings/layout and native memory-device layouts. MemoryEndpointIntegrationTest
is running next. These are native/Python checks, not packaged-browser proof.

MemoryEndpointIntegrationTest passes in 73.05 s, and the previously timed-out
MemoryInspectorIntegrationTest now passes in 42.16 s on the rebuilt module.
MemoryDeviceDocumentationTest finds one mechanical wide-time edit error in
its standalone C++ example harness: `std::SimulationTime` is not a type.
Restored that harness's original `std::size_t`, matching its independent
public reference snippets and drain helper. Product time paths remain explicit
64-bit; this native-only fixture has no dependency on the simulator's type.
A repository source search finds no other `std::SimulationTime`. Focused
documentation rerun is pending. CacheDocumentationTest passes afterward.

Checks 65–106 pass, including cache documentation/layout, legacy circuit, wire,
simulator advancement/unwired history/drain, analysis/profile/catalog selection,
all primitive gates/latches/flip-flops, storage/register widths, controllers,
arbiters, all five endpoint technologies, scalable/native devices and native
device contract checks. The native suite has five reported failures so far:
delay conservatism, the corrected web inventory, the two outer-cap conflicts
and the corrected standalone documentation fixture. None is claimed resolved
until its corresponding rerun passes.

MemoryEndpointContractTest passes in 119.42 s and MemoryEndpointTimingTest
in 11.59 s. Cache behavior regression checks follow.

The broad CacheReadTest passes in 445.22 s, covering the existing write-policy,
write-buffer and associativity/replacement streams. CacheControllerLockstepTest
is running next, comparing the old structural and compact controller traces.
A read-only review also confirms the new arithmetic/device construction helper
uses StructuralWiring's pin-ID ordering and aggregate argument sequencing;
actual final native/WASM parity is still pending.

CacheControllerLockstepTest passes in 325.04 s. The four recorded cache
scenarios also pass (component, direct-mapped, scalable, two-level), followed
by FF cells/words/banks and SRAM lower-level checks through SRAMTest. The
recorded fixture adapter explicitly rejects non-Unit timing profiles; the
reported game throughput/cycle periods remain Unit-profile measurements.

The native run progresses through all lower-level FF/SRAM/DRAM/ROM/NOR-flash
checks, memory helper/device contracts and timing, then RV32I primitives and
the 8-bit ALU parts. At check 210, 205/210 completed checks pass; the five
previously recorded failures are unchanged. ALU8ContractTest is running.

### Correction to the intermediate failure count

A complete failure-line scan finds one additional completed check that the
prior tail summaries missed: SRAMClockedSubarrayTest fails with `std::bad_alloc`
after 26.23 s beneath the 768 MiB address-space cap. Thus the boundary at
210 completed checks was 204 passes / six failures, replacing the earlier
205/210 count and "five unchanged" assessment. SRAMSubarrayTest (46.98 s),
SRAMClockedTest (0.14 s), SRAMContractTest (27.90 s) and subsequent SRAM
timing/lower-level memory checks pass. The large structural clocked array
needs a separate measured rerun with a bounded larger address-space limit
and host-memory guardian; no circuit pass or defect is inferred from this
allocation-limit failure. Keep this test out of the uncapped Node group.

ALU8ContractTest passes in 69.01 s; ALU32 parts, register file and instruction
oracle/lockstep/mismatch-detection checks pass. The native run has reached
the original single-cycle stage/core tests.

At 335 completed native checks, 329 pass and the same six failures remain.
The suite has passed the old single-cycle stage/core/system and program cases
and is running baseline-pipeline programs. A progress parser now scans every
result line (including failures without whitespace after CTest's dots) rather
than relying on tail summaries, preventing missed fast failures.

At 407 completed native checks, 401 pass with the same six failures.
Baseline-pipeline programs pass, followed by advanced-core/predictor program
coverage through the backward-taken policy. No additional failure appears
in the full result-line scan.

At 458 completed checks, 452 pass and the same six failures remain. The old
advanced-core/predictor program matrix completes, and the new foundation
rerun begins: MultiplyStepTest (0.42 s), DivideStepTest (0.48 s),
Multiplier32Test (10.01 s) and Divider32Test (8.89 s) pass. MulDivUnit32Test
is running.

The fresh registered M/core/device foundation panel through check 499 passes,
including arithmetic, semantics, table-only publication, five-stage core,
compiler smoke, standalone devices and recorded component scenarios.
SimulationTimeTest and SimulationTimePlaybackTest pass. Integrated FF
(8.86 s), cached FF (9.93 s) and cached DRAM (10.55 s) checks pass on the final
native library. At 504 completed checks, 498 pass with the same six failures.
Wide-time and repeated/fidelity/quantum system checks follow.

Fresh native wide-time system runs pass above 2^53 (8.97 s) and 2^63
(8.42 s). DeviceSystemLockstepTest passes in 57.81 s, including repeated
execution, event quantum 97, expanded core and fully structural logic around
scalable native memory. Input validation (0.03 s) and explicit console/device
evidence-budget failures (16.92 s) pass. At 509 completed checks, 503 pass
with the same six outstanding failures; full-system faults are running.

The six-case architectural fault integration passes in 48.24 s. The compiled
bring-up check also passes in the full native suite (90.25 s total test time,
distinct from the earlier probe's construction/execution measurements).
The first compiled game, RV32IMRecordedGame_2048_Test, is now running. At
511 completed checks, 505 pass with the same six outstanding failures.

### First compiled game execution pass

RV32IMRecordedGame_2048_Test passes in 627.21 s total CTest time. This is
actual RV32IM instruction/device execution with the runtime's ISA/device
checks and the registered game frame-count/console/report assertions. It is
not yet the later exact host-C frame/fingerprint comparison or a measured
live FPS result. Sokoban is now running. At 512 completed checks, 506 pass
and the same six outstanding failures remain.

The recording reference now explicitly identifies the packaged Pages build
as the browser worker/WASM entry point and the CLI as the native recording
entry point. A prepared new-layout merge accepts generated new entries while
asserting every baseline type/root/profile value is unchanged; generation,
merge execution and final layout/browser validation remain pending until
serial simulations finish.

RV32IMRecordedGame_sokoban_Test passes in 836.98 s total CTest time,
including its right/restart/up script at ticks 4,501/5,501/6,501. These inputs
are well inside the 500,000-cycle runtime budget. Snake is now running.
At 513 completed checks, 507 pass with the same six outstanding failures.
Exact host frame/fingerprint comparison and per-update performance remain
pending; total test duration is not a frame-rate measurement.

### Prepared provenance-checked native recording reuse

The recording CLI now hashes the complete `recording.json` bytes in addition
to individual frames. RecordedGuestWasmParityTest may consume the native
acceptance campaign's ten immutable baseline records through
`CIRCUITSIM_NATIVE_GUEST_RECORDINGS`. It verifies the current complete source
map, native executable hash, fixture-manifest hash, recording hash, frame
hashes, guest/ELF/script identity and compact FF/quantum/cycle configuration
before using each native result. Invalid/stale artifacts fail explicitly;
there is no silent fallback. The default still executes native guests first.
This avoids a duplicate multi-hour native sweep after the required native
repetition/fidelity/quantum campaign, while retaining the same exact native/
WASM result comparison. Positive reuse, stale-artifact rejection and actual
WASM parity remain unrun. No C++ implementation or running binary changes.

Snake's registered CPU/device game check passes in 859.59 s; Pong is running.
At 514 completed checks, 508 pass with the same six outstanding failures.

The later exact-reference CTests now invoke the existing bounded recording
CLI for their fresh native run before compiling the host reference. Each run
preserves its recording, exact peak RSS, progress and full provenance in a
unique `<build>/recorded-guest-native/<guest>-...` directory. An index is
published only after every exact frame/report assertion passes. The prepared
acceptance campaign may reuse that immutable baseline after validating its
source/binary/fixture/recording hashes and configuration, then reruns the cheap
current host-reference check. Native repetitions, quantum and expanded-core
comparisons still execute. Reference-only and existing-recording paths remain
free of another native run. This consolidates validation through the existing
CLI and preserves evidence that the previous check discarded; its actual
registered execution remains pending. All C++ binaries remain unchanged.

Pong's registered CPU/device game check passes in 247.71 s. Tetris is now
running; four of five main games have passed their first circuit check.
At 515 completed checks, 509 pass with the same six outstanding failures.
The ten exact-reference cases remain the stronger next gate.

### All five main games pass their first circuit check

Tetris passes in 792.84 s. The five registered CPU/device game checks now
all pass: 2048 627.21 s, Sokoban 836.98 s, Snake 859.59 s, Pong 247.71 s,
Tetris 792.84 s (total CTest durations, not FPS). The disabled/reset/input
system scenario also passes (7.22 s), followed by VisualizerModuleBindings
(6.89 s), FFMemoryBindings (1.77 s), VisualizerSessionContract (10.60 s) and
VisualizerPinIdentity (4.98 s). At 521 completed checks, 515 pass with the
same six outstanding failures. Automatic-layout validation is running;
exact-reference/frame/fingerprint cases still follow.

### First exact-reference artifact and remaining viewer data

Automatic layouts pass (46.52 s), as do DocumentationExamples (33.64 s),
TimingTableDocumentation (0.77 s), RV32IReferenceDocumentation (2.56 s),
VisualizerProfileStore (10.53 s), VisualizerTimingProfile (6.16 s), registry
coverage and strict game rules. ColorPaletteTest finds new uncolored M/device
types and existing uncolored arbiter/predictor layout variants. Added semantic
colors and variant prefixes while preserving every old explicit palette value
(`build-rv32im/palette-preservation.json`); expanded the existing parent/child
and width-variant assertions. Its rerun is pending.

StaticLayoutTest reports 5,862 missing entries, all in the new
`rv32im-device-system` scenario; the other 362 aliases report zero missing.
The planned new-system layout generation/merge remains pending. These are
the seventh/eighth reported full-suite failures and are tracked explicitly.

The new CLI-backed exact-reference bring-up passes in 74.08 s, exercising
retained evidence end to end. Artifact
`build-rv32im/recorded-guest-native/bringup-cwzebwvz/`: 23,922 cycles, 6,445
instructions, 10,532,992 events, maximum queue 1,598, zero setup violations,
two exact reference frames/reports. Construction 5.81227 s, execution
67.50694 s, capture/check 0.001668 s, peak RSS 146,108 KiB. Frames at cycles
22,510/23,852 and 6,092/6,428 retired instructions: later update 1,342 cycles,
336 instructions and 3.58997 s (~0.27855 checked updates/s). Recording SHA256
`54ffd74c7153af3071f23a03b8994b6655e4efbcc4c89b5d72fa8a45901a73ff`.
This is checked batch throughput, not delivered live FPS. The exact 2048
reference case is now running. At 532 completed tests, 524 pass / eight fail.

### First exact game frame/reference pass and measured update rate

The CLI-backed exact 2048 comparison passes in 509.27 s total CTest time.
All four frames and stable final report/fingerprint match the host reference.
Artifact `build-rv32im/recorded-guest-native/2048-zjjyt27f/`: 165,725 cycles,
39,714 retired instructions, 85,068,965 events, maximum queue 1,598, zero
setup violations; construction 5.81033 s, execution 502.82418 s, capture/check
0.003353 s, peak RSS 146,536 KiB (~143.10 MiB). Frame cycles
85,567 / 103,740 / 121,584 / 147,752; retired counts
22,144 / 26,754 / 31,290 / 37,829. Host presentation times
268.04727 / 324.92873 / 380.36760 / 462.66316 s.

The first draw costs about 268 s in this compact profile. Its three changed
updates cost 18,173 / 17,844 / 26,168 cycles and 56.88146 / 55.43887 /
82.29556 s: mean ~0.015415 checked updates/s (about 65 s/update). This is
a current bounded batch measurement with ISA/device verification, not a
delivered live-mode rate. Capture/check cost is a tiny fraction of execution;
this evidence points to CPU/device simulation and guest drawing work, rather
than host image copies, as the work to address before useful live play.
The alternative fidelity measurements remain pending, so this is a result
for the current compact profile, not a claim that every supported profile
has the same cost. Recording SHA256
`0c0c9bc481c2d6c24217d1e49272eb2c2744a2b006d042f27eb91811117f4c7b`.

At 533 completed native checks, 525 pass / eight fail as already recorded.
Sokoban's exact-reference case is running.

The exact Sokoban comparison passes in 681.30 s. All frames and the stable
final report/fingerprint match. Artifact `build-rv32im/recorded-guest-native/sokoban-dfbprz2m/`: 229221 cycles, 56570 retired instructions, 115310242 events, zero setup violations. Execution 674.7497866 s, capture/check 0.003417458 s, peak RSS 146400 KiB.
Frames: cycles [160674, 176272, 197525, 211255], host seconds [482.1934248, 529.1639371, 594.3851224, 635.4484989]. Later presentations/changed updates: 3/3 over 153.2550741 s (0.019575208309530287 presented updates/s, 0.019575208309530287 changed updates/s). Recording SHA256 `b49e7a8704968e989ff87d33084b3c8dc2efc9f86a8a455beb1202f2869fdbef`.
At 534 completed native checks, 526 pass / eight fail as already recorded;
Snake exact-reference execution follows.

The exact Snake comparison passes in 723.54 s: all four frames and stable
final report/fingerprint match, including the queued direction tap/release.
Artifact `build-rv32im/recorded-guest-native/snake-ldl_t03f/`: 237736 cycles, 57986 retired, 119940556 processed events, zero setup violations; execution 717.0569322 s, capture/check 0.003326362 s, peak 146476 KiB. Frame cycles [163598, 176709, 195380, 219773]; host seconds [493.2895079, 535.115668, 594.8538404, 674.1541289].
Later presentations/changed updates: 3/2 over 180.86462100000006 s; 0.016586991880518185 presented updates/s, 0.011057994587012124 changed updates/s. This distinguishes a direction-only presentation from actual motion.
Recording SHA256 `79e6d56624771ba57cecb6567945c3e30c7b1d142c40b651bce00335c04af9a1`. Pong exact execution follows;
535 complete / 527 pass / eight previously recorded failures.

The exact Pong comparison passes in 220.14 s: four frames and stable final
report/fingerprint match. Artifact `build-rv32im/recorded-guest-native/pong-2khpddrf/`: 72209 cycles, 16226 retired, 34322826 events, zero setup violations; execution 213.1759527 s, capture/check 0.004534899 s, peak 146404 KiB. Frame cycles [35781, 38096, 40405, 54258]; host seconds [118.8855296, 125.9625182, 133.1214255, 174.4300063].
Later presentations/changed updates: 3/1 over 55.544476700000004 s; 0.05401077079550557 presented updates/s and 0.018003590265168526 changed updates/s. Recording SHA256 `47a5256d703899c51f32949f2b206e52f7a1c990816a8877aa2715e5b3f96db6`.
At 536 completed checks, 528 pass / eight previously recorded failures;
Tetris exact-reference execution follows.


### Completed preceding native subset and expanded phase-6 scope

The serial capped suite completed 543 registered checks: 535 pass and eight
previously listed follow-ups, total 16061.15 s. All ten exact host C guest
comparisons passed, including Tetris (809.23 s), 2048-merge (465.80 s),
Tetris line clear (621.11 s), Pong rally (285.45 s) and Snake wall (633.31 s).
The twelve Node-dependent checks still require their separate resource scope.
An intended cancellation found the run had already finished; no signal was sent.

The owner added general device parameters, a larger-screen check and display/
keyboard visual abstractions to phase 6, then authorized phase 7 after phase
6 is complete. See the [parameter/visual record](2026-10-21-device-parameters-and-visuals.md).
The old-binary game results remain dated evidence. Source changes during its
last cases mean their post-run source map cannot establish that binary's
source; a frozen preceding source/binary receipt is retained. Final artifacts
must be captured after the updated build. The recording CLI now hashes source
and binary before execution and rejects concurrent changes before publishing
success evidence. No phase-6 completion or delivered live FPS is claimed.

### Five acceptance follow-ups resolved; viewer compatibility reruns

Fresh separate-scope reruns pass WebTestRunnerTest (0.27 s),
MemoryDeviceDocumentationTest (7.82 s), VisualizerColorPaletteTest (0.43 s),
RV32IProfileSweepTest (69.61 s) and RV32ITopologyTest (134.12 s). The latter
two retain their AS 1024/2048 MiB child limits, with no smaller outer hard
limit. The eighteen-check scope peaks at 551928 KiB and completes in 255.25 s,
sixteen passing initially. RegisterFileProfileMigrationTest then passes
unchanged outside the process sandbox (EPERM despite child output), and
VisualizerRewireGeometryTest passes after its VM harness loads the existing
`structureKeyed` helper. No geometry assertion or product rule is weakened.
The new device JS check passes its syntax, palette, key and larger-image checks.

Three original follow-ups remain: measured delay-conservatism exceptions,
the larger SRAM case's bounded allocation retry and new-system saved layouts.
The SRAM guard was waiting below its 2200 MiB start threshold. Only that
waiting guard was terminated to permit lower-memory work; it had no child
and no SRAM simulation had begun. Retry remains pending when headroom permits.

### Parameter and image-budget checks extended

Seven strengthened native checks pass in 150.38 s, peak 151236 KiB: every
queue depth now also resets; console/button devices exercise non-default
full/drain/overflow/wrap behavior; the CPU reads configured display geometry
before presenting above the old aperture. Both 128×96 and 97×65 complete 41
cycles with zero setup violations. A 512×512 synthetic guest explicitly
exhausts the aggregate capture byte budget, and the same 17 PRESENT commands
complete with frame retention disabled. These are memory/routing checks,
not whole-screen FPS measurements.

The expanded-core bring-up completes 23922 cycles / 6445 instructions and
matches all exact host reference frames/reports in 251.0733421 s execution.
This is a dated measurement before the initial renderer revision, and is not
a faster profile on this host snapshot. No delivered live rate is inferred.

Native evidence now binds hashes for native code and build/guest tools;
viewer files have a separate `viewerSourceSha256AtStart` snapshot. Frontend
changes cannot alter a native simulator binary, so cache reuse compares the
full native map, native binary, fixtures and recording bytes. Viewer changes
are checked in their own browser/site acceptance. This avoids treating an
unrelated frontend edit as a change to native CPU execution.

### Initial renderer revision: exact host output retained

The guest renderer initialized its previous-cell cache to 255, forcing a
second paint of black cells after the startup clear. It now uses the BSS's
zero-initialized cache and paints only nonblack starting cells. Startup still
clears video because hardware RST retains drawing memory; this preserves warm
startup behavior. Later moves/restarts continue repainting every changed cell.
No hardware, ISA, device contract or game rule changes.

The pinned GCC 12.4 build/audit passes for all ten revised RV32IM fixtures.
Ten strict host builds produce byte-identical complete frame sequences and
reports compared with saved preceding expectations, including all boundary
presets and Pong's independent full-grid projection. This establishes software
output equivalence; actual revised guest circuit execution and performance
remain to be measured. The preceding native game timings are historical
rather than current-software results.

### Conservatism baseline audit: zero comparisons are not evidence

The existing `build-cleanup/tests/DelayRuleTest` executable is dated October
19. Selecting the cache-controller IDs compared zero parts, so it cannot
reproduce the current cache cases and supplies no baseline pass/fail evidence.
The current isolated cache source differs only in two native-width time aliases;
the 754 legacy tables remain byte-identical, but that does not establish the
older binary's exact implementation. Current measured diagnostics are running.

This exposed a validation-tool issue: an unmatched part selection printed a
passing result after zero comparisons. The current CLI now rejects that case,
with a small CMake regression test. This source change is staged pending build;
all existing safety assertions and valid comparison cases remain in place.

### Current conservatism measurements recorded without changing hardware

The four selected current-source parts were actually compared: 6267 seeded
situations, 8129 common changed outputs, 8332 gate changes and 15618 inner-part
changes. No part broke the timing safety rule. The diagnostic run took
111.61 s and 319980 KiB peak RSS under AS 768 MiB. It measured these integer
floors (typical/worst percent): cache controller 8-bit 84/61, 16-bit 83/64,
32-bit 85/75; M core 29/25. These reproduce the rounded shares in the full run.

Added those measured per-part floors through the test's existing exception
mechanism. Cache cases use SAMPLE: the fixed sample does not reach every
output's longest path. The M core uses the existing DATA rationale for whole
cores beyond the 60000-node twin budget (its graph has 170612 nodes). Global
70/90 percent targets, all safety/strict-settling assertions, circuits and
stored delay tables remain unchanged. The floor metadata is staged; a normal
(non-report) comparison and the invalid-selector regression must pass before
this follow-up is resolved. No valid pre-change cache binary comparison was
available, and no earlier pass is claimed.

### Current input and provenance acceptance

The sampled-input correction passes consecutive-edge/uint64-sequence and
halt-boundary tests. A fresh audited bring-up completes 23922 cycles in
72.01335986 s with two exact frames, exact script metadata, checked reports
and zero setup violations. The recording CLI additionally passes forced
time/cycle/address-space exhaustion and stale-output rejection, with no
success artifacts on the failed bounded runs. Build configuration and
compiler-launcher provenance are retained alongside native source/binary
and fixture/frame hashes. Portable refresh is running serially.

All 363 native static scenario layouts pass after adding 52 entries while
preserving all old values. The large SRAM retry stops safely at the host
memory guard (exit 75); its normal CTest path contains no redundant preview.
No shared scenario behavior or history contract was weakened to force it
through the available memory. This regression still needs headroom.

## 2026-10-22 — Owner-directed wrap

The owner requested checkpointing this work and merging into `alpha`, which
now includes the newer visualizer commits. Do not start the long revised-game
campaign during this wrap or label phase 6 complete. Integration checks and
primary-work preservation are recorded in the [wrap record](2026-10-22-rv32im-milestone-wrap-and-integration.md).
Phase 7 remains unimplemented.

## 2026-10-22 — Generalization and acceptance resumed on main alpha

Work is in the sole main alpha checkout. Preserved native and WASM build
outputs were moved from the ordinary archive into `build-milestone` and
`build-wasm-milestone` without copying or deleting evidence. Their CMake source
and binary paths are retargeted to main alpha; the native build keeps Python
deployment off. Earlier paths remain historical evidence only.

The shared word work is recorded in
[component width generalization](2026-10-22-width-component-audit.md). Guest
drawing now queries display metadata and aligns word stores to the actual row
address. All ten fixtures rebuilt with pinned GCC 12.4 and passed the final
RV32IM-only instruction audit. Host rendering checks and circuit runs remain
pending. The CLI and WASM gain an additive custom-surface path for compiled
97x65 bring-up parity; default game geometry is unchanged.

Native builds are serial under the existing 768 MiB address-space/headroom
guard. Mechanical extraction errors were corrected during compilation; no
successful final build or test result is inferred yet. Cache-layout comparison
now releases its native session before spawning the portable comparison,
retaining the same assertions while reducing overlap.

### Current-source native checkpoint

The word/geometry panel passes 10 checks; the related regression panel has
104 initial passes plus a successful bounded-heap retry of its one Node
address-space failure. Integrated M/system clock checks, old register/ALU
fidelity, all device scenarios, cached FF/DRAM, wide/unsigned virtual time,
large display and the legacy cache-layout check pass. See the linked width
record for exact resource/check evidence. SRAM retries and remaining
current-source game/WASM/browser/package scopes are still in progress; phase
6 is not yet accepted.

### Remaining native resource gates resolved

The large SRAM subarray, clocked subarray and contract checks now pass in
89.04 s at 764,540 KiB peak RSS, under a 1,536 MiB address-space cap and the
unchanged 512 MiB host-headroom stop. The final stronger width panel passes
all ten checks in 11.07 s at 50,036 KiB. All 18 affected legacy timing tables
are recomputed under each of Unit, Embedded and Desktop: 54 comparisons,
zero differences. No timing-table file needs a numeric change.

The API audit preserves all 175 names/signatures/docs in the pre-milestone
snapshot and retains all five later additions; current total 183 includes
`create_alu`, `device_view_kind` and `supports_register_view`. The earlier
180-name checkpoint is a preservation report rather than a full signature
snapshot, so the five later signatures are not claimed independently compared.

The new ALU16 layout contributes 23 entries through an insertion-only merge;
old entry values and original formatting are retained. Full static validation
passes with zero problems in 367 scenarios at 288,088 KiB peak RSS. Current
WASM/site regeneration is underway, using one fresh process per requested
example group to avoid retaining earlier WASM heaps. Game/WASM/browser
acceptance remains pending; phase 6 is still open.

### Final acceptance selection and resource policy

The current native inventory has 570 registered checks. The completed width,
dependency and SRAM panels account for 118 distinct checks. The remaining
selection contains 437 checks, plus 15 compiled-game registrations whose
execution/reference assertions are covered by the stronger ten-guest campaign
(repeat, quantum and expanded-core comparisons as well as the baseline).
These counts describe the selection, not completed results.

Keep the existing native regression gate. Reuse each accepted native baseline
for WASM and browser comparison only after checking source, binary, fixture,
recording and frame hashes. This avoids repeating native game execution in
three different harnesses. Native simulations, compilation and browsers run
serially. Node uses a bounded heap without a virtual-address cap; native
checks retain bounded address space except tests that already install their
own larger bound. The host guard stops only this task's descendants if
available memory drops below 512 MiB. Current-source success evidence and
unrun/failure lists will replace these pending counts when complete.

### Current WASM package and width execution pass

The guarded serial `tools/build-pages.sh build-wasm-milestone` run completes
in 24:36.66 with 596,608 KiB peak RSS and exit zero. It regenerates all 158
example groups in separate processes, including `alu-word16`, then builds
the documentation strictly in 40.07 seconds. The auxiliary verification,
recorded-guest and delay-snapshot targets build serially in 14.38 seconds,
314,992 KiB peak RSS. No host OOM occurs.

`WidthComponentsTest` also passes in WASM. Complete deterministic snapshots
contain 808 stored tables per timing profile, across unit/embedded/desktop;
native and WASM bytes are identical (2,171,241 bytes), retaining SHA-256
`b0c61d9e7aa62175cfa56f8654b5a95d89e1ac551e88a358dc767a7825ead98f`.
This snapshot comparison does not recompute all tables from gates; the 54
affected-table recomputations are recorded separately above.

The new packaged WASM SHA-256 is
`5d60d97e8860d28de72cea4f1ffd7dccf5a0e7b75780ca968677fec83292d15b`;
the native guest runner is
`73964cf413031a0bcef749be0baa881b6f5144d409d152b5cdd65598a4043557`.
Logs and comparison receipts are under `build-milestone/acceptance-width/`.
Cross-platform component checks are running next. Game/browser/live-preview
acceptance and the remaining native regression selection are still pending.

### Fifteen portable component and compatibility checks pass

The selected web runner completes 15/15 checks in 455.07 seconds, with
528,788 KiB peak RSS for the guarded invocation. Coverage includes old
ALU8/ALU32 topologies/states/fidelity paths; FF/native-device and all 20
transaction-endpoint comparisons; memory inspection, registers and SRAM;
baseline/advanced/memory RV32I scenarios; all 367 scenario descriptors;
the 22 RV32IM/device scenarios; and exact wide-time execution/rewind.
`VisualizerScenarioWasmParityTest` now also compares ALU16's complete
topology, examples, states and backward scrubbing. The separate full width
semantic test passes in WASM in 21.30 seconds, 258,572 KiB peak RSS.

Receipts are `acceptance-width/web-components/results.json`, its individual
logs, and `wasm-width-semantics.log` under the isolated native build. No game
WASM comparison or browser result is inferred from these component checks.
The remaining 437 native registrations are next, before source freezing for
the expensive ten-guest repeat/quantum/fidelity campaign.

### Complete Unit timing verification passes

The first full timing-profile verification passes in 621.74 seconds under
the 768 MiB address-space cap. All 808 Unit entries are recomputed from the
circuits and match the stored file, including the no-gate-list inventory.
Embedded and Desktop verification are still running/pending in the same
serial native regression selection. This is a circuit recomputation, beyond
the earlier stored native/WASM snapshot comparison.

The component guide now documents `RegisterStateView` and `DeviceView`,
including geometry metadata, existing observer pin/state contracts, legacy
fallbacks and the read-only recorded-time boundary. Browser acceptance also
includes the new word-component page/ALU16 recording, bringing that page
panel to 23 cases; its execution remains pending.

### Complete three-profile timing verification passes

Embedded passes in 614.10 seconds and Desktop in 616.44 seconds. Together
with Unit, all 808 stored entries per profile (2,424 entries total) are
recomputed from the circuits and unchanged, and the no-gate-list inventory
matches. These runs preserve the source tables; no regeneration edit or
extra rebuild is needed. The serial native selection continues with the
old memory/simulator/core/documentation/playback checks.

The game WASM parity driver can now optionally retain each successfully
compared recording and a hash receipt through
`CIRCUITSIM_WASM_GUEST_RECORDINGS`. The acceptance run will use this to
publish measured WASM update/capture costs alongside native costs, without
running the same game again for benchmarking. The option is unused by
default. Its syntax check passes; actual export/parity execution is pending.

### Full delay-rule gate passes

The normal registered `DelayRuleTest` passes in 1,253.04 seconds, followed
by `DelayRuleSelectionTest` (0.08 seconds). The strict-settling variant is
running next. Current host headroom has improved, but the same serial
execution, address-space caps and host guard remain in use.

An additional check is staged in the existing standalone
`ArithmeticTimingOptimizationTest`: 72 adder/ALU cases across widths,
operation contracts, flag configurations, topologies and three profiles.
It requires valid conservative four-state settling graphs and checks that
the requested topology changes the actual prefix-level structure. Build and
execution wait for the active regression pass; no product/library source is
changed by this test addition. Do not infer its pass from the older binary.

### Resource evidence scope

The recorded success/limit outcomes and peak RSS describe this task's
guarded commands. No OOM kill has been observed for those commands. The
unprivileged account cannot read the system kernel journal, so whole-host
OOM events across unrelated work have not been independently audited. This
qualifies the earlier "no host OOM" wording; it is not a system-wide kernel
audit. The host guard retains a 512 MiB available-memory floor and only
terminates this task's identified descendants.

### Legacy comparison identity and strict diagnostic limits

`DelayRuleStrictSettlingTest` completes in 1,231.72 seconds. Its CLI option
sets report-only mode, so its successful exit is diagnostic coverage rather
than a zero-difference assertion. It reports 1,836 gate-only output moves,
507 outputs settling sooner in a parent of compact parts, and (after an
earlier X/Z) 836 opposite-value observations and 120,719 cases where only the
compact form shows 0/1. Preserve these observations as fidelity limits.

The normal comparison covers 160 parts and 175,031 situations. All 160
numeric per-part rows are identical to the saved pre-generalization
`native-full-capped.xml` baseline, including the 127,339 internal changes
within each part's table, 466/169,277 earlier parent settlements, seven
opposite-value observations and 120,598 compact-known observations after X/Z.
The earlier normal invocation failed only its subsequently corrected floor
metadata; no old normal pass is reconstructed. The archived strict stdout
was truncated, so identical strict counts are not claimed.

The baseline XML SHA-256 is
`5acd46557107fdbdfe27f3b307e673adfe3529453fe53b8924f5c4c8c5d630bd`.
Full current outputs and `delay-rule-baseline-comparison.json` are retained
under `build-milestone/acceptance-width/`. Functional/protocol equivalence and
zero setup violations at supported game clocks remain separate acceptance
checks. These diagnostics do not establish arbitrary-transient waveform
identity or physical hold-time certification.

### Old cache, memory, core and compiled bring-up checks advance

The current serial native pass clears `CacheReadTest` (377.66 seconds),
`CacheControllerLockstepTest` (274.35 seconds), old memory-device/endpoint
contracts, primitive/storage checks, all three original core building-block
paths and their program/prediction sweeps. The integrated evidence-budget
test passes in 44.46 seconds; the newly rebuilt compiled bring-up passes its
console/frame assertions in 117.97 seconds. This direct test does not export
the immutable baseline required by the later game comparison campaign.

At this checkpoint 418/421 checks in the capped batch pass; its final static
layout/timing-profile/partition checks and the 16 separately bounded Node or
self-capped checks remain. Current model/source freshness is recorded in
`pre-campaign-source-manifest.json`, with no include/src file newer than the
guest runner. Its canonical model-manifest SHA-256 is
`f988d6e3fae05ea9c9861b4cc96357fc03b000cd7a727ba27a31f0135f2d4ee7`.
The queued game pipeline checks that map and the unchanged guest binary
before beginning its expensive captures.

### Capped native batch completes

All 421 selected registrations pass in 6,906.27 seconds (1:55:06 wall time),
605,760 KiB peak RSS. Full details are preserved as
`acceptance-width/native-capped-details.log` before the next CTest batch
reuses its temporary logs. Together with the earlier 118 distinct checks,
539 registrations have current passing results. The remaining 16 Node or
self-capped CLI checks run next without the outer address-space limit and
with a 256 MiB Node heap. This checkpoint still excludes the queued new
72-case timing addition, game campaign and browser acceptance.

### All 555 direct native registrations and new width timing pass

The final 16 checks pass in 528.49 seconds, 553,192 KiB peak RSS. The union
with the earlier 118 and capped 421 contains 555 distinct passing native
registrations; `native-555-passed.json` retains their names. The other 15
game registrations remain covered by the pending stronger recording campaign.

The focused rebuild of `ArithmeticTimingOptimizationTest` succeeds. Its
updated invocation passes in 25.65 seconds, 41,476 KiB peak RSS, including
all 72 new timing cases. Unit-profile adder settling bounds are:

| Width | Ripple ticks | Prefix ticks | Prefix levels |
| --- | ---: | ---: | ---: |
| 8 | 18 | 13 | 3 |
| 16 | 34 | 16 | 4 |
| 32 | 66 | 19 | 5 |

These are conservative four-state settling bounds, not exact minimum delays,
physical clock rates or hold proofs. All three profiles, ALU semantics,
full/core flag configurations and topology structure checks are retained in
`word-timing-results.json` and the verbose log. The rebuild changes only the
standalone test binary. The model source map and guest runner still match
the regression freshness receipt. The first native recording baseline has
now started; no game campaign acceptance or performance result is claimed yet.

### Bring-up recording comparisons and progress report

The current-source bring-up baseline, repeat and event-quantum-97 runs pass
their exact host references and deterministic comparison: each completes
38,811 cycles and 11,533 retired instructions, with two presentations and
two changed frames, zero setup violations and 16,630,416 processed events.
Execution times are 137.36, 135.04 and 137.59 seconds respectively. These are
three of the planned 40 native recording runs, not three accepted games.
The expanded-core bring-up is running; the remaining native games, larger
compiled surface, game WASM comparisons and browser checks are queued
serially. The campaign retains its 768 MiB native address-space cap and
512 MiB host-availability stop. Phase 6 remains open, and phase 7 has not
started. Status and the width record now reflect the completed 555-check
native regression scope without presenting the pending acceptance as done.

### Native 2048 output and restored visual preview

The owner asked which game runs had completed and to see their output.
Bring-up now passes baseline/repeat/quantum-97/expanded-core: the expanded
case completes the same 38,811 cycles in 396.1382 seconds. The first current
2048 baseline completes 169,601 cycles / 42,973 retirements in 619.7863 seconds,
with four presentations, four changed frames, zero setup violations and
85,821,195 processed events. An explicit saved-recording invocation of
`RV32IMGameReferenceTest.py` passes all four exact frames, stable reports and
no-lost-input checks. Its repeat/quantum/expanded comparisons remain pending.

Exported the accepted baseline bytes and one-second-per-frame GIF previews
under `site-docs/assets/rv32im/recordings/`, linked from the public recorded-
games reference. The five exported files include both JSON recordings and
a manifest with binary, fixture, source-manifest, recording and frame hashes.
Both source maps retain digest
`f988d6e3fae05ea9c9861b4cc96357fc03b000cd7a727ba27a31f0135f2d4ee7`.
The 2048 recording SHA-256 is
`fd0b7e4b42043361591722836f612d266f6c220f3b1cc0f9850cafa1efb37d51`.
The GIFs decode the actual RGB332 pixels using the viewer's bit expansion;
their replay spacing is independent of measured simulation time.

The live 8893 service was active but HTTP requests timed out: its strict
rebuild had failed against the old generated example bundle after ALU16 was
added. This corrects the earlier assumption that the last successful preview
remained responsive. Stopped only that preview service and held this task's
campaign scheduler while its timed 2048 guest continued to completion. No
other job was interrupted. Staged and hash-verified all 27 files from the
already-built current package, preserving the earlier package under
`build-wasm/preview-width-current/simulator-before`.

The strict preview preflight passes in 55.94 seconds / 178,652 KiB peak RSS.
After restarting the existing capped service, five documentation routes return
200; all 27 simulator files and five export files match their expected hashes
(removing only the known live-reload injection from the two HTML files).
The live viewer loads the actual bring-up/2048 files, compares every RGBA pixel
of all six frames, displays keyboard summaries and reports no page errors.
Screenshots were inspected. Receipts are under
`build-wasm/preview-width-current/` and
`build-milestone/acceptance-width/progress-visuals/`. This is saved-file replay
coverage; no additional WASM guest or live input run is inferred. The existing
serial campaign scheduler is resumed, the native live module is untouched,
and the full phase-6/game/browser gates remain open.

### Original circuit visualizer acceptance correction

The owner clarified that watching a game means using the original circuit
visualizer with abstracted display/input/device components. The separate
`games.html` recording page and GIF exports are useful evidence, but the prior
visual acceptance scope was incomplete. This correction adds the missing
original-viewer integration to phase 6; it must not be deferred to phase 7.

The original canvas already draws device abstractions and inspector previews.
Its device-system scenario does not load/run a game ELF. Conversely, the batch
game runner disables full histories and exports frames independently of
`VisualizerSession`. Required acceptance is scripted game selection/playback
in the original viewer, with the CPU/router/memory/device topology from the
same execution and synchronized display, keyboard and other device state.
Retain normal component selection, available hierarchy/detail and old playback.
Bound captures and retained circuit detail to avoid OOM; unrecorded intermediate
states must remain explicitly unavailable. Native/WASM/browser checks must
cover this integrated path. Phase 7 subsequently connects live host input to
the same original-viewer device components.

This clarification changes records and acceptance criteria only. The frozen
model/binaries and ongoing serial native game campaign remain unchanged;
completed CPU/device/reference comparisons remain useful. The existing
standalone-viewer tests cannot certify the missing original-viewer integration.

### Original visualizer integration implementation underway

The owner requested showing a game through the original visualizer now.
The old frozen-model campaign had already stopped: seven native recording
runs passed (four bring-up, three 2048); expanded-core 2048 exhausted the
500,000,000-event budget after 159,744 cycles and four presentations.
This was a failed run, not acceptance or an OOM. No owned game process was
running at the new implementation start. Preserve those receipts; they
concern the preceding binary, and no later game/WASM/browser pipeline step ran.

Added an optional read-only presentation observer to the existing checked
guest runtime and a borrowed-circuit export path in `VisualizerSession`.
It serializes the real topology, current indexed pin/wire values and current
device/register observations at settled PRESENT boundaries. Device observation
can explicitly read current state without enabling history. Old history-based
queries retain their existing behavior. The exporter bounds retained JSON to
24 MiB/64 checkpoints and asserts that observation changes neither event
counts, pending event time nor current simulated time. Uncaptured memory-page
inspection remains explicitly unavailable.

The original page has an additive captured-circuit transport and presentation-
checkpoint status. Its existing canvas, device abstraction renderer, inspector,
explorer and timeline remain the UI. No second CPU is constructed for replay.
The new native target builds serially in 54.83 seconds, 267,136 KiB peak RSS,
with deployment off. A real scripted Pong rally capture is now running under
the same 768 MiB address-space/512 MiB headroom guard. Frontend, capture,
reference and browser checks are not yet claimed; the original-viewer link
will be published only after its actual recording has passed them.

### Original-canvas Pong delivered and checked

The current native capture completes 115,686 cycles / 29,005 retirements,
18 presentations and 18 changed frames, with zero setup violations.
Execution is 341.4513 seconds; the guarded process takes 347.85 seconds and
149,704 KiB peak RSS. The actual topology contains 6,426 components, 17,688
pins and 8,070 wires. Retained topology/checkpoint JSON is 10,835,282 bytes,
with history disabled. All 18 observer non-mutation assertions and post-run
source/binary hashes match. The independent host reference passes every
exact screen, stable report and no-lost-input/ISA/device assertion.

The probe SHA-256 is
`322d20e6436b060ab17789f5000ddb67273d87f54a8bd6db04ee67912eff68e3`;
the scoped source-manifest digest is
`f5a405782b22744fe534a0a8e6f589fa1a17aa0b4a83b9e846aed105f82500d1`.
Raw native circuit JSON SHA-256:
`4c97459b840c02feacd27846f3533605f03f12416d50d9a22c9681f2b4516b42`.
Packaging adds a layout reference without changing the captured values. The
served packet is 10,970,249 bytes, SHA-256
`f9caba3cca752c72ee01b043d39e7f6a59ad06493ef0f3d6fa04699fce45777f`.
Its 780,816-byte layout has SHA-256
`5429bbccb028df6ca891101a3b084a00dd3949188f53ebe149bee1f9113153bc`.
The tracked export includes binary/source/ELF/script/frame/package receipts.

The first browser attempt failed on missing root/configured-display layouts.
Compatible inheritance and editable fallbacks fix this without changing the
global layout. Three later attempts stopped safely through the guard below
512 MiB host availability; they are failed/resource-stopped checks, not passes.
Full wiring, the global layout store and duplicate test transfers were costly.
The final mode uses original progressive scopes, the small layout sidecar,
a layout-declared 12-pixel detail threshold and test-vector hashes. Normal
scenarios retain their 1-pixel default; deeper detail loads on zoom/selection.
The successful browser test bounds the driver heap to 128 MiB and renderer JS
heap to 256 MiB. No other process was interrupted or memory limit relaxed.

The final live 8893 browser test passes in 20.00 seconds (time-reported maximum
RSS 182,572 KiB, not aggregate subtree RSS). It compares all 18 actual pin/wire
vectors by SHA-256 and exact display/input state, then checks the original
inspector, backward seek, Play and Device views off/on. No page errors or
WASM simulation-module request occur. Screenshots were inspected. The page
opens its real display inspector initially. URL:
`/simulator/?recording=../assets/rv32im/circuits/pong-rally.json`.

Six native checks pass in 84.28 seconds, 244,488 KiB peak RSS: DeviceInspector,
SimulationTime, integrated device system, input validation, evidence budgets
and VisualizerSession contract. The final three-check JS panel passes in
0.80 seconds, including topology, device views and captured-transport
contracts/layout/scopes/wide-time/normal-HTTP checks. Registry coverage passes;
web inventory discovers the new browser test. Strict preview preflight passes
in 43.55 seconds, 180,300 KiB. Later strict service rebuilds cover the final
frontend/package. All 28 simulator and four circuit-export files match live
hashes; the original page and documentation routes return 200.

Evidence: `acceptance-width/original-viewer-capture/`, native/JS/browser logs,
`original-viewer-browser/`, and `build-wasm/preview-original-game/`.
Contract: [recorded circuit checkpoints](../reference/recorded-circuit-checkpoints.md).
This supersedes the missing-integration assessment for Pong. The old campaign
remains stopped at its expanded-game budget failure; full game coverage,
WASM capture production/parity and final phase-6 acceptance remain open.
Phase 7/live input is unimplemented. The native live extension is untouched;
main alpha/single Git checkout remain, without commit or push.


## 2026-10-22 — Component replacement acceptance and fresh Pong

The complete component-retirement record accepts the generic ALU/word,
mux/decoder/register-file, M-unit and bus/pixel implementations, exact IDs,
profiles/layouts, Python/WASM APIs, timing artifacts and public references.
It records current focused native/API/reference/catalog checks, all 158
WASM example groups, three regenerated 809-entry timing profiles, complete
unit-file verification and six portable/browser registrations.

Fresh native Pong completes 115,686 cycles, 29,005 instructions and 18 exact
reference screens, with zero setup violations. Every retained pin/wire vector
and simulated checkpoint time matches the previous actual capture. Execution
is 613.0546424 seconds at this build/host state, versus 341.4513218 previously;
event counters are identical. This is not a controlled throughput comparison.
Current source/binary/frame/package receipts replace the old public capture.

The matching local browser package passes all 18 Pong snapshots, display and
keyboard observations, original inspector, rewind, Play and Device views.
It also passes all 22 device/M recorded portable scenarios, all 23 component
pages and the selected generic ALU/display/button browser sessions. Local
8893 serves the matching assets and current ALU references; remote deployment
is not checked. Screenshot review confirms readable ALU functional groups and
Pong in its display/inspector. The machine overview remains crowded by its
many wiring helpers; broader game visual acceptance should review that framing.

The old seven-pass campaign and expanded-2048 budget failure remain historical
evidence. Other compiled-game/native-WASM/expanded comparisons and final
performance acceptance remain required for full phase 6. No interactive
session/frontier or live input is implemented by component retirement.

The same final binary additionally passes the short 128x96/97x65 CPU geometry/
high-video-address/PRESENT check in 23.55 seconds. Direct current API checks
include 320x200. Compiled larger game workloads remain a separate acceptance
item; this does not mark full phase 6 complete.


## 2026-10-24 — Full gameplay circuit-capture suite underway

The owner requested all remaining gameplay recordings and explicitly called
out OOM risk. Scope is every packaged fixture: bring-up, the five ordinary
game scripts, and 2048 merge, Tetris line clear, Pong rally and Snake wall.
All ten will be freshly captured against one current source/binary snapshot,
independently compared with exact host frames/reports, packaged for the
original circuit canvas, and retained with per-run resource/provenance evidence.
This is the complete baseline recording set, not the separate four-variant
or native/WASM acceptance campaign. Those phase-6 boundaries remain open.

The exporter was rebuilt serially with deployment off under a 768 MiB
address-space cap. The existing guard requires 1,536 MiB host available before
each command and terminates only owned processes below 768 MiB available.
Native histories stay disabled, with existing 24 MiB/64-checkpoint retention
and event/cycle budgets. Each native command has a one-hour wall limit.
Packaging uses a 256 MiB Node heap; Node needs unrestricted virtual address
reservation, so it uses the host-headroom guard without the native AS cap.
Captures, reference checking, packaging and later browser checks run serially.
Old published receipts are preserved until the complete replacement validates.

The resumable local campaign is under
`build-milestone/gameplay-capture-suite/`; it calls the existing native probe,
reference checker and packaging CLI. A common source manifest and per-game
hashes bind the runner, ELF, script, source, frames, capture and layout.
Publication and browser acceptance are pending.


## 2026-10-24 — Complete native recording set and original-canvas checks

All ten baseline fixtures complete against the same native exporter and
source snapshot, including a fresh replacement for the earlier Pong capture.
Code state is `alpha` at `5549ee0` with the uncommitted constant/device
hierarchy changes identified by the common source manifest. No C++ source or
fixture was changed during this capture campaign. This extends the earlier
Pong-only native-to-browser evidence; full phase-6 acceptance remains open.

| Fixture | PRESENT checkpoints | CPU cycles | Execution seconds | Peak native RSS MiB |
| --- | ---: | ---: | ---: | ---: |
| bringup | 2 | 38,811 | 215.60 | 154.41 |
| 2048 | 4 | 169,601 | 973.83 | 155.33 |
| sokoban | 4 | 188,614 | 1084.89 | 155.44 |
| snake | 4 | 141,132 | 805.86 | 155.50 |
| pong | 4 | 88,367 | 496.61 | 154.84 |
| tetris | 4 | 191,818 | 1121.72 | 155.14 |
| 2048-merge | 2 | 135,080 | 769.08 | 155.28 |
| tetris-line-clear | 2 | 142,549 | 820.89 | 155.54 |
| pong-rally | 18 | 115,686 | 653.39 | 155.45 |
| snake-wall | 3 | 115,575 | 647.16 | 155.57 |

Total: 47 retained checkpoints, 1,327,233 CPU cycles,
336,174 retired instructions and 7589.03 native execution seconds
(2 hours 6 minutes 29 seconds; setup, reference checking, packaging and
browser work are additional). These host timings are not a controlled
performance comparison or delivered gameplay FPS.

Every fixture passes exact host frame/report comparison, known-pixel masks,
input-log/queue checks and zero setup violations. Runtime ISA/device checks
and the presentation observer's non-mutation assertions pass. The original
canvas browser test passes all ten sessions and every retained pin/wire
vector by hash, display/input state, inspectors, Device views, rewind and Play.
No simulation worker or WASM module is loaded for replay; no page errors were
reported. The bring-up system overview and Tetris line-clear preview were
inspected. The captured hierarchy retains the current system floorplan.

All native/reference/package/browser commands were serial. Native address
space was capped at 768 MiB; start headroom was 1,536 MiB and the owned-process
guard stops below 768 MiB available. Native peak RSS was at most 155.58 MiB.
Histories stayed off, with 24 MiB/64-checkpoint retention limits. Node and
browser renderer heaps were capped at 256 MiB, driver heap at 128 MiB; those
processes used the host guard without a native virtual-address reservation
cap. All ten captures and browser sessions completed without a guard stop
or OOM failure. Native RSS is per-process peak, not aggregate browser RSS.

Native exporter SHA-256: `8f8ea64f5e47e6333fe74803ee53538a940a12244e2ae4562411aafd7ee92eca`.
Common published source-manifest SHA-256:
`19d2cd76678f76b0711f7b21ff1d8e79d8cdd5a161fccc7cbd9c3a6d20cc1e32`.

The complete public set is under `site-docs/assets/rv32im/circuits/`, with a
47-checkpoint index, per-game captures/layouts, packaging hashes, source/build
evidence, exact-reference receipts and browser receipts. Public gameplay links
are in `site-docs/rv32im/recorded-games.md`. Earlier frame-only exports keep
attested source snapshots. Previous Pong files and the old public index are
preserved in `build-milestone/gameplay-capture-suite/prior-published/` and
`prior-recorded-games.md`. Raw captures, progress/resource/reference logs and
screenshots remain in that campaign directory. Canonical exports total about
85 MiB; only one recording is loaded per viewer session.

The 8893 preview was paused during publication to avoid simultaneous rebuilds.
Strict preview packaging and served-route/hash verification are pending.
Repeat/quantum/expanded-core/cache and native/WASM capture campaigns, live
input, larger-screen workloads and broader regressions remain separate.


## 2026-10-24 — Publication acceptance

This replaces the pending preview statements in the preceding section. The
strict isolated preview build passes in 96.85 seconds, at 193,288 KiB peak
RSS. The normal 8893 service then passes its own strict rebuild in 86.84
seconds and resumes serving. Its hard memory limit remains 768 MiB with
swap disabled. No simulation/WASM source or published branch was changed.

Live checks verify the game index, system reference and simulator routes,
all ten recording links, and 79 byte-identical assets: 53 capture/layout/
evidence/index files plus 26 simulator assets. The final live original-canvas
Pong check passes every one of its 18 pin/wire/display/input checkpoints,
inspector, backward seek, Play and Device views, without page errors or a
WASM simulation-module request. Its time-reported maximum individual-process
RSS is 201,880 KiB; it is not aggregate browser memory. Temporary isolated
HTTP test serving was stopped after acceptance.

The public index now links all ten native baseline recordings. Evidence
includes `live-verification.json`, `live-browser.log`, `live-browser/`,
`mkdocs.log`, `mkdocs-resources.txt`, per-fixture browser screenshots/receipts
and the staged/public manifests under the campaign directory. A final source
diff whitespace check passes. Full general regressions, configuration/WASM
acceptance and live input remain outside this recording set. No commit, push
or remote deployment was performed; phase 6 remains open for those other
acceptance requirements.


## 2026-11-03 — Closed: superseded by the game scenarios record

The ten packaged fixtures this record accepted (five 3-press games, four
acceptance boards, bring-up) were retired on 2026-11-03 and replaced by
short versions (live scenarios) and long versions (computer players'
sessions) in [the game scenarios record](2026-11-03-game-scenarios-plan.md).
The open phase-6 acceptance campaign (`accept-recorded.py`: baseline,
repeat, quantum and expanded-core runs of the ten fixtures) was not
completed and is retired with them; `ComputerSystemLockstepTest` covers
repetition, quantum splitting and the expanded core in CTest. Earlier
sections remain as dated evidence of the retired fixtures.

Record closed 2026-11-03.
