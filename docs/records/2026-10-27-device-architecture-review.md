# Device architecture review against CS:APP

Started: 2026-10-27. Status: open. Code state: `alpha` at `5549ee0`, with the
existing uncommitted component, visualizer, profile and recording changes
preserved. Review only: no source, test, layout or recording was changed.

## Summary

CircuitSim has buses in the CS:APP sense: one universal valid/ready
request/response transaction contract carries address, data, mask, operation
and status between the core, caches, arbiter, RAM and every device. The
memory-mapped device side is careful (validated status precedence, side
effects once, held responses, caches only on the RAM branch, structural-first
devices with compact forms). It is a fixed games platform, not yet a
general, replaceable architecture:

- The address map has exactly six named regions; adding a device means
  editing the map type, router pins, parameter parser and system recipe.
- There is no I/O bridge or interconnect component: the router (one
  requester, one outstanding transaction), the fetch guard and a fixed
  two-port I/D arbiter are wired by hand in one recipe.
- There are no interrupts, no DMA and no exception handling. Every trap halts
  the core; the core has no Zicsr, `mtvec`, `mepc`, `mcause` or `MRET`.
- Standard parts (arbiter, cache) are created by class rather than by family.

A seven-phase target architecture follows the CS:APP layering (system bus →
I/O bridge → memory bus and I/O bus → device controllers, plus interrupts
and DMA), keeps the existing protocol and default contract IDs, and keeps
the ten scripted game fixtures byte-identical until a phase deliberately
changes structure. Owner decisions are listed at the end. Nothing is
implemented. Builds on the [Device I/O component review](2026-10-27-device-io-review.md)
and the [device/game roadmap](2026-10-19-device-io-games-plan.md).

## 2026-10-27 — Textbook reference model

Source: `books/… Computer Systems_ A Programmer’s Perspective-Pearson (2015).pdf`
(CS:APP 3e), read through `pdftotext` page ranges. Page numbers are the
printed ones.

- **1.4.1, Figure 1.4 (p. 44).** CPU, system bus, I/O bridge, memory bus,
  main memory and an I/O bus with controllers (USB, graphics) and adapters
  (disk). Data moves from disk to memory by DMA "without passing through the
  processor" (Figure 1.6).
- **6.1.1, Figures 6.6–6.8 (pp. 624–626).** "A bus is a collection of parallel
  wires that carry address, data, and control signals." Control wires say
  whether a transaction is a read or write and whether it is "of interest to
  the main memory, or to some other I/O device". The I/O bridge, which
  includes the memory controller, translates system-bus transactions to the
  memory bus. Read and write transactions are multi-step handshakes.
- **6.1.2, Figures 6.11–6.12 (pp. 632–634).** I/O buses are "designed to be
  independent of the underlying CPU" and accept many kinds of devices through
  controllers and adapters. The Aside notes that shared PCI has been replaced
  by point-to-point PCIe links joined by switches; the differences are not
  visible to programs. The CPU commands devices through memory-mapped I/O
  ports. A device then performs its own bus transactions (DMA), and on
  completion "notifies the CPU by sending an interrupt signal".
- **8.1, Figures 8.1–8.6 (pp. 758–764).** Exceptions go through an exception
  table to handlers. The four classes are interrupt (asynchronous, from I/O,
  returns to the next instruction), trap (intentional, for example a system
  call), fault (might return to the current instruction) and abort (never
  returns). An interrupt is taken after the current instruction finishes.
- **10.1 (p. 926).** All I/O devices are modeled as files, giving software
  one uniform interface. This is the software-side counterpart of a uniform
  device-controller contract. The guest platform library was not reviewed
  here.

## 2026-10-27 — Current structure mapped to the reference

