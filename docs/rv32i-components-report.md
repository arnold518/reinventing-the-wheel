# RV32I Components Report

Last updated: 2026-07-19

## Status

This report tracks the components built on the path toward the first RV32I CPU runner.

The project does not have a complete RV32I CPU yet. The current `rv32i` branch has the two major foundations needed before CPU wiring starts:

- A structural 32-bit ALU stack, centered on `ALU32`.
- A paired register-file stack (`RegisterFile32x32` and the behavioral fidelity of `RegisterFile32x32`) plus `Memory64Kx32` for CPU-scale memory.
- RV32I decode/control rule libraries, a raw program preload utility, and a functional instruction oracle for CPU bring-up tests.

For the educational structural RV32I runner, use:

- `ALU32` as the execution ALU.
- `RegisterFile32x32` as the architectural register file.
- Two `Memory64Kx32` instances: one instruction memory and one data memory.

Each of the five major core-block contracts will have structural and behavioral implementations with identical pins. The structural members form the educational core; the behavioral members are compact component references and possible fast substitutes. CPU-scale memory remains behavioral because smaller structural memories already provide the practical lower-level teaching path.

## Component Evolution Order

The component stack has evolved in this order:

1. Core gates and adders.
2. Multi-bit adapters, constants, and muxes.
3. 8-bit arithmetic and ALU prototypes.
4. 32-bit arithmetic, comparison, shift, and ALU components.
5. Structural sequential storage: latch, structural D flip-flop, and one-bit memory.
6. Register words and register files.
7. Small structural memory slices.
8. Practical behavioral storage for CPU-scale register and memory use.
9. RV32I decode and control libraries.
10. Raw program loader and memory preload/readback helpers.
11. Functional RV32I instruction oracle and per-instruction trace.
12. `RV32IReferenceSystem`, a reusable system wrapper with a behavioral core and two visible memory components.

## Components

### Core Gates

Purpose:

The basic gates are the lowest-level combinational logic used throughout the project.

Components:

- `NOTGate`
- `ANDGate`
- `ORGate`
- `XORGate`
- `NANDGate`
- `NORGate`

RV32I role:

These are not directly visible as top-level CPU blocks, but they are the primitive implementation units for structural adders, muxes, decoders, storage slices, and ALU internals.

Status:

- Implemented.
- Tested.
- Exposed to the visualizer.

### `HalfAdder` and `FullAdder`

Purpose:

`HalfAdder` and `FullAdder` are the first arithmetic composition layer above gates.

RV32I role:

They are the building blocks for ripple-carry addition. The later 32-bit adder depends on this addition path.

Behavior:

- `HalfAdder` computes one-bit sum and carry.
- `FullAdder` computes one-bit sum and carry with carry-in.

Status:

- Implemented structurally.
- Tested.
- Exposed to the visualizer.

### Multi-Bit Utility Components

Purpose:

These components make 32-bit datapath construction practical while preserving the project's single-bit and multi-bit wire model.

Components:

- `Rewire`
- `BitSplitter<N>`
- `BitJoiner<N>`
- `ConstantValue<WIDTH, VALUE>`

RV32I role:

They adapt bus-like pins into single-bit lanes and back again. They are used heavily in muxes, constants, ALU control, register files, and memory slices.

Status:

- Implemented.
- Tested with width, sign-extension, zero-extension, unmapped-lane, and unknown-value cases.
- Exposed to the visualizer where scenarios exist.

### Multi-Bit Muxes

Purpose:

Muxes select among candidate values. They are essential for ALU operation selection, register reads, memory reads, PC selection, and writeback selection.

Components:

- `Mux2to1`
- `Mux4to1`
- `Mux8to1`
- `Mux16to1`
- `Mux32to1`
- `Mux2to1_8bit`
- `Mux4to1_8bit`
- `Mux4to1_32bit`
- `Mux8to1_8bit`
- `Mux16to1_8bit`
- `Mux32to1_32bit`

RV32I role:

- `Mux32to1_32bit` is used by the structural 32-entry register file and memory slices.
- Smaller 32-bit muxes will be useful for writeback selection, next-PC selection, and ALU input selection.

Status:

- Implemented structurally.
- Tested.
- Exposed to the visualizer.

### Decoders

Purpose:

Decoders convert a binary index into one-hot enable lines.

Components:

- `Decoder2to4`
- `Decoder5to32`

RV32I role:

