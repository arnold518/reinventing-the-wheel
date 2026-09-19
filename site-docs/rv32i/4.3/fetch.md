# § 4.3.1. Fetch

`RV32ISingleCycleFetchStage` owns the architectural PC and one instruction requester. It exposes the instruction returned by the endpoint; it does not contain instruction memory or decode the word. In the core it is named `FETCH`.

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| `RV32ISingleCycleFetchStage` | PC register, sequential address, instruction request/response, fetch status | Yes | Yes |

## 4.3.1.1. `RV32ISingleCycleFetchStage`

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `CLK`, `RST` | Input | 1 each | Rising-edge capture; asynchronous active-high reset |
| `ISSUE` | Input | 1 | Permit a new fetch when the requester is idle |
| `TAKE` | Input | 1 | Consume a completed response |
| `PC_WRITE` | Input | 1 | Authorize PC capture at the rising edge |
| `NEXT_PC` | Input | 32 | Candidate PC from Next PC |
| `IMEM_REQ_READY` | Input | 1 | Endpoint accepts a presented request at the edge |
| `IMEM_RSP_VALID` | Input | 1 | Endpoint holds a response |
| `IMEM_RSP_READ_DATA` | Input | 32 | Returned instruction word |
| `IMEM_RSP_STATUS` | Input | 3 | Zero: success; nonzero: access fault |
| `PC`, `PC_PLUS_4` | Output | 32 each | Stored PC; PC+4 modulo 2^32 |
| `INSTRUCTION` | Output | 32 | Directly exposed response word |
| `DONE`, `FAULT`, `PC_MISALIGNED` | Output | 1 each | Matching response present; nonzero response status; PC low two bits nonzero |
| `IMEM_REQ_VALID`, `IMEM_RSP_READY` | Output | 1 each | Request held for acceptance; response consumption/draining permission |
| `IMEM_REQ_OP` | Output | 3 | Captured read operation, 0 |
| `IMEM_REQ_ADDR` | Output | 32 | Captured PC byte address |
| `IMEM_REQ_WRITE_DATA` | Output | 32 | Zero |
| `IMEM_REQ_WRITE_MASK` | Output | 4 | Captured fetch mask, 15; zero after requester reset until capture |

### Behavior

Reset clears PC and requester state. Otherwise a rising edge with `PC_WRITE=1` captures `NEXT_PC`; without that enable PC holds. PC+4 and alignment are continuously derived from PC. Misalignment is reported, not repaired by rounding the fetch address down.

On an idle rising edge with `ISSUE=1`, the requester captures the **old PC** as both address and internal tag, operation 0, data 0, and mask 15. The request is presented after capture and held until an acceptance edge. `REQ_READY` does not mean the instruction has arrived. After acceptance, a matching `RSP_VALID` raises `DONE`; the endpoint must retain its response until `RSP_READY` is accepted.

