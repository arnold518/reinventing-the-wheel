# § 4.5.5. Memory Access

`RV32IBaselineMemoryStage` adapts the EX/MEM packet to stage-local word-addressed memory pins. The complete core's combinational `DATA_PORT` converts these pins to the universal interface of an external zero-wait FF endpoint. The stage contains neither storage nor a pending transaction. EX/MEM and MEM/WB are the only surrounding instruction buffers.

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| `RV32IBaselineMemoryStage` | Direct load/store lanes, access qualification, MEM/WB packet | Yes | Yes |

## IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `IN_VALID` | Input | 1 | EX/MEM validity |
| `IN_PC`, `IN_INSTRUCTION`, `IN_ALU_RESULT`, `IN_STORE_DATA`, `IN_PC_PLUS_4`, `IN_NEXT_PC`, `IN_CONTROL`, `IN_REGISTER_ADDRESSES`, `IN_FETCH_STATUS`, `IN_EXECUTION_STATUS` | Input | 32 each | EX/MEM payload |
| `MEMORY_ENABLE` | Input | 1 | Global execution and older-WB-terminal permission |
| `DMEM_READ_DATA`, `DMEM_FAULT` | Input | 32, 1 | Native word and combinational access fault |
| `DMEM_WORD_ADDR`, `DMEM_WRITE_DATA` | Output | 32 each | Word index and aligned write lanes |
| `DMEM_WRITE_MASK` | Output | 4 | Byte enables |
| `DMEM_READ_EN`, `DMEM_WE` | Output | 1 each | Direct read enable; authorize store at memory's next rising edge |
| `OUT_VALID` | Output | 1 | Forwarded validity |
| `OUT_<field>` | Output | 32 each | All EX/MEM fields above forwarded under the OUT prefix |
| `OUT_MEMORY_DATA`, `OUT_MEMORY_STATUS` | Output | 32 each | Extended load data; bit 0 misalignment, bit 1 access fault |
| `TERMINAL` | Output | 1 | Valid incoming terminal reason or eligible memory error |

## Behavior

A memory operation is eligible only when IN_VALID is one, decode requests a load/store, and no earlier terminal reason accompanies that instruction. READ_EN is MEMORY_ENABLE && eligible. Stores also enable the read/access check so range faults are known before the write edge.

Byte addressing becomes WORD_ADDR=ALU_RESULT>>2. Byte offset selects and extends load lanes, shifts STORE_DATA, and forms WRITE_MASK. Loads support signed/unsigned bytes and halfwords, plus words. Misaligned halfword/word accesses and reserved size 3 are rejected. The original byte address remains in the packet for retirement.

WE additionally requires a store and valid alignment/size. It denotes the requested operation, not successful completion: the endpoint rejects an invalid store without modifying memory. Response faults affect terminal status but never feed back into WE or the request opcode. MEMORY_ENABLE=0 suppresses both read and write enables. Packet forwarding and lane calculation remain combinational; this input is not a request handshake or a packet register enable.

MEMORY_STATUS qualifies data errors by operation eligibility. A prior instruction fault takes precedence and suppresses the memory operation. The stage reports terminal status immediately to pipeline control; the fault becomes architectural only when this instruction reaches Writeback.

### Memory compatibility

Use `MemoryEndpoint<ff,32>`, either fused behavioral or a visible FF controller plus native FF storage. The latter can use scalable `MemoryContainer<ff,32>` or a small cell-expanded `FFMemory<32>`. The [complete-core memory table](core.md#memory-integration) lists the supported assemblies and construction restrictions.

The surrounding EX/MEM and MEM/WB registers do not by themselves add memory-wait support. This baseline's control stalls for register dependencies and branches, not delayed `REQ_READY`/`RSP_VALID`. The current SRAM, DRAM, ROM, and NOR-flash endpoints are therefore unsuitable: a delayed load could reach MEM/WB with stale data if custom wiring bypassed the program root's non-FF guard. Use the transaction-capable `RV32IFiveStageCore` for those endpoints.

### C++ example

`early_terminal` is the shared check for valid fetch/decode/target faults and halt. The returned packet forwards all incoming fields and adds loaded data/status. An endpoint stores bytes on its own rising edge only when `we` requests a valid, accepted store; an error response performs no write. This function itself stores nothing.

```cpp
--8<-- "site-docs/rv32i/4.5/examples.hpp:memory"
```

## Implementation

### Structural implementation

`ACCESS` reuses RV32ISingleCycleMemoryStage, which groups RV32IMemoryLanes and alignment checking. `PACKET_STATUS` groups incoming terminal-reason checks. Packet/status rewires and eligibility/fault gates surround the combinational access path. There is no REQUEST, pending flag, or duplicate memory device.

### Behavioral implementation

`memoryPacket` computes the same binary lane mappings, permission signals, and status. The registered family is `rv32i.baseline.memory`. The complete core connects to an external FF endpoint: an FF controller plus a native FF device, or their fused behavioral implementation. It shares the CPU clock and adds no wait cycle.

## Examples

The test includes signed byte extraction, byte/word stores, alignment/access faults, an older terminal instruction disabling memory, and invalid packet suppression.

<!-- simulator-examples: rv32i-baseline-memory RV32IBaselineMemoryStageTest -->

<div class="simulator-embed"><iframe src="../../../simulator/?scenario=rv32i-baseline-memory&amp;embed=1" title="Baseline Memory stage" loading="lazy" allowfullscreen></iframe></div>

<a href="../../../simulator/?scenario=rv32i-baseline-memory" class="md-button">Open Memory stage</a>

## Tests

`RV32IBaselineMemoryStageTest` compares both implementations. Program tests check retired memory effects against the instruction reference and independently verify the physical write history. Sources: `RV32IBaselinePipeline.cpp`, `RV32IBaselinePipelineBehavioral.cpp`.
