# Cache system plan

Started: 2026-10-14. Status: open. Code state: `github-pages` at `448e4e3`, with pre-existing worktree changes.

## Summary

The owner chose **caches, game programs, then AI accelerators**. The adopted
order within cache work is: (1) design and build a standalone structural cache
with a contract that a scalable cache can share; (2) measure small structural
memories, calibrate scalable-memory timing, and join the two RV32I scalable
memory images behind one address space; (3) build the equivalent scalable
cache and integrate both cache forms. The core keeps separate instruction and
data request paths. Existing memory endpoints can serve both the cache's
internal tag/data stores and its backing storage through the same REQ/RSP
contract; new cache lookup/refill logic is required. Cache geometry, write
policy, arbitration and timing rules below are design proposals until
implemented and measured. A detailed standalone structural-cache proposal
appears in the final dated section. No cache or
shared-RAM integration has been implemented for this record. The broader
context and earlier alternatives are in the
[future steps plan](2026-10-14-future-steps-plan.md).

## 2026-10-14 — Current evidence and constraints

- `src/modules/rv32i/RV32IMemorySystem.cpp` currently connects `IMEM_` and
  `DMEM_` to separate endpoints. `src/tests/RV32IProgramRoot.cpp` constructs
  two stores. The default `MemoryConfiguration` gives each 65,536 32-bit
  words. Existing program results and layouts must remain available as an
  uncached regression path.
- [The transaction port](../../site-docs/memory/3.2.md) has byte addresses,
  aligned 32-bit reads, four write lanes, one outstanding request, held
  responses and no burst command. A multiword line fill therefore needs one
  read per word. Reset clears interface state, not RAM, and cannot undo a
  native write already launched.
- [Advanced-core memory waits](../../site-docs/rv32i/4.6/memory-waits.md)
  already handle delayed acceptance and responses on both request paths,
  including stale speculative fetches. The single-cycle and baseline cores
  presently require zero-wait FF endpoints. An arbitrated shared port belongs
  on the advanced-core platform first.
- FF memory is already zero-wait in its default configuration. SRAM and DRAM
  can benefit in modeled cycles from repeated cache hits; DRAM timing also
  varies with row and refresh state. A cache is not assumed to improve host
  simulation speed or every workload.
- No cache family or cache tests were found in `include/modules`,
  `src/modules`, or `src/tests/TestRegistry.cpp` at this code state. The
  [component-profile contract](../reference/unified-component-profiles.md)
  requires a visible structural form before a compact behavioral form and
  separately established timing for each supported configuration.

## 2026-10-14 — Proposed platform and cache contract

```text
advanced core IMEM ── [I-cache or bypass] ──────────┐
                                                    ├─ arbiter ─ shared RAM
advanced core DMEM ── address decode ── [D-cache or bypass] ─┘
                                  └─ uncached MMIO (reserved for later)
```

1. **One physical RAM image.** Load ELF segments once before execution. Use
   one address map for fetch, load and store; check image, stack and heap
   bounds against the *shared* capacity. Keep the old two-endpoint root as a
   regression target. Do not change existing memory family IDs or core pins
   merely to introduce a new system root.
2. **Two logical CPU paths, one lower RAM port.** A small structural arbiter
   grants one request at a time, remembers its owner through the response,
   and routes the response only to that owner. Propose round-robin grants at
   transaction boundaries when both requesters wait. Specify simultaneous
   read/write ordering by grant order, held VALID/READY behavior, fault
   forwarding, reset/drain behavior and absence of duplicate stores. An
   independent reference model should compare the accepted transaction
   sequence and final RAM image.
3. **Separate optional L1 instruction and data caches.** This uses the
   existing two core paths and permits an instruction hit while data traffic
   uses RAM. Both caches see the *same* backing address space. Use the same
   32-bit REQ/RSP upper and lower contract, with at most one outstanding
   request per cache. A bypass profile connects both paths directly through
   the arbiter. The first proposed visible geometry is four direct-mapped
   lines of two words each (32 bytes of data per cache); one-word and longer
   lines can then be measured instead of assumed superior.
4. **Small structural design first.** Show address tag/index/word selection,
   tag comparison, valid state, data words, response holding, and a refill
   controller built from the existing gate, register and memory primitives.
   The I-cache serves reads only. A miss issues one lower-port READ per line
   word, installs a line only after every required read succeeds, and
   returns the requested word or the lower-port error. A failed or
   interrupted fill must not leave a partly valid line.