`TAKE` permits consuming that response. The core asserts it at an instruction-completion edge, including a halt/trap attempt. `ISSUE=0` prevents a new request but does not cancel a pending one. If PC changes while a request is outstanding, the requester drains the stale response without `DONE`; a tag mismatch sampled at an edge remains abandoned even if PC later returns. This is the [requester's existing contract](../4.2/memory-access.md#4263-rv32imemoryrequest), not a fetch buffer or speculative pipeline.

`INSTRUCTION` and `FAULT` follow endpoint response pins even when `DONE=0`; they are not automatically zeroed or valid architectural inputs then. `PC_WRITE` and `TAKE` are independent pins at this component boundary. The complete core, not Fetch alone, ensures a new PC is accepted only on an authorized completion.

#### C++ example

Use `memory_access_example::Request` from [4.2.6](../4.2/memory-access.md). Its `observe` method reads outputs without advancing state. Start with reset asserted and supply values within the declared pin widths. The requester captures using the old PC; `observe` then reports the settled post-edge signals without performing another clock edge.

```cpp
#include <cstdint>
namespace single_cycle_example {
struct FetchInputs {
    bool CLK, RST, ISSUE, TAKE, PC_WRITE;
    uint32_t NEXT_PC;
    bool IMEM_REQ_READY, IMEM_RSP_VALID;
    uint32_t IMEM_RSP_READ_DATA;
    unsigned IMEM_RSP_STATUS;
};
struct FetchOutputs {
    uint32_t PC, PC_PLUS_4, INSTRUCTION;
    bool DONE, FAULT, PC_MISALIGNED, IMEM_REQ_VALID, IMEM_RSP_READY;
    unsigned IMEM_REQ_OP;
    uint32_t IMEM_REQ_ADDR, IMEM_REQ_WRITE_DATA;
    unsigned IMEM_REQ_WRITE_MASK;
};
class Fetch {
    uint32_t pc=0;
    bool previous_clock=false;
    memory_access_example::Request request;
    memory_access_example::RequestInputs inputs(const FetchInputs& i) const {
        return {i.CLK,i.RST,i.ISSUE,i.TAKE,pc,pc,0,0,15,
                i.IMEM_REQ_READY,i.IMEM_RSP_VALID,i.IMEM_RSP_READ_DATA,i.IMEM_RSP_STATUS};
    }
public:
    FetchOutputs observe(const FetchInputs& i) const {
        const auto r=request.observe(inputs(i));
        return {pc,pc+4,r.READ_DATA,r.DONE,r.FAULT,bool(pc&3),
                r.BUS_REQ_VALID,r.BUS_RSP_READY,r.BUS_REQ_OP,
                r.BUS_REQ_ADDR,r.BUS_REQ_WRITE_DATA,r.BUS_REQ_WRITE_MASK};
    }
    FetchOutputs evaluate(const FetchInputs& i) {
        request.evaluate(inputs(i)); // All captured fields use pre-edge PC.
        if (i.RST) pc=0;
        else if (!previous_clock && i.CLK && i.PC_WRITE) pc=i.NEXT_PC;
        previous_clock=i.CLK;
        return observe(i);
    }
};
}
```

#### Unknown inputs and timing

The supported equivalence boundary is binary operation after reset, not power-up state. Structural PC/request state follows the selected register and gate models; the compact implementation initializes binary PC/request bookkeeping and only recognizes known low-to-high clocks. Nonbinary word-to-integer conversions in the compact state path fall back to zero, so unknown addresses, reset, and handshake controls are outside the equivalence claim. The response word is forwarded bitwise; an unknown response status produces `FAULT=X`. Behavioral outputs take one abstract tick, not the structural path delay.

### Implementation

#### Structural implementation

`PC_STATE` is [`Register32`](../4.2/register.md), with `FFWord<32>` at `PC_STATE.STORAGE`; `PC_PLUS_4_ADD` is `Adder32` with constant four and carry-in zero. `ALIGNMENT` checks PC for word alignment. `REQUEST` is `RV32IMemoryRequest`, with read/data/mask constants and the `IMEM_` bus connected at the boundary. These components remain expandable independently.

#### Behavioral implementation

`DirectStage`'s Fetch branch keeps PC, previous clock, and `MemoryRequestState`, then publishes the same settled outputs. It does not instantiate hidden gates or call the instruction oracle. Both forms are registered as `rv32i.single-cycle.fetch`.

### Examples

The scenario holds a request under backpressure, accepts it without completing, holds the response, then consumes it while moving PC to 16. It also checks disabled issue, restart, and reset with a pending request.

<!-- simulator-examples: rv32i-single-cycle-fetch RV32ISingleCycleFetchStageTest -->

#### Embedded simulation

<div class="simulator-embed"><iframe src="../../../simulator/?scenario=rv32i-single-cycle-fetch&amp;embed=1" title="RV32ISingleCycleFetchStage" loading="lazy" allowfullscreen></iframe></div>

<a href="../../../simulator/?scenario=rv32i-single-cycle-fetch" class="md-button">Open Fetch stage</a>

### Tests

`RV32ISingleCycleFetchStageTest` compares both implementations at recorded checkpoints. `RV32ISingleCycleStageIntegrationTest` checks PC hold during waits and completion-edge updates in the core. `RV32ISingleCycleDocumentationTest.py` checks this pin table and compiles/replays the exact C++ example. Source: `RV32ISingleCycleStages.hpp`, `RV32ISingleCycleStages.cpp`, and `RV32ISingleCycleStagesDirect.cpp`; scenarios: `src/tests/RV32ISingleCycleStageTests.cpp`.
