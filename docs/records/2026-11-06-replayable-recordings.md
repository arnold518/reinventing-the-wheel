# Replayable long-game recordings: saved machine state and replay

Started: 2026-11-06. Status: open (plan; implementation started). Code state:
`alpha` at `96ef121` plus the uncommitted Visual long-games work of
[the Visual long-games record](2026-11-03-visual-long-games.md).

## Summary

A whole simulated machine saves and restores exactly (`circuit::state`,
[reference](../reference/machine-state-saves.md)). The six long games are
published in Compact and in Visual with a machine save after every frame
(Compact 35 MB of saves, Visual 89 MB), and such a recording opens in the
page's simulator: the frame bar jumps between frames, ▶ and Play step event
by event, simulating on as needed, and main memory (and in Visual the
register file) is inspectable. Campaigns resume from their last save after
an interruption. On 2026-11-10/11 both sets were re-recorded on the SRAM
main memory ([SRAM record](2026-11-09-sram-main-memory.md)); all saves are
now version 2, `PublishedSavesTest` checks that published saves still open
in the current build, and the page shows the recorded frames when a save no
longer fits. Open: ◀ cannot go back past a frame's start.

## 2026-11-06 — Why the current recordings cannot do this

- A frame is `VisualizerSession::currentCheckpointJson`: every pin and wire
  value plus the state of parts with a device or register view (core
  registers, display with video memory, queues, timer, bus). Main memory
  (`MACHINE.MEMORY.RAM`, a 65,536-word behavioral part) stores its bytes in a
  native array, not on wires, and its inspector needs memory history, which a
  long run disables; the recording says "Memory pages were not captured".
- Storing every event costs about 5.7 KB per cycle in Visual (old Visual Snake
  full-tick capture): about 12 GB for Pong, 50 GB for the six games.
- The simulation is deterministic: the program and the button batches (a few
  KB per game) decide every event. Storing causes and recomputing effects is
  the economical form.

## 2026-11-06 — What a complete machine state is

Read from source (`include/simulator`, `include/basic`, `include/components`,
`src/runtime/RecordedGuest.cpp`):

1. **Simulator:** current time, the scheduling-order counter and the event
   queue. There are two event kinds only: `WireUpdateEvent` (wire, packed
   value) and `ComponentEvalEvent` (component); each queued entry has time,
   priority and order. Cancelled events are dropped.
2. **Wires and pins:** each `WireBase` and `PinBase` holds a `PackedLogic`
   value.
3. **`BasicComponent` timing state:** `TimedOutputs` (input values at the
   last evaluation, when each input last changed, pending publish times,
   delay-twin state) and `pending_changes_` (inertial-delay output changes,
   linked to queued events).
4. **Each stateful part's own members:** flip-flops, latches, registers, the
   register file, behavioral memory arrays, FIFOs, the compact device models,
   the iterative multiplier/divider, the behavioral core's pipeline.
5. **The checked runner** (`runChecked`): ISA reference state and memory, the
   bus RAM model, the device model (video, console, buttons, timer, LEDs),
   the outstanding transaction, completed transactions, input cursors and the
   result accumulators.