| CS:APP element | CircuitSim today | Evidence | Assessment |
| --- | --- | --- | --- |
| Bus transaction (address/data/control) | Universal REQ/RSP: `REQ_VALID/READY`, `REQ_OP[3]`, `REQ_ADDR[32]`, data, byte mask; `RSP_VALID/READY`, data, `RSP_STATUS[3]` | `MemoryProtocol.hpp:10-11`, `MemoryProtocol.cpp:103-112` | Strong. Shared by memory and devices. Point-to-point links, like the PCIe Aside. |
| System bus (CPU side) | Core `IMEM_` and `DMEM_` requester ports | `RV32IMDeviceSystem.cpp:100` | Present (split I/D). |
| I/O bridge | No component. Its role is spread across `DATA_ROUTER`, `FETCH_GUARD`, `RAM_ARBITER` and optional caches, wired in the system recipe | `RV32IMDeviceSystem.cpp:58-61,100-111` | Missing as an architectural unit. |
| Memory bus and controller | Arbiter → RAM endpoint (controller + native device); caches only on the RAM branch | `RV32IMDeviceSystem.cpp:101-109` | Present. The arbiter has exactly two requesters (I, D). |
| I/O bus | No shared segment. Each device is its own router port | `RV32IMDeviceSystem.cpp:110-111` | A point-to-point equivalent, but closed to new devices (below). |
| Device controller / adapter | A common front end: `DeviceRegisterPort` + generated register decode, used by console, timer, buttons and display | `DeviceLogic.hpp:89`, `DeviceState.cpp:181-185`, `DeviceBus.cpp:151-218` | A good pattern, but not a published contract. Each device writes its own decode and command group. |
| Memory-mapped I/O ports | Base/size windows, offsets per device | `DeviceConfiguration.hpp:32-56`, `site-docs/device-io/index.md` | Present. Count and names are fixed. |
| DMA | None. The arbiter cannot take a third requester | `TwoPortMemoryArbiter.hpp:5-8`; roadmap "no display DMA requester" | Missing. |
| Interrupts | None. "No interrupt or deadline comparator exists yet" | `site-docs/device-io/timer.md:41` | Missing; games poll. |
| Exceptions (trap/fault) | Illegal instruction, ECALL and bus-status errors stop the core (`TRAPPED`, `TRAP_CAUSE`); EBREAK halts | `RV32IDecodeControlUnit.cpp:48-50`, `site-docs/rv32im/device-system.md:45-47`, `site-docs/rv32im/core.md:80`, `site-docs/rv32i/4.1/machine.md:63` | Only the "abort" class exists; there are no handlers. |
| Uniform software interface (10.1) | Not reviewed (guest platform library) | — | Unverified. |

Additional transaction facts: the router accepts one transaction at a time
(`BUSY`/`SENT` registers, `DeviceBus.cpp:86-92`), forwards only read/write
(reference "only operation 0/1 is forwarded"), and the protocol carries no
read size. LB and LW to a register look the same, which the roadmap already
records. The arbiter alternates priority (round robin) and keeps a burst
grant until its last word (`TwoPortMemoryArbiter.cpp:62-64`).

## 2026-10-27 — Is it general and replaceable?

| Question | Answer | Evidence |
| --- | --- | --- |
| Same protocol for memory and devices? | Yes. Any endpoint family can sit behind any port. | `protocolPins` is used by router, port, devices and endpoints |
| Devices interchangeable by fidelity? | Yes. Device families have structural and compact forms; compact forms compile their own structural control. | `DeviceState.cpp:30-60`; device review |
| Address map data-driven? | Partly. Base and size are parameters, but there are always exactly six regions with fixed names (`std::array<…,6>`, names checked). | `DeviceConfiguration.hpp:36-45`, `DeviceLogic.hpp:45-56` |
| Add a device without editing the bus or system? | No. You must edit `AddressMap`, the router pin list (a second, global copy of the default map), the parameter parser and the system recipe's instantiation and wiring. | `DeviceBus.cpp:19-20,226`; `RV32IMDeviceSystem.cpp:24,67-73,110-116` |
| Is the system configured from one description? | No. The system forwards only `video_bytes` to the router; RAM depth (65,536 words) and the ELF limit (`0x40000`) are hard-coded beside a configurable `ram_bytes` router parameter. The guard accepts no parameters. | `RV32IMDeviceSystem.cpp:59,64,121`; `DeviceBus.cpp:227,264` |
| Parts selected by family? | Not everywhere. The arbiter and cache are created by class, bypassing family selection and manifests, although `memory.arbiter.32` exists. | `RV32IMDeviceSystem.cpp:61,105`; `TwoPortMemoryArbiter.hpp:21-22` |
| Reusable for a GPU or accelerator? | Not yet. It would need a requester (master) port into RAM, an N-port arbiter or interconnect, a completion interrupt, and a coherence rule. The D-cache's invalidate port exists but is tied off. | `RV32IMDeviceSystem.cpp:106` (`INV_VALID` false) |

## 2026-10-27 — Quality assessment

Strengths:

- One transaction contract from core to every target, with documented
  status precedence, side effects that happen exactly once, held responses
  under back-pressure, and no side effect on error or zero-mask writes
  (`rv32im-device-foundation.md`, "Device bus and side effects").
- Correct cache placement: device traffic bypasses every cache, and fetches
  to device space fault without side effects.
- Lower-level-first is respected: structural devices exist, and compact forms
  are proven gate-list compilations.
