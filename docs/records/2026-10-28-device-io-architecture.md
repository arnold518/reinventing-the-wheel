# Device I/O architecture (fresh design)

Started: 2026-10-28. Status: closed (2026-11-03). Code state: `alpha` at `5549ee0` plus the
uncommitted working tree. Implementation in progress since 2026-10-29.

## Summary

The owner asked for a fresh Device I/O architecture. The current
implementation and CS:APP 3e are references only; existing components may be
reused where they fit. **Goal (fixed 2026-10-28):** a general bus-and-device
architecture — every part talks through one standard bus port, buses connect
masters to slaves through an address map and an arbiter, bridges join buses,
and a new device is connected by describing it, not by rewiring the system.
The games running on it are the demonstration, ending with one game that uses
a device added only through the system description. Interrupts and DMA are
left for later, but the bus takes several masters from the start. The complete plan (eleven
parts, ten steps) is below; the owner's decisions are recorded on 2026-10-29.
**Now (2026-10-29):** implemented and measured (plan status at the end).
The one-bus machine equals the retired fixed system cycle for cycle; two
buses cost exactly one cycle per device access, a registered bridge three;
all 11 games are recorded at embedded timing with a 4.5 ns clock. 2026-11-02:
the full native suite was run once (15 stale failures, each fixed and rerun)
and the web checks covering the profile and layout changes pass; all 11
recordings, the 19 machine runs and the clock certificates were redone and
are identical to the published results. 2026-11-03: the full native suite
(607/607) and the complete browser suite (48 pass, 1 skipped for want of an
external capture, after 11 stale checks and layout gaps were fixed) pass on
the final state. Device structural cleanup is complete: split/join adapter chains were
removed and the Legacy adapter mode retired (2026-11-01); the status choices
use word gates (2026-11-02). **Closed 2026-11-03:** every plan part is done
and measured; follow-up work gets a new record.

Earlier assessments this design starts from:
[device component review](2026-10-27-device-io-review.md),
[architecture review against CS:APP](2026-10-27-device-architecture-review.md),
[device and games roadmap](2026-10-19-device-io-games-plan.md),
[game memory layout review](2026-10-26-game-memory-layout-review.md).

## 2026-10-28 — Why a fresh design

The current platform works and is correct (no incorrect behavior found in the
2026-10-27 reviews), but it is one fixed, hand-wired games machine:

- The address map has exactly six named regions, defined twice; adding a
  device means editing the map type, the router's pins, the parameter parser
  and the system recipe.
- There is no I/O bridge or interconnect component; its job is spread over
  the router, the fetch guard and the arbiter, wired in one recipe.
- The memory side reuses the chapter 3 parts, but no reusable memory
  subsystem exists: the § 4.7 program root (a test fixture) and the device
  system each wire "core → caches → arbiter → memory" by hand. The device
  system exposes two switches (FF or DRAM; one fixed cache) and fixes the RAM
  size in three places that must agree (endpoint depth, router window, ELF
  limit).
- The arbiter and cache are created by class, not selected by contract.
- One transaction at a time, two bus masters only, no interrupts, no DMA;
  every trap halts the core.
- About 14% of the device scenarios' parts are bit adapters.

## 2026-10-28 — Draft goal (for owner confirmation)

**Purpose.** A computer system architecture that a reader can follow from the
textbook's picture down to gates: CPU, system interconnect, memory subsystem
and I/O subsystem, each a replaceable part with a published contract, put
together from one description, and able to run the existing games.

### Goals and how each is checked

| # | Goal | Checked by |
| --- | --- | --- |
| G1 | **Layered like the textbook.** The system's hierarchy is CPU → system interconnect (the I/O bridge) → memory subsystem and I/O subsystem → device controllers, with names and drawing that match CS:APP Figures 1.4, 6.6 and 6.11. | A public architecture page maps each figure element to a component; the simulator shows the same hierarchy. |
| G2 | **One platform description.** One validated description (devices, windows, attributes, memory technology and size, caches, clock) builds the whole system; the address map, RAM size, fetch window and program-load limit all come from it. | Building from the description is the only path; tests build several descriptions, including the current games machine. |
| G3 | **Replaceable parts.** Every part (interconnect, arbiter, caches, memory, each device) is a catalog family selected by contract, with structural and compact forms; a device or memory technology is swapped or added by editing the description only. | A test adds a device and swaps the memory technology without changing any recipe; manifests list every part. |
| G4 | **A reusable memory subsystem.** One memory-subsystem family (technology and size, caches per side, optional shared L2, arbiter) used by this system and by the § 4.7 memory arrangements. | Both use it; § 4.7 results are unchanged. |
| G5 | **A device controller contract.** Every device has the same shape: one target port, clock and reset, declared external pins, and a register block described as data (offset, name, access, reset value) that generates both its decode and its documented register table. | Each device page's register table is generated; a contract test checks every device against it. |
| G6 | **Runs the existing games.** The ten scripted games run on the default description with the same results (frames, console output, cycle counts), keeping the guest ABI and memory map unless the owner decides otherwise. | The scripted fixtures pass; recordings are re-captured once, after this work. |
| G7 | **Lower-level-first and lean.** Structural forms first, compact forms checked against them, proven delay tables; no redundant adapters in new recipes. | Equivalence and delay-table checks; adapter census well below today's 14%. |

### Non-goals (unless the owner adds them)

- More than one clock domain; an MMU or virtual memory; an operating system;
  multiple CPUs and cache coherence between them.
- Live interactive input in this design (it stays the later phase 7 mode).
- Changing the RV32IM core's datapath beyond what interrupts or traps need.

### Constraints

- AGENTS.md: lower-level-first; serial builds; retired contract IDs are
  replaced completely with callers and layouts migrated (no aliases).
- Game recordings are re-captured once, after the device work (owner, 2026-10-27).
- Reuse where it fits: the request/response protocol, the memory endpoints
  and caches, `DeviceRegisterPort`, the FIFO, counter, console, timer, button
  and framebuffer devices, the instruction guard's fetch check.

## 2026-10-28 — Open decisions that fix the goal

1. **Interrupts and traps (CS:APP 8.1).** In this design or later: an
   interrupt controller plus machine-mode traps in the core (CSRs, `mtvec`,
   `MRET`), so a device can signal completion instead of being polled.
2. **DMA and a second bus master (CS:APP 6.1.2).** In this design or later:
   an N-port interconnect that accepts device masters, with a DMA engine as
   the first one.
3. **Accelerator attach point.** Whether G1–G5 must already leave a defined
   place for the later AI accelerator (target registers, DMA, interrupt).
4. **Compatibility.** Keep today's guest ABI and memory map as the default
   description (games unchanged), or allow a new map with the games rebuilt.
5. **Description format.** Catalog parameters, a C++ description structure,
   or a JSON platform file shared with the simulator and the site.

## 2026-10-28 — Goal fixed by the owner (replaces the draft goal above)

The owner's aim is "a general model, focusing on the communication through
buses, and connecting new devices", shown by the games. This replaces the
draft goal table above; its G4 (memory subsystem), G6 (games) and G7
(lower-level-first) carry over as means and checks, not separate aims.

**Goal.** A general bus-and-device architecture, demonstrated by the games
running on it, including one game that uses a device added only through the
system description.

What that means:

1. **One port for everything.** Every master (the CPU now; DMA or an
   accelerator later) and every slave (memory, devices) uses the same
   request/response port (today's protocol: address, data, byte mask,
   operation; answer with data and status).
2. **A general bus.** Any number of masters and slaves; an address decoder
   driven by a region table (data, not hand-written logic), an arbiter that
   takes turns fairly between masters, and a return path; unmapped addresses
   get an error answer.
3. **Bridges.** A bridge is a slave on one bus and a master on another, so
   buses chain into the textbook's layers (a fast system bus for CPU and
   memory, a slower peripheral bus for devices).
4. **A standard device shape.** A slave port, a register table (name,
   offset, access, reset value) from which the address-decoding front end
   and the documented register table are generated, and declared outside
   pins.
5. **One system description.** Buses, what is attached to each, and at which
   addresses; the whole system is built from it, and adding a device adds
   one entry.

How it is shown and checked:

- Random traffic from several masters on the bus against a simple reference
  model.
- Today's games machine as one description, with today's memory map and
  guest ABI kept: the ten games give the same results (recordings are
  re-captured once, afterwards).
- A small new device (for example a tone generator or an LED panel) added by
  description only, and a game that uses it.

Real-system reference: the shape matches on-chip buses such as AMBA
(AXI/AHB/APB), TileLink and Wishbone, AXI-to-APB bridges, register maps
written as data (SystemRDL, IP-XACT) and device trees. Deliberately simpler
at first: one request at a time per master, in order (like AHB-Lite/APB),
with no IDs, separate read/write channels or long bursts; these can be added
later without changing the shape.

Decisions resolved by this goal: interrupts and traps — later (polling stays);
DMA — later, but the bus accepts several masters from the start; accelerator
— attaches later as another master/slave pair through the same port;
compatibility — today's map and ABI are the default description. Still open:
the description's format.

## 2026-10-28 — Component breakdown (draft, for owner review)

