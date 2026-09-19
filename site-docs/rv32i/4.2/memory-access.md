# § 4.2.6. Memory Access

These are **CPU-side** components. They format loads/stores, check alignment, and hold one request until the [universal memory endpoint](../../memory/3.2.md) completes it. They do not contain the storage device or replace its [technology-specific controller](../../memory/3.3.md).

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| `RV32IMemoryLanes` | Little-endian subword extraction, store placement, masks, and endpoint address | Yes | No |
| `RV32IMemoryAlignmentUnit` | Address alignment predicate | Yes | Yes |
| `RV32IMemoryRequest` | CPU-owned, one-outstanding transaction requester | Yes | No |

Columns count registered component implementations. The behavioral cores and [single-cycle Fetch](../4.3/fetch.md)/[Memory](../4.3/memory.md) stages contain equivalent requester/formatting code, but those helpers are not selectable behavioral implementations of these two structural components. C++ examples below specify binary behavior without propagation delays.

```text
CPU effective address / store data
          │
          ▼
  RV32IMemoryLanes ── aligned byte address / placed data / byte mask ──┐
          ▲                                                         ▼
          └────────── response word ───────────────── RV32IMemoryRequest
                                                               ⇅ REQ / RSP
                                           MemoryContainer or MemoryEndpoint
```

## 4.2.6.1. `RV32IMemoryLanes`

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `ADDR` | Input | 32 | CPU byte address |
| `WRITE_DATA` | Input | 32 | Unshifted store source register |
| `WORD_DATA` | Input | 32 | Complete word returned by the endpoint |
| `SIZE` | Input | 2 | 0: byte; 1: halfword; 2: word; 3: invalid |
| `SIGN_EXTEND` | Input | 1 | Sign-extend byte/halfword loads |
| `WORD_ADDR` | Output | 32 | Endpoint **byte address**, not a word index |
| `WORD_WRITE_DATA` | Output | 32 | Store data shifted to its byte lane |
| `READ_DATA` | Output | 32 | CPU load result |
| `WRITE_MASK` | Output | 4 | One enable per endpoint byte lane |

### Behavior

Offset is `ADDR[1:0]`. Byte lane 0 is bits 7:0; lane 3 is bits 31:24. Valid accesses use `WORD_ADDR = ADDR & ~3`. Byte stores enable one lane; aligned halfword stores enable lanes 0–1 or 2–3; word stores enable all four. Store data moves left by `offset*8`; load data moves right by that amount, then is narrowed and sign/zero-extended.

A halfword at an odd address, word at a nonzero offset, or `SIZE=3` must **not** silently become an aligned legal access. The output address therefore sets bit 0 on these cases, making the endpoint reject the request as unaligned. There is no split transaction across words. Invalid inputs still produce deterministic combinational data/masks, but these are not permission to write: the endpoint must reject the bad address. `SIZE=3` gives read data and mask zero.

#### C++ example

```cpp
#include <cstdint>
namespace memory_access_example {
struct Lanes { uint32_t WORD_ADDR, WORD_WRITE_DATA, READ_DATA; unsigned WRITE_MASK; };
Lanes lanes(uint32_t address, uint32_t write, uint32_t word, unsigned size, bool sign) {
    const unsigned offset=address&3;
    const bool bad=size==3 || (size==1 && (offset&1)) || (size==2 && offset!=0);
    const uint32_t shifted=word>>(8*offset);
    uint32_t read=size==0 ? shifted&255 : size==1 ? shifted&65535 : size==2 ? shifted : 0;
    if (sign && size==0 && (read&128)) read|=0xffffff00U;
    if (sign && size==1 && (read&32768)) read|=0xffff0000U;
    const unsigned mask=size==0 ? 1U<<offset : size==1 ? 3U<<(offset&2) : size==2 ? 15 : 0;
    return {(address&~uint32_t{3})|uint32_t(bad),write<<(8*offset),read,mask};
}
}
```

#### Unknown inputs and timing

This is a combinational structural network. Splitters and rewires preserve individual `X`/`Z` bits; muxes and sign-extension gates can mask or propagate them. A known size or disabled sign extension may isolate unrelated bits. The binary example does not model those cases or the different path delays.

### Implementation

#### Structural implementation

