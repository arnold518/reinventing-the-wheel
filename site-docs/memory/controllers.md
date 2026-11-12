# § 3.5. Controllers and Endpoints

A controller turns [transactions](transactions.md) into the native interface
of one technology from [3.3](technologies/index.md). It owns request and
response state, never memory contents. An endpoint is a controller and one
device of the same technology and geometry. An arbiter lets two requesters
share one endpoint (or cache).

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| `MemoryController<T,W>` | Zero-wait flip-flop path, or the clocked sequencing of a delayed technology | Yes | Yes |
| `MemoryEndpoint<T,W>` | Controller plus native device, or one fused behavioral endpoint | Yes | Yes |
| `TwoPortMemoryArbiter<W>` | Two requesters sharing one lower memory, one request at a time | Yes | No |

T is `ff`, `sram`, `dram`, `rom` or `nor-flash`; W is 8, 16 or 32. The angle
brackets name dimensions in visualizer labels; the C++ constructors take width
and technology as arguments.

All RV32I cores use these endpoints. The single-cycle core and the baseline
pipeline use zero-wait flip-flop endpoints; the advanced pipeline can use any
technology. For flip-flop storage the controller adds no cycle. For the other
technologies it counts the cycles the device needs at its clock, so every
signal the requester sees changes just after a clock edge.

## Configuration

Controller and device must agree on depth, row and block geometry; an
endpoint builds matching pairs. A standalone controller must be connected to
a device built with the same configuration.

| Parameter | Default | Meaning |
| --- | ---: | --- |
| `depth_words` | 4 | Addressable words; capacity is N×W/8 bytes |
| `words_per_row` | 2 | Matrix words per row: a power of two dividing N, at most 16 at gate level and 1,024 compact |
| `block_words` | 2 | NOR erase block (whole rows dividing N); burst length for SRAM, ROM and DRAM (a power of two up to 16 dividing N; for DRAM within a row) |
| `access_cycles` | 2 | SRAM and ROM: clock edges from the accepting edge to the capture, 1…255. Experiments choose it from the proven access time at their clock (`staticAccessCycles`) |
| `refresh_interval_cycles` | 96 | DRAM: clock cycles between refreshes; 0 turns refresh off, otherwise at least 24. Experiments use tREFI divided by the period |
| `clocked` | 0 | SRAM only: 1 selects the on-chip clocked macro |
| `always_ready` | 0 | Flip-flop only: the consumer keeps `RSP_READY` = 1, so no response is held |
| `program_ticks`, `erase_ticks` | From the profile | NOR program and erase pulses |
| `retention_ticks` | From the profile | DRAM retention of the compact endpoint |
| `image_hex` | Empty | ROM construction image; the rest is zero |
| `phase_ticks`, `guard_ticks`, `refresh_interval_ticks` | 32, 64, 10,000 | Accepted for compatibility and still range-checked (a refresh schedule must cover every row within retention); they no longer set any timing |

Gate-level endpoint storage is limited to 4,096 cells. Request
`memory.<technology>.scalable-endpoint.<width>` instead of
`memory.<technology>.endpoint.<width>` to keep the controller visible while
the device uses its compact form; this removes the cell limit, not the
technology's timing or host resource limits. SRAM and ROM chips stay
asynchronous: the controller is clocked, their pins are not.

## 3.5.1. `MemoryController<T,W>`

Connects a requester to an external native device.

### IO pins

