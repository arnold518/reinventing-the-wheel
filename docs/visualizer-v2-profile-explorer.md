# Visualizer V2 Profile Explorer

## User model

The component explorer uses the same visual action as a folder tree:

- expanded means “build and show this component structurally”;
- collapsed means “build this component behaviorally”;
- a dot and `fixed` label mean that only one implementation exists.

There is no separate structural/behavioral switch. The arrow is the fidelity
control. Changing an arrow edits a draft; `Apply & Run` rebuilds and runs the
scenario. `Revert` discards unapplied edits.

Clicking the rest of a row selects the component and smoothly moves the canvas
camera to it. The camera target accounts for the explorer and inspector, so
the selected block stays visible instead of moving underneath a panel.

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
scenario base BuildProfile
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
    "rv32i-program9": {
      "revision": 1,
      "exact_overrides": {
        "RV32I_SINGLE_CYCLE_SYSTEM_ROOT.CORE.ALU": "behavioral"
      }
    }
  }
}
```

The real navigation tree always comes from the circuit that C++ actually
built. This prevents a stale JSON tree from disagreeing with the simulator.

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

## Main files

- `visualizer-v2/static/explorer.js`: profile draft and virtual tree
- `visualizer-v2/static/app.js`: circuit model, sessions, and smooth camera
- `visualizer-v2/static/styles.css`: docked explorer and responsive layout
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
MemoryBit Apply & Run: 28 structural -> 1 behavioral -> 28 structural
RV32I program 9: 54,049 component records, 48 rendered DOM rows
browser console errors: 0
```

The live screenshot pass also checked draft collapse, a successful rebuild,
returning to the inherited profile, and smooth focus on `MemoryBit.STATE`.

Behavioral state overlays follow the selected implementation. MemoryBit value
badges, register-file tables, and large-memory tables render only when their
component fidelity is `behavioral`. Structural versions show their child
circuits without the behavioral overlay or its additional state requests.
