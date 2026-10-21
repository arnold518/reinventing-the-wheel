# RV32IM milestone wrap and branch integration

Started: 2026-10-22. Status: open.
Code state: checkpoint `8c90c1f`, integrating `alpha` at `0d498c7`; the primary `realistic-timing` checkout remains at `0d498c7`.

## Summary

The owner requested wrapping the current worktree and merging it back to
the `alpha` branch. The owner merged the newer visualizer work into `alpha`,
which now matches the primary committed head. Preserve the primary
checkout's pending planning files and four newer visualizer/documentation
commits. Phase 6 acceptance remains open; phase 7 has not started. This wrap
must preserve those boundaries rather than label interactive play complete.

## 2026-10-22 — Integration inventory and scope

- Checkpoint the implemented RV32IM/device/scripted-game platform, 64-bit
  time, configurable queues/displays and observational graphics/keyboard UI.
- Current native/Python/WASM proofs and strict focused site builds are in
  the linked [parameter record](2026-10-21-device-parameters-and-visuals.md)
  and [milestone record](2026-10-21-scripted-games-phase4-6-implementation.md).
- The preceding native scope is 535/543, with seven focused follow-ups
  resolved and the large SRAM case resource limited. The portable refresh
  passes 54 checks, 11 system checks, all 22 device/native scenario comparisons
  and exact wide-time transport. All 175 old Python API entries are preserved.
- The strict site package regenerates all 22 new groups and retains 135
  previously generated legacy exports. Full current-source legacy regeneration
  stops at the 512 MiB host headroom guard, including in fresh processes.
- The first browser launch used a Python without Playwright. The documented
  system Python launches it; a missing device-system embed was found, fixed
  and rebuilt strictly. A later full browser scope stops at the memory guard
  after four M pages pass. This is not a complete browser pass.
- Add an explicit browser scenario filter for bounded display/keyboard checks;
  default full coverage and all page assertions remain. Record selected scope.
- Full revised-game repetition/fidelity/quantum/performance and compiled-game
  WASM parity remain unrun. Do not start that long campaign during this wrap.
  Phase 7 remains a subsequent milestone after acceptance.
- Primary pending files are STATUS, CHANGELOG, the future record, two device/
  playability records and original playability tools. Capture exact backups
  and inspect their relationship to this topic before integration. Do not
  reset, stash away or overwrite ongoing work as a shortcut.

## Merge procedure

Prepare the feature checkpoint, integrate with the chosen target in an
isolated checkout, resolve overlaps while retaining both changes, run serial
checks that fit the current RAM limit, then advance the requested branches
only after their current state is verified. No push or deployment is implied.

### Target confirmed and primary snapshot

The owner selected `alpha`, now at `0d498c7`, and no worktree has it checked
out. Integrate its four newer commits into the feature checkout, then advance
`alpha` only after checks and a fresh expected-head check. Leave the primary
`realistic-timing` checkout and its dirty files untouched. Exact copies and
SHA-256 receipts for all primary pending planning files and eight playability
tools are retained in the isolated build. The eight tools are copied unchanged
into this milestone so earlier audit references remain usable; the original
2026-10-19 future-plan sections are preserved before the later discussion.

### Merge resolutions

The checkpoint is `8c90c1f`. `alpha` is merged into the isolated checkout
for verification, retaining its modern toolbar/styles and content-area
geometry. Navigation includes §4.7 Memory Systems, §4.8 results, RV32IM and
device references. Registry and CTest append all four shared-memory
arrangements in the same order, followed by the new checks. Both visualizer
observation paths retain the shared-memory panel behavior and device views.

The layout merge starts from `alpha`'s exact text and adds 30 type, one
profile and 21 root entries. All prior `alpha` values and all 52 additions
are checked for equality; the original placements are not regenerated.
Scenario groups and aliases include both the new arrangements and devices.
The integrated serial native build is in progress under AS 768 MiB and the
512 MiB host-headroom guard. No branch target has advanced yet.

### Integrated native build and bounded regression progress

The full serial integrated native build passes in 341.56 s, peak 604480 KiB
RSS, under AS 768 MiB. The first local selection used a non-capturing regex
unsupported by CTest and selected no tests; that is a harness error, not a
passing run. The corrected launcher rejects empty selection.

