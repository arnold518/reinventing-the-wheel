# Visualizer: pictures follow fidelity, open/close parts, hover picking

Started: 2026-10-28. Status: open. Code state: `alpha` at `5549ee0` with the
uncommitted non-device plan work in the working tree.

## Summary

The explorer tree is the planned fidelity: expanded = structural, collapsed
= behavioral; Apply & Run rebuilds to match; the S/B badge shows what is
built. The canvas follows the tree at once for built parts (a collapsed part
is drawn closed with live pins; an expanded behavioral part stays a block
marked "Apply to build", with its future parts previewed in gray).
The running visualizer never writes `layout.json` (port 8765 retired);
browser layout saves store only edits; two hover-picking bugs are fixed.

2026-10-29: Device View returns as an independent canvas toggle, initially
off. When on, only outermost components with an existing visual abstraction
show it, independently of built fidelity. Descendants remain hidden; turning
it off restores the fidelity/tree view. Code and documentation are updated;
focused policy/state checks, two live browser checks and the strict site
build pass. Behavioral components keep their abstractions with Device View
off. All component outlines are now solid, as subsequently requested by the
owner. Direct canvas regressions and the updated strict build pass; the
current `app.js?v=82` feature is available on 8893.
The earlier broad acceptance remains paused by the owner's 2026-10-28
decision until host memory permits it.

## 2026-10-28 — Owner questions and findings

The preview showed `parallel-prefix-adder-w32`'s structural `ANDGate<32>`
closed as a symbol until clicked, while the fidelity panel said structural and
Device views was off. That was the phase C3 visual: a gate-level word gate
drawn closed (`closedWordGate`) unless it was on the selection path,
independent of fidelity and of the Device views toggle. The owner's design:
the multi-bit gate symbol is only an abstract visualization, and abstractions
should follow fidelity, shown only when behavioral.

