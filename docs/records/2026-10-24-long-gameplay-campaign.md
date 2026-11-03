# Long gameplay campaign

Started: 2026-10-24. Status: closed (interrupted and superseded). Code state: `alpha` at `5549ee0`, retaining
the existing uncommitted component/layout work and short native record set.

## Summary

Five substantial sessions are prepared and frozen: 3,090 planned frames,
with full host C references and progress thresholds. The original compact
queue was stopped at the owner's request; partials and its frozen evidence
remain under `build-milestone/long-gameplays-20261024`. Replacement visual
runs are tracked in the [fidelity record](2026-10-24-fidelity-presets-and-visual-gameplays.md).
No full long native game from this original campaign is claimed complete.

## 2026-10-24 — Plan

| Game | Session target | Playing policy |
| --- | --- | --- |
| 2048 | Up to 512 moves | Evaluate legal moves for open cells, merges and board shape; natural terminal states start a new seeded board |
| Sokoban | Eight solved multi-crate puzzles | Generate solvable boards, solve pushes/walking routes, advance only after all goals are filled |
| Snake | 768 movement updates | Follow a safe board-spanning route, collect food and grow; direction inputs pass through the button device |
| Pong | 900 physics updates | Move both paddles, sustain rallies, include points and new serves |
| Tetris | About 600 controls/updates | Choose rotations/columns to reduce holes and height, include falling and drops, clear lines and handle top-out |

All decisions become deterministic button scripts. Actual guest C executes on
the RV32IM CPU with independently checked ISA, memory and devices. A separate
host C run produces the expected screens/reports. Reference preflight must
show substantive progress: merges/tile growth, solved puzzles, Snake growth,
Pong bounces/points, and Tetris placements/line clears. Do not fill duration
with idle/no-op updates or repeated short acceptance fixtures.

Feed finite input batches after PRESENT at safe falling-edge boundaries,
without permitting observation callbacks to drive or advance simulation.
Keep every presented frame and split circuit checkpoints into bounded parts
that open in the existing visualizer. No full gate/memory histories or new
whole-machine behavioral implementation. Existing short fixtures stay intact.

Runs are serial, against frozen binaries/ELFs/scripts/reference/layout tools.
Use a detached user systemd unit, 768 MiB hard memory limit, no swap, low
CPU priority, native address-space cap and host headroom guard. Wait for
headroom, stop only owned processes on pressure, retain failed attempts and
completed games. Resume skips verified completed games; a stopped simulation
restarts its game, since arbitrary hardware-state restoration is unavailable.

The measured short native runs imply hours to days per long game and several
days for the full queue. The background manifest, per-game progress logs,
captures, reference verdicts and completion/failure status must remain
inspectable after this turn. This task ends after verified preparation and
an actual healthy background launch; it does not claim those future runs pass.

## 2026-10-24 — Prepared sessions and implementation

| Game | Planned frames | Full host C preflight |
| --- | ---: | --- |
| Pong | 901 | 30 paddle bounces, four points, five rounds, 1,456 paddle moves |
| 2048 | 513 | 397 merging moves, largest tile 512, two boards, total score 6,404 |
| Sokoban | 303 | Eight solved three-crate levels, 76 pushes, 295 walking/pushing controls plus seven level changes |
| Tetris | 604 | 88 pieces, 29 line clears, 96 rotations, 165 lateral moves, two boards |
| Snake | 769 | 46 food pickups, length 49, 194 turns, one continuous board |

`prepare-long-gameplays.py` uses the existing C rules to choose moves, creates
solvable Sokoban levels, compiles separate long guests with pinned GCC 12.4,
audits every instruction for RV32IM and runs the entire guest/control/renderer
path against host device equipment. Reports and final fingerprints must agree
with the player model. At least 85% of updates must change the screen. The
old ten fixtures and their ELFs/input scripts are unchanged. These targets
are preflight/reference results, not completed native gameplay claims.

`RecordedGuestOptions::presentation_inputs` queues finite batches only after
PRESENT, beginning at the next falling-edge-plus-one boundary and sending
one transition per clock. It rejects mixed absolute/paced sources, invalid
buttons, oversized batches and pending prior input. The observer remains
read-only. The long mode allows 100 million cycles/instructions; ordinary
absolute-input limits stay at ten million. The native CLI selects FIFO depth
seven. Every input still crosses the actual device interface; no player or
renderer runs on the host in place of the guest CPU.

`LongGameplayProbe` reads an audited external ELF and input batches, disables
gate/register/memory histories and writes at most 32 checkpoints/24 MiB per
part. Every PRESENT is kept. The recording transport accepts a bounded
playlist index, retains topology and small metadata, and lazily releases/
loads checkpoint windows. Time/cycle/guest/topology identity is checked across
parts. One timeline supports Play and backward seek across the whole session.
No circuit changed, so no delay-table regeneration was needed.

