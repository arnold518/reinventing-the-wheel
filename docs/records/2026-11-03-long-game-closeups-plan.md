# Long games in more detail than Compact: Visual close-ups

Started: 2026-11-03. Status: closed (2026-11-03; plan not pursued). Code state:
`alpha` at `5549ee0` plus the uncommitted work of the
[game scenarios record](2026-11-03-game-scenarios-plan.md).

## Summary

The owner wants to watch the core work in the long games. The long
recordings are Compact: the core is one block and the recording keeps one
state per frame. Recording a whole session with the core built from its
stages is too large by orders of magnitude. The plan keeps the Compact
recordings as each game's full timeline and adds **Visual close-ups**: live
scenarios that resume a long game from one frame's saved state and run a few
thousand cycles with a gate-built core, so the reader can step through the
pipeline as it executes that move's rule code. Next step: owner approval.

## 2026-11-03 — Measurements behind the choice

All on `build-milestone`, embedded timing, 4.5 ns clock:

| Quantity | Compact | Visual (core from its stages) |
| --- | ---: | ---: |
| Speed, bring-up program, no history | 485 cycles/s | 104 cycles/s |
| Signal events per cycle | 287 | 16,000 |
| Bring-up machine with history (3,700 cycles) | 9 s, 274 MiB | 55 s, 386 MiB |
| One frame's state in a recording | 25 KB | > 12 MB (24 MB budget gone after two frames) |
| Pong short version with history (13,324 cycles) | 35 s, 187 MiB | > 3 GB at 98 s (killed) |
| Full-tick capture (old Visual Snake, 265,000 cycles) | — | 1.5 GB, about 5.7 KB per cycle |

The long sessions total 8.9 million cycles. Cycles per game update (from the
published recordings): Sokoban median 7,809, Snake 7,465, 2048 21,189,
Tetris 11,739, Pong 6,659 (range 1,899–46,116).

## 2026-11-03 — Options considered

| Option | Cost | What the reader sees | Verdict |
| --- | --- | --- | --- |
| Whole sessions in Visual, one state per frame | About 25 h native; about 11 GB | The core at the same point of the program in every frame (just after `PRESENT`) | Rejected: huge and shows little |
| Whole sessions in Visual, every signal change | About 25 h; about 50 GB | Everything | Rejected: size |
| Whole sessions in Compact, every signal change | About 1 GB (estimated from 287 events per cycle) | Bus, memory and devices every cycle; the core still one block | Possible later addition, not what was asked |
| **Visual close-ups from saved frames** | Seconds to make, about 10 KB stored per close-up, about 1 minute and 400 MB to open | The gate-built core fetching, decoding and executing one move, every wire, inspectable | **Recommended** |

## 2026-11-03 — Plan

1. **Frame state from the Compact run.** At each `PRESENT` the runner already
   has the RAM, video memory and button levels. Export, for chosen frames, the
   RAM bytes the program has written, the 3 KB of video memory and the held
   buttons (a few KB per frame).
2. **A resumable guest.** `start.S` checks a resume word in the reserved page;
   when set it skips the BSS clear and `main` continues the session loop
   instead of starting a game. The loop keeps all its state in statics
   (`actions`, `episode` and the score are moved out of registers), so no CPU
   register needs restoring. On resume, the guest first pops the events that
   re-create the held buttons (they carry no action), then waits for the next
   batch as usual.
3. **Runner: start from a snapshot, stop after a window.**
   `RecordedGuestOptions` gains a starting state (RAM and video images,
   levels) and a window length in cycles. Every check still runs: the ISA
   model starts from the same RAM image, the device model from the same
   video and levels. The display's frame counter and the timer restart from
   zero after the reset; the window never reads them.
4. **Cross-fidelity check.** Each close-up runs once natively in Compact and
   once in Visual over the same window. The retired instructions, data
   accesses, device reads and the RAM and video at the window's end must be
   equal. This is a new proof that the gate-built core and the compact one
   agree on real game code.
5. **Close-up scenarios.** A `GuestScenarioTest` variant (`closeup-2048-128`,
   …) with the snapshot embedded in the fixture table, opening in Visual. The
   window is the batch's arrival plus about 3,000 cycles: the poll, the pop
   and the move's rule code (2048: about 900 instructions for a move; Pong's
   whole update fits). Two per game, chosen from the players' metrics: 2048,
   the move that makes the 128 tile and an ordinary merge; Sokoban, each
   winning push; Snake, a bite and the crash; Pong, a paddle bounce and the
   point; Tetris, a line clear and a rotation.
6. **Viewer.** In a long recording, frames with a close-up get a marker and
   a "Watch in Visual" action that opens the close-up scenario at that frame;
   the close-ups are also listed in the menu under the long games.
7. **Documentation and tests.** § 5.6 gains a "Close-ups" part with the
   pipeline diagram of one move. CTest runs each close-up's Compact/Visual
   equality natively; one browser check opens a close-up.

Open questions for the owner:

- Window length: about 3,000 cycles (rule code only, opens in about a
  minute), or longer windows that include drawing. The memory limit in
  Visual with full history is not measured yet: the bring-up machine's 3,700
  cycles kept 386 MiB, Pong's 13,324 passed 3 GB; phase 1 measures it.
- Whether to add the Compact full-tick capture of whole sessions (about
  1 GB) for bus and device activity between frames.

## 2026-11-03 — Frame sizes measured (corrects the table above)

The "> 12 MB per Visual frame" figure above is wrong: `RecordedCircuitProbe`'s
24 MB budget also counts the Visual topology, which alone is 64 MB raw.
Measured instead on four real Visual frames of `sokoban-long`
(`LongGameplayProbe … --fidelity visual --part-frames 1`, stopped after four
frames; `build-milestone/visual-frame-sample-2026-11-03/`) and on the
published Compact recording:

| Per frame | Compact | Visual |
| --- | ---: | ---: |
| Pin and wire values | 1,880 | 173,918 |
| Changed since the previous frame | 3.7 % | 1.1 % |
| Full state as stored now (JSON) | 22.7 KB | 1.02 MB |
| Full state, gzip | 1.0 KB | 15.6 KB |
| Changes only, gzip | 0.3 KB | 5.6 KB |
| Topology and scopes, once per recording | 0.6 MB (0.2 MB gzip) | 64 MB (2.5 MB gzip) |

Whole Compact parts compress 25 times (Sokoban 3.7 MB → 145 KB); GitHub
Pages serves JSON compressed and Git stores objects compressed, so the
published long recordings cost far less than their 58 MB on disk.

Corrected options: Visual frame recordings of the six long games are about
5 MB of change-only compressed frames plus about 15 MB of topology, not
11 GB; they need change-only (or compressed) frames in the recorder and
viewer first, since the current format would be about 1.3 GB on disk, above
the Pages site limit. Their remaining cost is time (about 25 h at 104
cycles/s), and each frame shows the core at the same point of the program
(just after `PRESENT`). Watching the core work still needs every cycle:
full-tick capture is already change-only and was about 5.7 KB per cycle for
the old Visual Snake, so close-ups (stored windows or live) remain the way to
show one move cycle by cycle. Awaiting the owner's choice between Visual
frame recordings, close-ups, or both.

## 2026-11-03 — Closed: the owner chose Visual recordings of the whole games

With the corrected sizes, the owner chose to record all six long games in
Visual (one state per frame) instead of close-ups. The preparation and the
campaign are recorded in
[the Visual long-games record](2026-11-03-visual-long-games.md). Close-ups
were not implemented.

Record closed 2026-11-03.
