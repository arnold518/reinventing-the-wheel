# Recorded circuit playback

This mode displays a completed scripted guest through the original
canvas, explorer, inspector and timeline. Native simulation produces the
capture; browser replay consumes JSON without constructing another CPU.
Existing native/WASM scenarios retain their normal transport.

## Full signal timeline

`LongGameplayProbe ... [profile] --ticks` subscribes before the first event,
keeping gate and memory histories disabled. It streams every effective wire
and pin-only publication, native memory-byte changes and behavioral register
file updates. Register-state parent views share the child's updates. Future
pin publications are buffered until the event frontier passes their time;
single-event drain quanta never expose an unfinished timestamp. Multiple
changes at one time retain the final value, matching the normal visualizer's
unique-timestamp timeline. Time zero and each settled PRESENT are retained.
Idle intervals hold their preceding state.

The schema-1 `circuitsim-tick-recording` index has policy `all-signal-ticks`:

| Field | Contents |
| --- | --- |
| `presentations` | Checked checkpoint playlist, used for frames, geometry and provenance |
| `timestampCount`, `chunks` | Full timeline length and contiguous chunk index ranges |
| `pinSources` | Pin-to-wire bindings; explicit pin-only histories take precedence |
| `auxiliary` | Bounded native memory-byte and register-word state, with component owners |
| `markers` | PRESENT indexes and exact times in the full signal timeline |

Each gzip chunk begins with `CST`, byte version 2, and unsigned varints for
pin, wire and auxiliary counts. Its complete baseline stores two bits per
logic bit, least significant first (`0`, `1`, `X`, `Z` are 0–3), padded with
zeroes to whole bytes per signal. Each following row stores absolute uint64
time, change count and sorted delta-coded signal indexes with packed values.
Global indexes are pins, then wires, then auxiliary slots. A chunk baseline
precedes its first row. Independent baselines permit seeking without loading
earlier chunks. Time values above JavaScript's exact integer range stay strings.

The writer flushes at 8 MiB or 8,192 rows, with a 16 MiB hard decoded limit;
pending future changes, signal counts, total rows/chunks and free disk also
have explicit limits. Replay retains one decoded chunk and two topology
scopes. It uses a scalar timeline count, avoiding millions of timestamp JSON
entries. Device pixels, queues, timer, registers and generic memory pages use
the selected tick's state. Detailed accepted-bus transaction history and
instruction disassembly are not exported by this recording format.
MkDocs links immutable game trace chunks on the same filesystem instead of
duplicating their bytes; builds on another filesystem use ordinary copies.
`tools/mkdocs_recordings.py` applies only to native tick chunk assets and
leaves other static files unchanged.

`tools/package-tick-recording.mjs` validates every compressed chunk and checks
all PRESENT pin/wire/device/register values against the independent checkpoint
export. It adds SHA-256 digests for chunks and pin bindings. The browser checks
digests, byte/count/time/width limits and four-state encoding during lazy reads.
Open `?recording=.../ticks.json` in the usual canvas. Play/step/rewind traverse
signal ticks; checkpoint arrows jump to PRESENT markers. Fidelity is fixed to
the actual captured build.

Existing checkpoint recordings keep their original granularity. Converting
their metadata cannot recover discarded transitions; full-tick playback
requires another native run. The frame-only `games.html` viewer was retired on
2026-11-03; recordings open in the visualizer, which lists the published
long games in its scenario menu.

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
In this older capture policy only retained PRESENT states exist. Intermediate gate transitions and memory
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
selection, navigation, opening/closing parts, rewind and Play use the original UI.

## Long-session playlists

Schema 1, kind `circuitsim-circuit-playlist`, contains `guest`, `parts` and
`checkpoints`. Parts have a same-site relative `file`, contiguous `start`
and `count`, and a publication SHA-256. Checkpoint metadata gives exact
`time` and native CPU `cycle`. At most 64 parts and 1,024 checkpoints are
accepted. Every part remains a normal bounded circuit recording. The native
long exporter uses at most 16 checkpoints/24 MiB per part, retaining every
PRESENT rather than sampling frames away.

The original recorded transport builds one full-session timeline, loads
parts serially on demand and releases the old checkpoint window before
fetching another. It retains topology, the initial state and small index
metadata. It checks topology identity, guest, counts, exact times and cycles
across parts. Backward seek reloads the required part. The standalone-record
and normal HTTP/WASM paths remain available.

`RecordedGuestOptions::presentation_inputs` is finite test equipment: one
batch after each PRESENT, starting at the next safe falling-edge boundary,
one transition per clock. It is exclusive with absolute-tick inputs. The
previous queue must be empty, batches must fit the selected FIFO, all buttons
and counts are bounded, and all events must be sampled before successful halt.
Observation callbacks remain read-only. This explicit mode permits up to
100 million cycles/instructions; absolute-input runs retain their 10-million
limit. The current native long probe selects a seven-entry input queue.

## Verification scope

`RecordedCircuitTransportTest` covers validation, wide time, scoped topology,
layout behavior and the unchanged native HTTP path. `RecordedCircuitBrowserTest`
loads a real long-game playlist (see `docs/guides/tests-and-scenarios.md`),
compares every retained pin/wire vector by SHA-256, checks display/input, and
exercises inspector, rewind, playback and device-view toggle without a WASM
simulation worker. The published long games
(`site-docs/assets/system/long-games/`, 2026-11-03 onwards) each carry an
`evidence.json` with exact host frame/report/input checks, the native exporter,
the source manifest, ELF/script/frame hashes, resource limits and peak native
RSS. These captures keep gate and memory histories disabled. The short game
versions are live scenarios, not captures. WASM capture production and live
input remain separate work.

## Shared topology for structural game recordings

Schema 2 recording parts replace inline `topology`/`topologySlices` with a
same-site `topologyRef`. The index has schema 1, kind
`circuitsim-recorded-topology`, a normal topology header, and `scopes` entries
with `file`, `ownerIds` and publication SHA-256. Native export streams chunks
of at most 512 owners; a full visual game currently has 26,833 components.
The index permits 65,536 components and 128 scope files. Signal limits remain
262,144 per kind. Each file and recording part remains bounded to 32 MiB.
The actual build manifest is stored once beside the graph, rather than copied
into every part. Existing schema 1 files keep their 16,384-component limit.

Validation reads scope files serially, checks complete unique owner coverage,
dense signal indexes, repeated definitions and wire endpoints, then retains
only signal widths, component metadata and scope fingerprints. Replay caches
at most two scope files plus one checkpoint window and the initial state.
Publication receipts retain SHA-256; runtime scope-change fingerprints detect
accidental mutation and do not claim cryptographic authentication. A new part
must refer to the same graph URL and match the playlist's times and cycles.
This preserves structural detail and every PRESENT without loading the full
wire graph or all gameplay states together.