Components are walked in a fixed order (pre-order over `getChildren()`, each
component's input then output pins in name order, then its own wires), so
two builds of the same machine under the same profile number every pin,
wire and component identically. A save names the profile fingerprint and the
count of each kind, and a restore refuses a different machine.

## 2026-11-06 — Plan

**Phase 1 — engine save and restore (native).**
- `circuit::state::Writer/Reader`: a versioned little-endian binary format.
- `Simulator::saveState/restoreState`: time, order counter, queue (events as
  kind + pin/wire/component number + value).
- Wire and pin values; `BasicComponent` timing and pending inertial changes
  (as queue references).
- `virtual StateSupport stateSupport() const` on `BasicComponent`, default
  `Unknown`. A part declares `Stateless` (everything is in its pins, wires
  and base timing state) or overrides `saveState/restoreState`. Saving a tree
  that contains an `Unknown` part fails and names its type, so a forgotten
  member cannot silently break a replay. Every leaf type in the Compact and
  Visual game machines is audited and declared.
- Proof (CTest, Compact and Visual, a short game): run A to cycle T2; run B to
  T1, save, build a fresh machine C, restore, run C to T2. The save of A at T2
  must equal the save of C at T2 byte for byte (a byte-equal save is an equal
  complete state), and every frame after T1 must match.

**Phase 2 — runner save and resume.**
- `RecordedGuestOptions`: `save_every` cycles with an `on_save` callback, and
  `resume` (a save) to start from. The runner's own state joins the save, so a
  resumed run keeps every check (ISA reference, device model).
- Proof: a long fixture resumed from a mid-game save produces the same
  frames, report and final save as the uninterrupted run.

**Phase 3 — recorder and campaign.**
- `LongGameplayProbe --save-every N`: writes gzip saves beside the parts and
  a save index (cycle, time, frame, file, size, SHA-256). Frames gain main
  memory as the pages written since the previous frame (256-byte pages).
- The campaign worker restarts an interrupted job from its last save instead
  of cycle 0 (the save holds the runner's checks too).
- Measure save size and spacing on one game; choose the spacing so that the
  whole set stays within about 250 MB and a seek simulates at most a few
  thousand cycles.

**Phase 4 — replay in the browser.**
- The WebAssembly build gains a replay scenario: build the game machine in
  Visual, restore a save, run a window of cycles with history, and show it in
  the normal visualizer, where play and step go event by event and memory
  pages are inspectable.
- The recording viewer gains "Step through events" at a frame: it opens the
  replay from the nearest save at or before that frame.
- Each replayed frame is compared with the recorded frame; a difference is
  reported, not hidden.
- Measure Visual speed in the browser and the window's memory.

**Phase 5 — re-record and publish.** A new Visual campaign of the six long
games with saves and memory pages (about 30 hours), published as the Visual
set. § 5.6, `site-docs/visualizer.md`, a reference page for the save format
and the tests guide are updated.

Limits stated up front: a save is valid only for the exact runner build,
profile and machine that wrote it; the replay does not hide a mismatch. The
browser window is bounded by memory (history is kept only from the restored
save onward).

## 2026-11-06 — Schedule

Implementation starts now while the current Visual campaign finishes Pong
(about 04:30). Until then builds run inside a memory-capped scope so the
campaign's memory guard is not tripped. The new campaign starts once phases
1–4 pass their checks.

## 2026-11-06 — Implementation (phases 1–4 written, not yet built)

- **Engine** (`MachineState.hpp/.cpp`, `StateArchive.hpp`): the save layout of
  [the reference](../reference/machine-state-saves.md). `Simulator` gained
  `queuedEvents`, `restoreScheduling`, `headlessPinStates` and `seedHistory`.
  Values are a bit stream plus a list of values with unknown bits; a part with
  no delay table and nothing pending costs one flag byte.
- **Declared parts** (every leaf kind of the Visual game machine, read from
  the published Visual topology: 36 kinds): gates, `Rewire`, bit adapters,
  constants, word gates and `GateListCompact` are stateless;
  `RegisterBehavioral`, `ClockedStateBitBehavioral`, `FlipFlopCellBehavioral`,
  `RegisterFileBehavioral`, `GateListPartBehavioral` (counters, FIFOs, device
  front ends, register ports, bus routers), `RequestKnownBits` and
  `BehavioralMemoryEndpoint` (FF technology: handshake, response buffer,
  access sequence, device model, storage) save their members. The Compact
  machine's behavioral core and the other memory technologies stay
  undeclared, so saving them fails by name.
- **Runner**: `save_at`/`on_save` (machine only, or with results), `resume`,
  `stop_after_cycles`; the runner's models join the save; its two reference
  memories are stored as differences from the machine's RAM.
- **Recorder and campaign**: `LongGameplayProbe --save-every N` and resume
  saves at each part; `--resume`. `run-long-gameplays.py launch --save-every N
  --replay-window W` publishes `saves/` with an index; an interrupted game
  continues from its resume save in the same attempt.
- **Browser**: `GameReplayScenarioTest` (`game-replay`, Visual only) built
  from a save set by `setGuestReplay` (WebAssembly `setGameReplay`); the
  worker fetches and decompresses the save; the recording viewer gains "Step
  through events" (nearest save at or before the shown frame) and the replay
  "Back to the recording".
- **Decision (replaces plan item 3's memory pages):** frames do not gain
  memory pages. Every save holds the whole RAM, and the replay (one click
  from any frame) shows memory with its history; adding pages to every frame
  would repeat the same bytes in each part.

## 2026-11-06 — First checks and measurements

All on `build-milestone` (Release), embedded timing, 4.5 ns clock, Visual.

- `MachineStateProbe pong-short --save 2500 --compare 5000`: PASS, saves at
  cycle 5000 equal byte for byte. `smoke-long --save 20000 --compare 22000`:
  PASS. First attempt failed (x1 unknown after the restore):
  `stateRestored` was declared but never called, so the register file kept
  its construction-time history entry; fixed in `restoreMachine`.
- The replay scenario natively (`SimulationTestRunner GameReplayScenarioTest`
  with `CIRCUITSIM_REPLAY_SAVE`): first refused, correctly, because its root
  was named `GAME_MACHINE` and the recording's `MACHINE` (the structure hash
  includes names); now built as `MACHINE`. 1,000 cycles from the smoke
  save at cycle 20,000 with full history: 21.7 s, 514 MB peak (the machine
  alone is about 155 MB), so history costs about 360 KB per cycle.
- Save size: 1.73–1.79 MB raw, 70–90 KB gzip. Wires and pins are 15 KB and
  25 KB of bits (7 KB compressed); nearly all the rest is the per-part timing
  state of the compact parts (`CIRCUITSIM_SAVE_SIZES=1` lists parts over
  2 KB): each 32-bit register about 7.5 KB (its gate-list twin keeps a value
  and a change time per gate node), each device register port about 54 KB,
  the button FIFO 61 KB. `zstd --patch-from` against a save 2,000 cycles
  earlier gives 41 KB instead of 89 KB; not worth a decoder the browser lacks.
- Smoke campaign with `--save-every 5000 --replay-window 3000` into a scratch
  publication: passed every host-reference check; nine saves published with
  their index (87 KB each).
- Resume: `LongGameplayProbe smoke-long --part-frames 1` stopped after its
  first resume save (cycle 23,157) and continued with `--resume`: the result
  equals the uninterrupted run's (3 frames, 54,519 cycles, 11,067
  instructions, report, console, inputs, device reads, event counters). The
  first resume crashed: the circuit view was created only with the topology,
  which a resumed run already has; fixed.

Decision: saves at frames, not every N cycles. With 360 KB of history per
cycle, a browser replay is about 1,000–2,000 cycles; saves every 1,000
cycles would be about 8,900 saves and 760 MB. One save right after each
frame (`--save-at-frames`) is 921 saves, about 80 MB for the six games, and
"Step through events" then starts at exactly the frame shown. A frame save
takes precedence over a resume save on the same cycle.

## 2026-11-06 — Browser replay

`build-wasm-milestone` (Emscripten, Release), served locally with a
smoke-long save beside the simulator; `GameReplayBrowserTest.py` with
`CIRCUITSIM_REPLAY_QUERY`.

- First attempt refused the save ("a run of another program, input or
  options"): the run identity hashed `size_t` values, 8 bytes natively and 4
  in WebAssembly. Every value is now widened to 64 bits before hashing.
- 300 cycles: opened in 28 s, 62,638 timestamps; register file and main
  memory readable mid-window (memory history restarts at the restored
  contents). 1,500 cycles: 75 s, 302,474 timestamps, Chrome processes about
  1.3 GB at the peak.
- Window chosen: 1,000 cycles (under a minute here); "Continue" runs the next
  window from the save the simulator makes where the last one ended. A save
  taken when a run stops equals the uninterrupted run's save at that cycle
  (`MachineStateProbe`, run B now stops at the compare cycle).
- Regression checks: `GamePongScenarioTest` (13,324 cycles, 3 frames),
  `ComputerSystemScenarioTest`, `RV32IMRecorded_pong-short_Test`,
  `GameReplayScenarioTest` pass.

## 2026-11-06 — Replay checked on a real recording (staged Sokoban)

The finished Sokoban recording of the saves campaign, served from its staging
folder beside the built simulator (local port 8932), in headless Chrome:

- The recording shows "Step through events"; pressed at frame 4 it opened
  the save taken right after that frame (`save-84579.bin.gz`) and ran 1,000
  cycles in 52–80 s: 213,886 timestamps. Consecutive steps are 175, 89, 11, 2,
  48 … ticks apart (clock period 4,500): event by event.
- Main memory: the RAM block on the canvas shows the instruction side
  (disassembled, following fetches) and the data side; its inspector shows
  the live transaction. Touched words go from 0 at the restore to 8 at the
  end of the window. The word at 0x17D0 changes from 00000003 to 00000004
  between two consecutive steps (ticks 381,724,863 → 381,726,000), the store
  of `sw x15, 20(x10)`; screenshots of both steps match. The register file
  is readable at every step.
- "Continue" (200-cycle windows): the next window starts at cycle 84,779, at
  the tick where the first ended (381,518,999).
- Fixed on the way: the two replay buttons stacked and the second was cut
  off (now one row; "Continue" names its cycle count in its title), and the
  scenario menu was blank during a replay (it now shows "Replay · <game>").

## 2026-11-06 — One menu entry per long game; Fidelity chooses the recording

Owner request: show Compact and Visual in one scenario, using the Fidelity
menu. The recording menu now has one entry per game (opening in Compact);
in a recording the Fidelity menu enables the presets the game was recorded
in (read from each set's manifest `fidelityPreset`) and marks the others
"(not recorded)"; choosing one opens the same game's other recording at the
same frame (`&frame=N`). Checked on preview 8893 with Pong: six entries,
Compact → Visual at frame 6 and back, no page errors.
`VisualizerScenarioBrowserTest` passes (its expected menu now leaves out
hidden scenarios). § 5.6 and `site-docs/visualizer.md` describe it.

## 2026-11-06 — One timeline: frames and events in the ordinary interface (replaces the replay buttons)

Owner request: the same interface as a normal scenario, ▶| moving through
frames and ▶ through events, no "Step through events" or "Back to the
recording", memory visible in the recording. Replaces the separate replay
mode of the sections above.

- **Engine:** the runner reuses its host wires on a machine that already has
  them, skips loading the image when resuming, and can stop at the cycle it
  starts on. Run saves are version 2: the machine section carries its length,
  so `resume_in_place` takes only the runner's part (the session continues on
  the machine as it is). Version 1 saves (the saves campaign of 04:47, whose
  runner was frozen before this change) are still read for full restores.
  `circuit::state::restoreValues` puts the wire and pin values and the time
  back to a save after viewing history moved them.
- **Session:** `GuestScenarioTest::gameAdvance(cycles)` (in place from the
  save made where the last run stopped) and `gameJump(save)` (full restore
  into the built machine); `VisualizerSession::gameAdvanceJson/gameJumpJson`
  rebuild the timeline (dropping ticks before the restored history) and
  return it. WebAssembly `gameAdvance`, `gameJump`; worker routes
  `/api/game/advance` and `/api/game/frame`.
- **Proof** (`GameSessionProbe`, in `GameReplayScenarioTest.py`): a session
  stepped 100 + 50 + 150 cycles with history viewed between, and one that
  jumped back to its save and ran 300 cycles, end with saves byte-equal to a
  single 300-cycle run (Pong short, and a real version 1 Sokoban save).
- **Viewer:** a Visual playlist with saves opens in the simulator
  (`game-session`): the frame bar (new, upper) spans the game; |◀ ▶| and the
  frame bar restore frame saves; the event bar under it spans the stretch
  simulated since the frame; ▶ and Play simulate 20 more cycles at its end.
  The Fidelity menu still switches to the Compact recording at the same
  frame. The replay buttons and "Continue" are removed.
- **Browser** (staged Sokoban, port 8933, `GameReplayBrowserTest.py` with
  `CIRCUITSIM_GAME_RECORDING`): PASS — opened at frame 4 in 21 s; 60 event
  steps 1–31 ticks apart; register file and main memory readable; ▶| to
  frame 5; Compact at frame 5 and back. Times in the page: jump 1.2 s,
  20 more cycles 1.7 s, one step 0.4 s (the Visual state has about 174,000
  values).

## 2026-11-06 — ▶ across frames; a rolling history window; the program's end

Owner question: ▶ is a general "one event" step; reaching the next frame it
must go on into that frame's events.

- ▶ never restores a save: at the end of the simulated stretch it simulates
  more cycles of the same machine in place, so a PRESENT is crossed like any
  other event and the next batch of presses is delivered by the runner as in
  the recording. The frame bar and status follow (`gameFrameAt`).
- History would grow about 360 KB per cycle and frames are 4,800–46,000
  cycles apart, so a session keeps a rolling window:
  `Simulator::trimHistoryBefore(start)` drops history before `start` (each
  wire and pin keeps its value then, re-timed to `start`; vectors give back
  their capacity), together with older checkpoints. Default window 600
  cycles, trimmed when the stretch passes 900
  (`CIRCUITSIM_GAME_HISTORY_CYCLES`). Machine state is untouched:
  `GameSessionProbe` with a 40-cycle window still ends byte-equal to the
  single run (kept history about 10,000 timestamps per stretch).
- When the program halts inside a run, the session reports `ended`; ▶ and
  Play stop at the end, and the status names the halt.
- Browser, staged Snake, frame 5 → 6 (7,029 cycles apart), simulated in
  100-cycle steps: the "frame 6" mark and the next presses appeared, ▶ went
  on past them, the frame bar and status moved to frame 6, no page errors;
  kept history 100,000–160,000 events; Chrome processes at 1.3 GB after
  opening, then 1.36–1.49 GB for the 6 minutes of simulation (peak 1.5 GB;
  1.7 GB before trimmed vectors released their capacity and with a
  1,000-cycle window).

## 2026-11-06 — "No events between frames" on the owner's proxy

The owner saw 2048 frame 75 (`…/proxy/8933/…?recording=…2048-long…&frame=74`)
with no events between frame marks, which is the frames-only recording
mode. Locally the same URL opens the game session (status ready, 2
timestamps, three ▶ steps simulate to 4,141). The page's scripts had kept
their cache-busting versions all day (`app.js?v=82`, `wasm_worker.js?v=20`,
`circuitsim_wasm.js?v=13`), so a browser could keep pre-session copies.
Bumped: `styles.css` 35, `transport.js` 22, `app.js` 85, `wasm_transport.js`
21, `wasm_worker.js` 21, `recorded_circuit_transport.mjs` 5,
`circuitsim_wasm.js` 14 (locateFile 12), `scenario_catalog.mjs` 2; the 8933
preview server now sends `Cache-Control: no-cache`.

Also, opening a frame or jumping to one now simulates its first 20 cycles
at once (about 2 s), so its events (and the presses that follow the frame)
are there to step through, instead of the restored moment alone.
`GameReplayBrowserTest` (staged Sokoban): PASS, opened in 24 s with the first
cycles simulated.

## 2026-11-06 — The end of the event bar, and ◀ at a frame's start

- Bug found and fixed: when the step that simulates 20 more cycles also
  trims the history, every event's index shifts; the step then used its old
  index + 1 and landed near the end of the new cycles. It now finds the next
  event by time. Browser check (2048 frame 75, stretch grown to 890 cycles,
  then ▶ at its end): the stretch start moved 7,220,873,496 → 7,222,270,499,
  events 187,050 → 127,807, and the step moved 2,017 ticks, to index 123,000
  (just after the old end). Stepping off the end of a fresh or trimmed
  stretch, and after dragging the bar to its end, moved 11–2,017 ticks.
- The event bar does not restart at the next frame: it keeps growing across
  frames (the frame bar and status follow), its oldest part trimmed.
- ◀ at the first kept event did nothing silently; it is now disabled there
  with the reason in its title. Going back past a frame's start would mean
  simulating the previous frame's whole stretch (4,800–46,000 cycles, minutes
  in the page), so |◀ goes to the previous frame's start instead.

## 2026-11-07 — The event bar spans the current frame

Owner report: ▶ at the end of a frame did not move well to the beginning of
the next. Stepping across Snake's frame 6 PRESENT event by event showed the
machine crossing correctly (2.249 → 2.250 → 2.280 ns into cycle 64,017; the
frame bar switched at the "frame 6" mark) but the event bar continuing near
its right end (step 123,226 of 124,344), since it spanned everything kept.

The event bar now spans the current frame by time, from its PRESENT to the
next frame's, with the simulated and kept part shaded (`--sim-from`,
`--sim-to`) and marks placed by time; dragging picks the kept event at or
before that time. Same crossing after the change: at the PRESENT the bar
read 0 of 33,403,500 ticks (frame 6 → 7), then 1, 31, 42 …; 33 steps back
returned to frame 5's bar at 794 ps before its end (31,629,706 of
31,630,500). `GameReplayBrowserTest` (staged Sokoban): PASS.