These are address-selection components, not instruction decoders.

- `Decoder2to4` supports four-entry teaching register/memory slices.
- `Decoder5to32` supports full 32-entry register-file and memory slices.
- A future RV32I instruction decoder will be a different component: it will decode instruction fields into operation/control signals.

Status:

- Implemented structurally.
- Tested.
- Exposed to the visualizer.

### 8-Bit Datapath Prototypes

Purpose:

The 8-bit components were the first reusable datapath layer before the RV32I 32-bit stack.

Components:

- `AND8`, `OR8`, `XOR8`, `NOT8`, `NAND8`, `NOR8`
- `Adder8`
- `TwosComplement8`
- `Subtractor8`
- `SubtractorWithBorrow8`
- `Incrementer8`
- `Decrementer8`
- `EqualityChecker8`
- `Comparator8`
- `SignedComparator8`
- `ShiftLeftLogical8`
- `ShiftRightLogical8`
- `ShiftRightArithmetic8`
- `ZeroDetect8`
- `ALU8`

RV32I role:

These are not the final RV32I execution units because RV32I uses `XLEN=32`. They remain useful as design prototypes and lower-cost test fixtures for arithmetic, comparison, shift, and ALU behavior.

Status:

- Implemented.
- Tested.
- Exposed to the visualizer.

### `Adder32`

Purpose:

`Adder32` computes a 32-bit sum.

RV32I role:

This is needed for:

- Integer addition.
- Address calculation for loads and stores.
- PC + 4.
- Branch and jump target calculation.

Status:

- Implemented structurally.
- Tested.
- Exposed to the visualizer.

### `AddSub32`

Purpose:

`AddSub32` performs 32-bit addition or subtraction using one shared arithmetic path.

RV32I role:

This supports:

- `ADD`
- `ADDI`
- `SUB`
- Address arithmetic when subtraction or comparison paths need shared add/sub behavior.

Status:

- Implemented structurally.
- Tested with carry, borrow, boundary, and representative RV32I-style cases.
- Exposed to the visualizer.

### `Logic32`

Purpose:

`Logic32` computes 32-bit bitwise logic operations.

RV32I role:

This supports:

- `AND`
- `ANDI`
- `OR`
- `ORI`
- `XOR`
- `XORI`

Status:

- Implemented structurally.
- Tested.
- Exposed to the visualizer.

### `ZeroDetect32`

Purpose:

`ZeroDetect32` detects whether a 32-bit input is zero.

RV32I role:

This is useful for branch comparisons and ALU flags, especially equality checks such as `BEQ` and `BNE`.

Status:

- Implemented structurally.
- Tested.
- Exposed to the visualizer.

### `Comparator32`

Purpose:

`Comparator32` computes 32-bit comparison results.

RV32I role:

This supports:

- `SLT`
- `SLTI`
- `SLTU`
- `SLTIU`
- Branch comparison logic for signed and unsigned branch conditions.

Status:

- Implemented structurally.
- Tested with signed and unsigned edge cases.
- Exposed to the visualizer.

### `Shifter32`

Purpose:

`Shifter32` computes 32-bit shift operations.

RV32I role:

This supports:

- `SLL`
- `SLLI`
- `SRL`
- `SRLI`
- `SRA`
- `SRAI`

Important behavior:

- RV32I shift amount uses the low 5 bits.
- Arithmetic right shift preserves the sign bit.

Status:

- Implemented structurally.
- Tested.
- Exposed to the visualizer.

### `ALU32`

Purpose:

`ALU32` is the first major RV32I-oriented execution component. It combines arithmetic, logic, comparison, shift, and flag outputs behind one 32-bit ALU interface.

Pins:

| Pin | Direction | Width | Meaning |
| --- | --- | --- | --- |
| `A` | input | 32 | First operand. |
| `B` | input | 32 | Second operand or immediate value. |
| `OP` | input | 5 | Project-local ALU operation select. |
| `RESULT` | output | 32 | Operation result. |
| `ZERO` | output | 1 | Result is zero. |
| `NEGATIVE` | output | 1 | Result bit 31 is high. |
| `CARRY` | output | 1 | Arithmetic carry/borrow-related flag. |
| `OVERFLOW` | output | 1 | Signed arithmetic overflow flag. |

RV32I role:

`ALU32` is the execution unit for RV32I integer ALU instructions and the arithmetic backend for load/store address calculation. A future control unit will translate decoded RV32I instructions into this component's `OP` values.

