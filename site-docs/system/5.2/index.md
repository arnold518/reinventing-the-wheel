# § 5.2. Buses

A bus lets several parts share connections: any master can reach any
target that owns the address it asks for. This section defines the port
every part shares, then builds a bus from two parts already proven
elsewhere, and finally a bridge that joins two buses into layers.

| Section | Coverage |
| --- | --- |
| 5.2 Buses (this page) | The bus port, masters and targets, windows, statuses |
| [5.2.1. BusRouter&lt;W&gt;](router.md) | One master's address decoder and request holder |
| [5.2.2. Bus&lt;W&gt;](bus.md) | Routers per master, an arbiter tree per shared target, built from a bus description |
| [5.2.3. BusBridge&lt;W&gt;](bridge.md) | A target on one bus that is a master on another; pass-through or registered |
| [Checking the rules](#checking-the-rules-busportcheckerw) | `BusPortChecker<W>`, a passive observer on any link |

## The bus port

Every connection in the computer is the transaction port of
[§ 3.4](../../memory/transactions.md). One side asks (the **master**: the
core's fetch port, its data port, a bridge's downstream side); the other side
answers (the **target**: memory, a device, a bridge's upstream side).

| Signal | From | Width | Meaning |
| --- | --- | ---: | --- |
| `REQ_VALID`, `REQ_OP`, `REQ_ADDR`, `REQ_WRITE_DATA`, `REQ_WRITE_MASK` | Master | 1, 3, 32, W, W/8 | Operation (0 read, 1 write), byte address, data, one mask bit per byte |
| `REQ_READY` | Target | 1 | The request is taken at this edge |
| `RSP_VALID`, `RSP_READ_DATA`, `RSP_STATUS` | Target | 1, W, 3 | The answer: data and status |
| `RSP_READY` | Master | 1 | The answer is taken at this edge |

W is 8, 16 or 32; the machines in this chapter use 32. A device's input
port has the opposite directions to its master's output port. All parts use
the system's `CLK` and active-high `RST`; a bridge does not introduce another
clock domain. Bits and byte masks are little-endian as in § 3.4.

The rules that let any target stand in for any other:

1. A transfer happens at a rising clock edge where VALID and READY are both 1.
2. A master holds a request unchanged until it is taken; a target holds a
   response unchanged until it is taken.
3. One request at a time per port: the next request follows the answer.
4. A request that cannot be done is still answered, with a status, and has
   no other effect.

| `RSP_STATUS` | Name | Given by |
| ---: | --- | --- |
| 0 | OK | |
| 1 | Alignment | An address not aligned to W/8 bytes (4 in these machines) |
| 2 | Range | No target owns the address (the bus), or the address is past a memory's end |
| 3 | ReadOnly | A write to a read-only register |
| 4 | Unsupported | An operation the target does not have (a read of a write-only register, a burst to a device, or a write on a fetch router) |
| 6 | Invalid | Unknown request bits, or a written value that does not fit its register |

Fetch masters have no route to non-executable windows, so a fetch there
answers **Range**, without reaching the device. The general memory protocol
also defines program, erase and burst operations; these bus routers accept
only reads and writes. Cache-fill bursts stay inside the memory subsystem.

## Windows

Each target owns a **window**: a base address and a size that is a power
of two, with the base a multiple of the size. A request belongs to the
window when its address satisfies `base ≤ address < base + size`; because
of the alignment rule, that is "the top bits of the address equal the top
bits of the base", one comparison.

A target usually sees the **offset** in its window: a read of `0x10002004`
reaches the buttons as address 4, so a device does not need to know where it
was placed. A window can instead pass the **whole address**, which is what a
bridge to another bus needs.

### Example: where does each request go?

With the default map ([§ 5](../index.md#address-map)):

| Request | Window | Target sees | Answer |
| --- | --- | --- | --- |
| data read `0x00001F00` | RAM | `0x00001F00` | the memory's word |
| data write `0x10000000` | CONSOLE | `0x0` (the TX register) | OK; a byte is queued |
| data read `0x10002008` | BUTTONS | `0x8` (COUNT) | number of waiting events |
| data read `0x10000100` | none (between windows) | — | Range, from the bus |
| fetch `0x10000000` | CONSOLE, but not executable | — | Range, from the bus; the core traps |
| data write `0x20000010` | VIDEO | `0x10` | OK; four pixels written |

## Checking the rules: `BusPortChecker<W>`

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| `BusPortChecker<W>` | Flags changed waiting transfers and responses without an outstanding request | Yes | No |

Catalog: `bus.port-checker.W` for W = 8, 16, 32. A checker is test
equipment: it only reads the link. A machine whose description has
`"check_ports": true` gets one on every bus link (`CHECK_<BUS>_<LINK>`) and an
output `PORT_VIOLATION`, the OR of all their flags; the game runner stops a
run the moment it rises.

### IO pins

| Pin | Direction | Width | Meaning |
| --- | --- | ---: | --- |
| `CLK`, `RST` | Input | 1 each | Clock; reset clears the flags |
| every link signal (`REQ_*`, `RSP_*`) | Input | as on the link | The observed link |
| `REQUEST_CHANGED` | Output | 1 | A waiting request was withdrawn or changed before it was taken |
| `RESPONSE_CHANGED` | Output | 1 | A waiting answer was withdrawn or changed before it was taken |
| `EXTRA_RESPONSE` | Output | 1 | An answer was taken with no request outstanding |
| `VIOLATION` | Output | 1 | Any of the three; all flags stay set until reset |

### Implementation

| Part | Contents |
| --- | --- |
| `REQUEST_WATCH`, `RESPONSE_WATCH` | Each field of the request (answer) captured at every clock edge in a `Register`, one `XORGate<w>` per field comparing it with the value now; `CHANGED` is the OR of all differences |
| `HANDSHAKE` | State bits for "the request (answer) was waiting at the last edge" and "a response is owed" (a burst keeps it until the next request); `EXTRA` is an answer taken with nothing owed |
| `FLAGS` | A waiting transfer withdrawn or changed, and `EXTRA`, each held in a sticky register; `VIOLATION` is their OR |

It samples at clock edges; it does not check analog setup/hold, progress or
timeout, every `X`/`Z` field, a second request accepted while another is
outstanding, or the number of words in a burst. A clean flag is evidence
for these implemented checks, not a complete protocol proof. The examples
use binary controls after reset.

### Examples

One link, watched: a request waits and is taken, its answer is taken, then a
new request changes its address while waiting. `REQUEST_CHANGED` and
`VIOLATION` rise and stay set until reset.

<!-- simulator-examples: bus-port-checker BusPortCheckerScenarioTest -->

<div class="simulator-embed">
  <iframe src="../../simulator/?scenario=bus-port-checker&amp;embed=1" title="BusPortChecker&lt;32&gt; simulation" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../simulator/?scenario=bus-port-checker" class="md-button">Open the full visualizer</a>

There is no separate compact checker or stored delay table. Its registers
and comparisons are included when timing a checked system; enabling it is
an observation configuration, not a zero-cost timing assumption.

Source: [port definition](https://github.com/arnold518/reinventing-the-wheel/blob/alpha/include/modules/memory/MemoryProtocol.hpp),
[checker circuit](https://github.com/arnold518/reinventing-the-wheel/blob/alpha/src/system/BusPortChecker.cpp),
[directed checker cases](https://github.com/arnold518/reinventing-the-wheel/blob/alpha/src/tests/SystemTests.cpp).

### Tests

| Test | Coverage |
| --- | --- |
| `BusPortCheckerTest` | A clean transfer stays clean; a withdrawn request, a changed address, a changed answer and an answer with no request each raise their flag |
| `BusPortCheckerScenarioTest` | The recorded example |
| `ComputerSystemPortCheckTest` | The bring-up program on `games-machine-checked` (the two-bus machine with a checker on all eleven links) runs with no violation |
