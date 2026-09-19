# § 4.3.2. Decode and Register Read

`RV32ISingleCycleDecodeStage` decodes the current instruction and owns the core's only architectural register file. It reads both sources continuously and accepts the selected writeback value at the completion edge. In the core it is named `DECODE`.

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| `RV32ISingleCycleDecodeStage` | Instruction controls, two source reads, and one destination write | Yes | Yes |

## 4.3.2.1. `RV32ISingleCycleDecodeStage`

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `INSTRUCTION` | Input | 32 | Current endpoint response word; held through completion by Fetch's protocol |
| `CLK`, `RST` | Input | 1 each | Rising-edge register write; asynchronous active-high reset |
| `REGISTER_WRITE` | Input | 1 | Qualified completion write enable, not decoded intent |
| `WB_VALUE` | Input | 32 | Value selected by Writeback |
| `RS1_VALUE`, `RS2_VALUE` | Output | 32 each | Values selected from the architectural register file |
| `RS1_ADDR`, `RS2_ADDR`, `RD_ADDR` | Output | 5 each | Raw instruction fields |
| `IMM` | Output | 32 | Constructed immediate |
| `ALU_OP` | Output | 5 | ALU32 operation, 0–12 for supported operations |
| `ALU_A_SEL`, `ALU_B_SEL` | Output | 2 each | A: 0 RS1, 1 PC, 2 zero; B: 0 RS2, 1 immediate, 2 zero |
| `LEGAL`, `REG_WRITE`, `MEM_READ`, `MEM_WRITE` | Output | 1 each | Legality and decoded update/access intent |
| `WRITEBACK_SEL` | Output | 2 | 0 none/zero, 1 ALU, 2 load, 3 PC+4 |
| `MEM_SIZE` | Output | 2 | 0 byte, 1 halfword, 2 word/default |
| `LOAD_SIGN_EXTEND` | Output | 1 | Signed byte/halfword load |
| `USES_RS1`, `USES_RS2` | Output | 1 each | Whether the corresponding raw field is an instruction dependency |
| `BRANCH_TYPE` | Output | 3 | 0 none, 1 EQ, 2 NE, 3 signed LT, 4 signed GE, 5 unsigned LT, 6 unsigned GE |
| `JUMP_TYPE` | Output | 2 | 0 none, 1 JAL, 2 JALR |
| `HALT_REQUEST`, `TRAP_REQUEST` | Output | 1 each | Decoded terminal intent |
| `DECODE_TRAP_CAUSE` | Output | 4 | 0 none, 1 illegal instruction, 2 ECALL; project codes |

### Behavior

All decoder outputs follow the complete recognition, immediate, and fallback rules in [4.2.4](../4.2/instruction-decoder.md). They are not latched here. Source and destination addresses remain raw fields even when an instruction does not use them. Both source outputs still read their raw addresses; `USES_RS1/2` describes dependency, not read gating.

Reset clears x1–x31; x0 is always zero. On a rising edge, `REGISTER_WRITE=1` writes `WB_VALUE` to the current instruction's nonzero `RD_ADDR`. This input is already qualified by [execution status](../4.2/execution-status.md). Decode does **not** internally AND it with `LEGAL` or `REG_WRITE`: those are separately exposed intent outputs. The core is responsible for suppressing writes on stalls and faults.

There is no WB-to-read bypass. If the destination is also a source, reads show old storage before capture and updated storage after capture/propagation. Changing `WB_VALUE` while CLK remains high does not write again. Decode contains neither an instruction register nor a pipeline hazard/forwarding unit.

#### C++ example

Reuse `decode_example::Decoded` and `decode_example::decode` from [4.2.4](../4.2/instruction-decoder.md). Inheritance below forwards **every** decoder output; only the two register-value fields are added. This is the binary, post-reset model of the [register-file contract](../4.2/register-file.md), not a redefinition of instruction decoding.

