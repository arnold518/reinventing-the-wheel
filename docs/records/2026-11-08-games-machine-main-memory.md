# Games machine main memory: FF against SRAM and DRAM

Started: 2026-11-08. Status: closed (2026-11-09). Code state: `alpha` at `1ee0600` plus the
uncommitted SRAM and clock-period options below.

## Summary

The games machine's main memory is a zero-wait flip-flop memory (`ff`). The
owner asked whether to replace it with the realistic technologies that
already exist. Measured on the bring-up program and the five short games
(Compact, `embedded`, 4,500 ps): every technology gives the same frames,
console text and instruction counts; SRAM costs 1.28–1.32× the FF cycles,
DRAM 3.6–4.2×, and the default caches (4 sets × 16-byte lines per port) make
every technology slower except DRAM (cached DRAM 3.1–4.1×). The 4.2 cycles
per instruction on FF are fully explained by the interconnect: each fetch
takes three cycles (the bus router's registered request, the arbiter not
taking a same-cycle answer, and the core offering the next fetch only the
cycle after the answer), plus data accesses and 33-edge multiply/divide.
Decision (2026-11-09): SRAM becomes the default main memory; the switch
and the re-recording continue in [SRAM as the games machine's main
memory](2026-11-09-sram-main-memory.md). The interconnect is left as it is.

## 2026-11-08 — Question and plan

- Context: the long games average 4.26 cycles per instruction (Pong,
  2,180,301 cycles for 511,921 instructions) on zero-wait FF memory; a
  five-stage pipeline usually gets near 1–1.5.
- The memory subsystem already takes `technology = ff | sram | dram` and
  `caches = none | default`, but the games machine's options
  (`GameMachineOptions`) only chose FF or DRAM. Added `sram` (machine option,
  runner option, `system_experiment --memory sram|dram|cached-sram`, and
  `ComputerSystemTimingTest` cases `sram`, `dram`, `cached-sram`). A run
  identity mixes `sram` in only when chosen, so existing saves keep their
  identity.
- Plan: certify each configuration's clock (embedded profile); run the
  bring-up program and the five short games in Compact at that clock; compare
  cycles, cycles per instruction and simulated time. No memory timing or
  cache setting is changed after seeing results.

## 2026-11-08 — The machine's SRAM and DRAM were not set up for a clock

Reading the memory path before running anything:

- `memory.subsystem.32` with `technology = sram` built the asynchronous SRAM
  chip with the endpoint defaults (`access_cycles = 2`, whatever the clock),
  and `dram` kept the default `words_per_row = 2` and a refresh every 96
  cycles. The memory-system experiments of § 4.8 (`RV32IExperiment.cpp`)
  instead use the on-chip clocked SRAM macro with wait states from the
  proven access time at the clock (`staticAccessCycles`), and DRAM with the
  profile's datasheet row (512 words in `embedded`) and one REFRESH per tREFI
  (7.8125 µs) at the clock.
- Changed: the subsystem takes `clock_period`; SRAM is always the clocked
  macro, with wait states from the clock when given; DRAM takes the
  profile's row size, and its refresh rate from the clock when given. The
  games machine passes the run's period as `clock_period` for an SRAM or
  DRAM memory (FF runs and their saves are unchanged).
  `system_experiment` reports the resulting `memorySetup`.
