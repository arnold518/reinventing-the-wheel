# § 4.4.4. Memory Access

`RV32ISingleCycleMemoryStage` formats direct load/store pins and load results. In the core it is named `MEMORY`. It contains **no storage, request registers, clock, or reset state**.

These are stage-local pins. The complete core's combinational `DATA_PORT` converts them to universal REQ/RSP pins for the external zero-wait FF endpoint; it adds no buffering or cycle. The endpoint's controller and native device are outside this stage.

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| `RV32ISingleCycleMemoryStage` | Combinational address/lane formatting, alignment check, and commit-gated store enable | Yes | Yes |

## 4.4.4.1. `RV32ISingleCycleMemoryStage`

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `ISSUE`, `COMMIT` | Input | 1 each | Active memory intent; final permission to commit |
| `MEM_WRITE`, `SIGN_EXTEND` | Input | 1 each | Store direction; signed byte/halfword load |
| `ADDR`, `WRITE_DATA` | Input | 32 each | Effective byte address; unshifted source |
| `SIZE` | Input | 2 | 0 byte, 1 halfword, 2 word, 3 invalid |
| `DMEM_READ_DATA` | Input | 32 | Native word before load-lane selection |
| `DMEM_FAULT` | Input | 1 | Native access fault |
| `READ_DATA` | Output | 32 | Narrowed and sign/zero-extended load value |
| `DONE`, `FAULT`, `MISALIGNED` | Output | 1 each | Constant one; native fault; bad alignment or invalid size |
| `DMEM_WORD_ADDR` | Output | 32 | ADDR/4 native word index |
| `DMEM_WRITE_DATA` | Output | 32 | Source shifted to selected little-endian lanes |
| `DMEM_WRITE_MASK` | Output | 4 | Enabled byte lanes |
| `DMEM_READ_EN`, `DMEM_WE` | Output | 1 each | Active access/read check; authorized store enable |

### Behavior

ADDR[1:0] selects the little-endian byte lane. RV32IMemoryLanes shifts store data, forms the byte mask, and extracts/sign-extends loads. Invalid SIZE=3 gives mask/result zero. Misaligned halfwords/words are reported; there are no split accesses.

DMEM_READ_EN follows ISSUE for both reads and stores. DMEM_WE is ISSUE && MEM_WRITE && COMMIT && !MISALIGNED. In the complete core, ISSUE excludes earlier fetch/decode faults and terminal state, and COMMIT is tied high: the endpoint validates the request and never writes on a fault. Its response must not feed COMMIT, because that would change a rejected write's REQ_OP to read and create a feedback loop. Standalone native-device callers may use COMMIT as an independent store authorization, provided the device rejects invalid writes.

The native memory owns the write clock and captures DMEM_WORD_ADDR/WRITE_DATA/WRITE_MASK on the same rising edge as PC and the register file. Changing this stage's input immediately changes its output after settling: there is no payload capture or response buffer. No store needs its new contents read back to finish.

DONE is constant one because this port has no handshake; it does not detect settling. FAULT follows DMEM_FAULT without ISSUE qualification. The core uses it only for an active memory instruction.

#### Memory compatibility

The connected core requires `MemoryEndpoint<ff,32>`: either the fused behavioral form or an FF controller with native FF storage. `MemoryContainer<ff,32>` supplies scalable storage behind that controller; `FFMemory<32>` supplies a small expanded circuit. Native devices are not wired directly to the core's universal pins. See the [complete-core memory table](core.md#memory-integration).

This stage cannot wait for a delayed response: DONE stays one and there is no pending-request state. The current SRAM, DRAM, ROM, and NOR-flash endpoints therefore require a wait-capable core, not just a controller attached to this stage. Construction guards reject them for the single-cycle system; bypassing those guards can produce incorrect execution rather than additional wait cycles.

#### C++ example

This binary reference reuses only the combinational lane and alignment examples from [4.3.5](../4.3/memory-access.md).

```cpp
#include <cstdint>
namespace single_cycle_example {
struct MemoryInputs {
    bool ISSUE, COMMIT, MEM_WRITE, SIGN_EXTEND;
    uint32_t ADDR, WRITE_DATA;
    unsigned SIZE;
    uint32_t DMEM_READ_DATA;
    bool DMEM_FAULT;
};
struct MemoryOutputs {
    uint32_t READ_DATA;
    bool DONE, FAULT, MISALIGNED;
    uint32_t DMEM_WORD_ADDR, DMEM_WRITE_DATA;
    unsigned DMEM_WRITE_MASK;
    bool DMEM_READ_EN, DMEM_WE;
};
class Memory {
public:
    MemoryOutputs observe(const MemoryInputs& i) const {
        const auto l=memory_access_example::lanes(i.ADDR,i.WRITE_DATA,i.DMEM_READ_DATA,i.SIZE,i.SIGN_EXTEND);
        const bool bad=i.SIZE==3||memory_access_example::misaligned(i.ADDR,i.SIZE);
        return {l.READ_DATA,true,i.DMEM_FAULT,bad,i.ADDR>>2,l.WORD_WRITE_DATA,l.WRITE_MASK,
                i.ISSUE,i.ISSUE&&i.MEM_WRITE&&i.COMMIT&&!bad};
    }
    MemoryOutputs evaluate(const MemoryInputs& i) { return observe(i); }
};
}
```

#### Unknown inputs and timing

The behavioral model preserves each selected load bit and its sign extension, including unknown bits. Unknown neighboring bytes do not invalidate a known byte load. Address, size, and store-data integer conversions are binary abstractions; nonbinary controls/store operands are outside equivalence. Fault bits are forwarded. Structural and behavioral delays differ, but neither requires an extra clock. Allow the entire load-to-writeback path to settle.

### Implementation

#### Structural implementation

LANES performs byte-lane formatting without constructing an unused guarded word-address output. ALIGNMENT plus SIZE_BITS/INVALID_SIZE rejects bad accesses; MEMORY_PORT forms the word index directly. WRITE_INTENT, ALIGNED, WRITE_ALLOWED, and COMMIT_STORE gate stores, so a bad address cannot write a rounded-down word. Constants drive DONE. There is no REQUEST child.

#### Behavioral implementation

BehavioralStage's Memory branch evaluates the same combinational logic without MemoryRequestState or owned contents. Both implementations are registered as `rv32i.single-cycle.memory`.

### Examples

Eleven accesses cover signed/unsigned loads, every store width, invalid size, misalignment, and native faults. Each checks direct outputs, commit permission, changed store data without any edge, and disabled access.

<!-- simulator-examples: rv32i-single-cycle-memory RV32ISingleCycleMemoryStageTest -->

#### Embedded simulation

<div class="simulator-embed"><iframe src="../../../simulator/?scenario=rv32i-single-cycle-memory&amp;embed=1" title="RV32ISingleCycleMemoryStage" loading="lazy" allowfullscreen></iframe></div>

<a href="../../../simulator/?scenario=rv32i-single-cycle-memory" class="md-button">Open Memory stage</a>

### Tests

`RV32ISingleCycleMemoryStageTest` compares both implementations. `RV32ISingleCycleStageIntegrationTest` checks old-source loads/stores with at most one cycle per instruction. `RV32ISingleCycleDocumentationTest.py` executes the exact C++ example. Sources: `RV32ISingleCycleStages.cpp`, `RV32ISingleCycleStagesBehavioral.cpp`, and `RV32ISingleCycleStageTests.cpp`.