Status:

- Implemented structurally.
- Tested through per-component tests and `ALU32StructuralContractTest`.
- Exposed to the visualizer as `alu32` and `rv32i-alu32`.

Detailed report:

- `docs/rv32i-milestone-1-report.md`

### `SRLatch`, `GatedDLatch`, and Structural `DFlipFlop`

Purpose:

These components provide the sequential foundation for structural storage.

Components:

- `SRLatch`
- `GatedDLatch`
- `DFlipFlop`

RV32I role:

They are the lower-level storage path for registers and memory bits. The structural `DFlipFlop` replaced the earlier behavioral D flip-flop so storage can be inspected in the visualizer.

Status:

- Implemented structurally.
- Tested.
- Exposed to the visualizer.

Detailed report:

- `docs/structural-dff-report.md`

### `MemoryBit`

Purpose:

`MemoryBit` is the one-bit structural write-enabled storage cell.

Pins:

| Pin | Direction | Width | Meaning |
| --- | --- | --- | --- |
| `D` | input | 1 | Data bit to write. |
| `WE` | input | 1 | Write enable. |
| `CLK` | input | 1 | Rising-edge clock. |
| `RST` | input | 1 | Reset. |
| `Q` | output | 1 | Stored bit. |

RV32I role:

This is the representative lower-level memory cell. It proves the storage behavior before larger storage is compressed into behavioral components.

Status:

- Implemented structurally using mux feedback and structural `DFlipFlop`.
- Tested.
- Exposed to the visualizer.

### the behavioral fidelity of `MemoryBit`

Purpose:

the behavioral fidelity of `MemoryBit` is the compact one-bit storage abstraction.

RV32I role:

It is used when the structural storage cell is too expensive to repeat at large scale. It keeps the same external behavior as `MemoryBit`.

Status:

- Implemented.
- Tested against the memory-bit contract.
- Exposed to the visualizer.

### `Register32` and the behavioral fidelity of `Register32`

Purpose:

These components store one 32-bit word.

RV32I role:

They are one register word, not the full RV32I register file. They establish the 32-bit storage word contract used by register-file components.

Important distinction:

- `Register32` is a 32-bit storage word made from structural `MemoryBit` cells.
- the behavioral fidelity of `Register32` is a 32-bit storage word made from behavioral memory bits.
- Neither one means all 32 RV32I architectural registers.

Status:

- Implemented.
- Tested.
- Exposed to the visualizer.

### `RegisterFile4x32`

Purpose:

`RegisterFile4x32` is a small teaching register file with four 32-bit entries.

RV32I role:

It demonstrates the shape of an RV32I-style register file:

- Two independent read ports.
- One write port.
- Register selection by address.
- Hardwired `x0` behavior in the larger register-file contract.

Status:

- Implemented.
- Tested.
- Exposed to the visualizer.

### `RegisterFile32x32`

Purpose:

`RegisterFile32x32` is the full RV32I architectural register-file shape with 32 32-bit entries.

RV32I role:

It represents registers `x0` through `x31` with:

- Two read addresses.
- Two read data outputs.
- One write address.
- One write data input.
- Write enable.
- Clock and reset.

Important behavior:

- `x0` reads as zero.
- Writes to `x0` do not change architectural state.
- Writes occur on clock edges.
- Reads are available through the read ports without waiting for a clock edge.

Status:

- Implemented as the full structural/register-file shape, using behavioral register words to keep scale manageable.
- Tested.
- Exposed to the visualizer.

### the behavioral fidelity of `RegisterFile32x32`

Purpose:

the behavioral fidelity of `RegisterFile32x32` is the compact CPU-scale RV32I register file.

RV32I role:

This is the register-file component intended for the first practical RV32I runner.

Important behavior:

- Stores 32 32-bit architectural registers.
- Keeps `x0` hardwired to zero.
- Provides two read ports and one write port.
- Writes on clock edge.
- Handles unknown values in explicit tests.

Status:

- Implemented.
- Tested with normal and unknown-state scenarios.
- Exposed to the visualizer as `behavioral-register-file32x32` and `behavioral-register-file32x32-unknown`.

### `Memory4x32`

Purpose:

`Memory4x32` is a four-word, 32-bit memory slice with CPU-facing memory pins.

RV32I role:

It is the tiny structural teaching slice for memory behavior. It demonstrates address selection, load/store controls, alignment/fault behavior, and read/write data flow.

