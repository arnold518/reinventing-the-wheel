# § 4.3.4. Memory Access

`RV32ISingleCycleMemoryStage` converts an effective address and store value into one universal data-memory transaction, then formats the returned load value. In the core it is named `MEMORY`. It contains request state, **not data storage** or a technology controller.

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| `RV32ISingleCycleMemoryStage` | Load/store lanes, alignment/size check, held transaction and response | Yes | Yes |

## 4.3.4.1. `RV32ISingleCycleMemoryStage`

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `CLK`, `RST` | Input | 1 each | Rising-edge request progress; asynchronous active-high requester reset |
| `ISSUE`, `TAKE` | Input | 1 each | Permit capture while idle; consume the response |
| `MEM_WRITE` | Input | 1 | 0 read, 1 ordinary write |
| `SIGN_EXTEND` | Input | 1 | Sign-extend byte/halfword load data |
| `ADDR`, `WRITE_DATA` | Input | 32 each | Effective byte address; unshifted store source |
| `SIZE` | Input | 2 | 0 byte, 1 halfword, 2 word, 3 invalid |
| `DMEM_REQ_READY` | Input | 1 | Request acceptance permission |
| `DMEM_RSP_VALID` | Input | 1 | Held endpoint response |
| `DMEM_RSP_READ_DATA` | Input | 32 | Response word, before lane selection |
| `DMEM_RSP_STATUS` | Input | 3 | Zero success; nonzero fault |
| `READ_DATA` | Output | 32 | Shifted and sign/zero-extended load result |
| `DONE`, `FAULT`, `MISALIGNED` | Output | 1 each | Matching response; nonzero status; bad address alignment **or invalid size** |
| `DMEM_REQ_VALID`, `DMEM_RSP_READY` | Output | 1 each | Held request; response consumption/draining permission |
| `DMEM_REQ_OP` | Output | 3 | Captured 0 read or 1 write |
| `DMEM_REQ_ADDR` | Output | 32 | Captured endpoint byte address, normally aligned to four bytes |
| `DMEM_REQ_WRITE_DATA` | Output | 32 | Captured store bits placed in byte lanes |
| `DMEM_REQ_WRITE_MASK` | Output | 4 | Captured byte-lane enables |

### Behavior

`ADDR[1:0]` selects a byte lane in the little-endian word. The formatting rules are those of [RV32IMemoryLanes](../4.2/memory-access.md): left-shift store data, form a byte mask, right-shift the response, narrow to the selected width, and sign/zero-extend. Word loads preserve all selected 32 bits; invalid size gives read result/mask zero. The output is computed even for stores, but it is not a stored-data read-back.

`MISALIGNED` is true for odd halfwords, non-word-aligned words, **or SIZE=3 at any address**. This differs from the bare alignment building block, which does not reject an aligned SIZE=3 by itself. For an invalid access the formatter sets bit 0 of the endpoint word address, deliberately producing a rejected transaction instead of silently accessing a neighboring aligned word. There is no split access across word boundaries. The complete core selects the architectural trap cause through execution status.

