# § 4.6.1. Operand Forwarding

Forwarding supplies an older instruction's result before its register-file write becomes visible to a younger instruction. It does not read back the younger instruction's own writes.

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| `RV32IForwardingUnit` | Two register comparisons with newest-producer priority | Yes | Yes |
| `RV32IDecodeStage` | Decode/register read with WB bypass and load-use detection | Yes | Yes |
| `RV32IExecuteStage` | Forwarded operands around the shared execution datapath | Yes | Yes |
| `RV32IForwardedOperands` | Internal selector and two operand muxes | Yes | Via enclosing stage |
| `RV32IExecutePacketStatus` | Internal terminal/redirect qualification gates | Yes | Via enclosing stage |

## Forwarding-unit IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `ID_EX_RS1`, `ID_EX_RS2` | Input | 5 each | Execute's source register numbers |
| `EX_MEM_RD`, `MEM_WB_RD` | Input | 5 each | Producer destinations |
| `EX_MEM_VALID`, `MEM_WB_VALID` | Input | 1 each | Producer slot contains an instruction |
| `EX_MEM_REG_WRITE`, `MEM_WB_REG_WRITE` | Input | 1 each | Producer writes a register |
| `EX_MEM_RESULT_READY` | Input | 1 | EX/MEM has a usable ALU/link result, not a pending load |
| `FORWARD_A`, `FORWARD_B` | Output | 2 each | `0`: captured operand; `1`: EX/MEM; `2`: MEM/WB; `3` unused |

## Behavior

For each source, a matching valid EX/MEM writer wins over MEM/WB. If that newer producer is not ready, select the captured operand—not an older WB value for the same register. The load-use interlock prevents the consumer from using that unavailable value in normal execution. Destination x0 never forwards.

The same selected RS2 feeds both ALU input selection and store data. Forwarded operands also feed branch comparisons and JALR target calculation. Decode uses a dedicated WB bypass: two source-register comparisons and two 2:1 data muxes. WB's value can therefore reach an instruction entering ID/EX on the very edge that commits WB.

### C++ example

`Producer.ready` matters only for EX/MEM. `Packet`, the binary ALU, target selection and status checks come from [4.5.4](../4.5/execute.md). Captured x0 operands are zero by the register-file contract.

```cpp
--8<-- "site-docs/rv32i/4.6/examples.hpp:forward"
```

## Stage-interface changes

The common packet inputs/outputs retain the [baseline Decode](../4.5/decode.md) and [Execute](../4.5/execute.md) field meanings. `TERMINAL` on the advanced Execute interface is named `PRETERMINAL`; the baseline's `CONTROL_FLOW` output is replaced by the more specific signals below.

| Stage | Additional or changed pins | Width | Meaning |
| --- | --- | ---: | --- |
| Decode input | `WB_VALID`, `WB_REG_WRITE`, `WB_REGISTER_WRITE` | 1 each | WB packet/write intent for bypass; qualified edge write for the register file |
| Decode input | `EX_VALID`, `EX_CONTROL`, `EX_REGISTER_ADDRESSES` | 1, 32, 32 | Current Execute producer for load-use detection |
| Decode output | `LOAD_USE_HAZARD`, `TERMINAL` | 1 each | Interlock and valid early-terminal instruction |
| Execute input | `EX_MEM_VALID`, `EX_MEM_REG_WRITE`, `EX_MEM_RESULT_READY`, `MEM_WB_VALID`, `MEM_WB_REG_WRITE` | 1 each | Forwarding qualifications |
| Execute input | `EX_MEM_RD`, `MEM_WB_RD` | 5 each | Producer destinations |
| Execute input | `EX_MEM_VALUE`, `MEM_WB_VALUE` | 32 each | Forwardable values |
| Execute output | `IS_BRANCH`, `BRANCH_TAKEN` | 1 each | Conditional-branch kind and actual condition; validity is qualified by recovery |
| Execute output | `REDIRECT`, `PRETERMINAL` | 1 each | Valid selected control transfer; valid early-terminal instruction |
| Execute output | `REDIRECT_PC` | 32 | Actual next PC, including fallthrough |
| Execute output | `CONTROL_TARGET`, `CONTROL_TAKEN` | 32, 1 | Candidate branch/jump destination and its selection, before the next-PC mux; paired with `OUT_PC_PLUS_4` for parallel recovery comparisons |

Decode retains `CLK`, `RST`, `WB_RD<5>` and `WB_VALUE<32>`. Its register-file write input is `WB_REGISTER_WRITE`, not the baseline's `REGISTER_WRITE`. Execute remains combinational. The core uses prediction **mismatch**, not Execute's compatibility `REDIRECT` output, to decide whether to flush.

## Implementation

### Structural implementation

Execute's main groups are `FORWARDED_OPERANDS`, `DATAPATH`, `NEXT_PC`, and `PACKET_STATUS`. `DATAPATH` is the same selectable single-cycle execution component used by the baseline: one ALU32 for arithmetic, address calculation, comparison, and JAL/JALR targets. The separate PC+immediate adder is needed alongside a conditional-branch comparison; PC+4 uses an incrementer. There is no duplicate RS1+immediate target adder.

The forwarding selector uses five-bit equality networks, nonzero checks and priority gates. Muxes select values without storing them. Packet/status rewiring stays separate from the datapath.

### Behavioral implementation

The selector computes the same priority equations; the stage evaluates forwarded operands through the equivalent ALU and next-PC rules. The two internal grouping components have no separately registered behavioral variant: selecting the enclosing stage's behavior replaces their assembly.

## Examples

The selector rows cover both sources, competing producers, x0, invalid producers, and a younger unready producer hiding WB. Execute rows also show forwarded branch operands and shared-ALU JAL/JALR targets.

<!-- simulator-examples: rv32i-forwarding RV32IForwardingUnitTest -->
<div class="simulator-embed"><iframe src="../../../simulator/?scenario=rv32i-forwarding&amp;embed=1" title="Operand forwarding selector" loading="lazy" allowfullscreen></iframe></div>

<!-- simulator-examples: rv32i-pipeline-execute RV32IExecuteStageTest -->
<div class="simulator-embed"><iframe src="../../../simulator/?scenario=rv32i-pipeline-execute&amp;embed=1" title="Grouped advanced Execute" loading="lazy" allowfullscreen></iframe></div>

## Tests

`RV32IForwardingUnitTest`, `RV32IDecodeStageTest`, and `RV32IExecuteStageTest` compare both fidelities. Program lockstep additionally checks arithmetic chains, loads, stores, branches and JALR against the architectural oracle. `RV32IAdvancedExamplesTest` compiles and checks the displayed selector example.