Status:

- Implemented.
- Tested.
- Exposed to the visualizer.

### `Memory32x32`

Purpose:

`Memory32x32` expands the small memory slice to 32 words.

RV32I role:

It is still too small for real program execution, but it validates that the structural memory pattern scales beyond the four-word slice.

Status:

- Implemented.
- Tested.
- Exposed to the visualizer.

### `Memory64Kx32`

Purpose:

`Memory64Kx32` is the practical RV32I memory component.

Capacity:

- 64K 32-bit words.
- 256 KiB total.
- Byte-addressed range: `0x00000000` through `0x0003ffff`.

Pins:

| Pin | Direction | Width | Meaning |
| --- | --- | --- | --- |
| `ADDR` | input | 32 | Byte address. |
| `WRITE_DATA` | input | 32 | Store data. |
| `READ_EN` | input | 1 | Enables read access. |
| `WRITE_EN` | input | 1 | Enables write access. |
| `SIZE` | input | 2 | Access size: byte, halfword, word. |
| `SIGN_EXTEND` | input | 1 | Sign-extend byte/halfword loads when high. |
| `CLK` | input | 1 | Rising-edge write clock. |
| `RST` | input | 1 | Clears memory. |
| `READ_DATA` | output | 32 | Load data. |
| `READY` | output | 1 | Always ready in this first memory model. |
| `FAULT` | output | 1 | Invalid size, misalignment, or out-of-range access. |

RV32I role:

This is the memory component intended for the first practical runner. Use two instances first:

- Instruction memory: word fetch by `PC`.
- Data memory: `LB`, `LBU`, `LH`, `LHU`, `LW`, `SB`, `SH`, and `SW`.

Supported accesses:

| RV32I access | `SIZE` | `SIGN_EXTEND` | Meaning |
| --- | --- | --- | --- |
| `LB` | `00` | `1` | Byte load, sign-extended. |
| `LBU` | `00` | `0` | Byte load, zero-extended. |
| `LH` | `01` | `1` | Halfword load, sign-extended. |
| `LHU` | `01` | `0` | Halfword load, zero-extended. |
| `LW` | `10` | `0` | Word load. |
| `SB` | `00` | ignored | Byte store. |
| `SH` | `01` | ignored | Halfword store. |
| `SW` | `10` | ignored | Word store. |

Status:

- Implemented.
- Tested with reset, byte/halfword/word load-store cases, sign extension, zero extension, alignment faults, range faults, invalid-size faults, disabled writes, and blocked faulted writes.
- Has test/debug helpers for program preload and direct byte/word readback.
- Exposed to the visualizer as `behavioral-memory64kx32`.

Preload/readback helpers:

| API | Purpose |
| --- | --- |
| `capacityBytes()` | Returns the 256 KiB byte capacity. |
| `capacityWords()` | Returns the 64K word capacity. |
| `clearContents()` | Clears all bytes to zero. |
| `loadBytes(base, data)` | Writes raw bytes at any byte address. |
| `loadWords(base, words)` | Writes 32-bit words as little-endian bytes; base must be 4-byte aligned. |
| `readBytes(base, count)` | Reads raw bytes for tests/debugging. |
| `readByte(address)` | Reads one byte. |
| `readWord(address)` | Reads one little-endian word; address must be 4-byte aligned. |

Detailed report:

- `docs/memory-components-report.md`

### `RV32IProgram`

Purpose:

`RV32IProgram` is a raw byte container for RV32I test fixtures and program preload.

RV32I role:

It lets tests load real instruction words into `Memory64Kx32` without simulating setup writes through memory pins.

Behavior:

- `fromBytes()` preserves the given byte vector.
- `fromWords()` converts each 32-bit instruction word to little-endian bytes.
- `wordAt()` reads complete 32-bit little-endian words.
- `loadInto()` preloads a `Memory64Kx32` instance at a chosen byte address.

Status:

- Implemented.
- Tested by `RV32IProgramLoaderTest`.
- Not a visualizer component because it is test/setup infrastructure, not hardware.

### `RV32IFunctionalMemory`

Purpose:

`RV32IFunctionalMemory` is the byte-addressed memory model used by the functional instruction oracle.

RV32I role:

It models the memory behavior expected by real RV32I program execution:

