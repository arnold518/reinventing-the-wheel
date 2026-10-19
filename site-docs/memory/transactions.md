# § 3.4. Transaction Protocol

The request/response interface every memory endpoint and cache presents, above the native devices of [3.3](technologies/index.md). It is a single-outstanding, aligned-word protocol with one burst read, not AXI, a DDR interface or a coherent interconnect. Width W is 8, 16 or 32; capacity is a construction parameter.

The transaction components are:

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| `MemoryTransactionPort<W>` | Track one delayed request; pass through or hold its response | Yes | No |
| `MemoryResponseBuffer<W>` | One-entry response buffer with a combinational bypass | Yes | No |
| `ClockedStateBit` | Resettable always-clocked bit for handshake state; a storage component documented in [4.3.1](../rv32i/4.3/register.md) | Yes | Yes |

## Protocol rules

Request and response transfers happen at a rising `CLK` edge when the
corresponding VALID and READY are both 1. The sender holds VALID and its
payload until the transfer and must not wait for READY before raising VALID.
At most one transaction is outstanding, and its response is held unchanged
until consumed.

`REQ_READY` depends on registered state only: a request is accepted while the
endpoint is idle, or in the cycle its current transaction completes, provided
no earlier response is still held. It never depends on the same cycle's
`RSP_READY`, so the consumer's decision cannot reach the device's inputs in
the same cycle, as on a real bus. A zero-wait endpoint (flip-flop storage) may
present its response in the request cycle and transfer both at the same edge.

<!-- diagram: transaction-handshake -->

Addresses are **byte addresses** aligned to W/8. Reads return a full word.
Writes and programs use positioned byte lanes; mask bit b enables byte b. ISA
load sizes, sign extension, unaligned splitting, atomics, arbitration and
caching are outside this port.

| `REQ_OP` | Operation | Meaning |
| ---: | --- | --- |
| 0 | READ | Read one aligned word |
| 1 | WRITE | Replace the enabled bytes in writable flip-flop, SRAM or DRAM storage |
| 2 | PROGRAM | NOR flash only: clear enabled bits, never restore 0→1 |
| 3 | ERASE_BLOCK | NOR flash only: restore one whole block to ones; the address must be block-aligned |
| 4 | READ_BURST | SRAM, ROM and DRAM: read the `block_words` words of the aligned block holding the address, from that word on and wrapping within the block, as successive responses |
| 5–7 | Reserved | Unsupported-operation response |

A burst's words arrive as one response each; the transaction ends with its
last word, and only then can the next request be accepted. A DRAM burst stays
within one row. Caches use bursts to fill a line.

| `RSP_STATUS` | Meaning |
| ---: | --- |
| 0 | Success |
| 1 | Misaligned word or block address |
| 2 | Address outside the configured capacity |
| 3 | Attempt to modify ROM |
| 4 | Unsupported operation |
| 5 | Native device failure |
| 6 | Unknown required request fields |
| 7 | Reserved |

Error and non-read responses have zero data. Unknown stored bits remain X even
in a successful read. A zero write mask changes nothing. Reads ignore write
data and mask; erase ignores both. Requests are checked in this order:
unknown operation or address → unsupported operation (including a burst on a
technology without one) → unknown mask for WRITE or PROGRAM → word alignment →
capacity → technology permission and block alignment. A burst that passes
alignment and capacity is accepted.

`RST` resets the **transaction interface**, not memory contents. It holds
VALID and READY low and discards pending response delivery. An operation
already sent to the device finishes before the device is reused; its writes,
programs or erases are not rolled back. Native flip-flop storage reset is
deliberately not tied to this interface reset.

## 3.4.1. `MemoryTransactionPort<W>`

