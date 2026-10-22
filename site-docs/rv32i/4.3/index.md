# § 4.3. RV32I Building Blocks

Component references for the RV32I datapath and its control and memory-access logic. [4.1](../4.1/index.md) specifies instruction behavior; this section defines the hardware interfaces and implementations used to realize that behavior. Complete core wiring and pipeline scheduling belong in the later core sections.

## Contents

| Section | Coverage |
| --- | --- |
| [4.3.1. Register&lt;W&gt;](register.md) | Width-parametric FF storage, reset, clocked capture, and PC/pipeline/control uses |
| [4.3.2. Register File](register-file.md) | Register addressing, two independent reads, clocked writes, x0, and FF storage construction |
| [4.3.3. ALU Configuration](alu.md) | The shared integer ALU at W=32, its CPU interface and Execute integration |
| [4.3.4. Instruction Decoder](instruction-decoder.md) | Field extraction, immediate construction, recognition, illegal encodings, and control outputs |
| [4.3.5. Memory Access](memory-access.md) | Byte/halfword/word formatting, sign extension, masks, alignment, and transaction requests |
| [4.3.6. Execution Control and Status](execution-status.md) | Reset, execution enable, halt/trap handling, and architectural update control |

## Component grouping

| Reference | Components covered |
| --- | --- |
| `Register<W>` | One word at widths 1/2/3/4/5/8/16/32; structurally contains W `FlipFlopCell` instances. Also `ClockedStateBit`, the always-clocked handshake state bit |
| Register File | `RegisterFile<W,Count>` at W=32, Count=32, hardwired-zero enabled (`RegisterFile32x32`); 31 writable `Register<32>` words |
| ALU Configuration | `ALU<W>` at W=32 with integer operations, prefix addition and core flags (`ALU<32,integer,prefix,core>`); reusable component references are in Chapter 2 |
| Instruction Decoder | `RV32IDecodeControlUnit`, `RV32IBitPatternMatcher` |
| Memory Access | `RV32IMemoryLanes`, `RV32IMemoryAlignmentUnit`, `RV32IMemoryRequest` |
| Execution Control and Status | `RV32IExecutionControlStatusUnit` |

[Chapter 2](../../alu/2.1.md) defines reusable arithmetic/logic components and their [integer operation contract](../../alu/2.7.md). This section specifies the CPU configurations: a 32×32 register file with hardwired x0 and an integer/prefix ALU with result/comparison outputs. W describes hardware width; RV32I specifies the instruction encoding and architectural register semantics.

There is no separate program-counter block: the stage-wise cores distribute PC storage, branch decisions and next-PC selection across Fetch, Execute, and Next PC ([4.4](../4.4/index.md)), allowing JAL/JALR targets to reuse the main ALU.

Each component reference follows the same pattern: **IO pins → behavior and C++ example → structural/behavioral implementation → examples and embedded simulation → tests**. Component tables distinguish registered structural and behavioral implementations. Example tables include all recorded checkpoints and their simulation times.

A behavioral building block selected through the catalog runs on the delays
proven on its gates; where it has a delay twin, the current input values and state can
shorten publication to the gate list's settling time. Clocked parts also carry
clock-to-output and setup times. The register file uses its table without a
delay twin, because its large gate list brought no measured timing benefit.
How tables, twins and profiles work is explained once in
[§ 1 Time and Delay](../../timing.md); the certified clock of a complete core
is in [4.2](../4.2/timing.md). Directly created parts without a matching table
use their constructor delay. Where a block's timing has something to show (the
adder's prefix network, the register file's read and write), its behavioral
implementation ends with a **Timing** paragraph and the paths that matter.

## Structural organization in the visualizer

| Reference | Organization |
| --- | --- |
| `Register<W>` | One whole-word write enable; expand the register to inspect its numbered bit cells |
| Register File | Address decoder → 31 whole-word registers → two independent read muxes; keep word/bit ordering inside storage |
| ALU Configuration | Six functional groups in the core interface; full-flag mode also has FLAGS. Shared component circuits use connectivity layouts |
| Instruction Decoder | Field extraction and recognition feed named control fields and immediate selection; expand each field to inspect its bit equations |
| Control Flow | Automatic connectivity layout for the PC, arithmetic, branch decision, and next-PC selection |
| Memory Access | Address guard → byte routing/selection → load extension and write mask; request payload/state are separate from the tag-comparison block |
| Execution Status | Conditions → seven ordered priority links → cause selection and retained status |

Automatic graph layout is used where it preserves readable signal flow. Repeated bit lanes, register words, control equations, and priority links keep an explicit numeric or functional ordering. These groupings are real structural hierarchy, not hidden behavioral replacements; the public IO and selectable fidelities are unchanged.

## Boundaries with other sections

[Chapter 3](../../memory/architecture.md) owns memory cells, technology-specific words/rows, banks and devices at both fidelities, together with the [transaction protocol](../../memory/transactions.md) and [memory controllers](../../memory/controllers.md). This section specifies the reusable `Register<W>` and register-file configurations used by RV32I, with FF cells as their storage. The register file is a CPU operand store, not an interchangeable transaction endpoint. `RV32IMemoryRequest` is the CPU-side requester; it is not a technology-specific memory controller.

The core sections are:

- [4.4. Single-Cycle Core](../4.4/index.md): Fetch, Decode, Execute, Memory, Writeback, and Next PC references; their complete wiring, memory endpoints, shared ALU, and completion rules.
- [4.5. Baseline Pipeline](../4.5/index.md): five stages, interstage registers, dependency stalls, and conservative control-flow handling.
- [4.6. Advanced Pipeline](../4.6/index.md): forwarding, load-use handling, switchable prediction, recovery, and memory waits.
- [4.7. Memory Systems](../memory-systems.md): the advanced core's two ports over separate memories, one shared memory through the arbiter, and caches above and below it.

Pipeline-only state and coordination stay with the pipeline documentation. They are not prerequisites for the single-cycle core's component references.
