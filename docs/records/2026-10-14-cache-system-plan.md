# Cache system plan

Started: 2026-10-14. Status: open. Code state: implementation on `alpha` at starting commit `1d861c0`; earlier planning began on `github-pages` at `448e4e3`.

## Summary

The owner chose **caches, game programs, then AI accelerators**. Structural
`Cache<W>` exists for W=8/16/32 with independent FF/SRAM tag and data stores,
expanded or scalable storage, line fill, two-way LRU, invalidation and
write-through/no-write-allocate. Four public examples now show direct mapped,
two-way, scalable and connected L1/L2 configurations; their focused native,
Pages/WASM and browser checks pass. An optional advanced RV32I test path
connects I/D caches to separate or shared backing RAM through an arbiter.
Short program tests pass, but the small ROM/SRAM comparison showed more CPU
cycles with caches, so performance benefit remains unproven. No fused
behavioral cache, coherent invalidation, MMIO bypass or certified CPU clock
exists. This remains an open cache-system topic. Earlier dated sections are
planning evidence; newer dated sections record implemented behavior and
corrections. The broader context is in the
[future steps plan](2026-10-14-future-steps-plan.md).

The 2026-10-15 timing-model review below distinguishes agreement with the
small digital structural circuits from calibration to physical memory. It
sets the comparison order: direct endpoint versus cache plus the **same**
endpoint, first as a standalone requester and then with a matched CPU root.
The later delay-rule correction recognizes existing gate-versus-compact timing
tests as sufficient to begin exploratory cache comparisons now. The experiment
protocol below specifies the paired topologies, workloads and result gates.
The 2026-10-16 refinement makes five CPU topologies the primary campaign;
single-port L1 variants are targeted follow-ups. The latest 2026-10-16 section
below replaces the earlier FF/SRAM/DRAM performance sweeps with one fixed
technology stack: SRAM L1 and L2 cache banks over DRAM main memory. It defines
the small structural check, three standalone paths, five CPU paths, measurement
rules and completion gates. No cache-speed result has been measured.
A later 2026-10-16 review probe found that, with current timing constants,
an SRAM-bank L1 hit (14 cycles at period 102) is slower than a direct DRAM
read to an open row (4) or a row conflict (10). The campaign waits on an
owner decision about that hit path and on the separate
[memory timing realism review](2026-10-16-memory-timing-realism-review.md).
The campaign is deferred until the
[realistic-timing plan](2026-10-16-realistic-timing-plan.md) is finished. The
latest section is the current experiment plan: `embedded` and `desktop`
timing profiles, datasheet DRAM rows, geometry frozen after a latency check,
and traces defined relative to cache sizes.

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

## 2026-10-14 — Proposed first implementation slice

This narrows S1.0 and S1.1 above into one reviewable first deliverable. It is
**proposed**, not implemented or benchmarked. The aim is a working standalone
read cache, not an empty catalog shell.

- **Fixed first test geometry:** 32-bit words, 4 sets, 1 way, 1 word per line:
  16 data bytes. Address bits `[3:2]` select the set; bits `[31:4]` are the
  tag. Set 0 sees both addresses `0x00` and `0x10`, so a conflict is easy to
  demonstrate. The initial cacheable interval is `[0x00, 0x40)`, backed by a
  16-word endpoint. Geometry remains a construction parameter and the
  structural design must not hardcode four sets as its public contract.
- **One new public component:** the proposed `memory.cache.32` structural
  family, with an ordinary upper REQ/RSP endpoint-shaped port and a lower
  requester-shaped REQ/RSP port. It handles READ and returns a defined error
  (`Unsupported`, after normal request validation) for WRITE, PROGRAM and
  ERASE in this first mode. Keep the lower backing as an
  external component selected by the test root or later system.
- **New internal structural logic:** address/set/tag decoding, tag equality,
  four valid bits, a one-request lookup/refill control sequence, and wiring
  that holds the upper response and never issues a duplicate lower READ.
  These are named, inspectable subcircuits of the cache; they need not be
  separate public families unless a genuinely reusable contract emerges.
- **Reuse existing parts:** one 4-word, 32-bit tag `MemoryEndpoint`; one
  4-word, 32-bit data `MemoryEndpoint`; `Register`/state bits, gates, muxes
  and adapters for control; and the existing structural transaction frontend
  and response buffer if their handshake fits. The tag/data endpoints need
  READ for lookup and WRITE for refill even though the upper port is read
  only. Start with expanded FF endpoint children. Build a standalone test
  root with an external expanded FF backing endpoint. No new cell array,
  native-memory adapter or native memory technology is needed.
- **Replacement proof in the first deliverable:** build the *same cache
  controller* after changing one internal endpoint child at a time from FF
  to a compatible small SRAM endpoint; check identical accepted values and
  lower READ traces at a safe clock. Also drive the unchanged cache through
  a delayed compatible backing endpoint. Different response timing is
  expected. The first gate therefore demonstrates actual replacement, not
  only a parameter that has never been exercised.
- **Minimum observable sequence:** read `0x00` (miss, exactly one lower READ),
  read `0x00` again (hit, zero lower READs), read `0x04` (different set),
  read `0x10` (conflict in set 0), then read `0x00` (miss again). Add reset and
  all-line invalidation, upper response backpressure, delayed lower response,
  misalignment, out-of-range and backing-fault cases. Compare upper responses,
  lower accepted requests and final backing bytes with an independent oracle.

The second increment adds two-word lines and then a second way, so word
selection and replacement have separate evidence. The third increment adds
writes. In this first slice there is no CPU integration, arbiter, write policy,
scalable cache representation or speedup claim.

**Evidence this date:** the existing memory protocol and endpoint classes,
`MemoryTransactionPort`, profile/catalog API, structural wiring and current
plan were rechecked. This is a scope refinement only; no source or test was
implemented and no simulator test was run.

## 2026-10-14 — Clarification: textbook organization versus the first example

The owner's question exposes a limitation in the preceding first-slice
wording: a four-set test alone does **not** establish a general cache. This
section sharpens its acceptance gate without changing the staged order.

The textbook's reusable organization is `(S, E, B, m)`: `S` sets, `E` lines
(ways) per set, `B` bytes per line and `m` address bits. Our first contract
fixes `m=32` because the existing memory transaction address is 32 bits, and
`B=4 × words_per_line` because each transaction word is 4 bytes. Within
bounded supported sizes, `S`, `E` and `words_per_line` determine *built
structure*, not only metadata on a four-line fixed circuit:

- `s=log2(S)`, `b=log2(B)`, `t=32-s-b`; address bits split into
  `[tag:t][set:s][byte-in-line:b]`. The low two byte bits are zero for an
  aligned 32-bit request; any remaining block-offset bits select a word.
- Each way owns `S` tag words, `S × words_per_line` data words and `S`
  valid bits. Total **data** capacity is `S × E × B`; tag and valid storage
  are real overhead but are not included in textbook data capacity.
- The number of way comparators, child endpoint banks, valid bits, address
  selectors, refill transfers and victim-state bits must be constructed from
  geometry. The same control rules apply at every supported geometry. Two
  ways can use one per-set LRU bit; more ways require a separately defined
  replacement mechanism before they are supported.
- Endpoint choice is independent of `(S,E,B,m)`: changing FF to SRAM for a
  tag or data bank must not change address mapping, hit/miss decisions or
  lower transaction order. It may change latency. The lower endpoint is an
  external choice and is not counted as cache capacity.

**Revised proof gate:** keep `S=4,E=1,B=4` as the easiest first visual example,
but run the first read-cache controller also with `S=1` and `S=2` at `E=1,B=4`
before calling its set count configurable. This tests the zero-index-bit case
and a different index width. The next increment must run `S=4,E=1,B=8`
(two words per line) to prove block-offset selection and two lower READs per
miss, then `S=4,E=2,B=8` to prove way matching and replacement. At each
geometry compare the same address-mapping formula, valid/tag decisions,
requested data and accepted lower traffic with the independent oracle.
Construction must reject unsupported values rather than silently behaving as
the four-set example. Later larger geometries need their own resource and
timing bounds; this is a 32-bit, bounded implementation of the textbook
organization, not every possible cache shape.

This is a plan correction only. No parameterized cache has been built or
verified yet.

## 2026-10-14 — Exact structural component inventory and endpoint fit

This refines the first-slice plan in response to the owner's request for an
explicit component build order. Names are proposed; contracts and behavior
must be checked in implementation before catalog registration. This section
adds no code.

### What an exact endpoint connection means

`protocolPins()` gives every 32-bit `MemoryEndpoint` the same transaction
fields: `CLK`, `RST`, `REQ_VALID`, `REQ_OP[2:0]`, `REQ_ADDR[31:0]`,
`REQ_WRITE_DATA[31:0]`, `REQ_WRITE_MASK[3:0]`, `REQ_READY`, `RSP_VALID`,
`RSP_READY`, `RSP_READ_DATA[31:0]`, `RSP_STATUS[2:0]`. A requester has the
opposite direction for transaction fields. The proposed cache upper bundle
uses the endpoint direction, while each internal bank bundle and the lower
backing bundle use the requester direction. `MEM_` is only a name prefix on
the lower bundle; clock/reset are forwarded. No technology-specific native
pins enter cache control and no FF/SRAM/DRAM adapter is needed.

This is **port compatibility**, not identical whole-component shape: a bare
`MemoryEndpoint` has one transaction port; a cache has an upper port, lower
port and maintenance pins. The `Cache32 + backing endpoint` assembly presents
an endpoint-shaped upper port to the requester. The cache translates full
backing byte addresses into small *local* tag/data endpoint byte addresses;
it forwards full byte addresses to the backing endpoint. Every child endpoint
still obeys its one-outstanding-request and held-response rules. Multiword
fills are separate single-word transactions, as the current port has no burst.
Use FF endpoints with default `always_ready=false` initially; their optional
`always_ready=true` mode imposes an extra requirement that `RSP_READY` stay
high. Vary that mode only after the ordinary handshake path passes.

### Components, in build order

| Order | Component | New or reused | Responsibility in the first read-cache slice |
| --- | --- | --- | --- |
| 1 | `Cache32` contract and configuration | New public family and parameter validator | Define upper/lower REQ/RSP bundles, invalidation handshake, `S/E/B` limits, 32-bit address rule and endpoint child choices. Its structural form is the assembly below. |
| 2 | `CacheTagMatch32` | New internal structural subcomponent | Compare the requested padded tag against the selected tag endpoint's 32-bit response, qualify it with that line's valid bit, and flag an ambiguous comparison. The controller checks endpoint status. Build from existing splitters, XOR/OR/NOT/AND gates; no stored data. |
| 3 | `CacheLineState` | New internal structural subcomponent | Hold one valid bit per set/way using existing register/storage components. Clear on reset/invalidation, clear victim before refill, set only after completed tag/data writes. Add per-set two-way replacement bits when `E=2` is built. |
| 4 | `CacheController32` | New internal structural composite | Capture one upper request; derive set, tag, local bank addresses and lower line address; sequence tag READ, hit data READ, miss backing READ, data WRITE, tag WRITE and response. Reuse the existing `MemoryTransactionPort<32>`/response buffer for upper handshake if its START/FINISH contract passes the focused check. Construct state from existing registers, gates, muxes and bit adapters. |
| 5 | Tag and data banks | Reuse existing `MemoryEndpoint<technology,32>` contracts | One tag endpoint and one data endpoint per way. For `S=4,E=1,B=4`, each is four 32-bit words. Both must accept READ and ordinary WRITE. Select expanded FF first, then replace either child independently with expanded SRAM while leaving `CacheController32` unchanged. |
| 6 | Standalone cache test root and oracle | New test fixture and independent reference model | Wire the cache's lower bundle to an external 16-word backing endpoint, initially FF; drive the upper bundle. Compare accepted upper/lower transactions, values, faults, final backing bytes and endpoint replacement behavior. This fixture is not a new product memory technology. |

