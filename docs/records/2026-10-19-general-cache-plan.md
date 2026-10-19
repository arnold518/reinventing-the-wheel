# General cache: policies and sizes as parameters

Started: 2026-10-19. Status: open (implementation in progress since
2026-10-20). Code state: branch `realistic-timing` at `f2d3108`.

## Summary

`Cache<W>` was general in geometry but fixed in behavior. This record plans
and carries out making the behavior parameters too, then a behavioral
(compact) cache proven equivalent to the structural one, for simulation speed
(games need millions of cycles). Done so far (2026-10-20): write buffer
(step 1), write-back and write-allocate (step 2), a line-state store for up
to 4,096 sets (step 3, revised), 4 and 8 ways with lru, plru, fifo or random
replacement (step 4). Next-line prefetch (step 5) is deferred: see the
step-4 decision. Next: the behavioral cache (step 6), then the campaign rerun
(step 7). Visualizer and browser checks of steps 1–4 wait for a
memory-gated run. Several requests in flight and multi-bank DRAM stay out of
scope.

## 2026-10-19 — Starting point

The owner asked whether the cache is "the general cache" and whether its
limits could all become parameters. What exists
(`include/modules/memory/Cache.hpp`, `src/modules/memory/Cache.cpp`, about
940 lines; public page `site-docs/memory/caches.md`):

| Setting | Supported now |
| --- | --- |
| Word width `W` | 8, 16, 32 |
| `sets` | power of two, at most 4 expanded or 64 scalable |
| `ways` | 1 or 2 (two-way: exact LRU, one bit per set) |
| `block_bytes` | 1, 2 or 4 words per line |
| Tag and data stores | FF or SRAM endpoints, chosen independently |
| Fill | one word at a time, or the whole line in one burst |
| Mode | read-only, or read-write |

Fixed, not parameters:

1. **Write-through, no-write-allocate.** Every store goes to the lower
   memory; a cached line is updated too; a write miss brings nothing in.
2. **One request at a time.** The upper port accepts one transaction; the
   requester waits during a miss or a write.
3. **No write buffer.** The requester waits until each store has completed
   in the lower memory.
4. **Replacement only for two ways.** More ways need a new mechanism.
5. **Valid and replacement bits are flip-flops in the controller.** This is
   why the scalable form stops at 64 sets.
6. **No behavioral form.** The controller always runs on its gates.

