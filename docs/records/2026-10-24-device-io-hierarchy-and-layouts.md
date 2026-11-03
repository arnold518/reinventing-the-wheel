# Device I/O hierarchy, scenario order and layouts

Started: 2026-10-24. Status: closed. Code state: `alpha` at `5549ee0`, with
the completed structural-constant cleanup still uncommitted.

## Summary

All twelve Device I/O scenarios now expose functional structural groups and
refreshed layouts. The integrated system has an explicit CPU/routing/RAM/
peripheral floorplan. Menu groups start with RV32IM, its programs, then
Device I/O; each group has an explicit intuitive order. Focused native,
timing, migration, static layout, native/WASM and browser checks pass. The
strict site and local 8893 preview are updated and verified. Existing
constant cleanup is preserved; full suites and the game campaign were not run.

## 2026-10-24 — Investigation and scope

The device scenarios select structural device architecture with compact
lower-level storage/word parts. Several peripherals and routing blocks still
place hundreds or thousands of loose gates directly under their roots.
Device observers and the recorded guest inspect named `PORT`, `PIXELS`,
`LEVELS`, `EVENTS`, `TX_FIFO` and `COUNTER` children; preserve those useful
functional boundaries or explicitly migrate every caller. The system's CPU,
routing, RAM and peripherals already have meaningful names and need a
deliberate system floorplan rather than arbitrary automatic placement.

Use private wiring-only groups and existing lower-level components, retaining
public contracts, register maps and structural/compact equivalence. Inspect
every Device I/O menu scenario, plus relevant parameter variants and shared
callers. Run focused native/timing and complete selected Device I/O layout/
portable/browser checks; broad game/performance campaigns remain outside
this visualization change. Baseline topology and layouts are retained under
`build-milestone/device-io-hierarchy/`.

## 2026-10-24 — Structural grouping and first checks

All twelve Device I/O menu scenarios were inspected. The existing system has
nine meaningful children; its issue is the overview geometry, while the
individual devices expose large flat gate lists. Private `DeviceGroup`
assemblies now separate decoding, captured requests, control, data selection,
storage and command handling. Register-address matching exposes byte slices;
button input exposes 32 small update cells. Existing word muxes are selectable
children. No whole-device behavioral replacement or public contract change
was introduced. The observer/runner's named `PORT`, `PIXELS`, `LEVELS`,
`EVENTS`, `TX_FIFO` and `COUNTER` boundaries remain available.

| Scenario | Direct children before → after |
| --- | ---: |
| Counter | 16 → 3 |
| FIFO | 308 → 3 |
| Register port | 333 → 4 |
| Error target | 16 → 2 |
| Address router | 1,589 → 10 |
| Fetch guard | 355 → 5 |
| Console | 686 → 5 |
| Timer | 730 → 5 |
| Buttons | 1,893 → 8 |
| Pixel selector | 163 → 2 |
| Framebuffer | 1,402 → 5 |
| Integrated system | 9 → 9; deliberate overview floorplan |

The initial 13 component/width registrations passed in 54.36 seconds; the
twelve recorded scenarios, observer and system timing registration passed in
57.33 seconds. Seven affected default tables were regenerated in all three
profiles. Five unit entries changed (router, guard, register port and both
FIFOs); realistic entries are identical. A later comparison after byte-slice
grouping found no further table differences. The table inventories retain
809 entries per profile. A review caught potentially compiler-dependent
argument evaluation in the new mux helpers; their final API takes a braced
choice, like the existing structural logic helper. Final rebuild and portable
acceptance are pending.

## 2026-10-24 — Menu, layout and compatibility review

The first three menu groups are RV32IM, its numbered programs, and Device I/O.
Explicit ordering puts steps/units/stages/core and programs 23–27 in order;
Device I/O progresses from state helpers through bus/peripherals/graphics to
the whole system. Native and JS presentation parity passes. The first new
ordering test accidentally included registry-only visual flags excluded by
the frontend; it now checks the actual frontend options and passes.