- The embedded profile's SRAM intervals are marked in source as placeholders
  awaiting calibration (`sram_bitline_base` 90 ps, 3 ps per row), and a
  memory this large (65,536 words) gets delays extrapolated from tables
  proven at 64 and 128 words ([memory timing](../../site-docs/memory/timing.md#large-memories)).
  So SRAM wait states here are an estimate, not a calibrated figure.
- For reference, the measured latencies at 4,104 ps (embedded,
  `tools/timing/documentation-timing.generated.json`): FF 0 extra cycles,
  clocked SRAM 2, DRAM read 8 (row open), 14 (row closed), 20 (other row open).

## 2026-11-08 — Clock certificates

`ComputerSystemTimingTest` (safe-clock analyzer), shortest safe period per
configuration:

| Configuration | `embedded` | `unit` (ticks) | Limiting path |
| --- | ---: | ---: | --- |
| `ff` | 4,464 ps | 92 | `CORE.IMEM_RSP_READ_DATA` (embedded) |
| `cached-ff` | 4,254 ps | 84 | instruction cache controller pending bit |
| `sram` | 4,254 ps | 160 | arbiter `I_RESPONSE.HELD_DATA` write enable (embedded); `MEMORY.RAM` write mask (unit) |
| `cached-sram` | 4,254 ps | 156 | |
| `dram` | 4,254 ps | 84 | |
| `cached-dram` | 4,254 ps | 84 | |

Every configuration is safe at 4,500 ps, the period the games are recorded
at, so all runs below use 4,500 ps; running each at its own certificate was
not done (it would shorten simulated time by at most 5 %, and change SRAM
wait states and DRAM refresh only through the period).

## 2026-11-08 — Results

`tools/system/run-experiments.py --profile embedded --period 4500 --machines
games-machine --memory M` for the six memories (outputs in
`build-milestone/memory-comparison/`, not tracked). Setup reported by the
runs: SRAM clocked, 1 access cycle at 4,500 ps; DRAM 512-word rows, a
refresh every 1,736 cycles. Every run passed the runner's checks, with no
setup violations, and gave the FF run's frames, console text and
instruction count.

Cycles, cycles per instruction, and cycles relative to FF:

| Program | `ff` | `sram` | `dram` | `cached-ff` | `cached-sram` | `cached-dram` |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Bring-up | 3,714 · 4.17 | 4,760 · 5.34 · 1.28× | 13,491 · 15.14 · 3.63× | 6,681 · 7.50 · 1.80× | 7,738 · 8.68 · 2.08× | 15,257 · 17.12 · 4.11× |
| 2048 | 32,012 · 4.19 | 41,322 · 5.40 · 1.29× | 124,268 · 16.25 · 3.88× | 50,986 · 6.67 · 1.59× | 56,490 · 7.39 · 1.76× | 99,961 · 13.07 · 3.12× |
| Sokoban | 43,273 · 4.44 | 55,533 · 5.70 · 1.28× | 169,589 · 17.41 · 3.92× | 72,691 · 7.46 · 1.68× | 82,368 · 8.46 · 1.90× | 150,923 · 15.49 · 3.49× |
| Snake | 28,194 · 4.22 | 36,963 · 5.53 · 1.31× | 118,134 · 17.69 · 4.19× | 48,234 · 7.22 · 1.71× | 54,649 · 8.18 · 1.94× | 100,326 · 15.02 · 3.56× |
| Pong | 13,324 · 4.18 | 17,268 · 5.41 · 1.30× | 55,711 · 17.47 · 4.18× | 22,221 · 6.97 · 1.67× | 25,069 · 7.86 · 1.88× | 46,885 · 14.70 · 3.52× |
| Tetris | 50,860 · 4.22 | 67,028 · 5.56 · 1.32× | 207,528 · 17.21 · 4.08× | 87,567 · 7.26 · 1.72× | 99,224 · 8.23 · 1.95× | 184,281 · 15.28 · 3.62× |

Observations:

- One SRAM wait state adds about one cycle per memory access, because no
  access overlaps another (see the next section).
- The default caches are too small to help: 64 bytes per port, and a hit is
  answered in the lookup cycle while a miss fills a 16-byte line. They slow
  FF and SRAM down; with DRAM they save 11–20 % in the games and cost 13 %
  in bring-up.
- DRAM costs 3.6–4.2× the cycles. The row hit rate was not measured.

## 2026-11-08 — Why FF takes 4.2 cycles per instruction

A throwaway tracer (scratch, not tracked) subscribed to the runner's signal
trace and sampled the core's, the memory subsystem's and its arbiter's pins
before every rising edge of the bring-up run on FF (3,714 cycles, as the
normal run).

One plain fetch, edge by edge (core accepts at edge N):

| Edge | Core | Bus router | Arbiter / RAM |
| --- | --- | --- | --- |
| N | offers the fetch; the router takes it | captures the request | idle |
| N+1 | waits | offers it to the memory subsystem | offers it to the RAM, which takes it and answers in the same cycle; the arbiter is not yet busy, so its `MEM_RSP_READY` is 0 and the answer waits in the RAM's buffer |
| N+2 | takes the answer | passes the answer through | busy: takes the answer and passes it straight through |
| N+3 | offers the next fetch | | |

So every fetch takes three cycles, with one request in flight at a time.
Over the whole run:

| Part | Count | Cycles | Per instruction |
| --- | ---: | ---: | ---: |
| Fetches at 3 cycles (104 were on a wrong path and discarded) | 995 | 2,985 | 3.35 |
| Data accesses, each holding the next fetch's answer 1 or 2 cycles | 203 (30 + 173) | 376 | 0.42 |
| Multiply/divide, 32 extra edges each | 11 | 352 | 0.40 |
| Total | | 3,713 (measured 3,714) | 4.17 |

The three fetch cycles come from:

1. the bus router's registered request, by design
   ([§ 5.2.1](../../site-docs/system/5.2/router.md));
2. the arbiter taking an answer only from the edge after the memory
   accepted the request ([§ 3.5.3](../../site-docs/memory/controllers.md)),
   so a zero-wait same-cycle answer waits one edge;
3. the core's fetch requester offering the next fetch only on the cycle
   after it takes an answer (never in the same cycle).

Connected straight to a zero-wait endpoint, the same core completes a
request and its answer at one edge ([§ 4.6.5](../../site-docs/rv32i/4.6/memory-waits.md)).
So the CPI is an interconnect property, not the memory technology. It also
explains why one SRAM wait state costs about 28 %: the extra cycle adds to
each three-cycle access instead of being hidden.

Not changed: none of the three serializations. Each is a design decision of
its part (and the router's is documented); changing them is separate work.

## 2026-11-08 — Checks

- `ComputerSystemTimingTest` built and run with the new `sram`, `dram`
  and `cached-sram` cases (certificates above).
- The 36 experiment runs above, with the runner's checks.
- FF bring-up at 4,500 ps unchanged (3,714 cycles) after the change.
- Strict MkDocs build (`mkdocs.preview.yml`) after editing § 5.3 and § 5.5:
  passed.
- Not run: the full CTest suite and the web tests.

## 2026-11-09 — Decision; record closed

The owner chose SRAM as the games machines' main memory, after the SRAM
timing check of the follow-up record. The `sram=1`/`dram=1` flags of this
record are replaced there by one `memory=sram|ff|dram` option with SRAM as
the default. The three interconnect serializations are left as they are:
the CPU stage closes, and a GPU memory system will need its own design for
many requests in flight. Follow-up: [SRAM as the games machine's main
memory](2026-11-09-sram-main-memory.md).