- Deterministic scripted input, separate device storage instances, and
  bounded observation and recording.

Structural weaknesses:

- **Closed topology.** The six-region map and the hand-wired system recipe
  (above).
- **No I/O bridge or interconnect unit.** The textbook's central component
  exists only as wiring.
- **Blocking router.** One outstanding transaction, with no ID or ordering
  field. Adequate for an in-order core; limiting for DMA or several masters.
- **Two-requester arbiter only.** No DMA or accelerator master is possible.
- **No completion signalling.** No interrupts, no timer compare, and traps
  are terminal, so CS:APP 8.1 handler flow (fault → handler → return) cannot
  be shown.
- **Uneven family use.** The arbiter and cache are built by class (above).
- **Adapter bloat.** About 14% of components in the reviewed device
  scenarios are bit adapters (device review); this makes the bus groups
  visually crowded.

Inconsistencies and documentation gaps:

- The default map is duplicated (`DeviceConfiguration.hpp` and
  `DeviceBus.cpp:19-20`).
- The router's per-target `CLK`/`RST` outputs are unused.
- The router page implies those child clocks are used.
- No page explains the architecture as layers (bridge, memory bus, I/O bus);
  `site-docs/device-io/index.md` lists components and the map only.

## 2026-10-27 — Target architecture

Keep the REQ/RSP protocol as the system-wide bus transaction. Add structure
around it:

1. **CPU bus interface.** Unchanged core `IMEM_`/`DMEM_` ports.
2. **System interconnect ("I/O bridge").** A generic structural family,
   proposed ID `bus.interconnect.<M>x<S>.<W>`, built from three parts:
   - a data-driven address decoder: a region table mapped to select lines,
     with per-region attributes (read/write/execute, cacheable);
   - an N-port round-robin arbiter, generalizing `TwoPortMemoryArbiter` with
     two-port arbiters as its structural slice;
   - per-target channels.

   The existing `AddressRouter` becomes the 1×S case and
   `InstructionAccessGuard` a 1×1 case with an execute attribute.
