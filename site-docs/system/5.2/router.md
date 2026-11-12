# § 5.2.1. `BusRouter<W>`

One master's view of the bus. The router takes the master's request, finds
which of its routes holds the address, and offers the request on that
route's port; the answer comes back the same way. An address in no route is
answered by the router itself. Every master on a bus has its own router.

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| `BusRouter<W>` | Address decode, one held request, error answers, response return | Yes | Yes |

Catalog: `bus.router.W` for W = 8, 16, 32 (`BusFetchRouter<W>` when it
serves a fetch master). Parameters:

| Parameter | Meaning |
| --- | --- |
| `access` | `fetch` (reads only) or `data` (reads and writes) |
| `routes` | Comma-separated `PREFIX@0xBASE+0xBYTES`; a trailing `!` passes the whole address instead of the offset |
| `pipelined` | Optional, and only `1` is accepted: take the next request at the edge the previous answer is taken, instead of one edge later. A bus description sets it with `"pipelined": true` ([§ 6.3.2](../../accelerator/6.3.md#632-the-pipelined-bus)) |

For example `MEMORY_@0x00000000+0x1000,DEVICE_@0x10000000+0x100` gives two
routes whose ports are named `MEMORY_*` and `DEVICE_*`.

## IO pins

| Pin | Direction | Width | Meaning |
| --- | --- | ---: | --- |
| `CLK`, `RST` | Input | 1 each | Clock, reset |
| `REQ_VALID`, `REQ_OP`, `REQ_ADDR`, `REQ_WRITE_DATA`, `REQ_WRITE_MASK`, `RSP_READY` | Input | 1, 3, 32, W, W/8, 1 | The master's request side |
| `REQ_READY`, `RSP_VALID`, `RSP_READ_DATA`, `RSP_STATUS` | Output | 1, 1, W, 3 | Back to the master |
| `<P>REQ_VALID`, `<P>REQ_OP`, `<P>REQ_ADDR`, `<P>REQ_WRITE_DATA`, `<P>REQ_WRITE_MASK`, `<P>RSP_READY` | Output | 1, 3, 32, W, W/8, 1 | Route P's request, per route |
| `<P>REQ_READY`, `<P>RSP_VALID`, `<P>RSP_READ_DATA`, `<P>RSP_STATUS` | Input | 1, 1, W, 3 | Route P's answer |
| `<P>CLK`, `<P>RST` | Output | 1 each | The clock and reset, passed on |

## Behavior

| Edge | Effect |
| --- | --- |
| Idle, `REQ_VALID` | Take the request (`REQ_READY` was 1); hold its fields and which route owns it |
| Holding, owned by route P | Offer it on P (`<P>REQ_VALID` = 1) until `<P>REQ_READY`; then wait for P's answer |
| P answers | `RSP_VALID`, data and status come straight through; taken when `RSP_READY` |
| Holding, no route (or an error) | Answer at once with the status; no route sees anything |

The decode checks, in order: unknown request bits → Invalid (6); an
operation the router does not pass (anything but read for a fetch router,
anything but read or write for a data router) → Unsupported (4); a write with
an unknown mask → Invalid (6); a misaligned address → Alignment (1); no
route → Range (2). One request is in flight at a time.

The request is registered: it reaches the route one clock edge after the
master offered it, and the router takes the next request only after the
answer is taken. A two-level path (a bus behind a bridge) pays this once per
level; [§ 5.7](../5.7.md) measures what that costs.

Reset discards ownership and the held request, suppressing ready/valid
while asserted. The master must hold its fields until admission; afterwards
the captured copy is independent of later master inputs. The target must
hold its answer while `RSP_READY = 0`, because the router passes that answer
through rather than storing another response copy. Reset does not undo a
write that a target has already executed.

Window sizes are aligned powers of two of at least 4 bytes. Duplicate
prefixes and overlapping windows are rejected at construction. For a local
window, the output address is the low log2(size) bits, zero-extended to 32;
there is no run-time subtractor. Write payload validity is the target's
responsibility; the router validates the operation, address and write mask.

## Implementation

### Structural implementation

| Part | Responsibility |
| --- | --- |
| `DECODE` | One window comparison per route (`DeviceAddressWindow<n>`: the top bits against the base), request validation, the status |
| `REQUEST` | The held operation, address, data, mask, status and the owning route |
| `CONTROL` | Busy and sent state: ready, accept, answer |
| `ROUTE_<P>` | Per route: offers the request, makes the offset (or passes the whole address), lets the answer through |
| `RESPONSE_SELECT` | The owning route's data and status, or the router's own error |

### Behavioral implementation

The compact router runs its own gates as a compiled gate list, with delays
proven on the same circuit for each parameter set at build time.
`BusInterconnectTest` drives the gate-level and compiled routers with the
same random traffic and requires identical outputs at every settle.

**Timing.** Admission captures the request on a rising edge. Address and
validation must settle before that edge; target response propagation remains
combinational. Routes affect pin count and gate paths, so this compact form
derives its delays for the actual width, access kind and route list when
built. There is no one stored default router table to apply to every map.

## Examples

A router with a 4 KiB memory route and a 256-byte device route. The device
read is held while the target is not ready; the target's answer passes
straight back; an unmapped address is answered by the router; a write goes
to the memory route.

<!-- simulator-examples: bus-router BusRouterScenarioTest -->

<div class="simulator-embed">
  <iframe src="../../../simulator/?scenario=bus-router&amp;embed=1" title="BusRouter&lt;32&gt; simulation" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../simulator/?scenario=bus-router" class="md-button">Open the full visualizer</a>

## Source and tests

[Router parameters and catalog factories](https://github.com/arnold518/reinventing-the-wheel/blob/alpha/src/system/Bus.cpp),
[structural decode, request and return paths](https://github.com/arnold518/reinventing-the-wheel/blob/alpha/src/modules/device/DeviceBus.cpp),
[scenarios and bus comparison](https://github.com/arnold518/reinventing-the-wheel/blob/alpha/src/tests/SystemTests.cpp).

| Test | Coverage |
| --- | --- |
| `BusRouterScenarioTest` | The recorded example, both fidelities |
| `BusInterconnectTest` | Local offset, whole-address window, error answer through a bus; 400 random cycles, gate-level and compiled routers equal |
| `ComputerSystemFaultTest` | Fetch from a device window, gaps between windows, unmapped data, beyond video memory: each traps the core |

The random comparison changes inputs without a protocol driver and compares
two router implementations; it is not an independent legal-traffic model
or an exhaustive check of every status-precedence combination.
