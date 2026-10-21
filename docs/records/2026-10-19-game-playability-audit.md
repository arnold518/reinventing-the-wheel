# Game playability: measured feasibility

Started: 2026-10-19. Status: closed (feasibility audit complete).
Code state: `realistic-timing` at `e24e289`, with concurrent arbiter,
test, documentation and visualizer changes in the worktree.

## Summary

Low-FPS play has a measured basis with compact execution, FF memory and
incremental drawing. On this host, the original headless prototypes reached
about 23 Pong and 7 Tetris updates/s natively, and 6.8 and 2.6 in a Chromium
worker. The current cached/shared-DRAM form reached only 0.24/0.10 natively
and 0.10/0.039 in the browser. A structural CPU/FF Pong check reached 0.13.
These are guest update budgets, not implemented live-game frame rates.

The owner requires new live modes alongside recorded playback. No existing
mode or simulator behavior was changed. A native 4 kHz CPU configuration
also verified with unchanged leaf/memory delays and enough host throughput
to keep up with its virtual time. Live sessions, MMIO/display devices,
bounded input handling and the WASM 64-bit time migration remain work for
the platform milestone. This audit is complete; those follow-ups belong to
the open [device/game plan](2026-10-19-device-io-games-plan.md).

## 2026-10-19 — Measurement design

- Reuse the existing `rv32i_experiment` runner, strict ELF loader, pinned
  RV32I compiler, startup/linker script and instruction-oracle verification.
- Measure game logic, incremental framebuffer drawing and full redraws.
  Compile with RV32I/ILP32 and software arithmetic; do not add an ISA extension.
- Compare the compact advanced core with FF endpoints against the existing
  L1/shared-DRAM topology. Include a bounded structural-core sample and
  native/WebAssembly execution where feasible.
- Exclude initialization and validation from the frame-update region but
  report them separately. Include the actual guest drawing instructions.
  Report repetitions, correctness checks, timing violations and source state.
- Distinguish headless update throughput from end-to-end keyboard/display
  latency. Devices and a live browser session do not exist yet; derived FPS
  is a budget for those additions, not a verified interactive game rate.

The original game workload source is
`tools/rv32i-performance/playability/game-kernels.c`. It includes paddle/ball
motion and collisions, tetromino movement/rotation/collision/locking and row
clearing. Tetris line-clear stress is deliberately a separate case. Its
2×2-pixel cells and minimal visuals are prototype scope, not a complete game
UI. Inputs are scripted; measured runs must not be described as live play.

Initial calibration (one sample, not a game estimate): compact advanced/FF,
569 cycles in 0.1010 execution seconds; C-ID/shared DRAM, a 2,000-cycle
incomplete prefix in 6.4866 seconds. Both report zero setup violations.
The runner was relinked against the current native library. WASM library
rebuild started; results pending.

## 2026-10-19 — Final method and source boundary

Host: Intel Core i5-7200U, four available hardware threads, Linux x86-64.
The host was shared with other work; unrelated processes were not stopped.
Retained performance panels ran serially, after our WASM compilation ended.
An earlier panel overlapped compilation and was discarded and rerun.
Timing variation from background load is reflected in the repeat ranges.

The native FF panel used source state `7f285cd`; the cached, structural and
slow-clock native follow-ups used `739c454`. The latter adds a shared-image
overlap guard during loading, outside the measured frame region. The WASM
library was rebuilt from the `7f285cd` source state. Twelve inspected core,
engine and memory source hashes matched that commit. Concurrent repository
work continued; the binaries and source hashes, not the latest moving HEAD,
define these results. Binary SHA-256 values and commands are in the local
metadata. Representative hashes:

- Native FF runner: `95c027144f4cfd6a53ca47786d7a5d363115f1ea65426af7bc05a66912d356b5`.
- Native follow-up runner: `6c63ac72a0d7f9ed9631c9df6d709bd9be5b36422b4f5ddc52632c9f70ee0586`.
- Browser: headless Chromium `149.0.7827.55`, installed Chromium-1228, in a
  dedicated worker. The normal UI event loop remained active while the
  worker ran. This checks worker isolation, not bounded live-input delivery:
  each audit call runs synchronously to completion.

