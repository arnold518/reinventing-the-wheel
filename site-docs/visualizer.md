---
hide:
  - toc
---

# Interactive simulator

Every scenario on this site runs here, in the browser, on the same
simulation engine as the tests. Pick a scenario, follow the signals on the
circuit, scrub through time, and open any part to see what it is built from.

- **Scenario**: the circuits and programs of chapters 2–4, grouped as in the
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
  history without performing an access. One memory shared by a processor's
  instruction fetches and data accesses through the
  [two-port arbiter](memory/controllers.md) is shown as two panels:
  instructions on the left (disassembled) and data on the right, each at the
  latest address its side used. The `rv32i-shared-memory` scenario shows it.

The example tables on component pages are recorded in `unit`, so a scenario
opened in another profile shows the same values at different times.

<div class="simulator-embed">
  <iframe
    src="../simulator/"
    title="CircuitSim interactive visualizer"
    loading="lazy"
    allowfullscreen>
  </iframe>
</div>

<a href="../simulator/" class="md-button md-button--primary">Open the simulator in a full page</a>