| # | Part | What it is | Reuses |
| --- | --- | --- | --- |
| 1 | Bus port contract | The one request/response port every master and slave uses, written down as a contract with a checker | Today's REQ/RSP protocol (`MemoryProtocol`) |
| 2 | Bus (interconnect) | N masters × M slaves: region-table address decoder, N-way round-robin arbiter, request routing and return path, error answer for unmapped addresses | The router's address match, `TwoPortMemoryArbiter` as the two-master slice, `BusErrorTarget` |
| 3 | Bridge | Slave on one bus, master on another; passes transactions on (optionally narrows the data width) | — |
| 4 | Memory subsystem | One configurable family: memory technology and size, caches per side, optional shared cache, behind one slave port | Chapter 3 endpoints, `Cache`, arbiter; replaces the hand wiring in the § 4.7 program root and the device system |
| 5 | Device shape and register front end | A register table (name, offset, access, reset value) generates the slave front end and the documented register table | `DeviceRegisterPort`, register decode |
| 6 | Devices | Console, timer, buttons, display and video memory, re-shaped to the device shape; plus one new example device | Today's device cores, FIFO, counter |
| 7 | CPU as a master | The RV32IM core's instruction and data ports as two masters; the fetch check becomes an "executable" attribute of a region | RV32IM core, instruction guard logic |
| 8 | System description and builder | The description (buses, attachments, addresses, memory) and the code that builds the system from it; today's games machine as the default description | — |
| 9 | Software side | The guest platform header (addresses, register offsets) generated from the same description; the games and the new device's game | Guest library, ten games |
| 10 | Showing it | Bus traffic in the simulator (which master talks to which slave, each transaction), the textbook mapping page | Visualizer, device views |
| 11 | Checks | Bus against a reference model with random multi-master traffic; device-shape contract tests; build-from-description tests; the games | Existing fixtures |

## 2026-10-28 — Complete plan (replaces the draft breakdown above)

### Design principles

- **One port.** Every connection is today's request/response port: `REQ_VALID/READY`,
  `REQ_OP[3]`, `REQ_ADDR[32]`, `REQ_WRITE_DATA[W]`, `REQ_WRITE_MASK[W/8]`,
  `RSP_VALID/READY`, `RSP_READ_DATA[W]`, `RSP_STATUS[3]`; operations Read,
  Write, Program, EraseBlock, ReadBurst; statuses OK, Alignment, Range,
  ReadOnly, Unsupported, Device. A transfer happens at a rising clock edge
  when valid and ready are both high. Nothing in the port changes.
- **Data, not wiring.** Address maps, device registers and the system itself
  are tables; recipes build from them.
- **Same timing for today's machine.** The default description must give the
  games the same frames, console output and cycle counts. So the bus lets
  different masters reach different slaves in the same cycle (a crossbar:
  one turn-taker per slave), and decoding passes through without an extra
  clock stage, as the current router does. Any stage that would cost a cycle
  (a registered bridge, say) is an option, off in the default description.
- **Lower-level first.** Every new part is built from gates and existing
  parts first; compact forms come after and are checked against them, with
  proven delay tables.
- **Device I/O only.** One clock; no interrupts or DMA yet, but nothing in the
  shape prevents them (several masters, optional interrupt pins later).

### Part 1 — Bus port contract and checker

- What: the port above, written once as a contract (`docs/reference/bus-port.md`
  and a public page), with the rules that are today spread over component
  pages: a request is held until accepted; one request in flight per port; a
  response is held until taken; a ReadBurst returns its words in order and
  keeps the path until the last one; status precedence; no side effect on an
  error or a zero-mask write.
- New part: `BusPortChecker` (`bus.port-checker.<W>`), a passive observer that
  sits on any link and records a violation (request changed while waiting, a
  second request in flight, response dropped). Used by tests and optionally by
  the simulator.
- Reuse: `MemoryProtocol` pin helpers stay the single definition of the pins.
- Checks: checker unit tests with deliberately broken traffic; every link in
  the default system runs clean under the games.

### Part 2 — The bus (interconnect)

- What: `Bus<W>` (`bus.interconnect.<W>`), N masters (1–8) × M slaves
  (1–16), configured by a region table: name, base, size (aligned power of
  two), slave index, attributes (read, write, execute; cacheable is a memory
  subsystem matter).
- Behavior: each master's request is decoded by address; an address in no
  region, or an operation the region does not allow, is answered by the bus
  itself (status Range, ReadOnly or Unsupported) without reaching a slave.
  Each slave has its own round-robin turn-taker; a master keeps its slave
  until its response (or the last burst word) is taken. The response goes
  back to the master that asked. The slave sees a local address (offset in
  its region), as today.
- Structure (structural first):
  - `BusDecoder` — per master: one region match per region (a masked compare
    of the high address bits), attribute checks, the selected slave as a
    one-hot word. Built from word gates.
  - `BusArbiter` — per slave: N-way round robin, built from the existing
    two-port arbiter logic generalized (the two-master case reproduces
    `TwoPortMemoryArbiter`'s turn order).
  - `BusRoute` — request multiplexing into each slave and response
    multiplexing back to each master (word muxes), plus the local-address
    subtraction.
  - `BusErrorAnswer` — the bus's own error response (from `BusErrorTarget`).
- Compact form: one leaf with the same turn order and decoding, checked
  against the structural bus with random multi-master traffic.
- Replaces: `AddressRouter<W>` (one master, six fixed regions) and the
  separate arbiter in the system; the instruction guard becomes the execute
  attribute (Part 7). Retired IDs `bus.word.address-router.*` and
  `bus.word.instruction-guard.*` are replaced completely (AGENTS.md).
- Checks: a reference model (plain C++ map lookup and per-slave queues) and
  random traffic from up to four masters to several slaves, both forms;
  exhaustive small cases (2 masters × 2 slaves) for turn order; bursts; every
  error status.

### Part 3 — Bridge

- What: `BusBridge<W>` (`bus.bridge.<W>`): a slave port on one bus and a master
  port on another. It forwards a transaction and its response unchanged, so
  buses chain into layers: a system bus (CPU, memory) and a peripheral bus
  (devices), as in the textbook's I/O bridge.
- Options: `registered` (one clock stage each way, as real bridges have;
  default off so today's timing holds); later a data-width change (32-bit
  system bus to a narrower peripheral bus).
- Checks: transactions through a bridge equal the same transactions without
  it (unregistered: same cycle; registered: one extra cycle each way).

### Part 4 — Memory subsystem

- What: `MemorySubsystem<W>` (`memory.subsystem.<W>`): one or two slave ports
  (instruction, data) in front of one memory. Description: technology (FF,
  SRAM, DRAM, ROM, NOR flash), size, an optional cache per port (all chapter
  3 cache options), an optional shared cache, and the arbiter between the
  ports.
- Structure: today's parts — `buildMemory` endpoints, `Cache<W>`, the arbiter
  — wired once, inside this part. The cache and arbiter get catalog families
  so profiles, manifests and the explorer see them.
- Replaces: the hand wiring in `RV32IMDeviceSystem` and in the § 4.7
  `RV32IProgramRoot`. § 4.7's arrangements become descriptions of this part.
- Checks: § 4.7 memory-arrangement tests and published cycle results
  unchanged; the cache tests; a sweep of technology × cache combinations.

### Part 5 — Device shape and register front end

- What: a device is a slave port, a register table, its outside pins and its
  own logic ("the device core"). A register entry has a name, offset, width,
  access and reset value. Access kinds, taken from today's devices:
  - read-only hardware value (COUNT, STATUS, LEVELS, HEAD);
  - read-only setting (WIDTH, HEIGHT, DIVISOR);
  - read-write register;
  - write-only data (console TX: pushes a byte);
  - command (POP, CLEAR_OVERFLOW, PRESENT: acts when bit 0 is written as 1).
- New part: `RegisterFrontEnd` generated from the table: the shared
  `DeviceRegisterPort` handshake, one offset decoder (`Decoder` on the low
  offset bits plus one "inside the window" check, instead of today's full
  32-bit compare per register), access checks with the right status, read
  selection (word mux), and one strobe per writable or command register for
  the device core.
- The same table generates the device page's register table and the guest
  header's offsets (Part 9).
- Checks: a generic contract test runs every device's table: each register's
  read, write, wrong-access status and command strobe, both forms.

### Part 6 — Devices

- Console, timer, buttons, display (control registers and video memory):
  today's device cores (FIFO, counter, button levels and events, frame
  control, pixel memory) kept, their register decoding replaced by Part 5.
  Register offsets and behavior unchanged.
- The buttons' 32 private comparators become one `Decoder<5>`; adapter
  round trips go away (the `Logic` minimal mode).
- New example device, added only through the description: an **LED panel**
  (16 LEDs as one read-write register plus a blink-rate register), drawn as a
  row of lights in the simulator. A tone generator is the alternative (no
  sound output exists in the simulator).
- Checks: each device's functional tests and the generic register test;
  structural/compact equivalence; delay tables.

### Part 7 — CPU as a master

- The RV32IM core's instruction and data ports become two masters on the
  system bus. The fetch guard becomes the execute attribute: only regions
  marked executable answer fetches; others answer with today's fault status,
  with no side effect.
- Checks: the RV32IM program tests and lockstep checks unchanged; a fetch from
  device space still faults.

### Part 8 — System description and builder