Nine top-level children expose the data flow instead of placing every bit gate side by side:

| Child | Structural helper / responsibility |
| --- | --- |
| `ADDRESS` | `RV32IMemoryAddress`: address/size decoding, guarded word address, byte offset, byte/halfword/word enables |
| `LANE_0` … `LANE_3` | Rewire four possible byte placements for store and load data |
| `STORE_LANES`, `LOAD_LANES` | Select the requested byte offset |
| `LOAD_EXTEND` | `RV32ILoadExtender`: select byte/halfword/word data and sign/zero extension |
| `WRITE_MASK` | `RV32IByteWriteMask`: generate the four byte-write enables |

`ADDRESS` receives `ADDR[32]` and `SIZE[2]`; it outputs `WORD_ADDR[32]`, `OFFSET[2]`, and `BYTE/HALF/WORD`. `LOAD_EXTEND` receives shifted `DATA[32]`, those three enables, and `SIGN_EXTEND`; its `OUT[32]` is assembled from four `RV32ILoadByte<0…3>` circuits. Each byte circuit contains eight numbered bit lanes; lower bytes retain input bits while higher bytes select the relevant sign fill. `WRITE_MASK` receives the offset and size enables and produces `MASK[4]`.

These are fixed structural helpers with no state or registered behavioral variants. All original alignment, reserved-size, and partial-unknown behavior remains in gates; grouping does not turn them into behavioral memory devices.

### Examples

<!-- simulator-examples: rv32i-memory-lanes RV32IMemoryLanesTest -->

#### Embedded simulation

<div class="simulator-embed"><iframe src="../../../simulator/?scenario=rv32i-memory-lanes&amp;embed=1" title="RV32IMemoryLanes" loading="lazy" allowfullscreen></iframe></div>

<a href="../../../simulator/?scenario=rv32i-memory-lanes" class="md-button">Open memory lanes</a>

### Tests

`RV32IMemoryLanesTest` exposes 32 checkpoints covering all four size encodings, four offsets, and both sign modes. `RV32IMemoryMigrationTest` additionally exercises formatting against real endpoint responses and checks that rejected stores do not change storage. These are sampled data words, not an exhaustive 32-bit operand test.

## 4.2.6.2. `RV32IMemoryAlignmentUnit`

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `ADDRESS` | Input | 32 | CPU byte address |
| `SIZE` | Input | 2 | Access-size encoding |
| `MISALIGNED` | Output | 1 | Address violates the selected alignment |

### Behavior

Byte accesses accept every address; halfwords require bit 0 zero; words require bits 1:0 zero. The circuit equation is `SIZE[0]·ADDRESS[0] OR SIZE[1]·(ADDRESS[0] OR ADDRESS[1])`. Consequently reserved `SIZE=3` has the same **alignment predicate** as word accesses; an aligned address produces zero. This component is not a size-validity checker. The lane formatter and execution-status unit separately reject size 3.

The single-cycle Memory stage also adds a size-3 check to its public `MISALIGNED` output. Fetch and Next PC use this building block with constant word size.

#### C++ example

```cpp
namespace memory_access_example {
bool misaligned(uint32_t address, unsigned size) {
    return ((size&1) && (address&1)) || ((size&2) && (address&3));
}
}
```

#### Unknown inputs and timing

Only the low two address bits and both size bits matter. AND with zero and OR with one mask unknowns; other indeterminate gate results become `X`. Both implementations use that equation, including `SIZE=3` and `X`/`Z`. Behavioral timing is one abstract tick; structural timing follows its gates.

### Implementation

#### Structural implementation

Two splitters, two AND gates, and two OR gates evaluate the predicate. It is registered as `rv32i.pipeline.memory-alignment`, but its address check is not intrinsically pipeline-specific.

#### Behavioral implementation

`RV32IMemoryAlignmentDirect` evaluates the same four-state gate expression without internal components. It does not coerce unknown bits into a binary integer.

### Examples

<!-- simulator-examples: rv32i-memory-alignment RV32IMemoryAlignmentUnitTest -->

#### Embedded simulation

<div class="simulator-embed"><iframe src="../../../simulator/?scenario=rv32i-memory-alignment&amp;embed=1" title="RV32IMemoryAlignmentUnit" loading="lazy" allowfullscreen></iframe></div>

