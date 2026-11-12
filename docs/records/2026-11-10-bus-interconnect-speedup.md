# Bus interconnect speed-up

Started: 2026-11-10. Status: open. Code state: `alpha` at `1ee0600`, with
the uncommitted SRAM main-memory work of another session in the tree (not
touched by this record).

## Summary

Built in Part I as default-off options (2026-11-10, sections at the end):
level 2 without a two-deep router; measured in campaign 1 (§ 6.6). Making
it the default waits for the chapter 3–5 test update. Originally
scheduled as part of stage 3 of the roadmap's Part I. On the chapter-5 machine every
memory access pays bus waits that are not memory time: an instruction fetch
takes three cycles on a zero-wait memory, which is most of the games' 4.2
cycles per instruction. This record holds the possible fixes, their costs
and the decisions they need. It was split out of the
[GPU and AI accelerator roadmap](2026-11-09-gpu-and-ai-accelerator-roadmap.md)
on 2026-11-10: it is chapter-5 bus work, not GPU work. On 2026-11-09 the
owner left the interconnect as it is when closing the
[main-memory comparison](2026-11-08-games-machine-main-memory.md); that
decision stands until this record is taken up. Any fix that changes the
default machine changes every game's cycle count and needs a new
recording of the long games. The owner made it the new default
(2026-11-10). The source review of that day corrects the levels: real
one-per-cycle access needs a pipelined, in-order protocol (two requests in
flight) across the core's requester, router, arbiter and SRAM controller,
reaching chapters 3–5. The owner chose that, level 2 (2026-11-10).

## 2026-11-10 — The three waits

From the edge-by-edge trace of the
[main-memory comparison](2026-11-08-games-machine-main-memory.md#2026-11-08--why-ff-takes-42-cycles-per-instruction)
(bring-up program, FF memory, 3,714 cycles):

```
edge N    core offers a fetch      → router stores it          (wait 1)
edge N+1  router passes it on      → memory answers at once,
                                     the arbiter is not yet busy
                                     and does not pass it back  (wait 2)
edge N+2  core takes the answer
edge N+3  core offers the next fetch                            (wait 3)
```

| Wait | Part | Why it is there | Cost of removing it |
| --- | --- | --- | --- |
| 1 | `BusRouter<W>` registers each request ([§ 5.2.1](../../site-docs/system/5.2/router.md)) | Deliberate: it cuts the combinational path core → decode → arbiter → memory → back | The path gets longer, so the certified clock may slow down |
| 2 | `TwoPortMemoryArbiter` takes an answer only from the edge after the memory accepted the request ([§ 3.5.3](../../site-docs/memory/controllers.md)) | A design simplification | Expected none; to be checked by the clock certificate |
| 3 | The core's fetch requester offers the next fetch only in the cycle after it takes an answer | A design simplification | Expected none; changes core timing |

In that run, fetches account for 3.35 of the 4.17 cycles per instruction,
data accesses 0.42 and multiply/divide 0.40. Since the 2026-11-09
decision the games machine uses SRAM (one wait state at 4,500 ps), which
adds about one cycle to each access because no access overlaps another.

Two related limits of today's bus:

- One request is in flight per router; the router takes the next only after
  the answer is taken.
- The router passes only reads and writes (anything else answers
  Unsupported), so burst reads (op 4) do not cross the bus; a DMA master
  moves one word per request.

## 2026-11-10 — Possible fixes

Three levels, each to be measured; the cycle figures are estimates from
the trace, not results.

1. **Remove waits 2 and 3.** About 2 cycles per fetch, no expected clock
   cost.
2. **Pipeline the router.** Keep its registered request but let it take
   the next request while the previous answer is on its way (two in
   flight, in order), as AMBA AHB overlaps one transfer's address phase
   with the previous transfer's data phase. About one fetch per cycle at
   an unchanged clock. This is the smallest form of "several requests in
   flight".
3. **Remove wait 1 instead of pipelining.** Also about one cycle per fetch,
   with a likely slower clock. A fair comparison point, less like real
   buses.

Optional with any level: let the router pass burst reads, so a DMA master
moves a line per request.

## 2026-11-10 — Who it affects

- **The games and § 5.6–5.7 results.** Any fix on the default machines
  changes cycle counts, clock certificates and every recording. The long
  games took about 5 h to record in Compact and 27 h in Visual. If a fix is
  wanted as the default, doing it before the current SRAM re-recording is
  final saves one full re-recording.
- **The AI accelerator (roadmap Part I).** Its DMA and the CPU it is
  compared with in campaign 1 run on this bus. Without a fix, a DMA engine
  that offers requests back to back still pays waits 1 and 2 (about one
  word per two cycles), and the CPU baseline runs at today's cycles per
  instruction. Campaign 1 states the bus it ran on.
- **The GPU (roadmap Part III).** Barely: SMs reach L2 and memory through
  their own crossbar. Only the CPU-to-GPU link would use this bus.

## 2026-11-10 — Decisions for the owner

1. Whether to do this at all, and when (before or after the SRAM
   re-recording; before or after roadmap Part I).
2. Which level: 1, 2 or 3.
3. Options or new default: parameters of the router, arbiter and core
   with today's behavior as default (no re-recording; new machines turn
   them on), or the new behavior as default (all results and recordings
   redone).

