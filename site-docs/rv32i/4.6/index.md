# § 4.6. Advanced Pipeline

`RV32IFiveStageCore` keeps the five stages and four packet registers from [the baseline](../4.5/index.md). It changes how operands arrive, how Fetch chooses its next address, and when the pipeline advances. It is still a single-issue, in-order RV32I core—not a new instruction set or an out-of-order processor.

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| `RV32IForwardingUnit` | Select the newest available operand | Yes | Yes |
| `RV32IHazardDetectionUnit` | Detect the remaining immediate load dependency | Yes | Yes |
| `RV32IPredictionCounter` | One-bit / saturating two-bit state transition | Yes | Yes |
| `RV32IBranchPredecode` | Extract fetch-time conditional and direct-jump targets | Yes | Yes |
| `RV32IBranchPredictor` | Switchable direction policy and indexed state | Yes | Yes |
| `RV32IBranchRecovery` | Compare saved and actual next PC; qualify updates | Yes | Yes |
| `RV32IPredictionPath` | Group prediction, packet metadata, and recovery | Yes | Yes |
| `RV32IPipelineCoordinator` | Combine waits, hazards, corrections, and terminal priority | Yes | Yes |
| `RV32IFiveStageCore` | Complete advanced pipeline with universal memory ports | Yes | Yes |

## Changes from the baseline

| Boundary | Retained | Added or changed |
| --- | --- | --- |
| Fetch | PC state and instruction acceptance | Predict next PC; retain one outstanding request when memory waits |
| Decode | Decoder, register file, packed controls | WB-to-ID bypass; stall only the immediate load-use dependency |
| Execute | Shared `RV32ISingleCycleExecuteStage` and Next-PC datapath | EX/MEM and MEM/WB operand forwarding; expose actual branch direction |
| Memory | Alignment, load/store lanes, in-order effects | Transaction completion can hold the pipeline |
| Writeback | Combinational `RV32IWritebackStage`, separate execution status | No new WB register or retirement-history circuit |
| Pipeline registers | IF/ID, ID/EX, EX/MEM, MEM/WB contracts | Two parallel 32-bit next-PC metadata registers; same enables, no additional stage |

The ALU, register file, instruction decoding, packet encoding, and architectural retirement rules remain those documented in §§4.2–4.5. [Forwarding](forwarding.md) describes the changed Decode/Execute interfaces; [memory waits](memory-waits.md) describes Fetch/Memory coordination.

<!-- diagram: advanced-prediction -->

## Section map

| Section | Main contract |
| --- | --- |
| [4.6.1. Operand Forwarding](forwarding.md) | Producer priority, WB bypass, grouped Execute |
| [4.6.2. Load-Use Hazards](hazards.md) | Hold the consumer, insert a bubble, advance the producer |
| [4.6.3. Branch Prediction](prediction.md) | Predecode, five policies, FF counter table |
| [4.6.4. Prediction Recovery](recovery.md) | Saved prediction, correction, flush and training priority |
| [4.6.5. Memory Wait Handling](memory-waits.md) | Zero-wait and delayed endpoints; movement coordinator |
| [4.6.6. Complete Advanced Core](core.md) | Assembly, configuration, observations and program checks |

Each unblocked instruction advances one stage at each rising edge. After filling, an independent instruction stream can retire one instruction per cycle. A single instruction still crosses five stages. Hazards, prediction misses, and memory waits reduce throughput.

Both fidelities implement the same settled binary signals and clock-edge behavior after reset. Gate propagation times are not a measured hardware frequency. Caches, BTBs, return-address stacks, speculative memory writes, and multiple outstanding requests are outside this section.