Builds were Release/O3. The guest compiler was the existing xPack GCC 12.4
toolchain, `-march=rv32i -mabi=ilp32 -O2`, with `libgcc`; no M extension was
added. The strict ELF loader, existing startup/linker script and `tohost`
verdict convention were reused. All normal measurements selected the
`embedded` profile and explicit 4,104 ps CPU period. Histories were off.
The CPU was the existing compact advanced pipeline except the stated
structural check. FF results use separate instruction/data endpoints;
C-ID uses the existing pair of SRAM-backed 256-byte L1s, structural arbiter
and shared DRAM with its existing timing. Video storage is ordinary guest
RAM in this audit: no MMIO router, input/timer device or scanout controller
is being measured.

Each ELF checks its final sampled checksum against the same C workload
compiled for the host. The measured region starts after a warm-up update
and ends before checksum/verdict handling. Guest game logic and pixel
stores are inside the region. Initialization, initial drawing and final
checking still execute, but are outside reported update throughput.
Thus full-redraw boot cost is not hidden in the frame rate, nor is it
included in the frame rate; it is part of the recorded total run time.

The first run of each configuration uses instruction lockstep, architectural
state checks and a complete final memory-image comparison. Subsequent timed
runs disable lockstep and must retain the same final fingerprint. The runner
subtracts verification time from the reported execution region. The main FF
panel has one verified run and two separate timed runs per case; rates below
are the median and range of those two timed runs. The existing experiment
runner also retains counters/observations per cycle, so these are performance
measurements of that path, not an optimized future session implementation.

## 2026-10-19 — Compact CPU with FF memory

All drawing is 8-bit indexed pixels. The basic logical screen is 64×48;
Tetris uses 2×2-pixel cells. The 160×120 case changes the framebuffer size.
Each update applies scripted input. Pong includes paddle/ball movement and
collisions; Tetris includes movement, rotation, collision, locking and row
clearing. The normal Tetris sequences include piece locks. These are minimal
prototypes with no complete menu, text renderer, sound or upstream game port.

| Guest work | Measured updates | Instructions/update | Cycles/update | Native updates/s (range) | Browser updates/s (range) |
| --- | ---: | ---: | ---: | ---: | ---: |
| Pong logic only | 512 | 65.9 | 92.9 | 95.86 (92.64–99.08) | 24.11 (23.94–24.28) |
| Pong incremental pixels | 128 | 271.8 | 366.8 | 22.87 (20.41–25.32) | 6.77 (6.64–6.90) |
| Pong full 64×48 redraw | 4 | 2,459 | 4,051 | 1.98 (1.90–2.07) | 0.612 (0.582–0.641) |
| Pong full 160×120 redraw | 1 | 14,559 | 24,215 | 0.377 (0.365–0.390) | 0.130 (0.122–0.138) |
| Tetris logic only | 64 | 501.4 | 665.5 | 14.39 (13.57–15.20) | 5.05 (5.03–5.07) |
| Tetris incremental pixels | 32 | 978.8 | 1,273.1 | 7.10 (6.93–7.26) | 2.64 (2.63–2.66) |
| Tetris full 64×48 redraw | 2 | 4,635 | 6,759 | 1.23 (1.22–1.25) | 0.454 (0.405–0.503) |
| Tetris row-clear stress | 4 | 6,713 | 8,675 | 0.995 (0.906–1.083) | 0.360 (0.345–0.375) |

The stress case reseeds the board and forces a row clear on every update,
including fixture preparation in the measured region. Its approximately
one-second native / 2.8-second browser update is an adverse workload result,
not the isolated cost or frequency of a row clear in normal play. It shows
why average rates alone do not establish responsive controls.

Normal incremental updates consumed approximately 44 ms (Pong) / 141 ms
(Tetris) natively, and 148 / 378 ms in the browser. These are computation
costs, not measured key-to-pixel latencies. A guest polling only once per
update can add another update's wait before responding. Input scheduling,
worker yielding and the eventual device/display path still need validation.

Browser-only palette/grayscale conversion plus `Canvas.putImageData` averaged
0.12–0.26 ms in 256-repeat microbenchmarks. This excludes worker transfer,
scanout and a running circuit inspector. It supports keeping the play view
small and exporting completed pixels instead of serializing every wire;
it does not establish the final presentation cost.

