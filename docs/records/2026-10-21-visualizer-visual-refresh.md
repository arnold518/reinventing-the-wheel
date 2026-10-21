# Visualizer visual refresh

Started: 2026-10-21. Status: closed. Code state: `realistic-timing` at `3d7a565`
(with unrelated uncommitted plan and status edits in the working tree).

## Summary

The owner found the visualizer's buttons and windows old-fashioned. The
toolbars, panels and form controls of `visualizer-v2/static` were restyled
into one dark theme with icon buttons, captioned dropdowns, a styled timeline
and a floating bottom dock. Element IDs, behavior and camera framing are
unchanged. The canvas drawing (component shapes, wire colors, canvas text) is
unchanged apart from the background and grid colors. Four visualizer browser
checks pass; the fifth stops at a docs-embed timeout that the unmodified
files share in the same scratch setup. The published site needs a Pages
build to show the new look.

## 2026-10-21 — Investigation

Screenshots of the WebAssembly simulator (1440×860, Full Adder and ALU8 with
the inspector open) showed:

- flat mid-blue buttons with heavy borders; text arrows `<<`, `<`, `>`, `>>`
  for the transport; Arial everywhere;
- browser-default dropdowns and timeline slider;
- a layout bug: the `Timing` dropdown, added later to the scenario column,
  did not fit the 76 px top bar and was cut off on desktop;
- a full-width bottom bar with no grouping between panel, view and layout
  actions.

The native server could not be used for screenshots: the deployed
`visualizer-v2/circuit_backend*.so` predates the scenario catalog and stops
with `KeyError: 'NativeMemoryContainerTest'`. The WebAssembly build in
`build-wasm/site` (built 2026-10-21) was used instead, through a scratch mirror
that links its WebAssembly files and the live `visualizer-v2/static` files.

## 2026-10-21 — Changes

- `styles.css` rewritten around tokens on `:root` (surfaces, borders, text,
  accent, radii, shadows, fonts). Icons are inline SVG masks, so they take the
  button's text color and need no extra files or network requests.
- Top bar: one row of captioned fields (Scenario, Timing; Core and Prediction
  on a second row in program view), a grouped transport with an accent Play
  button, the timeline, and the time readout. The Timing field now fits.
- Bottom bar: a floating dock centered over the visible canvas, grouped as
  panel toggles | fit and zoom | undo, redo, reset and save layout.
- Explorer, inspector, tooltip and loading card restyled; pin values in a
  monospace accent color; structural and behavioral badges tinted.
- Compact view (iframes and windows up to 960 px): same rows as before,
  captions hidden, Play and the layout actions icon-only; below 480 px the
  panel toggles are icon-only too, so the dock fits at 360 px.
- `index.html`: the transport is reordered to previous checkpoint, step back,
  Play, step forward, next checkpoint, reset. Icon-only buttons have
  `aria-label` and `title`. All IDs are kept.
- `app.js`: Play gets a `playing` class for its pause icon; the slider's
  filled part follows the current step; checkpoint marks are drawn under the
  track because the styled slider hides the browser's datalist ticks; canvas
  background `rgb(20, 23, 29)` and a dimmer background grid.

## 2026-10-21 — Verification

Run against a scratch mirror: today's `build-wasm/site` with
`visualizer-v2/static` linked in, served by `python3 -m http.server`, and
`tools/run-web-tests.py --base-url`.

- First run: `FFMemoryBrowserTest`, `MemoryInspectorBrowserTest`,
  `VisualizerScenarioBrowserTest` and `VisualizerTimingProfileBrowserTest`
  passed. `VisualizerWasmBrowserTest` failed with
  `('#time-slider', {'inside': False, 'clickable': True})` at 320 px: the
  fixed 9.5em Timing column plus the transport were wider than the screen.
  The compact grid now lets the Timing column shrink
  (`minmax(0, 9.5em)`).
- A direct measurement then found every top-bar control inside the viewport
  and clickable at 960, 640, 360 and 320 px, with and without the
  program-view row, and no top-bar overflow.
- Second run: `VisualizerScenarioBrowserTest` passed;
  `VisualizerWasmBrowserTest` passed its desktop and compact panel checks,
  then timed out at line 618 waiting 30 s for the iframe on `/visualizer/` to
  report ready. The same test against the files at `HEAD` (a second mirror)
  times out at the same line. Loaded by hand, that embed was ready in 4.6 s
  and correctly laid out. The steps after line 618 (chapter embeds) were not
  reached in either run.
- Not run: `tools/build-pages.sh`, the other browser checks, and the native
  server (its deployed backend stops with
  `KeyError: 'NativeMemoryContainerTest'`).

## 2026-10-22 — Merge compatibility with the device I/O work

Before committing, the owner asked that this change not conflict with the
ongoing device I/O work, which is uncommitted in the `.worktrees/rv32im-phase3`
worktree (based on `60acda2`). That work also edits `index.html` and `app.js`:
it inserts a `toggle-device-views` button as the first item of
`.view-actions`, a device section in the inspector, `device_view.js`, and
several `app.js` hunks. The worktree was only read, never changed.

- A three-way `git merge-file` of each shared file (base `60acda2`, this
  change, the worktree's file) found one conflict: the first divider and the
  rewritten `reset-view` line sat exactly where the device button is
  inserted. `app.js` and `styles.css` merged cleanly.
- Change: `reset-view` keeps its original line (`Reset View` text, now
  hidden by CSS and used as its accessible name); `app.js` sets its title;
  the divider after the panel toggles is `.panel-toggles::after`. The
  stylesheet also gives `#toggle-device-views` a monitor icon, the pressed
  style of the other toggles, and an icon-only form below 480 px.
- Re-run: all three files merge with no conflict and the merged `app.js`
  passes `node --check`. Screenshots of the dock with and without an injected
  device button at 1440 and 360 px; the top-bar and dock controls are again
  inside the viewport and clickable at 960, 640, 360 and 320 px.
- The device section's own markup (canvas, summary) was not styled further.