5. **Conservative D-cache writes.** Propose write-through and
   no-write-allocate initially. A store sends the original positioned data
   and four-bit mask to RAM and reports completion only when RAM completes.
   On success it updates matching cached bytes, if the line is present; on
   failure it leaves the cached line unchanged. A read miss fills a line like
   the I-cache. This avoids dirty eviction and makes RAM state observable to
   later devices, although stores still use lower-port bandwidth. A later
   write-back mode would be a separate contract and milestone.
6. **Cacheable RAM only.** Define cacheable ranges and reject or route other
   addresses before any line fill; never prefetch a device register. Reserve
   an uncached path for future input/display MMIO, but do not invent device
   addresses in this cache stage. A line crossing the RAM boundary must not
   read outside it. Preserve alignment, range, unsupported-operation and
   unknown-field statuses. Treat X/Z payloads according to the existing
   endpoint contract, and make unknown control signals a protocol violation.
7. **Instruction visibility is limited at first.** Preload the image before
   enabling the core and invalidate caches after any host image replacement.
   First program tests use immutable code. A D-port store to a fetched code
   address can leave a separate I-cache and prefetched instruction stale;
   [RISC-V Zifencei](https://docs.riscv.org/reference/isa/v20240411/unpriv/zifencei.html)
   requires `FENCE.I` for the architectural store-to-fetch guarantee. The
   current core does not implement it. Guest self-modifying code and DMA
   coherence remain outside this milestone until an explicit flush/order
   contract is implemented and tested.

This refines the earlier [candidate cache order](2026-10-14-future-steps-plan.md):
the single shared RAM and arbiter are a prerequisite, while I- and D-cache
integration remain separate milestones. It does not replace the owner's
cache-before-games priority.

## 2026-10-14 — Milestones and acceptance gates

| Milestone | Deliverable | Acceptance evidence |
| --- | --- | --- |
| C0 — uncached baseline | Pin the advanced-core workloads, memory images, address limits, and timing/traffic measurements; define cacheable RAM versus reserved device space. | Existing uncached results reproducible; no cache speedup claim from this baseline. |
| C1 — shared RAM | Small structural two-requester arbiter and one RAM-backed program root; a scalable RAM root can use the same port contract. | Both ports read one image; a direct instruction-port read after a completed D-store sees the new bytes when earlier fetches are drained; simultaneous requests are serialized in documented order; ELF and stack bounds checked; advanced-core programs match a unified-memory oracle. |
| C2 — small I-cache | Read-only visible cache, first geometry and bypass mode, with registered hit/miss/fill counters if observation does not alter the public port. | Hit, miss, conflict, refill, fault, reset, held response, speculative wrong-path fetch and line-boundary tests pass; full programs agree with C1 for immutable code. |
| C3 — small D-cache | Visible cache with masked write-through/no-write-allocate stores and uncached-range bypass. | Byte/halfword/word writes, zero mask, failed write, read-after-write, eviction, simultaneous I/D traffic, reset during an accepted store, and device-range non-prefetch agree with the reference model. |
| C4 — scalable forms | Compact cache implementations selected through component families/profiles; parameterized capacity with explicit bounds and timing. | Structural/behavioral equivalence at each shared small geometry; larger forms preserve the same transaction behavior and have independently justified timing, not a copied tiny-cache delay table. |
| C5 — integration study | Repeat C1 workloads with cache bypass and selected I/D configurations over FF, SRAM and DRAM backends; add one visualizer scenario per inspectable milestone. | Record hit/miss/eviction counts, lower reads/writes, modeled cycles/ticks, host seconds and simulator events. Select a practical game profile from measured correctness and cost, with no required speedup on zero-wait FF. |

At each milestone register focused scenarios in `src/tests/TestRegistry.cpp`,
check structural and behavioral modes where both exist, retain the uncached
program comparison, update the relevant chapter 3/4 public component pages,
and recertify core clock timing when hierarchy changes. Run serial builds as
required by `AGENTS.md`. The browser/WebAssembly checks become necessary when
the new scenarios or visualizer paths are added.

## 2026-10-14 — Decisions still needed

- Confirm separate I/D caches as the first visible hierarchy, or choose a
  single shared L1 after C1. The split proposal matches the core's two
  paths but needs a later instruction-visibility solution.
- Confirm the first cache geometry and hit-cycle target after timing the
  structural circuit. Two-word lines cost two lower READ transactions on
  every miss because the port has no burst operation.
- Confirm write-through/no-write-allocate for C3 and whether the first game
  profile may run uncached on data when store traffic dominates.
- Decide whether `FENCE.I` is needed before games; immutable guest code makes
  it deferrable, while code loading or rewriting during execution does not.
- Define later DMA sharing before graphics/GPU/AI devices can write cached
  buffers: an uncached buffer range or explicit maintenance is a simpler
  initial rule than hardware coherence.

Verification this date: current source, registered tests, public memory and
advanced-core pages, and the open future plan were inspected. The RISC-V
`FENCE.I` requirement was checked against the official specification. No
cache was built, no program was run through a unified RAM, and no performance
or timing certificate was produced for this proposed design.

## 2026-10-14 — Is an arbiter the right shared-RAM choice?

The question concerns the **lower RAM port**, not the number of CPU request
paths. If two requesters target one endpoint that accepts one outstanding
transaction, a component must choose which request goes first and send the
response back to its owner. A two-requester arbiter does exactly that. It
lets the existing advanced-core `IMEM_`/`DMEM_` ports and single-request
memory endpoint keep their contracts. It is the recommended first shared-RAM
adapter, subject to the C1 measurements; it is not a decision to make the
whole future platform a one-port bus.

Real systems use this mechanism at **contended shared resources**. AMD's
[AXI interconnect arbitration guide](https://docs.amd.com/r/en-US/pg059-axi-interconnect/Transaction-Arbitration)
describes round-robin selection among equal-priority requesters. Its
[System Cache interface guide](https://docs.amd.com/r/en-US/pg118-system-cache/Generic-AXI4-Slave-Interface)
explicitly offers either an interconnect sharing one cache interface or
multiple interfaces for multiple masters. [Chipyard's memory hierarchy](https://chipyard.readthedocs.io/en/stable/Customization/Memory-Hierarchy.html)
has separate L1 instruction/data caches, a system interconnect, a shared L2,
and one or more outer memory channels. These are concrete examples of
arbitration and parallelism coexisting; they do not prescribe our exact
microarchitecture.

| Backing design | What happens when fetch and data need RAM together | Fit here |
| --- | --- | --- |
| One RAM port plus 2:1 arbiter | One goes first; the other waits. Separate L1 hits can still complete without reaching this port. | Reuses today's endpoint and advanced-core wait logic; simplest visible first system. |
| True two-port RAM/controller | Both may proceed, except for defined same-address conflicts or internal limits. | Needs a new multiport memory contract, conflict ordering and structural/equivalence evidence. Existing one-request endpoint cannot supply this by wiring alone. |
| Banked RAM or several channels | Different banks can work in parallel; same-bank requests still contend. | Useful if measured traffic justifies the extra routing and memory components. |

The cost of the first option is explicit: before caches, a data access may
delay instruction fetch; after caches, simultaneous misses and write-through
stores still contend. A fair grant rule prevents permanent starvation under
continuously waiting requesters **at transaction boundaries**, although a
slow or blocked transaction can still make the other side wait. C1 should
record concurrent-request frequency and arbitration wait cycles, then C5
should compare those waits with hit rates and total modeled/host time. If
contention dominates the intended game workload, revisit a two-port or
banked backing memory without changing the CPU's two request paths or the
cache contracts.

This research supports the C1 arbiter as a **scoped initial choice**; the
owner has not yet selected the final backing-port count or arbiter policy.
Checked current core wiring, registered memory tests and public REQ/RSP
contract alongside the cited vendor/project documentation. No implementation
or performance comparison was run for this follow-up.

## 2026-10-14 — Owner revision: structural cache, timed shared RAM, scalable cache

This **replaces the implementation order** in the earlier “Proposed platform
and cache contract” and C0–C5 milestone sections. Those sections remain as
dated reasoning, not the current schedule. The owner adopted the three-stage
order in the Summary. The detailed mechanisms below are proposed acceptance
criteria, to be confirmed by implementation evidence.

The cache is a component in its own right. A small test circuit can put it
between a request generator and any compatible memory endpoint before the
RV32I memories are merged. The same upper and lower 32-bit, byte-addressed
REQ/RSP transaction contract then lets the structural cache later sit on the
unified system without being redesigned. The textbook *Computer Systems: A
Programmer's Perspective*, chapter 6, motivates making sets, ways and block
size explicit and testing spatial locality, conflicts and replacement rather
than assuming a cache helps every stream. Chipyard's
[memory hierarchy](https://chipyard.readthedocs.io/en/stable/Customization/Memory-Hierarchy.html)
is one example of separate I/D caches over shared outer memory; it does not
set this project's port count or cache policy.

### Stage 1 — Specify and build the structural cache

1. **Freeze one reusable contract.** Use the existing endpoint's upper and
   lower REQ/RSP pin widths, handshake, statuses, reset rules, byte addresses
   and four write lanes. Reads and writes are cache operations; define how
   PROGRAM and ERASE return the existing unsupported status. Document a valid
   geometry as `sets × ways × words_per_line × 4` data bytes, with powers of two and
   bounded structural sizes. The proposed first policies are `read_only`
   for instruction use and `read_write` for data use under one parameterized
   cache contract, not two incompatible designs. Define line invalidation
   and the rules for an in-flight fill before either form is built; reset
   invalidates tags but cannot cancel an already accepted backing write.
   Reserve distinct catalog IDs for the small expanded cache and its later
   scalable counterpart while keeping their public pins, parameters and
   observable behavior identical. A `BuildProfile` chooses fidelity within
   a family; cache presence or bypass is a system wiring choice.
2. **Build the visible data path and controller.** Start with a one-word,
   direct-mapped read cache, then add a two-word line and a two-way small
   configuration to exercise a real victim choice. Make tag, index, word
   select, valid bits, comparators, data storage, replacement bit, response
   holding and one-outstanding-miss refill sequence inspectable. Use gates,
   registers and the project's structural memory primitives. A refill reads
   each word through the current single-word lower port and sets valid only
   after all required words succeed. A fault or invalidation during a fill
   cannot expose a partly installed line. For two ways, a deterministic
   one-bit per-set victim rule is proposed; record the exact rule before
   writing reference tests.
3. **Add the data-cache write path against a standalone backing endpoint.**
   Propose write-through and no-write-allocate for the first version. Forward
   each masked store once, acknowledge it only after lower completion, and
   update matching cached bytes only on success. Keep the upper response
   stable until accepted. For a non-cacheable address, a system decoder must
   bypass the cache; the cache's own cacheable input is RAM. Do not fetch a
   line across the RAM boundary. If writes to an executable line are allowed,
   the I-cache visibility rule remains explicit: host image reload invalidates
   the I-cache; guest store-to-fetch synchronization needs a later `FENCE.I`
   or equivalent platform mechanism. The
   [RISC-V rule](https://docs.riscv.org/reference/isa/unpriv/zifencei.html)
   requires synchronization for that guarantee.
4. **Prove the part in isolation.** Register scenarios for cold/hot hits,
   line-end and address-end misses, conflict eviction, two-way replacement,
   masked and failed stores, read-after-write, lower backpressure, held
   responses, unknown/invalid requests, reset/invalidation during a fill and
   reset after an accepted store. Compare every accepted lower transaction,
   upper response and final backing image with an independent small cache
   oracle; use the same request streams for each structural geometry. Record
   hit/miss counts, lower traffic, modeled ticks and host time separately.
   Generate timing evidence for the supported small geometries and a safe
   clock before CPU integration. No scalable cache is claimed at this gate.

### Stage 2 — Measure timing and unify the scalable backing memories

1. **Measure before changing delays.** At matched small geometries, drive
   expanded FF, SRAM and DRAM devices/endpoints and their scalable forms with
   the same request streams and clock. Include the largest expanded 32-bit
   depth that fits the 4,096-cell limit (128 words), plus smaller sizes; vary
   address, read/write, masks, response backpressure, and DRAM row hit,
   conflict and refresh state. Record structural pin-path settling and safe
   clocks with `CircuitTimingAnalyzer`, and separately record accepted-request
   to response ticks/cycles with a transaction harness. Audit ROM and NOR
   flash separately if a shared timing path changes, including flash
   program/erase timing. Existing endpoint
   benchmarks measure host run time and total scenario ticks; they do not
   provide this per-access latency matrix. Use at least one measured size as
   a holdout when fitting any depth rule.
2. **Calibrate the actual gap.** The current FF scalable endpoint can answer
   in the request cycle if its path meets the clock; SRAM/ROM use timed phases,
   DRAM latency depends on row/refresh state, and flash operations have
   different durations. Sparse host-page lookup adds no simulated tick.
   Preserve those distinctions. Fix any measured mismatch in the applicable
   native sequencer, endpoint, delay table or large-depth scaling rule, then
   rerun the matched small cases. Above structural limits, label the chosen
   capacity extrapolation as an estimate and state its uncertainty; structural
   simulator ticks are **not** measured silicon nanoseconds. Recertify the
   platform clock if the modeled path changes. Use a genuinely slower timed
   backing configuration for cache benefit studies rather than inserting an
   arbitrary penalty into zero-wait FF memory.
3. **Join the two program memories.** Keep the advanced core's two REQ/RSP
   request paths and initially place a structural 2:1 transaction arbiter
   before one scalable RAM endpoint. Load the ELF image once, define RAM and
   reserved MMIO ranges and validate stack/heap/image bounds against the one
   capacity. The arbiter owns a grant until its response is routed, orders
   simultaneous requests deterministically, applies backpressure fairly at
   transaction boundaries, and never repeats a store. Test reset and a
   previously accepted write in flight. Preserve the old two-memory root as
   a regression configuration; the fixed-cycle cores need their own
   zero-wait solution before migration. A direct instruction-port read after
   a drained D-port store must observe the single backing image; speculative
   prefetched instructions and future I-cache hits have separate visibility
   rules. Run the stage-1 structural cache against this shared RAM through
   each logical path and compare advanced-core programs with a unified-memory
   oracle. Record arbitration waits to decide whether a banked/multiport
   backing is worth considering later.

### Stage 3 — Build and integrate scalable cache components

1. Implement the compact cache with the **same validated config, pins,
   transaction and maintenance semantics** as stage 1. Reuse the structural
   design's policy definitions for tag/index, victim, refill and masked
   write-through behavior; a host-side line array may replace a large gate
   array, but it must not invent a different hit, fault or completion rule.
   Expose its capacity and host resource bounds. Keep I and D as two instances
   of the common cache design, each optionally bypassed.
2. Run structural/scalable differential tests at every shared small geometry:
   compare upper responses, lower requests, backing image, invalidation,
   reset and fault traces under identical stimuli. Compare independently with
   the cache oracle. Derive compact-part timing from each supported small
   structural geometry; document estimated timing above it and recertify
   system clocks. Do not use a tiny cache's fixed delay for an arbitrarily
   large cache.
3. Integrate optional I/D caches with the advanced-core shared-RAM root.
   Compare cached and bypassed programs over FF, SRAM and DRAM at matched
   clocks and images, including sequential, strided, conflicting and
   write-heavy streams. Record correctness, hits, misses, evictions, lower
   reads/writes, arbitration waits, modeled cycles/ticks, host seconds and
   simulator events. A cache need not improve zero-wait FF cycles or host
   speed. Select a practical game memory profile only from these results.

**Stage gates:** stage 1 ends with a structural part and reproducible standalone
tests; stage 2 ends with an audited timing model, one address space and a
working structural-cache integration; stage 3 ends with structural/scalable
equivalence and program-level comparison. Update chapter 3/4 public
component and platform pages as each behavior becomes real, the component
catalog and timing tables with each new family, and visualizer/WebAssembly
scenarios when exposed there. Build serially. This plan update itself changes
no simulator behavior and runs no cache, memory-latency or program benchmark.

## 2026-10-14 — Feasibility of reusing the memory components

The owner asked whether a cache can be added by **composing existing memory
parts**, as endpoints compose a controller with a device, and whether its
storage/backing technologies can be chosen separately. Source review says
**yes, with a distinct cache controller and a small storage adaptation**:

```text
CPU REQ/RSP → cache lookup/refill/write controller
                  ├─ tag, valid and victim state
                  ├─ cache data arrays (FF first; SRAM option)
                  └─ lower REQ/RSP → existing MemoryEndpoint<T> → memory device
```

- `MemoryEndpoint::buildInternals()` in
  `src/modules/memory/MemoryController.cpp` already asks the catalog for a
  controller and a native device, then wires their native pins. The scalable
  endpoint keeps the controller visible while choosing a behavioral native
  `MemoryContainer`; the fused behavioral endpoint is a separate option.
  Reuse this **composition pattern**, not its device controller as the cache
  controller. A memory endpoint maps each request to storage; it has no cache
  tags, valid bits, victim choice, hit/miss logic or line refill.
- An existing structural `FFMemory<32>` can hold a small cache's data words
  and padded 32-bit tags; `FlipFlopCell` can hold valid/victim bits. Per-way
  tag/data arrays allow parallel lookup. FF and SRAM device families already
  offer structural and behavioral forms; their scalable native forms retain
  the same **per-technology** native pins. Thus the first scalable-cache
  implementation can retain the structural cache controller while replacing
  only the expanded storage arrays with scalable native memory components.
  A fused behavioral cache is optional later, after whole-cache equivalence.
- FF and SRAM native pins differ: FF uses `CLK`, `WE`, `READ_EN` and `RST`,
  while SRAM uses `CE_N`, `WE_N` and `OE_N` with its own access phases
  (`src/modules/memory/MemoryProtocol.cpp`). An internal `CacheArray` port or
  small per-technology adapter is needed to make **cache-array** technology
  swappable without rewriting lookup/refill control. Reset should invalidate
  cache metadata; stored data need not be erased. Using a full transaction
  endpoint separately for every tag/data bank is possible but adds its own
  handshake and controller timing to every hit, so it is not the recommended
  first array wiring.
- The **backing** technology is already swappable at the lower REQ/RSP port.
  Read-only I-cache traffic can use ROM/flash as well as FF/SRAM/DRAM.
  The proposed write-through D-cache needs an endpoint supporting ordinary
  WRITE: FF, SRAM or DRAM. ROM rejects writes; NOR flash uses PROGRAM and
  ERASE_BLOCK rather than ordinary RAM writes. Shared pins alone do not make
  these operations equivalent. Technology choice must therefore be checked
  against cache policy, fault behavior and measured timing.

This resembles real cache organization without claiming its physical speed:
[Ibex's instruction cache](https://ibex-core.readthedocs.io/en/latest/03_reference/icache.html)
has separate tag and data RAMs per way, and an
[AMD embedded-memory discussion](https://docs.amd.com/api/khub/documents/wV6kNgYx_LDKF0SpJtW32Q/content)
describes small SRAM caches over larger DRAM. In this simulator, the current
native SRAM model has timed access phases; an SRAM cache array is not
automatically faster than every backing configuration. Measure complete hit
and miss paths before calling it a faster L1.

**Proposed refinement to the three stages:** build the stage-1 cache from
existing small structural memory arrays and new structural tag/control
logic; stage 2 can attach it to any compatible lower endpoint while unifying
the CPU backing RAM; stage 3 first swaps only cache arrays for scalable
same-technology native devices under the unchanged controller, then considers
a fused behavioral cache if needed for host speed. Compare whole-cache
transactions and timing at matched small geometries after each substitution.
The technology adapters, family IDs and exact cache-array port remain design
choices. This was a source and documentation feasibility review; no cache
was assembled or run, and no latency comparison was measured.

## 2026-10-14 — Correction: endpoint replaceability also applies inside a cache

The owner pointed out that the preceding feasibility section stopped at
native-device pins. **This corrects its claim that a cache-array technology
adapter is required for replaceability.** At the REQ/RSP boundary, every
`MemoryEndpoint<T,32>` has the same pin layout and transaction handshake
(`protocolPins` in `src/modules/memory/MemoryProtocol.cpp`). The endpoint
itself translates to the technology's native pins. A cache can therefore use
separate 32-bit endpoints for its tag and data banks, as well as another
endpoint for backing memory, without a new FF-versus-SRAM pin adapter.

For a small structural cache, one possible wiring is:

```text
CPU → new structural cache controller
        ├─ tag MemoryEndpoint<FF,32> per way
        ├─ data MemoryEndpoint<FF,32> per way
        ├─ valid/victim bits built from existing storage cells
        └─ lower REQ/RSP → MemoryEndpoint<chosen backing technology,32>
```

The tag and data banks can receive parallel READ requests; the cache
controller waits for their responses, compares tags, selects the data way,
and handles a miss. For stage 3, it can select the corresponding
`scalable-endpoint` child contracts in place of expanded endpoints, selecting
their structural fidelity to keep each endpoint controller visible, while
keeping the cache controller and its external contract. Choose the child
endpoint's technology by its contract ID or cache configuration;
`BuildProfile` selects only the implementation fidelity. This is the same
composition principle already used by `MemoryEndpoint::buildInternals()`.
It removes the proposed **native-array adapter** from the initial path.

The replacement boundary has two practical qualifications. First, the
requested operation must be supported: cache tag/data banks need ordinary
WRITE, so FF, SRAM and DRAM endpoints qualify; ROM and NOR flash have
different write rules. A read-only cache's backing can still be ROM/flash.
Second, each internal endpoint has a one-outstanding-request handshake and
its own controller latency. Thus replacement keeps the wiring and
functional protocol, while hit time, area and simulator cost can change.
An FF endpoint can be zero-wait at a suitable clock; an SRAM endpoint follows
its access phases. Measure the complete cache hit path before choosing a
storage technology. Direct native-array wiring with per-technology adapters
is an **optional later optimization** if endpoint-based banks prove too slow
or visually noisy, not a prerequisite for the first structural cache.

This changes the recommended stage-1 implementation detail to endpoint-based
tag/data banks and the stage-3 scaling method to swapping those child
endpoints. The owner's three-stage order and the need for new structural
cache control are unchanged. Source review covered the catalog's expanded
and scalable endpoint factories and the common protocol; no cache assembly
or benchmark was run for this correction.

## 2026-10-14 — Detailed plan for the standalone structural cache

This expands Stage 1 of the owner-revised plan and applies the endpoint
correction above. The earlier native-array adapter is **not** a prerequisite.
The one-family recommendation below replaces Stage 1's earlier suggestion
of separate public IDs for expanded and scalable caches; the endpoint child
form is a parameter while cache fidelity remains a profile choice.
The mechanisms below are recommendations to implement and verify, not measured
results or an owner's selection of every policy. CPU integration and a compact
cache belong to later stages.

### Textbook model and reusable boundary

The local *Computer Systems: A Programmer's Perspective*, chapter 6,
§§6.3.1 and 6.4.1–6.4.7, describes `S` sets, `E` lines per set and `B` bytes
per line, giving `S × E × B` data bytes. A request selects a set, matches a
valid tag, then selects a word within that line. Its cold/conflict/capacity
misses, direct versus set-associative placement, and separate write/update
policies motivate our geometry and test streams. It also describes separate
instruction and data caches, but does not determine our port count or timing.
See [the repository textbook](../../books/Randal%20E.%20Bryant_%20David%20R.%20O%E2%80%99Hallaron%20-%20Computer%20Systems_%20A%20Programmer%E2%80%99s%20Perspective-Pearson%20%282015%29.pdf).

```text
requester -- REQ/RSP -- structural cache -- REQ/RSP -- chosen backing endpoint
                          |-- chosen tag endpoint per way
                          |-- chosen data endpoint per way
                          `-- visible valid/replacement state and control
```

The cache has **no built-in backing-memory technology**. Its lower port
forwards byte addresses to the compatible endpoint its parent connects. Tag
and data endpoints are children, each independently chosen by technology and
expanded/scalable storage form; control uses their common REQ/RSP pins only.
This is the same replaceability boundary that existing memory requesters use.
Replacement preserves pins and transaction meaning, not latency or supported
operations. Internal banks need READ and ordinary WRITE, so FF/SRAM/DRAM
qualify; ROM and NOR flash do not. A read-only lower backing may also be ROM
or flash; a writable lower backing must accept ordinary WRITE. Reject an
incompatible *internal* selection at cache construction, and validate the
external backing selection when assembling the standalone test root or a
later CPU system.

### Proposed external contract and geometry

- Register **one 32-bit cache family** (proposed ID `memory.cache.32`) with a
  structural implementation first. Keep identical public pins and parameter
  meanings for a later compact implementation. Cache-child technology and
  expanded/scalable form are configuration choices; `BuildProfile` chooses
  implementation fidelity at each path. Do not bind the controller to a child
  C++ class or add a second cache contract just to choose child storage.
- Keep the standard, unprefixed upper `CLK`, `RST`, `REQ_*`, `RSP_*` names,
  widths, version-2 edge handshake, statuses and one-outstanding rule. Add a
  `MEM_`-prefixed requester port for the lower endpoint, with forwarded
  `MEM_CLK`/`MEM_RST`. Bridge input clock/reset to those output pins through
  `Rewire`, as parent inputs cannot wire directly to their own outputs. Add a
  separate all-line invalidation request/ready handshake; transaction reset
  itself does not erase any memory contents.
- Proposed parameters are `sets` (power of two), `ways` (1 or 2 initially),
  `words_per_line` (power of two), `mode` (`read_only` or `read_write`), a
  line-aligned cacheable byte interval, and independently chosen tag/data
  endpoint technologies and forms. Each way's tag endpoint needs `sets`
  32-bit words; its data endpoint needs `sets × words_per_line` words. Derive
  child depths from geometry, validate each technology's row/timing settings,
  and reject expanded storage above its 4,096-cell limit. Backing-memory
  parameters belong to the parent, not the cache. Support only combinations
  that the structural control and tests actually cover.
- For byte address `A`, let `b = 2 + log2(words_per_line)` and
  `s = log2(sets)`. Line base is `A & ~(2^b - 1)`, set is the next `s` bits,
  tag is `A >> (b+s)`, and word offset is bits `[b-1:2]`. Tag-bank byte
  address is `4 × set`; data-bank byte address is
  `4 × (set × words_per_line + word_offset)`. A tag occupies one 32-bit
  endpoint word; its valid bit is separate structural state. Retain the full
  32-bit lower address. Four sets, two ways and two words per line give 64
  data bytes, plus tags and metadata.
- A line must fit wholly in the configured cacheable interval before a fill.
  An outer decoder will later route MMIO around the cache. Standalone
  out-of-range requests return a defined error without touching banks or
  backing. Match the endpoint's order for invalid opcode/address, required
  mask, alignment and capacity checks; specify mode/operation errors after
  those checks. Stored X data remain X on a successful read. Unknown control
  pins are protocol violations under the existing transaction contract.

### Structural transaction sequence

1. **Capture one upper request.** Reuse the structural
   `MemoryTransactionPort<32>` and `MemoryResponseBuffer<32>` if their
   START/FINISH contract fits. Use register families to retain accepted
   address, operation, data and mask; hold responses under backpressure.
   Child endpoints can accept and complete on the same rising edge, so the
   controller must observe same-edge completion as well as delayed replies.
   Conservative `REQ_READY=0` while busy is allowed.
2. **Look up the line.** Decode set/tag/word with existing adapters, gates,
   comparators and muxes. Read valid bits and send tag READs to each way in
   parallel, then read the selected way's data word after a known match.
   This avoids irrelevant reads from other data banks; parallel speculative
   data reads can be evaluated later. An endpoint fault or ambiguous valid
   tag cannot silently become a hit. Implement control with visible gate and
   register state, not a product C++ map storing cache contents.
3. **Fill on a read miss.** Prefer the lowest-numbered invalid way. If both
   ways are valid, propose a per-set most-recently-used bit: evict the other
   way, giving exact two-way LRU. Update that bit after each successful hit
   or fill, never after a failed transaction. One-way mapping needs no victim
   bit. Clear victim valid before changing contents. Issue one lower READ per
   line word;
   write each successful result through the chosen data endpoint. Write its
   tag and set valid **last**, after all words and the tag succeed. On a lower
   error, propagate its status; on an internal bank error, return `Device`.
   In both cases leave the victim invalid. Return the requested word only
   after a complete valid line exists. An invalidation request arriving during
   a fill waits for the active transaction, then clears valid bits before its
   own acknowledgement.
4. **Add data writes after read-only operation works.** Propose write-through
   and no-write-allocate in `read_write` mode. A write miss sends one original
   positioned, masked WRITE to the lower endpoint and does not allocate. A
   write hit waits for lower success, then applies the same mask to the
   matching data endpoint. A lower failure leaves the cached line unchanged.
   If backing succeeds but the internal update fails, invalidate that line
   before reporting lower success; backing is authoritative. Zero-mask writes
   retain endpoint semantics. `read_only` rejects WRITE; PROGRAM/ERASE are
   unsupported by this first cache. Freeze exact status precedence before
   implementation.
5. **Reset and invalidate safely.** Reset clears the public handshake and
   valid/replacement metadata, not backing or internal memory contents. A
   lower write accepted before reset may still complete; never replay or
   claim to roll it back. Suppress stale responses and wait for child
   endpoints to become ready before reuse. Accept all-line invalidation only
   at a safe transaction boundary, so it does not discard an upper request;
   give a waiting invalidation priority over a new upper request at idle.

### Build increments and proof gates

| Increment | Smallest result | Evidence before proceeding |
| --- | --- | --- |
| S1.0 Contract and wiring | One catalog contract, two reusable port bundles, validated geometry and technology choices, endpoint children selected by contract ID. | Invalid geometry, internal ROM/flash and expanded-cell overrun are rejected; build manifest shows chosen children and fidelities. |
| S1.1 One-word direct map | Read-only structural lookup and one-word refill, with small FF tag/data endpoints. | Cold miss, hit, conflict eviction, alignment/range/status, held response, immediate FF and delayed lower replies, no duplicate lower READ. |
| S1.2 Lines and ways | Two-word line, then two-way sets and deterministic replacement. | Second-word hit, exact lower READ count, LRU trace, boundary and each-word fault cases; partial line never becomes valid. |
| S1.3 Writable policy | Masked write-through/no-write-allocate under the same contract. | Write hit/miss, masks and zero mask, lower/internal failure, read-after-write, reset after accepted store, final backing bytes. |
| S1.4 Replacement and timing | Swap tag/data endpoint children independently to another writable technology; vary compatible lower backing. | Independent oracle agrees on upper/lower transactions and final bytes; timing differences are measured; analyzer supplies small-geometry paths and a safe clock. |

Use an **independent software oracle as test equipment**, not as the product
cache. Run identical deterministic streams across geometries and technologies.
Test child request/response backpressure, simultaneous tag replies and
same-edge FF completion. Record hits, misses, evictions, lower reads/writes,
modeled ticks and host seconds separately. Register one logical scenario and
focused contract/timing cases through `TestRegistry.cpp` and
`tests/CMakeLists.txt`; add catalog verification evidence only after the tests
exist. Then document actual pins, behavior, internals and edge cases in the
chapter 3 public reference. The standalone structural stage ends when more
than one compatible endpoint selection works and the oracle agrees on all
accepted transactions. CPU wiring, shared-RAM arbitration, a fused behavioral
cache and performance claims remain later work.

**Verification this date:** read the local textbook's organization,
associativity, write-policy and split-cache sections; reviewed the current
version-2 REQ/RSP page, endpoint catalog/assembly, transaction port,
structural wiring, component profiles and endpoint scenarios. This is a
planning change only. No cache source, test, timing table or public component
page was created, and no cache simulation or benchmark was run.
