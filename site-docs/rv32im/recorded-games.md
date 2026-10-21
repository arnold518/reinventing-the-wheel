# Recorded games

The new batch mode runs alongside circuit playback. It executes
freestanding RV32IM software with predetermined device inputs, captures
console output and RGB332 frames, and ends at EBREAK. The compiled bring-up program has completed actual native and WebAssembly
circuit execution with checked console, pixels and reports. Complete game execution
and native/WebAssembly acceptance remain in progress.

## Programs

| Guest | Workload |
| --- | --- |
| Bring-up | Console greeting, timer/button read, colored pixels and a moving square |
| 2048 | Directional compaction, one merge per tile, deterministic spawning and restart |
| Sokoban | Walls, crate pushes, goals and restart |
| Snake | Queued direction taps, grid updates, food, growth and collision |
| Pong | Both players' levels, ball motion, paddle/wall bounce and misses |
| Tetris | Seven tetrominoes, rotation, horizontal moves, hard drop, line clear and top-out |

The initial fixtures use a fixed seed/level and three down actions per game.
Separate acceptance fixtures start with a 2048 merge board, a Tetris line-clear
board, Snake near a wall, and a Pong rally. The rally performs 17 finite
guest-controlled updates after one scripted start event.
Snake also receives a release between polls. Arcade movement uses explicit
scripted update actions in these short fixtures. Host rule tests exercise
additional boundaries. This is finite scripted execution, not sustained
interactive play or a measured FPS promise.

## Guest and device boundary

`tools/rv32im/guest` owns original game rules, startup/BSS/linker setup, volatile
MMIO access, integer drawing, tile glyphs and a deterministic PRNG. Code and
assets are preloaded. The guest draws into persistent 64×48 pixel memory and
issues PRESENT; the host copies and scales those pixels.

Buttons use logical IDs: up/right/down/left 0–3, action/drop 4, restart 5,
scripted update 6, player-two up/down 7–8 and pause 9. Reads of event-head are
nondestructive; a POP store consumes one transition. The fixed input FIFO
retains taps and reports overflow. The runner checks levels/queue behavior
independently at the execution edge.

The report page at `0x3f000` contains magic/version, game ID, action count,
score, terminal state, state fingerprint, sampled timer and queue status.
It is ordinary RAM written by the guest. Guest restart reinitializes game
rules and redraws; it does not reset the simulated CPU/devices.

## Build and run

Use the existing pinned GCC 12.4 RV32IM/ILP32 toolchain with the scoped build
helper. It audits every final text instruction and records compiler, source,
ELF and input hashes in `tests/fixtures/rv32im/games/manifest.json`.

```sh
python3 tools/rv32im/build-guests.py --compiler /path/to/riscv-none-elf-gcc
cmake -S . -B build-games -DCIRCUITSIM_DEPLOY_PYTHON=OFF \
  -DCIRCUITSIM_SPLIT_DELAY_TABLES=ON
cmake --build build-games --target RecordedGuestProbe -j1
python3 tools/rv32im/run-recorded.py bringup \
  --runner build-games/tests/RecordedGuestProbe --output build-games/bringup
```

Keep builds/runs serial and resource bounded on this host. The CLI has cycle,
wall-time and address-space limits; choose a fresh output directory for each
run so an earlier recording cannot be mistaken for a failed rerun.
`stderr.log` receives cycle/retirement progress while the guest runs.
The evidence includes hashes of the recording bytes and each captured frame.
The underlying runtime also bounds
instructions, events, input and frames. Console output and device-read evidence
are each capped at 65,536 entries; lower limits can be selected through the
C++ batch options. The viewer accepts files up to 32 MiB. Exhausted limits or
failed reference checks are failures.

For fidelity/quantum comparisons, the CLI also accepts `--quantum-events 97`
and `--logic-profile expanded-core`. The latter expands the CPU assembly with
compact children; `structural` expands all available logic. The default is
`compact`. Recordings identify their profile name and fingerprint. Different
profiles may have different event counts; architectural results and captured
device outputs must agree at the tested operating clock. Expanded profiles are
comparison equipment: their runs check setup violations, but the all-state
assembled clock certificate currently covers the compact configurations.

`tools/build-pages.sh` packages the browser worker and WASM assets. Serve that
generated site to run scripts in the browser or open native CLI recordings.

Open the additive <a href="../../simulator/games.html">recording viewer</a>, run a packaged
script in its worker or load the exported `recording.json`. The circuit
visualizer remains available separately. Frame scrubbing/replay does not
execute guest instructions. Replay FPS is a display setting; run evidence
reports simulated cycles, host execution time and capture cost separately.
Unknown pixel bits are shown in magenta.

## Verification boundary

The runner checks CPU retirement/registers through the independent I/M
instruction oracle. Separate models check RAM/video, console, timer and
buttons; device read values are sampled at backend execution and retained
across waits. PRESENT captures are checked against ordered guest stores.

Strict host C rule tests assert boundary cases. Host C reference runs provide
expected guest reports and exact rendered bytes for comparison with compiled
RV32IM execution. The host adapter is test equipment and is excluded from
the audited RISC-V binaries. Native/WASM and repetition/quantum comparisons,
integrated timing, browser checks, old regressions and performance evidence
remain milestone acceptance gates.

## Display and keyboard views

Captured geometry sets the canvas size; it is not fixed to 64×48. Each
dimension is at most 4096 and each frame may contain up to 262144 pixel bytes,
including row padding, with a 4 MiB aggregate pixel budget per
recording and the existing 32 MiB file limit. RGB332 conversion and unknown
pixel colors are shared with the circuit device preview.

A keyboard panel follows the inspected frame's logical input levels, queue
count and overflow. The saved input events carry exact virtual ticks and
sequence numbers. These are observations of input accepted by the device,
not keyboard controls for a running guest. Older recordings without input
snapshots still open and explicitly report that no keyboard snapshot exists.
Malformed keyboard snapshots are rejected before drawing; they must contain
32 four-state input levels and the device's queue/head/overflow fields.