- `SystemDescription`: clock; buses (width, which masters); attachments
  (memory subsystem, devices, bridges) with name, contract, base, size,
  attributes and parameters; outside pins to forward. Validated: regions
  aligned and non-overlapping, sizes consistent, every name unique.
- `System` (`system.described`): builds everything from the description;
  RAM size, memory window, fetch window and program-load limit all come from
  it, so they cannot disagree.
- Default description `games-machine`: today's map and guest ABI, one system
  bus (CPU I/D masters; memory subsystem; bridge), one peripheral bus
  (console, timer, buttons, display, video memory). `RV32IMDeviceSystem`
  becomes this description.
- Format: a C++ structure with a text form (JSON) that the simulator, the
  site and the guest-header generator read (owner decision, below).
- Checks: build several descriptions (with and without caches, DRAM memory,
  an extra device, a second bus); invalid descriptions rejected with clear
  reasons.

### Part 9 — Software side

- The guest platform header (`platform.h`: addresses, register offsets, bits)
  generated from the description, so hardware and software cannot disagree.
- The ten games rebuilt against the generated header (same values, so the same
  binaries for the default description).
- A new game (or a game variant) that uses the LED panel.
- Checks: the generated header equals today's for the default description;
  the ten scripted fixtures give the same results; the LED game's frames and
  LED states are checked.

### Part 10 — Showing it

- A bus view in the simulator: for each bus, which master is talking to which
  slave in each cycle, with the transaction (operation, address, data,
  status) on hover; a per-slave traffic count over a time range.
- The system drawn in layers: CPU, system bus, memory subsystem, bridge,
  peripheral bus, devices.
- A public chapter page mapping CS:APP Figures 1.4, 6.6 and 6.11 to the
  components, and pages for each new part (component template).
- Checks: browser checks for the bus view on a game recording.

### Part 11 — Checks and migration

- Order of evidence: part tests → system builds → games → recordings.
- Delay tables regenerated for every new part; the safe-clock certificate of
  the games machine recomputed.
- Retired IDs (`bus.word.address-router.*`, `bus.word.instruction-guard.*`,
  `rv32im.system.devices`) replaced completely: callers, scenarios, saved
  layouts and profiles migrated; no aliases.
- Recordings re-captured once at the end (owner decision 2026-10-27).

### Order of work

| Step | Parts | Done when |
| --- | --- | --- |
| 1 | Port contract and checker | Checker catches each broken-traffic case |
| 2 | Bus (structural, then compact) | Reference-model random traffic passes in both forms |
| 3 | Bridge | Bridged traffic equals direct traffic |
| 4 | Memory subsystem | § 4.7 tests and results unchanged |
| 5 | Register front end and device shape | Generic register test passes for all four devices |
| 6 | CPU as master, system description, builder | Default description builds; RV32IM program tests pass |
| 7 | Games on the new system | Ten fixtures: same frames, console output and cycle counts |
| 8 | LED panel and its game | Added by description only; its game checked |
| 9 | Bus view and pages | Browser checks; strict site build |
| 10 | Migration and recordings | Old IDs gone; recordings re-captured; full suite |

### Risks

- **Cycle counts.** A decoder or bridge stage that adds a cycle changes the
  games' timing; the default description keeps everything pass-through, and
  step 7 checks cycle counts exactly.
- **Bursts through the bus.** A cache fill keeps its slave for several
  answers; the turn-taker must not hand the slave over mid-burst.
- **Size and speed.** A general crossbar is larger than today's fixed router;
  compact forms keep game runs fast, and step 7 measures run time.
- **Migration size.** Paths of the system change (new layers), so layouts and
  recordings move once.

### Owner decisions for this plan

1. Description format: C++ structure with a JSON text form (recommended), or
   catalog parameters only.
2. Example device: LED panel (recommended) or tone generator.
3. Layering of the default system: a system bus and a peripheral bus joined by
   a bridge (recommended, textbook shape), or one bus for everything.
4. Cycle counts: must the games' cycle counts stay exactly the same
   (recommended), or may a realistic registered bridge add cycles?

## 2026-10-29 — Owner decisions and implementation started

Decisions (2 and 3 by the owner; 1 and 4 the recommendations the owner's
implementation goal adopts):

1. **Description format:** a JSON system description read into a C++
   structure; the simulator, the site and the guest-header generator read the
   same file.
2. **Example device:** the LED panel.
3. **Buses:** start with one simple bus, then build the two-bus layering of the
   textbook (system bus, bridge, peripheral bus), and compare them.
4. **Timing:** the bridge's relay stage is an option. Three machines are
   compared: one bus (must equal today exactly: same results and cycle
   counts), two buses with a pass-through bridge (same results and cycles),
   and two buses with a registered bridge (same results, measured extra
   cycles and certified clock).

Owner's implementation goal (2026-10-29): implement the plan; reuse existing
components, retire old Device I/O components that are not used; retire the
RV32IM and Device I/O site documentation and write one new chapter following
the other chapters' patterns, templates, examples and diagrams; then run
experiments and gameplay recordings at one realistic fidelity. Minimal tests,
one heavy job at a time.

Implementation approach for the bus, chosen for exact timing: the bus is
built per master and per slave from the two existing proven pieces, so the
default machine keeps today's handshake timing:

- per master, a request router generalized from `AddressRouter` (its fixed six
  regions become the region table; the instruction guard is the same router
  with execute-only regions);
- per slave with several masters, a turn-taker: the existing
  `TwoPortMemoryArbiter` for two, a tree of them for more; a slave with one
  master is wired straight through.

## 2026-10-29 — Implementation, first pass

What was written (all under `include/system`, `src/system` unless noted):

| Part | Contract | Built from |
| --- | --- | --- |
| JSON reader, `SystemDescription` | — | `Json.cpp`, `SystemDescription.cpp`; built-in descriptions in `systems/*.json` embedded at build time |
| Master router | `bus.router.<W>` (access, routes) | the old address router's gates (`buildAddressRouter`), now per master with its own region list; compact form = its own gates compiled to a gate list |
| Bus | `bus.interconnect.<W>` (config = bus JSON) | one router per master; a `memory.arbiter.<W>` per target port shared by two masters; straight wires otherwise |
| Bridge | `bus.bridge.<W>` (registered 0/1) | pass-through wires, or three state bits plus request/response registers |
| Memory subsystem | `memory.subsystem.32` (technology, words, caches) | the existing arbiter, `rv32i::buildMemory` and optional `Cache<32>` pair, wired once |
| Computer system | `system.computer` (machine or description) | parts and buses from the description; every `CLK`/`RST`/`*_CLK`/`*_RST` input on the system clock; an undriven part input is a description error |
| Register front end | private group per device | a register table: `Decoder<n>` on the word index plus an in-window check, access checks with the old status order, a word-mux read chain, a strobe per writable register, storage for read-write registers |
| LED panel | `device.leds` | front end (LEDS, BLINK) + `ClockedCounter<32>` + a 32:1 mux for the blink phase |
| Guest header | `system_header` tool | regions and every device's register table; `tools/rv32im/guest/system.h` and `system-leds.h` are generated and checked by `GuestHeaderTest` |

Decisions taken while implementing:

- **The bus keeps the routers' registered request stage.** Each master's
  router holds a request for one cycle, as the old router did. A second bus
  behind a bridge therefore adds that stage again, even with a pass-through
  bridge: the two-bus machine cannot equal the one-bus cycle counts. This
  replaces the plan's expectation (decision 4) that a pass-through bridge
  keeps today's cycles; the comparison measures the difference instead.
- **Fetch masters** reach only regions marked `execute`; the fetch router is
  the old instruction guard with its window taken from the description.
- **A target window that passes whole addresses** (`"local": false`) lets
  the peripheral bus keep the devices' absolute addresses, so the guest
  header and binaries are the same for the one- and two-bus machines.
- **Register tables** keep every device's offsets and status rules
  unchanged: Range (2) when no register matches, ReadOnly (3) for a write to
  a read-only register, Unsupported (4) for a read of a write-only register,
  Device (6) for a payload that does not fit. The buttons' 32 comparators
  became one `Decoder<5>` and three word gates.
- **The games machine options** (caches, DRAM, display size, timer, queue
  depths) patch a description (`withGameOptions`) instead of a separate
  configuration type.

Baseline for the cycle comparison (old `RV32IMDeviceSystem`, build of
2026-10-28, unit timing): the tiny bring-up ELF of `RV32IMDeviceSystemTest`
takes 126 cycles (FF RAM), 206 (cached FF), 393 (cached DRAM).

## 2026-10-29 — First verification, retirement, documentation

Checks run (native `build-milestone`, unit timing, one job at a time):