## 2026-11-07 — Seeking to any moment of the frame

Owner report: moving to an arbitrary time on the event bar did nothing
outside the simulated part. Now a release there seeks: before the kept part
the frame's save is restored; then the session runs without history in
chunks of up to 500 cycles (`GuestScenarioTest::gameFastForward`: history
off, run in place, then a full restore of where it stopped with history on,
marked "simulated to cycle N") to 300 cycles before the target, then with
history in 100-cycle chunks, and shows the event at or before the target.
The status line shows the progress.

- `GameSessionProbe`: 150 cycles fast-forwarded then 150 with history end in
  the single run's state: every wire, pin, part, memory byte, the runner's
  state, time, scheduling order and queue equal; only the activity counters
  and the history-off pin record (observation only) differ, which the probe
  now leaves out of that comparison (it found its own bug on the way: the
  machine section's length prefix follows the pin record).
- Browser (staged Sokoban frame 4, 35,140,500 ticks = 7,809 cycles): seek to
  25 % (about 1,950 cycles ahead) took 157 s and landed 919 ps before the
  target; seek to 10 % (before the kept part: frame save, then about 780
  cycles) took 52 s and landed 32 ps before; ▶ then moved 52 ps; no errors.
  Without history the Visual machine runs about as fast in the page as with
  it (12–15 cycles per second), so a seek costs about 1 s per 13 cycles: up
  to about 10 minutes in the middle of a long 2048 frame.

## 2026-11-07 — What made the session slower, and the fixes

Owner question: the fast forward used to work, why the lag now. Measured in
the page (staged Sokoban): ▶| took 4.2 s (1.2 s before), because the jump
had been made to simulate the frame's first 20 cycles before showing it;
and every advance resent the whole kept timeline (650 KB at 65,000 events,
growing to about 200,000) and rebuilt it from every wire's history in a set,
then the page rebuilt its bar (1.6 s at 65,000 events). The simulation
itself is about 16 cycles per second in the page with or without history,
plus about 1 s per request for the save at the end of a run and the next
run's setup.

- An advance answers with its change only (`dropped`, `front`, `added`);
  the simulator finds the new ticks from the end of each history
  (`Simulator::uniqueTimestampsAfter`); the page checks the count and keeps
  the same list.
- A jump (and opening) shows the frame first and simulates its first cycles
  after; simulator requests are queued (`gameQueue`), so a step pressed
  meanwhile waits instead of being dropped.
- After the change: ▶| 1.9 s including the following 20 cycles, an advance
  of 20 / 100 / 300 cycles 2.4 / 6.8 / 19.1 s. `GameReplayBrowserTest` and
  the native session checks pass.

## 2026-11-07 — Play was "completely broken"

Owner report after a hard reload: Play does not work at all. Measured: it was
not stuck (no request hung, no errors), it stepped one event per tick, each
step moving the whole Visual state (about 174,000 values, 0.75 s): about 22
events in 21 s, a few picoseconds of a frame tens of microseconds long, so
nothing visibly moved. In the frames-only recordings Play went frame to
frame; that is what it should do here.

- First change: Play restored each next frame's save (no first cycles):
  4 s per frame (restore 1.1–1.5 s, state 0.4 s). A restore also no longer
  saves again when its save is version 2 (`runFromSave` reuses it); version 1
  saves (this recording) still convert by saving.
- Then: every recorded pin and wire has the session's index (checked over
  all scopes of Snake: 113,586 pins and 60,332 wires, all equal), so Play
  draws the recording's own frame states (parts fetched and decompressed in
  the page, two kept), with no simulation; main memory, absent from recorded
  frames, reads "Pause to see memory". Pause (and any step or jump) restores
  the shown frame and simulates its first cycles. Snake from frame 11: 58
  frames in 20 s; eight different displays in eight samples; after Pause
  4,047 events and ▶ moved 2,003 ps.