## 2026-11-10 — Owner: back in the roadmap

The owner put the speed-up back into the roadmap: it is part of Part I,
stage 3, done before the accelerator's DMA and campaign 1. This record
stays its working record. Decision 1 above is answered (yes, in Part I);
decisions 2 (level) and 3 (option or default) remain open. Part I also
switches the RV32IM core to a one-cycle array multiplier, which changes
cycle counts too.

## 2026-11-10 — Owner: new default; source review (corrects the waits table and the levels)

The owner made the speed-up the **new default** and asked for a review
before choosing the level. Source read: `src/modules/memory/TwoPortMemoryArbiter.cpp`,
the router in `src/modules/device/DeviceBus.cpp` (`buildRouter`),
`include/modules/rv32i/RV32IMemoryRequest.hpp`, `site-docs/memory/transactions.md`,
`site-docs/rv32i/4.6/memory-waits.md`. Nothing was built or run.

**Wait 1, router.** `DeviceRouterControl` gives `REQ_READY = !BUSY`; the
accepting edge sets `BUSY`; the held request is offered in the next cycle;
`BUSY` clears at the edge where the answer is taken, so the next request
is accepted one edge later. The router passes the target's answer and the
master's `RSP_READY` straight through.

**Wait 2, arbiter.** `MemoryArbiterResponseRoute` gives
`MEM_RSP_READY = BUSY AND owner's buffer empty`. `BUSY` is a state bit set
by the accepting edge, so in the cycle a request is offered and accepted
the arbiter cannot take an answer, and a same-cycle answer waits one edge.
The source comment gives the reason for not passing the requester's
`RSP_READY` through: a stalled requester must not hold the shared memory.
A fix (`MEM_RSP_READY` also true while accepting, for the selected port's
empty buffer) adds a combinational path from the grant to the memory's
`RSP_READY`; whether any endpoint's `REQ_READY` depends on its `RSP_READY`
in the same cycle (a loop) must be checked first.

**Wait 3 is not a simplification (replaces the table above).**
`RV32IMemoryRequest` is a one-outstanding requester: it offers its next
request only after taking the previous answer. Directly on a zero-wait
endpoint that costs nothing (request and answer at one edge, § 4.6). Behind
a registered router the answer comes at least one edge after acceptance,
so a one-outstanding master reaches at best one request per two edges.
The table's "expected none" for wait 3 is wrong: removing it means a
master with two requests in flight.

**Corrected levels (replace "Possible fixes").**

- **Level 1, arbiter only:** about 2 edges per access instead of 3. Touches
  the arbiter, which § 4.7–4.8's shared arrangements also use.
