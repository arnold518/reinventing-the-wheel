# § 4.4.5. Writeback

`RV32ISingleCycleWritebackStage` selects a destination-register value. In the core it is named `WRITEBACK`. It contains no register file or clock: the physical write port belongs to [Decode](decode.md).

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| `RV32ISingleCycleWritebackStage` | Select zero, ALU result, load data, or link value | Yes | Yes |

## 4.4.5.1. `RV32ISingleCycleWritebackStage`

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `ALU_RESULT` | Input | 32 | Arithmetic/logic/address result from Execute |
| `READ_DATA` | Input | 32 | Formatted load value from Memory |
| `PC_PLUS_4` | Input | 32 | Link value computed by Fetch |
| `WRITEBACK_SEL` | Input | 2 | Source selector |
| `WB_VALUE` | Output | 32 | Selected register write value |

### Behavior

| WRITEBACK_SEL | WB_VALUE | Typical use |
| ---: | --- | --- |
| 0 | Zero | Instructions without register writeback |
| 1 | ALU_RESULT | Arithmetic, logic, shifts, comparisons, LUI, AUIPC |
| 2 | READ_DATA | Loads |
| 3 | PC_PLUS_4 | JAL/JALR return address |

Every two-bit selector is defined. Selection does not authorize a write. WB_VALUE may change while the combinational path settles; only the final value is captured at the edge when execution status permits REGISTER_WRITE. A no-write instruction can have any WB_VALUE without changing a register. There is no saturation, sign conversion, additional arithmetic, or pending-memory state here.

#### C++ example

```cpp
#include <cstdint>
namespace single_cycle_example {
struct WritebackInputs {
    uint32_t ALU_RESULT, READ_DATA, PC_PLUS_4;
    unsigned WRITEBACK_SEL;
};
struct WritebackOutputs { uint32_t WB_VALUE; };
WritebackOutputs writeback(const WritebackInputs& i) {
    const uint32_t sources[]{0,i.ALU_RESULT,i.READ_DATA,i.PC_PLUS_4};
    return {sources[i.WRITEBACK_SEL]}; // Selector must fit its two-bit pin.
}
}
```

#### Unknown inputs and timing

For a known selector, the behavioral model copies the selected bit vector (or emits zero for selector 0), preserving selected X/Z bits. An unknown selector makes its output X. Structural mux logic can resolve or transform X/Z differently; the registered equivalence scope is settled binary values. Neither implementation stores a result. Behavioral output delay is one abstract tick; structural delay follows the mux tree.

### Implementation

#### Structural implementation

`RESULT_SELECT` is `Mux4to1_32bit`. `ZERO32` drives its input 0; the three result pins drive inputs 1–3. The selector is the decoder's two-bit writeback control.

#### Behavioral implementation

`BehavioralStage`'s Writeback branch selects the same input vector directly. The family is `rv32i.single-cycle.writeback`.

### Examples

The four selections use distinct ALU/load/link patterns so a swapped source cannot pass by coincidence. The initial setup row is also included.

<!-- simulator-examples: rv32i-single-cycle-writeback RV32ISingleCycleWritebackStageTest -->

#### Embedded simulation

<div class="simulator-embed"><iframe src="../../../simulator/?scenario=rv32i-single-cycle-writeback&amp;embed=1" title="RV32ISingleCycleWritebackStage" loading="lazy" allowfullscreen></iframe></div>

<a href="../../../simulator/?scenario=rv32i-single-cycle-writeback" class="md-button">Open Writeback stage</a>

### Tests

`RV32ISingleCycleWritebackStageTest` covers every selector in both implementations. The integration test checks load, ALU, and JALR link writes without updating registers early. `RV32ISingleCycleDocumentationTest.py` compiles/replays this example. Source: `RV32ISingleCycleStages.cpp` and `RV32ISingleCycleStagesBehavioral.cpp`; scenarios: `src/tests/RV32ISingleCycleStageTests.cpp`.
