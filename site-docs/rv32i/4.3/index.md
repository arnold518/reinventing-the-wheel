# § 4.3. RV32I Building Blocks

Component references for the RV32I datapath and its control and memory-access logic. [4.1](../4.1/index.md) specifies instruction behavior; this section defines the hardware interfaces and implementations used to realize that behavior. Complete core wiring and pipeline scheduling belong in the later core sections.

## Contents

| Section | Coverage |
| --- | --- |
| [4.3.1. Register&lt;W&gt;](register.md) | Width-parametric FF storage, reset, clocked capture, and PC/pipeline/control uses |
| [4.3.2. Register File](register-file.md) | Register addressing, two independent reads, clocked writes, x0, and FF storage construction |
| [4.3.3. ALU32](alu32.md) | Operation encoding, arithmetic/logic/shifts/comparisons, result selection, and flags |
| [4.3.4. Instruction Decoder](instruction-decoder.md) | Field extraction, immediate construction, recognition, illegal encodings, and control outputs |
| [4.3.5. Memory Access](memory-access.md) | Byte/halfword/word formatting, sign extension, masks, alignment, and transaction requests |
| [4.3.6. Execution Control and Status](execution-status.md) | Reset, execution enable, halt/trap handling, and architectural update control |

## Component grouping

| Reference | Components covered |
| --- | --- |
| `Register<W>` | One word at widths 1/2/3/4/5/8/16/32; structurally contains W `FlipFlopCell` instances |
| Register File | `RegisterFile32x32`; its storage bank holds 31 `Register<32>` words, each built from `FlipFlopCell` instances |
| ALU32 | `ALU32`, `ALU32Core`, `AddSub32`, `Logic32`, `Shifter32`, `ALU32ControlDecoder`, `ALU32CompareUnit`, `ALU32ResultSelector`, `ALU32FlagUnit` |
| Instruction Decoder | `RV32IDecodeControlUnit`, `RV32IBitPatternMatcher` |
| Memory Access | `RV32IMemoryLanes`, `RV32IMemoryAlignmentUnit`, `RV32IMemoryRequest` |
| Execution Control and Status | `RV32IExecutionControlStatusUnit` |

ALU32's internal units belong within its reference rather than becoming separate top-level sections. Existing ALU8 concepts can be cross-referenced, but the 32-bit pins, operation encoding, and behavior must be specified explicitly. `ALU32` is registered as `rv32i.alu32`; its arithmetic operations are reusable even though this particular interface serves the RV32I core.

There is no separate program-counter block: the stage-wise cores distribute PC storage, branch decisions and next-PC selection across Fetch, Execute, and Next PC ([4.4](../4.4/index.md)), allowing JAL/JALR targets to reuse the main ALU.

Each component reference follows the same pattern: **IO pins → behavior and C++ example → structural/behavioral implementation → examples and embedded simulation → tests**. Component tables distinguish registered structural and behavioral implementations. Example tables include all recorded checkpoints and their simulation times.

## Structural organization in the visualizer

| Reference | Organization |
| --- | --- |
| `Register<W>` | One whole-word write enable; expand the register to inspect its numbered bit cells |
| Register File | Address decoder → 31 whole-word registers → two independent read muxes; keep word/bit ordering inside storage |
| ALU32 | Seven functional units; all eight 4.3.3 components and their structural descendants use automatic connectivity layouts, without custom placement overrides |
| Instruction Decoder | Field extraction and recognition feed named control fields and immediate selection; expand each field to inspect its bit equations |
| Control Flow | Automatic connectivity layout for the PC, arithmetic, branch decision, and next-PC selection |
| Memory Access | Address guard → byte routing/selection → load extension and write mask; request payload/state are separate from the tag-comparison block |
| Execution Status | Conditions → seven ordered priority links → cause selection and retained status |

Automatic graph layout is used where it preserves readable signal flow. Repeated bit lanes, register words, control equations, and priority links keep an explicit numeric or functional ordering. These groupings are real structural hierarchy, not hidden behavioral replacements; the public IO and selectable fidelities are unchanged.

## Boundaries with other sections

[Chapter 3](../../memory/3.1/index.md) owns memory cells, technology-specific words/rows, banks, devices, and `MemoryContainer`, together with the [transaction protocol](../../memory/3.2.md) and [memory controllers](../../memory/3.3.md). This section introduces `Register<W>` and the register file as RV32I building blocks reusing FF cells. The register file is a CPU operand store, not an interchangeable transaction endpoint. `RV32IMemoryRequest` is the CPU-side requester; it is not a technology-specific memory controller.

The core sections are:

- [4.4. Single-Cycle Core](../4.4/index.md): Fetch, Decode, Execute, Memory, Writeback, and Next PC references; their complete wiring, memory endpoints, shared ALU, and completion rules.
- [4.5. Baseline Pipeline](../4.5/index.md): five stages, interstage registers, dependency stalls, and conservative control-flow handling.
- [4.6. Advanced Pipeline](../4.6/index.md): forwarding, load-use handling, switchable prediction, recovery, and memory waits.

Pipeline-only state and coordination stay with the pipeline documentation. They are not prerequisites for the single-cycle core's component references.
