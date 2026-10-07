# § 4.4.1. Fetch

`RV32ISingleCycleFetchStage` owns the architectural PC, computes PC+4, and exposes a direct instruction-memory read. It contains neither instruction storage nor a pending request. In the core it is named `FETCH`.

The direct pins are local to the stage. The complete core's combinational `INSTRUCTION_PORT` converts them to universal REQ/RSP pins for the external zero-wait FF endpoint. This formatting does not capture a request or add a cycle.

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| `RV32ISingleCycleFetchStage` | PC register, sequential address, direct instruction read, alignment/fault status | Yes | Yes |

## 4.4.1.1. `RV32ISingleCycleFetchStage`

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `CLK`, `RST` | Input | 1 each | Rising-edge PC capture; asynchronous active-high PC reset |
| `ISSUE` | Input | 1 | Instruction-memory read enable |
| `PC_WRITE` | Input | 1 | Authorize PC capture |
| `NEXT_PC` | Input | 32 | Candidate PC |
| `IMEM_READ_DATA` | Input | 32 | Current instruction word |
| `IMEM_FAULT` | Input | 1 | Native memory access fault |
| `PC`, `PC_PLUS_4`, `INSTRUCTION` | Output | 32 each | Stored PC, PC+4, directly forwarded instruction |
| `DONE`, `FAULT`, `PC_MISALIGNED` | Output | 1 each | Constant one; forwarded access fault; PC low bits nonzero |
| `IMEM_WORD_ADDR` | Output | 32 | PC/4 word index |
| `IMEM_READ_EN` | Output | 1 | Follows ISSUE |

### Behavior

RST clears PC to zero. A known rising edge with PC_WRITE=1 captures NEXT_PC; otherwise PC holds. Changing NEXT_PC while CLK stays high cannot update PC again.

Instruction data, fault, PC+4, and the word index are available after combinational settling. DONE is constant one: it denotes a direct port without a handshake, **not** a measured propagation-ready signal. The caller must allow the path to settle before the edge.

PC_MISALIGNED reports the original byte PC's low two bits. Dividing by four for the native word address does not legalize a misaligned fetch; execution status traps it. INSTRUCTION and FAULT are forwarded without qualification. The complete core gates their architectural use through execution status.

#### C++ example

This binary reference contains the entire PC state transition and direct pin behavior. It has no transaction state. Reset before execution.

```cpp
#include <cstdint>
namespace single_cycle_example {
struct FetchInputs {
    bool CLK, RST, ISSUE, PC_WRITE;
    uint32_t NEXT_PC, IMEM_READ_DATA;
    bool IMEM_FAULT;
};
struct FetchOutputs {
    uint32_t PC, PC_PLUS_4, INSTRUCTION;
    bool DONE, FAULT, PC_MISALIGNED;
    uint32_t IMEM_WORD_ADDR;
    bool IMEM_READ_EN;
};
class Fetch {
    uint32_t pc=0;
    bool previous_clock=false;
public:
    FetchOutputs observe(const FetchInputs& i) const {
        return {pc,pc+4,i.IMEM_READ_DATA,true,i.IMEM_FAULT,bool(pc&3),pc>>2,i.ISSUE};
    }
    FetchOutputs evaluate(const FetchInputs& i) {
        if(i.RST) pc=0;
        else if(!previous_clock&&i.CLK&&i.PC_WRITE) pc=i.NEXT_PC;
        previous_clock=i.CLK;
        return observe(i);
    }
};
}
```

#### Unknown inputs and timing

Equivalence covers binary operation after reset. Instruction/fault bits are forwarded directly. The behavioral PC model recognizes known low-to-high edges; unknown integer inputs fall back to zero and are outside the PC equivalence claim. One behavioral output tick need not equal structural propagation delay. Memory must settle within the CPU period; there is no variable-latency wait mechanism here.

### Implementation

#### Structural implementation

`PC_STATE` is Register<32>, `PC_PLUS_4_ADD` is a structural `IncrementBy4`, and `ALIGNMENT` checks the byte PC. The incrementer passes bits 1:0 through and uses a parallel carry-prefix network for bits 31:2; it needs no general second operand, carry-in, or unused carry-out. `MEMORY_PORT` is a zero-delay Rewire: it drops the address's two low bits for word indexing and forwards data, fault, and enable. The only architectural storage is PC_STATE. There is no REQUEST child.

#### Behavioral implementation

The Fetch branch of BehavioralStage stores PC and previous clock, then publishes the same direct outputs. Both forms are registered as `rv32i.single-cycle.fetch`.

### Examples

The scenario reads without a clock, holds PC before commit, captures NEXT_PC once per rising edge, checks enable/fault forwarding, and resets PC.

<!-- simulator-examples: rv32i-single-cycle-fetch RV32ISingleCycleFetchStageTest -->

#### Embedded simulation

<div class="simulator-embed"><iframe src="../../../simulator/?scenario=rv32i-single-cycle-fetch&amp;embed=1" title="RV32ISingleCycleFetchStage" loading="lazy" allowfullscreen></iframe></div>

<a href="../../../simulator/?scenario=rv32i-single-cycle-fetch" class="md-button">Open Fetch stage</a>

### Tests

`RV32ISingleCycleFetchStageTest` compares both implementations. `RV32ISingleCycleStageIntegrationTest` checks one-cycle completion and old-state operand use. `RV32ISingleCycleDocumentationTest.py` checks every pin and executes this exact C++ example. Sources: `RV32ISingleCycleStages.cpp`, `RV32ISingleCycleStagesBehavioral.cpp`, and `RV32ISingleCycleStageTests.cpp`.