## 2026-10-19 — Cached and structural configurations

These use one measured update after one warm-up update. They are shorter
calm-frame cases, not the long-run FF averages above. The native cached
checks have one verified and one timed pass. Browser cached and structural
checks have one verified pass each; their reported execution time excludes
lockstep time. Do not treat those single-pass numbers as a statistical range.

| Configuration and workload | Cycles/update | Native updates/s | Browser updates/s | Browser seconds/update |
| --- | ---: | ---: | ---: | ---: |
| Compact CPU, C-ID/shared DRAM, Pong incremental | 2,306 | 0.244 | 0.100 | 10.00 |
| Compact CPU, C-ID/shared DRAM, Tetris incremental | 4,663 | 0.101 | 0.0389 | 25.71 |
| Structural CPU, FF memory, Pong incremental | 356 | 0.134 | Not run | Not run |

The native structural case took 7.44 seconds for its measured update and
completed with oracle verification. It retains scalable memory; this is a
structural CPU check, not a claim that all 256 KiB of RAM were expanded.

The FF Pong path processed about 60 events/cycle. C-ID processed about
1,989 for Pong and 2,587 for Tetris; native cycle throughput was roughly
563 and 471 cycles/s. The cache controller remains structural even with
scalable child stores. Higher cycle cost per guest instruction and more
events per cycle both matter. The cache campaign's simulated-cycle speedup
over *uncached DRAM* does not imply faster host execution than compact FF.

## 2026-10-19 — A slow CPU clock that can be paced

Matching the nominal 4,104 ps CPU clock to wall time would require about
244 million simulated cycles per second. The measured paths cannot do that.
A distinct play configuration can select a much slower modeled CPU clock
while retaining the existing gate and FF memory delays. It must declare
that clock, size its timer accordingly and handle slow-frame input correctly.

Two additional **native**, oracle-verified runs used a 250,000,000 ps period
(4 kHz CPU), with unchanged `embedded` leaf/memory timing and FF endpoints:

| Workload | Simulated time in ROI | Host execution time | Update budget at 4 kHz |
| --- | ---: | ---: | ---: |
| Pong incremental, 128 updates | 11.736 s | 5.581 s | 10.91/s |
| Tetris incremental, 32 updates | 10.185 s | 4.869 s | 3.14/s |

Both produced exactly the same cycles, final state and memory fingerprint
as the ordinary-clock FF cases, with zero setup violations. The host had
about 2.1× execution headroom relative to the modeled time. This verifies
the compute requirement for native pacing at that clock; it does not test
an implemented wall-clock runner, timer device or live input loop.

From the browser's measured cycle costs, a proposed 1.5 kHz CPU would budget
4.09 Pong and 1.18 Tetris updates/s before new device costs. This is a
calculation, not a tested browser preset. The current wasm32 time type must
be migrated before this can be used for long runs: `size_t` absolute ticks
cover only 4.29 ms at 1 ps. Audit browser runs stayed below that limit;
no overflow reproduction or 64-bit implementation was performed.

The slow-clock result applies to FF memory. It does not justify slowing the
DRAM controller below its refresh/retention requirements. Realistic DRAM
and the current structural cache hierarchy need a separately verified play
configuration or faster validated implementations. Do not tune memory
intervals to make a frame-rate claim.

## 2026-10-19 — Verification, artifacts and reproducibility

- **57 retained completed runs:** 31 native and 26 in Chromium; **23** use
  full instruction lockstep and final-memory comparison. All report **zero**
  setup violations. Every case's complete final-state fingerprint agrees
  across its tested backends/configurations, including structural FF and
  compact cached Pong. Remaining passes reproduce their verified result.
- **24,582 sanitized host updates:** three game cases, two renderers and
  4,097 updates per renderer. ASan/UBSan report no errors. Incremental/full
  pixels, board and explicit game state compare byte for byte **after every
  update**. Shape-cell, collision-boundary, wall-bounce and row-collapse
  checks also run. These validate the original prototypes, not an upstream
  game's complete rule set.
- Python audit scripts compile. Native runner/library and the WASM library
  were built serially; the audit WASM binding compiled and ran in Chromium.
  No product simulator/frontend implementation was changed, no existing
  playback scenario was replaced, and no full suite or published site check
  was run for this benchmark-only work.

