# § 4.6.4. Prediction Recovery

Recovery compares the next PC actually chosen by Execute with the prediction saved for **that instruction**. Comparing only taken/not-taken is insufficient: the predicted address itself may be wrong, and a taken branch to PC+4 needs no correction.

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| `RV32IBranchRecovery` | Full-address comparison and accepted-resolution qualification | Yes | Yes |
| `RV32IPredictionPath` | Predictor, saved next-PC values, recovery and PC selection | Yes | Yes |

## BranchRecovery

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `VALID`, `PRETERMINAL` | Input | 1 each | Execute slot validity and early-terminal status |
| `ADVANCE`, `KILL` | Input | 1 each | EX/MEM will capture; an older terminal/fault instead kills this slot |
| `IS_BRANCH`, `RST` | Input | 1 each | Conditional-branch kind; reset |
| `PREDICTED_PC`, `FALLTHROUGH_PC`, `TARGET_PC` | Input | 32 each | Saved prediction and the two candidate next addresses |
| `TAKEN` | Input | 1 | Select TARGET_PC for a taken branch or jump, otherwise FALLTHROUGH_PC |
| `MISPREDICT` | Output | 1 | Eligible valid instruction has unequal addresses |
| `RESOLVE` | Output | 1 | Eligible instruction actually leaves Execute without being killed |
| `UPDATE` | Output | 1 | RESOLVE and IS_BRANCH; train once at this edge |
| `RECOVERY`, `BRANCH_MISS` | Output | 1 each | Qualified next-PC correction; conditional-branch subset |

### Behavior and C++ example

MISPREDICT is a request to the coordinator, so it is deliberately **not** gated by ADVANCE. The coordinator may hold it while an older memory request waits. RESOLVE, UPDATE and the observation pulses are gated by actual advancement and older-instruction priority. This avoids both a combinational dependency loop and repeated table training during a stall.

```cpp
--8<-- "site-docs/rv32i/4.6/examples.hpp:recovery"
```

### Implementation

Two structural XOR/reduction networks compare the saved prediction with fallthrough and target in parallel. `TAKEN` selects the one-bit mismatch. Thus branch resolution does not first wait for a 32-bit next-PC mux and then a comparison. Qualification gates generate the outputs. The behavioral implementation selects the same address predicate. This component stores nothing. If target equals fallthrough, a direction difference alone causes no recovery.

### Examples

The table exhausts 256 combinations of validity, terminal status, advancement, kill, instruction kind, reset, selected-address difference and taken direction, followed by two target-equals-fallthrough cases.

<!-- simulator-examples: rv32i-branch-recovery RV32IBranchRecoveryTest -->
<div class="simulator-embed"><iframe src="../../../simulator/?scenario=rv32i-branch-recovery&amp;embed=1" title="Prediction recovery qualification" loading="lazy" allowfullscreen></iframe></div>

## PredictionPath

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `CLK`, `RST`, `IF_ID_WRITE`, `ID_EX_WRITE` | Input | 1 each | Clock/reset and the actual packet-register enables |
| `FETCH_PC`, `FETCH_INSTRUCTION` | Input | 32 each | Current accepted-candidate fetch |
| `EX_PC`, `EX_FALLTHROUGH_PC`, `EX_TARGET_PC` | Input | 32 each | Execute instruction identity and candidate next addresses |
| `EX_CONTROL_TAKEN` | Input | 1 | Taken conditional branch or jump; selects the actual candidate |
| `EX_VALID`, `EX_PRETERMINAL`, `EX_ADVANCE`, `EX_KILL` | Input | 1 each | Recovery qualifications from packet and coordinator |
| `EX_IS_BRANCH`, `EX_TAKEN` | Input | 1 each | Actual conditional-branch kind and outcome |
| `REDIRECT_APPLY` | Input | 1 | Coordinator permits correction of Fetch PC |
| `PREDICTED_NEXT_PC`, `EX_PREDICTED_NEXT_PC` | Output | 32 each | Current lookup and saved Execute-slot prediction |
| `FETCH_NEXT_PC` | Output | 32 | Corrected address when REDIRECT_APPLY, otherwise current prediction |
| `MISPREDICT`, `RESOLVE`, `UPDATE`, `RECOVERY`, `BRANCH_MISS` | Output | 1 each | BranchRecovery outputs |

### Behavior and C++ example

The same `prediction` and `prediction_entries` configuration applies as for BranchPredictor. Two Register<32> components accompany IF/ID and ID/EX; each captures using its packet's write enable. All captures use pre-edge values. A hold preserves both packet and prediction. A bubble may carry stale metadata because its real VALID bit prevents use; separate prediction-valid registers are unnecessary.

```cpp
--8<-- "site-docs/rv32i/4.6/examples.hpp:path"
```

### Implementation

The structural children are `PREDECODE`, `PREDICTOR`, `IF_ID_PREDICTION`, `ID_EX_PREDICTION`, `RECOVERY`, `ACTUAL_NEXT_PC`, and `RECOVERY_PC_SELECT`. The address mux supplies correction data but is not ahead of the mismatch comparison. The compact implementation stores only the same table and two metadata words. It has no architectural register file or instruction history.

### Example: correction while a load waits

Suppose an older load is in MEM and a branch is in EX. The branch resolves to a different next PC, but the load's response is not ready:

| State | MISPREDICT | EX_ADVANCE | UPDATE | Effect |
| --- | --- | --- | --- | --- |
| Data response pending | 1 | 0 | 0 | Hold packets and prediction; do not redirect or retrain |
| Successful response arrives | 1 | 1 | 1 for a conditional branch | Advance older work, correct Fetch, flush younger IF/ID and ID/EX |
| Response is a load fault | May be 1 | 1 with EX_KILL=1 | 0 | Older fault wins; branch cannot advance as valid work or train |

Wrong-path instructions never reach an enabled architectural write. A killed instruction-memory request is drained rather than confused with the redirected fetch; that ownership belongs to the requester in [4.6.5](memory-waits.md).

### Examples

<!-- simulator-examples: rv32i-prediction-path RV32IPredictionPathTest -->
<div class="simulator-embed"><iframe src="../../../simulator/?scenario=rv32i-prediction-path&amp;embed=1" title="Prediction metadata and recovery path" loading="lazy" allowfullscreen></iframe></div>

## Tests

`RV32IBranchRecoveryTest` checks all qualification combinations. `RV32IPredictionPathTest` checks metadata alignment, holds, correction, killed training and lookup/update collision ordering in both fidelities. Program tests exercise conditional branches, direct jumps and register-indirect jumps. `RV32IMemoryMigrationTest` combines prediction with delayed ROM/SRAM endpoints, pause/resume, wrong-path store suppression and loops.