3. **Memory bus.** Cacheable regions go through optional L1/L2 to the RAM
   endpoint (today's RAM branch).
4. **I/O bus.** Uncacheable regions route point-to-point to device
   controllers, as in the PCIe Aside. Optionally group the router and devices
   under an `IO` subsystem component so the visual hierarchy matches
   Figure 6.11. That changes paths, so it is a decision below.
5. **Device controller contract.** A device family must provide:
   - one target REQ/RSP port, `CLK`/`RST`;
   - an optional `IRQ` output;
   - an optional `DMA_` requester port;
   - declared external pins (for example console TX, button input);
   - a register-block descriptor: offset, name, access (R/W/RO/WO/command),
     reset value.

   The descriptor generates the existing `registerDecode` structure and the
   public register table. `DeviceRegisterPort` stays the standard structural
   front end.
6. **Platform description.** One validated, data-driven list of
   `{family ID, instance name, base, bytes, attributes, irq, parameters}`
   drives instantiation, routing and external-pin forwarding. RAM size, ELF
   limit and guard windows come from the same description. The current
   default description reproduces today's machine exactly.
7. **Interrupts and traps.** Two pieces, both structural first:
   - **Interrupt devices.** A timer compare and an external interrupt
     controller (`mtime`/`mtimecmp` plus a small pending/enable/priority
     controller, shaped like RISC-V CLINT/PLIC).
   - **Core changes, machine mode.** A Zicsr subset (CSRRW/S/C and the
     immediate forms) and `mstatus.MIE/MPIE`, `mie`/`mip` (MTIE, MEIE),
     `mtvec` (direct mode), `mepc`, `mcause`, `mtval`; MRET; ECALL, EBREAK,
     illegal instructions and bus faults vector to `mtvec`. Interrupts are
     taken at commit between instructions (Figure 8.5); the pipeline flushes
     younger instructions for precise exceptions. The independent oracle gets
     its own implementation first.
   - **Compatibility.** A core parameter (`traps=halt` default,
     `traps=vector`) keeps every existing fixture and published cycle count
     unchanged.
8. **DMA.** Devices with a `DMA_` port join the N-port arbiter on the RAM
   side. The first user is a framebuffer fill/copy engine (the roadmap's
   optional blit unit). The coherence rule is DMA to uncached video memory
   first; RAM DMA later uses the D-cache `INV_VALID` port plus FENCE
   semantics. A future GPU or accelerator attaches through the same contract
   (target registers + DMA + IRQ).

## 2026-10-27 — Phased plan

Each phase keeps the ten scripted fixtures passing their exact references.
Phases marked "structure-identical" must also keep recordings and layouts
byte-identical.

| Phase | Goal | Validation | Migration |
| --- | --- | --- | --- |
| P0 | Decide contracts (decisions below); write `docs/reference/system-bus-architecture.md` and a public layering page | Review only | None |
| P1 | Data-driven address decoder and region table (variable count and names). Router and guard become instances of it. Remove the duplicate map; system forwards the full map. Structure-identical for the default map. | Exhaustive region/attribute tests in both fidelities against an independent map model; existing router/guard tests; fixture recordings and layouts byte-identical | None for default IDs; new configurations get new layout variants |
| P2 | Platform description and generic system recipe (device list, external-pin forwarding); arbiter and cache selected by family | All fixtures exact; system children keep names `CORE`, `CONSOLE`, …; manifest audit shows the arbiter/cache families | Profiles gain manifest entries; fingerprints may change and need checking |
| P3 | N-port round-robin arbiter (two-port slice first), `DMA_` requester convention | Exhaustive small-N contention tests against a reference; cache campaign unchanged at two ports | New contract IDs; `memory.arbiter.*` kept |
| P4 | Interrupt devices: timer compare, interrupt controller, `IRQ` lines, pollable pending registers | Device scenario tests (structural/compact), timing tables, site pages | New device IDs only |
| P5 | Core machine-mode traps behind `traps=vector` | Oracle first; new privileged program tests; lockstep structural/compact; timing regeneration; consider the official riscv-arch-test privileged subset; default `traps=halt` keeps fixtures | Core contract parameter; documentation for the RV32IM core and decoder |
| P6 | DMA fill/copy engine to video memory, then RAM DMA with cache invalidation | Byte-exact memory/frame checks; contention tests; no stale lines across FENCE | New device ID; new fixture |
| P7 | Accelerator attach point (target + DMA + IRQ) for the GPU/AI roadmap | Defined when that roadmap starts | — |

**Ordering with the adapter cleanup.** Do the `Logic` adapter fix from the
device review before or together with P1. Both regenerate timing tables and
force recapture of recordings whose paths or event counts change, so batching
them avoids two migrations.

**Resources.** Builds stay serial (one heavy job, 4 GiB address-space
cap). Fixture checks run one at a time under the existing 768 MiB service
caps.

## 2026-10-27 — Owner decisions

1. **What "universal" means here:** a data-driven platform description as the
   single source of the map and device list (recommended), and whether it is
   given as catalog parameters or a JSON platform file.
2. **Visual and path grouping:** adopt an `IO` subsystem / bridge grouping so
   paths and visual hierarchy match Figures 6.6 and 6.11 (path migration), or
   keep the flat system children.
3. **Interrupts and traps:** add RISC-V machine-mode traps (Zicsr subset,
   MRET) to the RV32IM five-stage core behind `traps=vector`, and choose the
   interrupt controller shape: CLINT/PLIC-like (recommended) or a custom
   minimal one.
4. **DMA and coherence:** start with uncached video-memory DMA only, or go
   straight to RAM DMA with cache invalidation.
5. **Contract IDs:** keep `bus.word.address-router.*` and
   `bus.word.instruction-guard.*` as default instances of the new decoder
   (recommended, no migration), or retire them in favour of
   `bus.interconnect.*` with full layout/profile migration (no aliases, per
   AGENTS.md).
6. **Unused per-target outputs:** remove the router/guard per-target
   `CLK`/`RST` outputs (also asked in the device review).
7. **Read size:** add a read-size field to the protocol so devices can tell
   LB from LW (optional; affects every endpoint).

## 2026-10-27 — Verification of this review

Read the textbook sections listed above, the foundation reference, the
roadmap's platform and map sections, the device review summary, the device
headers and `DeviceBus.cpp`, `RV32IMDeviceSystem.cpp`, `MemoryProtocol.hpp/.cpp`,
`TwoPortMemoryArbiter.hpp/.cpp` (comments), the trap encodings and the
relevant site pages. No build, test, simulation or recording was run. The
guest platform library and the WASM device path were not reviewed. The
phase plan's validation steps are proposals, not completed checks.

## 2026-10-28 — Recordings to recapture after this work

The ten committed game recordings (`site-docs/assets/rv32im/circuits/`) are
already stale for the current `unit` timing: same cycles and frames,
different event counts (see the
[non-device plan record](2026-10-27-non-device-improvement-plan.md)). The
owner chose to recapture them once, after the device I/O changes.