Assessment given: fidelity alone should not be the only way to tidy the view,
because applying a fidelity rebuilds and re-simulates the circuit (playback
stops; the layout must be saved first; `applyExplorerProfile`), and recorded
replays have a fixed fidelity. Agreed design: fidelity decides what runs and
pictures; opening/closing decides what is drawn, without a rebuild; a
behavioral marker on the canvas; picking uses what is drawn; scenario defaults
follow their default fidelity (owner: "For all scenarios, the default is same
as the fidelity").

Hover bugs (owner reports: hovering a structural component highlighted some
wires and pins; some components did not highlight on hover). Cause, from
source: `rebuildSpatialIndexes` built the component index from the parts
visible at geometry time (camera-dependent) and was not rebuilt on pan or
zoom, so parts that came into view later were missing; pins and wires were
indexed whenever no ancestor was a device/word-gate abstraction (and the
check skipped the owner itself), so the internals of parts closed by zoom or
by a device view stayed pickable, and the hover overlay then drew the
invisible wire. Not reproduced in a browser before the fix.

## 2026-10-28 — Change

`visualizer-v2/static/app.js`, `index.html` (cache key `app.js?v=77`):

- `devicePicture(c)`: behavioral and a supported device view.
  `componentClosedByView(c)`: the viewer's choice, else closed by default only
  for a device picture; never while something inside it is selected.
  `componentOpen(c)`: has children, large enough on screen, not closed by view.
  `componentDrawn(c)`: every enclosing part is open.
- Drawing: grid, wires and children only when open (leaves unchanged); the
  word gate symbol only for a word gate without children (behavioral); the
  device picture only for a closed behavioral device. Behavioral parts have a
  dashed outline; a closed part with contents shows `▸` before its name.
- Picking: the spatial indexes hold every part, pin and wire once
  (depth-first z for parts); hover accepts a part only if drawn, a pin only if
  its part is drawn with detail, a wire only if its owner is open and drawn.
  Opening/closing needs no geometry rebuild.
- Inspector **Open**/**Close** button for a selected part with contents;
  choices are stored per scenario in `localStorage`
  (`circuitsim-open-parts:<scenario>`, wrapped in try/catch) and are not part
  of `layout.json`. Reopening to the default removes the entry.
- Progressive topology does not request scopes of parts closed by view.
- Removed: the Device views button and its handler, `deviceAbstraction`,
  `componentAbstraction`, `wordGateOpen`, `closedWordGate` and the
  selection-path geometry rebuild for word gates. The inspector's device
  preview already showed any supported device regardless of fidelity.

Tests: new `VisualizerOpenCloseBrowserTest` (smoke, cleanup): structural
`XORGate<32>` opens by zoom without selection; hover reaches a one-bit gate
after a camera move with no geometry rebuild; Close draws a box and hover over
an inner gate returns the closed part; the choice survives a reload; Open
restores the default and stores nothing; a leaf has no button; no Device views
button. `RV32IMDeviceBrowserTest` opens behavioral devices through
`app.openChoices` where it used to turn Device views off, and both it and
`RecordedCircuitBrowserTest` exercise Open/Close instead of the toggle.

Docs: `site-docs/visualizer.md` (section rewritten), device pages
(framebuffer, buttons, Device I/O index, recorded games, device system) and
three reference pages no longer mention Device views.

Native probe check: a behavioral `PROPAGATE` override gives `XORGate<32>`
with fidelity `behavioral` and no children (so it gets the symbol); one-bit
gates report `unspecified` fidelity, word gates `structural`.

Pending: browser runs of the new check, the three visualizer checks, the two
device checks, and copying the simulator to the 8893 preview.

## 2026-10-28 — Layout storage: only the viewer's edits

Owner question: where layouts are saved, given hosting on GitHub Pages.
Findings from source: the native server rewrites `visualizer-v2/layout.json`
(`server.py`, POST `/api/layout`); the serverless build (Pages and the 8893
preview) saved the whole merged layout (about 16 MB in memory, 8.1 MB as
JSON) to IndexedDB `circuitsim-visualizer-v2`/`layout` on every save, and
that snapshot then overrode the shipped defaults on later visits, so a
browser that had saved once never received later layout improvements for any
entry (apart from revisioned entries whose placements still matched an old
default, and the legacy ALU reflow). Recorded replays save to
`localStorage`. There is no export. Owner decision: keep browser-local
layouts for now and store only the edits (recommendation 1); no export.

Change:

- New `visualizer-v2/static/layout_changes.mjs`: `layoutChanges(defaults,
  layout)` returns `{format: "layout-changes-v1", layout: edits}` with changed
  top-level values and settings, and per entry only changed fields and
  changed child placements, plus `layout_revision`,
  `automatic_layout_revision` and `layout_mode` of an edited entry.
  `mergeLayout` and `mergeLayoutStores` moved here from `wasm_worker.js`;
  `mergeLayoutStores` accepts either the changes record or an older full
  snapshot. Key order is ignored when comparing.
- `wasm_worker.js` stores `layoutChanges(currentDefaults, layout)` on save
  (defaults include the profile layouts generated for the current
  scenario). Cache keys: `transport.js?v=18`, `wasm_transport.js?v=17`,
  `wasm_worker.js?v=17`.
- Limits: a field or child the viewer removed cannot be stored as a change;
  an older full snapshot keeps the values it froze (as edits) after its next
  save. Clearing site data returns to the published layout.

Checks: new `VisualizerLayoutChangesTest` (Node, CTest
`visualizer;layout;unit`) on the real `layout.json`: no edits store nothing;
one placement, one color, one setting and one new root layout store 454
bytes instead of 8,120,922; round trip exact; a later release reaches the
edited entry's other child and an unedited entry; revision fields kept;
restoring a value removes it; an older full snapshot loads unchanged. Passes.
`VisualizerAutomaticLayoutMigrationTest` (815 migrations) and
`VisualizerProfileLayoutsTest` pass. `VisualizerReferenceProfileTest` was
failing before this change (its sandbox lacked `normalizeFidelityPreset`,
`profileStorageKey`, `setFidelityProfile`, `completeProfileLayouts` and
`componentLayout`, used by `loadScenario` since the 2026-10-24 fidelity
presets); stubs added (the real preset helpers are imported); passes.
New browser check `VisualizerLayoutStorageBrowserTest` (save without edits,
color edit stored as a change and restored after reload) is written but not
yet run. Docs: `site-docs/visualizer.md` (Save Layout bullet) and a storage
table in `docs/reference/visualizer-v2-profile-explorer.md`.

## 2026-10-28 — Explorer tree replaces Open/Close; fidelity on the badge

This replaces the per-browser Open/Close design of the "Change" section
above (inspector button, `localStorage` choices, pictures only when
behavioral).

Owner decisions, in order:

- Store each component's expanded/collapsed state with the layout
  (`layout.json` data model). Expanding/collapsing in the explorer decides
  what the canvas draws: expanded shows the children; collapsed shows the
  part's visual abstraction if it has one, otherwise no children. Buttons to
  save and reset the tree (names chosen: **Save tree**, **Reset tree**).
- Nothing may change the raw `layout.json` file, the 8893 preview included.
  (Checked: the 8893 preview is the WASM build and never wrote it; the native
  server on port 8765 rewrote it on Save Layout, as did developer layout
  tools.) The owner retired port 8765, so no native save path is built.
- Behavioral parts never expand (asked after seeing that only the four
  compact devices — console, timer, buttons, display under the compact
  preset — are behavioral with internal groups; 249 behavioral parts of the
  default device system have none). The S/B badge is the fidelity switch.

Change (`visualizer-v2/static/`):

- `explorer.js` (v4): the arrow expands/collapses through app callbacks
  (`canExpand`, `isExpanded`, `onToggleExpanded`), no rebuild; a behavioral
  part or leaf has a disabled dot. The badge is a `button.explorer-fidelity`
  that edits the fidelity draft (disabled when only one form exists).
  Searches walk into collapsed structural parts, never behavioral ones.
- `app.js` (v78): `treeExpandable` (children and not behavioral),
  `treeExpanded`, `componentClosedByView` (behavioral, or collapsed in the
  tree unless something inside is selected). A closed part draws its word
  gate symbol or device view at any fidelity, else a `▸` box. Tree state is
  `app.treeState` (paths mapped to `false`), kept across rebuilds of the same
  scenario, separate from `app.layout` so tree edits do not mark the layout
  unsaved or block Apply & Run. **Save tree** posts `/api/layout/tree`;
  **Reset tree** restores the published tree (unsaved). Both are disabled
  for recordings. The inspector Open/Close button and the `localStorage`
  choices are removed.
- `wasm_worker.js` (v18): payload `treeKey` (scenario key, not the
  fingerprinted layout key, so a collapse survives fidelity switches),
  `treeState`, `publishedTreeState`; `/api/layout/tree` validates and stores;
  Save Layout keeps the current tree.
- `layout_changes.mjs` (v2): `tree_states[scenario]` is stored whole when it
  differs from the published one, and replaces it as a whole on load.
- `index.html`: Save tree / Reset tree in the explorer footer, subtitle
  "Arrows open or close parts. S/B switches fidelity.", cache keys bumped
  (styles 32, explorer 4, transport 19).

Tests: `VisualizerOpenCloseBrowserTest` replaced by
`VisualizerExplorerTreeBrowserTest` (default expanded, collapse without a
rebuild, hover, Save tree across reload, Reset tree, leaf dot, badge switch
to behavioral removes the arrow). The 19 uses of `.explorer-arrow` that
switched fidelity in eight browser tests now use `.explorer-fidelity`.
`RV32IMDeviceBrowserTest` collapses and expands a structural device root
from the explorer; `RecordedCircuitBrowserTest` checks Save tree is
disabled. `VisualizerLayoutChangesTest` gained tree cases (published trees
not stored, saved trees win per scenario, an untouched scenario follows a
release) and passes. Browser runs pending (full native suite still running).

Docs: `site-docs/visualizer.md` ("Fidelity and the explorer tree", Explorer
bullet), device pages, RV32IM and Device I/O index pages; the explorer
reference's user model and storage table (native row marked retired);
`unified-component-profiles.md`.

## 2026-10-28 — Explorer tree as the planned fidelity (replaces the tree storage)

This replaces the previous section's model (arrow as a view-only fold, badge
as the fidelity switch, Save tree / Reset tree, `tree_states`).

Owner: fidelity and open/close mean almost the same thing; the only
difference is that the simulator runs on the fidelity. Badges become
read-only; expanding/collapsing proposes the new fidelity; expanding a
behavioral node shows its children in gray without badges; Apply makes the
built fidelity match the tree. Then: the canvas follows the tree, except for
the gray components, which are not simulated yet. Decisions taken with the
owner's agreement: parts with one form get no arrow; Revert returns the tree
to what is built; recordings have disabled arrows.

Assessment given: one concept; Apply already saves the profile and Revert
discards drafts, so the tree storage goes; collapsing for tidiness is cheap
because behavioral parts run on the delays proven from their gate-level form
(pin timing unchanged, internal event counts change); everything drawn is
simulated (a part collapsed before Apply keeps live pin values); the solid
or dashed outline tells the built fidelity.

Change (`visualizer-v2/static/`, deployed to the 8893 preview on request):

- `explorer.js` (v5): the arrow edits the fidelity draft (expanded =
  structural); rows whose draft differs from the built fidelity get class
  `pending` (italic); a planned-structural part built behavioral gets a gray
  placeholder row ("Its parts appear after Apply & Run"); the badge is a
  read-only span from `builtMode`; `onDraftChanged` tells the app.
- `app.js` (v79): `plannedFidelity` (the explorer draft),
  `componentClosedByView` (built behavioral, or planned behavioral unless
  something inside is selected), `pendingStructural` (title "· Apply to
  build"). Tree state, Save tree / Reset tree removed.
- `wasm_worker.js` (v19) and `layout_changes.mjs` (v3): tree endpoint,
  payload fields and `tree_states` removed. `index.html`: buttons removed,
  subtitle "Expand to build from parts, collapse for one block; Apply & Run
  builds it." `styles.css` (v33): pending and placeholder rows.

Checked on 8893 (headless, after the preview rebuilt): page ready; collapsing
`PROPAGATE` closes it with the same session, badge S, "1 unapplied change";
Apply makes it behavioral (badge B, no children); expanding marks it pending;
Apply builds it again (35 children: 32 one-bit XOR gates and 3 bit adapters,
as before); no page errors.

Not built yet: the gray preview of a behavioral part's actual parts (needs a
part-preview call in C++/WASM and a WASM rebuild). Tests:
`VisualizerExplorerTreeBrowserTest` rewritten for this model; the 19 arrow
uses in eight browser tests switch fidelity again as before;
`RV32IMDeviceBrowserTest` collapses a structural device root without a
rebuild and reverts; `RecordedCircuitBrowserTest` checks the arrows are
disabled. Docs rewritten (visualizer page, explorer and profile references,
RV32IM, Device I/O and recorded-games pages). Browser runs pending.

## 2026-10-28 — Gray preview of parts built only after Apply

- C++: `VisualizerSession::previewComponentsJson(test, overrides, preset)`
  builds the scenario with the same profile as a session (the profile code
  moved into one helper, `applyVisualizerProfile`, used by both) and stops
  after `setupCircuit()`; it lists every component (id, name, type,
  parentIndex, fidelity, availableFidelities, profileSelectable).
  `VisualizerSessionProbe preview <test> <scenario> [--fidelity] [--override]`
  and the WASM binding `previewComponentsJson(test)` (current overrides and
  preset) expose it. Native check: the 32-bit prefix adder with `PROPAGATE`
  structural previews 724 parts (35 inside it) in 0.08 s; the device system
  under compact with a structural console previews 1,104 parts (502 inside
  the console).
- Worker: `POST /api/circuit/preview` sets the preset and overrides and
  returns the list; Apply accepts a choice for a previewed part.
- Explorer (v6): `setPreview`; a planned-structural part built behavioral
  lists its previewed parts as gray rows with no badge (their arrows edit the
  draft; clicking the row does not focus, since nothing is built);
  previews are cleared on each new build. App (v80): after each draft change
  (150 ms debounce) it requests a preview when some part is pending
  structural; on failure the placeholder row stays.
- Builds: native probe and test runner, and the WASM module (26 s, 364 MB
  peak, under a 1 GB cap; the build refreshed `pages/simulator`, so the
  generated `example-rows.json` was copied back from the built site —
  simulation is unchanged). Cache keys: `circuitsim_wasm.js?v=13`, wasm
  `?v=11`, worker 20, transport 21, explorer 6, app 80, styles 34. Deployed
  to the 8893 preview (served module hash checked).

Browser checks (each under a 1–1.1 GB cap, host memory permitting):
`VisualizerExplorerTreeBrowserTest` (now also waits for 35 gray preview rows
without badges), `VisualizerLayoutStorageBrowserTest`,
`VisualizerScenarioBrowserTest`, `RecordedCircuitBrowserTest`,
`VisualizerTimingProfileBrowserTest` and `MemoryInspectorBrowserTest` pass.
`MemoryInspectorBrowserTest` first failed on a pre-existing ambiguous
`locator("canvas")` (the inspector's device-preview canvas is in the
committed page); it now targets `#circuit-canvas`. Stopped by their memory
cap (the host had about 1 GB free): `VisualizerWasmBrowserTest` (1.1 GB),
`VisualizerRewireBrowserTest` (1 GB; it passed earlier today at 1.1 GB
before the explorer change), `SRAMDocumentationBrowserTest`. Not run:
`MemoryDocumentationBrowserTest`, `MemoryDeviceDocumentationBrowserTest`,
`MemoryEndpointBrowserTest`, `FFMemoryBrowserTest`,
`RV32IMDeviceBrowserTest`; the strict site build with the new pages.

## 2026-10-28 — Strict site build and more browser checks

- Strict MkDocs build of the rewritten pages passes (46 s, 196 MB peak, no
  warnings); the simulator was copied back into the built site afterwards.
- With about 1.15–1.25 GB free on the host and a 1.1 GB cap per check:
  `VisualizerRewireBrowserTest` and `FFMemoryBrowserTest` pass.
  `SRAMDocumentationBrowserTest`, `MemoryDocumentationBrowserTest`,
  `MemoryDeviceDocumentationBrowserTest` and `MemoryEndpointBrowserTest` were
  stopped by the cap partway (each had passed its first scenarios; they load
  documentation pages with several embedded simulators). Not run:
  `VisualizerWasmBrowserTest` (stopped earlier at 1.1 GB; it passed at
  3 GB this morning before the explorer change) and `RV32IMDeviceBrowserTest`.
  These need more free host memory.

## 2026-10-28 — Verification paused (owner decision)

A queued run that waited 30 minutes for 3 GB of free host memory skipped
`VisualizerWasmBrowserTest` (free memory stayed near 1.2–1.3 GB) and was
stopped. The owner chose to stop here for now. Not run, to resume later with
more free memory: `VisualizerWasmBrowserTest`, `RV32IMDeviceBrowserTest`,
`SRAMDocumentationBrowserTest`, `MemoryDocumentationBrowserTest`,
`MemoryDeviceDocumentationBrowserTest`, `MemoryEndpointBrowserTest`, and the
full native suite (561 of 591 tests).

## 2026-10-29 — Outermost Device View

Owner request: bring back the Device View button, showing a component's
visual abstraction only when none of its ancestors has one. This supersedes
the earlier removal of the button, while preserving the explorer's fidelity
model.

- Restore `#toggle-device-views`, initially off, reusing its existing monitor
  icon and responsive styling. With it on, recognize existing device/bus
  panels, gate symbols, register values, register-file tables, memory arrays
  and cell values independently of built fidelity. The highest abstractable
  component on each branch closes its child circuit; separate branches can
  show their own abstractions. Parts without an abstraction keep their usual
  fidelity/tree view. Solid/dashed outlines still report built fidelity.
- Hidden descendant selections, including pins and wires, resolve to their
  outermost abstraction; its own boundary pins remain selectable. Explorer
  focus uses the same visible owner. Turning the toggle off restores the
  current fidelity/tree view. The toggle does not change sessions, timeline,
  profile draft, layout, or simulation state, and works on fixed recordings.
- In this mode, request additional state only for outermost device,
  register-file and memory abstractions. Pin-based register/cell values use
  the current recorded state. Guard overlay responses against later view or
  timeline requests. Missing retained memory/register-file data is labeled
  "State not recorded". No C++ implementation, fidelity factory or delay
  table changes are needed.
- Document the toggle and selection/state behavior in the public visualizer
  page and internal explorer reference. Frontend cache key: `app.js?v=81`.
- Extend `VisualizerDeviceViewTest` with mixed structural/behavioral nested
  device, gate and storage cases; independent siblings; hidden selections;
  fidelity preservation; and late overlay responses after a view change.
  Repair its pre-existing worker-layout helper lookup: `mergeLayout` now
  comes from the imported `layout_changes.mjs`, not a worker-local function.
  The device-view and memory-cell Node checks pass; both modified browser
  scripts compile. Browser verification and strict site build are pending.

### 2026-10-29 — Focused verification completed

This replaces the pending checks in the implementation section above.

- CTest in `build-milestone`: `VisualizerDeviceViewTest` and
  `VisualizerMemoryCellViewTest`, 2/2 passed. Both modified Python browser
  scripts compile. Scoped `git diff --check` passes.
- `tools/run-web-tests.py` against the current 8893 preview, under a
  1,100 MiB/no-swap scope: `VisualizerExplorerTreeBrowserTest` and
  `RecordedCircuitBrowserTest`, 2/2 passed (14.54 s and 5.80 s). The explorer
  check covers structural word-gate abstraction, hidden-child hover and
  focus, unchanged session/index/draft/layout/state, return to structural
  contents, and existing Apply/Revert/gray-preview behavior. The recorded
  check covers independent console/timer/keyboard/display/structural-bus
  views, four suppressed nested abstractions, descendant focus, backward
  seeking, and all 18 Pong pin/wire/display/input checkpoints. No page
  errors; captured playback never loads a WASM CPU. The screenshot was
  inspected: the console is drawn as its panel, with selectable boundary
  pins, the Device View button active and the original fidelity badge.
- Initial launch failed before browser startup because the MkDocs virtualenv
  has no Playwright. Use `/usr/bin/python3` for these browser checks. The
  first real explorer run passed the new view checks, then its existing
  immediate arrow-text assertion raced the virtualized tree's next render.
  That assertion and the Revert counterpart now wait for their target text;
  the final run passes both checks. No memory-cap interruption occurred.
- Strict MkDocs preview build passes under a 768 MiB address-space cap,
  48.92 s, zero warnings/errors (existing simulator-link informational
  messages remain). Refresh only `app.js` and `index.html` in
  `build-wasm-milestone/pages/simulator`; served app SHA-256 matches source
  (`f35fed8126c225c26c0b8265c2a0ccf7416a4aa3f24a964464fa598d2a4f3c66`).
  The live page has the restored button and `app.js?v=81`; preview service
  is active with zero automatic restarts.
- Evidence: `build-milestone/device-view-20261029/` contains strict-build
  output, live assets, browser results and screenshots. Key results above
  are retained here rather than depending on that generated directory.
  No native/WASM rebuild, timing-table regeneration or full suite was run.
  The six broader browser checks and full native suite paused by the owner
  on 2026-10-28 remain outside this focused change; this record stays open
  for that acceptance.

## 2026-10-29 — Behavioral pictures with Device View off; solid outlines

Owner clarification: behavioral parts with a visual abstraction should show
it even with Device View off. Owner also requests removal of dotted outlines.
This supersedes the outline distinction described in earlier dated sections.

- Source inspection confirms that the renderer already permits behavioral
  device panels, gate symbols and storage overlays with Device View off,
  including when the draft asks for structural but Apply has not rebuilt it.
  Retain that behavior and add direct canvas regression coverage for it.
- Remove the behavioral `setLineDash([7, 4])` from component borders. All
  component outlines use the existing solid style; explorer S/B badges still
  identify built fidelity. Frontend cache key advances to `app.js?v=82`.
- Update the public visualizer reference and internal explorer contract to
  state both rules explicitly. No simulation, fidelity or timing change.
- Focused device-view and memory-cell CTest checks pass, 2/2. The browser
  checks now observe actual gate-symbol/device-panel drawing with the toggle
  off and assert that no dashed outline is requested; the gate check also
  covers pending structural rebuilds. Browser and strict site checks pending.

### 2026-10-29 — Canvas regression results

- `VisualizerExplorerTreeBrowserTest` and `RecordedCircuitBrowserTest` pass
  on 8893 under a 1,100 MiB/no-swap scope (48.52 s and 6.11 s). Direct renderer
  observation confirms a behavioral word-gate symbol with Device View off,
  both before and while pending a structural rebuild; it also confirms the
  recorded display's actual checkpoint pixels with the toggle off. Neither
  render requests a dashed stroke. Existing outermost-view, Apply/Revert,
  hover and all 18 recorded checkpoint checks still pass, without page errors.
- Inspect the saved screenshot: Device View is off, the display still shows
  its captured pixels and its selected component border is solid. The live
  public visualizer page describes both rules. Served `app.js` matches the
  source SHA-256
  `2aa93eb3dc5229031c061b7f7591dbb21eff0c21f176ec72a3bda3b0cd17cf73`.
  An initial 20-second asset probe timed out during the preview's 52.36-second
  strict rebuild; the retry succeeds. No service restart or memory-cap stop.
- Evidence is under `build-milestone/device-view-off-20261029/`. The isolated
  strict site build is still running; no native/WASM rebuild, delay-table
  regeneration or broad suite is part of this rendering follow-up.

### 2026-10-29 — Follow-up verification complete

The isolated strict MkDocs preview build completes successfully in 50.66 s
under the 768 MiB address-space cap, with no warnings/errors. This replaces
the pending strict-build statements above. Scoped `git diff --check` also
passes. Both requested rendering rules are complete and live on 8893; the
earlier owner-paused broad acceptance remains separate and this record stays
open for it.
