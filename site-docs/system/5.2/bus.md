# § 5.2.2. `Bus<W>`

The bus joins a described list of masters and targets by a table of address
windows. It is built from parts the earlier sections already proved: one
[router](router.md) per master and, where several masters share a target
port, a tree of the
[two-port arbiter](../../memory/controllers.md#353-twoportmemoryarbiterw)
of § 3.

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| `Bus<W>` | Routers per master, an arbiter tree per shared target, straight wires otherwise | Yes | On request (the whole bus compiled) |

Catalog: `bus.interconnect.W` for W = 8, 16, 32; one parameter, `config`,
the bus's JSON description (below). The bus is part of a
[system description](../5.5.md) and is normally built by
`ComputerSystem`.

<!-- diagram: bus-structure -->

## The bus description

```json
{"name": "BUS", "width": 32,
 "masters": [
   {"name": "CPU_I", "port": "CORE.IMEM_", "access": "fetch"},
   {"name": "CPU_D", "port": "CORE.DMEM_", "access": "data"}],
 "regions": [
   {"name": "RAM", "base": "0x00000000", "bytes": "0x40000", "execute": true,
    "targets": {"CPU_I": "MEMORY.I_", "CPU_D": "MEMORY.D_"}},
   {"name": "CONSOLE", "base": "0x10000000", "bytes": "0x100", "target": "CONSOLE"}]}
```

| Field | Meaning |
| --- | --- |
| bus `pipelined` | `true` builds every router of this bus with `pipelined=1` ([§ 6.3.2](../../accelerator/6.3.md#632-the-pipelined-bus)); default `false` |
| master `port` | The part and pin prefix of the master's port: `CORE.IMEM_` is the core's `IMEM_REQ_VALID`, … |
| master `access` | `fetch` masters only read, and only reach regions with `"execute": true` |
| region `target` | The port every master reaches through this window |
| region `targets` | A port per master instead; a master missing from the map does not see the window |
| region `local` | `false` passes the whole address to the target (a bridge) |

In the example, both of the core's ports reach RAM, but each through its
own port of the memory subsystem; only the data port sees the console.

## IO pins

| Pin | Direction | Width | Meaning |
| --- | --- | ---: | --- |
| `CLK`, `RST` | Input | 1 each | Clock and reset of every router and arbiter |
| `<MASTER>_REQ_VALID`, `…REQ_OP`, `…REQ_ADDR`, `…REQ_WRITE_DATA`, `…REQ_WRITE_MASK`, `…RSP_READY` | Input | 1, 3, 32, W, W/8, 1 | Each master's request side |
| `<MASTER>_REQ_READY`, `…RSP_VALID`, `…RSP_READ_DATA`, `…RSP_STATUS` | Output | 1, 1, W, 3 | Back to each master |
| `<TARGET>REQ_VALID`, `…REQ_OP`, `…REQ_ADDR`, `…REQ_WRITE_DATA`, `…REQ_WRITE_MASK`, `…RSP_READY` | Output | 1, 3, 32, W, W/8, 1 | To each target; e.g. TARGET = `MEMORY_I_` for `MEMORY.I_` |
| `<TARGET>REQ_READY`, `…RSP_VALID`, `…RSP_READ_DATA`, `…RSP_STATUS` | Input | 1, 1, W, 3 | Answers from each target |
| `<TARGET>CLK`, `<TARGET>RST` | Output | 1 each | Port clock/reset outputs; the computer clocks parts directly |

## Behavior

- Each master gets exactly the behavior of its router: one request at a
  time, decoded against the windows it can see, an error answer for any
  other address.
- Two masters can talk to two different targets in the same cycle; they wait
  for each other only at a shared target.
- At a target reached by two masters, the arbiter takes one request at a
  time and alternates when both are waiting. It retains ownership until the
  answer is consumed, including when that master applies backpressure.
- At a target reached by three or more masters, two-port arbiters form a
  tree: masters are paired in description order, an unpaired one moves up a
  level, and each pair's winner competes at the next arbiter. Every arbiter
  alternates between its two inputs, so the shares are not equal: with three
  masters all waiting, the third wins every other transfer and the first two
  one in four each.
- A target sees requests only for its window, with addresses made local to
  it.

A description is checked before anything is built: windows must be aligned
powers of two and must not overlap; every master must reach at least one
window; a master may not reach one target port through two windows.

The arbiter can hold a burst on its own, but the bus's routers reject burst
operations. The current machines send only reads and writes through a bus;
their cache fills happen below the memory subsystem's ports. There is no
equal-share N-way arbitration, transaction ID, out-of-order return or
timeout. A target
that never answers can hold its owner indefinitely; other targets remain
independent.

## Implementation

| Part | Built when |
| --- | --- |
| `<MASTER>_ROUTER` — `bus.router.W` | One per master, with the windows that master sees |
| `<PORT>_ARBITER` — `memory.arbiter.W` | One per target port that exactly two masters reach (first master on `I_`, second on `D_`) |
| `<PORT>_ARBITER_<k>` — `memory.arbiter.W` | The tree for a port that three or more masters reach, numbered from the leaves; the last one drives the port |
| Wires | A target port with one master: the router's route pins are the bus's port pins |

Because the parts are the old router and arbiter, a machine whose bus has
the same windows as the old fixed router has the same timing, cycle for
cycle ([§ 5.7](../5.7.md)). The layout variant of a bus in the simulator is
named by a hash of its description.

**Compact form.** The whole bus can also be compiled into one compact
part: the same gates, compiled together, with its routers' and arbiters'
storage characterized. It is chosen only by an exact path rule or a parent
that requires it, never by a profile's general behavioral choice; the
machines in this chapter build the bus from its parts. The compiled bus has
no stored delay table: its timing is computed from the gates it compiles.

**Timing.** The assembly adds no register beyond each router and any shared
arbiter. Built from parts, the selected child implementations supply their
proven delays. Reset and four-state handling come from those children. A
clock certificate depends on the actual topology and endpoints, as measured
in § 5.7.

## Examples

A bus with two masters, `CPU` and `DMA`, and two targets: `MEMORY` at
`0x0`, which both masters reach (so its port has an arbiter), and `DEVICE`
at `0x10000000`, which only `CPU` reaches. The targets answer at once with
fixed words (`0x1111` from memory, `0x2222` from the device). Both masters
ask for memory in the same cycle: each router holds its own request, the
arbiter serves them in turn, and both get their answer. The device answers
`CPU`; `DMA`, which has no window there, gets Range.

<!-- simulator-examples: bus-interconnect BusInterconnectScenarioTest -->

<div class="simulator-embed">
  <iframe src="../../../simulator/?scenario=bus-interconnect&amp;embed=1" title="Bus&lt;32&gt; simulation" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../simulator/?scenario=bus-interconnect" class="md-button">Open the full visualizer</a>

Inside a whole machine, open the [computer system](../5.5.md#examples) and
expand `BUS`. Its two routers are `CPU_I_ROUTER` (one route, to the
memory's fetch port) and `CPU_D_ROUTER` (six routes).

## Source and tests

[Bus description and interface](https://github.com/arnold518/reinventing-the-wheel/blob/alpha/include/system/Bus.hpp),
[router/arbiter assembly](https://github.com/arnold518/reinventing-the-wheel/blob/alpha/src/system/Bus.cpp),
[verification](https://github.com/arnold518/reinventing-the-wheel/blob/alpha/src/tests/SystemTests.cpp).

| Test | Coverage |
| --- | --- |
| `BusInterconnectTest` | A bus with a fetch master and three masters sharing the memory's data port (a two-arbiter tree): local offset, whole-address window, error answer; 400 random cycles equal between the gate-level bus, the bus with compiled routers and the whole bus compiled |
| `BusInterconnectScenarioTest` | The recorded example |
| `SystemDescriptionTest` | Region visibility per master, fetch masters, overlapping, misaligned and unreachable windows rejected |
| `ComputerSystemTest` and the games | The bus inside the machines, every access checked against an independent model |

The 400-cycle test establishes equivalence for those sampled inputs, not
an independent random-transaction or arbitration-fairness campaign.