| Check | Result |
| --- | --- |
| `ComputerSystemTest`, `…CachedFFTest`, `…CachedDRAMTest` | 126, 206, 393 cycles: **identical** to the old `RV32IMDeviceSystem` (126, 206, 393) |
| `ComputerSystemTwoBusTest`, `…RegisteredBridgeTest` | 129 and 135 cycles for the same 24 instructions |
| `ComputerSystemFaultTest`, `ComputerSystemLockstepTest` | pass |
| `SystemDescriptionTest`, `GuestHeaderContentTest`, `BusInterconnectTest`, `LedPanelTest` | pass (the bus test compares gate-level and compiled routers only; the arbiter's compact model is defined for protocol-legal traffic, which random inputs break) |
| `BusRouterScenarioTest`, `BusBridgeScenarioTest`, `MemorySubsystemScenarioTest`, `LedPanelScenarioTest`, `ComputerSystemScenarioTest` | pass, both fidelities where available |
| `ConsoleDeviceTest`, `TimerDeviceTest`, `ButtonDeviceTest`, `FramebufferDisplayTest` and their scenarios | pass on the register-table front end |
| Guest rebuild with the generated header | all ten existing ELFs byte-identical; `snake-leds` new |

Corrections found while testing: the front end first used `Mux<2,3>`
(not a supported mux shape) for statuses; it now uses three-bit gate
selections. Compiled routers and bridges first built their reference circuit
without a profile (raw flip-flops are not compilable state); they now build
it all-structural with characterized storage, like the other compiled parts.

Retired (no aliases): `rv32im.system.devices` (`RV32IMDeviceSystem`),
`bus.word.address-router.<W>`, `bus.word.instruction-guard.<W>`,
`bus.word.error-target.<W>`, `video.pixel-selector.<W>`, the fixed
`AddressMap`. Their tests, scenarios, menu entries, palette entries and the
device-system floorplan were replaced by the new parts'. The old public
chapters `site-docs/rv32im/` and `site-docs/device-io/` were removed; the
new chapter is `site-docs/system/` (§ 5). Old recordings of the fixed machine
(tracked ones removed; about 2 GB of untracked evidence moved to
`build/retired-site-assets/rv32im-2026-10-29/`) are replaced by recordings of
the new machines. The maintained reference is now
`docs/reference/computer-system.md`; the old one is archived.

Added for the plan's Part 10: a `bus` device view (each master's request
and answer, each target's offered request, at the selected time) and an
`leds` view (a row of 16 lights) in the inspector and canvas.

## 2026-10-29 — Port checker (plan Part 1) and further checks

`BusPortChecker<W>` (`bus.port-checker.<W>`) is a passive structural
observer: per field a register and an XOR word compare the link with its
value at the last edge; sticky flags for a changed or withdrawn waiting
request, a changed waiting answer, and an answer with no request outstanding
(bursts keep a request outstanding). A description with `"check_ports":
true` gets a checker on every bus link and an output `PORT_VIOLATION`; the
game runner fails a run when it rises. `systems/games-machine-checked.json`
is the two-bus machine with checkers on its eleven links.

| Check | Result |
| --- | --- |
| `BusPortCheckerTest` | clean transfer clean; four broken-traffic cases each flagged |
| `ComputerSystemPortCheckTest` | bring-up program clean on all eleven links (129 cycles, as unchecked) |
| `BuiltinComponentCatalogInventoryTest` | passes after two fixes to the earlier generic compiled forms (their contract tests and type names now follow the gate-level form) and the 2–32 word-gate widths in its list |
| `ComputerSystemTimingTest` (unit) | every machine certified: 92 (FF), 84 (cached), 90 (two-bus, registered bridge), 94 (LED) ticks |
| `ComputerSystemTimingTest` (embedded) | 4464 ps (FF, two-bus, registered bridge), 4470 ps (LED), 4254 ps (cached FF); the bridge's `M_CLK`/`M_RST` are now held low, since a clock passed on as data could not be timed |
| Games vs the old machine (unit, compact) | bring-up 38,811 cycles and pong 88,367: identical cycles, instructions, frames, console, report, device reads and inputs; 27% and 26% fewer events |

Chosen realistic fidelity for the experiments and recordings: the
`embedded` timing profile, compact parts on their proven delays, a 4500 ps
clock (222 MHz), which every flip-flop-memory machine is certified for.

## 2026-10-29 — Experiments and recordings

Campaign: `tools/system/run-experiments.py`, one run at a time, every run
checked by the recorded runner (instruction model, memory and device models,
frames, inputs, setup violations). Summaries are tracked in
`site-docs/assets/system/results/`; the tables in § 5.6 and § 5.7 are
generated from them by `tools/system/results-tables.py`.

**Same machine as before** (unit timing, compact parts, 1000-tick clock):

| Game | Memory | Old `RV32IMDeviceSystem` | New `games-machine` | Results | Events old → new |
| --- | --- | ---: | ---: | --- | --- |
| Bring-up | FF | 38,811 | 38,811 | identical | 15,181,178 → 11,000,184 |
| Pong | FF | 88,367 | 88,367 | identical | 33,391,339 → 24,655,432 |
| Snake | cached FF | 260,970 | 260,970 | identical | 443,666,985 → 207,853,755 |

**One bus or two** (cycles; frames and console identical, reports differ
only in timer samples):

| Game | One bus | Two buses | Registered bridge |
| --- | ---: | ---: | ---: |
| Bring-up | 38,811 | 38,851 (+40) | 38,931 (+120) |
| 2048 | 169,601 | 169,679 (+78) | 169,835 (+234) |
| Sokoban | 188,614 | 188,692 (+78) | 188,848 (+234) |
| Snake | 141,132 | 141,213 (+81) | 141,375 (+243) |
| Pong | 88,367 | 88,445 (+78) | 88,601 (+234) |
| Tetris | 191,818 | 191,896 (+78) | 192,052 (+234) |

Finding: the extra cycles equal the number of device accesses exactly
(device reads + console bytes + PRESENTs + POPs: 40, 78, 81). Each device
access pays one edge for the second router behind a pass-through bridge,
three behind a registered bridge (second router + two register stages).
Memory traffic does not pay. This confirms the earlier correction: a
pass-through bridge cannot keep the one-bus cycle counts, because each
router registers its request.

**Recordings at the realistic fidelity** (embedded timing, compact parts,
4500 ps clock, `games-machine`; LED game on `games-machine-leds`): all 11
fixtures pass every check and take exactly the cycles of the old unit-timing
recordings (bring-up 38,811; 2048 169,601; Sokoban 188,614; Snake 141,132;
Pong 88,367; Tetris 191,818; 2048 merge 135,080; Tetris line clear 142,549;
Pong rally 115,686; Snake wall 115,575; Snake with LEDs 128,806). Host time
143–1,117 s per game. Packaged with `tools/package-recorded-circuit.mjs` into
`site-docs/assets/system/recordings/` (about 1.5 MB each with layout).

Other checks and fixes this round:

- Example rows: all 174 groups regenerated in WebAssembly and verified;
  strict MkDocs build passes. Two Basic-chapter pages from the earlier work
  were fixed (the 8-bit joiner's partial row now drives its other inputs as
  unknown; the clock page had a table marker for a test that is not a
  recorded component scenario).
- `RecordedCircuitBrowserTest` passes on the new Pong-rally recording;
  `RecordedCircuitTransportTest` passes. The recorded viewer now shows a
  recording's own timing profile (it hard-coded `unit`) and answers any
  requested profile with the recorded one.
- `VisualizerColorPaletteTest` (failing before this work) passes after
  prefix colors for the layout families that had none;
  `DeviceParametersBindingsTest` (also failing before) passes after two stale
  expectations (a compact 4×4 display and a compact 16-bit ALU are valid) and
  real window checks for router routes (a malformed route was accepted).
- `VisualizerScenarioCatalogTest`, `VisualizerFidelityProfileTest` and
  `GuestHeaderTest` pass.

Not run: the full native suite; the WebAssembly browser suites other than
the recording check (`RV32IMDeviceBrowserTest`, `DeviceSystemFidelityBrowserTest`
were updated for the new scenarios but not run).

## 2026-10-29 — Plan status

| Part | Status | Differences from the plan |
| --- | --- | --- |
| 1 Port contract and checker | Done: § 5.2 page; `BusPortChecker<W>`, `check_ports` | The contract is documented in § 5.2 and `docs/reference/computer-system.md`, not a separate `bus-port.md` |
| 2 Bus | Done: `Bus<W>` from per-master `BusRouter<W>` and two-port arbiters | At most two masters per target port (the arbiter has two sides); no N-way round robin yet. No whole-bus compact form: its routers and arbiters have theirs |
| 3 Bridge | Done: pass-through and registered | A ReadBurst does not cross the registered bridge (none is sent through one) |
| 4 Memory subsystem | Done: FF, SRAM, DRAM; optional cache pair | § 4.7's `RV32IProgramRoot` keeps its own wiring (its five arrangements include one with two separate memories, which is not this part) |
| 5 Register front end | Done for all devices | — |
| 6 Devices, LED panel | Done | No tone generator |
| 7 CPU as master | Done (fetch masters reach only `execute` regions) | — |
| 8 Description and builder | Done: schema 1, five built-in machines, games options | — |
| 9 Software | Done: generated headers, identical binaries, `snake-leds` | — |
| 10 Showing it | Done: `bus` and `leds` inspector views, machine/bus/memory floorplans, § 5 with five figures | The bus view shows the transaction at the selected time; no per-target traffic count over a range |
| 11 Checks and migration | Done: retired IDs gone, layouts migrated, recordings re-captured, clocks certified | New parts get delays proven at build time from their gates (compiled forms), not stored tables; the full native suite is not run |

## 2026-10-29 — Layout migration

The full static layout check over all 374 scenarios first found 19,792
missing placements in 244 scenarios: mostly from the earlier word-grouping
work (RV32I result select, forwarding, hazard, prediction tables, branch
recovery, row decoders, wide word gates), plus the bridge's renumbered gates,
the bus and router floorplans and the front ends' decoders. Fixed by
`recalculate_layouts.py --fill-missing` for those scenarios, a re-flow of the
types whose children changed, a corrected `RV32IRequestTagMatch` floorplan
(one word XOR instead of 32 single ones), wrapped leftover rows in the
machine/bus/memory floorplans, and one tool fix (re-flowing a scenario's root
type created no root entry). Result: 0 problems in 374 scenarios; the 15
layout CTest checks pass, including `RV32IMemoryLayoutTest` and
`RV32IBuildingBlockLayoutTest`, which failed before the re-flow.

## 2026-10-29 — Documentation completeness review and acceptance boundary

The [chapter completeness review](2026-10-29-computer-system-documentation-review.md)
adds the source-supported component detail missing from the first chapter
draft and corrects several overly broad claims. It does not complete the
architecture acceptance gates. In addition to the plan-status differences
above, the source review identifies these remaining boundaries:

- Bus routers reject bursts; the arbiter's standalone burst ability does
  not establish burst routing through the bus. The random bus check is
  router equivalence, not an independent legal-traffic/fairness campaign.
- The port checker does not detect every one-outstanding-request violation
  or count burst words. Its clean flag establishes its three checks only.
- Register tables build hardware/header data, but the site's Markdown maps
  are maintained by hand. The planned generic register access test is not
  present; nonzero RW reset metadata is not applied by the current builder.
- The memory subsystem still creates its internal cache/arbiter by class
  and covers the shared-memory/optional cache-pair cases. The planned broad
  technology/cache sweep and migration of § 4.7 are not completed.
- The clock panel does not include the checked-link machine or all accepted
  parameters. Arbitrary X/Z controls, every width, full native/WASM browser
  and broader game acceptance need their own tests before stronger claims.

Existing default timing tables suffice for the newly published selected
rows. Router/bridge/register-group numeric tables require an export and
verification of the concrete parameter set, because they are derived when
built. The standalone RV32IM timing test was rerun successfully in all three
profiles; no hardware, delay tables or game recordings changed in the review.

## 2026-10-29 — Whole-catalog compact availability correction

The [follow-up catalog check](2026-10-29-computer-system-compact-availability.md)
corrects one conclusion in the earlier documentation review: the shared
generic gate-list registry adds explicit compact forms for multiply/divide
steps, M decode control and the M coordinator. Their absence from a module's
own factory list does not mean they are absent from the final catalog.
Decode/execute stage roots, standalone multiplier/divider roots, the whole
bus and memory subsystem remain structural assemblies. The generic compact
test passes 36 configurations and 4,032 binary/four-state vectors. Numeric
step/decode/coordinator delay tables are derived when built and need a
configuration-specific export before publication; no stored table was added.

## 2026-10-29 — Console structural adoption audit

Owner questions: does `CONSOLE_DEVICE_ROOT.REGISTERS` use the new multi-width
basic gates; can it; is `TX_BYTE` merely a Rewire; did this workflow review
the devices themselves?

Current source and the current packaged WASM agree:

| Component | Current structure | Assessment |
| --- | --- | --- |
| `REGISTERS` | 166 direct children: 26 Rewires, one `Decoder<2>`, 75 OR, 41 AND, nine NOT, one known-fields checker, 11 constants, two `Mux<2,32>`; 378 total descendants, no word-gate component | Partial adoption: word read-data muxes and a shared decoder; scalar three-bit status selections remain |
| `TX_BYTE` | `ConsoleTxByte` group with `SPLIT_0` and `JOIN_1`, both Rewire; no gates or state | Semantically one zero-delay `WRITE_DATA[7:0]` slice; a single Rewire can implement it |

`RegisterFrontEnd.cpp::statusChoice` calls `Logic::mux` at width 3. Minimal
`Logic::mux` groups only widths 4/8/16/32; width 3 falls through to one
shared select inverter and AND/AND/OR per bit. This is not a basic-gate width
limitation: `word::gateWidth` already supports every width 2–32. Six console
status choices could use two `ANDGate<3>` and one `ORGate<3>` each, with select
and inverse-select broadcasts, preserving the scalar gate rule and depth.
`Mux<2,3>` is unavailable today, but it is not required for that regrouping.
Address, reserved-payload and mask reductions are reductions across bits;
they should not be replaced indiscriminately with bitwise word gates.

`TX_BYTE` is constructed by `DeviceState.cpp:194-198` with the Minimal
helper, yet splitting the 32-bit word and joining only its low eight bits
still creates two adapters. One Rewire with a 32-bit input, an 8-bit output
and eight bit mappings performs the exact four-state operation. Removing the
wrapper or its children changes topology/type layouts and capture paths;
those migrations and verification belong with implementation, not this audit.

Scope of the preceding workflow:

- Devices were reviewed on 2026-10-27; redundant adapters and scalar recipes
  were identified. The word-grouping record explicitly deferred Device I/O
  to P1. The architecture review says to do the adapter fix before or with
  P1, rather than preserve the deferral indefinitely.
- The new implementation did touch the devices: shared register tables/front
  ends replaced console/timer/button/display decoding, and button level update
  became a decoder plus word gates. Existing device/scenario checks and game
  comparisons are recorded in the earlier sections. The later chapter review
  checked source/interfaces and acceptance gaps; it was not another complete
  circuit-simplification pass.
- `DeviceLogic.hpp` still selects `Adapters::Legacy`, with a comment promising
  migration at the now-implemented redesign. `REGISTERS` itself uses Minimal,
  so that Legacy setting alone does not explain its remaining scalar muxes.
  Device-wide cleanup was only partially completed. It remains a gap against
  G7's lean-structure goal; it is not established as an intentional permanent
  design choice or a correctness defect.

Verification: build-only `WasmVisualizer::previewComponentsJson` for
`ConsoleDeviceScenarioTest` with the Gate preset, from the exact
`build-wasm-milestone/pages/simulator` package selected by port 8893. The
5,466-component preview was produced under a 900 MiB/no-swap scope, with no
simulation. Evidence: `build-milestone/console-structure-audit-20261029/`.
The first native probe attempt was rejected as stale: the preserved
`build-milestone/tests/VisualizerSessionProbe` binary predates the redesign
and produces neither REGISTERS nor TX_BYTE. Its output is labeled
`stale-native-preview.json` and is not current evidence. No source, scenario,
layout, delay table or recording changed; no functional/timing test was run.

## 2026-11-01 — Remaining plan items, adapter chains, scenario review

Code state: `alpha` at `5549ee0` plus the uncommitted working tree.

**More than two masters per target port.** `Bus` no longer rejects a third
master. A port reached by k ≥ 3 masters gets a tree of `memory.arbiter.W`
named `<PORT>_ARBITER_<n>`: masters are paired in description order, an
unpaired one moves up a level, the last arbiter drives the port. Each
arbiter alternates between its two inputs, so shares are unequal (three
masters all waiting: the third gets 1/2, the first two 1/4 each); the site
says so. The arbiter's unread `D_CLK` is no longer wired.

**Whole-bus compact form.** `bus.interconnect.W` has an on-request compact
implementation (`SystemPart::behavioral_on_request` → `gate_list`): the bus's
gates compiled together with characterized storage, chosen only by an exact
path rule or a parent requirement. Its clock check first failed with "clock
used as data" because a zero-delay Rewire whose outputs drive nothing
counted as a clock sink; `TimingGraph` now exempts such dead pass-throughs
and names the sink in the message. `BusInterconnectTest` (a fetch master and
three masters sharing the data port, two arbiters) compares the gate-level
bus, compiled routers and the whole bus compiled: 400 random cycles agree.

**Split→join adapter chains (owner question).** Not intended structure: an
artifact of the bit-vector helper, which split every word on input and
joined on output, so one recipe's join fed the next recipe's split. Fixed at
the source in `StructuralNetBuilder`: `splitWord` returns a known join's bits
instead of splitting again; `joinBits` returns the original word when the
bits are that word in order, and otherwise builds one Rewire from whole
source words and loose bits; `finish()` prunes adapters whose outputs drive
nothing (`Component::removeChild`, `ComponentBuilder::removeComponent`).
The Legacy adapter mode is retired (`Logic::Adapters` has only Minimal;
devices use it). `RV32IDecodeControlField` maps single recognition lines
straight to its output word. Census over every visualizer scenario
(adapter whose sinks are all adapters): 68 chains in 20 parent types before,
**0 in 380 scenarios after**. `part_delay_tables` rewrite and comparison:
**no stored delay changed**; the file lost two tables for retired contracts
(`bus.word.address-router.32`, `bus.word.instruction-guard.32`) and gained
the three `bus.bridge.W` tables.

**Menu.** The 18 § 5 entries are in group `system` with a fixed order:
bus parts, memory subsystem, register port, state parts, devices, then the
five machines. `VisualizerScenarioCatalogTest` follows that order.

**Scenario checkpoint review.** Before: most `reset`/`ready` rows and the
button setup rows checked nothing; register port, console, router, bridge
and memory-subsystem rows left strobes, addresses, write fields, op codes
and `REQ_READY` unchecked; § 5.1 rows checked 3–10 of 19–35 outputs.
Changes:

- Both scenario helpers apply a port rule (`include/tests/PortRule.hpp`):
  in `reset` every handshake output is 0; in `ready` no responder has an
  answer pending. Responder readiness is deliberately not a generic rule:
  the memory arbiter raises `D_REQ_READY` only while D asks, which the
  handshake allows.
- Targeted rows: register port strobe/field/response timing, backend wait,
  empty and partial masks, misalignment, unknown op; console room/count,
  full queue, oldest-first, read-only status; timer count against clock
  edges and undeclared frequency; buttons held level, event count/status,
  both pops; display height/format/frame count; FIFO full/overflow/clear;
  counter wrap; router op and mask; bridge write fields and an error status
  passed back; memory subsystem simultaneous I/D requests (fetch first after
  reset, data held until granted, both answered); LED blink read-back;
  decode-control complete M control word; execute packet fields at the M
  result; coordinator every movement control during the arithmetic hold.
- Three new expectations were wrong at first and were corrected after
  reading the circuit, not by copying values: the register port's answer is
  already valid in the strobe cycle (the response buffer falls through when
  empty); `D_REQ_READY` is 0 while D is idle; the first memory sequence
  dropped D's request before it was taken (a protocol violation by the
  test). § 5.3 and § 5.4.1 now state the first two.
