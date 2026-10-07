# § 4.5.8. Complete Core

`RV32IBaselinePipelineCore` is the zero-wait-memory, stall-only RV32I pipeline. It retains the same [instruction semantics and machine contract](../4.1/machine.md) as the single-cycle implementation. Instructions overlap, but execute and retire in order.

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| `RV32IBaselinePipelineCore` | Five stages, four pipeline registers, dependency and control blocks | Yes | Yes |

## IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `CLK`, `RST`, `ENABLE` | Input | 1 each | Rising-edge state advance, asynchronous reset, global execution enable |
| `IMEM_REQ_READY`, `DMEM_REQ_READY` | Input | 1 each | Endpoint can accept the request on the next edge |
| `IMEM_RSP_VALID`, `DMEM_RSP_VALID` | Input | 1 each | Endpoint response is present during this cycle |
| `IMEM_RSP_READ_DATA`, `DMEM_RSP_READ_DATA` | Input | 32 each | Instruction/data response |
| `IMEM_RSP_STATUS`, `DMEM_RSP_STATUS` | Input | 3 each | Zero success; nonzero access error |
| `IMEM_REQ_ADDR`, `DMEM_REQ_ADDR` | Output | 32 each | Aligned byte addresses |
| `IMEM_REQ_OP`, `DMEM_REQ_OP` | Output | 3 each | READ=0; data WRITE=1 |
| `IMEM_REQ_VALID`, `DMEM_REQ_VALID` | Output | 1 each | Active access |
| `IMEM_REQ_WRITE_DATA`, `DMEM_REQ_WRITE_DATA` | Output | 32 each | Positioned store data; instruction port zero |
| `IMEM_REQ_WRITE_MASK`, `DMEM_REQ_WRITE_MASK` | Output | 4 each | Store byte enables; instruction port zero |
| `IMEM_RSP_READY`, `DMEM_RSP_READY` | Output | 1 each | Always accept a response |
| `FETCH_PC` | Output | 32 | Current fetch PC; completed PC is host-side observation, not a core pin |
| `HALTED`, `TRAPPED`, `TRAP_CAUSE` | Output | 1, 1, 4 | Architectural stopping state |
| `INSTRUCTION_ATTEMPT`, `COMMIT_VALID` | Output | 1 each | Accept current fetch packet; permit current WB retirement |
| `IF_ID_VALID`, `ID_EX_VALID`, `EX_MEM_VALID`, `MEM_WB_VALID` | Output | 1 each | Occupied instruction slots |
| `IF_ID_PC`, `ID_EX_PC`, `EX_MEM_PC`, `MEM_WB_PC` | Output | 32 each | Stored slot PC; meaningful as an instruction only when valid |
| `DATA_STALL`, `PIPELINE_STALL` | Output | 1 each | Same active RAW-stall condition |
| `BRANCH_WAIT`, `PIPELINE_FLUSH` | Output | 1 each | Branch waiting; active younger-slot bubble condition |
| `LOAD_USE_STALL`, `MEMORY_STALL`, `DATA_PORT_STALL` | Output | 1 each | Compatibility observations, always zero in this baseline |

The pin names are universal, but the baseline timing contract requires a response in the request cycle. Default FF endpoints satisfy it. Delayed endpoints require the transaction-capable pipeline; these pins alone do not add stall control.

## Behavior

The [stage contracts](index.md) define data transformation. [Control](control.md) decides whether each packet advances. All stages use pre-edge state; all enabled registers then capture together. Execute never re-reads a register updated by that same instruction. There is no combinational route around a pipeline register or hidden functional instruction execution in the structural core.

An independent stream first retires on the fifth active edge and can then retire one instruction per cycle. An immediate RAW consumer waits through the producer's EX, MEM, and WB stages. Branches wait for Execute whether taken or untaken. These are performance differences, not changes to instruction semantics.

ENABLE=0 freezes PC, pipeline registers, register-file updates and retirement, and suppresses stores. Reset clears architectural state and pipeline validity; externally provisioned memory images remain intact. Only valid instructions can retire or write memory. Older faults suppress younger stores and redirects; fault information travels with the instruction until WB.

### Binary example types

The [complete example header](examples.hpp) provides the C++ examples used throughout 4.5. It is ordinary binary C++ with no simulator events; it reuses the already documented instruction decoder and encoded control definitions. Packet members correspond to the named fields in [4.5.1](registers.md); only the fields present at that boundary are stored.

```cpp
--8<-- "site-docs/rv32i/4.5/examples.hpp:packet"
```

The one-edge ordering is: evaluate Decode/Execute/Memory and control from the old packets and register file; evaluate retiring WB; then capture each enabled pipeline register. A WB register write must not alter the already computed Decode packet on that same edge. In the structural simulator, the pipeline-register edges enforce this naturally. The compact model computes all candidate packets before changing its stored state.

## Implementation

### Structural implementation

| Child | Responsibility |
| --- | --- |
| `FETCH`, `IF_ID`, `DECODE`, `ID_EX`, `EXECUTE`, `EX_MEM`, `MEMORY`, `MEM_WB`, `WRITEBACK` | Left-to-right pipeline datapath, alternating stages and packet registers |
| `DEPENDENCIES` | Three older-producer comparisons |
| `CONTROL` | Stage enables, bubbles, branch wait, precise stopping |
| `EXECUTION_STATUS` | Sticky halt/trap flags and four-bit cause; six storage bits outside Writeback |
| `INSTRUCTION_PORT`, `DATA_PORT` | Combinational stage-to-transaction formatting, without state or wait cycles |
| `NO_MEMORY_WAIT` | Constant zero for compatibility diagnostic outputs |

