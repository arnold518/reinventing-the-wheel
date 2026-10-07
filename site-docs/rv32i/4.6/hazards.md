# § 4.6.2. Load-Use Hazards

ALU forwarding removes most of the [baseline's register-dependency waits](../4.5/control.md). A load's data arrives one stage later, so a directly dependent instruction still needs a bubble with zero-wait data memory.

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| `RV32IHazardDetectionUnit` | Match Decode's used sources against Execute's load destination | Yes | Yes |

## IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `ID_VALID`, `EX_VALID` | Input | 1 each | Decode/Execute slot validity |
| `ID_RS1`, `ID_RS2`, `EX_RD` | Input | 5 each | Consumer sources and load destination |
| `ID_USES_RS1`, `ID_USES_RS2` | Input | 1 each | Ignore encoded register fields unused by the instruction |
| `EX_MEM_READ` | Input | 1 | Execute instruction is a load |
| `LOAD_USE_HAZARD`, `STALL_FETCH`, `STALL_IF_ID`, `BUBBLE_ID_EX` | Output | 1 each | Same combinational hazard predicate; the coordinator applies priorities |

## Behavior

Assert when both packets are valid, Execute is a load to a nonzero register, and Decode actually reads that register. An ordinary ALU producer, a destination of x0, or an unused encoded source does not assert it. Store data, branch operands, and JALR sources count as genuine reads.

```cpp
--8<-- "site-docs/rv32i/4.6/examples.hpp:hazard"
```

The detector does not stop the clock or mutate a packet. On an otherwise unblocked cycle, the [coordinator](memory-waits.md) holds Fetch and IF/ID, writes a bubble into ID/EX, and advances the load through EX/MEM. If an older memory request is still waiting, the whole pipeline waits first.

### Example: load followed by use

For `lw x1,0(x2); add x3,x1,x4`, with zero-wait data memory:

| Current cycle | Load | Add | Ending-edge action |
| --- | --- | --- | --- |
| A | EX | ID | Hold Add in IF/ID; send load to MEM; bubble ID/EX |
| B | MEM | ID | Capture Add in ID/EX; capture loaded data in MEM/WB |
| C | WB | EX | Forward loaded value from MEM/WB into Add |

With a delayed endpoint, cycle B can repeat as memory waits. That extra delay is counted as a memory stall, not additional load-use bubbles.

## Implementation

### Structural implementation

Five-bit equality circuits compare both sources with EX_RD. Register-nonzero, source-use, valid and load gates qualify the result; the four outputs expose the same predicate.

### Behavioral implementation

The direct implementation evaluates the displayed predicate after inputs settle. There is no storage or counter in this component.

## Examples

<!-- simulator-examples: rv32i-load-use RV32IHazardDetectionUnitTest -->
<div class="simulator-embed"><iframe src="../../../simulator/?scenario=rv32i-load-use&amp;embed=1" title="Load-use hazard detection" loading="lazy" allowfullscreen></iframe></div>

## Tests

`RV32IHazardDetectionUnitTest` checks both fidelities. The shared program cases include a load-use chain and load-to-branch dependencies. `RV32IAdvancedExamplesTest` checks the exact C++ example against every detector row.
