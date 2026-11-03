# § 5. Computer System

The earlier chapters built a processor and memories. This chapter connects
them into a computer: a processor with multiply and divide, main memory,
and devices that talk to the outside world, all joined by **buses**. The
machine runs small games written in C, and the same games run on three
differently built machines, compared at the end of the chapter.

<!-- diagram: system-overview -->

Three ideas carry the whole chapter.

1. **One port.** Every connection uses the request/response port of
   [§ 3.4](../memory/transactions.md): a request (operation, address, data,
   byte mask) goes one way, a response (data, status) comes back. A memory, a
   device, a bridge and a bus all have the same port, so any of them can be
   put where another was.
2. **Addresses choose the target.** Every target owns a window of the
   address space. The bus looks at a request's address and passes it to the
   target whose window holds it; an address in no window is answered with an
   error. A device is used by reading and writing its window, exactly like
   memory ("memory-mapped I/O").
3. **The system is data.** A machine is a JSON description: its parts, its
   buses and its address map. The simulator builds the circuit from it, and
   the C header the games are compiled against is generated from the same
   file, so hardware and software cannot disagree. Adding a device is adding
   a few lines to the description.

## Contents

| Section | What it covers |
| --- | --- |
| [5.1. RV32IM Core](5.1/index.md) | Multiply and divide units, the five-stage core that runs all eight M instructions, recorded programs |
| [5.2. Buses](5.2/index.md) | The bus port, the per-master router, the bus, and the bridge that joins two buses |
| [5.3. Memory Subsystem](5.3.md) | Main memory with an instruction and a data port, optional caches |
| [5.4. Devices](5.4/index.md) | Register tables and the shared register front end; console, timer, buttons, display and LED panel |
| [5.5. System Description](5.5.md) | The JSON description, the `ComputerSystem` built from it, and the generated guest header |
| [5.6. Games](5.6.md) | The C games, how a run is checked, and recorded play |
| [5.7. Experiments](5.7.md) | One bus, two buses, a registered bridge: cycles, run time and clock |

## Where the textbook figures are

The chapter follows the hardware organization of *Computer Systems: A
Programmer's Perspective* (3rd edition), simplified to one clock and without
interrupts or DMA.

| Textbook | Here |
| --- | --- |
| Figure 1.4 (hardware organization): CPU, buses, main memory, I/O devices | `ComputerSystem` built from `systems/games-machine.json` ([5.5](5.5.md)) |
| Figure 6.6 (bus structure): system bus, I/O bridge, memory bus | `games-machine-two-bus`: a system bus, a [bridge](5.2/bridge.md) and a peripheral bus ([5.7](5.7.md)) |
| Figure 6.11–6.12 (I/O through addresses, device controllers) | Each device's register window and its [register table](5.4/registers.md) |
| Section 6.4 (caches between CPU and memory) | The optional caches in the [memory subsystem](5.3.md) |

## The machines

Every machine is a file in `systems/`. All of them run the same games with
the same compiled programs.

| Machine | Buses | What it adds |
| --- | --- | --- |
| `games-machine` | One bus | The reference machine: core, memory subsystem, console, timer, buttons, display |
| `games-machine-two-bus` | System bus + peripheral bus | The devices move behind a pass-through bridge |
| `games-machine-registered-bridge` | System bus + peripheral bus | The bridge registers each request and response |
| `games-machine-leds` | One bus | An LED panel, added only through the description |

## Address map

The default address map, the same in every machine. Each window is an
aligned power of two; the gaps between windows hold nothing.

| Window | Base | Bytes | Target |
| --- | --- | --- | --- |
| RAM | `0x00000000` | `0x40000` | Memory subsystem (fetch and data) |
| CONSOLE | `0x10000000` | `0x100` | Console registers |
| TIMER | `0x10001000` | `0x100` | Timer registers |
| BUTTONS | `0x10002000` | `0x100` | Button registers |
| DISPLAY | `0x10003000` | `0x100` | Display registers |
| LEDS | `0x10004000` | `0x100` | LED panel registers (LED machine only) |
| VIDEO | `0x20000000` | `0x1000` | Display pixel memory |

Only RAM is executable: an instruction fetch from any other window is
answered with a Range status and the core traps.

## Reading this chapter

Component sections follow the pattern of the earlier chapters: **IO pins →
behavior → implementation → examples and embedded simulation → tests**. The
system-level sections (5.5–5.7) explain how the parts are put together and
measured. In the simulator, the chapter's scenarios are in the **Computer
System** menu group; expanding a part shows its gates, collapsing it selects
its compact form.
