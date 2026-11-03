# Full tick gameplay playback

Started: 2026-10-25. Status: closed (2026-11-03). Code state: `alpha` at `5549ee0`, preserving
the pending component, layout, profile and recorded-game work.

## Summary

The original visualizer now replays every signal-change tick through bounded
compressed chunks, including native memory and register state. Five focused
checks and a real 4,024-tick Visual browser guest pass. Every settled PRESENT
pin/wire/device/register state matches the independent checkpoint export.
The same approved 146-frame scripts are frozen for a new five-game campaign,
`circuitsim-gameplay-ticks-1792919551`. Snake is published. The worker was
OOM-killed on October 26 at 03:47 KST; no capture is currently running. Pong is
resource-stopped, 2048 is incomplete and Sokoban/Tetris remain pending.
Earlier completed frame recordings remain available at their original URLs.

## 2026-10-25 — Investigation and implementation boundary

The native `LongGameplayProbe` disables histories and exports only through
`presentation_observer`. The recorded transport builds its entire timeline
from those checkpoints. Snake's completed Visual run processed 4,796,953,473
events and 1,581,613,246 wire changes; naive per-tick full JSON snapshots are
impractical. The host has approximately 20 GiB free disk and variable memory
headroom. Streaming, compression, bounded decoding and a disk guard are
required. Existing approved short-game fixtures, actual Visual fidelity and
independent instruction/device/frame checks must be preserved.

The trace must retain every signal-change timestamp and final four-state
value at that timestamp, including delayed pin-only publications. Replaying
idle ticks holds the preceding state. PRESENT remains a navigation marker,
not the sampling interval. No simulation history may grow with elapsed time.

## 2026-10-25 — Implementation and focused verification

- Add optional scoped signal observation before the first native event. Engine
  histories remain disabled. Observe actual wire changes and explicit pin
  publications, buffering future output times and finishing complete event
  timestamps across small drain quanta. Auxiliary deltas capture native memory
  bytes/fills and register words, including decode-stage register aliases.
- Write packed four-state gzip chunks, at most 8 MiB or 8,192 rows per window,
  with independent snapshots. A 16 MiB decoded hard cap, bounded future state,
  row/chunk/signal limits and free-disk guard prevent unbounded retention.
- Add full tick transport to the normal canvas: scalar global timeline count,
  one decoded chunk, two topology scopes, lazy forward/backward seek and
  PRESENT markers. Device, timer/queue, register and generic memory-page views
  read the selected tick. Exact wide timestamps and existing frame-only modes
  remain supported. Accepted-bus transaction history/disassembly remain outside
  this export format.
- Validate every chunk and all PRESENT states before publication, with byte,
  geometry, time, index and SHA-256 checks. Add immutable native chunk hardlinks
  to MkDocs builds on the same filesystem, with ordinary copy fallback across
  filesystems, avoiding a second multi-gigabyte trace copy in local preview.

The serial native build passes. Five focused CTests pass in 1.10 seconds:
`SimulatorDrainUntilIdleTest`, `MemoryStorageTest`, `TickRecordingTest`,
`RecordedTraceAssetsTest` and `RecordedCircuitTransportTest`. The native trace
fixture compares all 1,202 retained-history ticks, including wide time, X/Z,
explicit wired/unwired pin histories, future publications, memory fills,
register words, cancellation, single-event quanta and chunk seek/rewind.

A 95-cycle, 29-instruction, two-PRESENT real Visual guest records 4,024 ticks
in 229,653 compressed bytes (1,774,302 decoded). Its current frozen-binary
repeat has byte-identical trace/pin-binding files and identical native results,
frames and event counters after excluding host timing. It runs in 6.58 wall
seconds, peak 287,380 KiB RSS, zero swaps. Packaging validates every PRESENT
pin, wire, pixel and device/register field, peak 178,916 KiB on the initial run.
This is a validation guest, not a complete gameplay or performance projection.

The original canvas browser check passes: all PRESENT states, intermediate
step/back, full slider range, marker jumps, generic RAM/register/device views,
Play and rewind; no page errors and no WASM module loaded. The successful
browser used a 512 MiB no-swap service scope and normal 1,536/1,024 MiB host
guards. Earlier attempts stopped safely under pressure; one assertion selected
the router's same-named RAM address window and was corrected to select root
RAM. Test-only lower-threshold attempts were also guard-stopped; background
campaign thresholds remain unchanged. Evidence is under
`build-milestone/tick-smoke-frozen`, `tick-browser` and the resource files.

## 2026-10-25 — Replacement campaign

The prior frame-only unit and its deferred retry are stopped; Snake 10, Pong
45 and Sokoban 34 remain published. New unit
`circuitsim-gameplay-ticks-1792919551` freezes the current native/Node binaries,
layout, transport/packagers and the same audited ELF/input/reference fixtures.
Order/targets remain Snake 10, Pong 45, 2048 25, Sokoban 34 and Tetris 32.
Publication is `assets/rv32im/gameplay-demos/visual-ticks`, separate from the
older checkpoint suite. All five must pass complete native instruction,
device, frame/report/input and trace checks before per-game publication.