- `GameReplayBrowserTest` now also plays 6 s (16 frames), pauses and steps:
  PASS.

## 2026-11-07 — Reverted the viewer to before the frame-long event bar

Owner: everything worked well until an update; revert. Chosen point: before
the event bar spanned the current frame (2026-11-07 00:30). Removed from the
viewer: the frame-long event bar and its shading, seeking from the event
bar, frame-by-frame Play from recorded parts, the request queue and showing a
frame before its first cycles. Back: the event bar spans what has been
simulated since the frame (its thumb moves with each step), Play steps
events and simulates on at the end, a jump simulates the frame's first 20
cycles before showing it, clicks during a request are ignored. Kept: frame
marks, the exact time readout, the history window, the next event found by
time after a trim, ◀ disabled at the first kept event, and the timeline sent
as a change (`applyGameTimeline` rebuilds the same list, so the viewer cannot
tell). The engine keeps `gameFastForward`, `uniqueTimestampsAfter` and the
restore that reuses a version 2 save; nothing in the viewer calls the first.

Checks: `GameReplayBrowserTest` (staged Sokoban) PASS; Play on Snake frame 11
stepped 4,012 → 4,038, simulated on (4,039 → 8,076 events) and the event
bar's thumb followed; no page errors.

## 2026-11-08 — The saves campaign finished; the Visual set now has saves