The floorplan preserves stage order even though control and register-write feedback wires run backward. There are no stateful memory requesters, cache controllers, instruction/data arrays, or forwarding units hidden in the core. Port-formatting groups translate stage-local word addresses to byte-addressed transactions.

### Behavioral implementation

`BehavioralCore` stores four instruction packets, the fetch PC, register values, and halt/trap status. It evaluates the same cycle rules independently of the structural wiring. It does not run an architectural oracle internally and does not instantiate a shadow structural CPU. Both forms share `rv32i.core.baseline-pipeline`, a pipeline-state view accepting a host-observed PC and a read-only view of the pending MEM/WB retirement packet. Neither keeps retirement counters or saved trace records.

## Examples

This connected-core scenario fills all four pipeline registers with independent instructions, pauses while they are occupied, resumes, drains a halt, and resets. Each expected row is checked in both fidelities.

<!-- simulator-examples: rv32i-baseline-core RV32IBaselinePipelineCoreTest -->

<div class="simulator-embed"><iframe src="../../../simulator/?scenario=rv32i-baseline-core&amp;embed=1" title="Baseline pipeline core" loading="lazy" allowfullscreen></iframe></div>

<a href="../../../simulator/?scenario=rv32i-baseline-core" class="md-button">Open baseline core</a>

### Memory integration

Program fixtures supply separate [MemoryEndpoint&lt;ff,32&gt;](../../memory/3.3.md) instances for instructions and data, each 65,536 words (256 KiB) by default. Both ports require zero-wait timing: instruction/data reads and response status settle within the current cycle, and authorized stores use the shared rising edge. Fetch captures the instruction into IF/ID and Memory forwards a load into MEM/WB without an extra memory-wait cycle.

| Memory component or assembly | Suitable for this core? | Use |
| --- | --- | --- |
| `MemoryEndpoint<ff,32>`, fused behavioral implementation | Yes; default | Scalable instruction and read/write data storage |
| `MemoryEndpoint<ff,32>`, visible FF controller plus native device | Yes | Same timing, with the controller/device boundary visible |
| `MemoryEndpoint<sram,32>`, `<dram,32>`, `<rom,32>`, `<nor-flash,32>` | No, with their current controllers | These endpoints require delayed-response handling |
| Native `MemoryContainer`, `FFMemory`, or other memory device alone | Not directly | Native pins need the matching controller to form an endpoint |

The visible FF assembly can use [MemoryContainer&lt;ff,32&gt;](../../memory/3.1.6.md) for scalable storage, or [FFMemory&lt;32&gt;](../../memory/3.1.1.md) for a small cell-expanded circuit. Program-sized images should keep scalable storage. Capacity and fidelity do not change the timing contract. CPU reset clears the endpoint interface, not the stored program/data.

!!! warning "Hazard stalls are not memory-wait support"

    The baseline waits for register dependencies and branches, but does not hold its packets in response to `REQ_READY=0` or `RSP_VALID=0`. Its memory-stall observation pins are always zero. The program root rejects non-FF configurations. Bypassing that guard with custom wiring can capture stale instructions/load values or lose stores; the result is not simply a slower pipeline. Delayed endpoints require the transaction-capable `RV32IFiveStageCore`, whose request tracking and pipeline control wait for completion.

### Complete programs

All 22 programs from [4.2.3](../4.2/programs/index.md) are registered as `RV32IBaselinePipelineProgramTest/program-01` through `program-22`. Their assembly, initial memory, expected instructions, and explanations are unchanged. Select `rv32i-baseline-program1` through `rv32i-baseline-program22` in the simulator.

The example below sums an array, branches around the loop, and stores its result. It uses two external zero-wait FF endpoints. Inspect `CORE` to see the baseline circuitry and use the instruction checkpoints to follow retirement.

<!-- simulator-program: rv32i-baseline-program1 RV32IBaselinePipelineProgramTest/program-01 -->

<div class="simulator-embed"><iframe src="../../../simulator/?scenario=rv32i-baseline-program1&amp;embed=1" title="Array sum on baseline pipeline" loading="lazy" allowfullscreen></iframe></div>

<a href="../../../simulator/?scenario=rv32i-baseline-program1" class="md-button">Open array-sum program</a>

## Tests

`RV32IBaselinePipelineCoreTest` runs the connected-core scenario and all 22 programs. Each program compares architectural state and retired memory effects with the instruction oracle, checks physical memory writes independently, and compares structural/behavioral retirement times, cycle counts, RAW stalls, and branch-wait counts. The harness samples COMMIT_VALID and the MEM/WB packet before each edge, tracks completed PC and counts in host memory, and checks hardware state after the edge. The testbench PC output is this host observation, not a core register or feedback signal. This observation does not add circuitry to Writeback. Focused stage/control tests cover the smaller contracts.

These tests establish the baseline's simulated behavior. They do not establish realistic cache/DRAM latency, hardware clock frequency, or performance relative to a physical processor. Variable-latency memories and forwarding are outside this baseline; the existing advanced `RV32IFiveStageCore` supports them and shares the same simplified Writeback stage.

Sources: `RV32IBaselinePipelineCore.cpp`, `RV32IBaselinePipelineBehavioral.cpp`, `RV32IBaselinePipelineTests.cpp`, and the shared RV32I program tests.
