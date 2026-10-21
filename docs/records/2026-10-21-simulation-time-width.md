# Simulation time width prerequisite

Started: 2026-10-21. Status: open.
Code state: isolated `rv32im-phase3` worktree from `60acda2`, with phase 0–3 additions.

## Summary

Independent prerequisite for [scripted platform/games phases 4–6](2026-10-21-scripted-games-phase4-6-implementation.md). Explicit unsigned 64-bit ticks, checked scheduling, widened histories and exact large-tick JSON are implemented. Standalone wide-time/publication/history/overflow and recorded-rewind checks pass in native and intermediate WASM builds. Short device-system guests above 2^53 and 2^63 pass in WASM. Exact Python/native/WASM transport, final artifact refresh and old-behavior regressions remain pending. Existing identities, pin widths, event ordering and short numeric playback representations are preserved by design; full compatibility acceptance is still open.

## 2026-10-21 — Audit and proposed compatibility boundary

- `Event::time`, `Simulator::current_time`, drain deadlines/results, signal histories and unique timestamps use size_t. MemoryStorage history entries and BasicComponent publication/setup bookkeeping do too. Sizes, addresses, indexes, event counts and widths must remain distinct.
- BasicComponent delay twins already use signed 64-bit internal timestamps with -1 sentinels. Avoid silently casting unsigned wide time into that domain. A conservative-table fallback outside the twin's representable range is a possible narrow solution; validate its eligibility and publication ordering explicitly. Do not rewrite the logic/timing engine casually.
- Introduce `SimulationTime` as an unsigned 64-bit tick type, independent of pointer width. Guard additions/multiplications and external conversions against overflow; retain 32-bit guest timer counters and existing pin widths.
- Existing safe-range viewer JSON timestamps retain their numeric representation. Large values need exact decimal ticks and frontend comparisons that preserve ordering; indexes remain ordinary numbers. Specify and test the mixed/exact representation before claiming long-recording compatibility.
- Audit runtime event scheduling, gate delays, memory device timing, delay twins/publication, histories, native/Python/WASM transport, scenario/test timing helpers and observation paths. Analysis durations must not accidentally narrow time or certify wrapped arithmetic.
- Validate both native and wasm32 execution above 2^32, exact transport above 2^53, overflow rejection and signal/memory rewind. Run old playback and timing regressions independently. Live input scheduling/frontier APIs remain phase 7.

No time-width code has been changed yet. The preceding phase-3 compatibility panel is running against the freshly relinked foundation binary.

### Migration started (unverified)

This supersedes the initial no-code statement above. Added SimulationTime/checked add/multiply, converted unambiguous event/evaluate/history/scenario time declarations, and separated count/index fields found in mixed declarations. Core simulator and memory histories now use the explicit type. BasicComponent setup bookkeeping uses optional timestamps instead of a time+1 sentinel; publication additions are checked. The signed delay-twin refinement is guarded by its representable current-time/contract-delay range, with conservative table timing outside it. Clock/profile and pending-memory-output timing conversions are in progress. No changed time-width binary has been compiled or tested yet. The concurrent compatibility panel uses the saved preceding foundation binary and does not validate these source edits.

### wasm32 compile audit prioritized

The serial native build reached the new device-system assembly; a heterogeneous shared-pointer initializer-list error there was corrected. Before finishing/repeating all native objects, stopped only that build and started a fresh Emscripten build with 64-to-32 conversion diagnostics. Native 64-bit size_t would hide these misses. The first wasm32 error exposed `TimedLogicDomain::ffStatusBounds` restriction/settling timestamps still declared size_t; they are timestamps (not bit masks), and were corrected to SimulationTime. This corrects the earlier working assumption about those locals. Further warnings in SRAM timing helpers, analysis experiment plumbing and table extrapolation remain to audit. Neither the current native nor WASM migrated binary has passed execution yet.

Memory pulse widths, DRAM row/cell retention, delay-line/ROM/SRAM publication, memory timer settle fields and transparent utility output scheduling are included in the type/arithmetic audit. Signed relative timing-analysis contracts explicitly reject values that collide with their OPEN sentinel; absolute simulator time retains the full unsigned range. Existing widths/addresses/counts remain distinct.

### Extended pending execution checks

Added real memory delay-line and power-on-reset cases carrying a delay above 2^32 at an absolute time above 2^53, alongside scalar/twin/history/overflow checks. The new device batch accepts an explicit start epoch, so its short FF guest can exercise real CPU/router/device execution above 2^53 and 2^63 without billions of clock edges. Its default start remains zero. These checks are registered but not run yet.

The wasm32 compile audit also found scheduled wakeup fields, clocked SRAM withdrawal/fallback bounds and test history delimiters still using size_t; correct them before current-source validation. Count parsers remain counts and reject unrepresentable values rather than silently wrapping. The existing prediction entry limit remains 1..256 powers of two; its parser now checks the limit before narrowing.

The compile audit also identified latest-register queries still passing size_t's maximum as a time sentinel. Changed those queries to MAX_SIMULATION_TIME; counts/index sentinels remain unchanged. Added register-file history checks above 2^53/2^63, corrected the FF-memory overflow regression to test the time maximum, and widened scenario row-spacing helpers and expected timestamp vectors. These are migration corrections; existing short scenario data/ordering is retained. Execution remains pending until the fresh build links.

### First wasm32 execution pass

The first linked portable verifier passes SimulationTimeTest and SimulationTimePlaybackTest. Coverage includes absolute times above 2^32/2^53/2^63, scalar publication, memory/register history, wide memory delay/reset pulses, signed-twin/table range handling, arithmetic/scheduling overflow and recorded rewind. Logs: `/tmp/circuitsim-time-wasm-execution.log` and `/tmp/circuitsim-time-playback-wasm.log`. The verifier is an intermediate build; native execution, exact Python/native/WASM JSON parity, old registered regressions and final source/artifact refresh remain required. No whole time-migration completion is claimed from these two passes.

### Fresh native execution

SimulationTimeTest and SimulationTimePlaybackTest pass in the fresh native
verification build (0.03 s and 0.02 s). This refresh includes the explicit
tick fields and checked arithmetic. Log
`/tmp/circuitsim-native-foundation-refresh.log`; its independent optional
expanded-system clock check failed, so the containing panel is 8/9 rather
than an overall pass. Binding/old-playback verification remains pending.