- **Level 2, pipelined in-order protocol:** a master may have a second
  request in flight; answers return in order with no IDs; any part may
  still take one request at a time (pipelining is allowed, not required).
  Real reference: the Wishbone B4 pipelined mode, AMBA AHB's overlapped
  address and data phases. Needs, besides level 1:
  - the protocol rule of § 3.4 ("at most one transaction outstanding")
    rewritten, with the endpoint contract tests and `BusPortChecker`;
  - `RV32IMemoryRequest` with two in flight, shared by the RV32I advanced
    core and the RV32IM core; its stale-response discard after a PC change
    already exists and must cover the second request (single-cycle and
    baseline cores use the zero-wait port and are not affected);
  - the router accepting the next request at the edge its answer is taken
    (`REQ_READY = !BUSY OR done`), still holding one at a time;
  - the arbiter remembering which port owns each request in flight, so
    answers return to the right requester in order;
  - the clocked SRAM controller accepting a new request every cycle. Since
    the 2026-11-09 decision the games run on SRAM, and a single-outstanding
    SRAM endpoint would cap every access at about two edges, wasting most
    of level 2. DRAM, ROM, flash and the caches may keep taking one request
    at a time.
- **Level 3, router not registered:** dropped as an option; it lengthens
  the clock path and is less like real buses.

Estimated effect, to be measured: on FF or a pipelined SRAM, about one
access per edge, so the games' fetch share of CPI (3.35 of 4.17 on FF)
falls toward 1; data accesses still share the RAM port with fetches.

**Reach of a new default.** Results that change and are rerun: § 4.6–4.8
advanced-core runs on delayed or shared memories, § 5.6–5.7 and the long
game recordings, plus the clock certificates and delay tables of the
changed parts. The single-cycle and baseline cores' results stay.

## 2026-11-10 — Owner: level 2

Decision: **level 2**, the pipelined in-order protocol, as the new default,
including level 1's arbiter fix, a requester with two requests in flight,
the router and arbiter changes, and a clocked SRAM controller that accepts
a request every cycle. The results listed above (§ 4.6–4.8 advanced-core
runs on delayed or shared memory, § 5.6–5.7, the long games) are rerun
after it. Done in roadmap Part I, stage 3, before the DMA engine.

## 2026-11-10 — Implemented as options (Part I, stage 3)

The owner's goal for Part I forbids changing or running the earlier
chapters' tests for now, so level 2 is built as **options, off by default**;
making them the default comes with updating those tests and rerunning the
results listed above.

- Source review correction: the memory controller (`MemoryController.cpp`)
  already takes a new request at the edge its current one completes and
  holds the finished answer in its response buffer; the SRAM controller
  needs no change.
- Arbiter `pipelined=1` (`TwoPortMemoryArbiter::buildPipelined`, also the
  memory subsystem's `pipelined=1`): from idle, a same-cycle answer passes;
  the next request is accepted at the completing edge. No combinational
  loop: the lower memory's `REQ_READY` is registered-only.
- Router `pipelined=1` (`DeviceBus.cpp`): `REQ_READY = !BUSY or done`; the
  default router's gates keep their original construction order (names and
  layouts unchanged).
- Core `overlapped_fetch=1` (`RV32IMemoryRequest::buildOverlapped`): when an
  earlier-accepted fetch's answer is taken and the PC advances, the next fetch
  for `FETCH_NEXT_PC` is offered in the same cycle; a wrong guess is
  discarded by the existing address check.
- Bus description `"pipelined": true` turns on the routers and arbiters
  (omitted from the JSON when false, so existing configuration hashes stay).
- Not done: a two-deep router (an SRAM wait is not hidden).

Checks: see the [Part I record](2026-11-10-part1-accelerator-implementation.md).

## 2026-11-10 — Checks

Planning only. Read: the main-memory comparison and SRAM records, § 5.2.1
router page, § 3.5 controller page. No build, test or measurement.

## 2026-11-10 — Measured (campaign 1)

From the [Part I record](2026-11-10-part1-accelerator-implementation.md)
and § 6.6. Matrix multiply on the CPU alone (SRAM), array multiplier plus
pipelined bus against the chapter-5 bus and iterative multiplier: the same
729,492 instructions in 3,228,143 cycles instead of 6,557,573. Bus alone
(`PipelinedBusTest`): `demo-cpu` 54,455 → 47,809 cycles, `demo-n4`
69,671 → 60,184; memory-subsystem traffic one access per cycle on FF
(256 → 128 cycles for 128 accesses), about one per two on SRAM (384 → 257).
Also new: the test's DRAM and random-refusal cases pass on both arbiters.
The campaign's first DRAM matrix-multiply run hung, but not because of the
bus: the `unit` profile's DRAM has no refresh and a 1 ms retention (see the
Part I record); with a 64 ms retention it completed.