`Cache32` is the only new **public cache family** in this slice. The internal
names above are inspectable compositional blocks, not new public catalog
families unless a later reuse case justifies separate public contracts. The
existing `MemoryController` remains inside each memory endpoint: it controls
that technology's native device, and cannot perform cache tag lookup or
refill. The new `CacheController32` owns those cache operations but stores no
memory words itself.

### First operational checkpoints

1. Validate `S=1,2,4`, `E=1`, `B=4`, the cacheable interval and internal bank
   capabilities at construction. Build all three geometries from the same
   structural blueprint, not three handwritten caches.
2. Connect FF tag/data endpoints and an external FF backing endpoint. Check
   reset clears valid bits and leaves backing contents intact.
3. Drive a cold READ: the controller reads the tag/valid state, sends exactly
   one lower READ on a miss, writes data and tag through their endpoint ports,
   sets valid last, then responds. A repeated READ hits and sends no lower
   READ. Check conflicts for each supported set count.
4. Stall each of the tag, data and backing endpoints in turn. Check held
   request payload, same-edge FF response, delayed response, upper response
   backpressure, error propagation and no duplicate lower request. Check
   all-line invalidation, misalignment and range validation.
5. Swap tag FF to SRAM, then data FF to SRAM, and use a delayed compatible
   backing endpoint. At a safe clock, the same input trace must give the same
   values, statuses, hit/miss decisions and lower accepted-request trace;
   timing may differ. Do not claim replacement from schema matching alone.

Only after these checkpoints add `B=8` (two word transactions per miss),
`E=2` (parallel tag banks and victim selection), and the writable policy.
Full program integration remains after the shared backing-memory stage.

**Evidence this date:** inspected `protocolPins`, the endpoint assembly and
catalog, `MemoryTransactionPort`, existing gate/splitter/register components
and the RV32I structural 32-bit tag-comparison pattern. No implementation,
compile, simulation or endpoint-swap test was run for this refinement.

## 2026-10-14 — Correction: cache word width must follow memory endpoints

The owner correctly challenged the preceding 32-bit-only component inventory.
This **replaces its public `Cache32` and `CacheController32` restriction** and
the first-slice width restriction, while preserving the endpoint composition,
structural control and staged geometry checks. The earlier 32-bit example was
chosen for RV32I convenience; it is insufficient for a reusable cache in this
memory library. No cache contract ID has been registered yet, so this change
requires no compatibility migration.

The textbook's `m` is the number of **address bits**, whereas the endpoint's
`W` is the number of **data bits in a word**. They are different parameters.
The existing port has `m=32` byte-address bits and endpoint widths
`W in {8,16,32}`. A bounded general cache for this library should use the
same `W` on its upper requester-facing and lower backing-facing REQ/RSP
bundles. The byte mask width is `W/8`; aligned requests address `W/8` bytes.
The supported external contracts are proposed as `memory.cache.8`,
`memory.cache.16` and `memory.cache.32`, all generated from one
width-parameterized structural blueprint and one policy definition. A
structural and later compact implementation of a given width share that
width's contract. Build profiles choose fidelity, not word width.

For `L=words_per_line` and `q=W/8` bytes per word:

- `B=q × L` bytes per line, `C=S × E × B` cached data bytes;
  `b=log2(B)`, `s=log2(S)`, `t=32-b-s`. Require power-of-two `S` and `L`
  and a valid nonnegative tag width.
- The backing address is still the original 32-bit **byte address**.
  `word_offset=floor(A/q) mod L`, `set=floor(A/B) mod S`, and
  `tag=floor(A/(B×S))`. No address-bit positions are hardcoded to W=32.
- A data bank for each way is an existing `MemoryEndpoint<technology,W>`
  with `S×L` words. Its local byte address is
  `q × (set×L + word_offset)`; write masks and read data are W bits wide.
- A tag bank for each way can remain an existing
  `MemoryEndpoint<technology,32>` with `S` words, addressed locally at
  `4×set`. Tags describe the 32-bit address and can require all 32 bits when
  `S=B=1`; storing one padded tag in a 32-bit endpoint avoids splitting a
  tag across several 8/16-bit transactions. This tag width is **metadata
  width**, not a restriction on the cache's data word width. Valid/replacement
  state is separate structural storage.
- Internal tag/data banks still need READ and ordinary WRITE, so select FF,
  SRAM or DRAM at their required widths. The lower endpoint must have the
  same W as the cache and support the operations selected by cache policy.
  Cross-width backing needs a separate, explicit bridge and is outside this
  cache contract; matching W connects through the exact existing REQ/RSP
  fields. ROM/flash are possible read-only backing choices, not internal
  writable tag/data banks.

The component inventory is revised accordingly: `Cache<W>` is the public
assembly, `CacheController<W>` is width-parameterized structural control,
`CacheLineState` remains geometry-dependent, and `CacheTagMatch32` remains a
32-bit structural metadata comparator. `MemoryTransactionPort<W>` and
`MemoryResponseBuffer<W>` are reused where their handshake fits. Endpoint
children are selected by technology and width through existing catalog
contracts; no new native memory or technology-specific adapter is added.

**Revised first gate:** bring up the smallest one-word direct-mapped circuit
at W=8, then W=16 and W=32 from the *same* blueprint before calling the
cache word-width general. With `S=4,E=1,L=1`:

| Data width | Line bytes | Cached data bytes | Example same-set conflict | Tag bank / data bank |
| --- | ---: | ---: | --- | --- |
| W=8 | 1 | 4 | `0x00` and `0x04` | 4×32-bit / 4×8-bit words |
| W=16 | 2 | 8 | `0x00` and `0x08` | 4×32-bit / 4×16-bit words |
| W=32 | 4 | 16 | `0x00` and `0x10` | 4×32-bit / 4×32-bit words |

At each width also check `S=1,2,4`, alignment, masks on the internal refill
WRITE, X data, held responses, immediate and delayed endpoints, and one
technology swap. Later prove `L=2` and `E=2` at every supported W before
advertising those geometry combinations as generally supported. The
structural-size limit and verified timing bounds remain explicit.

This design is general **within the existing endpoint interface and tested
bounds**. It is not yet a complete cache hierarchy: coherent DMA, multiple
ports, write-back/dirty eviction, virtual address translation, arbitrary W
and hardware bursts need separate contracts and proof. The first read-only
slice is also not a completed cache implementation. A cache plus matching-W
backing endpoint can present an endpoint-shaped upper transaction port; a
cache by itself still has a distinct two-sided component shape.

**Evidence this date:** verified the `8,16,32` endpoint registrations,
`EndpointConfig::validate`, width-derived REQ/RSP data and mask pins,
`MemoryTransactionPort` and structural register widths against current
source. This is a design correction. No code, simulation or width-parity test
was run.

## 2026-10-14 — Clarification: textbook geometry as construction parameters

The owner asked whether the textbook's cache parameters should be template
parameters. They must be **explicit structural parameters**, but they do not
all need to be C++ template arguments. This section refines the previous
width correction and replaces `words_per_line` as the proposed *public*
geometry parameter with the textbook's `block_bytes`. The derived internal
value remains `words_per_line = block_bytes / (W/8)`.

| Quantity | Proposed representation | Why |
| --- | --- | --- |
| Data word width `W` | C++ `Cache<W>` specialization for W=8,16,32; corresponding catalog IDs `memory.cache.8/.16/.32` | Changes public data and mask pin widths and matches existing width-specialized memory families. |
| Address width `m` | Fixed 32-bit `REQ_ADDR` contract for this cache family | The existing endpoint protocol has 32-bit byte addresses. A different `m` would require a different port contract. |
| Sets `S` | `CacheConfig.sets` and catalog `sets` construction parameter | Determines set index bits, tag/data endpoint depths, valid bits and set wiring. |
| Ways `E` | `CacheConfig.ways` and catalog `ways` construction parameter | Determines the number of endpoint bank pairs, tag comparators and replacement state. First supported values: 1 and 2. |
| Block size `B` | `CacheConfig.block_bytes` and catalog `block_bytes` construction parameter | Matches the textbook's byte count; determines word selection and lower READs per miss. Require a power of two, at least `W/8`, divisible by `W/8`. |
| Tag/data technology and form | Separate construction parameters selecting existing endpoint contracts | Replacement choice is independent of cache geometry and C++ type. |

`S`, `E` and `B` are fixed when `buildInternals()` elaborates the circuit.
They do **not** change while that circuit is running. The builder creates the
actual number of endpoints, comparators, state bits and wires for those
values. Thus a construction parameter still produces a fully structural
circuit; it is not a behavioral variable-size lookup table. Use bounded
validation before building and retain the selected parameters in the catalog
request/manifest. A C++ `Cache<W,S,E,B>` type for every geometry would require
separate compiled instantiations and make the existing catalog/profile route
less useful without improving structural fidelity. Internal compile-time
helpers remain possible if a specific circuit benefits from them.

For example, `Cache<8>` with `sets=4`, `ways=1`, `block_bytes=1` builds four
one-byte data lines; `Cache<32>` with the same S/E and `block_bytes=4` builds
four four-byte lines. `Cache<8>` with `block_bytes=4` instead builds four
words per line and performs four lower word READs on a miss. Data capacity is
always `S × E × B` bytes. The tag endpoint may stay 32-bit wide at every W
because it stores address metadata, while the data and backing endpoints
match W.

The exact public configuration should expose `block_bytes` rather than both
`block_bytes` and `words_per_line`: two independent values could disagree.
Existing dated sections that name `words_per_line` describe the same derived
internal count; this section supersedes them only as a public API proposal.
The first implementation gate still uses one word per line at each W, then
varies block size and ways from the same blueprint. No C++ template or cache
component has been implemented yet.

**Evidence this date:** `FFMemory<Width>` specializes on width while its
`MemoryConfig.depth_words` is a construction parameter; memory endpoint
contracts register W=8/16/32 and take `ParameterMap` configuration. The
book's `(S,E,B,m)` is a mathematical organization, not a requirement that all
four values be encoded as C++ template arguments. No build or test was run.

## 2026-10-14 — Structural cache implementation in progress

The owner authorized implementation and then extended the target to a complete
general cache. The current checkout is branch `alpha` at `1d861c0`; this
replaces the earlier header's `github-pages` code-state description for this
implementation section. The existing documentation edits were already present
when source work began.

