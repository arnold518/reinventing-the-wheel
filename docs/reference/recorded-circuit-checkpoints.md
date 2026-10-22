# Recorded circuit checkpoints

This additive mode displays a completed scripted guest through the original
canvas, explorer, inspector and timeline. Native simulation produces the
capture; browser replay consumes JSON without constructing another CPU.
Existing native/WASM scenarios retain their normal transport.

## Capture

`RecordedGuestOptions::presentation_observer` runs after device checks and
propagation at a PRESENT boundary. It receives the real root, simulator and
settled observation time, and must not drive or advance the circuit. The
exporter checks that time, next-event time and event count are unchanged.

The borrowed-root `VisualizerSession` builds topology from that circuit.
`currentCheckpointJson(time)` captures indexed pin/wire values and available
device/register observations. It requires a time at or after the last event,
with no pending event at or before it. `deviceStateJson(..., current=true)`
reads current pins/storage; historical observation retains its existing checks.
Only retained PRESENT states exist. Intermediate gate transitions and memory
pages are not reconstructed. The native exporter bounds retained JSON to
24 MiB/64 checkpoints; guest execution keeps its existing budgets.

## Packet and replay

Schema 1, kind `circuitsim-circuit-recording`, contains:

| Field | Contents |
| --- | --- |
| `capturePolicy`, `sourceRuntime` | `presentation-checkpoints`, currently `native` |
| `topology`, `topologySlices` | Existing topology contracts, real identities and global signal indexes |
| `checkpoints` | Indexed state plus component-ID maps of inspector observations |
| `recording` | Checked guest frames, input, hashes and execution evidence |
| `presentation.layout` | Optional same-site layout-sidecar URL, relative to this packet |

`tools/package-recorded-circuit.mjs` validates the native packet and prepares
a layout sidecar containing only used types/profiles/child placements. It
inherits compatible root/configuration floorplans and fills missing structures
with editable grids. The original layout file is unchanged. Output files must
be new; a package receipt hashes the raw capture, prepared packet and layout.

Open the original page with `?recording=RELATIVE_URL`. Input is limited to
32 MiB, 64 checkpoints, 16,384 components, 262,144 pins/wires each and existing
pixel budgets. Validation checks dense indexes, widths, four-state values,
parent/pin ownership, exact unsigned time, and display/input agreement with
the checked guest frames. Captures/layouts must use the same site.

The adapter provides existing scenario/circuit/state/component-state/topology
and layout endpoints. Wiring loads progressively as the user zooms. Capture
layouts default to `topology_detail_min_pixels: 12`; normal layouts retain
their existing 1-pixel default. Layout edits use a recording-specific local
key. Fidelity/clock remain fixed. The display inspector opens initially;
selection, navigation, Device views, rewind and Play use the original UI.

## Verification scope

`RecordedCircuitTransportTest` covers validation, wide time, scoped topology,
layout behavior and the unchanged native HTTP path. `RecordedCircuitBrowserTest`
loads the real Pong capture, compares every retained pin/wire vector by
SHA-256, checks display/input, and exercises inspector, rewind, playback and
device-view toggle without a WASM simulation worker. The phase-6 record owns
dated results/resource limits. Native-to-browser Pong is verified; full game
coverage, WASM capture production/parity and live input remain separate work.