Campaign `build-milestone/long-games-visual-campaign-3` (frame saves, runner
of 2026-11-06 04:11, save version 1): Sokoban, Snake, Snake LEDs, 2048 and
Tetris passed. Pong's capture was interrupted once, resumed from its last
part (`capture-1`) and finished (370 frames, exit 0), but the frozen
finalize stopped with "generator raised StopIteration": it read a peak
memory from every `capture*-resources.txt`, and the interrupted capture's is
empty. Fixed in `run-long-gameplays.py` (peaks from the files that have one);
Pong was then finalized from its finished attempt with the repository's
script (`finalizedBy` in `campaign.json`): every host-reference check
passed; campaign `completed`.

| Game | Cycles | Simulation | Saves |
| --- | ---: | ---: | ---: |
| Sokoban | 673,842 | 1 h 50 min | 73 |
| Snake | 966,300 | 2 h 28 min | 124 |
| Snake LEDs | 975,228 | 2 h 33 min | 124 |
| 2048 | 1,954,776 | 5 h 26 min | 85 |
| Tetris | 2,147,750 | 6 h 16 min | 145 |
| Pong | 2,180,301 | 8 h 51 min (with the interruption) | 370 |

Published: the set replaced `site-docs/assets/system/long-games/visual/`
(120 MB: 34 MB of frame parts, 86 MB of saves); the set without saves moved
to `build-milestone/retired-visual-without-saves-2026-11-08`. § 5.6 table
regenerated and figures updated. `GameReplayBrowserTest` against the
published Visual Pong on preview 8893: PASS (opened at frame 4 of 370 in
20 s, 60 event steps, registers and memory, next frame, Compact/Visual);
the check no longer needs `CIRCUITSIM_GAME_RECORDING`.