Shared structural frontend. The backend (the controller's sequencer) captures the live request at an edge with `START` = 1 and holds `DONE` and its result until an edge with `FINISH` = 1. The port owns no memory contents and does not validate addresses or operations.

Here `<W>` denotes the pin width. The actual C++ `MemoryTransactionPort` constructor takes W as a runtime argument; the simplified reference below uses a template to keep its payload widths together.

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `CLK`, `RST` | Input | 1 | Clock and active-high interface reset |
| `REQ_VALID` | Input | 1 | Request available |
| `REQ_OP` | Input | 3 | Operation code |
| `REQ_ADDR` | Input | 32 | Aligned byte address |
| `REQ_WRITE_DATA` | Input | W | Positioned write/program data |
| `REQ_WRITE_MASK` | Input | W/8 | Byte enables |
| `REQ_READY` | Output | 1 | Can accept a request |
| `RSP_VALID` | Output | 1 | Response available |
| `RSP_READY` | Input | 1 | Requester accepts the response |
| `RSP_READ_DATA` | Output | W | Held read result |
| `RSP_STATUS` | Output | 3 | Held status |
| `BACK_READY`, `DONE` | Input | 1 | Backend available / operation complete |
| `LAST` | Input | 1 | The completing response is the transaction's last (low between the words of a burst) |
| `RESULT` | Input | W | Backend read result |
| `STATUS` | Input | 3 | Backend status |
| `START`, `FINISH` | Output | 1 | Accept live request / release completed backend result |
| `SPACE` | Output | 1 | No response is held, so the one shown leaves at the next edge whatever `RSP_READY` does |
| `OP` | Output | 3 | Live operation |
| `ADDR` | Output | 32 | Live byte address |
| `DATA` | Output | W | Live data |
| `MASK` | Output | W/8 | Live byte mask |

### Behavior

There is one pending bit, not a phase pipeline. `START` accepts a request when the port is idle or its transaction is completing with `LAST`, the backend is ready, and no response is held. A completion passes directly through the response buffer; if the receiver cannot take it, the buffer captures the result and status, and `FINISH` releases the backend, which can then continue (refresh, for example) while that response stays held. Between the words of a burst `LAST` is low, so the transaction stays pending. The general flip-flop controller uses the same response buffer without this frontend; its always-ready configuration returns a zero-wait response without a buffer.

#### C++ example

This reference shows the response storage and the pending state. No request register is duplicated in the frontend: the sequencer captures the request on `START`. Control flags are binary; payloads use the project's four-state `Bits` and `Request` types. It assumes an initial reset and describes settled values, not gate delays.

```cpp
#include "MemoryEndpointEngine.hpp" // Only value/configuration helpers.

namespace memory_example {
using namespace circuit::memory;

template<std::size_t W>
class ResponseBuffer {
    bool full_ = false;
    Bits data_ = Bits(W, L), status_ = bits(3, 0);
public:
    struct Inputs {
        bool reset = false, edge = false, valid = false, ready = false;
        Bits data = Bits(W, L), status = bits(3, 0);
    };
    struct Outputs { bool ready, valid; Bits data, status; };
    bool full() const { return full_; }
    Outputs outputs(const Inputs& i) const {
        return {!i.reset && (!full_ || i.ready),
                !i.reset && (full_ || i.valid),
                full_ ? data_ : i.data, full_ ? status_ : i.status};
    }
    void step(const Inputs& i) {
        if (i.reset) { full_ = false; data_.assign(W, L); status_ = bits(3, 0); }
        else if (i.edge) {
            const bool capture = i.valid && (!full_ || i.ready) && (full_ || !i.ready);
            const bool hold = full_ && !i.ready;
            if (capture) { data_ = i.data; status_ = i.status; }
            full_ = capture || hold;
        }
    }
};

template<std::size_t W>
class Port {
    static_assert(W == 8 || W == 16 || W == 32);
    bool pending_ = false;
    ResponseBuffer<W> response_;
public:
    struct Inputs {
        bool rising_edge = false, reset = false;
        bool request_valid = false, response_ready = false;
        bool backend_ready = false, done = false, last = true;
        Request request{bits(3, 0), bits(32, 0), Bits(W, L), Bits(W / 8, L)};
        Bits result = Bits(W, L), status = bits(3, 0);
    };
    struct Outputs {
        bool request_ready, response_valid, start, finish, space;
        Request request;
        Bits read_data, status;
    };
private:
    auto response_input(const Inputs& i) const {
        return typename ResponseBuffer<W>::Inputs{
            i.reset, i.rising_edge, pending_ && i.done,
            i.response_ready, i.result, i.status};
    }
public:
    Outputs outputs(const Inputs& i) const {
        auto r = response_.outputs(response_input(i));
        const bool complete = pending_ && i.done;
        // Registered state only: idle, or the last response is completing,
        // and no earlier response is held. Never this cycle's RSP_READY.
        const bool ready = !i.reset && (!pending_ || (complete && i.last))
                           && i.backend_ready && !response_.full();
        return {ready, r.valid, ready && i.request_valid, complete && r.ready,
                !response_.full(), i.request, r.data, r.status};
    }
    Outputs step(const Inputs& i) {
        const auto before = outputs(i); // All FFs sample the old state together.
        response_.step(response_input(i));
        if (i.reset) pending_ = false;
        else if (i.rising_edge) // A burst's transaction ends with its last word.
            pending_ = before.start || (pending_ && !(before.finish && i.last));
        return outputs(i);
    }
};
} // namespace memory_example
```

An empty response buffer bypasses its registers. On a blocked transfer it captures the payload; when an old response is consumed and a new one arrives together, the old one is delivered and the new one retained. The frontend samples `START` and `FINISH` with pre-edge values. Changing request inputs after acceptance changes the live backend pins but not the sequencer's captured request. Reset clears pending and held state, not memory.

`request`, `result`, and `status` must have the pin widths listed above. The excerpt deliberately does not validate addresses or opcodes; a frontend must forward those fields to the controller, including unknown payload bits. Its internal header requires the `include` and `src/modules/memory` include paths and `SimulatorLib`. `MemoryEndpointIntegrationTest` compiles and runs the exact excerpt at W=8/16/32: bypass without an edge, backend stalls, reset, response holding, the registered `REQ_READY` and burst words.

#### Unknown inputs

CLK, reset, VALID and READY must be binary and meet the flip-flops' setup and hold times. Unknown handshake controls are protocol violations, not alternate transactions. Payload data may contain X; the controller rejects an unknown operation, address or required mask before issuing native commands.

### Implementation

#### Structural implementation

`PENDING` is one `ClockedStateBit`. `RESPONSE` is a `MemoryResponseBuffer<W>` with an occupancy state bit, data/status registers, and bypass multiplexers; its reused register component is defined in [4.3.1](../rv32i/4.3/register.md). Live request fields cross the scope boundary through zero-delay rewires. The layout separates request forwarding, handshake control and response retention.

### Timing

Built from gates and flip-flops only, so it has no delay table of its own: outputs settle through its gates under the [profile's gate delays](../timing.md#timing-profiles), and its registers launch one flip-flop clock-to-output delay after the edge. Its delays are part of the proven tables of the controllers and endpoints built on it.

### Examples

<!-- simulator-examples: memory-transaction-port MemoryTransactionPortTest -->

Accepts address 12, demonstrates that backend pins remain live while the sequencer owns the accepted request, passes through result `0xA5`, retains it after RESULT changes, then resets the interface. Every preparation, edge and hold checkpoint is included in the scrollable table, with its recorded time.

#### Embedded simulation

<div class="simulator-embed">
  <iframe src="../../simulator/?scenario=memory-transaction-port" title="MemoryTransactionPort recorded simulation" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../simulator/?scenario=memory-transaction-port" class="md-button">Open MemoryTransactionPort in the simulator</a>

### Source and tests

- [Interface: `MemoryProtocol.hpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/include/modules/memory/MemoryProtocol.hpp)
- [Structural implementation: `MemoryController.cpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/modules/memory/MemoryController.cpp)
- [Tests: `MemoryEndpointTests.cpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/tests/MemoryEndpointTests.cpp)
- [Executable C++ excerpt checks](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/tests/fixtures/memory/transaction-controller-examples.cpp), extracted and run by `MemoryEndpointIntegrationTest`

| Test | Coverage |
| --- | --- |
| `MemoryTransactionPortTest` | Complete sequence above; live request and held response |
| `MemoryEndpointContractTest` | Frontend connected to native devices and compared with compact endpoints |
| `MemoryEndpointTimingTest` | Reset, backpressure, native operations in flight and refresh |
| `MemoryEndpointIntegrationTest` | Execute the documented frontend reference at W=8/16/32; verify bypass, response capture and handshake behavior |

## 3.4.2. `MemoryResponseBuffer<W>`

One entry of response storage, shared by the zero-wait and delayed controllers. It is not a request queue or memory array.

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `CLK`, `RST` | Input | 1 | Shared clock and interface reset |
| `IN_VALID`, `OUT_READY` | Input | 1 | Producer presents data / consumer accepts data |
| `IN_DATA`, `IN_STATUS` | Input | W, 3 | New response payload |
| `IN_READY`, `OUT_VALID` | Output | 1 | Input capacity / response available |
| `OUT_DATA`, `OUT_STATUS` | Output | W, 3 | Live or held response payload |
| `FULL` | Output | 1 | An older response occupies the register |

### Behavior

When empty, the input passes directly to the output. An available receiver can consume it without filling the register. Backpressure captures one response on the edge; subsequent input changes cannot alter it. When a full buffer consumes and replaces on the same edge, the old response is delivered first and the new response is retained. Reset clears occupancy and retained data/status without touching device contents.

#### C++ example

`ResponseBuffer<W>` in the preceding excerpt is the complete state/transition reference. This wrapper selects its 32-bit instance; it adds no behavior or additional state.

```cpp
namespace memory_example {
using ResponseBuffer32 = ResponseBuffer<32>;
}
```

#### Unknown inputs

Controls must be known binary values. Payload flip-flops preserve unknown data bits. “Zero-wait” means no extra clock cycle, not zero delay: the bypass and selection gates still settle. Data and status outside `OUT_VALID` have no transaction meaning.

### Implementation

#### Structural implementation

`OCCUPIED` is a `ClockedStateBit`; `HELD_DATA` and `HELD_STATUS` are FF-based registers. `SELECT_DATA` and `SELECT_STATUS` choose held versus live inputs. The lower control row computes space, capture, hold and next occupancy. The buffer itself is structural; its state components can be selected independently by fidelity.

`HELD_DATA` retains W bits and `HELD_STATUS` retains 3 bits using the
[register family defined in 4.3.1](../rv32i/4.3/register.md). Including
`OCCUPIED`, the buffer retains W+4 bits: 12, 20, or 36 at W=8, 16, or 32.
An empty buffer bypasses this storage; a blocked response needs it to remain
stable. Selecting compact storage or a behavioral enclosing endpoint changes
simulation detail, not the number of logical storage bits or the response contract.

### Timing

Built from gates and flip-flops only, so it has no delay table of its own: outputs settle through its gates under the [profile's gate delays](../timing.md#timing-profiles), and its registers launch one flip-flop clock-to-output delay after the edge. Its delays are part of the proven tables of the controllers and endpoints built on it.

### Examples

<!-- simulator-examples: memory-response-buffer MemoryResponseBufferTest -->

The checkpoints show empty bypass, a transfer without filling, capture under backpressure, stable holding, same-edge consume/replace and reset.

<div class="simulator-embed"><iframe src="../../simulator/?scenario=memory-response-buffer&amp;embed=1" title="Memory response buffer" loading="lazy" allowfullscreen></iframe></div>

<a href="../../simulator/?scenario=memory-response-buffer" class="md-button">Open response buffer</a>

### Source and tests

`MemoryController.cpp` defines the circuit. `MemoryResponseBufferTest` supplies the recorded rows; `MemoryEndpointContractTest` repeats the structural checks at W=8/16/32. The executable documentation fixture checks the same bypass and replacement rules.