```cpp
#include <array>
#include <cstdint>
namespace single_cycle_example {
struct DecodeInputs {
    uint32_t INSTRUCTION;
    bool CLK, RST, REGISTER_WRITE;
    uint32_t WB_VALUE;
};
struct DecodeOutputs : decode_example::Decoded {
    uint32_t RS1_VALUE=0, RS2_VALUE=0;
};
class Decode {
    std::array<uint32_t,32> registers{};
    bool previous_clock=false;
public:
    DecodeOutputs observe(const DecodeInputs& i) const {
        const auto d=decode_example::decode(i.INSTRUCTION);
        DecodeOutputs out;
        static_cast<decode_example::Decoded&>(out)=d;
        out.RS1_VALUE=registers[d.RS1_ADDR];
        out.RS2_VALUE=registers[d.RS2_ADDR];
        return out;
    }
    DecodeOutputs evaluate(const DecodeInputs& i) {
        const unsigned rd=(i.INSTRUCTION>>7)&31;
        if (i.RST) registers.fill(0);
        else if (!previous_clock && i.CLK && i.REGISTER_WRITE && rd!=0)
            registers[rd]=i.WB_VALUE;
        previous_clock=i.CLK;
        return observe(i);
    }
    const std::array<uint32_t,32>& state() const { return registers; }
};
}
```

#### Unknown inputs and timing

Real simulator x1–x31 start unknown until reset; the simplified array is only a post-reset reference. Structural nonbinary behavior follows the decoder and register-file children. The behavioral stage makes all outputs unknown for a nonbinary instruction and does not write a destination in that case; reset can still clear registers. Known enabled writes copy the WB bit vector. Unknown clock/reset/control cases and captured Z behavior are outside binary stage equivalence. Outputs have one abstract delay tick; recorded register history becomes visible at that output time, not retroactively.

### Implementation

#### Structural implementation

`CONTROL` is `RV32IDecodeControlUnit`; `REGISTER_FILE` is `RegisterFile32x32`. Raw addresses connect to the register file, `WB_VALUE` to its write-data input, and `REGISTER_WRITE` to its write enable. The register file reuses the FF bank/word/cell hierarchy from chapter 3. No second copy of its storage is kept in the stage.

#### Behavioral implementation

`DirectDecodeStage` uses the direct stage evaluator, a 32-word four-state register vector, and timestamped register snapshots. It computes controls through `RV32IDecoder`/`RV32IControl` and implements writes on known rising edges. Both implementations expose `register-state-view` so the visualizer can inspect registers without depending on the internal hierarchy. The family is `rv32i.single-cycle.decode`.

### Examples

The scenario reuses every decoder vector, then checks register setup without bypass, edge-only capture, high/falling-clock holds, ignored x0 writes, and reset. For example, setting `WB_VALUE=7` and write enable does not change x1 until the capture edge.

<!-- simulator-examples: rv32i-single-cycle-decode RV32ISingleCycleDecodeStageTest -->

#### Embedded simulation

<div class="simulator-embed"><iframe src="../../../simulator/?scenario=rv32i-single-cycle-decode&amp;embed=1" title="RV32ISingleCycleDecodeStage" loading="lazy" allowfullscreen></iframe></div>

<a href="../../../simulator/?scenario=rv32i-single-cycle-decode" class="md-button">Open Decode stage</a>

### Tests

`RV32ISingleCycleDecodeStageTest` compares both implementations, including every forwarded control. `RV32ISingleCycleStageIntegrationTest` exercises overlapping source/destination registers and unchanged registers during memory waits. Native/WASM parity checks also inspect register history. `RV32ISingleCycleDocumentationTest.py` verifies this table and replays the exact example. Source: `RV32ISingleCycleStages.cpp` and `RV32ISingleCycleStagesDirect.cpp`; scenarios: `src/tests/RV32ISingleCycleStageTests.cpp`.