On an idle rising edge with `ISSUE=1`, `REQUEST` captures operation, formatted address/data/mask, and the original effective address as its internal tag. These request fields remain stable under backpressure even if input store data changes. Acceptance clears request-valid; a matching response raises `DONE`. `TAKE` permits consuming it. Dropping `ISSUE` does not withdraw a pending request. Tag changes and sticky abandonment follow [4.2.6's requester rules](../4.2/memory-access.md#4263-rv32imemoryrequest).

Load formatting uses the **current** ADDR, SIZE, and SIGN_EXTEND, not separately latched load metadata. The core therefore keeps the instruction and architectural sources unchanged until completion. Other callers must do the same; changing only SIZE while waiting does not retag a transaction. `READ_DATA` follows the response pins and `FAULT` follows response status without DONE qualification. Consume their architectural meaning only when DONE is high.

Reset clears the requester, not external storage. Already accepted endpoint effects are not rolled back by a core reset; system integration must follow the [memory reset contract](../../memory/3.2.md).

#### C++ example

Reuse `memory_access_example::lanes`, `misaligned`, and `Request` from [4.2.6](../4.2/memory-access.md). This binary reference includes all pins, the invalid-size check, request state, payload hold, and returned lane selection. Start with reset asserted.

```cpp
#include <cstdint>
namespace single_cycle_example {
struct MemoryInputs {
    bool CLK, RST, ISSUE, TAKE, MEM_WRITE, SIGN_EXTEND;
    uint32_t ADDR, WRITE_DATA;
    unsigned SIZE;
    bool DMEM_REQ_READY, DMEM_RSP_VALID;
    uint32_t DMEM_RSP_READ_DATA;
    unsigned DMEM_RSP_STATUS;
};
struct MemoryOutputs {
    uint32_t READ_DATA;
    bool DONE, FAULT, MISALIGNED, DMEM_REQ_VALID, DMEM_RSP_READY;
    unsigned DMEM_REQ_OP;
    uint32_t DMEM_REQ_ADDR, DMEM_REQ_WRITE_DATA;
    unsigned DMEM_REQ_WRITE_MASK;
};
class Memory {
    memory_access_example::Request request;
    memory_access_example::RequestInputs inputs(const MemoryInputs& i) const {
        const auto l=memory_access_example::lanes(i.ADDR,i.WRITE_DATA,0,i.SIZE,i.SIGN_EXTEND);
        return {i.CLK,i.RST,i.ISSUE,i.TAKE,i.ADDR,l.WORD_ADDR,l.WORD_WRITE_DATA,
                unsigned(i.MEM_WRITE),l.WRITE_MASK,i.DMEM_REQ_READY,i.DMEM_RSP_VALID,
                i.DMEM_RSP_READ_DATA,i.DMEM_RSP_STATUS};
    }
public:
    MemoryOutputs observe(const MemoryInputs& i) const {
        const auto r=request.observe(inputs(i));
        const auto l=memory_access_example::lanes(i.ADDR,i.WRITE_DATA,r.READ_DATA,i.SIZE,i.SIGN_EXTEND);
        const bool bad=i.SIZE==3 || memory_access_example::misaligned(i.ADDR,i.SIZE);
        return {l.READ_DATA,r.DONE,r.FAULT,bad,r.BUS_REQ_VALID,r.BUS_RSP_READY,
                r.BUS_REQ_OP,r.BUS_REQ_ADDR,r.BUS_REQ_WRITE_DATA,r.BUS_REQ_WRITE_MASK};
    }
    MemoryOutputs evaluate(const MemoryInputs& i) {
        request.evaluate(inputs(i));
        return observe(i);
    }
};
}
```

#### Unknown inputs and timing

Structural behavior follows lane gates, registers, and requester logic. The compact model preserves individual response-data bits during lane extraction and sign extension, so a known byte can be read from a word with unknown neighboring bytes. Address/size/store-data conversions and protocol state are binary abstractions; a nonbinary integer input falls back to zero. Unknown controls and X/Z store capture are outside stage equivalence. Unknown response status gives FAULT=X. Neither the example nor the behavioral delay models physical memory latency: the external endpoint and handshake determine the wait.

### Implementation

#### Structural implementation

`LANES` is `RV32IMemoryLanes`; `REQUEST` is `RV32IMemoryRequest`. `ALIGNMENT` checks address bits, while `SIZE_BITS`, `INVALID_SIZE`, and `BAD_ADDRESS_OR_SIZE` add explicit size-3 rejection. `OPERATION` packs MEM_WRITE into the low operation bit; `ZERO` supplies the other bits. These eight children implement formatting/control, not an SRAM/DRAM array.

#### Behavioral implementation

`DirectStage`'s Memory branch uses `MemoryRequestState`, word-address/mask calculations, and bit-vector response formatting. It has the same `DMEM_` endpoint interface and no owned memory contents. Both implementations belong to `rv32i.single-cycle.memory`.

### Examples

Eleven transactions cover signed/unsigned byte and halfword loads, word loads, all store widths, misalignment, invalid size, and access faults. Each transaction checks capture, held payload, acceptance, held response, and consumption. The table includes setup and clock-preparation rows, not only eleven result rows.

<!-- simulator-examples: rv32i-single-cycle-memory RV32ISingleCycleMemoryStageTest -->

#### Embedded simulation

<div class="simulator-embed"><iframe src="../../../simulator/?scenario=rv32i-single-cycle-memory&amp;embed=1" title="RV32ISingleCycleMemoryStage" loading="lazy" allowfullscreen></iframe></div>

<a href="../../../simulator/?scenario=rv32i-single-cycle-memory" class="md-button">Open Memory stage</a>

### Tests

`RV32ISingleCycleMemoryStageTest` compares both implementations. `RV32ISingleCycleStageIntegrationTest` verifies `lw x1,0(x1)` and exact store effects, while `RV32IMemoryMigrationTest` exercises actual interchangeable endpoints. `RV32ISingleCycleDocumentationTest.py` checks the table and exact C++ example. Source: `RV32ISingleCycleStages.cpp` and `RV32ISingleCycleStagesDirect.cpp`; scenarios: `src/tests/RV32ISingleCycleStageTests.cpp`.
