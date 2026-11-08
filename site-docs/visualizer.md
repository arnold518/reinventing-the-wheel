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
- **Timing**: the [timing profile](basic/1.2.md#timing-profiles) the circuit is
  built and simulated with. `unit` counts one tick per gate; `embedded` and
  `desktop` use the gate delays of 65 nm and 32 nm logic and real memory
  timing, and show times in ps, ns, µs or ms. Changing it rebuilds the
  circuit. The choice is kept in the page address, so a link such as
  `simulator/?scenario=alu-width8&timing=embedded` opens that scenario in that
  profile.
- **Fidelity**: rebuild with `gate`, `logic`, `visual` or `compact`, independently
  of timing. Every scenario opens in one of them: the one that shows it as it
  was designed to be read, keeping a large circuit fast enough to play.
  Gates, wiring, ALUs, memory parts and small building blocks open in `gate`;
  device parts, devices, buses and pipeline stages in `logic`; the CPU
  programs, the memory arrangements of § 4.7 and the whole computer systems
  in `visual`; a few iterative arithmetic parts and the memory subsystem in
  `compact`. Switching scenario returns to the new scenario's own profile; a
  choice other than it is kept in the page address.
  A few fixed demonstrations (the full circuit, the clock generator, the
  two-level cache, the program loader) are shown as built and cannot change.
  `gate` expands all selectable parts, including storage, and rejects devices
  that exceed structural capacity. `logic` keeps computation/control structural
  with compact registers and large memory storage. `visual` uses behavioral
  forms for full device, register and memory views; other logic stays structural.
  `compact` prefers behavioral forms wherever available. Primitive gate symbols
  do not count as full visual abstractions. For example,
  `simulator/?scenario=register32&timing=embedded&fidelity=visual` keeps both choices.
  Explorer edits are saved separately for each preset and shown as **+ overrides**.
  Unsupported presets are disabled with a reason. For example, the full device
  system cannot use Gate because its 256 KiB RAM requires compact storage.
  Changing a preset generates placements for newly exposed component structures
  while preserving compatible saved layouts.
  Recorded games keep the fidelity used during capture; selecting a different
  preset requires another simulation.
- **Explorer**: the component tree. Expanding a part plans it structural and
  collapsing plans it behavioral; Apply & Run rebuilds the circuit with that
  choice, and the S/B badge shows what is built.
- **Save Layout**: keeps your layout edits (placements, sizes, colors) in this
  browser only. Only what you changed is stored, so every part you have not
  edited still follows the published layout when the site is updated.
  Clearing this site's data returns to the published layout.
- **Memory inspector**: the contents of a memory, read from its recorded
  history without performing an access.

The scenario menu follows the overall component order: foundations, ALU,
memories, RV32I components and programs 01–22, then the Computer System
groups: RV32IM components, programs 23–27, and the buses, memory subsystem,
devices and whole machine. Developer circuits come last. Selecting a bus
shows its traffic in the inspector: which master is asking which target, and
each answer. Within each group,
supporting parts precede complete devices or cores, width variants increase
numerically, and programs retain their numbering. The RV32I program rows
share the core and prediction selectors.

Instruction-memory views decode for the containing processor: RV32IM
machines display all eight multiply/divide instructions, while RV32I
machines continue to mark those encodings as unsupported. Structural and
compact cores use the same display decoding.

The example tables on component pages are recorded in `unit`, so a scenario
opened in another profile shows the same values at different times.

RV32IM arithmetic/stages and device components have semantic family colors.
Width variants and structure-specific predictor/arbiter layouts retain their
family colors. Saved layout color overrides take priority.

One memory shared by a processor's instruction fetches and data accesses
through the [two-port arbiter](memory/controllers.md) is shown in the memory
inspector as two panels: instructions on the left (disassembled) and data on
the right, each at the latest address its side used. The memory
arrangements of [§ 4.7](rv32i/memory-systems.md) show it.

## Recorded time and game frames

Circuit scenarios are simulated before timeline playback. Moving the slider
restores recorded values; it does not advance a running game. The short game
scenarios of [§ 5.6](system/5.6.md#short-versions) are ordinary scenarios:
the whole machine runs the program when the scenario opens, with every
wire's history, which takes under a minute (Pong) to about three minutes
(Tetris) on the development machine.

The long games open as recordings: `?recording=.../playlist.json` shows the
machine's state at every presented frame, loading parts of 16 frames as you
move through the session. A full-tick capture (`ticks.json`) retains every
signal-change time instead, with PRESENT markers for jumping between frames;
propagation, register changes and memory writes can be inspected between
them. The six long games are listed in the scenario menu under
**Computer System / Games (long versions, recorded)**, one entry per game.
A game opens in Compact; the Fidelity menu offers the fidelities it was
recorded in (Compact and Visual) and marks the others "not recorded", and
switching keeps the frame (`&frame=N` in the address). Choosing a scenario
from a recording returns to the live visualizer.

A recording made with a machine save after every frame (all six long games,
in Compact and in Visual) opens in the simulator itself: the whole machine is
restored from the shown frame's save, so every wire, register and memory word
can be inspected, main memory included. In Compact the core is one block, so
its register file has no view of its own there; Visual shows it. The timeline then has two bars. The upper
one spans the whole game, one step per frame; dragging it or ▶| and |◀ jump
between frames (about a second each). The thinner one under it spans what has
been simulated since that frame: ▶ steps event by event and, when it reaches
the end, the page simulates 20 more cycles (about two seconds) and goes on,
through the next frame and the presses that follow it like any other event;
Play does the same continuously. The page keeps the last 600 to 900 cycles
of events (a Visual cycle records about 360 KB), so the event bar covers the
recent stretch. The event bar does not restart at the next frame: it keeps
growing across frames, and its oldest part is dropped as it goes, so its
thumb moves back when that happens. ◀ steps back through what is kept; at
its first event (a frame just opened, or the oldest kept event) it is
disabled, since nothing earlier has been simulated there; |◀ then goes to the
start of the previous frame. Opening a frame simulates its first 20 cycles.
At the program's end (its halt) ▶ stops. Opening a recording this way builds the
machine first, about 20 seconds. A step through a Visual machine moves about
174,000 values, so it takes a fraction of a second; a Compact machine (about
1,900 values) steps several times faster.

The time readout is exact. With a timing profile whose tick has a length
(embedded, desktop: 1 ps), it shows every digit down to the picosecond in the
largest unit the value reaches, grouped in threes, for example
`1.813 860 041 ms`, so neighbouring events a few picoseconds apart never read
the same; the unit profile shows ticks. In a recorded game the second line
gives the clock cycle and the time into it, for example
`cycle 403,077 + 41 ps`.

Simulation ticks use an explicit unsigned 64-bit type in the time-width
migration. Viewer data keeps ordinary numeric timestamps through 2^53−1;
larger ticks are exact decimal strings, displayed without rounding through
a JavaScript number. Timeline indexes remain ordinary numbers. This migration
and its long native/WASM recording checks are in progress.

## Fidelity and the explorer tree

The explorer tree is the planned fidelity. Expanded (▾) means "build this
part from its parts" (structural); collapsed (▸) means "build it as one
block" (behavioral). **Apply & Run** rebuilds the circuit to match the tree;
**Revert** returns the tree to what is built. The S/B badge on each row is
read-only and shows what is built and simulated now. A part with only one
form has a dot instead of an arrow.

The canvas follows the tree at once for parts that exist:

- A part you collapse is drawn closed immediately, before Apply: as its
  visual abstraction if it has one (a word gate's symbol with its width, such
  as `/32`, or a device view: display pixels, logical keyboard keys, console
  status, timer ticks, LED panel lights, FIFO state or a bus transaction),
  otherwise as a box marked `▸`. Its pins
  still show simulated values; its inner parts are hidden.
- A behavioral part shows its visual abstraction when available, even with
  Device View off; otherwise it is a plain block. When you expand it,
  it keeps that view, marked "Apply to build", until Apply & Run builds its parts;
  meanwhile the explorer lists those parts in gray, without badges, so you
  can also expand or collapse them before applying once.
- An expanded structural part shows its contents once it is large enough on
  screen.

Changing a part's fidelity changes that part only; choices for parts inside
it are kept and apply again when it is built from its parts. Selecting a
pin, wire or part inside a collapsed part shows it while selected. Hover and
selection reach only what is drawn.

**Device View** starts off. Turn it on to show existing visual abstractions
for structural as well as behavioral components: device panels, bus traffic,
gate symbols, register values, register-file tables, memory arrays and cell
values. Only the outermost abstraction on each branch appears. A display,
for example, hides its pixel memory and that memory's cells; separate devices
still show their own panels. Components without an abstraction retain their
usual view. All component outlines are solid; the explorer's S/B badges show
the built fidelity.

This button changes the canvas view immediately. It preserves the simulation,
timeline, fidelity draft and layout, and works with recorded circuits too.
Selecting a hidden descendant in the explorer focuses its visible abstraction;
boundary pins remain selectable. Turn Device View off to return to the
fidelity/tree view and inspect the descendants. Values come from the selected
timestamp; a recording without retained table data shows "State not recorded".

Selecting a device, structural or behavioral, opens a larger preview and
state summary in the inspector. Held keys, released keys and
unknown levels have distinct colors; queue head/count and overflow remain
visible even after a quick press/release. Display geometry and pixel masks
come from the configured component and recorded storage. Seeking backwards
reads history and does not issue device commands or resume execution.

The separate recorded-game page also has a display and keyboard panel. Its
playback controls inspect captured output; interactive simulation is a
separate mode being developed after milestone acceptance.