Recalculated 114 affected/new layout scopes from the captured starting
layouts, preserving unrelated geometry and migration history. Validation
caught and corrected a button-array adapter at the edge, missing layouts for
the newly used structural Mux<2,8>, and an arbiter sizing mismatch. The latter
is a fixed explicit child, so profile-only proportions do not apply: the
system floorplan fits its actual shape and preserves its shared styling.
All twelve device scenarios plus MulDiv, IM core, Fibonacci and I core pass
the static check (16 scenarios, zero problems). Saved fidelity paths for
moved named registers migrate in native and WASM; explicit current choices
win. Existing automatic-layout migration checks pass. The first layout-tool
CLI loaded the old deployed backend from its script directory; subsequent
checks preload/use the isolated backend, leaving deployed modules untouched.

Public component references now describe the functional hierarchy. The
existing device browser check additionally inspects distinct nested device
scopes, exact root group names, menu order and actual geometry for all twelve
sessions. Final native/WASM builds, example regeneration, browser inspection
and the 8893 preview refresh remain pending.

## 2026-10-24 — Final acceptance and preview

This section replaces the pending-validation statements above. The final
serial native build uses `CIRCUITSIM_DEPLOY_PYTHON=OFF`; the live native
visualizer modules were not overwritten. All 27 selected component, width,
recording, observer and system-timing CTests pass in 131.15 seconds. All 21
compiled default timing-table comparisons match the regenerated results.
Only the five unit entries described above changed; inventories remain at
809 entries per profile. The compact system configurations certify at 520
ticks against the fixture's 1,000-tick period. The timing test still reports
the preexisting six missing all-state contracts for expanded-core analysis;
this run supplies no expanded-core certificate.

The native/JS catalog checks pass (two tests), including exact order inside
RV32IM and its programs. Saved-register path migration agrees across both
backends, is idempotent, and retains explicit current choices. The style
migration check verifies that unchanged defaults upgrade while custom
colors, proportions and manual layouts survive. Existing automatic-layout
migration covers 792 entries. Final static checks report zero layout
problems across all 16 selected scenarios. The final saved-layout changes
include 70 type scopes, 35 root scopes and device profile overlays; the
earlier 114-scope figure described an intermediate generator pass. Shared
RV32IM-core and two-port-arbiter type layouts match the captured baseline.

The isolated serial WASM/package build regenerates all twelve selected
example groups and passes strict MkDocs in 96.63 seconds. The registered
native/WASM comparison passes all twelve selected device scenarios, checking
topology, examples, recorded state, observers and rewind. The browser check
passes in 454.10 seconds: all 22 referenced public pages, twelve device
sessions and the neighboring RV32I-core session. It inspects 180 distinct
nested device scopes across the device sessions, plus fidelity/device-view
controls and geometry.

A final integrated-overview refinement followed that browser run: the root
aspect is 1.10, RAM has enough width for its caption, and the fixed arbiter
retains its existing actual proportions. A focused final check of all nine
system children passes bounds, overlap and minimum-caption-size assertions
against both the isolated site and the live 8893 preview. The final RAM
caption is 6.09 pixels at the checked 1440-by-1000 viewport. The final
overview screenshot was inspected. The complete twelve-session browser
selection was not repeated after this system-only geometry adjustment.

Port 8893 serves the refreshed simulator package after its strict preview
build (96.24 seconds). Fourteen documentation/simulator routes respond and
all 26 non-HTML assets match the staged hashes. The served menu starts with
RV32IM, RV32IM programs and Device I/O. The previous simulator package is
retained at
`build-wasm/device-io-hierarchy-refresh/20261023T175503Z/simulator-before`.
Live overview assertions pass with the final package. No remote deployment,
push, full native/web suite, benchmark or gameplay campaign was performed.

Evidence is under `build-milestone/device-io-hierarchy/`: final native and
timing logs, `layout-all-final.log`, `migration-styles-final.log`,
`wasm-parity.log`, `web/results.json`, `system-overview-final.png`,
`system-overview-live.png`, `live-overview.log`, and
`preview-refresh.json`. The key results above are retained here rather than
depending on those ignored artifacts.