Retain serial 768 MiB service / 640 MiB native AS caps, no swap,
1,536/1,024 MiB start/stop host guards and three bounded pressure retries.
Cap compressed traces across attempts at 12 GiB and stop below 1 GiB free disk.
No full game is claimed complete from the small trial. Strict site/live checks
and campaign startup are recorded below when finished.

## 2026-10-25 — Live deployment and queue startup

The strict MkDocs preview build passes in 59.13 seconds with the new trace
asset hook. Deploy the tested JS package to `build-wasm/pages/simulator`,
preserving the previous package at
`build-milestone/full-tick-visualizer-backup-20261025`, and restart preview.
The WASM binary and native Python module are unchanged. Ten live 8893
asset/route checks pass, including source hashes, index/manifest and native
gzip bytes. The gzip response has no Content-Encoding header, so the replay
decoder receives the intended compressed bytes.

Publish the small native guest at
`assets/rv32im/circuit-tests/full-tick-smoke/ticks.json`, with ELF, generator,
button script, exact-state package proof, browser receipt and source/binary
hashes. It contains two PRESENTs and 4,024 full signal ticks, explicitly marked
validation-only. The page links it for immediate manual inspection while full
games recapture. Its current frozen-binary output matches the browser-tested
trace and pin bindings byte for byte.

Start `circuitsim-gameplay-ticks-1792919551`. The unit is active, zero swap,
peak service memory 372,035,584 bytes in the first attempt. Snake produced two
bounded chunks (16,384 ticks) before a host-pressure stop. The worker is now
waiting for the required memory headroom before retry; the other four games
are pending. Those partials are not a verified recording, and their progress
does not describe the new waiting attempt. Old workers remain stopped. Native
full-game completion, portable capture production and broad suites are unrun.

## 2026-10-26 — Published game and preview storage recovery

The frozen campaign has published Snake's full-tick recording and is executing
Pong, with the remaining games queued. Large published traces exposed the
preview's tmpfs build-storage limit, causing an OOM restart loop before the
HTTP listener opened. The [separate recovery](2026-10-26-preview-memory-recovery.md)
moves the preview cache onto disk under the same memory cap and restores 8893.
Gameplay execution and frozen capture/packaging files were not modified or
restarted for that recovery.

## 2026-10-26 — Missing two-panel RAM inspection

The owner asks why device/game RAM lacks the instruction/data two-panel view.
Source comparison identifies two integration gaps. The full-tick adapter
returns `memoryRole: generic` and a single `windowWords` page, without
`instructionSide`/`dataSide` or instruction decoding. The normal canvas still
has its unified-memory renderer, selected only when those fields exist.

The native `MemoryInspector` also discovers unified context only beneath a
component named `SHARED_MEMORY`. `RV32IMDeviceSystem` names its backing endpoint
`RAM` and its two-port arbiter `RAM_ARBITER`, so that older name-based check
misses the new hierarchy. The hardware does share instruction/data RAM.
Supporting both paths requires correct arbiter/access context and decoded
instruction windows, including seek/rewind across trace chunks. This audit
does not change code, recordings or the running campaign; the parity gap
remains open rather than being described as a deliberate hardware difference.

## 2026-10-26 — Live campaign and profile coverage check

At the owner's status query the full-tick unit remains active, with no service
swap. Snake is verified/published: 15,404,996 signal-change timestamps in
1,881 chunks and 10 PRESENTs. Pong exhausted its bounded host-pressure retries
and is resource-stopped; 2048 is running, with Sokoban and Tetris queued.
Some job progress fields survive retries, so use the current attempt's own
`native/ticks-progress.json` rather than presenting old cycle/frame counts as
current progress. No workers are restarted or extra runs launched by this check.

These are the approved 146-frame demonstrations. The earlier long endurance
campaign remains stopped. Current capture fidelity is Visual; worker and
capture process environments leave `CIRCUITSIM_TIMING_PROFILE` unset, selecting
the engine's Unit default. The guest clock period is 1,000 ticks.

Each recording binds one actual fidelity build and timing model. Replay keeps
fidelity fixed and currently advertises/enforces Unit timing only. Other
fidelities require separate simulations; strict Gate cannot build this system's
full-sized RAM. The engine supports Embedded/Desktop through timing selection,
but the gameplay CLI does not expose a timing option and its replay metadata
currently assumes Unit. Supporting those captures requires explicit timing
metadata, profile-appropriate clock/input timing and validation before new
native runs. A different delay model cannot be recovered by merely rescaling
an existing trace. No multi-profile/timing gameplay matrix is running.

## 2026-10-26 — Worker termination found during status check