Not done: rewriting these version 1 saves as version 2 (a jump would skip
one save of the machine); the full CTest and web suites.

## 2026-11-08 — Compact too: the compact core saves; Compact re-recorded with saves

Owner request: the same for Compact, so memory shows there too.

- `RV32IMCoreBehavioral` (the Compact machine's core: `Primitive` plus a
  `GateListModel`) declares its state like `GateListPartBehavioral`. The
  replay session takes its preset from its build profile
  (`fidelity-<preset>-v1`), so a Compact save continues in Compact; Gate and
  Logic stay unavailable for it.
- Bug found on the way: a restore of a version 2 save skips the redundant
  save but still passed `on_save`, which the runner refuses ("save_at and
  on_save go together"); the version 1 saves of the Visual set never took
  that path. Fixed; every session check rerun.
- Checks: `MachineStateProbe pong-short --fidelity compact` (2,500 → 5,000)
  byte-equal (new CTest `MachineStateResume_pong-short-compact_Test`);
  `GameSessionProbe` in Compact, in Visual with version 2 saves and with the
  Sokoban version 1 save: all end equal to a single run. A Compact save is
  757 KB raw, 33 KB deflated.
- Launched the Compact campaign with `--save-at-frames --replay-window 1000`
  (2026-11-08, unit `circuitsim-long-games-1794111508`, publication staged
  in `build-milestone/compact-replay-publication`), saves in version 2.

## 2026-11-08 — Compact set published with saves

The Compact campaign (`build-milestone/long-games-compact-campaign-saves`)
completed: all six games passed the host-reference checks, every game with
the same frames as the recording it replaces.

| Game | Simulation | Saves | Saves size |
| --- | ---: | ---: | ---: |
| Sokoban | 31 min | 73 | 2.6 MB |
| Snake | 40 min | 124 | 4.4 MB |
| Snake LEDs | 38 min | 124 | 5.3 MB |
| 2048 | 68 min | 85 | 3.1 MB |
| Tetris | 73 min | 145 | 5.3 MB |
| Pong | 78 min | 370 | 14 MB |

Peak native memory about 180 MB per game. Published: the staged set replaced
the Compact files in `site-docs/assets/system/long-games/` (frames 3.4 MB,
saves 35 MB, about 38 KB per save); the set without saves moved to
`build-milestone/retired-compact-without-saves-2026-11-08`. § 5.6 table
regenerated and text updated; the visualizer page says both fidelities open
as game sessions.

`RecordedCircuitBrowserTest` checks the frames-only viewer, which no
published game uses any more; it now answers the saves index with 404, as
for a recording without saves, and keeps checking that path.

Checks on preview 8893: `GameReplayBrowserTest` with Compact Pong (opened at
frame 4 of 370 in 17 s, 60 event steps in 7 s, memory readable, next frame,
Visual and back at the same frame) and with Visual Pong (13 s, 25 s): PASS;
`RecordedCircuitBrowserTest`: PASS; strict MkDocs build (`mkdocs.preview.yml`):
PASS. Not run: the full CTest and web suites.

## 2026-11-11 — Both sets re-recorded on SRAM; Visual saves now version 2

The games machines' main memory became SRAM
([SRAM record](2026-11-09-sram-main-memory.md)), which changes the machine
and its cycle counts, so the FF saves no longer fit. Both published sets
were re-recorded with a save after every frame: Compact (5.9 h, 34.5 MB of
saves) and Visual (30.1 h, 88.8 MB). Frames are the same as before; cycles
are 28–32 % higher. Every save is now version 2 (the Visual set's version 1
saves are gone), and the SRAM endpoint saves its pending request.

New with the switch: `PublishedSavesTest` (CTest) restores one published save
of Pong from each set in the current build, so a change that makes the
published saves stop fitting fails a test instead of only the page; and the
page reloads with `saves=off` and shows the recorded frames when the
simulator refuses a save (`GameSaveFallbackBrowserTest`). This replaces the
open items "the Visual saves are version 1" and "the full CTest and web
suites have not been run on this work" (the full native suite and the
browser checks of the SRAM record ran on the re-recorded sets).