The first code increment adds a width-specialized `Cache<W>` assembly with an
upper endpoint-shaped port, a lower requester-shaped port, a structural
read controller, a 32-bit tag comparator, per-set valid state, and replaceable
FF/SRAM tag/data endpoint children. Its bounded initial geometry is one way,
one word per line, and 1, 2 or 4 sets. An external memory endpoint supplies
backing storage. A standalone scenario is being added to check hit, conflict,
invalidation and independent tag/data technology swaps. This increment is
still being compiled and tested; it is not evidence that the complete general
cache has been achieved.

The owner's expanded target requires follow-up geometry and behavior work:
multiword lines, multiple ways and replacement, masked writes, each supported
word width, technology/profile equivalence, and a scalable form. Advanced-core
experiments are reserved for the integration stage where the new cache actually
changes core memory traffic.

## 2026-10-15 — Standalone cache implementation and verification

This replaces the preceding section's "being compiled" status. On branch
`alpha` at starting commit `1d861c0`, the worktree now contains a standalone
`Cache<W>` implementation. The upper port uses the common REQ/RSP endpoint
pins, the lower `MEM_` port uses the same protocol in requester direction,
and `INV_VALID/INV_READY` clears all line-valid and replacement state at an
idle boundary. The lower endpoint remains a separately wired component.

`CacheConfig` fixes W through the catalog contract and sets, ways, block
bytes, backing depth, read-only/read-write mode, and FF/SRAM tag/data
technologies at construction. The expanded form builds existing small memory
endpoints; the scalable form selects existing scalable endpoints. Both use
the same structural controller, comparator, per-set state bits and refill
sequence. The bounded supported sizes are 1/2/4 expanded sets or up to 64
power-of-two scalable sets, one/two ways and one/two/four words per line.
The catalog marks the new structural implementations `Characterized`, with
`CacheReadTest` as their contract evidence; no behavioral cache implementation
or cache-wide timing table is claimed.
The second way uses an invalid-first choice followed by per-set exact
two-way LRU. Read misses fetch every line word; a line becomes valid only
after data and tag writes finish. Writable mode sends a masked backing write
once and updates a cached hit after backing success. A write miss does not
allocate. Lower failures are returned; a failed internal update invalidates
the affected line. Reset clears metadata and transaction state, not memory
contents.

`CacheReadTest` covers W=8/16/32, set and line sizes, conflicts, two-way
replacement, independent SRAM tag/data substitutions, multiword fills,
read-only rejection, 8/16/32-bit masked writes, write-through backing bytes,
write miss without allocation, zero mask, backing ROM failure, range and
alignment errors, held write responses, invalidation, reset during a miss,
and scalable storage at 8, 16 and 64 sets. A deterministic mixed read/write
stream at W=8 and W=32 is compared with an independent set/way/byte reference
model, including lower request counts and final backing bytes. An
intentionally short lower ROM fails on the second fill word; a retry proves
that the partial line was not marked valid. The fixture connects structural
ROM or previously validated FF/SRAM/DRAM endpoints as backing. It does not yet provide
a full independent timing trace oracle for every fault and four-state path.

Verification: serial `SimulationTestRunner` build passed; focused CTest
six focused cache, catalog and profile CTests passed 6/6 in 107.72 seconds;
the direct cache runner passed again after adding SRAM/DRAM backing reads
and a 16-bit scalable-bank masked write.
`git diff --check` passed. The strict MkDocs build passed after installing
`requirements-docs.txt` in a temporary venv;
it wrote to `/tmp/circuitsim-cache-site`. These checks do not establish a
safe CPU clock, cache speedup, all lower-technology combinations, or
simultaneous I/D traffic. The existing advanced core was not modified or
experimented on because no cached core platform exists yet. The public
component contract is in `site-docs/memory/3.4.md`.

Open work in the adopted order: characterize small memory access timing and
calibrate scalable RAM, replace the two RV32I memory images with one shared
RAM behind arbitration, then attach optional instruction/data caches and
compare structural/scalable outcomes and programs. Additional standalone
proof should cover deliberate lower faults at every fill word, unknown
four-state request fields, invalidation while busy, and broader reset/write
overlap. The cache is not yet a coherent hierarchy; external writes require
invalidation.

## 2026-10-15 — Component workflow and public page correction

This corrects the preceding section's documentation claim. The first public
cache page described the contract in prose but did not follow
`docs/guides/adding-a-component.md`: it lacked a registered
`ComponentScenarioTest`, a visualizer alias and saved layout, and a generated
scenario example. The earlier strict MkDocs build used the existing example
bundle and therefore did not verify a cache example.

`CacheComponentScenarioTest` now owns the expanded 8-bit cache's logical
component scenario. Its independent checkpoints drive the upper request and
the `MEM_` lower response pins through reset, a cold miss and fill, a held
upper response, and a subsequent hit without a lower request. `CacheReadTest`
remains the broader headless test for other widths, geometries, technologies,
errors, and reference-model checks. The new scenario is registered with CTest
and `TestRegistry`, has alias `cache-read-hit` in the `memory-cache` menu, and
has a generated saved layout. The public page now follows the component
template: exact pin table, behavior, C++ example, unknown/timing scope,
structural and behavioral status, a generated example marker, simulator link,
and tests.

Verification: the scenario runner passed with 12 recorded checkpoints;
`VisualizerScenarioCatalogTest` passed 8/8 against the rebuilt isolated
native backend. A native `VisualizerSession` generated the 12 example rows,
and a strict MkDocs build with those current rows passed; the rendered page
contains the example table and simulator link. A final focused CTest run
passed 4/4: the new scenario, catalog inventory, registry coverage and
visualizer catalog. Both saved layout and scenario-menu JSON parsed, and
`git diff --check` passed. The page's C++ example compiled after correcting
the global `Component` namespace, and the strict MkDocs build passed again.
`tools/build-pages.sh` stopped
before packaging because `emcmake` is unavailable on this host. The current
WebAssembly package, browser behavior, and published site are consequently
unverified for the new scenario. This correction does not change the cache
controller or establish its CPU timing.

## 2026-10-15 — Test visibility and behavioral scope

The two cache tests reran and passed 2/2: `CacheComponentScenarioTest` in
0.11 seconds and the broader `CacheReadTest` in 133.13 seconds. The latter is
headless because its many backing-memory and geometry fixtures are intended
for contract verification rather than one visualizer timeline. The component
scenario is in the source registry and visualizer menu, but the existing
packaged WebAssembly module predates it. The [port 8893 recovery
record](2026-10-15-pages-preview-example-recovery.md) explains why the
documentation page now loads while the embedded cache simulator still needs
a full WASM build.

This cache stage registered structural fidelity only. The scalable contract
replaces its tag/data endpoint storage with scalable endpoints under the
same structural controller; it is not a fused behavioral cache. The earlier
feasibility section explicitly made a fused form optional after whole-cache
equivalence. The current code has no structural/behavioral whole-cache
equivalence test or proven behavioral timing table. The public page now
states that boundary and distinguishes the visible scenario from the
headless broad test. Calling this a complete cache hierarchy or behavioral
cache would overstate the implementation.

## 2026-10-15 — Browser scenario verification replaces earlier WASM gap

The [cache browser scenario record](2026-10-15-cache-browser-scenario.md)
corrects the previous sections' unverified-WASM status. The cached
Emscripten SDK was activated, the Pages package rebuilt, and the
`cache-read-hit` rows were exported and verified by the new WASM module.
The focused browser check passed after separating cache layout assertions
from endpoint-specific ones. Port 8893 now serves the rebuilt simulator and
scenario menu. This confirms the visible scenario, not a behavioral cache
implementation, whole-cache equivalence, or CPU integration.

## 2026-10-15 — C++ reference example correction

The cache page's first C++ block only constructed `Cache<32>`. That did not
serve the behavioral-reference purpose of the C++ examples on the neighboring
memory component pages. The page now contains a standalone, width and
geometry-parameterized binary reference for address decoding, tag hit,
invalid-first/two-way LRU victim choice, full-line refill, masked
write-through/no-write-allocate, and invalidation. It uses a separate backing
vector as a stand-in for the externally connected endpoint, and explains how
its logical line maps to the structural tag/data endpoints and valid bits.
The reference is documentation, not a registered behavioral cache fidelity.

`CacheDocumentationTest` extracts and compiles the exact page block, then
checks 8/16/32-bit words, read hit and refill counts, two-way replacement,
masked write-through, write miss without allocation, read-only rejection, and
invalidation. It passed directly and as a newly registered CTest (1/1).
The strict MkDocs build passed with the current generated example bundle.
The reference assumes completed successful binary transactions; it does not
model REQ/RSP timing, four-state inputs, or lower/internal failure status.

## 2026-10-15 — Broader component scenario and cache stacking

The previous visual scenario exercised one 8-bit, one-set, one-way, one-word
read-only cache through a cold miss and a hit. It could not show spatial
locality, a real replacement choice or write policy. The registered
`CacheComponentScenarioTest` now uses two sets, two ways, two 8-bit words per
line and read/write mode. Its independent public-pin checkpoints cover both
fill words, a same-line hit, two-way occupancy, LRU eviction and retention,
masked write-through on a hit, write miss without allocation, read-after-write,
all-line invalidation, a held upper response, lower request backpressure and
an out-of-range read with no lower request. Its component-port lower memory is
still a test driver; the generated table has 116 verified checkpoints. The
existing alias `cache-read-hit` remains valid, while the menu title and public
description now name the broader behavior.

A separate headless fixture wires upper `MEM_` pins directly to a second
matching-width cache's upper REQ/RSP pins, and the second cache to an FF RAM
endpoint. The test uses a one-word direct-mapped upper cache and a two-word,
two-way lower cache. An upper miss hits a lower resident line without adding
a RAM read; a write reaches RAM and remains visible after upper eviction.
Broadcast invalidation while both levels are idle causes the next read to
refill from the changed RAM image.
This establishes a functional **single-requester two-level cache path**.
The components do not coordinate invalidation across levels or multiple
requesters. A CPU memory hierarchy still needs one shared address map,
arbitration for I/D requests, cacheable-range routing that bypasses MMIO,
and an instruction/data visibility rule for stores to executable memory.
No inclusion policy, coherence protocol, safe CPU clock or performance
benefit has been established. The cache remains a bounded reusable design,
not a final generalized cache system.

Verification: the updated native component scenario passed; the broad
`CacheReadTest`, including the new two-level fixture, passed; the isolated
native binding and visualizer probe were rebuilt; a focused Pages rebuild
generated 116 WASM-verified cache rows and passed strict MkDocs. The focused
`VisualizerScenarioCatalogTest`, `CacheDocumentationTest` and component
scenario CTests passed 3/3. The memory endpoint browser test passed against
port 8893, including the cache's generated rows, WASM timeline and two-way
layout. The first browser command used a venv without Playwright and failed
at import; the installed system Python ran the test successfully.

## 2026-10-15 — Cache example expansion in progress

