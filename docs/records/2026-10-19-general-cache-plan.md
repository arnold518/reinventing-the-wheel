# General cache: policies and sizes as parameters

Started: 2026-10-19. Status: open (plan only, deferred by the owner; no work
started). Code state: branch `realistic-timing` at `f2d3108`.

## Summary

The current `Cache<W>` is general in geometry (word width, sets, ways, line
size, FF or SRAM stores, burst fill) but fixed in behavior: write-through,
no-write-allocate, one request at a time, no write buffer, two-way LRU at
most, at most 4 expanded or 64 scalable sets. This record plans making the
behavior parameters too: write buffer, write-back, write-allocate, more ways
with a choice of replacement, many more sets, optionally next-line prefetch.
Several requests in flight and multi-bank DRAM are out of scope for the
cache itself. The owner asked to record the plan and not to start it now.
Next step when resumed: the write buffer.

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
