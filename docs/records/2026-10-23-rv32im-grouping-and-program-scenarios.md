# RV32IM structural grouping and recorded program scenarios

Started: 2026-10-23. Status: closed. Code state: `alpha` based on `7aeff2d`,
with this structural/scenario change.

## Summary

The owner requested meaningful subcomponents in the crowded arithmetic and
execute views, stronger recorded component cases and dedicated RV32IM program
scenarios in the existing visualizer. Validation must remain minimal.
The groups, expanded recordings and three dedicated program scenarios are
implemented and available on port 8893. Focused native, timing, migration,
layout, WASM example and browser checks pass. No full suite or game campaign
was run. This follows the closed
[scenario-view fixes](2026-10-23-rv32im-scenario-visualization-fixes.md).

## 2026-10-23 — Source review and scope

The existing RV32I execute stage uses wiring-only IOComponent groups for
forwarded operands and packet status. These retain structural gates and
selectable child fidelity. RV32IM's iterative arithmetic and extension stages
instead place ownership, operand preparation, bitwise result muxes and sign
correction in large flat scopes. The existing recorded arithmetic scenarios
show only one operation each; execute shows one multiply, and the standalone
core shows fetch backpressure without executing a program.

Reuse the existing structural-group approach, preserving the arithmetic
contract and gate logic. Add directed recorded edge cases and separate
registered program fixtures with oracle-checked commits and inspectable
history. The owner clarified that program scenarios must be visible in the
scenario visualizer, rather than additional headless-only program sweeps.
Run a bounded selection covering the changed arithmetic/stage scenarios and
new programs, plus necessary layout/package checks; do not run broad suites.

## 2026-10-23 — Implementation and focused native results

Multiplier and divider now contain CONTROL, OPERANDS, DATAPATH and RESULT
groups. Named word muxes replace flat per-bit selectors at the visible
boundaries. MulDiv has a RESULT_SELECTION group; execute has INTEGER,
M_OPERANDS, M_CONTROL, MULDIV and RESULT_SELECTION plus its three adapters.
Decoder overrides and coordinator hold/movement gates also have their own
structural groups. These are private wiring-only IOComponents following the
existing I-core pattern, with no new behavioral contract or instruction
model. Existing named register/step fidelity paths migrate to their new groups.

All ten strengthened component recordings and the three dedicated programs
pass their first focused native run (24.04 seconds for the 13 scenarios).
The recordings now include all eight M operations, signed/unsigned boundary
cases, nonzero remainders, divide by zero, signed overflow, pause/held result/
take, cancellation while paused and in-flight reset. Decode covers all M
encodings and illegal fallbacks; execute includes a pending load, later
forwarding, fault suppression and signed divide. The standalone core example
isolates fetch retention/disable/reset; complete execution is in the programs.

The three menu scenarios are `rv32im-program-arithmetic`,
`rv32im-program-dependencies` and `rv32im-program-divide-edges`. They reuse the
I program fixture's real instruction/data endpoints with an IM core and retain
history at reset, first arithmetic wait, every commit and halt. Every retired
PC/instruction, architectural register/PC/terminal state and final memory is
oracle-checked, alongside directed register/store expectations. The programs
cover all eight operations and x0; a store/load/M dependency chain and taken
branch flush; and division/sign boundaries. Topology-only setup remains
separate from recording, with editable component fidelity. Native program
checks pass after the final checkpoint-format changes.

The first registry check exposed a pre-existing ALU scenario-order mismatch
between CTest and the registry. Reordered those declarations without changing
their execution or weakening coverage. A four-check retry (new grouped
decoder/coordinator, dependency program and registry) passes in 4.18 seconds.
An initial build failure from the new fixture's incorrect BuildManifest
constructor was corrected before these passes.

The focused timing comparison caught a changed MulDiv table after introducing
the named muxes. Regenerated only that contract with `--clocked` in all three
profiles and replaced its generated entry, preserving all other entries.
Only the unit table changes: maximum result launch 48→51, A/B/OP setup
30/28/16→32/29/17 and internal period 39→40. After rebuild, both selected
MulDiv/core stored tables recompute with zero differences. The focused timing
registration certifies unit/embedded/desktop periods of 68/2322/1620 for
MulDiv and 94/3900/2716 for the core; the public MulDiv boundary is updated.
The native fixtures report zero setup violations at their 200-tick unit clock.

