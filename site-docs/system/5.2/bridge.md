# § 5.2.3. `BusBridge<W>`

A bridge is a target on one bus and a master on another. Requests that
reach it on its target side (`S_`) leave on its master side (`M_`); answers
go back. With a bridge, buses form layers, as in a real computer: a fast
**system bus** for the core and memory, and a **peripheral bus** for the
slow devices behind it.

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| `BusBridge<W>` | Pass a transfer between two bus ports, straight or through registers | Yes | Yes |

Catalog: `bus.bridge.W` for W = 8, 16, 32; parameter `registered` = 0
(default, pass-through) or 1.

<!-- diagram: bus-layers -->

## IO pins

| Pin | Direction | Width | Meaning |
| --- | --- | ---: | --- |
| `CLK`, `RST` | Input | 1 each | Clock and reset |
| `S_REQ_VALID`, `S_REQ_OP`, `S_REQ_ADDR`, `S_REQ_WRITE_DATA`, `S_REQ_WRITE_MASK`, `S_RSP_READY` | Input | 1, 3, 32, W, W/8, 1 | Requests from the upper bus |
| `S_REQ_READY`, `S_RSP_VALID`, `S_RSP_READ_DATA`, `S_RSP_STATUS` | Output | 1, 1, W, 3 | Answers to the upper bus |
| `M_REQ_VALID`, `M_REQ_OP`, `M_REQ_ADDR`, `M_REQ_WRITE_DATA`, `M_REQ_WRITE_MASK`, `M_RSP_READY` | Output | 1, 3, 32, W, W/8, 1 | Requests onto the lower bus |
| `M_REQ_READY`, `M_RSP_VALID`, `M_RSP_READ_DATA`, `M_RSP_STATUS` | Input | 1, 1, W, 3 | Answers from the lower bus |
| `S_CLK`, `S_RST` | Input | 1 each | Unused port clock (the bridge uses `CLK`) |
| `M_CLK`, `M_RST` | Output | 1 each | Unused, held at 0 (the lower bus is clocked by the system) |

## Behavior

**Pass-through** (`registered = 0`): every `S_` request signal is the
matching `M_` signal and every `M_` answer the matching `S_` answer. A
transfer crosses in the same cycle.

**Registered** (`registered = 1`): one transfer at a time, one clock edge
each way.

| State | `S_REQ_READY` | `M_REQ_VALID` | `M_RSP_READY` | `S_RSP_VALID` | Leaves when |
| --- | :---: | :---: | :---: | :---: | --- |
| Idle | 1 | 0 | 0 | 0 | `S_REQ_VALID`: the request is captured |
| Holding the request | 0 | 1 | 0 | 0 | `M_REQ_READY`: it is taken below |
| Waiting | 0 | 0 | 1 | 0 | `M_RSP_VALID`: the answer is captured |
| Holding the answer | 0 | 0 | 0 | 1 | `S_RSP_READY`: it is taken above |

A transfer through the registered bridge costs at least two more clock
edges than through the pass-through one. It carries single-word operations;
a burst does not cross it (no machine sends one through a bridge: caches are
inside the memory subsystem).

The registered form holds the captured request unchanged under downstream
backpressure and holds the captured answer under upstream backpressure.
Reset clears all three state bits and suppresses upstream readiness; it
discards an unfinished transfer rather than generating a cancellation
answer. A downstream write that already happened is not rolled back.

The pass-through form adds no state, status validation or reset gating to
the transaction wires. It forwards `X`/`Z` too. In registered mode, legal
binary handshake traffic after reset is the system operating boundary;
there is no timeout or conversion of unknown controls into an error status.

### Why the address is not changed

The system bus passes the whole peripheral window (`"local": false`) to
the bridge, so the peripheral bus sees `0x10002004`, not `0x2004`, and its
windows keep the device addresses of the one-bus machine. The guest program
is the same binary on every machine.

## Implementation

### Structural implementation

Pass-through: zero-delay wires (`Rewire`). Registered: three state bits
(`HOLD_REQUEST`, `WAITING`, `HOLD_RESPONSE`) and registers for the request
fields (`REQ_OP`, `REQ_ADDR`, `REQ_WRITE_DATA`, `REQ_WRITE_MASK`, captured on
accept) and the answer (`RSP_READ_DATA`, `RSP_STATUS`, captured when it
arrives).

### Behavioral implementation

Both forms compile the bridge's own gates to a gate list with delays proven
on them.

**Timing.** Pass-through links are zero-delay rewires. In registered mode,
the fields and state must meet their register setup/hold constraints; the
new request and answer appear after clock-to-output propagation. Delays are
derived at build time for the selected width and registration setting, so
there is no universal stored bridge table. This is a same-width bridge
with one shared clock, not a width converter or clock-domain crossing.

## Examples

A registered bridge: a read is taken, offered below one edge later, answered
below, held, and taken above.

<!-- simulator-examples: bus-bridge BusBridgeScenarioTest -->

<div class="simulator-embed">
  <iframe src="../../../simulator/?scenario=bus-bridge&amp;embed=1" title="BusBridge&lt;32&gt; simulation" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../simulator/?scenario=bus-bridge" class="md-button">Open the full visualizer</a>

## Source and tests

[Interface](https://github.com/arnold518/reinventing-the-wheel/blob/alpha/include/system/Bridge.hpp),
[register state and compiled form](https://github.com/arnold518/reinventing-the-wheel/blob/alpha/src/system/Bridge.cpp),
[recorded transfer](https://github.com/arnold518/reinventing-the-wheel/blob/alpha/src/tests/SystemTests.cpp).

| Test | Coverage |
| --- | --- |
| `BusBridgeScenarioTest` | The recorded registered transfer, both fidelities |
| `ComputerSystemTwoBusTest`, `ComputerSystemRegisteredBridgeTest` | The bring-up program through each bridge, every access checked |
| Games on the two-bus machines | [§ 5.7](../5.7.md) |