The owner requested concrete examples for several single-level geometries and
a visible multi-level cache path. The current public page has one two-way,
two-word 8-bit scripted lower-port scenario; the two-level FF-backed fixture
is headless only. This work will add registered direct-mapped and scalable
single-cache scenarios plus a visual two-cache fixture over a real backing
endpoint. Public explanations will state each geometry, the address sequence,
the expected lower traffic, and whether the lower port is scripted or wired to
another component. Verification and any corrections follow in a later section.

## 2026-10-15 — Example implementation and page rewrite

Added three scenario entries beyond the existing two-way byte-cache example:
a 32-bit direct-mapped cache (2 sets, 1 way, 1 word/line), a 32-bit scalable
SRAM-bank cache (8 sets, 2 ways, 2 words/line), and a connected two-level
8-bit cache fixture over an FF RAM endpoint. The single-cache scenarios use
scripted lower responses so their generated checkpoint tables expose each
lower READ. The hierarchy scenario uses real cache-to-cache and cache-to-RAM
connections, checks backing request counts and data, and publishes ten
timeline checkpoints. Its layout-only setup constructs the circuit without
advancing simulation; the visualizer invokes a separate playback hook.

Rewrote the public cache page within its component template: configuration
now explains word width, sets, ways, line size and backing range before the
secondary storage and mode choices. The IO, behavior, C++ reference,
implementation, examples and tests sections remain in order. Four examples
state geometry, lower-port wiring and expected traffic. The existing C++
reference code is unchanged. Native direct-mapped, scalable and hierarchy
scenario tests initially passed. The visualizer catalog check caught the
hierarchy's first setup method advancing simulation; that implementation was
split into topology-only setup and explicit timeline playback. Verification of
the revised path follows below.

## 2026-10-15 — Example and visualizer verification

The first hierarchy layout preview built 22,259 components because its FF
tag/data stores and registers used expanded gate-level forms. The visual
fixture now applies the same compact leaf profile used by the existing cache
scenario; both cache controllers remain structural. Its topology contains
2,260 components. The direct-mapped and scalable examples contain 1,495 and
2,056 respectively. `recalculate_layouts.py --fill-missing` added only their
three scenario roots and missing type layouts to the saved layout file.

After separating topology setup from timeline playback, eight focused native
CTest entries passed: the four cache scenarios, `CacheLayoutTest`,
`CacheDocumentationTest`, `VisualizerScenarioCatalogTest` and
`TestRegistryCoverageTest`. The catalog test confirms layout construction
does not execute the hierarchy's simulation. The generic playback hook on
`SimulationTest` is a no-op for existing tests and is called only when a
visualizer session opens; broader visualizer regressions remain unrun.

`tools/build-pages.sh`, with the local Emscripten SDK, an isolated MkDocs
environment and a cache-scenario example filter, rebuilt the WASM simulator
and passed strict MkDocs. It exported 30 direct-mapped, 116 existing two-way
and 58 scalable example rows, each verified against the WASM timeline. The
two-level scenario has ten checked timeline labels and no generated component
table because it is a composite fixture. The focused
`MemoryEndpointBrowserTest` passed 1/1 in
`build/validation/web-20261015-204729`; its log explicitly confirms all
three table-backed cache examples and the connected L1/L2/RAM scenario.
The full native and browser suites were not run for this documentation change.
The internal test-organization reference and adding-component guide now state
the topology-only setup and optional playback contract used by this fixture.

## 2026-10-15 — Memory realism and cache-comparison review

