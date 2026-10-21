# Bare-metal RV32IM guests

Original small C game rules and MMIO library for the scripted device milestone.
These sources are bring-up work; circuit execution/acceptance is pending.

`platform.h` uses the device map documented in `site-docs/device-io/`. All
I/O is volatile guest loads/stores. `present()` brackets the command with
`fence iorw, iorw`; the initial platform supports unbuffered write-through RAM
caches and uncached video. No host rendering implements game rules.

Input IDs: 0 up, 1 right, 2 down, 3 left, 4 action/hard drop, 5 restart,
6 simulation update, 7 player-two up, 8 player-two down, 9 pause. Down events
perform actions; release events are consumed without actions. Snake changes
direction on a tap and moves on update. Pong samples both players' held levels
on update. Tetris updates/drop/rotation use integers and seven tetrominoes.

Linking reserves stack below `0x3e000` and a report page at `0x3f000`. Startup
clears BSS and halts with EBREAK after main returns. A game consumes a declared
finite number of down events; budget exhaustion in the simulator is a failure.
Game state uses fixed widths, a fixed xorshift seed, and a field-wise FNV-1a
fingerprint. The library provides persistent dirty-cell drawing at 64x48,
RGB332 colors, decimal tile glyphs, and immutable host snapshots on PRESENT.
Startup clears retained drawing memory; the zero-initialized cell cache then
skips black cells. Moves and restarts repaint changed cells, including cells
that become black. The smaller initial draw preserves the exact pixel output.

Rule checks, compiled-instruction audit, runtime/provenance manifests, input
scripts, frame hashes and native/WASM comparison must pass before these are
reported as working games. No upstream game source or media is incorporated.