- The five machine scenarios still only show reset, release and a queued
  button; § 5.5 says what their rows check and that they run no program.

Verification so far: all 13 § 5 component scenarios pass; `BusInterconnectTest`
passes; census and delay-table comparison as above. Pending: § 5.1 scenario
runs, layout refill for changed and new types, static layout and palette
checks, screenshots, WebAssembly rebuild, example rows, strict site build,
game re-recording, § 4.7 migration, full native and browser suites.

## 2026-11-01 — Visual review of the § 5 scenarios

All 18 § 5 scenarios were opened in the native visualizer (isolated server on
scratch copies of `layout.json`/`profiles.json`, `build-milestone` module) at
the scenario default, Visual and Compact presets: 54 views, all `ready`, no
page errors. A palette census over every part of those scenarios found one
type on the default colour, `BusPortChecker<32>`. Screenshots were reviewed by
eye. Findings and fixes:

| Finding | Fix |
| --- | --- |
| `BusPortChecker<W>` drew 160 loose parts (field registers, XOR words, OR reductions) under its root | Regrouped into `REQUEST_WATCH`, `RESPONSE_WATCH` (`BusPortCheckerWatch<…>`), `HANDSHAKE` and `FLAGS`; same gates; root now has 4 children. `BusPortCheckerTest`, its scenario, the checked-machine scenario and `ComputerSystemPortCheckTest` pass |
| `BusPortChecker<32>` and its new groups uncoloured | Palette prefixes for the checker, its watches, handshake and flags |
| LED panel in Visual built fully structural (1,774 parts) | `device.leds` added to the Visual preset's picture kinds; now 8 parts with the LED picture; `site-docs/visualizer.md` lists it |
| Machine floorplan put bridges, 11 port checkers and the checkers' OR gates in the device column, so the checked machine's devices were slivers | Middle column holds buses with each bridge between them; checkers in one row along the bottom; loose gates in the leftover rows |
| Bus floorplan matched only `*_ARBITER`, so tree arbiters fell into the leftover row; routers got 40 % of the height even with no arbiter | Arbiters matched by `_ARBITER`; routers take the full height when no port is shared |