- little-endian instruction fetch
- byte/halfword/word loads
- byte/halfword/word stores
- load sign extension and zero extension
- halfword/word alignment faults
- range faults

Status:

- Implemented.
- Tested by `RV32IInstructionOracleTest`.
- Not a visualizer component.

### `RV32IInstructionOracle`

Purpose:

`RV32IInstructionOracle` is the functional RV32I reference model and golden trace generator.

RV32I role:

It runs raw RV32I programs against:

- `pc`
- `x[32]`
- byte-addressed memory
- halt/trap state

It reuses `RV32IDecoder` and `RV32IControl`, then emits one semantic trace row per instruction.

Status:

- Implemented.
- Tested by `RV32IInstructionOracleTest`.
- Not a circuit component and not exposed to the visualizer.

Detailed report:

- `docs/rv32i-milestone-6-report.md`

## CPU-Level Components

The pure RV32I decode/control libraries now feed the first CPU-facing system component.

### `RV32IReferenceCore`

Purpose:

Provide the first clocked component implementation of RV32I execution.

Implementation:

- `BasicComponent`.
- Owns `pc`, `x[32]`, halt/trap state, and latest trace/debug fields.
- Uses the milestone 6 `RV32IInstructionOracle` execution rules.
- Talks to instruction and data memory through visible internal pins.
- Keeps memory-interface pins internal to the system.
- Exposes only essential status pins needed by the system.
- Provides instrumentation getters for registers, PC, trap cause, and last trace row.

Important boundary:

The core should not own memory. It should be reusable inside a system wrapper.

### `RV32IReferenceSystem`

Purpose:

Provide the first reusable RV32I machine component.

Implementation:

- Composite `IOComponent`.
- Contains one `RV32IReferenceCore`.
- Contains one `Memory64Kx32` instruction memory.
- Contains one `Memory64Kx32` data memory.
- Exposes external `CLK`, `RST`, `ENABLE`, `PC`, `HALTED`, and `TRAPPED`.
- Does not expose top-level memory or trace/debug pins.
- Provides helper methods for program preload and data-memory/register readback.

Important reset rule:

System reset should reset the core, not clear the preloaded instruction memory.

Detailed plan:

- `docs/rv32i-milestone-7-plan.md`

### Control-Flow Implementations

Purpose:

Store the current instruction address and compute sequential, branch, `JAL`, and `JALR` control flow.

Implemented forms:

- Structural `RV32IControlFlowUnit` using `Register32`, `Adder32`, muxes, rewires, and branch-decision gates.
- Compact the behavioral fidelity of `RV32IControlFlowUnit` with identical pins.

`RV32IControlFlowUnitEquivalenceTest` runs the same clock/input sequence in isolated simulators and checks both traces against independent expectations. The structural core uses the structural implementation. See `docs/rv32i-control-flow-equivalence-report.md`.

### Instruction Decoder

Purpose:

Decode a 32-bit RV32I instruction into instruction type, register fields, immediate fields, ALU operation, memory controls, branch controls, and writeback controls.

Important distinction:

This is not the same as `Decoder2to4` or `Decoder5to32`. Those are one-hot address decoders. The RV32I decoder interprets instruction bits according to the RISC-V ISA.

Current state:

- `RV32IDecoder` exists as a pure C++ library and is tested.
- `RV32IControl` exists as a pure C++ library and is tested.
- Structural `RV32IDecodeControlUnit` exposes field, immediate, masked instruction recognition, and raw-control logic.
- the behavioral fidelity of `RV32IDecodeControlUnit` exposes the same pins as a compact reference and is not hidden inside the structural component.
- `RV32IDecodeControlUnitEquivalenceTest` passes all 40 supported instruction forms, representative illegal encodings, and 64 deterministic pseudo-random raw words. See `docs/rv32i-decode-control-equivalence-report.md`.

### Immediate Generator

Purpose:

Extract and sign-extend RV32I immediates.

Needed immediate formats:

- I-type
- S-type
- B-type
- U-type
- J-type

### Control Unit

Purpose:

Translate decoded instruction information into datapath control signals.

Examples:

- ALU operation select.
- Register write enable.
- Memory read/write enable.
- Memory access size.
- Load sign-extension control.
- Writeback source select.
- Branch/jump select.

### Branch And Jump Unit

Purpose:

Choose whether the next PC is sequential, branch target, `JAL` target, or `JALR` target.

Expected inputs:

