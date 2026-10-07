# § 4.5.6. Writeback and Retirement

Writeback is combinational, just like [single-cycle Writeback](../4.4/writeback.md). The preceding MEM/WB register holds the instruction's results; the register file in Decode captures the selected value. There is no result register or completed-PC register inside Writeback.

Stopping state belongs to a separate execution-status block, shared by the baseline and advanced pipelines.

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| `RV32IWritebackStage` | Combinational result selection and retirement permission | Yes | Yes |
| `RV32IPipelineExecutionStatus` | Six persistent halt/trap status bits outside the datapath | Yes | Yes |

## Writeback

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `COMMIT_ENABLE` | Input | 1 | Permission from pipeline control |
| `IN_VALID` | Input | 1 | MEM/WB validity |
| `IN_ALU_RESULT`, `IN_MEMORY_DATA`, `IN_PC_PLUS_4`, `IN_CONTROL`, `IN_REGISTER_ADDRESSES`, `IN_FETCH_STATUS`, `IN_EXECUTION_STATUS`, `IN_MEMORY_STATUS` | Input | 32 each | Result, destination/control and fault fields from MEM/WB |
| `WB_VALID`, `WB_REG_WRITE` | Output | 1 each | Incoming validity and decoded write intent, independent of commit permission |
| `REGISTER_WRITE`, `WB_RD`, `WB_VALUE` | Output | 1, 5, 32 | Qualified register-file write port |
| `TERMINAL`, `RETIRE_TERMINAL`, `COMMIT_VALID` | Output | 1 each | Pending valid halt/trap, permitted terminal retirement, permitted retirement |
| `HALT_EVENT`, `TRAP_EVENT`, `TRAP_CAUSE` | Output | 1, 1, 4 | Qualified stop events and combinational cause; not stored status |

### Behavior

WRITEBACK_SEL chooses zero (0), ALU_RESULT (1), MEMORY_DATA (2), or PC_PLUS_4 (3). WB_RD comes from the retiring packet. These outputs settle whenever the inputs change: **no clock is needed**.

COMMIT_VALID is COMMIT_ENABLE && IN_VALID. REGISTER_WRITE additionally requires decoded write intent and no terminal condition. These are permissions sampled by receiving registers at the next rising edge, not one-tick completion pulses. The register file enforces x0's hardwired-zero rule.

Fault priority is: PC misalignment → instruction access fault → illegal instruction → decoded trap → load misalignment/access fault → store misalignment/access fault → selected-target misalignment. Halt is considered only when there is no trap. [Machine-model trap codes](../4.1/machine.md) are used, not privileged RISC-V CSRs.

TERMINAL identifies a valid stopping packet even when COMMIT_ENABLE=0. HALT_EVENT and TRAP_EVENT require commit permission, and RETIRE_TERMINAL is their OR. TRAP_CAUSE is unqualified: it describes the current fault/control fields even when IN_VALID or COMMIT_ENABLE is low. It is captured only with TRAP_EVENT. A bubble suppresses events; changing the fault fields changes the cause after settling. Nothing sticks here.

There is no CLK, RST, or IN_NEXT_PC input. The enclosing controller suppresses COMMIT_ENABLE during reset, pause and after stopping. Stores have already taken place at the MEM edge; Writeback does not repeat them.

#### C++ example

This pure function describes all binary outputs. `Packet` and its packed control fields are shared with the other examples.

```cpp
--8<-- "site-docs/rv32i/4.5/examples.hpp:writeback"
```

### Implementation

#### Structural implementation

`WRITEBACK_MUX` selects the result. `RETIREMENT` is an `RV32IPipelineRetirementUnit` that qualifies completion, register writes and stop events. Rewire blocks unpack fields and pass validity through; gates report pending terminal conditions. **There are zero stored bits in this stage.**

#### Behavioral implementation

`RV32IWritebackStageDirect` evaluates the same selection, fault priority and permissions without retaining state. Both implementations share `rv32i.pipeline.stage.writeback`.

### Examples

The scenario changes inputs without clock edges: ALU/load/link selection, disabled commits, bubbles, halt and every trap cause. Each terminal case is followed by another packet or bubble to verify that events do not stick.