Observed but left as is: at time 0 every picture shows X until reset settles;
the memory arbiter and the port checker have no compact form, so they stay
gate-level in Compact (documented); the compact core has no picture; the
native server's memory grows across machine loads (a fresh server per two
machines stayed under a 1.2 GB cap).

Incident: the first screenshot attempt ran the server (500 MB cap) and
Chrome (650 MB cap) together with about 840 MB free; the host ran out of
memory and rebooted. Each cap was below free memory, their sum was not.
Repository files were intact afterwards; scratch output in `/tmp` was lost
and recreated. Later runs kept the sum of caps well below free memory.

Layout state after the review: refilled the bus, checker and machine
scenarios (reflowing router internals, `BusPortChecker<32>`, `ComputerSystem`
and `Bus<32>`); `tools/check-static-layouts.py`: 0 problems in 380 scenarios
before the final machine reflow and 0 in the six reflowed scenarios after.

## 2026-11-01 — § 4.7 arrangements on the memory subsystem (plan Part 4)

The last open plan item. `MemorySubsystemConfig` now carries the full form:
any `rv32i::MemoryConfiguration`, an optional cache per port and an
optional shared cache (any `CacheConfig`), the arbiter's burst length and
optional invalidation inputs `INV_L1`/`INV_L2`; accessors expose the built
endpoint, arbiter and caches. The catalog parameters (technology, words,
caches=none|default) map onto it unchanged, except that the default caches'
tie-off constants are now named `<CACHE>_NO_INVALIDATION`.

- `RV32IProgramRoot` builds arrangements B, C-ID, D-0 and D-ID as one
  `MEMORY` subsystem (core `IMEM_`/`DMEM_` to its `I_`/`D_`); arrangement A
  and separate-memory caches keep two endpoints, as before. Program loading
  uses the subsystem's `RAM`.
- `RV32IExperiment` (§ 4.8) builds its shared arrangements the same way, with
  `INV_L1`/`INV_L2` from `FLUSH_L1`/`FLUSH_L2`, the L2 line as the arbiter's
  burst length and the same burst rules. Request boundaries keep their
  published labels (`ARBITER`, `IL1`, `DL1`, `L2`); the memory's compact
  rule moves from `EXPERIMENT.MEM` to `EXPERIMENT.MEMORY.RAM`.
- The memory inspector recognises the subsystem's `RAM` as the shared
  memory (formerly by the name `SHARED_MEMORY`); this also gives the § 5
  machines' memory the fetch/data panels.
- Floorplans: the program root with `CLOCK`, `CORE`, `MEMORY`; the subsystem
  stacks caches, arbiter, shared cache and memory.
- Docs: § 4.7 names the subsystem and its parts; § 5.3 describes the full
  form, the shared cache and the invalidation inputs.

Verification plan: the arrangement scenarios and inspector checks; exact
cycles of published § 4.8 runs (`calls`, embedded: A 62,529, B 94,309, C-ID
write-through 26,321, D-0 write-back 23,037, D-ID write buffer 22,013).

Verification (2026-11-01):

- First run: the four shared arrangements failed (`x1` unknown; the memory
  answered Invalid). Cause: the program root wired the clock and reset to
  `MEMORY` in one wiring pass and to the core in `wireMemorySystem`'s
  separate pass; a source pin carries one wire, so `MEMORY.CLK` was cut off.
  The unified arrangements now wire core and subsystem in one pass (the
  experiment builder already did).
- `RV32IMemoryArrangementBTest`, `RV32ISharedMemoryProgramTest` (C-ID),
  `…D0Test`, `…DIDTest`: pass (oracle per retired instruction, gate-level
  core against the all-compact answer sheet). `MemorySubsystemScenarioTest`,
  `ComputerSystemCachedFFTest`, `ComputerSystemScenarioTest` pass.
- § 4.8 published cycles reproduced exactly with the campaign command
  (`calls`, embedded, 4,104 ps, DRAM): A 62,529; B 94,309; C-ID write-through
  26,321; D-0 write-back 23,037; D-ID write buffer 22,013. Other programs,
  behaviors and `desktop` were not rerun.
- Layout: subsystems with different parts shared the key `MemorySubsystem`
  (the static check flagged it); the part is now a `ComponentLayoutView` with
  variant `caches-none|i|d|s…`. The § 4.7 arrangements, the subsystem
  scenario and the five machines were refilled; static check 0 problems in
  those 11 scenarios. `MemoryInspectorIntegrationTest`,
  `VisualizerSessionContractTest`, `VisualizerAutomaticLayoutTest` pass.

With this, every part of the plan is implemented (the 2026-10-29 plan-status
differences for Parts 2 and 4 are resolved).

## 2026-11-01 — Delay tables in all profiles; full-suite attempt

- The first full native suite stopped at build: `MemoryFeedbackTimingTest`
  reaches the private `MemoryHandshakeWiring.hpp`, whose include of
  `modules/composite/WordSliceBuilder.hpp` the test target cannot resolve
  (from the earlier word-gate work). The include is now relative.
- The embedded and desktop table checks failed for the same reason as the
  unit table earlier: they still held the two retired contracts and lacked
  the three `bus.bridge.W` tables. Rewritten with `part_delay_tables
  --profile=… --write --clocked` and compared entry by entry: **no stored
  delay changed** in either profile (1,038 → 1,039 tables).
- Mistake: the script of the first attempt did not stop after the build
  failure and started CTest anyway; a second, intended run then ran beside it
  for about two hours, both writing one log. Both were stopped and their
  results discarded; a clean run follows with its own log.

## 2026-11-02 — Component layout and colour review; named default profiles

Owner requests: review every component in the device-I/O scenarios (layout
and colours); remove "Scenario default" so every scenario opens in one named
profile, chosen here.