This section refines Stage 2's earlier phrase "measure before changing delays":
small structural devices are the reference for **internal model fidelity**,
but their abstract ticks are not measured silicon timing. A behavioral model
can agree perfectly with a structural model whose phase durations are poorly
calibrated to a real device. The repository textbook's chapter 6 motivates a
small, fast store caching blocks from a larger, slower store under temporal
and spatial locality. Intel describes the same cache-versus-DRAM locality
mechanism in its [optimization manual](https://cdrdv2-public.intel.com/821612/248966-Optimization-Reference-Manual-V1-050.pdf).

### Current implementation and evidence

- `createMemoryContainer` selects the existing `memory.<technology>.device.<W>.behavioral`
  implementation and changes its display type. It adds neither a new timing
  algorithm nor REQ/RSP control. The fused `BehavioralMemoryEndpoint` separately
  models controller and device behavior at the transaction boundary.
- FF structure is decoder, clocked cells and read selector. Its compact device
  retains asynchronous reads, edge writes, masks, reset and four-state data.
  Large FF depth is functionally scalable, but does not model the area, wiring
  and power of millions of physical flip-flops. FF can be zero-wait when the
  certified clock covers its combinational path.
- SRAM structure includes a latch cell, row/column selection, digital
  complementary bitlines and sensing. Its compact device and structural
  access control share `AccessSequence`: four configurable phases precede
  readiness. This checks implementation agreement, not whether four phases
  of the default `phase_ticks` match a commercial asynchronous SRAM's
  address-to-data and cycle times. For scale, an [Infineon asynchronous SRAM](https://www.infineon.com/part/CY7C1069GN30-10ZSXIT)
  specifies address access and read-cycle timing in nanoseconds; our ticks
  have no nanosecond mapping.
- DRAM has a digital charge cell, row buffer, destructive sample/restore,
  precharge and refresh. The compact device reuses the native command
  sequencer but represents storage/sensing separately with paged contents and
  sparse row timestamps. The endpoint distinguishes row hit, closed row,
  conflict and refresh. Real controllers also have banks, bursts and timing
  constraints such as precharge-to-activate and activate-to-read; our one-bank,
  word-at-a-time endpoint omits their parallelism and transfer behavior.
  [Microchip's SDRAM read-cycle description](https://onlinedocs.microchip.com/oxy/GUID-B822915F-C375-4172-91BD-AB6F326EB783-en-US-1/GUID-A9CFAA65-9F00-4E1D-AB82-84F228BBF5B6.html)
  documents those distinctions.
- ROM's detailed device is fixed cells with row/column selection; its compact
  device directly reads the same image and preserves disabled, invalid and
  unknown outputs. The transaction controller currently waits the same
  `4*phase_ticks + 2*guard_ticks` for ROM as for SRAM, even though native ROM
  has no SRAM access sequence. That wait is a controller policy to review,
  not a structural ROM-cell delay measurement.
- NOR flash retains write-enable, busy, program 1-to-0, block erase and read
  semantics. Its compact device shares the command sequencer and separately
  implements stored bits. It is a read-only instruction-backing candidate,
  not a substitute for ordinary write-through RAM: WRITE is unsupported and
  PROGRAM/ERASE have different semantics. Physical wear, persistence across
  simulator instances and detailed bus protocols are outside this model.
- `NativeMemoryDeviceContractTest` compares small expanded devices and
  containers. FF/SRAM device tests cover 8/16/32-bit widths and several
  geometries; DRAM/ROM/NOR tests cover widths, row geometry and command
  behavior. `SRAMTimingTest` and `MemoryDeviceTimingTest` sample cancellation,
  busy, refresh and expiry. `ComponentTestRunner::compare` checks settled
  output snapshots, not exact transient waveforms or every completion tick.
  An existing `build-cleanup` binary, newer than the native-memory source,
  passed 8/8 focused tests on 2026-10-15: FF/SRAM/device/container/endpoint
  contracts and SRAM/device/endpoint timing. It was not rebuilt in this
  review. No large structural counterpart exists beyond 4,096 cells.
- Catalog delay tables are proven on supported small geometry/path cases;
  the large-depth rule extends growth seen at 64 and 128 words per doubling
  and marks it `ESTIMATE`. Phase durations and controller guards are separate
  from those propagation-delay tables. A same-cycle FF host lookup and a
  multi-cycle DRAM transaction can both be faithful to their respective
  structural abstractions without being physically calibrated.

### Proposed validation order; no implementation in this review

1. **Measure the native boundary.** For each technology at 8/16/32 bits and
   supported small depths/row layouts, replay identical streams against
   expanded and compact native devices. Record each public transition and
   native input-to-output or command-to-completion interval, not only settled
   final values. Include
   changing addresses and controls, X/Z, masked writes, stale deadlines,
   reset, and technology-specific row/refresh or program/erase states.
2. **Measure the endpoint boundary.** Repeat with a structural controller
   plus expanded device, the same controller plus a compact native device,
   and the fused endpoint. Compare accepted requests, response data/status,
   response/ready cycles, occupancy and backpressure at a common certified
   clock. Attribute any difference to device propagation, controller phases,
   or response buffering. Hold out at least one structural depth when fitting
   any depth trend. Label unsupported geometry and extrapolation explicitly.
3. **Review physical plausibility separately.** Select a named representative
   target and timing units before claiming realistic latency. Audit SRAM's
   four-phase policy and ROM's inherited SRAM wait first; if inappropriate,
   change the **structural device or controller** and its compact counterpart
   together, then regenerate delay tables and recertify clocks. Extend DRAM
   with banks/bursts only if the intended cache study needs their effects.
   Do not add arbitrary waits solely to force a cache win.
4. **Compare the same backing endpoint with and without a cache.** A standalone
   requester drives either `requester -> backing` or `requester -> Cache ->
   backing`; the backing technology, capacity, image, timing settings and
   clock are identical. Try cold then repeated reads, same-line neighbors,
   streaming, conflicts, mixed writes and DRAM row hits/conflicts. Record
   hit/miss/eviction counts, lower READ/WRITE traffic, per-access completion
   cycles/ticks, throughput and simulator host time separately. FF backing is
   a useful zero-wait control, not a required cache-speedup case. Small
   structural geometry demonstrates mechanism and model fidelity; its cache
   and backing capacity ratio cannot establish large-system performance.
5. **Then compare CPU systems.** The current short cached/uncached migration
   tests use paused toy programs, and the ordinary experiment runner still
   builds two uncached memories. The unified `RV32IProgramRoot` requires both
   caches, so a matched uncached shared-RAM path is missing. Add that bypass
   path, use the same advanced core, program, image, RAM, arbiter, clock and
   workload for both configurations, and check the instruction oracle before
   reporting cycles. Report minimum safe clock periods separately. The
   present cache serially requests tag then data through endpoints, has one
   outstanding request and fills multiword lines one READ at a time; those
   costs may outweigh hits until the design or backing latency changes.

Decision status: the comparison matrix and any physical target are proposed
for owner review; no timing values, cache speedup or hardware calibration are
claimed here.

## 2026-10-15 — Correction: existing aggressive delay comparisons

This corrects the implication in the preceding review and its proposed
validation order that memory timing needs a new structural/behavioral
comparison before cache experiments can begin. `tests/cpp/DelayRuleTest.cpp`
already drives gate-level, compact and mixed implementations through seeded
single-pin, X/Z, multi-pin, staggered and clock-edge changes. It compares
output-change ticks, checks that compact outputs are not earlier than gates,
and measures how much later they settle. `PartDelayTablesCompleteTest` separately
checks generated tables against the characterized structural circuits. These
cover memory devices, controllers and endpoints where both forms exist, in
addition to the contract and technology-specific timing tests named above.

Focused current-build check:

```text
build-cleanup/tests/DelayRuleTest --report memory.ff.endpoint.8 memory.sram.endpoint.8 memory.dram.endpoint.8 memory.rom.endpoint.8 memory.nor-flash.endpoint.8
6 part/variant rows; 3,208 situations; 970 outputs changed in both forms;
0 delay-rule breaks; 0 unrecorded closeness failures; exit 0.
FF endpoint: 94% typical gate/compact settling-time ratio.
SRAM and ROM endpoints: 63% each; NOR-flash endpoint: 67%, with recorded
self-timed launch exceptions.
DRAM endpoint: two initial situations and no paired output changes; its
self-timed transaction behavior is not covered by this run.
```

The delay-rule bench stops when a self-timed part continues working, so the
focused result does not establish exact transaction completion ticks for
DRAM, SRAM, ROM or flash under all requests. Existing endpoint timing tests
exercise technology-specific cases, and the earlier delay record reports
gate/compact CPU cycle agreement on selected timed-memory programs; neither
claim is physical chip calibration. Large memory delay tables remain marked
estimates beyond the structural size limit.

Decision: proceed with matched direct-versus-cached experiments using current
models. Include structural and compact endpoint configurations in the
experiment matrix and compare their per-request response cycles there. Treat
any discrepancy as a model-fidelity finding to investigate, rather than
requiring a new blanket timing-validation phase before the first experiment.
Do not claim cache speedup or hardware-realistic timing before results exist.

## 2026-10-15 — Cache experiment protocol; planned, not run

### Questions and comparison rules

Measure (a) whether caches reduce backing-memory requests, (b) whether that
saving exceeds this cache's lookup/refill cost in simulated cycles, (c) where
an L2 helps after an L1 miss, and (d) how the same choices affect the advanced
RV32I core. A lower request count alone is not a latency speedup. No result
from this protocol is a measured silicon latency or an energy result.

The primary pair is `scripted requester -> backing endpoint` versus the
**same** requester and backing endpoint with `Cache<32>` inserted. Both use
the same width, address range, initial bytes, technology, endpoint parameters,
clock period and input trace. Construct each case afresh, then check its
`BuildManifest` so requested structural, compact and scalable forms were
actually selected. Keep the cache controller structural throughout; only its
FF/SRAM tag and data endpoints change form or technology. Do not add a
synthetic delay to the direct path or to the backing memory.

For a direct request, record offer, acceptance, first `RSP_VALID` and response
consumption; collect the same four times at the cache upper port. Count
accepted lower READ/WRITE transactions at each boundary, rather than
inferring hits from output values. A separate reference cache model classifies
cold, capacity/conflict and hit cases. Check every upper response and the
final backing bytes against a technology-appropriate independent expected
result. For read-only ROM/NOR flash, use READ traces only; prepare the NOR
image before timing and keep program/erase outside the measured region.

### Stage A: small, matched memory experiment

Implement a deterministic `CachePerformanceBenchmark` alongside the existing
`MemoryEndpointBenchmark`, using the common `benchmark::RunRecord` and the
serial `tools/benchmarks/run.py memory` campaign. Reuse the existing
transaction-port and cache fixture wiring where useful; the benchmark driver
is a host test fixture, not a new product component. Give it a small CTest
smoke case that checks event counting and a known hit/miss trace. Capture
the exact configuration, build manifest, trace hash/seed, binary hash, source
revision/dirty state and completion status in the run artifacts.

Begin with W=32 and 32 backing words, two words per row where applicable,
at the current endpoint defaults (`phase_ticks=32`, `guard_ticks=64`). This
is inside the expanded 4,096-cell bound. At this *same* geometry select three
explicit profiles: structural controller plus expanded native device,
structural controller plus compact native device (`MemoryContainer`), and
fused behavioral endpoint. Confirm
correctness and compare response cycles across forms while measuring cache
benefit; `DelayRuleTest` need not be repeated as a new prerequisite. Check
W=8 and W=16 on a short representative stream, rather than multiplying
every performance case by all widths.

Initial cache: 2 sets, 1 way, 1 word per line, FF tags and FF data. Change
one factor at a time: 2 words per line for spatial locality; 2 ways for
conflicts; FF/SRAM tag/data combinations for storage technology; expanded
versus scalable banks at identical small geometry. Keep the lower endpoint
fixed within each pair. For DRAM, record row-hit, row-conflict and refresh
conditions; use a separate run with refresh due, not a pooled average.

| Trace, each started from a fresh identical image | What it exposes |
| --- | --- |
| Cold read followed by 63 reads of the same word | Fill cost, steady hit cost and lower READ suppression |
| Alternate two words in one two-word line for 64 reads | Spatial locality and the two lower READs paid at first fill |
| Read 32 successive words once | Streaming misses and unnecessary cache overhead |
| Alternate two lines mapping to one set, then compare one and two ways | Conflict misses and replacement benefit |
| Interleave masked writes, reads and a write miss | Write-through traffic, no-write-allocate and final bytes |
| DRAM same-row then alternating-row reads, with and without due refresh | Row-buffer/refresh effects on the direct and cached paths |

Use a cold total (including compulsory fills) and a separate warm interval
after an explicitly identical request prefix. In latency mode, offer each
request at the same predeclared absolute time in both paired runs, with enough
space for either response; in throughput mode, offer the next immediately
when allowed. This prevents queueing and DRAM
refresh timing from being silently mixed into an isolated service-latency
claim. Report offer-to-response and acceptance-to-response latency, total
modeled ticks, cycles at the shared period, requests per modeled tick,
median/tail latency, lower READ/WRITE counts, classified hit/miss counts,
processed simulator events and host seconds. Repeat host measurements on
fresh circuits and report the sample count; modeled results should be
deterministic. A result is valid only if responses, final bytes and protocol
counts match expectations and no simulation budget was exhausted.

Run this matrix on FF, SRAM and DRAM backing. FF is the zero-wait control;
SRAM and DRAM are the principal cache-speed candidates. Run ROM as read-only
and label its SRAM-like endpoint wait policy explicitly. Run NOR flash as a
read-only candidate only after its initialization path is verified. These
cases answer different questions; do not average a speedup across technologies.

### Stage B: hierarchy and scaling

With the same W=32 direct backing endpoint, compare direct, L1 only, and
L1 -> L2. Use a small expanded FF-bank L1 (2 sets, 1 way, 1 word/line) and a
scalable SRAM-bank L2 (8 sets, 2 ways, 4 words/line) over a backing capacity
of at least 128 words. Choose a repeated-neighbor trace that misses L1 but
hits L2, plus the streaming/conflict negative controls. Count L1-to-L2 and
L2-to-backing transactions separately. Keep the L2 line-fill cost visible:
the current lower port transfers one word per READ, with no burst. Compare
small expanded/scalable cache banks at the same geometry before attributing
any scaling trend to storage capacity. Large-depth delay estimates are not
small-structure proofs; label them as such in result records.

### Stage C: advanced-core experiment

Extend the ordinary `RV32IExperiment` runner and its CLI/campaign options to
select bypass, I-cache, D-cache and both caches. Share topology construction
with `RV32IProgramRoot` rather than maintaining two CPU wiring algorithms.
First use two separate equal-technology backing endpoints to compare direct,
I-only, D-only and I+D configurations without arbiter effects. Then allow the
advanced core to use one backing image and the structural two-port arbiter
with either bypasses or caches; the current unified test root rejects the
uncached case, so this requires an explicit bypass path. Compare shared
direct versus shared cached to isolate the cache choice. Compare separate
direct versus shared direct separately to show shared-port contention; do
not attribute that difference to caching alone.

Use the existing prepared workload/oracle path with one identical ELF/image,
prediction policy and advanced-core fidelity per pair. Begin with fixed
program 20 and the named `stream`, `pointer-chase` and `sort` workloads, then
add a compiled workload only after those pass. The oracle, expected result,
final RAM bytes, retired count and program fingerprint must agree. Record
cycles, modeled ticks, CPI, instruction/data request waits, cache-boundary
traffic, host seconds and event count. Use compact core/storage for long
runs and a bounded structural-reference pair for correctness; state which
parts are structural in every result.

For all cycle comparisons use one clock period that is safe for **both**
members of a pair. Try `CircuitTimingAnalyzer::safeClock` on each topology;
the present cache CPU clock has no certificate, so a failed/unknown analysis
must be reported as such. Until certified, use a conservative common period,
check setup violations and correctness, and repeat a smaller case at nearby
periods to detect timing sensitivity. Do not call that period a proven
frequency. Report each topology's eventual minimum certified period
separately from same-period cycle ratios. Keep reset, setup, warmup and
verification time outside the measured CPU window.

### Review gate

Publish the full result table, including losses, empty/failed cases and
configuration fingerprints. A cache is beneficial for a stated trace only
when its complete modeled service/throughput time improves over the matched
direct case and both produce identical answers. If traffic falls but time
does not, use the measured tag/data, fill and arbiter costs to decide whether
to improve the structural cache or to keep the finding as a limitation.
Do not change memory phase timings merely to obtain a positive result.

Status: protocol and fixture/runner changes are planned; no direct-versus-
cached experiment was run in this section.

## 2026-10-15 — Advanced-core comparison matrix refinement; planned

This section replaces the less detailed Stage C above. The experiment has
four main architectures requested by the owner, plus instruction/data cache
switches needed to explain their differences. All use the **advanced** core
with its existing separate instruction and data transaction ports. `I` below
means a read-only instruction L1; `D` means a read/write, write-through data
L1. `L2` is one shared read/write cache *after* the arbiter. `RAM` means one
matching 32-bit endpoint, not a new memory implementation.

| ID | Instruction path | Data path | Shared lower path | Question |
| --- | --- | --- | --- | --- |
| A | Direct to instruction RAM | Direct to data RAM | None; two physical stores | Current two-memory baseline |
| B | Bypass L1 | Bypass L1 | Arbiter -> RAM | Effect of sharing one port and image |
| C-I | I L1 | Bypass L1 | Arbiter -> RAM | Instruction L1 alone |
| C-D | Bypass L1 | D L1 | Arbiter -> RAM | Data L1 alone |
| C-ID | I L1 | D L1 | Arbiter -> RAM | One level on both ports |
| D-0 | Bypass L1 | Bypass L1 | Arbiter -> shared L2 -> RAM | Shared L2 alone |
| D-I | I L1 | Bypass L1 | Arbiter -> shared L2 -> RAM | Instruction L1 over L2 |
| D-D | Bypass L1 | D L1 | Arbiter -> shared L2 -> RAM | Data L1 over L2 |
| D-ID | I L1 | D L1 | Arbiter -> shared L2 -> RAM | Two complete cache levels |

The position of L2 matters: placing it after the arbiter gives instruction
and data requests one shared L2 and one RAM image. `D-0` is a useful control:
it measures L2 without L1, so an L2 benefit cannot be mistaken for an L1
benefit. B/C/D use the **same** arbiter implementation and backing endpoint;
bypassed ports connect through the existing transaction pins. No cache or
arbiter is inserted into A.

### How to interpret the pairs

- A vs B measures the complete change from two independent stores/ports to
  one store/port with arbitration. A has two physical memory images, so this
  difference is **not** the pure gate cost of the arbiter. Keep the same
  addressable range, technology and timing per endpoint; load the same
  complete initial image into both A stores, and once into B's shared store.
  The selected programs must fit one store and must not write instructions.
  A separate dual-requester fixture can isolate the arbiter's own wait.
- B vs C-ID measures the net contribution of both L1s with the same arbiter
  and RAM. B vs C-I and B vs C-D isolate the port targeted by one L1. Do not
  add the two separate gains to predict C-ID: arbitration and speculative
  fetch timing make their effects interact. Report that interaction.
- B vs D-0 measures a shared L2 alone. C-ID vs D-ID measures adding L2
  underneath unchanged I/D L1s. D-0/D-I/D-D/D-ID give the same instruction
  and data focus with L2 present. B vs D-ID is the full hierarchy effect.
- For a later capacity-control check, compare D-ID with a legal one-level
  configuration whose combined data bytes are close to the L1+L2 total.
  Label different allocation, ports and tag overhead; this is not part of
  the first nine-case panel.

### Fixed hardware and program setup

Freeze core fidelity, predictor policy/entries, one clock period, memory
technology, endpoint profile, memory capacity, device phase/refresh settings,
program image, initial data, cache parameters and measurement window within
each nine-case panel. Begin with scalable W=32 SRAM backing; then try DRAM,
with FF as the zero-wait control. ROM and NOR flash cannot serve as the one
ordinary writable RAM for this panel. CPU-scale memory uses compact/scalable
devices because the expanded device limit is 4,096 cells; the small
structural-versus-compact checks belong to Stage A and bounded CPU fixtures.

Proposed initial L1 geometry: 4 sets, 2 ways, 2 words/line, expanded FF tag
and data endpoints on each CPU port. Proposed L2: 64 sets, 2 ways, 4
words/line, scalable SRAM tag and data endpoints. Set
`backing_depth_words` on every cache to the shared RAM's depth. Keep these
geometries fixed across B/C/D within a panel; register any later size sweep
as a separate experiment. All L1/L2 writes remain write-through and
no-write-allocate, so a store reaches the shared RAM exactly once. Because
there is no I/D coherence protocol, exclude self-modifying code and external
writes during measurement; a future such workload needs explicit
invalidation and its own correctness test.

Use one identical **unified address image** for all variants: code and data
at disjoint addresses, both preloaded into each of A's two stores so reads
at any tested address agree, and preloaded once into B/C/D's shared store.
The current experiment runner creates two stores and two oracle memories;
its shared mode must instead load and check one store and one independent
oracle image. Do not clear or initialize the same shared device twice through
the two CPU-facing accessors. Preserve A's legacy result as a regression
when refactoring the runner.

### Workload and observation panel

Start with fixed program 20 as a bounded integration check. Then use
`calls` or `branch-pattern` for repeated instruction fetches, `sort` for a
reused writable data set, `stream` for contiguous copy and write-through
traffic, `pointer-chase` for dependent/poor-locality loads, and `crc32` as a
more compute-heavy control. Freeze parameters and seeded inputs before
looking at speedups. For each program report the cold complete run, and a
separate warm body window when the workload provides a repeatable training
prefix; use the same retirement PC markers and training iteration count in
all nine variants. The program's expected result, instruction oracle,
retired instruction count, final registers, final RAM bytes and completion
reason must agree. Speculative instruction *requests* may differ with timing;
record them instead of requiring identical request traces.

At every boundary count accepted requests and responses, READ/WRITE traffic,
response latency and cycles with offered-but-unaccepted requests. In
particular record core I/D ports, both L1 lower ports, both arbiter inputs,
the arbiter's memory port, L2's lower port and RAM. Classify L1I, L1D and L2
hits/misses with a separate reference model fed by the requests each cache
actually accepts. Count simultaneous I/D arbiter demand and grant waits;
separate downstream backpressure from an arbitration tie where possible.
Also retain CPU cycles, retired instructions, CPI, memory-stall observations,
modeled ticks, processed events and host seconds. Memory requests saved
are an explanatory metric; **completed CPU time** is the performance result.

Use one period safe for every topology in a comparison. Try the timing
analyzer on each constructed root; the current cached CPU root has no
certificate, so until that changes use a conservative common period,
require zero observed setup violations and stable correct results in a
nearby-period check, and label the result as an experiment at that period,
not as a certified CPU frequency. Report each topology's eventual certified
minimum period separately. If an L2 changes the critical path, compare
same-period cycles first and certified modeled time only after certification.

### Construction and run order

1. Extract one reusable memory-topology builder for `RV32IProgramRoot` and
   `RV32IExperiment`: optional I/D L1s, optional shared L2, and a bypass on
   each port. Generalize the existing unified-root restriction, wire the
   structural arbiter to bypasses or L1s, and keep all selected paths in the
   `BuildManifest`. Add no new cache or memory technology for this matrix.
2. Add bounded integration tests for A, B, C-I, C-D, C-ID, D-0, D-I, D-D
   and D-ID: a store/load program, a branch/redirect program and a shared
   backing-byte check. Exercise held responses, one accepted write and
   reset/invalidation at safe boundaries. Confirm A reproduces the current
   uncached runner's result before measuring new topologies.
3. Extend the existing RV32I experiment CLI and serial campaign runner with
   named topology/cache options and per-boundary counters. Run a short
   A/B/C-ID/D-ID pilot to size cycle/event/host budgets; retain failures and
   incomplete cases, and record the exact manifest, binary, source state,
   program fingerprint, clock, configurations and measurement window.
4. Run the complete nine-variant matrix at one technology and workload set,
   serially. Repeat with selected DRAM and FF cases, then a bounded
   structural-reference comparison and a compiled workload. Do not start a
   broad structural campaign until pilot host cost and correctness are known.
5. Publish a table that separates A->B sharing cost, B->C L1 effect,
   C-ID->D-ID L2 effect and the I-only/D-only interactions, including losses
   and any clock-certification or large-depth-estimate limits.

Status: this is a topology and measurement plan. The nine-case CPU runner,
shared direct bypass, CPU-side L2 path and campaign results do not exist yet.

## 2026-10-16 — Focus the CPU campaign on five topologies

This replaces the preceding section's proposal to run all nine variants for
every workload. It retains the same topology and measurement rules. The first
campaign needs these five configurations:

| ID | Connection | Role |
| --- | --- | --- |
| A | Core -> separate instruction RAM and data RAM | Existing two-memory baseline |
| B (`C-0`) | Both CPU ports -> arbiter -> one RAM | Shared-memory baseline; no cache |
| C-ID | I-L1 and D-L1 -> arbiter -> one RAM | Full one-level system |
| D-0 | Both CPU ports -> arbiter -> shared L2 -> one RAM | L2 without L1 |
| D-ID | I-L1 and D-L1 -> arbiter -> shared L2 -> one RAM | Full two-level system |

`C-0` is exactly B, so it is one run, not a duplicate case. These four
shared-memory configurations form a complete 2x2 comparison: L1 pair absent
or present, and shared L2 absent or present. B vs C-ID measures both L1s
without L2; D-0 vs D-ID measures them with L2. B vs D-0 measures L2 alone;
C-ID vs D-ID measures L2 beneath the L1s. The difference between these two
L2 effects shows whether L1 traffic suppression changes L2's value. A vs B
remains the broader two-store versus one-store/arbiter comparison, with the
storage and contention difference stated explicitly.

Keep per-port, arbiter, L2 and RAM traffic/stall counters in all five runs.
They can point to which port dominates without paying for four more full
campaign variants. `C-I`, `C-D`, `D-I` and `D-D` are **ablation runs** on
selected workloads only when the full L1 pair's gain or loss needs
attribution, or when choosing different I/D cache sizes. Their marginal
effects may interact through arbitration; a port counter alone does not
establish the causal speedup of one cache. The reusable topology builder may
still accept optional I/D caches for generality. A short wiring/correctness
smoke for each optional path is enough before targeted performance use.

Execution order: first A and B to validate the unified image and bypass;
then C-ID; then D-0 and D-ID. Run the five-case SRAM panel with the frozen
workloads, clock and cache geometry above. Use its counters to select any
I-only/D-only follow-up. Repeat the main five on selected DRAM workloads and
FF controls within measured host budgets. Keep cold and warm windows,
oracle/final-memory checks, manifest/provenance, clock limits and negative
results exactly as specified above.

Status: this is a reduction of the planned experiment matrix. No new CPU
topology or benchmark has been implemented or run.

## 2026-10-16 — Focused SRAM-cache/DRAM-memory experiment plan

### Decision and scope

The owner chose one technology stack for the cache-performance work. **Both
cache levels use SRAM for tag and data banks; the final backing memory uses
DRAM.** A read-only SRAM instruction L1 and a writable SRAM data L1 sit above
one writable SRAM L2 when present. DRAM is main memory after L2, not the L2
cache's storage. This is the conventional on-chip-cache/off-chip-memory
technology split described in the [Intel Technology Journal](https://www.intel.com/content/dam/www/public/us/en/documents/research/2007-vol11-iss-3-intel-technology-journal.pdf).
The current `CacheConfig` accepts only FF or SRAM tag/data banks; DRAM is a
compatible lower endpoint, not a bank technology.

This section **replaces** the performance technology sweeps in the
2026-10-15 experiment protocol and its FF-L1/SRAM-L2/SRAM-backing CPU preset.
It retains the five CPU topologies adopted immediately above. Existing FF,
ROM and NOR-flash contract tests and public scenarios remain useful component
evidence; they are not additional performance panels. A different technology
is a targeted diagnostic only if an observed result needs attribution. L3,
I-only/D-only performance sweeps, MMIO, self-modifying code and coherence are
outside this campaign. This plan changes no component contract or timing
constant and makes no claim that simulator ticks equal physical nanoseconds.

### Fixed hardware profile

| Part | Initial frozen configuration | Role |
| --- | --- | --- |
| Upper port | `W=32`, aligned byte addresses, existing REQ/RSP contract | Same word and transaction rules throughout |
| I-L1 and D-L1 | Each 4 sets, 2 ways, 2 words per line (`block_bytes=8`): 64 data bytes; SRAM tags and data | I-L1 read-only; D-L1 write-through and no-write-allocate |
| Shared L2 | 64 sets, 2 ways, 4 words per line (`block_bytes=16`): 2,048 data bytes; SRAM tags and data | One read/write cache after the arbiter |
| Main RAM | One scalable 65,536-word (`256 KiB`) DRAM endpoint; A has two separately initialized, otherwise identical copies | One address image for code and data in B/C-ID/D-0/D-ID |
| DRAM geometry/timing | 16 words per row, `phase_ticks=32`, `guard_ticks=64`, `refresh_interval_ticks=10000`, `retention_ticks=1000000000` | Freeze the existing timing units; a four-word L2 line fits within a row |

The 16-word row and the existing default phase, guard and refresh values are
predeclared, not tuned after seeing cache results. Use the same RAM depth,
image, row policy, timing and clock in all paired cases. Set each cache's
`backing_depth_words=65536`. Check DRAM refresh-configuration validity before
building and include refresh state in the measurements. Keep history recording
off for headless performance runs. Store the exact `BuildManifest` and
`EndpointConfig`; selecting the same technology name is insufficient if one
run silently chooses a different controller or native-device fidelity.

For the **primary performance** run, keep cache control structural, select
scalable SRAM banks at both levels and a structural DRAM endpoint with a
scalable native store. Use the same profile in every compared topology; the
advanced core may use its compact form for bounded host cost. The cache has
no fused behavioral implementation. If the structural endpoint profile is
too expensive for long programs, first establish matched response timing and
correctness for its fused behavioral endpoint at a small geometry, then
publish that as a separately named profile. Never combine profile results in
one comparison table. The small expanded SRAM/DRAM slice below remains the
lower-level reference for the scalable paths.

### Step 1 — Prove the chosen parts and measure their latency

Build a small `W=32` reference circuit with the same L1 geometry, a reduced
4-set/2-way/4-word SRAM L2, and a 64-word DRAM endpoint with 16-word rows.
All expanded stores fit the 4,096-cell limit. For each part, compare an
expanded structural device/endpoint with the scalable native store behind
the same structural controller at identical small geometry. Include one
whole stack. Check accepted requests, response data/status, completion cycle,
masked writes, repeated reads, refill order, reset and DRAM row/refresh
cases. Use the existing delay-rule evidence; add focused transaction timing
checks where it does not cover this stack. Compare the fused behavioral
endpoint only if a later performance run will use it.

Measure an SRAM L1 read **hit**, an L2 hit after an L1 miss, and DRAM row-hit,
row-conflict and refresh-delayed reads with the chosen common period. This is
an engineering gate: record how much of a hit is tag lookup, data lookup,
handshake and response holding. The current cache accesses its generic SRAM
tag and data endpoints serially, while the SRAM controller waits four native
phases plus guards. The label "SRAM L1" alone therefore does not establish a
short near-core hit. If the hit path is unexpectedly slower than a DRAM
access, identify the structural bottleneck before drawing conclusions about
the value of caches; do not shorten phase delays just to force a gain.
Small-device equivalence supports the model, not a silicon timing claim.

### Step 2 — Standalone direct, L1 and L1/L2 comparison

Add one deterministic `CachePerformanceBenchmark` to the existing memory
benchmark runner and common `benchmark::RunRecord`. Drive the *same* 256-KiB
DRAM image and request trace through these three newly constructed paths.
The standalone L1 and L2 both use `read_write` mode for the mixed trace:

| ID | Path | Question |
| --- | --- | --- |
| S0 | Requester -> DRAM | Direct access baseline |
| S1 | Requester -> SRAM L1 -> DRAM | Does L1 reuse repay lookup and fill cost? |
| S2 | Requester -> SRAM L1 -> SRAM L2 -> DRAM | Does L2 save DRAM reads after L1 misses? |

Use these four fixed traces, each with a fresh identical initial image:

1. Repeat a small eight-word region for 64 reads, after a common cold prefix
   and in a separately measured warm window. It fits L1.
2. Repeat a 64-word region for two full passes. It exceeds L1 but fits L2;
   record the cold first pass and warm second pass separately.
3. Read 1,024 successive words once. It exceeds L2 and tests streaming
   overhead without claiming a cache must help.
4. Interleave byte-masked stores and reads in the small region, including
   one write miss. Check final DRAM bytes and confirm write-through traffic.

All three paths use identical offer times for an isolated-latency run; leave
enough time for every response so queueing does not change the request trace.
Run a second back-to-back mode only if throughput is needed. Count accepted
READ/WRITE requests at every boundary, classify hits/misses from the actual
accepted trace, and record offer/accept/response times, modeled ticks,
common-period cycles, DRAM row/refresh events, event count and host seconds.
An L1 hit should avoid lower READs, and an L2 hit should avoid DRAM READs;
those are traffic checks, not prerequisites for a speedup. Compare full cold
cost and warm cost separately. A slower S1 or S2 remains a valid result.

### Step 3 — Five advanced-core systems

Add bypass and L2 choices to one reusable CPU memory-topology builder shared
by `RV32IProgramRoot` and `RV32IExperiment`. Use the advanced core only: it
handles delayed endpoints. Keep the core fidelity, predictor, clock, full
program image, RAM capacity and initial bytes fixed. A's two DRAM stores both
receive the *same complete image*; B/C-ID/D-0/D-ID each use one shared DRAM
image and the same structural two-port arbiter. No program in this campaign
writes executable bytes.

| ID | CPU path | Comparison role |
| --- | --- | --- |
| A | I port -> DRAM I copy; D port -> DRAM D copy | Existing two-store regression |
| B | I/D ports -> arbiter -> DRAM | Shared uncached baseline |
| C-ID | I/D ports -> separate SRAM L1s -> arbiter -> DRAM | L1 pair effect against B |
| D-0 | I/D ports -> arbiter -> shared SRAM L2 -> DRAM | L2 alone against B |
| D-ID | I/D ports -> separate SRAM L1s -> arbiter -> shared SRAM L2 -> DRAM | Full hierarchy; L2 effect against C-ID and L1 effect against D-0 |

First pass a bounded store/load and branch/redirect program through all five
paths, checking instruction-oracle retirement, final registers, completion
reason and every final RAM byte that the program may write. Then run fixed
program 20 as a campaign smoke. The main workload set is **`calls`** for
instruction reuse, **`sort`** for reused writable data and **`stream`** for
contiguous traffic and write-through pressure. Freeze each workload's seed,
size, iteration count, expected result, image hash and measurement PC window
*before* measuring any topology. Check each workload's address footprint and
completion budget against the common 256-KiB image. `pointer-chase` or an
I-only/D-only case is a follow-up only if the primary counters leave a
specific cause unresolved; it is not part of the first full matrix.

Run A and B first to validate the shared-image migration, then C-ID, D-0
and D-ID. Pilot the smallest complete program to establish bounded
cycle/event/host budgets. If a longer workload exceeds them, choose a smaller
common input for **all five** cases and record that change before comparing
results. An incomplete run never becomes a speedup datum. Preserve the old
two-store result as a regression check.

### Fairness, reporting and interpretation

- Obtain a common safe period for all compared CPU topologies where the
  timing analyzer supports them. If the cached root remains uncertified,
  use one conservative period with zero observed setup violations and a
  nearby-period correctness check; label it exploratory. Report each
  topology's eventual certified minimum period separately. Common-period
  cycles are the primary architectural comparison; modeled ticks are not
  physical seconds.
- Record CPU cycles, retired instructions, CPI, I/D request waits, arbiter
  grants/contention, cache-boundary requests and responses, L1/L2 hit/miss
  classifications, DRAM reads/writes and row/refresh state, simulator events
  and host seconds. Keep reset, image loading, warmup and verification outside
  the measured CPU window. Record both a complete cold run and a comparable
  warm body window when the workload offers one.
- Require matching oracle state, expected result, retired instruction count,
  final RAM bytes and completion status. In A, check the D store's final
  writable bytes and the unchanged I copy separately. Speculative fetch
  traces may differ, so do not require identical I-port request counts.
  Compare B->C-ID for L1, B->D-0 for L2 alone, C-ID->D-ID for L2
  beneath L1, and D-0->D-ID for L1 above L2.
  A->B includes the change from two physical stores to one shared store;
  never attribute that whole difference to arbiter logic alone.
- Report negative results. If caches save DRAM READs but add cycles, use
  measured hit latency, line-fill transactions, write-through traffic and
  contention to locate the cost. Any change to lookup structure, SRAM
  controller, fill width or write policy starts a new measured revision with
  structural and equivalence tests; do not tune undocumented delays or swap
  to FF banks to manufacture a positive headline.

### Implementation and completion gates

1. Add the small expanded/scalable SRAM-and-DRAM parity fixture and focused
   CTest timing assertions. Retain existing FF/ROM/NOR regressions without
   expanding them into performance campaigns.
2. Add the three-path standalone runner, four frozen traces, independent
   expected responses and boundary counters. Register it in
   `tools/benchmarks/run.py memory`; produce provenance and complete/partial
   artifacts through the existing serial campaign contract.
3. Generalize the CPU topology builder and runner, add short correctness
   CTests for all five paths, then run the fixed three-workload panel using
   the same BuildProfile and DRAM configuration. Validate the five manifests.
4. Publish a dated result record with complete configuration, source/binary
   hashes, clock evidence, counts, latencies, CPU comparisons, failures and
   host budget. Update the public cache and RV32I experiment pages only for
   implemented behavior and checked measurements. If those pages or visual
   scenarios change, run the strict site build and relevant browser/WASM
   checks. Build serially as required by `AGENTS.md`.

The experiment is complete when every declared run either has verified
results or is explicitly reported as incomplete, and the B/C/D comparisons
have a causal explanation supported by the boundary counters. A positive
speedup is **not** a completion gate. Neither small-structure parity nor a
speedup in this digital model proves physical DRAM/SRAM latency, energy use
or game performance; those require separately calibrated targets and later
program measurements.

Verification for this planning update: reviewed current cache validation and
ports, endpoint geometry/timing, CPU root/experiment construction, benchmark
runner and the open record. No code, benchmark fixture, CPU topology, timing
constant or performance result was changed or run in this section.

## 2026-10-16 — Plan review: hit latency measured before building the campaign

This section reviews the focused SRAM-cache/DRAM-memory plan above against
current source and a small latency probe. It does not change that plan; it
adds evidence that the owner needs before the campaign is built.

### Probe

A scratch C++ program (not in the repository) linked the current
`build-cleanup/libSimulatorLib.a` (built from the uncommitted worktree on
`alpha` at `1d861c0`, newer than `Cache.cpp`). It drove one requester at a
fixed clock with `RSP_READY=1`, one request at a time, and counted rising
edges from the request-transfer edge to the response-transfer edge. Backing
was a 64-word W=32 DRAM endpoint, 16 words per row, `phase_ticks=32`,
`guard_ticks=64`, refresh disabled to isolate row effects. Caches were
`Cache<32>`, expanded banks, read/write: L1 4 sets × 2 ways × 8-byte lines
(the planned L1 geometry), and the plan's reduced L2 4 × 2 × 16-byte lines.
Hits and misses were inferred from the address sequence and the latencies;
the probe's lower-request counter did not work and was not used.

Cycles at period 102 (the current timed-memory experiment period):

| Access | Direct DRAM | SRAM L1 → DRAM | FF L1 → DRAM | SRAM L1 → SRAM L2 → DRAM |
| --- | ---: | ---: | ---: | ---: |
| Read, DRAM row already open / L1 hit way 0 | 4 | 14 | 4 | 14 |
| Read, L1 hit in way 1 | — | 21 | 6 | 21 |
| Read, DRAM row conflict | 10 | — | — | — |
| Read miss, same DRAM row | — | 49 | 19 | 69 (L2 hit) |
| Read miss, DRAM row conflict | — | 55 | 25 | 132–139 |
| Write, same row (write-through) | 5 | 21 | 11 | 37 |

The fused behavioral and structural-controller DRAM endpoints gave identical
cycles. At periods 50 and 400 the ordering was unchanged: SRAM L1 hit 20 and
8 cycles against DRAM row hit 7 and 2, row conflict 19 and 4. An FF-bank L1
hit took 4 cycles at every period, so it is bound by controller steps, not
by storage. Period 50 is uncertified and was used only to show the trend.
Host time for construction plus about 300 cycles was 0.3 s (structural DRAM
endpoint), 4.7 s (SRAM L1), 9.3 s (expanded FF L1) and 19 s (L1+L2).

### Why, from source

- An SRAM endpoint request takes `4*phase_ticks + 2*guard_ticks` plus a
  release guard: 320 ticks (`MemoryEndpointEngine.cpp`, `StaticAccess`). A
  DRAM read to the open row takes command raise/lower guards, one phase and
  a release guard: 288 ticks. **With the current constants an SRAM bank
  access is slower than a DRAM row-buffer hit.** A closed row adds an
  activate (288 ticks); a row conflict adds a precharge too.
- `CachePhaseSequencer` reads the tag, decides, then reads data: two full
  bank transactions per hit. A two-way cache searches way 0 and then way 1
  serially (`RETRY_SECOND_WAY`), so a way-1 hit costs a third transaction.
- A fill is serial: per word one lower READ and one bank WRITE, then a tag
  WRITE, then the data READ that answers the request.
- With 16-word rows, DRAM's open row already acts like one 64-byte line with
  a 4-cycle hit. The existing `MemoryConfiguration` default is 2 words per
  row, so published DRAM results used a different row size from this plan.

### Consequence for the plan

Even at a 100 % hit rate, the planned SRAM L1 cannot beat direct DRAM when
the DRAM row is open, and it is slower than a row conflict. The SRAM L2 hit
after an L1 miss (69 cycles) is far slower than any direct DRAM access.
The planned S1/S2 and C-ID/D-0/D-ID comparisons would therefore measure,
almost certainly, that every cached path is slower, for a reason already
visible in the timing constants and controller sequence rather than in
locality. The only plausible cached gain is topology B, where I and D
traffic could thrash one DRAM row buffer; an SRAM L1 hit (14) still exceeds
a row conflict (10) there. Running the plan unchanged would document this
cost, but would not answer its stated question about cache value.

Plan details that need correction regardless of that choice:

- The "A reproduces the published result" check must use the published
  memory settings (2-word rows, fused endpoint profile at
  `EXPERIMENT.IMEM/DMEM`), not the new 16-word-row configuration.
- The CPU safe-clock analysis supports `BehavioralMemoryEndpoint`
  handshake models (`memoryTimingStates`); the cache's internal endpoints are
  not covered, so the cached roots will be uncertified as the plan expects.
- The named workloads are tiny loops. `sort`'s code (about 25 instructions)
  exceeds the 64-byte I-L1; `calls` and `stream` fit. Footprints should be
  stated with the frozen sizes.

Decision status: the owner must choose between running the current design as
a revision-0 cost measurement, improving the cache hit path structurally,
and predeclaring a different SRAM/DRAM timing calibration, before Step 2 is
built. No repository code, test or timing constant was changed. No CTest or
full suite was run for this review.

## 2026-10-16 — Review: run the cache experiment after the realistic-timing plan

The owner asked whether this experiment can wait until the
[realistic leaf delays and gate-level memory control plan](2026-10-16-realistic-timing-plan.md)
is finished. Review against the focused SRAM-cache/DRAM-memory plan above:

**Yes, it should wait.** Under current timing an SRAM bank access is slower
than a DRAM open-row read, so every cached path is predictably slower;
running now would measure the timing constants, not caching. The timing plan
fixes exactly that, and its phase 7 also changes the cache's hit path. Both
would make results from an earlier run obsolete.

What stays valid:

- The question, the five CPU topologies (A, B, C-ID, D-0, D-ID) and their
  2x2 interpretation, the three standalone paths S0–S2, the four traces, the
  workloads (`calls`, `sort`, `stream`), fairness and reporting rules,
  negative-result rule and completion gates.
- The owner's technology choice: SRAM L1/L2 over DRAM main memory. It only
  becomes a meaningful test once SRAM is faster than DRAM.
- The current-timing measurements as a historical "revision 0" row,
  reproducible from the planned `timing-legacy` tag.

What must be restated before running (a new dated section at that time):

1. **Fixed hardware profile.** `phase_ticks`, `guard_ticks`,
   `refresh_interval_ticks` and `retention_ticks` are removed or redefined.
   Name the timing profile (`embedded` by default, `desktop` as a second
   panel) instead. Take DRAM row size, refresh and timing from the cited
   SDRAM datasheet rather than the 16-word row chosen for the old model.
2. **Step 1** becomes a check that, under the chosen profile, an L1 hit costs
   a few cycles and a DRAM access tens of cycles, as the calibration intends;
   the structural/scalable parity part is covered by timing-plan phases 5–6.
3. **Cache geometry.** The 64-byte L1 and 2-KiB L2 were sized for the
   abstract model and the current limits (64 scalable sets, four words per
   line). Revisit sizes with realistic miss costs, and extend the cache limits
   if needed, before freezing the panel.
4. **Bursts and DRAM banks.** Decide in timing-plan phase 7, before this
   experiment's panel is frozen, since line fills dominate miss cost.
5. **Clock.** Use the per-profile certified period. After timing-plan phase 4
   the analyzer may certify cached roots; report any that remain uncertified.
6. **Host budgets.** Re-pilot: realistic DRAM latency of tens of cycles
   lengthens every run.

Timing-independent infrastructure (the shared CPU memory-topology builder
with bypass and L2, boundary counters, the `CachePerformanceBenchmark`
harness) could be built earlier. It is deferred too: it touches files the
concurrent sessions and timing-plan phases 4–7 also change, and would need
rechecking afterwards.

No code changed and nothing was run for this review.

## 2026-10-16 — Current experiment plan after the realistic-timing work

This section **replaces**, in the focused SRAM-cache/DRAM-memory plan above,
its fixed hardware profile, Step 1, the absolute trace sizes and the frozen
cache geometry, and turns the previous review's "must be restated" list into
decisions. Everything else in that plan stays: the question, technology
choice, standalone paths S0–S2, five CPU topologies, workloads, fairness,
reporting, negative-result rule and completion gates.

### Preconditions

Run only after phases 0–7 of the
[realistic-timing plan](2026-10-16-realistic-timing-plan.md): realistic leaf
delays, gate-level memory control, geometry-dependent arrays, calibration and
the cache hit-path rebuild. No campaign runs on the current timing; the
recorded probe results stand as the only current-timing ("revision 0")
evidence, reproducible from the `timing-legacy` tag. Building the runner
twice for a revision-0 campaign is not worth it.

### Timing and memory settings

| Setting | Decision |
| --- | --- |
| Timing profile | `embedded` (65 nm) for the primary panel; the same five topologies and three workloads repeated under `desktop` (32 nm) as a second panel within its host budget |
| Main memory | One DRAM endpoint; 65,536 words (256 KiB) as before; refresh interval, retention and command timing from the profile (Micron MT48LC16M16A2 for `embedded`, MT40A1G8 for `desktop`) |
| DRAM row size | From the datasheet instead of 16 words: a 32-bit bus of two ×16 MT48LC16M16A2 chips gives 1 KiB per chip, 512 words per row. Requires raising the 16-word row limit (`MemoryProtocol.cpp:32`, `DigitalMemory.cpp:49`) for the scalable device in timing-plan phases 5–6; the structural reference stays a small slice |
| DRAM banks | One bank unless timing-plan phase 7 adds banks; state it in results (the datasheet part has four) |
| Bursts | Decided in timing-plan phase 7; record whether line fills are bursts or single-word reads |
| Clock | The per-profile certified period. Cached roots that the analyzer still cannot certify use the earlier rule: a conservative common period, zero setup violations, a nearby-period check, labeled exploratory |
| Provenance | Profile name and fingerprint added to every run record, with the existing manifest, source and binary hashes |

### Step 1 (replaces the earlier structural parity step)

Structural/behavioral parity is covered by timing-plan phases 5–6. Before
the campaign, under each profile, measure and record: an L1 hit, an L2 hit
after an L1 miss, DRAM open-row, closed-row and row-change reads, and a
refresh-delayed read. Expected order: L1 hit a few cycles, L2 hit well below
a DRAM access, DRAM tens of cycles under `embedded`. A violation stops the
campaign and goes back to the timing plan; delays are never adjusted to suit
the cache.

### Cache geometry and traces

- Sizes are frozen in a pre-run section **after** Step 1, by this rule: the
  L1 is smaller than the `sort` working set and the L2 larger, so the three
  workloads cover fits-L1, fits-L2 and exceeds-L2 cases. Default starting
  point within today's limits: L1 8 sets × 2 ways × 16-byte lines (256 B
  each); L2 64 sets × 2 ways × 16-byte lines (2 KiB). If the rule needs more,
  timing-plan phase 7 raises the 64-set and four-words-per-line limits
  (`Cache.cpp:23–26`) first.
- Standalone traces are defined relative to the frozen sizes instead of in
  absolute words: a region half the L1 data size, read repeatedly; a region
  twice the L1 size and half the L2 size, two passes; a stream four times the
  L2 size; and masked stores and reads in the first region, with one write
  miss.
- Workload inputs (`sort`/`stream` size, iterations, seed) are frozen in the
  same pre-run section, after a pilot sizes host budgets per profile.

### Order

1. Timing-plan phases 0–7 complete.
2. Step 1 per profile; freeze geometry, traces and workload inputs in a dated
   section.
3. Build the shared CPU memory-topology builder (bypass, L1s, shared L2),
   boundary counters and `CachePerformanceBenchmark`, with focused correctness
   tests per topology (no full suite).
4. Standalone S0–S2 under `embedded`, then the five-topology CPU panel, then
   the `desktop` panel.
5. Publish per the earlier reporting rules; full suite and site checks once,
   before publishing.

No code changed and nothing was run for this update.

## 2026-10-19 — Campaign results published (assistant decision)

The campaign planned above ran during phase 7 of the
[realistic-timing work](2026-10-16-realistic-timing-progress.md) (Step 1,
standalone S0–S2 with locality traces, the five-topology CPU panel and
pointer-chase, in `embedded` and `desktop`; every run verified). It stayed
"exploratory and unpublished" for two reasons, both resolved in phase 8: the
gate-level clocked SRAM macro inside the cache (explained and fixed; the
standalone benchmark with gate-level banks gives identical cycles and
traffic) and the cached roots' clock (now certified by the analyzer at the
core's clock, the clock the campaign used). The remaining condition was the
owner allowing site updates, which came with the documentation phase.

**Decision:** publish the results as a section of § 4.7 ("Memory topologies
and caches"), not on the cache component page, whose rewrite the owner
deferred. The tables are generated from the raw run records (checked:
verified, completed, zero setup violations) and equal the progress-record
copies. The C-ID → D-ID statement is limited to `calls`, `sort` and
`stream` (4–8 % slower); `pointer-chase` gains from the L2 under the L1s in
`embedded`. A topology figure (`cache-topologies`) is added. Spot checks of
the panel at the current code are recorded in the documentation record.
