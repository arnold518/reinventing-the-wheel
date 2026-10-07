# § 4.5.2. Fetch

`RV32IBaselineFetchStage` owns the fetch PC and exposes native instruction-memory pins. The fetch PC is ahead of the architectural PC reported by Writeback while instructions are in flight.

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| `RV32IBaselineFetchStage` | Fetch PC, PC+4/redirect selection, instruction and fault packet | Yes | Yes |

## IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `CLK`, `RST` | Input | 1 each | Rising-edge capture; asynchronous active-high reset |
| `PC_WRITE`, `PC_REDIRECT` | Input | 1 each | PC update permission; select REDIRECT_PC rather than PC+4 |
| `FETCH_VALID` | Input | 1 | Instruction read enable and outgoing packet validity |
| `REDIRECT_PC` | Input | 32 | Resolved branch/jump target or fallthrough |
| `IMEM_READ_DATA`, `IMEM_FAULT` | Input | 32, 1 | Direct instruction word and access fault |
| `IMEM_WORD_ADDR`, `IMEM_READ_EN` | Output | 32, 1 | Word index PC/4 and read enable |
| `FETCH_PC` | Output | 32 | Current fetch PC, in bytes |
| `OUT_VALID` | Output | 1 | Follows FETCH_VALID |
| `OUT_PC`, `OUT_INSTRUCTION`, `OUT_FETCH_STATUS` | Output | 32 each | IF/ID packet fields |

## Behavior

Reset sets PC to zero. With PC_WRITE=1, a known rising edge selects REDIRECT_PC when PC_REDIRECT=1, otherwise PC+4. Without that edge and enable, PC holds. FETCH_VALID controls read enable independently of PC_WRITE: redirecting a PC does not have to accept an instruction on the same edge.

OUT_PC is the current PC; OUT_INSTRUCTION directly follows IMEM_READ_DATA. FETCH_STATUS bit 0 reports nonzero PC bits 1:0, and bit 1 follows IMEM_FAULT. A fault is recorded in the packet, not acted on immediately. Invalid or unaccepted packets cannot trap.

### C++ example

The examples use the [shared Packet definition](core.md#binary-example-types). Fetch writes only the three IF/ID fields and validity. This model contains no instruction storage or pending request.

```cpp
--8<-- "site-docs/rv32i/4.5/examples.hpp:fetch"
```

## Implementation

### Structural implementation

`FETCH_DATAPATH` reuses RV32ISingleCycleFetchStage: Register<32>, the structural PC+4 incrementer, alignment check, and direct memory-port rewiring. `NEXT_PC_SELECT` chooses PC+4 or the resolved redirect. `FETCH_STATUS_PACK` groups the two fault bits.

### Behavioral implementation

The Fetch branch of `DirectStage` stores PC and previous clock, then produces the same native pins and IF/ID packet. Its contract is `rv32i.baseline.fetch`. Equivalence covers settled binary operation after reset, not uninitialized/X-valued PCs.

## Examples

The rows cover reset, direct read, sequential advance, hold, redirect, alignment, and access faults. Times and checkpoint numbers come from the runnable scenario.

<!-- simulator-examples: rv32i-baseline-fetch RV32IBaselineFetchStageTest -->

<div class="simulator-embed"><iframe src="../../../simulator/?scenario=rv32i-baseline-fetch&amp;embed=1" title="Baseline Fetch stage" loading="lazy" allowfullscreen></iframe></div>

<a href="../../../simulator/?scenario=rv32i-baseline-fetch" class="md-button">Open Fetch stage</a>

## Tests

`RV32IBaselineFetchStageTest` compares both implementations. The core test checks that a branch redirect does not accept the old instruction-memory word. Sources: `RV32IBaselinePipeline.cpp`, `RV32IBaselinePipelineDirect.cpp`.
