# § 4.5.3. Decode and Register Read

`RV32IBaselineDecodeStage` decodes IF/ID and reads the architectural register file. Writes come from the older instruction in Writeback, not from the instruction currently being decoded.

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| `RV32IBaselineDecodeStage` | Instruction decoding, two register reads, independent WB write port | Yes | Yes |

## IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `CLK`, `RST` | Input | 1 each | Register-file clock and asynchronous reset |
| `IN_VALID` | Input | 1 | IF/ID validity |
| `IN_PC`, `IN_INSTRUCTION`, `IN_FETCH_STATUS` | Input | 32 each | IF/ID payload |
| `REGISTER_WRITE`, `WB_RD`, `WB_VALUE` | Input | 1, 5, 32 | Authorized WB write, its destination, its value |
| `OUT_VALID` | Output | 1 | Incoming validity |
| `OUT_PC`, `OUT_INSTRUCTION`, `OUT_FETCH_STATUS` | Output | 32 each | Forwarded packet fields |
| `OUT_RS1_VALUE`, `OUT_RS2_VALUE`, `OUT_IMMEDIATE` | Output | 32 each | Current source values and sign-extended immediate |
| `OUT_CONTROL`, `OUT_REGISTER_ADDRESSES` | Output | 32 each | [Packed control and register addresses](registers.md#packet-encodings) |
| `TERMINAL`, `CONTROL_FLOW` | Output | 1 each | Valid packet needs precise stopping; valid branch/JAL/JALR |

## Behavior

The decoder uses the [instruction decoding rules](../4.3/instruction-decoder.md). The register file resets to zero, writes a nonzero WB_RD on an enabled rising edge, and ignores writes to x0. Both source reads are combinational.

There is **no writeback bypass**. Before a writeback edge the old register value remains on the read port. A consumer therefore stays in IF/ID until its producer has passed Writeback. ID/EX captures source operands only when control releases the dependency stall.

TERMINAL is IN_VALID && (fetch fault || PC misalignment || illegal instruction || halt request || trap request). CONTROL_FLOW is IN_VALID && (nonzero branch type || nonzero jump type). These are requests to pipeline control, not immediate architectural state changes. All ordinary decoder outputs are still computed for an invalid packet.

### C++ example

The instruction decoder and control encoder below are the same reusable, non-simulator functions documented in 4.3. Packet fields and packed encodings are defined in [4.5.1](registers.md). The register update uses WB_RD independently of the decoded RD.

```cpp
--8<-- "site-docs/rv32i/4.5/examples.hpp:decode"
```

## Implementation

### Structural implementation

`CONTROL` is RV32IDecodeControlUnit and `REGISTER_FILE` is the FF-backed RegisterFile32x32. Rewire components form CONTROL, REGISTER_ADDRESSES, and forwarded packet fields. `PACKET_STATUS` groups the shared terminal/control-flow gates; Decode supplies zero execution status because target calculation has not occurred yet. No forwarding selector or WB_BYPASS component is instantiated.

### Behavioral implementation

`DirectDecode` uses the shared `DirectStage` implementation to store the 32 registers and their history, update the WB destination on a known rising edge, then decode and read sources. Both forms expose architectural register-state access for core snapshots and program verification, and share `rv32i.baseline.decode`. Reset precedes binary equivalence checks.

## Examples

The scenario writes x1 while decoding an instruction whose destination is x7, checks old-value reads before the edge, checks x0, and exercises branch/terminal qualification.

<!-- simulator-examples: rv32i-baseline-decode RV32IBaselineDecodeStageTest -->

<div class="simulator-embed"><iframe src="../../../simulator/?scenario=rv32i-baseline-decode&amp;embed=1" title="Baseline Decode stage" loading="lazy" allowfullscreen></iframe></div>

<a href="../../../simulator/?scenario=rv32i-baseline-decode" class="md-button">Open Decode stage</a>

## Tests

`RV32IBaselineDecodeStageTest` compares both implementations. Program tests check the architectural register file at every retirement. Sources: `RV32IBaselinePipeline.cpp`, `RV32IBaselinePipelineDirect.cpp`.
