# RV32IM scenario names, structural views and layout fixes

Started: 2026-10-23. Status: closed.
Code state: main `alpha` based on `41dc41b`, with this scenario/view change.
The preceding audit is retained as dated evidence.

## Summary

The owner requested descriptive roots matching the other core scenarios,
structural defaults for MulDiv and the cores, and repairs for layout/triangle
errors. This follows the [closed visualization audit](2026-10-23-rv32im-scenario-visualization-audit.md).
The 22 root names, shared structural inspection profile, saved-path migration
and missing structural layout scopes are implemented. The native build,
28 focused registrations and static layout checks across 25 scenarios pass.
The serial WASM build, all 156 example groups, strict site, 22 native/WASM
comparisons and full 22-page/23-session browser selection pass. MulDiv's
readable engine floorplan passes a further focused browser check. Port 8893
serves the verified package; seven routes, 26 asset hashes and three live
structural views pass. Existing RV32I core source/defaults remain structural;
saved-profile and toggle checks include them.

## 2026-10-23 — Scope and initial evidence

Read the current instructions/status/index, component/test guides, open
foundation/game records, public component references, shared scenario helper,
RV32I core scenarios and their architecture profile, layout/profile migration
and browser layout lookup.

The original RV32I scenarios use component-specific root IDs. Their
architecture profile keeps RV32I blocks structural while compacting storage
and other lower-level details. The RV32IM helper instead uses `DUT` for all
21 M/device component demos and prefers behavioral globally. The separate
integrated-system scenario also names its root `DUT`.

Fresh native core topologies: `rv32i-core` has root
`RV32I_SINGLE_CYCLE_CORE_ROOT` and 9,546 components; `rv32i-five-stage-core`
has `RV32I_FIVE_STAGE_CORE_ROOT` and 12,697; baseline has
`RV32IBaselinePipelineCore_ROOT` and 9,234. All are structural. The prior
audit's root expansion checks covered compact-to-structural root changes,
not arbitrary nested toggles and all resulting layout scopes. The owner's
reported errors require that broader focused reproduction.

## 2026-10-23 — Error reproduction and implementation

The owner's error is reproduced in fresh 8893 browser contexts. Expanding
`muldiv-w32` builds 2,715 components but the explorer reports
`Missing layout for root_layouts.muldiv-w32@8231892f35bafe9b.children.MULTIPLY`.
Expanding the IM core builds 3,929 but reports a missing FETCH_REQUEST at its
corresponding root key. These are caught layout errors, not uncaught page
errors. This corrects the earlier audit's assessment that root expansion
worked: that check asserted counts/fidelity and page errors but omitted the
explorer error status. Nested register toggles in multiplier/divider and the
existing RV32I core succeed in the same reproduction.

Renamed all 21 shared component-demo roots and the integrated-system root
using the other cores' component-specific `_ROOT` convention. Scenario
metadata records the root names; saved native/WASM `DUT` overrides migrate
per scenario, preserving descendant choices and canonical-path precedence.
The inspection profile reuses RV32I's structural architecture policy and
keeps RV32IM/arithmetic/device/bus/video functional blocks structural, while
retaining compact storage and lower-level word choices. Existing RV32I core
source/defaults remain structural.

The serial native build passes. The first legacy layout-tool invocation
accidentally selected the old deployed Python extension because its script
directory preceded PYTHONPATH; launching with the isolated module first
corrected that. Its fill-missing mode repeatedly deep-copied large flat
layout dictionaries and was stopped during the divider after completing
three scopes. The partial file is retained. Layout completion instead uses
topology-only construction, matching complete saved child-name sets before
reusing geometry and generating only missing scope data. Validation of those
placements and the current package is still pending.

Extended the device browser check to assert descriptive structural roots,
exercise structural/behavioral round trips and nested expansion, and reject
caught explorer layout errors. It also checks the existing `rv32i-core`.
Native/WASM saved-profile migration checks cover all 22 renamed roots.

## 2026-10-23 — Native and layout verification

The shared inspection profile also drives the integrated-system component
demo, so its CPU/device functional blocks are visible. It is separate from
the batch game's throughput profile. Updated root counts include 2,715 for
MulDiv, 17,318 for the IM core and 29,571 for the integrated system. The
existing three I-core root counts and structural defaults are unchanged.

The 28 selected native registrations pass (148.04 seconds): all 22 renamed
M/device/system scenarios, the three existing core scenarios, scenario catalog,
profile persistence and native/WASM path-migration checks. All 22 root-name
migrations preserve canonical choices and descendant paths. The static layout
audit passes with zero problems across those 22 scenarios and the three I
cores. It checks completeness, structure-key collisions and parent bounds;
browser checks additionally cover visible overlaps and fidelity round trips.