Static layout checks pass with zero problems across the ten M component
scenarios, three programs and integrated device-system view. New group scopes
and changed roots have complete placements. Native/WASM path migration and
custom-layout retention checks pass. The serial WASM/site rebuild is scoped
to the 11 affected component/system example groups and three new programs;
unrelated previously verified examples are retained. Focused browser
inspection and the preview refresh are pending.

## 2026-10-23 — Current WASM package and layout review

The serial WASM build and 14 focused example groups pass. They regenerate
11 affected component/system examples and the three new recorded programs;
unrelated verified exports are retained. The recorded row counts are now
7/7 for multiply/divide steps, 29/29 for multiplier/divider, 49 for MulDiv,
13/15 for decode control/stage, 17 for execute, 8 for coordinator and 7 for
the isolated core interface. The three programs have 14, 14 and 15 checked
timeline checkpoints respectively, including reset/wait and every commit.
The strict combined documentation build passes in 93.94 seconds.

A geometry review restored four unrelated I-pipeline type/profile scopes
that the initial generator unnecessarily treated as changed due to obsolete
extra saved keys. They are semantically identical to the starting revision;
only M arithmetic/extension layouts, new groups and program roots change.
Migration snapshots retain styles and placements for children that still
exist. Removed old flat children need no duplicate geometry snapshots.
The final 14-scenario static check and native/JS path/layout migration check
pass again. Current layout/catalog/worker/migration bytes match the packaged
assets. Native and WASM table catalogs remain at 809 entries per profile.

Current inspection topology has four root children each for multiplier and
divider (359/720 total components), 23 for MulDiv (1,106), eight for execute
(8,257), and two each for decode control and coordinator. A program fixture
has four main children and 15,557 total components. Fewer visible loose gates
come from hierarchy and existing lower-level mux abstractions; the strict
gate-level timing proof still traverses the complete arithmetic hardware.
The four-scenario browser inspection is in progress. A final documentation
refresh will package the program page's single embedded viewer and links to
the other dedicated programs.

## 2026-10-23 — Final visual review and preview acceptance

All four sessions in `RV32IMInspectionBrowserTest` pass in 69.83 seconds:
multiplier, divider, execute and the dependency program. The checks focus
their grouped scopes, reject missing placements/overlaps/out-of-bounds
children and caught explorer/page errors, and check program menu entries,
structural CPU fidelity, halt and rewind. Screenshots confirm the arithmetic
groups. Visual review found the execute stage still inherited a 2.91 aspect
ratio and large pins, making its new groups tiny. Replaced that frame with a
0.95 aspect and smaller pins, like the I stage, and an explicit arrangement
of integer execution, operands/control, MulDiv and result selection.

The two affected browser sessions (execute and its enclosing dependency
program) pass again in 42.71 seconds. The six affected parent/program/system
static layouts have zero problems. Migration snapshots reference the shipped
starting revision and retain custom choices/positions; native/JS migration
checks pass, including the old execute aspect/pins and custom MulDiv placement.
The final generated layout leaves unrelated type/profile scopes semantically
identical to the starting revision.

The final standalone MkDocs render reports completion in 96.48 seconds, but
its shell ultimately reports SIGTERM (143), so that invocation is not counted
as a successful command. The earlier combined strict build passed. The final
current preview then rebuilt in strict mode successfully in 89.13 seconds,
including the final single-viewer program page and register-inspection text.
The simulator package was copied back into the standalone site after MkDocs
cleaned its output directory.

Updated port 8893 atomically from the reviewed 28-file simulator package and
restarted `reinventing-the-wheel-pages.service`. Its prior package is retained
under `build-wasm/rv32im-grouping-refresh/20261023T135401Z/simulator-before`.
All eight checked routes respond, all 26 non-HTML simulator assets match exact
package hashes, and the program page contains three generated tables and one
embedded viewer. Fresh browser playback of the dependency program on 8893
passes grouped-scope inspection, menu checks, halt and rewind. The three new
programs have a dedicated `recorded-program` CTest label; listing that label
selects exactly those three tests.

Key evidence is retained under `build-milestone/rv32im-grouping/` in
`native.log`, `final-native-checks.log`, `clock.log`, `timing-final.log`,
`build-pages.log`, `web/results.json`, `web-floorplan/results.json`,
`layout-final-check.log`, `layout-floorplan.log`, `migration-floorplan.log`,
`preview-refresh.json`, `preview-checks.json` and `live-program.log`.
The important outcomes and limits are copied above. No complete native/web
suite, performance campaign, remaining game acceptance, push or published-site
deployment was performed.