<a href="../../../simulator/?scenario=rv32i-memory-alignment" class="md-button">Open alignment checker</a>

### Tests

`RV32IMemoryAlignmentUnitTest` includes six named binary cases and all $4^4=256$ combinations of LOW/HIGH/X/Z for the four meaningful bits. It checks 262 checkpoints in each implementation. Source: `src/modules/rv32i/RV32IPipelineControl.cpp`.

## 4.2.6.3. `RV32IMemoryRequest`

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `CLK`, `RST` | Input | 1 each | Rising-edge transaction state; asynchronous active-high reset |
| `ISSUE` | Input | 1 | Capture a new request when idle |
| `TAKE` | Input | 1 | CPU can consume the current response |
| `TAG` | Input | 32 | Current CPU request identity; a change abandons the saved request |
| `ADDR`, `WRITE_DATA` | Input | 32 each | Endpoint byte address and lane-placed data |
| `OP` | Input | 3 | Universal protocol operation (read=0, write=1); forwarded without decoding |
| `WRITE_MASK` | Input | 4 | Endpoint byte write enables |
| `BUS_REQ_READY` | Input | 1 | Endpoint can accept the request |
| `BUS_RSP_VALID` | Input | 1 | Endpoint holds a response |
| `BUS_RSP_READ_DATA` | Input | 32 | Endpoint response word |
| `BUS_RSP_STATUS` | Input | 3 | Universal response status; zero is success |
| `BUS_REQ_VALID` | Output | 1 | Saved request is awaiting acceptance |
| `BUS_REQ_OP` | Output | 3 | Saved operation |
| `BUS_REQ_ADDR`, `BUS_REQ_WRITE_DATA` | Output | 32 each | Saved request payload |
| `BUS_REQ_WRITE_MASK` | Output | 4 | Saved byte mask |
| `BUS_RSP_READY` | Output | 1 | Consume or discard endpoint response |
| `DONE` | Output | 1 | A valid response belongs to the current CPU request |
| `READ_DATA` | Output | 32 | Direct copy of response data; meaningful with DONE |
| `FAULT` | Output | 1 | Nonzero response status; meaningful with DONE |

### Behavior

There is at most one outstanding transaction:

1. On an idle rising edge with `ISSUE=1`, latch tag and payload. Asserting ISSUE alone does not assert bus valid before this capture.
2. Keep `BUS_REQ_VALID` and the saved payload stable until a rising edge with `BUS_REQ_READY=1`. CPU input changes cannot mutate this request.
3. After acceptance, wait for a response. `DONE` announces a matching valid response even if `TAKE=0`; the **endpoint** holds response data/status under this backpressure.
4. With TAKE or an abandoned tag, assert `BUS_RSP_READY`. A rising edge with response valid completes/drains the transaction. A new request is captured on a later idle edge, not simultaneously on the completion edge.

`TAG` is local bookkeeping, not a bus transaction ID. A mismatch immediately suppresses DONE and enables draining once the request has been sent. A mismatch sampled while busy is sticky: A→B→A across clock edges cannot make the old response valid again. A transient mismatch entirely between edges is not stored. Abandonment does **not** withdraw a stalled request or cancel an accepted store; it discards its eventual response. The CPU must choose tags that distinguish its requests and coordinate any redirects.

Reset clears busy/sent/abandoned and saved payload, suppresses bus valid/ready and DONE, and does not undo memory writes. Reset must be coordinated with the connected endpoint's transaction reset. `READ_DATA` and `FAULT` remain direct combinational response signals, even while idle or reset; they are not gated by DONE. ISSUE held high can start another transaction after the current one finishes, so the caller must manage it.

#### C++ example

This binary reference covers every output, payload hold, reset, completion, and abandonment rule. Start with reset asserted; input values must settle before each clock edge.

`observe` reads combinational outputs without advancing state. `evaluate` applies reset/edge changes and returns the resulting outputs. The stage examples in 4.3 use this distinction to capture all inputs from the old state.

