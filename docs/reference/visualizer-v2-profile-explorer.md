# Visualizer V2 Profile Explorer

## User model

The explorer tree is the planned fidelity (owner decision, 2026-10-28; it
replaces an interim model in which the arrow was a view-only fold and the
badge switched fidelity):

- Expanded (▾) requests structural, collapsed (▸) behavioral, for a part that
  offers both; a dot marks a part with one form. Changing an arrow edits a
  draft (the row turns italic, "N unapplied changes"); `Apply & Run` rebuilds
  and runs the scenario to match; `Revert` discards unapplied edits.
- The S/B badge is read-only: the fidelity built and simulated now.
  Component outlines use the same solid style for both fidelities.
- The canvas follows the draft at once for parts that exist: a part planned
  behavioral but still built structurally is drawn closed (visual
  abstraction or `▸` box) with live pin values; a part planned structural but
  built behavioral stays a block titled "Apply to build". The explorer lists
  the parts it would have in gray, without badges, from a build-only preview
  (`POST /api/circuit/preview` in the worker →
  `VisualizerSession::previewComponentsJson`: the same profile as a session,
  `setupCircuit()` only, nothing simulated); their arrows edit the same
  draft, and Apply accepts choices for previewed parts. A placeholder row
  shows until the preview arrives or if it fails.
- Choices for parts inside a collapsed one are kept by path and apply again
  when it is built from its parts. A recording's fidelity is fixed, so its
  arrows are disabled.

Clicking the rest of a row selects the component and smoothly moves the canvas
camera to it. The camera target accounts for the explorer and inspector, so
the selected block stays visible instead of moving underneath a panel.

`Device View` is an independent canvas toggle, initially off. When enabled,
each branch shows its outermost existing visual abstraction, regardless of
built or planned fidelity: device panels (including buses), gate symbols,
register values, register-file tables, memory arrays and cell values. Its
children and internal wires stay hidden, including when a descendant is
selected in the explorer: selection and camera focus use the outermost
abstraction instead. Its boundary pins remain selectable. Components without
an abstraction retain the usual fidelity/tree view. Turning the toggle off
restores that view, including the visual abstractions of behavioral parts.
Those abstractions also remain while a behavioral part is pending a structural
rebuild. The session, timestamp, profile draft and layout are
preserved; fixed recorded circuits support the toggle too.

The camera fits the component into the actual unobstructed canvas rectangle:
below the top toolbar, above the bottom toolbar, right of an overlaid explorer,
and left of the inspector. The fitted visual bounds include the component
shell, pin triangles, and boundary stubs. A fixed screen-space margin remains
around those bounds. The target center is the center of the main canvas after
excluding the left explorer; opening the right inspector may reduce the fit
zoom but does not pull that target center back toward the left. Focus zoom has
no fixed upper cap: deeply nested components are fitted from their actual
world-space size. During the animation, the camera's world-space view center
moves toward the selected component while zoom changes logarithmically.
Because that center remains inside the circuit hierarchy, returning from a
deep leaf to an ancestor does not fly through empty space. This also avoids
the overshoot and direction reversal caused by independently interpolating
world offset and zoom.

## Data flow

```text
the scenario's fidelity preset (its catalog profile, or the one chosen)
          +
profiles.json exact-path overrides
          |
          v
C++ ComponentCatalog builds the selected recursive tree
          |
          v
new simulation session + topology + profile revision
          |
          v
virtualized explorer and circuit canvas
```

`profiles.json` deliberately contains no navigation tree. A saved entry is
only a sparse map such as:

```json
{
  "schema_version": 1,
  "scenarios": {
    "rv32i-program9::logic": {
      "revision": 1,
      "exact_overrides": {
        "RV32I_PROGRAM_ROOT.CORE.ALU": "behavioral"
      }
    }
  }
}
```

Entries are keyed `<scenario>::<preset>`: each fidelity preset of a scenario
keeps its own overrides. An entry saved under the bare scenario name (before
every scenario had a named default profile) moves to the scenario's default
profile when the store is loaded.

The real navigation tree always comes from the circuit that C++ actually
built. This prevents a stale JSON tree from disagreeing with the simulator.

The single-cycle and five-stage program scenarios deliberately use the same
fixed `RV32I_PROGRAM_ROOT`. Its four direct children are `CLOCK`, `CORE`,
`INSTRUCTION_MEMORY`, and `DATA_MEMORY`. The root and clock show as fixed;
expanding or collapsing starts at `CORE` and at any descendant family that
actually supplies both fidelities. Saved profiles that used either older
program-root prefix are translated when they are loaded.

## Apply safety

Each topology response has an opaque simulation session ID. State requests use
that ID, so a rebuild cannot accidentally mix the old topology with new state.

The server validates:

- the scenario supports a build profile;
- the client is applying the revision it loaded;
- each changed path exists in the active circuit; and
- the component provides the requested fidelity.

It then builds and runs a candidate session before atomically saving the
profile. A failed candidate leaves the saved profile and active session
unchanged.

Layout edits and profile edits are independent. `Apply & Run` is disabled
while unsaved layout edits exist so that a topology replacement cannot
silently discard a manual placement.

## Large-tree performance

The explorer keeps the full component records in JavaScript but creates DOM
elements only for the rows around the current scroll position. A fully
structural RV32I program has 54,049 component records, while a normal viewport
renders roughly 40–50 row elements.

Search is debounced and includes the ancestors of matches, so results retain
their location in the hierarchy. The explorer width and open/closed state are
stored locally in the browser.