The requester side is exactly the [common protocol](transactions.md#protocol-rules):
`CLK`, `RST`, `REQ_VALID`, `REQ_READY`, `REQ_OP`, `REQ_ADDR`,
`REQ_WRITE_DATA`, `REQ_WRITE_MASK`, `RSP_VALID`, `RSP_READY`,
`RSP_READ_DATA`, `RSP_STATUS`. The device side uses the native pin names with
a `MEM_` prefix; directions below are relative to the controller.

| Technology | Controller outputs → device inputs | Device outputs → controller inputs |
| --- | --- | --- |
| FF | `MEM_WORD_ADDR<32>`, `MEM_WRITE_DATA<W>`, `MEM_WRITE_MASK<W/8>`, `MEM_CLK`, `MEM_RST`, `MEM_WE`, `MEM_READ_EN` | `MEM_READ_DATA<W>`, `MEM_FAULT` |
| SRAM | `MEM_WORD_ADDR<32>`, `MEM_WRITE_DATA<W>`, `MEM_WRITE_MASK<W/8>`, `MEM_CE_N`, `MEM_WE_N`, `MEM_OE_N`; `MEM_CLK` for the clocked macro | `MEM_READ_DATA<W>`, `MEM_READY` |
| ROM | `MEM_WORD_ADDR<32>`, `MEM_CE_N`, `MEM_OE_N` | `MEM_READ_DATA<W>`, `MEM_READY` |
| DRAM | `MEM_ADDR<32>`, `MEM_WRITE_DATA<W>`, `MEM_WRITE_MASK<W/8>`, `MEM_CLK`, `MEM_CMD<3>` | `MEM_READ_DATA<W>`, `MEM_BUSY`, `MEM_ERROR`, `MEM_READ_VALID`, `MEM_ROW_OPEN` |
| NOR flash | `MEM_WORD_ADDR<32>`, `MEM_WRITE_DATA<W>`, `MEM_WRITE_MASK<W/8>`, `MEM_CLK`, `MEM_CMD<3>` | `MEM_READ_DATA<W>`, `MEM_BUSY`, `MEM_ERROR`, `MEM_READ_VALID` |

Unannotated controls and status pins are one bit. The flip-flop storage's
native reset is held low: a protocol reset does not clear its contents.

### Behavior

Every request is checked first ([status order](transactions.md#protocol-rules));
an invalid request completes with its status and never reaches the device.

| Technology | What a valid access does |
| --- | --- |
| FF | Checked and answered in the same cycle: a read through the device's read path, a write at the shared clock edge; the response bypasses the empty buffer |
| SRAM | Drives chip, write and output enables and the held fields for `access_cycles` edges, then captures the data; status `Device` if `MEM_READY` is not high at that edge. The clocked macro is shown the request during the accepting cycle and counts from that edge |
| ROM | As SRAM, read only; a write gets `ReadOnly` |
| DRAM | Uses the open row if it matches; otherwise PRECHARGE if another row is open, then ACTIVATE, then READ, WRITE or READ_BURST; each command is shown when the device is idle and the previous one ended without fault |
| NOR flash | READ; or WRITE ENABLE followed by PROGRAM or ERASE BLOCK; waits for `BUSY` to fall, checks `ERROR` and `READ_VALID` |

A DRAM refresh is due when the refresh counter expires; it is served from
idle (PRECHARGE if a row is open, then REFRESH) while new requests wait, and
also while a response is held under backpressure, so traffic cannot starve
it. Stopping the clock stops refresh; data can then expire, and REFRESH cannot
recover it. A reset during an operation already sent to the device suppresses
its response but lets the operation finish; it does not undo a write.

#### C++ example

The request check every controller applies, as a function of the request
fields. `known_*` say whether a field has no X or Z bits. The result is the
`RSP_STATUS` code; zero means the device is accessed.

```cpp
#include <cstdint>
enum class Tech { FF, SRAM, DRAM, ROM, NOR };
unsigned request_status(Tech t, unsigned width, std::uint64_t depth_words,
                        unsigned block_words, unsigned words_per_row,
                        bool known_op, unsigned op, bool known_address,
                        std::uint32_t address, bool known_mask) {
    enum { Ok, Alignment, Range, ReadOnly, Unsupported, Device, Invalid };
    if (!known_op || !known_address) return Invalid;
    const bool burst = op == 4;
    if (burst && (t == Tech::FF || t == Tech::NOR ||
                  (t == Tech::DRAM && block_words > words_per_row)))
        return Unsupported;
    if (op > 4) return Unsupported;
    if ((op == 1 || op == 2) && !known_mask) return Invalid;
    if (address % (width / 8)) return Alignment;
    const std::uint64_t word = address / (width / 8);
    if (word >= depth_words) return Range;
    if (burst) return Ok;
    if (t == Tech::ROM && op != 0) return ReadOnly;
    if (t == Tech::NOR) {
        if (op == 1) return Unsupported;
        if (op == 3 && word % block_words) return Alignment;
    } else if (op > 1) {
        return Unsupported;
    }
    return Ok;
}
```

#### Unknown inputs

Handshake controls must be binary. An unknown operation, address or required
mask returns status 6 (`InvalidRequest`); unknown read contents stay X and are
not a readiness signal. The controller does not guess completion from
`READ_DATA` and has no timeout that could retry a store.

### Implementation

#### Structural implementation

**Flip-flop:** `REQUEST_CHECK` validates the address and operation with gates
(a balanced high-address-bit check for power-of-two capacities, a constant
comparison otherwise); `RESPONSE` is the fall-through response buffer, with a
few gates for readiness and status. With `always_ready=1` the response goes
straight out in the same cycle and no payload is stored; the fixed-cycle RV32I
cores use this.

**Delayed technologies:** `TRANSACTION` is the
[`MemoryTransactionPort`](transactions.md#341-memorytransactionportw) and
`DEVICE_CONTROL` the technology's sequencer, built from gates and flip-flops:
[`StaticMemorySequencer`](building-blocks.md#staticmemorysequencer) for SRAM
and ROM, [`CommandMemorySequencer`](building-blocks.md#commandmemorysequencer)
for DRAM and NOR flash, each with a
[`MemoryRequestCheck`](building-blocks.md#memoryrequestcheck). The command
devices answer with registered status on the shared clock, so the sequencer
reads `MEM_BUSY` directly; read data and `MEM_READY` pass through a
[`ProtocolStableData`](building-blocks.md#poweronreset-and-protocolstabledata)
marker, because they are captured only at the counted edge or after `BUSY`
falls.

#### Behavioral implementation

[`MemoryControllerBehavioral`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/modules/memory/MemoryController.cpp) is the whole controller as one part. It keeps the same state as the structural form (one pending request, one held response, the sequencer's state) and the same rules, so its native pin activity is the same cycle for cycle; it owns no device and no storage. An endpoint requests its controller through the catalog, so a profile can show it as gates or as this one part; `MemoryEndpointContractTest` compares the two in front of the same device for all five technologies.

**Timing.** Everything the controller drives to the device and back to the requester
comes from its registers, one clock-to-output delay after the edge; every
request field must be stable a setup time before the edge. The address needs
the longest setup, because the controller checks it (in range, aligned) before
the edge decides; `REQ_ADDR → RSP_STATUS` is that check. The flip-flop
controller also passes a read straight through in the same cycle, so it has a
path from the address to the read data.

<!-- timing-rows
FF: address check | memory.ff.controller.8.behavioral | REQ_ADDR → RSP_STATUS
FF: address to read data | memory.ff.controller.8.behavioral | REQ_ADDR → RSP_READ_DATA
FF: address, setup | memory.ff.controller.8.behavioral | REQ_ADDR before CLK ↑
SRAM: address, setup | memory.sram.controller.8.behavioral | REQ_ADDR before CLK ↑
SRAM: edge to chip enable | memory.sram.controller.8.behavioral | CLK ↑ → MEM_CE_N
SRAM: edge to response | memory.sram.controller.8.behavioral | CLK ↑ → RSP_VALID
-->

### Examples

<!-- simulator-examples: memory-controller MemoryControllerTest -->

Standalone FF controller with an externally driven `MEM_READ_DATA=0xA5`, first reading that value and then writing `0x3C`. The mock device pins make the controller boundary visible; full real-device connections follow below. The complete table includes every request, wait, backpressure, and consume checkpoint.

#### Embedded simulation

<div class="simulator-embed">
  <iframe src="../../simulator/?scenario=memory-controller" title="MemoryController recorded simulation" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../simulator/?scenario=memory-controller" class="md-button">Open MemoryController in the simulator</a>

### Source and tests

- [Public interface: `MemoryController.hpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/include/modules/memory/MemoryController.hpp)
- [Structural wiring: `MemoryController.cpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/modules/memory/MemoryController.cpp)
- [Sequencers as gates: `MemorySequencers.cpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/modules/memory/MemorySequencers.cpp)
- `MemoryControllerTest`, `MemoryEndpointContractTest`, and `MemoryEndpointTimingTest` in [MemoryEndpointTests.cpp](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/tests/MemoryEndpointTests.cpp).
- `MemoryEndpointIntegrationTest` compiles the request check above and compares it with the production check over every operation, alignment and range case for all five technologies.

## 3.5.2. `MemoryEndpoint<T,W>`

Assembly of `CONTROLLER` and `MEMORY`. Unlike a standalone controller, it
contains storage. Its fused behavioral implementation is
`BehavioralMemoryEndpoint`, replacing both at this boundary. Selecting the
[native device's behavioral form](technologies/scalable.md) instead keeps the
controller visible and the device's pins.

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `CLK`, `RST`, `REQ_VALID`, `RSP_READY` | Input | 1 | Shared clock, interface reset, handshakes |
| `REQ_OP` | Input | 3 | Operation |
| `REQ_ADDR` | Input | 32 | Aligned byte address |
| `REQ_WRITE_DATA` | Input | W | Positioned data |
| `REQ_WRITE_MASK` | Input | W/8 | Byte enables |
| `REQ_READY`, `RSP_VALID` | Output | 1 | Request acceptance / response available |
| `RSP_READ_DATA` | Output | W | Result |
| `RSP_STATUS` | Output | 3 | Status |

### Behavior

The protocol and the technology's rules apply unchanged; the native addresses,
enables and command status stay visible inside the assembled endpoint.

#### C++ example

What an endpoint answers to each accepted request, as values: the responses
in order, each with its data and status. Storage is a byte array of
`depth_words × W/8` bytes; `status` comes from the request check above.

```cpp
#include <cstdint>
#include <vector>
struct EndpointResponse { std::uint32_t data; unsigned status; };
std::vector<EndpointResponse> endpoint_responses(
        Tech t, unsigned width, unsigned block_words, unsigned words_per_row,
        std::vector<std::uint8_t>& bytes, unsigned op, std::uint32_t address,
        std::uint32_t data, unsigned mask) {
    const unsigned size = width / 8;
    const std::uint64_t depth = bytes.size() / size;
    const unsigned status = request_status(t, width, depth, block_words, words_per_row,
                                           true, op, true, address, true);
    if (status) return {{0, status}};
    const std::uint64_t word = address / size;
    const auto read = [&](std::uint64_t w) {
        std::uint32_t value = 0;
        for (unsigned b = 0; b < size; ++b) value |= std::uint32_t(bytes[w * size + b]) << (8 * b);
        return value;
    };
    std::vector<EndpointResponse> out;
    if (op == 0) out.push_back({read(word), 0});
    if (op == 4) // The aligned block, from this word on, wrapping inside it.
        for (unsigned i = 0; i < block_words; ++i) {
            const std::uint64_t base = word - word % block_words;
            out.push_back({read(base + (word - base + i) % block_words), 0});
        }
    for (unsigned b = 0; b < size && (op == 1 || op == 2); ++b)
        if (mask >> b & 1u) {
            const auto byte = std::uint8_t(data >> (8 * b));
            auto& stored = bytes[word * size + b];
            stored = op == 1 ? byte : std::uint8_t(stored & byte); // PROGRAM clears bits only.
        }
    if (op == 3)
        for (std::uint64_t i = 0; i < std::uint64_t(block_words) * size; ++i)
            bytes[word * size + i] = 0xff;
    if (op != 0 && op != 4) out.push_back({0, 0});
    return out;
}
```

#### Unknown inputs

The endpoint adds no transaction rules of its own. Structural and compact
forms agree at the recorded transaction checkpoints, not at every internal
gate change. Unknown data and error results follow [3.4](transactions.md).

### Implementation

#### Structural implementation

Controller on the left, native device on the right. The device keeps its own
word, row and column layout and pins, and its children can choose their own
behavioral forms in the explorer.

#### Behavioral implementation

Each technology's endpoint can instead be the fused `BehavioralMemoryEndpoint`
leaf: one controller state and one compact device model, stepped edge for
edge as the gates would be, so it takes the same cycles
(`MemoryLatencyGatesTest` compares the [measured latencies](timing.md#measured-latency)).
A standalone behavioral native device still needs its controller to reach
transaction pins.

The `memory.<technology>.scalable-endpoint.<width>` contracts default to the
fused endpoint. Their structural selection shows `CONTROLLER` and a compact
native `MEMORY`; it never expands a full-size store into cells, so CPU memories
can be expanded in the explorer without allocating thousands of cells. An
image can be provided before simulation; Python exposes this as
`initialize_memory_bytes(component, byte_address, bytes)`. Runtime writes must
use the protocol.

**Timing.** An endpoint's latency in clock cycles depends on its technology and, for
DRAM, on the row state. Measured on 256 KiB memories at the advanced core's
clock:

<!-- latency-table: all -->

#### Visualization and inspection

The memory table is a read-only view of recorded storage and signal history. Python and WebAssembly use the same inspector. Inspection and timeline scrubbing do not send transactions or advance the device.

| Display | Meaning |
|---------|---------|
| Address and highlighted row | The live offered request, or the accepted request while its response is pending/held |
| `READ`, `WRITE`, `PROGRAM`, `ERASE` | Operation belonging to that request; read is green and modifying operations are orange |
| `READY`, `WAIT`, `PENDING`, `RESPONSE`, `HELD` | Request/response phase; `HELD` means the consumer has not accepted the response |
| Accepted time, write data, byte mask | Original accepted payload, not later values on request pins |
| Device phase and maintenance | Recorded native sequencing activity, including automatic refresh; not an extra controller |
| Instruction text | RV32I disassembly for an instruction-memory endpoint and its nested native device |

Internal activity timestamps are included in the visualizer timeline even when
the endpoint's external bus has not changed. Only signals belonging to that
technology's native interface are listed. This observation history adds no
simulated registers, events, or memory latency.

The observer samples handshake and payload signals **before the rising edge**, as required by the synchronous interface. It handles a request and response completing on the same edge without inventing a pending cycle. For a delayed response, later changes to `REQ_ADDR`, `REQ_OP`, or write data do not change the displayed accepted request. Reset drops that transaction context, not the stored image. In a CPU instruction memory, the highlight identifies the **fetch address**, not the retired instruction or every occupied pipeline stage.

`Follow access` centers the table on the active word. Manual row scrolling suspends centering. The inspector's byte-address field, `Previous page`, and `Next page` browse the entire device; they read history directly without performing a hardware read. Address pages contain at most 256 words per request (64 by default). The sparse touched-word summary is limited to 256 entries, but this does **not** limit address browsing. Unknown/unwritten values remain visible according to the device's storage model.

### Examples

#### FF endpoint

<!-- simulator-examples: memory-endpoint-ff MemoryEndpointFFTest -->

Writes/reads the first and last words, checks zero and partial byte masks, rejects unsupported programming, out-of-range accesses and unknown masks, then writes/reads unknown data. Reads ignore unknown write fields. All response-hold checkpoints are included.

<div class="simulator-embed"><iframe src="../../simulator/?scenario=memory-endpoint-ff" title="FF memory endpoint" loading="lazy" allowfullscreen></iframe></div>

<a href="../../simulator/?scenario=memory-endpoint-ff" class="md-button">Open FF endpoint</a>

#### SRAM endpoint

<!-- simulator-examples: memory-endpoint-sram MemoryEndpointSRAMTest -->

The same load/store sequence through native asynchronous chip/write/output controls. The bus clock belongs to the controller, not the SRAM cells.

<div class="simulator-embed"><iframe src="../../simulator/?scenario=memory-endpoint-sram" title="SRAM memory endpoint" loading="lazy" allowfullscreen></iframe></div>

<a href="../../simulator/?scenario=memory-endpoint-sram" class="md-button">Open SRAM endpoint</a>

#### DRAM endpoint

<!-- simulator-examples: memory-endpoint-dram MemoryEndpointDRAMTest -->

First/last-word accesses cross rows. The controller inserts activation and precharge commands and services refresh while clocks continue. The timed test additionally holds a response longer than retention and tests stopping the clock.

<div class="simulator-embed"><iframe src="../../simulator/?scenario=memory-endpoint-dram" title="DRAM memory endpoint" loading="lazy" allowfullscreen></iframe></div>

<a href="../../simulator/?scenario=memory-endpoint-dram" class="md-button">Open DRAM endpoint</a>

#### ROM endpoint

<!-- simulator-examples: memory-endpoint-rom MemoryEndpointROMTest -->

Reads `0xA5` from the construction image and rejects writes without changing contents. Capacity and reserved-operation checks still apply.

<div class="simulator-embed"><iframe src="../../simulator/?scenario=memory-endpoint-rom" title="ROM memory endpoint" loading="lazy" allowfullscreen></iframe></div>

<a href="../../simulator/?scenario=memory-endpoint-rom" class="md-button">Open ROM endpoint</a>

#### NOR-flash endpoint

<!-- simulator-examples: memory-endpoint-flash MemoryEndpointFlashTest -->

Programs `0x81` and `0x3C` into separate blocks, demonstrates that programming ones cannot restore cleared bits, rejects an ordinary RAM WRITE, erases the first block, and verifies the other block survives.

<div class="simulator-embed"><iframe src="../../simulator/?scenario=memory-endpoint-flash" title="NOR flash memory endpoint" loading="lazy" allowfullscreen></iframe></div>

<a href="../../simulator/?scenario=memory-endpoint-flash" class="md-button">Open NOR-flash endpoint</a>

### Source and tests

- [Assembly and native wiring: `MemoryController.cpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/modules/memory/MemoryController.cpp)
- [Catalog: `MemoryEndpointCatalog.cpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/modules/memory/MemoryEndpointCatalog.cpp)
- [All endpoint scenarios and contracts: `MemoryEndpointTests.cpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/tests/MemoryEndpointTests.cpp)

`MemoryEndpointContractTest` compares all five technology endpoints at widths 8/16/32. `MemoryEndpointTimingTest` checks held responses, interface reset without storage clearing, reset during flash programming, refresh under backpressure, and retention loss after stopping the clock. `FFAlwaysReadyControllerTest` checks the narrower, unbuffered FF response contract. RV32I integration is checked separately by `RV32IMemoryMigrationTest`: all three cores use this common port. The strict single-cycle core and baseline pipeline require zero-wait FF endpoints and use the always-ready response path; the advanced pipeline retains the buffered endpoint for backpressure. Neither integration nor these endpoint tests imply cache/coherence support.

`MemoryInspectorTest`, `MemoryInspectorIntegrationTest.py`, and `MemoryInspectorBrowserTest.py` check accepted-request tracking, same-edge completion, rewind, native/Python/WASM agreement, fetched-instruction and data-access highlights, and address paging. These are observation tests, separate from the hardware contract tests above.

[`MemoryEndpointIntegrationTest`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/tests/MemoryEndpointIntegrationTest.py) also compiles the exact documentation snippets and runs the [reference checks](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/tests/fixtures/memory/transaction-controller-examples.cpp): the transaction port at W=8/16/32, the request check against the production check, and the endpoint reference's reads, writes, bursts, programs and erases.

## 3.5.3. `TwoPortMemoryArbiter<W>`

Lets two requesters, called I and D (a processor's instruction and data
ports), share one lower memory: an endpoint or a cache. It passes on one
request at a time and returns each response to the port that asked.

### IO pins

Three [transaction ports](transactions.md#protocol-rules) of width W:

| Pins | Direction | Width | Description |
| --- | --- | ---: | --- |
| `I_CLK`, `I_RST`, `D_CLK`, `D_RST` | Input | 1 each | Both requesters' clock and reset. Both run on one clock: the arbiter is clocked by `I_CLK`, and `D_CLK` must carry the same clock (it is not read). Either reset resets the arbiter |
| `I_REQ_VALID`, `I_REQ_OP`, `I_REQ_ADDR`, `I_REQ_WRITE_DATA`, `I_REQ_WRITE_MASK`, `I_RSP_READY` | Input | 1, 3, 32, W, W/8, 1 | Port I's request and its readiness for a response |
| `I_REQ_READY`, `I_RSP_VALID`, `I_RSP_READ_DATA`, `I_RSP_STATUS` | Output | 1, 1, W, 3 | Port I's handshake and response |
| `D_…` | Input / Output | as for I | Port D, the same pins |
| `MEM_CLK`, `MEM_RST` | Output | 1 each | The shared clock and reset, passed on to the lower memory |
| `MEM_REQ_VALID`, `MEM_REQ_OP`, `MEM_REQ_ADDR`, `MEM_REQ_WRITE_DATA`, `MEM_REQ_WRITE_MASK`, `MEM_RSP_READY` | Output | 1, 3, 32, W, W/8, 1 | The selected request, and readiness for its response |
| `MEM_REQ_READY`, `MEM_RSP_VALID`, `MEM_RSP_READ_DATA`, `MEM_RSP_STATUS` | Input | 1, 1, W, 3 | The lower memory's handshake and response |

The construction parameter `block_words` (a power of two up to 16, default 2)
is the number of words the lower memory returns for a burst read. The
parameter `pipelined` (default `0`) selects the pipelined arbiter of
[§ 6.3.2](../accelerator/6.3.md#632-the-pipelined-bus); the rules below are
those of the default arbiter.

### Behavior

- **One request at a time.** While idle, the arbiter offers one port's
  request to the lower memory; it is busy from the edge the lower memory
  accepts it until its response arrives. A burst read (operation 4) keeps it
  busy until the last of its `block_words` words.
- **Who goes first.** If only one port asks, it is offered. If both ask, the
  port that was not granted last goes first (round robin); after reset, I
  goes first. A port's `REQ_READY` is high only when its request is the one
  offered and the lower memory is ready.
- **Responses.** Each response goes to the port that owns the current
  request. Each port has a one-entry response buffer: a response leaves the
  lower memory as soon as its owner's buffer is empty, even if that port is
  not ready to take it, so a port that is busy elsewhere (a processor stalled
  on its other port) cannot hold the lower memory. A port that is ready takes
  the response straight through, in the same cycle.
- **Reset** clears the busy, owner, priority and burst state and both buffers,
  and is passed on as `MEM_RST`. A response that arrives after reset is not
  delivered.

#### C++ example

The arbiter as a function of its inputs, one call per settled state. It
updates its state when `I_CLK` rises; outputs follow the current inputs and
state. Values are binary.

```cpp
#include <cstdint>
namespace arbiter_example {
using U = std::uint32_t;
struct Port { U REQ_VALID = 0, REQ_OP = 0, REQ_ADDR = 0, REQ_WRITE_DATA = 0,
              REQ_WRITE_MASK = 0, RSP_READY = 0; };
struct Inputs { U I_CLK = 0, I_RST = 0, D_RST = 0; Port I, D;
                U MEM_REQ_READY = 0, MEM_RSP_VALID = 0, MEM_RSP_READ_DATA = 0,
                  MEM_RSP_STATUS = 0; };
struct Response { U VALID = 0, READ_DATA = 0, STATUS = 0; };
struct Outputs { U I_REQ_READY = 0, D_REQ_READY = 0, MEM_REQ_VALID = 0, MEM_REQ_OP = 0,
                   MEM_REQ_ADDR = 0, MEM_REQ_WRITE_DATA = 0, MEM_REQ_WRITE_MASK = 0,
                   MEM_RSP_READY = 0, MEM_CLK = 0, MEM_RST = 0;
                 Response I, D; };
class Arbiter {
public:
    explicit Arbiter(U block_words) : block_(block_words) {}
    Outputs evaluate(const Inputs& in) {
        const bool reset = in.I_RST || in.D_RST;
        if (reset) state_ = {};
        else if (in.I_CLK && !clock_) edge(in, combine(in));
        clock_ = in.I_CLK;
        return combine(in);
    }
private:
    struct Buffer { bool full = false; U data = 0, status = 0; };
    struct State { bool busy = false, owner_d = false, prefer_d = false, burst = false;
                   U beat = 0; Buffer i, d; } state_;
    U block_;
    bool clock_ = false;
    Outputs combine(const Inputs& in) const {
        Outputs out;
        const bool idle = !state_.busy;
        const bool select_d = in.D.REQ_VALID && (state_.prefer_d || !in.I.REQ_VALID);
        const Port& chosen = select_d ? in.D : in.I;
        out.MEM_REQ_VALID = idle && (in.I.REQ_VALID || in.D.REQ_VALID);
        out.MEM_REQ_OP = chosen.REQ_OP;
        out.MEM_REQ_ADDR = chosen.REQ_ADDR;
        out.MEM_REQ_WRITE_DATA = chosen.REQ_WRITE_DATA;
        out.MEM_REQ_WRITE_MASK = chosen.REQ_WRITE_MASK;
        out.I_REQ_READY = idle && !select_d && in.MEM_REQ_READY;
        out.D_REQ_READY = idle && select_d && in.MEM_REQ_READY;
        const Buffer& owner = state_.owner_d ? state_.d : state_.i;
        out.MEM_RSP_READY = state_.busy && !owner.full;
        const bool arriving = out.MEM_RSP_READY && in.MEM_RSP_VALID;
        auto respond = [&](const Buffer& held, bool mine) {
            const bool incoming = arriving && mine;
            return Response{held.full || incoming, held.full ? held.data : in.MEM_RSP_READ_DATA,
                            held.full ? held.status : in.MEM_RSP_STATUS};
        };
        out.I = respond(state_.i, !state_.owner_d);
        out.D = respond(state_.d, state_.owner_d);
        out.MEM_CLK = in.I_CLK;
        out.MEM_RST = in.I_RST || in.D_RST;
        return out;
    }
    void edge(const Inputs& in, const Outputs& out) {
        const bool select_d = in.D.REQ_VALID && (state_.prefer_d || !in.I.REQ_VALID);
        const bool accept = out.MEM_REQ_VALID && in.MEM_REQ_READY;
        const bool complete = out.MEM_RSP_READY && in.MEM_RSP_VALID;
        const bool last = state_.beat == block_ - 1;
        const bool finished = complete && (!state_.burst || last);
        auto keep = [&](Buffer& buffer, const Response& shown, bool mine, U ready) {
            if (buffer.full) { if (ready) buffer.full = false; }
            else if (complete && mine && !ready) buffer = {true, shown.READ_DATA, shown.STATUS};
        };
        keep(state_.i, out.I, !state_.owner_d, in.I.RSP_READY);
        keep(state_.d, out.D, state_.owner_d, in.D.RSP_READY);
        if (accept) {
            state_.busy = true; state_.owner_d = select_d; state_.prefer_d = !select_d;
            state_.burst = out.MEM_REQ_OP == 4; state_.beat = 0;
        } else {
            if (finished) state_.busy = false;
            if (complete && state_.burst) state_.beat = (state_.beat + 1) % block_;
        }
    }
};
} // namespace arbiter_example
```

#### Unknown inputs

The handshake inputs must be binary. An unknown `REQ_VALID` makes the grant
unknown, and with it the readiness outputs and the selected request fields.
Request fields and responses are passed through unchanged: an unknown address
or read-data bit stays unknown on the other side. An unknown reset can make
the arbiter's state unknown.

### Implementation

#### Structural implementation

<!-- diagram: memory-arbiter -->

| Part | Responsibility |
| --- | --- |
| `GRANT` | Chooses the port to offer (D if it asks and has priority or I is not asking), offers it while idle, and drives `I_REQ_READY`, `D_REQ_READY` and the accept signal |
| `SELECT_REQ_OP`, `SELECT_REQ_ADDR`, `SELECT_REQ_WRITE_DATA`, `SELECT_REQ_WRITE_MASK` | One multiplexer per request field, switched by the grant |
| `STATE` | Clocked bits: busy, owner (I or D), priority, burst, and a burst word counter of log₂(`block_words`) bits, with their next-state logic |
| `RESPONSE_ROUTE` | Takes a response while the owner's buffer is empty and steers it to that buffer; signals completion to `STATE` |
| `I_RESPONSE`, `D_RESPONSE` | One [`MemoryResponseBuffer`](transactions.md#342-memoryresponsebufferw) per port |
| `RESET`, `LOWER_CLOCK`, `LOWER_RESET` | The shared reset, and the clock and reset passed on to the lower memory |

All state is clocked straight from `I_CLK`: a gate in the clock path, such as
an AND of both clocks, would delay the arbiter's edge behind the requesters'.
There is no behavioral form; the arbiter is small, and the experiments
simulate it as gates.

### Examples

<!-- simulator-examples: memory-arbiter MemoryArbiterTest -->

Both ports ask while the lower memory is busy; the tie goes to I. I's response
goes into I's buffer although I is not ready, so D is offered at once. D's
write is granted and its response returns with its status. After that grant,
a tie goes to I again. A two-word burst read for D then holds the arbiter
while I waits; I is offered as soon as the last word has gone. The lower
memory's responses are driven by the scenario.

#### Embedded simulation

<div class="simulator-embed">
  <iframe src="../../simulator/?scenario=memory-arbiter" title="TwoPortMemoryArbiter recorded simulation" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../simulator/?scenario=memory-arbiter" class="md-button">Open TwoPortMemoryArbiter in the simulator</a>

### Source and tests

- [Public interface: `TwoPortMemoryArbiter.hpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/include/modules/memory/TwoPortMemoryArbiter.hpp)
- [Structural wiring and catalog: `TwoPortMemoryArbiter.cpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/modules/memory/TwoPortMemoryArbiter.cpp)
- `MemoryArbiterTest` in [MemoryEndpointTests.cpp](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/tests/MemoryEndpointTests.cpp) records the example above in every timing profile.
- [`TwoPortMemoryArbiterTest`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/tests/TwoPortMemoryArbiterTests.cpp) checks widths 8, 16 and 32 and bursts of 1, 2 and 4 words.
- `MemoryEndpointIntegrationTest` runs every checkpoint of the example through the C++ reference above.
- `RV32ITopologyTest` runs a program through the arbiter in every shared-memory arrangement of [§ 4.7](../rv32i/memory-systems.md).
