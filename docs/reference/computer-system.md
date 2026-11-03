# Computer system contracts

The maintained contracts of the computer built in site chapter 5:
[§ 5 Computer System](../../site-docs/system/index.md) holds the public pin
schemas and component pages. The design and its evidence are in the
[Device I/O architecture record](../records/2026-10-28-device-io-architecture.md).
The superseded contracts of the fixed device system are archived in
[the RV32IM and device foundation](../archive/2026-10-29-rv32im-device-foundation.md).

## Contracts

| Contract | Type | Parameters | Forms |
| --- | --- | --- | --- |
| `system.computer` | `ComputerSystem` | `machine` (built-in name, optionally with games options) or `description` (JSON) | structural |
| `bus.interconnect.<W>` | `Bus<W>` | `config`: one bus's JSON | structural |
| `bus.router.<W>` | `BusRouter<W>`, `BusFetchRouter<W>` | `access` (fetch, data), `routes` (`PREFIX@0xBASE+0xBYTES[!],...`) | structural, compiled gate list |
| `bus.bridge.<W>` | `BusBridge<W>`, `RegisteredBridge<W>` | `registered` (0, 1) | structural, compiled gate list |
| `memory.subsystem.32` | `MemorySubsystem` | `technology` (ff, sram, dram), `words`, `caches` (none, default) | structural |
| `device.leds` | `LedPanel` | — | structural, compact device |
| `device.console`, `device.timer`, `device.buttons`, `device.framebuffer` | as before | as before | structural, compact device |
| `device.word.register-port.<W>` | `DeviceRegisterPort<W>` | — | structural; compiled at W = 32 |

W is 8, 16 or 32 for buses, routers and bridges. Retired IDs, with no
aliases: `rv32im.system.devices`, `bus.word.address-router.<W>`,
`bus.word.instruction-guard.<W>`, `bus.word.error-target.<W>`,
`video.pixel-selector.<W>`.

## System description (schema 1)

- `parts[]`: `name`, `family` (a catalog contract), `parameters` (strings;
  numbers and booleans are converted, nested objects kept as JSON text).
- `buses[]`: `name`, `width`, `masters[]` (`name`, `port` = `PART.PREFIX`,
  `access` = fetch|data), `regions[]` (`name`, `base`, `bytes`, `execute`,
  `local`, `target` and/or `targets` = master → port).
- `inputs[]` (`name`, `to`), `outputs[]` (`name`, `from`).

Validation: unique part, bus, master and region names; aligned power-of-two
regions of at least 4 bytes that do not overlap on one bus; known parts and
masters; every master reaches a region; a fetch master only reaches
`execute` regions. Building adds: a master may not reach one port through two
regions; every part input must be driven. Any number of masters may share
a target port. Built-in descriptions are `systems/*.json`, embedded at build time
(`BuiltinSystems.generated.hpp`).

## Building

`ComputerSystem` adds parts by contract (so profiles select fidelity per
part), buses as `bus.interconnect.<W>` with their JSON, connects master and
target ports by pin name (`REQ_VALID`, `REQ_OP`, `REQ_ADDR`,
`REQ_WRITE_DATA`, `REQ_WRITE_MASK`, `RSP_READY` one way; `REQ_READY`,
`RSP_VALID`, `RSP_READ_DATA`, `RSP_STATUS` the other), the description's
inputs and outputs, and the system `CLK`/`RST` to every part and bus input
named `CLK`, `RST`, `*_CLK` or `*_RST`. Port `CLK`/`RST` outputs are not
connected. Views: the first part with `RV32IPipelineStateView` is the core,
the first `MemorySubsystem` is main memory (program load limit = its size,
reduced to the executable window at address 0), the first
`device.framebuffer` is the display (its `PIXELS` child for capture).

`Bus<W>` builds one `bus.router.<W>` per master with the regions that master
sees (route prefix = `portPrefix(port)`: `MEMORY.I_` → `MEMORY_I_`), one
`memory.arbiter.<W>` per port exactly two masters reach (`<PORT>_ARBITER`,
first master on `I_`), a tree of them for three or more masters
(`<PORT>_ARBITER_<k>`: pairs in description order, an unpaired master moves
up a level, the last arbiter drives the port), and straight connections
otherwise. The whole bus also has an on-request compact form
(`ImplementationDescriptor::gate_list`, chosen only by an exact path rule or
a parent requirement): its gates compiled together with characterized
storage, no stored delay table. The router is the former address router's
gate recipe: a registered request stage, one transaction at a time, a Range
answer for any address outside its routes, local offsets unless the region
has `local: false`. A fetch router accepts reads only. Each bus level behind
a bridge therefore adds one registered stage.

`GameMachineOptions` (`caches`, `dram`, `width`, `height`, `video_bytes`,
`timer_divisor`, `timer_frequency_hz`, `console_depth`, `button_depth`)
patch a games-machine description (`withGameOptions`); the `VIDEO` region
follows the display's storage.

## Register tables

`deviceRegisterTable(contract, parameters)` returns a device's table:
`device` name, registers (`name`, word `offset` < 0x80, `access`, `width`,
`value`, `summary`, `fields`). Access kinds: read-only (value from the core),
setting (constant), read-write (stored in the front end), write-only data,
command (bit 0 acts). Status precedence in the front end: no register →
Range (2); write to read-only or setting → ReadOnly (3); read of write-only
data or command → Unsupported (4); a nonzero-mask write whose value has
unknown bits or bits above its width → Invalid (6). The register port's own
checks (alignment, operation, mask) come first. The front end decodes with
one `Decoder<n>` over the word index enabled by an in-window check.

The guest header (`system_header`) lists every non-bridge region's
`_BASE`/`_BYTES` and every register device's `<PART>_<REG>` address,
`_OFFSET` and field macros, with CPU addresses that follow bridges.
`tools/rv32im/guest/system.h` and `system-leds.h` are generated and checked
by `GuestHeaderTest`.

## Visualizer

Floorplans in `visualizer-v2/device_layout.py`: the machine (core and memory
on the left; buses with each bridge between them in the middle; devices on
the right; port checkers in a bottom row); a bus (routers on the left,
arbiters, including an arbiter tree, on the right); the memory subsystem
(port caches, arbiter, shared cache, memory, top to bottom; one layout
variant per cache set, `caches-none|i|d|s…`); device roots (`PORT`,
`REGISTERS`, then the device's own parts); and small grids for the register
port, FIFO, clocked counter and RV32IM decode control. The register front
end and the port checker are grouped (`DECODE`/`CHECKS`/`WRITES`/`READS`;
watches, handshake, flags), and each group family has its own palette
prefix. The machines open in the Visual preset (Gate is unavailable for
their 256 KiB memory); devices, buses, routers, the bridge, the register port,
FIFO and counter open in Logic; the memory subsystem and the port checker in
Compact (their gate-level parts stay structural there).

## Recorded runs

`runRecordedGuest` builds `system.computer` with `RecordedGuestOptions::system`
(default `games-machine`) and the games options, and checks every data
access, device sample, ordered write, PRESENT capture and retirement against
independent models. Its device model knows the games-machine register map
and, when the machine has a part named `LEDS`, the LED panel (`LEDS`,
`BLINK`, and the pins at every frame: lit when `BLINK` is 0 or counter bit
`BLINK` is 0, the counter being the cycles since reset). Fixtures record the
system they were built for (`snake-leds` → `games-machine-leds`).
`system_experiment` runs a fixture on any machine, memory configuration,
fidelity preset, timing profile and period, re-timing its input events to
one tick after the new falling edge, and can write a visualizer recording.