Focused native compatibility/input checks pass (two CTests, 43.02 seconds),
including invalid paced controls. The real native long-mode preflight uses
two updates and forces one checkpoint per part, exercising three hardware
frames and three independently validated packets. All actual screens,
reports, masks, button levels and input logs match host references, with zero
setup violations. The original-canvas browser check passes all three native
pin/wire vectors, display/input states, cross-part seeks/rewind and Play.
JS contract checks cover legacy records/HTTP, wide times, malformed playlists,
concurrent state/inspector loading, part release/reload and mixed topologies.

The frozen campaign stores the native and Node binaries, guests, scripts,
reference images/reports, layout and packaging/validation tools. Later source
edits do not alter active runs. Completed games are rechecked by hashes before
resume skips them. Failed/interrupted attempts remain available; a native
restart begins that game again. The worker continues other games after a
game failure, retries host-pressure stops at most three times and never calls
partials verified. Exact native/reference checks and validated transport parts
are required before atomic publication. The index updates per game, avoiding
per-frame preview rebuilds.

Limits: one native process; 768 MiB service maximum, 640 MiB memory high/native
AS cap, swap disabled, CPU quota 90% of one CPU and nice 10. Start headroom is
1,536 MiB and the owned-process stop floor is 1,024 MiB. Each command is bounded
to seven days, native execution to 100 million cycles, and free disk below
1 GiB is a failure. The current sessions target hundreds of real updates,
not a fixed native completion time or delivered FPS.

## 2026-10-24 — Healthy background launch

The detached unit is `circuitsim-long-gameplays-1792818961.service`, main PID
3307552 at launch. Systemd reports active/running, 768 MiB MemoryMax,
640 MiB MemoryHigh, MemorySwapMax=0, CPU quota 900 ms/second and nice 10.
Initial total service memory was 177,823,744 bytes (about 170 MiB). Pong
advanced beyond 57,344 native cycles/16,641 retirements and one PRESENT
without an error. The queue order is Pong, 2048, Sokoban, Tetris, Snake.
This is a healthy launch, not completion of the five full native sessions.

The native preflight completed 98,648 cycles, 25,395 instructions and three
PRESENTs, with zero setup violations, 546.78 seconds execution and
158,760 KiB peak RSS. Exact host/reference and real three-part browser checks
pass; the final browser check reports 201,052 KiB maximum individual-process
RSS, not aggregate browser memory. The strict isolated site build passes in
97.93 seconds; the preview service's strict rebuild passes in 94.93 seconds.
Live checks verify the long index, queued/running manifest, public game page
and exact viewer module bytes. Earlier short recordings remain available.

The campaign freezes executable tools, binary/ELF/input/reference/layout
hashes and rejects changed frozen files. It records a heartbeat/progress
snapshot every 15 seconds locally, without triggering per-frame site rebuilds.
Results are published atomically only after complete native/reference and
transport validation. The public index links a single continuous playlist
for each completed game. Resume checks completed publication hashes before
skipping; it does not silently replace existing results.

Current progress: `build-milestone/long-gameplays-20261024/campaign.json`.
Progress/error/resource logs and retained partials are in `jobs/` there.
Preflight/build/native/browser/site evidence is under
`build-milestone/long-gameplays-preparation/` and the two smoke directories.
The public index is `/assets/rv32im/long-gameplays/index.html`. Commands:

```sh
python3 tools/rv32im/run-long-gameplays.py status --output build-milestone/long-gameplays-20261024
python3 tools/rv32im/run-long-gameplays.py stop --output build-milestone/long-gameplays-20261024
python3 tools/rv32im/run-long-gameplays.py resume --output build-milestone/long-gameplays-20261024
```

The queue intentionally remains running after this turn. No full long native,
WASM capture, expanded-core/cache campaign or live keyboard result is claimed.
No commit, push or remote deployment was performed.

## 2026-10-24 — Compact queue stopped

The owner requested a new `visual` campaign. Stopped
`circuitsim-long-gameplays-1792818961` through the campaign CLI and confirmed
inactive/PID zero. Pong's last progress was 2,445,312 cycles, 714,523 retired
instructions and 184 presentations, after 13,549.2 seconds. Four jobs remained
pending. The interrupted campaign manifest, raw partials and frozen binaries
are preserved; these are partial results, not a completed 901-frame Pong.
The replacement uses a separate `visual` publication directory and records
its actual preset/manifest. Earlier dated measurements above describe the
original compact campaign.

## 2026-10-24 — Handoff

Closed this original compact campaign as interrupted. The replacement visual
queue is active under `circuitsim-long-gameplays-1792838150`; its implementation,
validation and ongoing run state belong to the linked fidelity record. Closing
this record does not claim the interrupted game's full reference checks passed.
