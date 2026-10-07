# RV32I 4.3 layout and hierarchy review

## Scope

Review all six 4.3 references, keeping public pins, behavioral implementations,
and the lower-level-first design intact. Retain word/bit order where a generic
graph layout would destroy it. New helper blocks are fixed structural circuits,
not new behavioral implementations or simulator primitives.

| Reference | Decision |
| --- | --- |
| Register file | Keep its six-part decoder/storage/two-read-port layout and numeric FF word order |
| ALU32 and its seven units | Retain the existing functional hierarchy; following the explicit automatic-layout request, regenerate all eight 4.3.2 references and structural descendants using the standard connectivity algorithm, with no custom bit-row/compare/flag placement rules |
| Decoder | Replace the flat 541-child view with a five-block input/recognition/control/immediate floorplan; recognition and eighteen control fields remain expandable |
| Pattern matcher | Keep numeric predicate/AND ordering; no functional changes |
| Control flow / alignment | Calculate automatic connectivity layouts |
| Memory lanes | Split 210 immediate children into nine blocks; group load-extension gates into four byte circuits with eight ordered bit lanes each |
| Memory requester | Group 32-bit tag comparison into `TAG_MATCH`; 102 immediate children become 38, with payload, state and response areas distinct |
| Execution status | Wrap the seven three-gate priority links; 87 immediate children become 73, with explicit top-to-bottom priority ordering |

The recognizer's forty instruction-match lines use supported 32-bit and 8-bit
bundles. The control fields preserve the original serial OR equations and
defaults. Request tag comparison preserves the XOR/balanced-OR network; the
status helpers preserve the original three-gate equations. Public component
contracts and existing selectable storage paths remain unchanged.

## Regeneration and checks

Run with the native binding's Python, serially:

```sh
python tools/generate-rv32i-memory-layouts.py
python tools/generate-rv32i-building-block-layouts.py
python tests/RV32IBuildingBlockLayoutTest.py
python tests/RV32IMemoryLayoutTest.py
node tests/RV32ILayoutMigrationTest.mjs
```

The generators refit affected CPU ancestors and standalone scenarios, preserve
unrelated register placements, and retain old default geometry for browser
migration. `rv32i-blocks-v4` and `rv32i-alu-auto-v6` upgrade unchanged defaults; user-edited geometry and
colors remain. Removed flat child placements are retained in `previous_default`
but have no corresponding component in the new hierarchy.

`RV32IBuildingBlockLayoutTest` verifies grouping, the unchanged control-gate
counts, hierarchy-local wiring, bounds, title clearance, and no overlap.
`RV32IBuildingBlocksBrowserTest` checks all six documents, sixteen embedded scenarios,
all recorded expected pin values, and focused views of the ALU and new helpers.
Pass a screenshot directory and optionally an older `layout.json` as arguments
to inspect the migration visually. Both CPU program views are additionally
covered by `RV32ICoreLayoutBrowserTest` and native/WASM parity checks.

Use focused component/stage/core tests and serial compilation. Do not rebuild
or run unrelated memory suites for a geometry-only follow-up.

## Initial review validation (before the automatic-ALU follow-up)

- 26 focused native component/stage/core tests passed.
- All 44 registered single-cycle/pipeline program cases passed, run serially.
- The building-block documentation, single-cycle documentation and visualizer
  binding tests passed.
- 64 component layouts passed hierarchy/bounds/overlap checks; the broader CPU
  layout audit checked 80 RV32I types/variants.
- 158 saved-default migration cases passed, including preservation of edited
  coordinates, proportions and colors.
- Native/WASM parity passed for all six single-cycle stages and both program
  systems, in structural and behavioral modes (16 configurations).
- The strict MkDocs build passed. Published simulator assets, example rows and
  layout defaults were verified against the live preview on port 8893.
