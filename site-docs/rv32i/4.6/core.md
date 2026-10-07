# § 4.6.6. Complete Advanced Core

`RV32IFiveStageCore` is the advanced in-order, single-issue core. It preserves the reference instruction semantics while overlapping stages, forwarding operands, predicting control flow, and waiting correctly for memory transactions.

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| `RV32IFiveStageCore` | Five stages, packet registers, control and universal memory requesters | Yes | Yes |

## IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `CLK`, `RST`, `ENABLE` | Input | 1 each | Shared clock, reset and execution permission |
| `IMEM_REQ_READY`, `DMEM_REQ_READY` | Input | 1 each | Endpoint can accept the presented request |
| `IMEM_RSP_VALID`, `DMEM_RSP_VALID` | Input | 1 each | Complete response is available |
| `IMEM_RSP_READ_DATA`, `DMEM_RSP_READ_DATA` | Input | 32 each | Response word |
| `IMEM_RSP_STATUS`, `DMEM_RSP_STATUS` | Input | 3 each | Universal response status |
| `IMEM_REQ_VALID`, `DMEM_REQ_VALID` | Output | 1 each | Request payload is valid |
| `IMEM_REQ_OP`, `DMEM_REQ_OP` | Output | 3 each | Read/write operation |
| `IMEM_REQ_ADDR`, `DMEM_REQ_ADDR` | Output | 32 each | Byte-addressed transaction address |
| `IMEM_REQ_WRITE_DATA`, `DMEM_REQ_WRITE_DATA` | Output | 32 each | Lane-positioned store word |
| `IMEM_REQ_WRITE_MASK`, `DMEM_REQ_WRITE_MASK` | Output | 4 each | Enabled byte lanes |
| `IMEM_RSP_READY`, `DMEM_RSP_READY` | Output | 1 each | Requester can consume or drain a response |
| `FETCH_PC` | Output | 32 | Current speculative fetch address, not retired architectural PC |
| `HALTED`, `TRAPPED`, `TRAP_CAUSE` | Output | 1, 1, 4 | Persistent execution status |
| `INSTRUCTION_ATTEMPT`, `COMMIT_VALID` | Output | 1 each | Qualified WB instruction-attempt / retirement event |
| `PIPELINE_STALL`, `LOAD_USE_STALL`, `MEMORY_STALL`, `DATA_PORT_STALL`, `PIPELINE_FLUSH` | Output | 1 each | Current movement-control observations |
| `IF_ID_VALID`, `ID_EX_VALID`, `EX_MEM_VALID`, `MEM_WB_VALID` | Output | 1 each | Packet validity |
| `IF_ID_PC`, `ID_EX_PC`, `EX_MEM_PC`, `MEM_WB_PC` | Output | 32 each | PC of each packet, meaningful when valid |
| `BRANCH_RESOLVED`, `BRANCH_MISS`, `PREDICTION_RECOVERY` | Output | 1 each | Qualified conditional resolution, conditional next-PC miss, all-instruction correction |

