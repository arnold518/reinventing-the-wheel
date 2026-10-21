---
hide:
  - toc
---

# Interactive simulator

Every scenario on this site runs here, in the browser, on the same
simulation engine as the tests. Pick a scenario, follow the signals on the
circuit, scrub through time, and open any part to see what it is built from.

<div class="simulator-embed">
  <iframe
    src="../simulator/"
    title="CircuitSim interactive visualizer"
    loading="lazy"
    allowfullscreen>
  </iframe>
</div>

<a href="../simulator/" class="md-button md-button--primary">Open the simulator in a full page</a>

- **Scenario**: the documented circuits and programs, grouped as in the
  documentation.
- **Timing**: the [timing profile](timing.md#timing-profiles) the circuit is
  built and simulated with. `unit` counts one tick per gate; `embedded` and
  `desktop` use the gate delays of 65 nm and 32 nm logic and real memory
  timing, and show times in ps, ns, µs or ms. Changing it rebuilds the
  circuit. The choice is kept in the page address, so a link such as
  `simulator/?scenario=alu8&timing=embedded` opens that scenario in that
  profile.
- **Explorer**: the component tree; switching a part between its structural
  and behavioral form rebuilds the circuit with that choice.
- **Memory inspector**: the contents of a memory, read from its recorded
  history without performing an access.

The example tables on component pages are recorded in `unit`, so a scenario
opened in another profile shows the same values at different times.

RV32IM arithmetic/stages and device components have semantic family colors.
Width variants and structure-specific predictor/arbiter layouts retain their
family colors. Saved layout color overrides take priority.

One memory shared by a processor's instruction fetches and data accesses
through the [two-port arbiter](memory/controllers.md) is shown in the memory
inspector as two panels: instructions on the left (disassembled) and data on
the right, each at the latest address its side used. The
`rv32i-shared-memory` scenario shows it.

## Recorded time and game frames

Circuit scenarios are simulated before timeline playback. Moving the slider
restores recorded values; it does not advance a running game. The additive
<a href="../simulator/games.html">recorded-game viewer</a> displays immutable
screen captures from the new scripted platform, which is still undergoing
integration validation.

Simulation ticks use an explicit unsigned 64-bit type in the time-width
migration. Viewer data keeps ordinary numeric timestamps through 2^53−1;
larger ticks are exact decimal strings, displayed without rounding through
a JavaScript number. Timeline indexes remain ordinary numbers. This migration
and its long native/WASM recording checks are in progress.

## Device abstractions

**Device views** shows display pixels, logical keyboard keys, console status,
timer ticks and FIFO state from the selected recorded time. Turn it off to
inspect each component's internal circuit. Selecting a device opens a larger
preview and state summary in the inspector. Held keys, released keys and
unknown levels have distinct colors; queue head/count and overflow remain
visible even after a quick press/release. Display geometry and pixel masks
come from the configured component and recorded storage. Seeking backwards
reads history and does not issue device commands or resume execution.

The separate recorded-game page also has a display and keyboard panel. Its
playback controls inspect captured output; interactive simulation is a
separate mode being developed after milestone acceptance.