<!-- simulator-examples: rv32i-pipeline-writeback RV32IWritebackStageTest -->

<div class="simulator-embed"><iframe src="../../../simulator/?scenario=rv32i-pipeline-writeback&amp;embed=1" title="Combinational pipeline Writeback" loading="lazy" allowfullscreen></iframe></div>

<a href="../../../simulator/?scenario=rv32i-pipeline-writeback" class="md-button">Open Writeback stage</a>

### Tests

`RV32IWritebackStageTest` checks both implementations. The documentation test checks its complete pin contract and verifies that its hierarchy contains no registers or flip-flops. `RV32IBaselineExamplesTest` replays the same rows through the C++ example.

## Execution status

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `CLK`, `RST` | Input | 1 each | Rising-edge capture; asynchronous active-high reset |
| `HALT_EVENT`, `TRAP_EVENT` | Input | 1 each | Writeback's permitted stopping events |
| `EVENT_TRAP_CAUSE` | Input | 4 | Writeback's combinational cause |
| `HALTED`, `TRAPPED`, `TRAP_CAUSE` | Output | 1, 1, 4 | Persistent stopping status |

### Behavior

Reset clears all six bits, regardless of the clock or event inputs. Otherwise, a rising edge sets HALTED when HALT_EVENT=1, and sets TRAPPED and captures EVENT_TRAP_CAUSE when TRAP_EVENT=1. Each flag holds until reset; cause holds until another trap event or reset. Falling edges and changes to cause alone do not update state.

The two event inputs are independent; asserting both sets both flags. Writeback itself never generates both together because a trap takes priority over a halt. The complete core feeds HALTED/TRAPPED back to control, preventing further retirement once either is set. Four cause bits are simulator reporting state, not a privileged exception mechanism.

#### C++ example

```cpp
--8<-- "site-docs/rv32i/4.5/examples.hpp:execution-status"
```

### Implementation

#### Structural implementation

`HALTED_STATE` and `TRAPPED_STATE` are individual `FlipFlopCell`s. `TRAP_CAUSE_WORD` is a four-bit `Register<4>` sharing the trap-capture enable, clock, and reset. The block appears as `EXECUTION_STATUS` beside pipeline control, not between MEM/WB and the register file.

#### Behavioral implementation

`RV32IPipelineExecutionStatusDirect` retains the same six status bits and detects rising edges, with asynchronous reset taking priority. Family: `rv32i.pipeline.execution-status`.

### Examples

The scenario checks edge-only capture, sticky flags, reset, every four-bit cause value and ignored cause changes when no trap event is accepted.

<!-- simulator-examples: rv32i-pipeline-status RV32IPipelineExecutionStatusTest -->

<div class="simulator-embed"><iframe src="../../../simulator/?scenario=rv32i-pipeline-status&amp;embed=1" title="Pipeline execution status" loading="lazy" allowfullscreen></iframe></div>

<a href="../../../simulator/?scenario=rv32i-pipeline-status" class="md-button">Open execution status</a>

### Tests

`RV32IPipelineExecutionStatusTest` compares structural and behavioral implementations; `RV32IBaselineExamplesTest` checks the example. Complete-core program tests check stopping and precise traps in context. Source for both components: `RV32IWritebackStage.cpp` (structural) and `RV32IPipelineStagesDirect.cpp` (behavioral).

## Completed-PC observation

The hardware retains the fetch PC and PCs carried by pipeline packets. It needs no extra completed-PC register. `RV32IRetirementView` exposes the settled MEM/WB packet before its retirement edge; the host-side `RV32IPipelineObserver` follows that packet's computed next PC, retaining the current PC for halt/trap. It does not execute the instruction or calculate its result with the oracle.

`RV32IPipelineStateView::snapshotPipelineState(observed_pc)` combines that host-supplied PC with the actual register-file and execution-status state. The program testbench displays this observation on its own PC output; the pipeline core has no PC output for it, and it never feeds back into the circuit. Retirement counters and saved traces likewise belong to the harness.