```cpp
namespace memory_access_example {
struct RequestInputs {
    bool CLK, RST, ISSUE, TAKE;
    uint32_t TAG, ADDR, WRITE_DATA;
    unsigned OP, WRITE_MASK;
    bool BUS_REQ_READY, BUS_RSP_VALID;
    uint32_t BUS_RSP_READ_DATA;
    unsigned BUS_RSP_STATUS;
};
struct RequestOutputs {
    bool DONE, FAULT;
    uint32_t READ_DATA;
    bool BUS_REQ_VALID, BUS_RSP_READY;
    unsigned BUS_REQ_OP;
    uint32_t BUS_REQ_ADDR, BUS_REQ_WRITE_DATA;
    unsigned BUS_REQ_WRITE_MASK;
};
class Request {
    bool busy=false, sent=false, abandoned=false, previous_clock=false;
    uint32_t tag=0, address=0, data=0;
    unsigned op=0, mask=0;
    bool discard(const RequestInputs& i) const { return abandoned || tag!=i.TAG; }
    RequestOutputs outputs(const RequestInputs& i) const {
        return {sent && i.BUS_RSP_VALID && !discard(i) && !i.RST,
                i.BUS_RSP_STATUS!=0, i.BUS_RSP_READ_DATA,
                busy && !sent && !i.RST, sent && (i.TAKE || discard(i)) && !i.RST,
                op,address,data,mask};
    }
public:
    RequestOutputs observe(const RequestInputs& i) const { return outputs(i); }
    RequestOutputs evaluate(const RequestInputs& i) {
        if (i.RST) {
            busy=sent=abandoned=false; tag=address=data=0; op=mask=0;
        } else if (!previous_clock && i.CLK) {
            const auto before=outputs(i);
            if (before.BUS_RSP_READY && i.BUS_RSP_VALID) busy=sent=abandoned=false;
            else if (!busy && i.ISSUE) {
                busy=true; sent=abandoned=false;
                tag=i.TAG; address=i.ADDR; data=i.WRITE_DATA; op=i.OP; mask=i.WRITE_MASK;
            } else if (busy) {
                abandoned=discard(i);
                if (before.BUS_REQ_VALID && i.BUS_REQ_READY) sent=true;
            }
        }
        previous_clock=i.CLK;
        return outputs(i);
    }
};
}
```

#### Unknown inputs and timing

State is initially unknown until reset. Four-state behavior follows the FF cells, payload registers, equality gates, and enables. Response data is rewired and preserves `X`/`Z`; status is OR-reduced. The example assumes binary, stable handshake controls and does not model setup/hold violations or propagation glitches. Request/response handshakes are sampled on clock edges, not whenever valid and ready happen to overlap between edges.

### Implementation

#### Structural implementation

`BUSY`, `SENT`, and `ABANDONED` are `FlipFlopCell` state bits. Five `MemoryPayloadRegister` children retain the tag and request fields. `TAG_MATCH` is a fixed structural `RV32IRequestTagMatch`: inputs `CURRENT[32]` and `SAVED[32]` feed 32 XOR comparisons and an OR-reduction tree, producing `CHANGED`. The numeric tag-bit layout is inside this block; payload, handshake state, and response handling remain visible at the requester level.

`RESPONSE_DATA` is a direct boundary rewire, not a response register. The retained request/state is why a requester cannot be replaced with a port rename. There is no registered behavioral implementation; `RV32IMemoryRequestModel.hpp` supplies equivalent state bookkeeping inside the behavioral CPU implementations.

### Examples

<!-- simulator-examples: rv32i-memory-request RV32IMemoryRequestTest -->

#### Embedded simulation

<div class="simulator-embed"><iframe src="../../../simulator/?scenario=rv32i-memory-request&amp;embed=1" title="RV32IMemoryRequest" loading="lazy" allowfullscreen></iframe></div>

<a href="../../../simulator/?scenario=rv32i-memory-request" class="md-button">Open memory requester</a>

### Tests

`RV32IMemoryRequestTest` records 21 checkpoints: capture, request backpressure, payload stability, acceptance, held response, fault forwarding, consumption, redirect/ABA abandonment, draining, and reset. `RV32IMemoryMigrationTest` connects requesters to the container and expanded endpoint implementations and checks core integration. `RV32IBuildingBlocksDocumentationTest` compiles both memory examples and replays their declared binary actions. Source: `src/modules/rv32i/RV32IMemoryLanes.cpp` and `RV32IMemoryRequest.cpp`.
