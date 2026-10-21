# Scripted device platform and games implementation

Started: 2026-10-21. Status: open.
Code state: isolated `rv32im-phase3` worktree from `60acda2`, with uncommitted foundation, wide-time, system and game work.

## Summary

The CPU/device system, bounded batch runtime, ten reproducible fixtures and
recording viewer are staged in the isolated worktree. The preceding binary
completed 535/543 capped checks; all ten exact host C game comparisons pass.
Subsequent inventory/fixture/palette/profile/topology/layout follow-ups and
the focused delay-floor rerun pass. The large SRAM retry stops safely for
insufficient host memory. General geometry/queue parameters, display and
keyboard observers, Python declarations and shared JS helpers pass focused
checks; all 363 scenario layouts pass with old values preserved. All three
timing profiles verify 808 entries with 754 legacy entries unchanged each.
Final revised-game repetitions/fidelity/quantum measurements and
WASM/browser/site acceptance remain. Phase 6 is open; phase 7 is authorized
after its gates and has not started.

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