At 08:56 KST, the systemd journal shows an OOM-kill notification at 03:47:44
and the worker exiting with SIGKILL at 03:47:48, followed by a failed
`oom-kill` result at 03:47:50. The journal reports a 640.8 MiB peak for the
unit. No gameplay units or native capture/worker processes remain active.
This replaces the earlier live-running assessment above.

The campaign JSON was last updated at 03:43:52 and still says `running`, with
2048 `waiting-for-memory`; those labels describe the state before termination.
Snake's published manifest confirms 15,404,996 timestamps and 10 PRESENTs.
Pong remains resource-stopped, 2048 has no completed verified publication,
and Sokoban/Tetris have not started. Retain completed and partial artifacts.
No restart or additional capture was launched during this status check.

## 2026-10-26 — Snake execution cost audit

The successful full-tick attempt's resource receipts separate native capture
from postprocessing. Native wall time is 1:44:59; runtime execution is
6,297.604569 seconds, with 5,655.66 user CPU seconds, 5.23 system seconds,
89% CPU utilization, 294,312 KiB peak RSS and zero swaps. Checkpoint packaging
takes 4.88 seconds and complete tick validation takes 607.90 seconds. The
attempt's start-to-publication interval is 4h 36m 38s. About 2h 40m of that
interval is outside the measured commands, consistent with the worker's
memory-headroom waits before launching each command; it is not all simulation.

The published evidence records 282,854 CPU cycles, 70,123 retired instructions,
4,796,953,473 processed events (3,358,195,923 wire updates and 1,438,757,550
component evaluations), and 1,581,613,246 effective wire changes. Its
15,404,996 recorded timestamps group events at the same simulation time:
approximately 311 processed events and 103 effective wire changes per saved
timestamp. Native execution including streaming trace capture averages about
762,000 events or 45 guest CPU cycles per wall second. The trace occupies
1,562,020,666 compressed bytes and 5,944,225,365 decoded bytes.

The earlier checkpoint-only Visual Snake has the same ELF/input hashes,
profile fingerprint, cycles, instructions and event counters, but a different
runner and host conditions; its 9,908.921971-second execution is not a
controlled trace-overhead baseline. No CPU profile or controlled capture-on /
capture-off measurement was run here. The receipts establish the workload,
CPU-bound behavior and waiting costs, but do not apportion native time among
event scheduling, component propagation, reference checking and trace encoding.

## 2026-10-26 — Event-processing source audit

The owner questions the measured 762,000 events/second and expects roughly
10 million. Source inspection confirms that a processed event is not just a
constant-time gate operation. The simulator uses a `std::priority_queue` of
shared event objects: insertion/removal take O(log Q), with virtual priority
comparisons for equal timestamps. Snake's maximum queue depth is 5,969.

`WireUpdateEvent::process` constructs a temporary logic vector even before
the unchanged-value check, and `processWireUpdateEvent` obtains another vector
from the wire for that comparison. Changed wires obtain values again during
fanout propagation. `propagateWireChange` scans sinks to remove expired weak
pointers, locks pin/owner pointers, uses dynamic casts, copies values into
pins and allocates/schedules evaluation or hierarchy-crossing wire events.
The shown scheduling path does not coalesce repeated evaluation requests for
the same component/time. Cost depends on signal width and fanout in addition
to queue size; the fixed-width bounds do not make the constants cheap.

The streamed trace adds pointer-keyed signal lookup, value packing and ordered
maps for pending timestamps and changed signal IDs, then encoding and gzip
work. These remain active despite retained simulator histories being disabled.
Counters show 1,776,582,677 wire-update events without an effective wire change,
52.9% of all processed wire updates. The existing unchanged-value guard avoids
propagation for those events, but only after queue processing and vector
construction/comparison.

The measured wall cost is about 1.31 microseconds/event; 10 million/second
would require 0.10 microseconds/event. The source supplies concrete optimization
candidates, not their measured CPU shares or a demonstrated 13-fold speedup.
A bounded CPU profile and matched trace-enabled/disabled measurements are
still needed before selecting engine, scheduler or trace changes. No benchmark,
compile, event-semantics change or campaign restart was performed by this audit.


## 2026-11-03 — Closed: campaign superseded

The queue `circuitsim-gameplay-ticks-1792919551` ran the retired
146-frame demonstration scripts on the retired device system in Visual. It
was not resumed. The long games are now goal-based sessions of new fixtures,
recorded at one checkpoint per frame in Compact with embedded timing and a
4.5 ns clock ([the game scenarios record](2026-11-03-game-scenarios-plan.md)).
Full-tick capture itself is kept: `LongGameplayProbe --ticks` and
`run-long-gameplays.py launch --ticks` still produce it, and
`TickRecordingTest` / `TickRecordingBrowserTest` still check it. The
published Snake full-tick capture and its paths under `assets/rv32im/` were
removed with the retired chapter earlier; no capture of the new fixtures was
made.

Record closed 2026-11-03.