Filled 15 shared structural type scopes (one newly keyed counter variant)
and added 35 canonical/current-profile root overlays. Existing unrelated
layout entries remain semantically identical; complete saved child-name sets
identify reused geometry, with the counter's missing layout generated by the
existing floorplanner. Shared type placements avoid duplicating full geometry
for every profile. The actual component blueprints and timing tables were
unchanged; no new gate-level delay characterization is required for scenario
names or fidelity selections. The serial WASM/site rebuild is in progress.

## 2026-10-23 — Current package and web acceptance

The serial WASM rebuild and all 156 isolated example groups pass, followed by
the strict combined site build (93.63 seconds). Packaged layout, scenario
metadata and worker bytes match the current sources. All 22 native/WASM
component scenarios match topology, examples and recorded state (79.82
seconds), and the frozen Pong circuit/browser playback check passes (40.45
seconds).

The first updated device browser attempt stopped before live sessions on its
old fixed count of 23 pages. The ALU refactor had removed the duplicate word-
component recording; the current M/device/system inventory has 22 pages.
Replaced that stale count with exact coverage of the 22 root-metadata entries,
updated the test guide and kept the extra RV32I core session. The browser
retry is in progress. Each structural view checks child bounds/overlaps,
root/nested fidelity round trips and caught explorer errors; MulDiv also
loads a real IndexedDB profile containing old `DUT` paths. Its renderer heap
cap is 512 MiB to cover the expanded inspection views, with one context at a
time and the existing two-renderer limit.

## 2026-10-23 — Full browser pass and MulDiv visual review

The corrected browser registration passes all 22 component pages and 23
recorded sessions, including the existing RV32I core (948.09 seconds). It
checks structural roots, child bounds/overlaps, real root/nested fidelity
round trips and old IndexedDB `DUT` overrides. There are no caught layout
errors or page errors. Screenshots confirm the IM core's visible pipeline.

Visual inspection also found that the inherited MulDiv gate-grid floorplan
made its two engines nearly invisible among control gates, despite valid
bounds. Added an intentional shared floorplan: MULTIPLY and DIVIDE each use
over 31% of the parent's width, with OPERATION and control/result gates below.
Smaller title/pin styling matches the other core views. Saved unchanged
shipped layouts upgrade through `rv32im-muldiv-v1`; custom placements/styles
and explicit manual layouts are retained. The migration regression and
25-scenario static check pass again. The component circuits, profiles and
verified examples are unchanged by this geometry-only follow-up.

The final strict site is refreshing the layout/migration assets using the
already verified 156-group bundle. A focused browser recheck of MulDiv and
the existing I core will verify the new floorplan before the 8893 package is
updated; the broader successful browser results remain valid for unchanged
components. Live deployment and final records are pending.

## 2026-10-23 — Final floorplan and live preview verification

The final strict site refresh passes in 95.46 seconds using the verified
156-group example bundle. The focused browser registration passes in
128.68 seconds: all 22 component reference pages plus MulDiv and the existing
I-core live sessions. It verifies both arithmetic engines occupy more than
one quarter of MulDiv's width, as well as the saved-profile, bounds and
fidelity-switch checks. Visual inspection confirms the multiplier and divider
are prominent above operation state and control logic. The final migration
registration and all 25 static layout scenarios also pass.

Copied the 28-file verified simulator package into the main preview's source
directory, retaining the previous package at
`build-wasm/rv32im-viewer-refresh/20261023T123423Z/simulator-before`.
Restarted `reinventing-the-wheel-pages.service`; its strict preview rebuild
completed. All seven checked HTML routes respond, with the existing iframe
live-reload guard intact. All 26 non-HTML assets match the package hashes, and
the frozen Pong circuit's served hash matches its source. The generated
MulDiv, IM-core and device-system examples identify structural fidelity.

Fresh browser contexts on port 8893 confirm the descriptive structural roots
and visible child layouts: MulDiv has 2,715 components, the IM core 17,318 and
the I core 9,546, with no explorer layout errors or page errors. Current
source/package layout bytes match. Evidence is retained under
`build-milestone/rv32im-viewer-fix/`: `web-browser-recheck/results.json`,
`web-floorplan-final/results.json`, `site-final.log`, `preview-refresh.json`
and `preview-checks.json`; the key results are copied above.

This closes the requested naming, default structural views and triangle/layout
repairs. Compact storage remains available under structural architecture
blocks, following the existing core inspection profile. Individual circuits,
timing tables and batch-game profiles were unchanged. The full native/web
suites, remaining game acceptance, a running-program IM-core example and
published-site deployment were outside this selection; no push was performed.