- Decoded branch/jump type.
- ALU or comparator flags.
- `PC`.
- Immediate.
- `RS1` for `JALR`.

### Execution Control/Status And Direct Memory Wiring

Purpose:

Control precise instruction completion, halt/trap state, and permission for direct RV32I memory connections.

Responsibilities:

- Structural `RV32IExecutionControlStatusUnit` classifies address alignment, prioritizes faults, latches status/cause, and gates PC/register/memory permissions.
- the behavioral fidelity of `RV32IExecutionControlStatusUnit` provides the same stateful pin contract for fast reference and equivalence testing.
- ALU output wires directly to memory address, `rs2` directly to store data, decoded size/sign controls directly to memory, and read data directly to writeback.
- There is no separate load/store forwarding module in the current five-block plan.

`RV32IExecutionControlStatusUnitEquivalenceTest` passes 30 labeled checkpoints covering reset, enable and memory waits, request persistence, every supported halt/trap cause, priority collisions, latched-state suppression, and reset recovery. See `docs/rv32i-execution-status-equivalence-report.md`.

### Writeback Mux

Purpose:

Select the data written to the register file.

Expected sources:

- ALU result.
- Load data.
- `PC + 4` for `JAL` and `JALR`.
- Upper-immediate result for `LUI` and `AUIPC` path, depending on datapath split.

### RV32I CPU Core

Purpose:

Tie together PC, decoder/control, register file, ALU, memory interface, branch/jump logic, and writeback selection.

Recommended boundary:

The CPU core should expose memory-facing pins rather than hiding all memory inside the core.

### RV32I System/Test Top Level

Purpose:

Instantiate:

- One CPU core.
- One `Memory64Kx32` instruction memory.
- One `Memory64Kx32` data memory.
- Clock/reset/test harness wiring.

This is where the first real assembly programs should run.

## Current Verification Status

The merged `rv32i` branch has passed:

```bash
cmake --build build -j 8
ctest --test-dir build --output-on-failure -R "Memory64Kx32Test|Memory32x32Test|Memory4x32Test|RegisterFile32x32BehavioralContractTest|RegisterFile32x32BehavioralUnknownPolicyTest|RegisterFile32x32StructuralContractTest|ALU32StructuralContractTest|ALU32BehavioralContractTest"
ctest --test-dir build --output-on-failure -R "RV32IControlTest|RV32IDecoderTest|ALU32StructuralContractTest|ALU32BehavioralContractTest|Memory64Kx32Test|RegisterFile32x32BehavioralContractTest"
ctest --test-dir build --output-on-failure -R "RV32IProgramLoaderTest|RV32IControlTest|RV32IDecoderTest|Memory64Kx32Test"
ctest --test-dir build --output-on-failure -R "RV32IInstructionOracleTest|RV32IProgramLoaderTest|RV32IControlTest|RV32IDecoderTest"
ctest --test-dir build --output-on-failure
```

Most recent result after the structural core/system and separated scenarios landed:

- Full regression passed: 142/142 in 525.19 seconds in the final serial verification run.

## Visualizer Coverage

The current visualizer exposes scenarios for the major foundation components:

- Gate tests.
- Adder tests.
- 8-bit arithmetic and ALU tests.
- 32-bit arithmetic, comparison, shift, and ALU tests.
- `alu32`
- `rv32i-alu32`
- `memory-bit`
- `register32`
- `register-file4x32`
- `rv32i-register-file`
- `memory4x32`
- `memory32x32`
- `memory64kx32`
- `rv32i-program-loader`
- `rv32i-control-flow`
- `rv32i-decode-control`
- `rv32i-execution-status`
- `rv32i-core`
- `rv32i-program1` through `rv32i-program16`

Each visual scenario contains one logical device under test. The component
explorer uses the scenario's recursive profile: expanded rows request
structural fidelity, collapsed rows request behavioral fidelity, and
`Apply & Run` rebuilds the same scenario. Separate fidelity-specific scenario
names and tests are no longer used.

## Practical Next Step

The structural single-cycle core/system milestone is complete: it instantiates
the structural implementation of all five major contracts, uses generic
operand/writeback muxes, connects separate instruction/data memories directly,
and passes all 16 reused lockstep programs. Progressive exact topology loading
now keeps the 54,049-component system usable in the browser. The next RV32I
hardening step is external answer-sheet validation against Spike, Sail, or the
official architecture tests. See `docs/rv32i-structural-core-report.md`.