The 2026-10-19 cache campaign (`site-docs/rv32i/experiments.md`, "Memory
topologies and caches") measured exactly this design. Its most direct
consequence: adding the L2 under the two L1s was 4–8 % slower than the L1s
alone in `calls`, `sort` and `stream`, because every store is written
through two levels while the core waits (limits 1 and 3).

## 2026-10-19 — Plan

### Parameters to add

| Parameter | Choices | What has to be built |
| --- | --- | --- |
| `write_buffer` | 0 (today), 1, 2, 4 entries | A queue of (address, data, mask) that drains to the lower memory in the background. A read miss must not overtake a queued store to the same address: either search the queue and forward, or drain it before the miss (simpler; decide at design time). A full queue makes the store wait. |
| `write_policy` | `through` (today), `back` | A dirty bit per line. A dirty victim is written to the lower memory before its way is refilled. Invalidate becomes flush: a sequencer walks every line and writes out the dirty ones before clearing. |
| `write_miss` | `no-allocate` (today), `allocate` | Fetch the line on a write miss (the existing fill sequence), then write into it. With `through` this is unusual but legal; with `back` it is the normal pairing. |
| `ways` + `replacement` | 1, 2, 4, 8 ways; `lru` (exact, 2 ways), `tree-plru`, `fifo`, `random` (LFSR) | Replacement state per set sized by the policy; the way comparators and banks already scale with `ways`. |
| `sets` | up to about 1,024 (scalable) | Move valid, dirty and replacement bits out of controller flip-flops into the tag store (stored next to the tag, as real caches do), so metadata grows with the tag bank, not the controller. |
| `prefetch` (optional) | `none`, `next-line` | On a miss, also fetch the next line when the lower port is idle. |

All of these stay inside the cache: pins and the transaction protocol do
not change, so cores, endpoints, arbiters and the experiment topologies
accept any combination.

### Out of scope for the cache

- **Several requests in flight (non-blocking cache, MSHRs).** The
  transaction protocol allows one outstanding request per port, and the
  cores issue one request and wait. A non-blocking cache gains almost
  nothing until the protocol carries request IDs (every endpoint and core
  changes) and a core can issue ahead. Separate project. For the current
  in-order cores, the write buffer gives most of the benefit.
- **Multi-bank DRAM.** A memory-device change (several open rows), worth its
  own record.

### Costs and rules

- **Reference model.** An independent model of every policy combination;
  the gate-level cache must agree with it on every upper and lower
  transaction and on the final memory contents. The test matrix grows with
  combinations, so choose a covering set rather than the full product.
- **Clock.** Write-back and the write buffer add control paths; each new
  configuration is certified again, and the clock may grow.
- **Simulation speed.** Bigger caches and more policies make long
  experiments slower on gates. Per the lower-level-first rule, a behavioral
  form comes only after the structural cache is complete and tested, with
  equivalence tests.
- **Write-back and instruction fetch.** With a write-back data cache the
  lower memory can be stale; code that writes instructions must flush
  first. Our programs do not, but the rule must be documented and tested.
- **Gate-level size.** Detailed devices are limited to 4,096 cells; larger
  stores use the scalable form, as today.
- **Memory realism.** Memory intervals stay as calibrated; nothing is tuned
  toward cache results.

### Order

1. Write buffer (smallest change; targets the measured L2 penalty).
2. Write-back and write-allocate (dirty bits, eviction, flush).
3. Metadata in the tag store (lifts the set limit).
4. 4 and 8 ways with a choice of replacement.
5. Next-line prefetch (optional).
6. Rerun the cache campaign over chosen combinations (for example
   write-through against write-back, with and without a buffer), with
   unchanged memory timing, and rewrite the public cache page.

Each step gets its tests, clock certification and a dated section here.

## 2026-10-19 — Addition: a behavioral cache (owner request)

**Why.** Interactive games need large caches and millions of simulated
cycles. Size and speed are separate needs:

- **Size** needs no behavioral form. The scalable cache already uses compact
  SRAM stores for tags and data; its limit (64 sets, 2 KiB per cache at two
  ways and 16-byte lines) comes from the valid and replacement bits kept as
  flip-flops in the structural controller. Step 3 above (metadata in the tag
  store) lifts it.
- **Speed** does. The controller is simulated gate by gate on every access.
  In the cache campaign (spot checks at the current code, `embedded`), a run
  without caches simulated about 4,700 cycles per second (topology B,
  `calls`: 94,793 cycles in 20 s), and a run with both cache levels about 290
  (D-ID, `sort`: 192,759 cycles in 671 s). At 290 cycles per second, ten
  million cycles take about ten hours.

**What it is.** A `CacheBehavioral<W>` registered as the behavioral
implementation of the existing contracts `memory.cache.<W>` and
`memory.cache.scalable.<W>`: the same pins, the same transaction and status
rules, the same cycle on which every request is accepted and every response
appears, for every parameter combination the structural cache supports at
that point (geometry and the behavior parameters of steps 1–5). It keeps
tags, valid, dirty and replacement state and data in host storage
(`MemoryStorage`, like the compact memory devices) and drives its lower port
as the structural cache does.

**Rules (lower-level first).**

- It comes **after** the structural cache is complete for the parameters it
  claims; it is an abstraction of that cache, not a replacement for it.
- An equivalence test runs the same request sequences (the recorded cache
  scenarios, the standalone traces S0–S2 and the five CPU topologies) on both
  forms and requires the same upper and lower transactions cycle for cycle
  and the same final memory, including four-state cases where the structural
  cache defines them.
- Its timing comes from the structural cache's proven delay table, like
  every compact part (`part_delay_tables`); the clock analysis must certify
  the same period with either form.
- It does not change any cache result: rerunning a campaign case with the
  behavioral cache must give the same cycles.

**Order.** Inserted as step 6, before the campaign rerun, which becomes step
7:

1. Write buffer.
2. Write-back and write-allocate.
3. Metadata in the tag store.
4. 4 and 8 ways with a choice of replacement.
5. Next-line prefetch (optional).
6. **Behavioral cache**, equivalent to the structural cache for every
   supported parameter combination, with a measured simulation speedup.
7. Rerun the cache campaign over chosen combinations and rewrite the public
   cache page; the long runs may use the behavioral cache once equivalence
   holds.

This section replaces the order given in the plan section above.

## 2026-10-20 — Step 1 done: write buffer

The owner asked to implement the plan in full, with minimal tests along the
way.

**Design.**
- `CacheConfig::write_buffer` (catalog parameter `write_buffer`): 0, 1, 2 or
  4 entries, read-write caches only; 0 keeps write-through exactly as
  before.
- `CacheWriteQueue<W>` (structural): per slot a valid bit and address, data
  and mask registers; slot 0 is the oldest. It sends slot 0 to the lower
  port while no fill uses it (`GRANT`), and every slot moves down when the
  response arrives; a new write enters the first free slot after the move,
  so one write can enter and one leave at an edge.
- Controller: a write is queued in its lookup cycle and answered then (like a
  read hit), its line updated in that cycle in the hit way if present. A
  read miss waits in the new `MISS_WAIT` state until the queue is idle, so a
  fill always reads up-to-date memory. A full queue blocks new requests.
  Invalidation waits for the queue to drain, so afterwards the backing
  memory holds every write.
- Posted writes: the backing memory's status for a buffered write is not
  returned; a reset discards queued writes.
- Experiment tool: `--il1`, `--dl1`, `--l2` take `KEY=VALUE,…` overrides of a
  cache's catalog settings. After the program stops, the run clocks on,
  uncounted, until every write queue has been idle for two cycles, then
  checks memory.

**Checks.** `CacheReadTest` gains a write-buffer stream against a reference
model (1, 2 and 4 entries; W = 8, 16, 32; one and two ways; FF and SRAM data
stores): every read value and read count matches, a buffered write is
answered within two cycles of acceptance, every write reaches the backing
memory exactly once and the final bytes match. The rig now counts lower
requests at every rising edge it drives. `RV32ITopologyTest` gains a
buffered D-ID run: `sort` (16 words, 2 iterations, `embedded`) verifies in
4,765 cycles against 6,481 without buffers (4 entries: also 4,765). The four
cache scenarios pass unchanged.

## 2026-10-20 — Step 2 done: write-back and write-allocate

**Design.**
- Settings `write_policy` (`through` | `back`) and `write_miss`
  (`no-allocate` | `allocate`), read-write caches only; write-back takes no
  write buffer.
- Dirty bits: `CacheLineState` with `dirty` (type `CacheDirtyLineState<S>`)
  keeps one per line beside the valid bit; a fill leaves the line clean, a
  write-back write hit sets it, a completed write-back clears it.
- A write hit in write-back updates its line in the lookup cycle and is
  answered then. A write miss without allocation goes around the cache
  (backing write, answered when it completes). With allocation a write miss
  fills its line like a read miss and is looked up again (`REFILLED_WRITE`,
  one cycle later so a clocked bank is not read and written at one edge).
- Eviction: a miss whose victim is valid and dirty first writes the victim
  back word by word (`EVICT_READ` reads the word from the data bank,
  `EVICT_SEND`/`EVICT_WAIT` write it to the backing memory); the victim's
  tag and set are captured at the lookup. The backing status of a
  write-back is not returned.
- Flush: an invalidation request starts a walk over every line
  (`FLUSH_READ` reads the tag and line state, `FLUSH_CHECK` writes a dirty
  line back); `INV_READY` rises once the walk is complete.
- Found by the reference test and fixed: the lower port's fields were chosen
  by the request's kind, so a write-allocate fill was sent as a write; a new
  state bit `SENDING_WRITE` now says what MEMORY_SEND is doing, and the data
  bank's write source is chosen by whether a write updates the line.
- Experiment: inputs `FLUSH_L1` and `FLUSH_L2` (held low, fixed in the clock
  analysis) drive the caches' invalidation; after the program stops the run
  flushes the L1s, then the L2, uncounted, and only then checks memory. This
  replaces the step-1 drain loop.

**Checks.** `CacheReadTest` gains a write-policy stream against a reference
model with dirty lines (write-through with allocate; write-back with and
without allocate; W = 8, 16, 32; one and two ways; FF and SRAM data): read
values, fills, final backing bytes after the flush and the number of backing
writes all match. `RV32ITopologyTest` (now also write-back with allocate),
`RV32IBinaryClockCLITest` and the four cache scenarios pass. D-ID `sort`
(16 words, 2 iterations, `embedded`): 6,481 cycles write-through, 4,765 with
2-entry buffers, 4,796 write-back with allocate, 5,006 write-back without;
all verified. The clock analysis certifies D-ID at 4,104 ps in all three
variants (limit unchanged: the instruction L1's front end).

## 2026-10-20 — Step 3 done, revised: a line-state store

**Plan change.** The plan said to keep the valid, dirty and replacement bits
in the tag words. That would rewrite a tag word on every hit (replacement)
and change the timing of every existing cache, including the published
campaign. Revised: the bits move to a separate **line-state store** (one
32-bit word per set in an ordinary memory device beside the tags), selected
by a new setting `line_state=store`; the default `registers` keeps the
flip-flop line state and every existing cache unchanged.

**Design.**
- Word layout: bits 0..7 valid per way, 8..15 dirty per way, 16 most
  recently used way (room for eight ways in step 4).
- The word is read with the tags (at the accepting edge, after a refill, and
  in the flush walk) and kept in `LINE_WORD`; each change the flip-flop
  version makes (eviction, fill, touch, write-back write, completed
  write-back) is applied to the copy and written to the store at the same
  edge. No write coincides with a read, so clocked banks are safe.
- The store's contents are unknown at power-up: after reset the controller
  writes an empty word to every set (`CLEAR_INDEX`, one set per cycle)
  before it accepts a request. An invalidation becomes the same clearing
  walk (after the flush walk in write-back); `INV_READY` rises when it is
  done.
- The controller no longer grows with the sets: up to 128 sets with
  gate-level stores, 4,096 with scalable ones (registers: 4 and 64).
- The bank is `LINE_STORE` (helpers `META_`), of the tag banks' technology.
  Its floorplan follows in step 4 with the floorplan for more ways.

**Checks.** `CacheReadTest` runs the write-policy and write-buffer streams in
store mode too, and a 256-set write-back cache with SRAM stores (scalable),
all against the reference model; the flip-flop mode and its tests are
unchanged. The cache scenarios and `RV32ITopologyTest` pass. A D-ID run with
store-mode write-back caches (L2 with 256 sets) verifies, `sort` in 5,050
cycles.

## 2026-10-20 — Step 4 done: 4 and 8 ways, a choice of replacement

**Design.**
- `ways` takes 1, 2, 4 or 8. New setting `replacement`: `lru` (exact, one
  or two ways; the default there), `plru` (tree of ways−1 bits per set; the
  default for four and eight ways), `fifo` (a pointer per set, advanced by
  every completed fill) and `random` (the low bits of a 16-bit XNOR LFSR,
  taps 16, 15, 13, 4, shared by all sets and stepped by every completed fill,
  so runs stay deterministic). An invalid way is always the victim first.
  `lru` with more than two ways is refused.
- pseudo-LRU: node n's children are 2n+1 and 2n+2; the root decides the way
  index's top bit; each bit points to the half that holds the victim; every
  access (read hit, completed fill, write into a line) points the nodes on
  its way's path away from it. For two ways it chooses as `lru` does.
- The controller now carries way indexes (log2 ways bits) instead of one
  "way one" bit: the hit way (encoded from the tag matches), the victim, the
  current way (`CURRENT_WAY_b`), the write way, the walked way of the flush;
  tags, data words and line bits are chosen by a multiplexer tree, and a
  decoder turns an index into one line per way. The two-way `lru` structure
  is kept as it was (the existing `CacheReplacementState`); other policies in
  flip-flop mode use a new per-set `CachePolicyBits<S,B>`; in store mode the
  bits sit in the line word from bit 16 (up to 7 bits).
- The cache floorplan (`visualizer-v2/cache_layout.py`) takes 1, 2, 4 or 8
  way rows and places `LINE_STORE` in a row under the tag stores with its
  `META_` helpers in a third helper row.
- Experiment tool: `--dl1 ways=4` without `replacement` chooses `plru`.
- Test rig: wire history is now disabled (the rig reads present values
  only). With history, `CacheReadTest` with the new eight-way cases was
  killed by its 1.5 GB memory cap.

**Checks.**
- `CacheReadTest` passes (6 min 18 s). It adds eight write-policy streams
  against the reference model, which now models each policy: 2-way plru; 4-way
  plru, fifo and random; 8-way fifo (store, SRAM data), plru (one set) and
  plru (store, write-back). Also the validation of ways and replacement, and
  the round trip through catalog parameters.
- ctest `RV32ITopologyTest`, `RV32IBinaryClockCLITest` and the four cache
  scenarios: 6/6 pass.
- D-ID `sort` (16 words, 2 iterations, `embedded`, 4,104 ps), all verified:
  - write-through, default caches: 6,481 cycles, unchanged;
  - write-back with allocate: 4,796, unchanged;
  - data L1 with 4 ways × 4 sets (plru, or fifo): 6,481;
  - data L1 with 8 ways × 2 sets, random, store mode: 6,481;
  - 4-way write-back data L1 and an 8-way, 8-set plru L2 in store mode: 4,802.

  The small sort fits either geometry, so the equal counts are expected.
- Clock analysis, D-ID program 20: 4,104 ps certified for the default caches,
  and for a 4-way data L1 with an 8-way fifo store-mode L2. The limit is
  unchanged (the instruction L1's front end).
- The geometry part of `CacheLayoutTest` passes for 1/2/4/8 ways, with and
  without the line store, run directly in Python.

**Not run yet.** The Python module and probe were not rebuilt: `TestsBinding.cpp`
includes the cache header and needs about 2.8 GB to compile, and only 1.2 GB
was free. So these checks wait for a memory-gated run: the full
`CacheLayoutTest`, `VisualizerStaticLayoutTest` and the browser checks. The
saved layouts of cache controllers probably need regenerating: their keys
hash child names, and steps 1–4 added children (for example `SENDING_WRITE`).
That run will show it.

**Decision: next-line prefetch deferred.** The controller serves one request
at a time. A prefetch would hold the controller and the lower port right
when the core's next request comes, and a sequential stream already gets
the next words from the line itself (and a burst fill). A useful prefetch
needs a request that can be served while the prefetch runs, which is the
"several requests in flight" item this plan leaves out. Step 5 is dropped
from this plan; a later record can take it up together with that item.
Next: step 6, the behavioral cache.