The first focused run passes eight checks, including the full old memory
migration (207.51 s), then the host-headroom guard stops the inspector
integration check. The next run passes device documentation, then stops at
the legacy cache-layout allocation. Both are resource stops, not assertion
failures or host OOM. Remaining checks now run one at a time with individual
logs/results, retaining those resource limits explicitly.

The inspector parity harness now releases the Python circuit after capturing
plain JSON expected states, before constructing its native counterpart; all
assertions are retained. Its full rerun still reaches the host-memory guard.
Further isolated methods may establish a smaller verified scope. No completed
full inspector or 81-check pass is claimed at this point.

### Integrated native/Python/Node scope completed

The selected native/Python scope accounts for 81 distinct checks: 80 pass and
the legacy CacheLayoutTest remains resource limited. The complete static
layout sweep, exact registry order, RV32I memory migration, all four shared
arrangements, RV32IM core/compiled C, device parameters/observations, system
profiles, wide epochs, lockstep, larger displays, input/capture/fault cases
and binding/contract checks pass. No assertion failure is reported.

MemoryInspectorIntegrationTest now uses a compact CPU with structural
arbiter/cache circuits in both adapters, and releases its captured Python
circuit before native comparison. It passes in 38.61 s, peak 175008 KiB RSS.
This replaces the resource-limited full-core observation attempt; separate
registered arrangement scenarios pass with their default structural cores.
All assertions and memory-role/history comparisons remain.

All 13 Node viewer/generator checks pass (3.87 s, peak 98696 KiB RSS), including
old viewer helpers and the new device/isolated-example checks. The refreshed
API comparison preserves all 175 old names/signatures/docs and exactly five
additions. All original dated sections in the three primary planning/audit
records are present unchanged; their primary files and eight original tools
still match the saved hashes. The integrated WASM rebuild is in progress.

### Integrated WASM and bounded parity refresh

All seven portable targets rebuild successfully in 132.33 s, peak
316628 KiB RSS, using the existing serial compiler cap and link level 1.
Four current portable checks pass in 213.26 s (peak 319288 KiB RSS): the M
core, short CPU/device system, larger screens and exact input validation.
The complete native/WASM timing snapshot remains byte-identical: 2171241
bytes, SHA-256 b0c61d9e7aa62175cfa56f8654b5a95d89e1ac551e88a358dc767a7825ead98f,
all 808 entries in each profile.

The current old memory-inspector WASM/native parity check passes. The full
22-scenario device topology run passes the coordinator then reaches the
host-memory guard while loading the M-core topology; it is resource limited.
An optional device-scenario alias filter now bounds this check while still
requiring all 22 source example markers; the default retains full coverage.
Five selected device scenarios (framebuffer, buttons, console, timer and FIFO)
pass exact topology/examples/state/observer/rewind parity in 12.13 s. The
current exact-time web check passes in 2.27 s. The earlier complete 22-scenario
result remains dated pre-integration evidence, not a full current-tree pass.

The final strict site refresh selects those five device example groups and
retains the remaining 152 exports from the preceding packaged bundle. This
avoids repeating the known large-topology allocation during the wrap; full
current-source example regeneration remains an acceptance gap.

### Strict package and final browser limit

The integrated strict Pages package passes, refreshing five device example
groups and retaining 152 preceding exports. Peak RSS is 379752 KiB. The
package contains the current compiled WASM, modern toolbar, device graphics/
keyboard code and recording viewer. It is local evidence, not a deployment.

Both the normal Chromium attempt and a temporary single-process/software
rendering attempt stop at the 512 MiB host-headroom guard during the device
browser check. The recording-viewer check is not reached, and no current
browser appearance/pixel pass is claimed. Temporary launch flags are removed
from the tracked tests; do not ship an unverified additional browser mode.
No further memory-heavy campaign is started during the wrap.

Ready to complete the integration merge and merge the tested checkpoint into
`alpha` through `.worktrees/alpha-integration`. The primary checkout remains
on `realistic-timing`, with its pending files preserved. Outstanding phase-6
gates include the resource-limited cache/SRAM checks, full current topology/
example/browser scope, revised ten-game fidelity/quantum/repetition/performance
and compiled-game native/WASM comparisons. Phase 7 is unimplemented.
