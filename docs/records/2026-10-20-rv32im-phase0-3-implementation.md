# RV32IM and device foundation implementation

Started: 2026-10-20. Status: open.
Code state: isolated branch `rv32im-phase3`, starting at `60acda2`.
Worktree: `.worktrees/rv32im-phase3` under the primary checkout.

## Summary

Implementing phases 0 through 3 of the [adopted roadmap](2026-10-19-device-io-games-plan.md#rv32im-scripted-roadmap): compatibility contracts, structural M arithmetic, new RV32IM core, and device foundations. Recorded game execution is now part of the separately authorized phase 4–6 work; live sessions remain later. Implementation has reached phase 3; final profile, compatibility and viewer/site/WASM validation is still in progress. Structural designs remain available. The separately authorized phase 4–6 work now adds compact gate-list forms derived from those structures; their current validation is recorded in the phase 4–6 topic.

The primary checkout has ongoing memory-system/site/viewer work. All feature edits and builds use this isolated worktree. No merge or shared build/module deployment is performed.

## 2026-10-20 — Isolation, memory budget and initial inventory

- The owner authorized implementation through phase 3 and emphasized avoiding OOM. Created the isolated worktree after filesystem approval.
- Initial host memory: 7.1 GiB total, about 1.2 GiB available; swap 3.6/4.0 GiB used. Build serially, with low optimization, no debug symbols, bounded address space, and memory-availability checks. Do not run compiles concurrently with another visible build.
- Current baseline includes expanded/behavioral cache control; older playability numbers predate those changes and are not new throughput evidence.
- Existing public component contracts and the primary dirty worktree are preserved. Baseline checks and implementation evidence will be added below as work proceeds.

### Memory-limited baseline build

The 768 MiB address-space cap stopped GCC while compiling the existing generated delay table (`ProvenPartDelaysUnit.cpp`): `virtual memory exhausted`. This was a controlled compiler failure, not a kernel OOM. Swap was essentially full, so the cap was not raised. For the baseline build only, copied the three pre-existing native delay-table object files into the isolated build after verifying every project dependency listed by their compiler dependency files byte-for-byte against this worktree. Object/dependency SHA256 provenance is retained in `build-rv32im/reused-delay-objects.json`. Other sources compile locally at O0/g0. This does not constitute recompiling the generated delay-table sources at the cap; record that build limitation explicitly. No shared object or source file was modified.

### Implementation progress (unverified code)

Added independent M decode/oracle semantics, gate-built 4/32-bit multiply/divide steps and iterative 32-bit units; added new M decode/execute/coordinator/core assembly code. Added structural request routing/fetch guarding, device frontend, FIFO/counter, console/timer/buttons and tiny framebuffer/pixel interpretation. These additions are not yet compiled or declared complete. Shared decode/core blueprint extractions are prepared; existing I entry points retain their families and defaults.

The original TestRegistry translation unit also exceeded the 768 MiB compile cap. It compiled with macro-expansion tracking disabled at a 1 GiB cap while available memory exceeded 1.4 GB; the baseline runner linked. Catalog inventory and two-port arbiter checks passed. The long O0 CacheReadTest was stopped without a verdict; a separately bounded baseline core/arithmetic panel is running. No other task's process was stopped.

## 2026-10-21 — Baseline failure isolated before feature linkage

The bounded baseline panel passed FullAdder, Adder32, ALU32 and RV32IDecoder, but all three original I-core checks segfaulted in the unmodified baseline binary. A debugger run (outside ptrace sandbox after approval) located the crash in `BasicComponent::publishTime`, called by `RegisterFile32x32Behavioral`. Source inspection established an unconditional access to `t.settled[out]` even when there is no delay twin and that vector is empty. Added the `by_gates` guard to that condition in this worktree only; it protects the table-only path without changing gate-twin timing. A focused regression and rerun are pending. This is an existing prerequisite defect, not a change in the new ISA semantics.

Available memory later increased sufficiently to compile the Unit delay-table source fresh at a 1.5 GiB address-space cap with macro tracking disabled. The next profile compile deferred when availability fell below 2.2 GB. Keep reused Embedded/Desktop artifacts flagged as build limitations until rebuilt. Current dependency equality did not prove historical object provenance; do not treat the earlier copied objects as a complete source rebuild.

### Compatibility baseline after the prerequisite guard

FullAdder, Adder32, ALU32, RV32IDecoder, RV32IFiveStageCore and RV32ISingleCycleCore passed in the still-baseline executable after the table-only publication guard. The original RV32IBaselinePipelineCoreTest reached the explicit 120-second low-optimization test limit. This is a bounded validation gap, not a newly passing full suite. The earlier catalog and arbiter baseline checks also passed; CacheReadTest was stopped without a result.

The device frontend design is now a registered single-request slot plus the existing structural response buffer. REQ_READY depends only on reset and occupied response/request state. Accepted payloads are retained before peripheral execution; response data/status sample at the completion edge and side-effect strobes fire exactly once. This corrects the unverified initial draft that could have made readiness depend on current command fields. Console-full writes may wait internally; reading status remains possible when the interface is idle.

### First linked feature checks

The feature runner builds with the new arithmetic/core/device sources. Sixteen focused checks passed in 13.68 seconds: TableOnlyDelayPublication, MultiplyStep, DivideStep, RV32IMSemantics, ClockedCounter, SynchronousFifo, DeviceRegisterPort32, BusErrorTarget32, AddressRouter32, InstructionAccessGuard32, ConsoleDevice32, TimerDevice32, ButtonDevice32, IndexedPixelSelector32, FramebufferDisplay32 and catalog inventory. These include fully structural 4/32-bit arithmetic-step cases and compact-child variants, with bounded history-off sweeps. Full-width arithmetic and M-core checks are running separately.

New contracts currently provide structural forms and reuse existing selectable lower-level parts; no behavioral replacement of a new core or peripheral is being claimed. TestRegistry integration is additive through a small append hook. Production-type inventory is extended; old entries remain. New low-level checks are not yet recorded viewer scenarios; frontend/examples work remains before declaring the relevant scenario path complete.

### Arithmetic/core panel and timing feedback

All eight further checks passed (300.01 seconds total, serial O0): Multiplier32, Divider32, MulDivUnit32, M decode/control/stage, execute, coordinator and five-stage core. Core tests compare each retirement and architectural state against the independent oracle, with delayed request/response channels, dependent M chains, load/store, x0, divide-by-zero, taken-branch squash and older-load-fault cancellation. These results precede the synchronous cancellation revision below and require a rerun.

The first strict structural M-unit clock analysis returned Unknown because cancellation drove asynchronous reset pins that the timing contracts require fixed during operation. Changed cancellation to a synchronous ownership-state clear, with enable overridden on that edge; external RST remains the asynchronous reset. Operand/result state is irrelevant once ownership is canceled and is replaced at the next launch. This replaces the initial cancellation wiring, not the 32-iteration algorithm. Separate certification is still pending.

Added scoped catalog parameters for counter divisor, timer divisor/frequency and framebuffer geometry, and retained the existing predictor parameter parser for the new core. No old parameter schema changes. Added a pinned-toolchain C smoke source/startup/linker/build script and ELF using `rv32im/ilp32`; disassembly has 81 32-bit instructions and six compiler-selected M mnemonics (the compiler expands signed high products using MUL/MULHU and sign correction). Explicit assembly-level cases retain coverage of all eight M instructions. Compiled execution and parameter boundary checks are pending.

### Compiled execution and recorded scenarios

The pinned GCC 12.4.0-2 RV32IM/ILP32 C fixture passed full core/oracle retirement, register, terminal and final-memory comparison, with its C self-check returning zero. Setup violations were zero at the conservative test cadence. All 21 ComponentScenarioTest aliases passed; they retain bounded wire history separately from the large history-off sweeps, share their existing logical test owners, and cover representative arithmetic, pipeline and device transitions. Added their frontend groups/aliases without changing existing aliases. One temporary CTest-only `simulationScenarioTest` name was accidentally generated from the labels list; the registry correctly rejected it. Removed it from source; the generated CTest configuration needs refreshing before the final panel.

After synchronous cancellation, the strict all-structural MulDiv unit certifies at **62 unit ticks** under declared falling-edge binary input transitions, fixed deasserted reset and enabled operation. This is a unit certificate, not a whole-machine or real-time throughput claim. Complete-core and realistic-profile analysis remains pending.

### Complete-core certificate and host-memory interruption

The strict structural standalone M core certified at **92 unit ticks** (141,543 timing nodes, 2,289 captures) with binary memory responses changing at the falling edge, fixed RST=0 and ENABLE=1. This certificate covers the exposed core boundary, not a cache/device machine. Added a registered timing test that allows input transitions one tick after falling edges and checks all three timing profiles. Updated functional core tests to a fixed 100-tick unit clock with that input phase, rather than the earlier generously spaced manual drive cadence; the updated run is pending.

The first recorded-scenario panel also passed all four old cache scenarios and catalog inventory. Its sole failure was the removed accidental CTest name, from a configuration generated before that fix. Native/WASM rendering has not yet been checked.

Host available memory fell from ~1.8 GB to ~337 MB, then ~232 MB with swap full. Interrupted only this worktree's build at the runner-link step; no kernel OOM was observed and no other work was stopped. The library was already rebuilt; runner/timing/probe linking and further checks wait for adequate memory. Keep serial execution and per-process caps. Public references and frontend metadata are being prepared without launching a compiler.

### Validation wiring corrections

The new timing test passed all six structural certificates across unit/embedded/desktop (91.22 seconds). The broad `RV32IM.*Test` regex also selected old RV32I Memory/Middle checks, whose names share that prefix. Those unrelated checks failed because their optional experiment executable/Python binding had deliberately not been built. This is a selection/build-scope error, not a functional device verdict. Added a dedicated `phase3` CTest label for exact future selection.

Review of registry-coverage ordering found both new names placed too early in CMake and an existing baseline mismatch: RV32ISharedMemoryProgramTest was listed before the component tests in CMake but appended after program families in the registry. Moved that CMake test registration to its existing registry position, then appended the new check/scenario names in hook order. Names and scenario behavior are unchanged. Final registry coverage will verify this correction after reconfiguration.

### Certified standalone periods

These are strict structural boundary certificates with inputs changing one tick after the falling edge (binary), RST=0, ENABLE=1, ideal 50% clock and rising-edge output sampling. They replace the earlier zero-delay input premise for the supported test boundary. They are simulated timing bounds, not physical Fmax or host FPS.

| Profile | MulDiv unit ticks | Standalone M core ticks |
| --- | ---: | ---: |
| unit | 64 | 94 |
| embedded (ps) | 2,322 | 3,900 |
| desktop (ps) | 1,620 | 2,716 |

The unit/core graphs have 33,134/141,543 nodes and 249/2,289 captures; 495/4,175 setup and hold relationships are checked in each respective graph. The unit clock used by the final functional core panel is 100 ticks. An integrated RAM/cache/device machine needs its own certificate; the numbers above do not replace existing I-system clock tables.

### Native/WASM construction-order review

Before packaging, found nested new gate-builder calls whose argument evaluation order could differ between GCC and clang, recreating the topology-order issue fixed in the baseline memories. Changed the private new builder's multi-input gate/mux/arithmetic methods to take one aggregate of arguments. Braced initialization sequences their nested construction left to right; the old multi-argument overload is absent, preventing accidental use. This changes only unmerged new component internals, not original I wiring. Functional/timing checks must be rerun after rebuilding, and native/WASM parity must verify the result.

Counter divisor=1 omits the divider register while other divisors include it. Added only the new ClockedCounter type prefix to the existing structure-keyed layout mechanism in native, Python and JS, so parameter variants cannot share an incompatible floorplan. Materialized the repository-pinned pybind11 source from local Git objects in the isolated worktree without creating a shared submodule checkout or accessing the network. Python deployment remains disabled.

### Fixed-clock panel and added edge coverage

All 46 new checks passed individually in the 54-test broad selection, including fixed-100-tick M core and C execution with zero setup violations. The other five old memory building-block checks selected by that regex passed; the three build-scope errors described above remain separate. These results precede the deterministic-construction revision and the additional cases below.

Added directed full-width hardware operand pairs for INT_MIN/-1, zero divisors, sign boundaries and high-product extremes (not merely reference-only assertions). Added rd=rs1/rs2 and M-to-branch sequences, periodic ENABLE pauses and coordinated in-flight core reset/restart. The next timing proof permits ENABLE to change at the same declared falling-edge phase. CheckCase now requires zero setup violations after each history-off functional check. Corrected pixel-selector catalog observation to combinational, and advertised the new decode/core observer capabilities with the existing capability IDs. The new pipeline families retain a known-binary operational domain, consistent with their reused I-stage abstractions.

The native Python binding's existing ModulesBinding translation unit exceeded both 768 MiB and 1 GiB address-space caps even with macro tracking disabled at the latter. A 1.5 GiB attempt is queued behind a 2.3 GiB available-memory threshold; no other compiler runs concurrently. The bounded guard stops only its own process group if available memory drops below 320 MiB. Deployment to either viewer directory is disabled.

### Viewer integration and bounded build progress

ModulesBinding compiled successfully at a 1.25 GiB address-space cap with aggressive GCC garbage collection and macro tracking disabled. The 1.5 GiB attempt never launched (insufficient headroom); it was canceled before trying that smaller cap. The later module link hit the 768 MiB cap; retrying with GNU ld's lower-memory mode is planned. These are controlled build-limit failures, not passing viewer checks.

New recorded examples have frontend aliases, generated-from-catalog pin tables and simulator embeds. M core/coordinator styles reuse the existing pipeline proportions, and the floorplanner accepts only the new M cancellation helper nodes as additional control-row parts. Existing I layouts/aliases are retained. Native storage now keeps framebuffer history by default for timeline inspection; headless callers can disable it with the existing view API. Explicit rendering/capture adapters are still phase 4. Added all 256 RGB332 values across all four byte positions and invalid geometry checks. Added explicit test-kind classification for only the new contracts.

### Slow-producer regression caught and corrected

A direct execute-stage regression failed: a valid M packet with a matching older EX/MEM producer, RESULT_READY=0 and stale register-file data started the arithmetic slot prematurely. The complete core's existing load-use/memory coordination still passed the stronger 17-cycle delayed-memory program, but the stage contract must handle this state itself. Added a structural pending-operand guard using the existing valid/write/rd/ready/address fields; EXECUTION_WAIT stays asserted while the slot waits, and launch captures the completed forwarded value. No public pin or old forwarding rule changes. Also suppress arithmetic launch for illegal/terminal/fetch-fault packets. Added completion-value and faulted-packet cases. The deliberately failing regression is retained as evidence; the corrected build/rerun is pending.

### Corrected core and observer checks

The corrected execute-stage regression, full M core, compiled C, catalog inventory, registry coverage and all three timing profiles passed (six checks, 102.96 seconds). This includes 17-cycle data-response waits, periodic ENABLE pauses and reset while the arithmetic slot actually reports BUSY. Standalone periods remain 64/94 unit ticks, 2322/3900 ps embedded and 1620/2716 ps desktop for unit/core. With ENABLE allowed to change after falling edges, the current core graph has 141,740 nodes and 4,558 setup/hold relationships; the unit graph remains unchanged. Old figures above describe the earlier fixed-ENABLE graph.

The Python module linked at a 1 GiB cap using GNU ld's lower-memory mode; deployment stayed disabled. Removed generic MemoryStateView inheritance from the new display control bank: PIXELS owns the pixel-memory protocol/view, so inspection cannot mistake control-register addresses for drawing-memory accesses. Delegated root image helper methods remain. Native pixel history is enabled by default, with the existing disable API for headless callers. This changes only the unmerged new display class.

Added a bounded all-eight-M/load-store program for a fully structural versus compact-child profile comparison, requiring identical sampled memory protocol/commit timing and oracle-checked state in both. This is the phase-2 fidelity agreement path: no new whole-core behavioral replacement is introduced. Whole-component compact forms remain conditional optimizations after actual integrated performance evidence.

### Recorded viewer validation preparation

Corrected the maintained timing boundary to allow ENABLE changes after falling edges, matching the completed certificate. Added explicit registered web checks for all 21 new recorded examples: native/WASM topology and history parity, plus component-page/mobile/recorded-checkpoint browser coverage. These checks are prepared, not yet passing. The first layout-fill command remained deferred below its 1.6 GiB headroom threshold and was canceled before launch. A retry uses a 1 GiB address-space cap and 1.4 GiB starting headroom; it adds missing entries only. A partial comparison shows no changes to existing layout values.

### Address-space and storage clarification

The owner asked whether all information resides in one memory component. Verified the router's separate RAM/console/timer/buttons/display/video ports and the separate counter/FIFO/register/pixel-storage structures. Clarified in the maintained references that one address space does not imply one backing array: global video base 0x20000000 maps to local offset zero in a 4 KiB endpoint; unmapped gaps allocate no storage. The complete device system remains phase 4.

Layout fill completed for all 21 new aliases. A recursive comparison against the HEAD-matching saved file found zero changed existing values and 48 additions (26 type entries, one profile and 21 scenario roots). Retained original JSON text around the additions to preserve existing formatting and ordering; Git's minimal/histogram diff reports additions only. The exact-label 46-check phase-3 rerun is in progress, serial with a 768 MiB address-space cap.

### Final exact-label native phase-3 panel

All 46 tests selected by `ctest -L '^phase3$' --timeout 1200 -j1` passed in 716.91 seconds at the 768 MiB address-space cap. This supersedes earlier pre-revision panel results. It includes all 21 recorded scenarios, the strengthened arithmetic/device cases, compiled C, pending-operand regression and all three structural timing certificates. The core check now also passes its fully structural versus compact-child all-eight-M/load-store program with identical sampled request/response/commit timing, independent oracle comparisons and zero setup violations. This proves the selected child profiles agree; it does not introduce a new whole-component behavioral M core. Compatibility and viewer/site/WASM checks remain separate.

### Fresh remaining native delay tables

Both Embedded and Desktop generated delay-table translation units now compile fresh, sequentially at a 1 GiB address-space cap with GCC garbage collection parameters and macro-expansion tracking disabled. The initial 1.4 GiB-headroom command remained deferred and was canceled; the successful retry required 1.32 GiB at launch and stopped only its own process group below 384 MiB. No low-memory stop occurred. Object hashes are retained in `build-rv32im/fresh-delay-tables.json`. Together with the earlier fresh Unit table this removes the copied-delay-object limitation after relinking; the relink and following regression checks are in progress.

### Foundation compatibility baseline before time-width changes

The freshly relinked foundation binary passed all 23 selected compatibility checks in 1208.81 seconds, serial and memory capped: full adder/Adder32/ALU32/old decoder, registers/state bit, transaction/response ports, arbiter, FF/scalable/native memory contracts, four cache scenarios, all three original I cores, catalog inventory and registry coverage. The earlier baseline-pipeline 120-second validation gap is resolved: its check passed in 167.80 seconds under the larger bounded limit. MemoryEndpointContractTest took 770.86 seconds at this deliberately low native optimization. The full suite is not claimed. Runner/probe hashes are saved before time-width edits; this panel validates that preceding binary, not the in-progress migration. Native viewer checks follow separately.

### Native viewer baseline panel

Seven of eight selected checks passed against the preceding foundation module/probe: scenario catalog, RV32I memory layout, cache layout, module bindings, portable session contract, pin identity and automatic layouts. MemoryInspectorIntegrationTest hit its configured 180-second timeout (CTest's command-line default did not override that per-test property). Total panel time was 900.73 seconds. This is a bounded validation gap; it is not a passing integration check or a demonstrated functional regression. Retry on the optimized build or directly with a larger explicit bound. Node/V8 cannot run under the small virtual-address cap, so this serial panel used its own-process-group memory guardian, 256 MiB Node heap limit and a 512 MiB host-availability stop threshold. No memory stop occurred. A separate selected-new-scenario static-layout check follows.