Requests follow [3.2](../../memory/3.2.md). The core emits ordinary reads/writes only, not DRAM refresh or flash program/erase commands. See [memory compatibility](memory-waits.md#suitable-memories).

## Configuration

The registered family is `rv32i.core.educational-five-stage`. Its parameters configure microarchitecture, not fidelity:

```cpp
builder.add(circuit::families::RV32IFiveStageCore, "CORE", {
    {"prediction", "two-bit"},
    {"prediction_entries", "16"},
});
```

The existing family API defaults to `not-taken` and 16 entries. All five policies are listed in [4.6.3](prediction.md). Direct JAL still predicts its target under `not-taken`; this policy name refers to conditional branches.

Native construction without a build profile can use `Component::create<RV32IFiveStageCore>(name, rv32i::PredictionConfig{...})`. In either path, configure before reset/run. The visualizer's registered program scenarios expose the five policies through the scenario selector; structural/behavioral selection remains a separate control.

## Behavior and grouping

| Group | Responsibility |
| --- | --- |
| `FETCH`, `IF_ID`, `DECODE`, `ID_EX`, `EXECUTE`, `EX_MEM`, `MEMORY`, `MEM_WB`, `WRITEBACK` | Left-to-right instruction flow and four edge-captured packets |
| `PREDICTION` | Predecode, predictor bank, saved next-PC metadata and recovery |
| `COORDINATOR` | Holds, bubbles, redirects, request issue, response consumption and commit permissions |
| `FETCH_REQUEST`, `DATA_REQUEST`, `LOAD_STORE` | CPU request ownership and byte lanes |
| `EXECUTION_STATUS` | Sticky halt/trap state, separate from combinational WB |

Decode can bypass WB; Execute can forward from EX/MEM or MEM/WB. Only the immediate load dependency inserts a data bubble with zero-wait memory. A correct prediction does not flush. A wrong next PC flushes younger IF/ID and ID/EX while the resolved instruction advances. Older faults take precedence over younger effects and predictor updates.

Carry propagation uses the same gate-built parallel-prefix arithmetic as the other cores. Recovery compares both next-address candidates before selecting the mismatch bit. Instruction requests are speculative while execution is active; admission remains separately qualified by the coordinator. See [memory waits](memory-waits.md) for response draining and the side-effect-free IMEM requirement.

The root connects these named blocks rather than exposing individual control gates. `COORDINATOR.FETCH_RESPONSE_TAKE` drives the instruction requester's `TAKE`; `COORDINATOR.DATA_REQUEST_ISSUE` drives the data requester's `ISSUE`. Operation packing and constants remain simple wiring helpers. The four pipeline-register banks form tall, narrow boundaries between the stages; expanding a bank shows its ordered payload fields and width-sized registers.

At every edge, next packets are computed from **old** packets. Explicit bypass paths carry values from older instructions; an instruction never rereads a register or memory location it has just updated to finish itself. Pipeline metadata and prediction state are not architectural state. Completed-PC tracking and traces remain host observations, not extra WB storage.

Reset clears PC, packets, registers, prediction state and execution status; it cancels requester state without erasing endpoint storage. Pausing ENABLE holds architectural progress. Endpoints may finish already accepted transactions while paused; responses remain owned until the core consumes them.

### C++ behavior reference

The exact binary examples for the added units are compiled from `examples.hpp`: [forwarding](forwarding.md), [load-use](hazards.md), [prediction](prediction.md), [recovery](recovery.md), and [coordination](memory-waits.md). The inherited ALU, packet and writeback examples remain in §4.5. The compact complete-core implementation is `RV32IFiveStageCoreDirect.cpp`; it composes those contracts with the one-outstanding requester model. It is not an instruction-at-a-time oracle and cannot advance two stage boundaries on one clock edge.

## Implementation

### Structural implementation

The core connects selectable stage, packet, control and prediction components. Execute reuses the shared ALU datapath. Prediction computes the sequential and predicted next PC; Fetch stores that selected address without a second PC+4 adder or next-PC mux. A read-only fetch requester stores one address word, also used as its response tag; the data requester retains the general write-capable payload. The four packet registers use compact storage for defined control, status, and register-address fields while exposing the shared 32-bit packet pins. Behavioral memory parts can be selected independently to bound simulation size. The lower-level table, forwarding, and recovery circuitry remains inspectable.

### Behavioral implementation

The compact model keeps the same packets, register file, counter table, saved predictions and requester state. It computes the same pre-edge results and edge captures. Program tests compare architectural traces against the instruction oracle and compare configured structural/behavioral retirement cycles and prediction counters.

## Program examples

The following short loop initializes a counter and accumulator, repeatedly accumulates and decrements, branches back while nonzero, then halts. Its first encounters show cold predictor state; later encounters show training. The complete instruction-by-instruction answer sheet for the two-bit configuration follows.

<!-- simulator-program: rv32i-predict-two-bit-program20 RV32IPredictionProgramTest/two-bit/program-20 -->
<div class="simulator-embed"><iframe src="../../../simulator/?scenario=rv32i-predict-two-bit-program20&amp;embed=1" title="Advanced pipeline with two-bit prediction" loading="lazy" allowfullscreen></iframe></div>

Compare the same program with [not-taken](../../../simulator/?scenario=rv32i-predict-not-taken-program20), [taken](../../../simulator/?scenario=rv32i-predict-taken-program20), [backward-taken](../../../simulator/?scenario=rv32i-predict-backward-taken-program20), [one-bit](../../../simulator/?scenario=rv32i-predict-one-bit-program20), or [two-bit](../../../simulator/?scenario=rv32i-predict-two-bit-program20). These are identical instructions and initial state; only predictor policy changes.

The five policy-comparison programs are [direct/indirect calls, precise illegal-instruction stopping, repeated load-use pairs, a branch-heavy loop, and an array sum](../4.2/programs/index.md). Their scenarios are numbered 05, 09, 19, 20 and 21. The original 22 shared program cases continue to run with the default advanced-core policy. A comprehensive performance comparison is deferred; these checks establish correctness and comparable measurements, not a ranking for real processors.

## Observations and tests

Host metrics include cycles, CPI, retired instructions, load-use stalls, memory stalls, flushes, packet occupancy, resolved conditional branches, conditional next-PC misses, and qualified corrections including JALR. `prediction.next_pc_accuracy` is omitted when no conditional branch resolves. A branch whose taken target equals PC+4 is correct by this next-PC metric regardless of its direction prediction. Terminal flushes are not predictor misses.

`RV32IFiveStageCoreTest` checks basic assembly. `RV32IFiveStageCoreProgramTest/program-01` through `22` run oracle lockstep. `RV32IPredictionProgramTest/<policy>/program-05`, `09`, `19`, `20`, and `21` additionally compare retirement timing and prediction metrics across fidelities. `RV32IMemoryMigrationTest` checks delayed endpoints and wrong-path effects; `RV32IAdvancedExamplesTest` verifies the displayed unit examples. All tests are run serially to keep memory usage bounded.