Pins, wires, and their geometry load progressively. The first circuit response
contains the complete component navigation tree and only the root drawing
scope. `POST /api/topology` supplies exact owner-wire and boundary-pin records
in batches when zoom or focus makes a deeper scope resolvable. Every state
response still contains the complete indexed value vector for every pin and
wire, and the server retains the full time-travel history; only drawing
metadata is delayed.

Pin identities include direction: `<component ID>.input.<name>` or
`<component ID>.output.<name>`. An input and an output may legally share a
display name (for example, a memory-lane Rewire's `LOAD` and `STORE` ports),
but they have separate IDs and state indices. Native, WASM, and legacy desktop
wire endpoints all use the core pin ID. Backend directionless lookups are
allowed only for unambiguous names; ambiguous names require a direction.
The browser accepts identical records repeated across lazy topology scopes,
but rejects conflicting duplicate IDs or missing wire endpoints instead of
silently dropping ports. Component IDs and saved layout keys are unchanged.

For the 54,049-component Program 1 tree this reduced the initial response from
128.8 MB to 17.9 MB uncompressed and 0.77 MB over browser gzip. A clean local
headless browser displayed the overview in 2.8 seconds. Deep focus remains
exact: selecting a depth-8 NAND gate fetched its missing scopes and exposed
both inputs and its output without reloading the simulation.

The blocking overlay reports three distinct phases:

- building and simulating the circuit;
- decoding the compact circuit index; and
- preparing the root overview.

After the overview appears, any visible-detail fetch is non-blocking and is
reported beside the component/pin/wire counts.

## Appearance

The page chrome is styled from tokens on `:root` in `styles.css` (surfaces,
borders, text, accent, radii, shadows and fonts). The canvas clear color in
`app.js` matches `--canvas-bg`. Button icons are inline SVG masks chosen by
element ID, so they follow the button's text color. The top bar is a grid of
captioned fields, the transport, the timeline and the readout; the bottom
toolbar is a dock centered over the visible canvas, and the camera still
treats its top edge as the lower limit of the unobstructed area. Checkpoints
are drawn as marks under the timeline by `renderCheckpointMarks`, because the
styled slider hides the browser's datalist ticks. In compact view, captions
and some button labels are hidden; every icon-only button keeps an
`aria-label` and a `title`.

## Where layouts and choices are saved

| Runtime | Save Layout | Fidelity choices (Apply & Run) |
| --- | --- | --- |
| Serverless WASM (GitHub Pages, 8893 preview) | browser IndexedDB `circuitsim-visualizer-v2`, key `layout` | same database, key `profiles` |
| Recorded replay | browser `localStorage`, one key per recording | fixed by the capture |
| Native (`server.py`, port 8765) | retired by the owner (2026-10-28); its Save Layout rewrote `layout.json` | `visualizer-v2/profiles.json` |

The running visualizer never writes `layout.json` (owner rule, 2026-10-28):
it changes only through the repository's layout tools and commits. The
published site ships that file, and a visitor's saves stay in that browser.
The explorer tree is the fidelity profile, so it is saved by Apply & Run;
published defaults are the scenarios' default profiles. The serverless save stores only the
edits, as differences from the shipped layout
(`visualizer-v2/static/layout_changes.mjs`, record format
`layout-changes-v1`): changed fields per entry and changed child placements
by name, plus the entry's revision fields so revision upgrades still apply.
Entries a visitor never edited therefore follow each new release. A field the
viewer removed cannot be expressed and is not stored. A full snapshot saved
before this format still loads; the next save replaces it with changes, and
values it froze stay as edits. `VisualizerLayoutChangesTest` (Node) and
`VisualizerLayoutStorageBrowserTest` cover this.

## Main files

- `visualizer-v2/static/explorer.js`: profile draft and virtual tree
- `visualizer-v2/static/app.js`: circuit model, sessions, and smooth camera
- `visualizer-v2/static/styles.css`: theme tokens, docked explorer and responsive layout
- `visualizer-v2/server.py`: profile-aware session lifecycle and HTTP API
- `visualizer-v2/profile_store.py`: validated atomic persistence
- `visualizer-v2/profiles.json`: saved sparse exact-path choices

## Verification

Run the focused visualizer checks:

```bash
ctest --test-dir build --output-on-failure \
  -R 'VisualizerModuleBindingsTest|VisualizerProfileStoreTest'
```

Final verification:

```text
complete CTest suite: see unified-component-migration-report.md
canonical layouts: 110 scenarios, 131 type layouts, 110 root layouts
live service: http://127.0.0.1:8765/api/health -> {"ok":true}
FlipFlopCell Apply & Run: 28 structural -> 1 behavioral -> 28 structural
RV32I program 9: 54,049 component records, 48 rendered DOM rows
browser console errors: 0
```

The live screenshot pass also checked draft collapse, a successful rebuild,
returning to the inherited profile, and smooth focus on `FlipFlopCell.STATE`.

With Device View off, state overlays follow the built implementation:
FlipFlopCell value badges, register-file tables and large-memory tables appear
for behavioral parts. Structural forms show their child circuits; terminal
DRAM/NOR cells retain their pin-value badges. Closed device components and
gate symbols follow the fidelity draft as described above.

With Device View on, structural forms can use those same overlays. Additional
state requests cover only the outermost device, register-file and memory
abstractions; register and cell values use recorded boundary pins. A view
change invalidates outstanding overlay requests, so late responses cannot
overwrite the newer view's state. Seeking refreshes the visible overlays at
the selected timestamp. Missing retained memory/register-file data is labeled
"State not recorded". This is an observational rendering choice and does not
change simulation fidelity or synthesize missing recording data.