- All six rendered references passed navigation, embedded-link, table and
  mobile-width checks. All sixteen embedded scenarios passed their recorded
  pin/checkpoint assertions across focused browser runs; the ten updated
  scenarios were also checked with the pre-review saved layout seeded in
  IndexedDB. Expanded ALU/helper views passed bounds and overlap checks.
- Both CPU program views passed saved-layout migration checks: seven focused
  single-cycle views and eight pipeline views, including the containing Decode
  and Memory stages.

Browser checks run serially in focused batches, including existing saved
layouts. The static build can be served separately for these checks so a
MkDocs live reload cannot interrupt a long checkpoint sweep.

## Automatic-ALU follow-up

`ALU32_FLAGS_ROOT.RESULT_SPLIT` has been replaced by `SIGN_BIT`: a zero-delay
`Rewire` with `RESULT[32]` input and `SIGN[1]` output, mapping only bit 31.
The other 31 output pins are no longer present. Four-state sign forwarding and
independence from unknown lower bits have dedicated test rows.

Regenerate just this scope with:

```sh
python tools/generate-rv32i-building-block-layouts.py --alu32-only
```

This overwrites saved default child placements, rather than only filling missing
ones. It covers AddSub32, Logic32, Shifter32, control, comparison, result selection,
flags, and integrated ALU32, recursively including their structural children.
Type, fidelity-profile and all eight standalone root defaults are refreshed.
Shared gate-level component types consequently use their calculated defaults
wherever reused; styles and colors are retained. The geometry regression now
compares every generated placement against the normal graph algorithm and
rejects any ALU-specific custom placement override.

Focused native validation passed: twelve ALU/execute-stage tests, the ALU8
contract regression, both layout suites, layout migration, rewire geometry,
and the compiled 4.3 documentation examples (18 tests total). The flag scenario
now has 42 checkpoints, including the eight new four-state sign-isolation cases.

Native/WASM parity passed for all eight references and the single-cycle execute
stage, including topology, timing, example rows and final state. The refreshed
eight embeds passed all 816 checkpoint rows and their layout bounds in the
browser with the previous saved layout loaded; `SIGN_BIT` was verified to have
exactly the 32-bit input and 1-bit output. Expanded ALU views also passed.
All 179 migration cases passed, regeneration was idempotent, the strict MkDocs
build passed, and the five deployed engine/layout/example assets were verified
byte-for-byte against the live preview on port 8893.

## Child-stub clearance correction

The automatic layout used the parent's pin/stub ratios for child footprints.
In `ALU32_ROOT.COMPARE.ARITHMETIC_ZERO`, that reserved 2.5% of the splitter's
width rather than its actual 5.5%. The overlapping endpoints made
`A_bus_internal` take a backward detour through the splitter body.

Automatic graph placement now resolves each child's own type/profile styling,
uses the largest child footprint within each column, and budgets the actual
outer widths when fitting columns. The grid fallback and horizontal constraints
also use child-specific footprints. Parent pin clearance is kept independent.
The same correction is used by native default/reset placement and the offline
layout generators; no wire topology, ALU behavior, or routing semantics changed.

All eight 4.3.2 references and their structural descendants were regenerated as
`rv32i-alu-auto-v6`. Previous shipped defaults migrate on reload; edited child
placements, component proportions and colors are retained. Regression checks
cover graph/grid placement with mixed pin sizes and child profile overrides,
parent/child stub clearance, and non-overlapping child footprints. Browser checks
also require both comparator and flags zero-detector input buses to advance
monotonically toward their splitters, without a backward detour.

Validation: 74 building-block layouts and 80 RV32I types/variants passed their
layout suites; all 179 migration cases passed. The eight ALU references passed
all 816 live WASM checkpoint rows with the previous saved defaults seeded in
IndexedDB, including the new zero-detector routing assertions. Wire/rewire
geometry checks passed. Regeneration is idempotent, and the updated layout and
migration module match both the production package and port 8893 byte-for-byte.