Key results are copied above. Local evidence is under
`build/playability-audit-2026-10-19/`: per-run JSON/stderr, guest ELF and build
manifest, `source-state.json`, per-configuration metadata, `host-validation.json`,
`verification-summary.json`, and aggregate `summary.json`/`summary.csv`.
Artifacts include exact commands, binary/ELF hashes, instruction/cycle/event
counts, fingerprints and excluded setup/verification/whole-run times.

The reproducible workload and audit tools are under
`tools/rv32i-performance/playability/`. Representative commands used:

```sh
cmake --build build-cleanup --target rv32i_experiment -j1
python3 tools/rv32i-performance/playability/build.py
python3 tools/rv32i-performance/playability/validate.py
python3 tools/rv32i-performance/playability/run-native.py --cases pong-logic pong-dirty pong-full64 pong-full160 tetris-logic tetris-dirty tetris-full64 tetris-lineclear
python3 tools/rv32i-performance/playability/run-native.py --memory dram --topology C-ID --cases pong-dirty-short tetris-dirty-short --repeats 2
python3 tools/rv32i-performance/playability/run-native.py --fidelity reference --cases pong-dirty-short --repeats 1
python3 tools/rv32i-performance/playability/run-native.py --cases pong-dirty tetris-dirty --period 250000000 --repeats 1
cmake --build build-wasm --target SimulatorLib -j1
```

The audit binding was linked with the configured Emscripten `em++`:

```sh
em++ tools/rv32i-performance/playability/wasm-probe.cpp build-wasm/libSimulatorLib.a \
  -Iinclude -Ibuild/playability-audit-2026-10-19 -std=c++20 -O3 -fexceptions --bind \
  -sMODULARIZE=1 -sEXPORT_ES6=1 -sEXPORT_NAME=createAudit -sENVIRONMENT=web,worker,node \
  -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=268435456 -sMAXIMUM_MEMORY=2147483648 \
  -sSTACK_SIZE=8388608 -sDISABLE_EXCEPTION_CATCHING=0 -sNO_EXIT_RUNTIME=1 -sFILESYSTEM=1 \
  --embed-file build/playability-audit-2026-10-19/elfs@/elfs \
  -o build/playability-audit-2026-10-19/audit.mjs
python3 tools/rv32i-performance/playability/run-browser.py --executable /home/arnold/.cache/ms-playwright/chromium-1228/chrome-linux64/chrome --cases pong-logic pong-dirty pong-full64 pong-full160 tetris-logic tetris-dirty tetris-full64 tetris-lineclear
python3 tools/rv32i-performance/playability/run-browser.py --executable /home/arnold/.cache/ms-playwright/chromium-1228/chrome-linux64/chrome --memory dram --topology C-ID --cases pong-dirty-short tetris-dirty-short --repeats 1
python3 tools/rv32i-performance/playability/summarize.py
```

Initial tooling failures: the audit WASM link could not write Emscripten's
cache from the sandbox; an approved rerun succeeded. The browser driver
stalled in the sandbox and was interrupted; the approved external run
worked. Playwright's default Chromium-1223 was absent, so the existing
Chromium-1228 executable was selected; nothing was downloaded. A preliminary
checksum that shifted out the Tetris state was corrected before retained
panels. The longer retained workloads and per-update renderer comparisons
replace those preliminary samples.

## 2026-10-19 — Conclusion and next acceptance boundary

The evidence supports developing additive low-FPS play modes. A reasonable
first budget is a small indexed framebuffer with incremental updates and a
compact CPU/FF profile; the browser can enlarge logical pixels for display.
The measured ceilings allow planning around roughly 3 Pong / 1 Tetris
update per second in the browser with some headroom. Those are proposed
targets, not verified end-to-end rates. The current structural CPU and
cached/shared-DRAM paths do not provide a practical real-time game budget.

The next implementation proof must retain recorded playback, add the live
session/time/input contracts, and measure actual input-to-visible-output
latency, frame deadlines, device overhead, memory bounds and pause/reset.
It should include normal movement and adverse locking/row-clearing updates.
Only that proof can establish the delivered game FPS. No approval to
implement those product modes is implied by this completed audit.
