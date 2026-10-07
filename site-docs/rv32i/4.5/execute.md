# § 4.5.4. Execute

`RV32IBaselineExecuteStage` uses the operands captured in ID/EX. It has no register-file access, forwarding path, or internal sequential state.

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| `RV32IBaselineExecuteStage` | Shared-ALU computation, target resolution, EX/MEM packet | Yes | Yes |

## IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `IN_VALID` | Input | 1 | ID/EX validity |
| `IN_PC`, `IN_INSTRUCTION`, `IN_FETCH_STATUS` | Input | 32 each | Instruction identity and fetch status |
| `IN_RS1_VALUE`, `IN_RS2_VALUE`, `IN_IMMEDIATE` | Input | 32 each | Captured operands and immediate |
| `IN_CONTROL`, `IN_REGISTER_ADDRESSES` | Input | 32 each | Decoded control and register addresses |
| `OUT_VALID` | Output | 1 | Forwarded validity |
| `OUT_PC`, `OUT_INSTRUCTION`, `OUT_FETCH_STATUS`, `OUT_CONTROL`, `OUT_REGISTER_ADDRESSES` | Output | 32 each | Forwarded packet fields |
| `OUT_ALU_RESULT`, `OUT_STORE_DATA` | Output | 32 each | ALU result and captured RS2 |
| `OUT_PC_PLUS_4`, `OUT_NEXT_PC` | Output | 32 each | Link/fallthrough value and resolved next PC |
| `OUT_EXECUTION_STATUS` | Output | 32 | Bit 0: selected target misaligned; other bits zero |
| `TERMINAL`, `CONTROL_FLOW` | Output | 1 each | Valid terminal instruction; valid branch/jump awaiting resolution |

## Behavior

Operand selectors and ALU_OP follow [the single-cycle Execute datapath](../4.4/execute.md). Arithmetic, logic, shifts, comparisons, and address calculations use the same ALU32. Branch comparisons use its comparison outputs. A separate PC+immediate adder works concurrently with branch comparison; JAL and JALR instead reuse the main ALU for their target addition. JALR clears target bit 0.

NEXT_PC is the selected target for a jump or taken branch, otherwise PC+4. Only a selected misaligned target sets EXECUTION_STATUS. An untaken branch with a numerically misaligned candidate target does not trap.

This packet contract does not carry PC+4 in ID/EX, so Execute recomputes it with one incrementer. It is not a second ALU. STORE_DATA always equals the old captured RS2, never a newly written register value.

TERMINAL combines incoming terminal reasons with selected-target misalignment, qualified by IN_VALID. CONTROL_FLOW is asserted for every valid branch/jump, including untaken branches. The controller resumes Fetch only after this resolution cycle.

### C++ example

The [shared Packet type](core.md#binary-example-types) carries instruction fields. `early_terminal` checks validity, the defined fetch/execution status bits, legality, halt, and trap. The arithmetic helper includes ALU32's reserved-opcode zero result.

```cpp
--8<-- "site-docs/rv32i/4.5/examples.hpp:alu"
--8<-- "site-docs/rv32i/4.5/examples.hpp:execute"
```

## Implementation

### Structural implementation

`DATAPATH` reuses RV32ISingleCycleExecuteStage; `NEXT_PC` reuses RV32ISingleCycleNextPCStage. A structural `IncrementBy4`, field/status rewiring, and the shared `PACKET_STATUS` group complete the stage. There is exactly one CPU-specific ALU32 datapath and no separate RS1+immediate target adder.

### Behavioral implementation

`executePacket` evaluates the same binary ALU, branch conditions, targets, and status without a simulator-owned register. The registered family is `rv32i.baseline.execute`. Structural gate delays and behavioral output delay differ; compare after settling.

## Examples

The rows cover arithmetic, both branch outcomes, selected-target alignment, JAL/JALR target reuse, signed/unsigned comparison, and invalid-slot qualification.

<!-- simulator-examples: rv32i-baseline-execute RV32IBaselineExecuteStageTest -->

<div class="simulator-embed"><iframe src="../../../simulator/?scenario=rv32i-baseline-execute&amp;embed=1" title="Baseline Execute stage" loading="lazy" allowfullscreen></iframe></div>

<a href="../../../simulator/?scenario=rv32i-baseline-execute" class="md-button">Open Execute stage</a>

## Tests

`RV32IBaselineExecuteStageTest` compares both implementations; program cases exercise the complete instruction set supported by the reference. Sources: `RV32IBaselinePipeline.cpp`, `RV32IBaselinePipelineDirect.cpp`.