**Review method.** A script replays the browser's placement lookup
(`tools/check-static-layouts.py` functions) for all 258 parents in the § 5
scenarios and measures child coverage, bounding-box use, tiny parts drawn
large, overlaps, flat parents (> 40 children), child colour equal to the
parent's, and uncoloured parts. Findings: 37 parents with children in under
half their box (worst: `DeviceRegisterPort`, 10 %), 20 same-colour
parent/child families, 33 flat parents (every device's `REGISTERS`: 72–205
loose gates), 2 overlaps, 0 uncoloured. Screenshots of `BUS_ROOT` and
`FRAMEBUFFER_DISPLAY_ROOT` on 8893 confirmed the worst cases (a one-bit
constant drawn as the largest part; parts pushed to one edge).

**Changes.**
- Floorplans (`visualizer-v2/device_layout.py`): device roots left to right
  (PORT, REGISTERS, the device's own parts; constants in the leftover row);
  bus routers on the left, arbiters on the right; 2×2 register port; FIFO;
  clocked counter; RV32IM decode control (integer decoder above its override);
  every placement now fits and centres the part in its slot.
- Colours: specific shades ahead of the general prefixes for the register
  port, address decode, FIFO and RV32IM-wrapper families; new groups coloured.
- `RegisterFrontEnd` regrouped into `DECODE`, `CHECKS`, `WRITES`, `READS`
  (same gates). All device scenarios, both compared fidelities,
  `ComputerSystemTest`, the machine scenarios and `LedGameTest` pass.
- Static check after the reflows: 0 problems in the 20 affected scenarios.
- Not changed: the M-extension control blocks and the RV32I decode parts
  stay flat (chapter-4 and § 5.1 internals); two overlaps (`Incrementer`,
  a router decode) remain to fix.

**Named default profiles.** `scenarios.json` gives every component scenario
and program a `fidelity`: Gate for gates, wiring, ALUs, building blocks,
memory and device parts (179); Logic for cores and programs (196); Visual
for the five machines and the scalable memory container, where Gate is
unavailable (6). The menu no longer offers "Scenario default"; a scenario
opens in its profile, a switch of scenario returns to the new scenario's
profile, and a different choice stays in the URL. Saved explorer profiles
are keyed `<scenario>::<preset>`; older keys move to the default when loaded
(`profiles.defaults.json` migrated). Six fixed scenarios (no build profiles)
are shown as built with the menu disabled. In C++ an empty preset keeps the
test's own profile for tests and the example-row generator; the probe
defaults to the catalog profile, or the test profile under
`CIRCUITSIM_PROBE_DEFAULT_FIDELITY=test` (set by `tools/run-web-tests.py`).
Layout tools build topologies through `server.scenario_topology`, under the
catalog profile.

Checks so far: `VisualizerFidelityProfileTest`, `VisualizerProfileStoreTest`,
`VisualizerScenarioCatalogTest`, `VisualizerSessionContractTest`,
`VisualizerReferenceProfileTest`, `VisualizerModuleBindingsTest`,
`DeviceParametersBindingsTest`, palette and profile-layout tests pass. The
static check under the new profiles found 2,779 missing placements in 171
scenarios (layouts were keyed to the old profile); filling is in progress.
Also fixed: `DocumentationDiagramsTest`'s stale page list. Open from the
stopped full suite: `SettlingBoundTest` timed out at 600 s.


Correction (2026-11-02, same day): the first choice of default profiles (by
kind: Gate for parts, Logic for cores) made 47 scenarios much larger than
their former curated builds, because every named preset expands word gates
and multiplexers that the old scenario profiles kept compact (examples:
`rv32im-decode-stage` 2,469 → 40,733 parts, `muldiv-w32` 352 → 14,689,
`console-device` 608 → 5,449, the RV32I cores ×7–10 under Gate). All four
presets were measured for those 47 and for one program per core, and each
scenario now opens in the preset closest to its former build (smallest
|log(size ratio)|, ties to the more structural): Gate 132, Logic 49, Visual
192 (all 176 program scenarios, the § 4.7 arrangements, the machines),
Compact 7. Most land within 2× of the former build or match it exactly; the
largest remaining growth is `muldiv-w32` (×22 under Logic; Compact collapses
it to one part).

Follow-up (2026-11-02): with the final profiles, the static check found
2,779 then 1,809 missing placements (171 then 18 scenarios), filled; then
shared keys where compact device groups (machines, Visual) and the DRAM/NOR
command controls (two configurations under Gate) met different structures.
Fixes: compact device groups lay out as `…[config-compact]`, their compiled
model is typed `CompactModel<…>` (neutral colour, not its group's); the two
command controls join the structure-keyed types (server, C++ session, app).
`CHECKS` split into `PAYLOAD` and `STATUS`; palette entries for the memory
request check and storage bank parts now shown under Logic; the incrementer,
display memory controller and router decode overlaps reflowed. Result:
static check 0 problems in 380 scenarios; review of 402 parents in the § 5
scenarios: 0 overlaps, 0 same-coloured children, 0 uncoloured. Remaining by
choice: single-purpose gate blocks over 40 gates (status selections,
validation, decoders) and small slices with spare space.

## 2026-11-02 — Full native suite

One clean run (build first, CTest only on success, `-j1`, 2.5 GB cap) ended
at 07:32 with 15 failures of 615 (`build-milestone/Testing/Temporary/
LastTestsFailed.log`; the scratch log was lost when the host rebooted around
13:00, while nothing of this work was running). `SettlingBoundTest`, which
timed out in the stopped run, passed. Failures and causes:

| Failures | Cause | Fix |
| --- | --- | --- |
| 8 tests of retired parts (`BusErrorTarget`, `AddressRouter`, `InstructionAccessGuard`, `IndexedPixelSelector`, test and scenario) | Still listed in `tests/CMakeLists.txt` after the redesign retired them from the registry | Removed from the list |
| `TestRegistryCoverageTest` | CTest order differed from the registry (`ANDGateWidth8Test`, the § 5 system tests, `ComputerSystemLockstepTest`) | CMake list reordered to the registry |
| `SimulationTimeBindingsTest` | Injects a scenario outside the menu, which has no catalog profile; the probe also resolved its default before reading `--fidelity` | Test names `gate` on both sides; probe resolves the default after options |
| `RecursiveBuildProfileTest` | Expected the RV32I ALU units to stay gate-level under a behavioral ALU subtree; they gained compact forms in the 2026-10-28 behavioral-gap work | Expectation: each unit takes its compact form where it has one |
| `DocumentationExamplesTest` | § 2.6 said `ALU<16>`/`ALU<32>` are structural only; they have the on-request gate-list form | Page table and text corrected |
| `RV32ISingleCycleLayoutTest` | `rv32i-core` now opens in Logic; its floorplan was not saved for that profile | `generate-single-cycle-core-layout.py` run |
| `VisualizerAutomaticLayoutTest` | Compared a preset-built topology with a test-profile build | Both sides use the scenario's profile |
| `RV32IProfileToggleSweepTest` | New word-gate widths in the canonical core (PC+4 incrementer prefix levels, register-file write decoder) were missing from the audit | Eight audit entries added |

Web checks (2026-11-02, only those covering today's changes, against the
8893 site): `FidelityPresetWasmParityTest`, `VisualizerScenarioWasmParityTest`,
`VisualizerScenarioBrowserTest` and `DeviceSystemFidelityBrowserTest` (now
expecting the machine to open in Visual, with no scenario option) pass. Found
and fixed: the recorded-game viewer failed ("No fidelity profile for scenario
'recorded-game-pong-rally'"): a recording is not a catalog scenario, so the
app's lookup now returns no profile for it and the recording keeps its own
(`RecordedCircuitBrowserTest` passes). `RV32IMDeviceBrowserTest` required an
example table for every § 5 scenario: § 5.5 gains one per machine variant;
its iframe selector matched `computer-system` as a prefix of the variants and
now matches the exact alias; and it compares the embedded simulator's profile
with the table's, so example rows are now recorded under each scenario's
catalog profile (`tools/generate-example-rows.mjs`).

## 2026-11-02 — Device browser check

`RV32IMDeviceBrowserTest` against 8893 failed in three places after the
profile and layout changes; all fixed, and it now passes
(`build/validation/web-1102l`). `DeviceSystemFidelityBrowserTest` passes again
after the changes (`web-1102m`); static check of the five devices and two
machines: 0 problems.

| Failure | Cause | Fix |
| --- | --- | --- |
| `button-device`: EVENTS overlaps REGISTERS after a fidelity toggle | A toggle gives the root a new profile with no saved layout; the page then seeds it from the plain `button-device` root entry, which (like the console, timer and framebuffer ones) dated from before the register grouping (children `READ_DATA`, `REGISTER_DECODE`, …) | The 12 root entries naming retired children removed (plain, `@763e…`, `@c2b6…` of those four devices); such profiles now use the type layout, like the other devices |
| `button-device`: INPUT_UPDATE overlaps EVENT_ENCODING after the root returns to structural | The structural and compact forms of a device share one type layout key; `TimerDevice`, `ButtonDevice` and `LedPanel`'s type layouts were tuned for the compact form (wider, shorter boxes), so a structural profile without its own root entry overlapped | Type layouts take the structural placements (from the Logic root entries); the compact-tuned placements stay under the Visual and Compact profile layouts. A compact box is shorter at any width, so it also fits the structural slots |
| Behavioral device root expected to be one box | By design (§ 5.4) the compact device keeps its compiled groups | The check now requires that no part below the root stays structural |
| Button view expected one queued event at the end | The scenario was extended to pop both events (`queue-empty`) | Expects an empty queue |
| `computer-system` BUS caption 5.8 px (< 6) | BUS at width 0.26 in the machine overview | Width 0.28, centred, in every machine entry that held it (two type layouts, three root entries) |

Not done: giving compact devices their own layout key (a `ComponentLayoutView`
variant on `DeviceBehavioral`, as for compact groups) would remove the shared
key but needs native and WebAssembly rebuilds; the data split above is used
instead.

## 2026-11-02 — Recordings and experiments redone

After the 2026-11-01/02 structural changes (adapter chains removed, register
front end and port checker grouped, § 4.7 on `MemorySubsystem`), every
recording and § 5.7 experiment was run again with the same tools and settings
(`build-milestone/system_experiment` and `ComputerSystemTimingTest`, built
04:09 today; only two test sources are newer). One job at a time, each under
a 1.5–2 GB cap.

- **Recordings** (`tools/system/run-experiments.py --profile embedded
  --period 4500 --fidelity compact --memory ff --record`; ten games on
  `games-machine`, Snake with LEDs on `games-machine-leds`): all 11 pass with
  0 setup violations and are identical to the published runs in cycles,
  instructions, presentations, events, console and report words (bring-up
  38,811 … Tetris 191,818). Host time 73–362 s per game (before 143–1,117 s).
  Packaged with the current `layout.json` by
  `tools/package-recorded-circuit.mjs` into
  `site-docs/assets/system/recordings/`; `embedded.json` replaced (same
  fields) and § 5.6 regenerated by `tools/system/results-tables.py`.
- **Machine comparison** (unit timing, 1000-tick clock, six games × three
  machines) and the cached-FF Snake run: all 19 identical to the published
  results except host time; `unit.json`/`unit-cached.json` replaced, § 5.7
  regenerated.
- **Clock certificates** (unit and embedded, five machine cases): identical
  (92/84/90/90/94 ticks; 4,464/4,254/4,464/4,464/4,470 ps; same limiting
  paths); `clock.json` unchanged.
- Checks: `RecordedCircuitBrowserTest` (new Pong-rally recording on 8893),
  `RecordedCircuitTransportTest`, `DocumentationExamplesTest` pass;
  `results-tables.py --check` passes; 8893's strict build passes.
- § 5.7's limits no longer claim the native suite and device browser checks
  are unrun.

## 2026-11-02 — Status choices on word gates (closes the console audit gap)

The 2026-10-29 console audit's remaining item: the register front end's
three-bit status choices (`RegisterFrontEnd.cpp::statusChoice`) and the
router/endpoint `DeviceStatusMux` groups (`DeviceBus.cpp`) were built by
`Logic::mux` at width 3, which has no `Mux<2,3>` and fell back to one select
inverter and AND/AND/OR per bit.

Change: one helper, `detail::wordChoice` (`src/modules/device/DeviceGroups.hpp`),
builds a 2:1 choice of any word-gate width as the select inverter, two
zero-delay Rewires spreading the select and its inverse, `<X>LOW` =
`ANDGate<W>(low, ~select)`, `<X>HIGH` = `ANDGate<W>(high, select)` and one
`ORGate<W>`. Both users call it: each front-end choice is `<CHOICE>_LOW`,
`<CHOICE>_HIGH`, `<CHOICE>` inside `STATUS`; each `DeviceStatusMux` holds
`LOW`, `HIGH`, `OUT_OR`. Per bit these are the same NOT/AND/OR gates and
depth, so timing is unchanged. Address, payload and mask reductions stay
reductions (the audit's advice).

Verification (only what the change touches):

- Native build (155 s, `-j1`, 4 GiB cap). 23 targeted tests pass: the
  register port, console, timer, button, display, LED panel and bus tests and
  their scenarios, `ComputerSystemTest`, two-bus, registered-bridge,
  port-check, `ComputerSystemTimingTest`, `GateListCompactEquivalenceTest`,
  `PartDelayTablesTest`, `DeviceParametersBindingsTest`.
- Clock certificates (unit and embedded, five cases): unchanged (92/84/90/90/94
  ticks; 4,464/4,254/4,464/4,464/4,470 ps).
- Bring-up re-recorded at embedded timing, 4.5 ns: cycles, instructions,
  events, console, report, recorded circuit and checkpoints identical to the
  published recording (only host seconds differ), so the published
  recordings stay valid.
- Layouts: `DeviceStatusMux` and the five `RegisterStatus<…>` types reflowed
  (`recalculate_layouts.py --fill-missing` for the 15 device/bus/machine
  scenarios); static check 0 problems.
- WebAssembly rebuilt (261 s); example rows for the 15 device, bus and machine
  scenarios regenerated and verified: identical to the previous bundle.
- Public page: § 5.4.1's `STATUS` description.
- Browser checks on the rebuilt package (8893): `RV32IMDeviceBrowserTest` and
  `DeviceSystemFidelityBrowserTest` pass.


## 2026-11-03 — Full native and browser suites (replaces "not run")

This replaces the 2026-10-29 plan-status row for Part 11 ("the full native
suite is not run") and the 2026-11-02 notes that the complete browser suite
was not rerun. Both suites were run once on the final state of the work.

**Native suite** (`ctest --test-dir build-milestone -j1`, 2.5 GB cap,
started 2026-11-02 after the status-choice change): 607 of 607 pass,
3 h 15 min.

**Browser suite** (`tools/run-web-tests.py --suite all` against the 8893
preview, 2.5 GB cap, 2 h 37 min): 38 of 49 passed on the first run
(`build/validation/web-full-1102`). The 11 failures were fixed and each
was rerun, together with the 4 checks the fixes could affect
(`web-1103a`…`web-1103i`). The result is 48 passed and 1 skipped (below).

| Failure | Cause | Fix |
| --- | --- | --- |
| `RV32IMDeviceWasmParityTest` | Read the retired `site-docs/rv32im` and `device-io` chapters | Reads every § 5 example (28 scenarios, `LedPanel` observer added); 28 of 28 match native |
| `TickRecordingBrowserTest` | Needs an external capture (`CIRCUITSIM_TICK_RECORDING_FILE`) | The manifest names it (`requires_env`); the runner reports such a check as skipped. Not run: no capture was made |
| `DocumentationDiagramsBrowserTest` | `timing/` moved to `basic/1.2/`; `cache-topologies` labels cramped with the web font; the five § 5 figures and § 4.6 were not checked | Path fixed; small boxes 32 px high; § 5 and § 4.6 pages added, which found label crossings in `system-overview` and `led-panel` (address notes moved below the drawing, `BLINK` tag moved) |
| `VisualizerTimingProfileBrowserTest` | Expected `fidelity=` in the URL for the scenario's own profile, which the page leaves out | Expects it only for another profile |
| `VisualizerRewireBrowserTest` | FF memory's `READ_SELECT.WORD_0_BITS` no longer exists | Uses the current unpacker `READ_SELECT.ENABLE.A_BITS` (same `Rewire<in=1,out=8>`) |
| `VisualizerWasmBrowserTest` | Expected no compact form for `HalfAdder`, `FullAdder` and `ALU<W>` (they have the on-request gate-list form, as § 2 says); `ArithmeticUnit<8>` counted 908 parts, 911 in the Gate profile it now opens in | Expectations updated |
| `RV32IBuildingBlocksBrowserTest` | `rv32i-alu-result-select` opens in Gate, which had no root entry; the page then starts from the plain entry, which held an older bit-sliced arrangement | The matching entry (old fingerprint, same children) copied to the Gate key and the plain entry; same for `rv32i-memory-request` and `decoder-address5` |
| `MemoryDeviceDocumentationBrowserTest` | Same lookup: `matrix-column-mux`'s plain entry kept proportions from before the column-mux reflow (0.24), so `DATA_BITS` stuck out | Stale proportions dropped from the plain entry (the type's 0.33 applies, as for the old profile key) |
| `MemoryDocumentationBrowserTest` | Shipped default profiles set `flip-flop-cell::gate` and `rv32i-register-file::gate` parts to structural, which Gate already is; the no-op override changed the fingerprint, so the page did not match its example rows | Both no-op entries removed from `profiles.defaults.json` |
| `SRAMLayoutBrowserTest` | The check wrote a position without a width (an invalid placement), so reload rebuilt the row | Writes a complete placement, as the page's editors do |
| `RecordedGameViewerBrowserTest` | `games.html` now defaults to Visual; the bring-up run exceeded the 500-million-event budget. The evidence block is in a closed `<details>`, whose rendered text is empty | Page defaults to Compact (the profile the recordings use), others selectable; the check reads `textContent` |

`tools/check-static-layouts.py` missed the two lookup cases because it did
not model the browser's start from the plain entry (`normalizeLayoutPayload`).
It now does. Over all 380 scenarios it found exactly the `matrix-column-mux`
case, and it reports 0 problems after the fixes.

Native checks after the fixes: `VisualizerStaticLayoutTest`,
`WebTestRunnerTest`, `DocumentationDiagramsTest`,
`DocumentationExamplesTest`, `VisualizerProfileStoreTest`,
`RegisterFileProfileMigrationTest` and `VisualizerReferenceProfileTest`
pass. The 8893 strict build passes. The native suite was not rerun after
these fixes; they change tests, layouts, shipped profiles, a figure and the
games page, but no C++.

Record closed 2026-11-03.
