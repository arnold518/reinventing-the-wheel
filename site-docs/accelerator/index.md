# § 6. AI Accelerator

Neural networks spend nearly all of their time multiplying matrices. This
chapter builds hardware for exactly that job and attaches it to the computer
of [chapter 5](../system/index.md): a **matrix unit** in the style of
Google's first TPU, at a small size. The processor still runs the program;
it hands the matrix work to the unit through a few registers, the unit
fetches its operands from main memory itself, multiplies them in a grid of
multiply-add cells and writes the results back.

<!-- diagram: matrix-unit-overview -->

Three ideas carry the chapter.

1. **Do many multiplications at once.** A multiply that takes one bit per
   clock edge ([§ 5.1.1](../system/5.1/arithmetic.md)) is far too slow when a
   layer needs fifty thousand of them. An array multiplier finishes in one
   combinational pass ([6.1](6.1.md)), and a grid of N × N cells starts N²
   multiply-adds every edge ([6.2](6.2.md)).
2. **Reuse every number that was fetched.** A weight is loaded into a cell
   once and meets every input that streams past it (*weight-stationary*);
   an input passes from cell to cell along its row. Moving data, not
   arithmetic, is what limits such a unit, so the chapter measures both.
3. **The processor stays in charge.** The unit is an ordinary chapter-5
   device: a register window, a command queue, and a DMA port that is one
   more master on the bus. A C driver turns a layer into a short list of
   commands ([6.4](6.4.md)).

## Contents

| Section | What it covers |
| --- | --- |
| [6.1. Arithmetic](6.1.md) | The array multiplier beside the iterative one, the RV32IM core's fast multiplier option, the int8 multiply-add cell, the requantizer |
| [6.2. Systolic Array](6.2.md) | N × N weight-stationary cells with input skew and output de-skew |
| [6.3. Data Movement](6.3.md) | The unit's copy engine (its DMA), and the pipelined bus option |
| [6.4. Matrix Unit](6.4.md) | The device: registers, commands, buffers, how a layer is computed |
| [6.5. Networks](6.5.md) | MNIST: a small MLP and CNN, int8 quantization, the guest programs and the machine |
| [6.6. Experiments](6.6.md) | The CPU alone against the CPU with the unit |

## The machine

`systems/accelerator-machine.json` is the chapter-5 machine without the
display and buttons, plus the unit:

| Window | Base | Bytes | Target |
| --- | --- | --- | --- |
| RAM | `0x00000000` | `0x40000` | Memory subsystem; reached by the core's fetch and data ports **and the unit's DMA port** |
| CONSOLE | `0x10000000` | `0x100` | Console registers |
| TIMER | `0x10001000` | `0x100` | Timer registers (a cycle counter for the programs' timings) |
| MATRIX | `0x10005000` | `0x100` | Matrix unit registers |

The core's data port and the unit's DMA port share the memory's data port
through a two-port arbiter, so the unit's transfers and the processor's loads
take turns. `accelerator-machine-cpu.json` is the same machine without the
unit: the comparison point of [6.6](6.6.md).

## Reading this chapter

Component sections follow the pattern of the earlier chapters: **IO pins →
behavior → implementation → examples → source and tests**. Every part
exists as gates; the larger ones also have a compact form that is proven
equal to the gates and runs on delays taken from them. Each example table
is generated from a recorded scenario, and the same scenario opens in the
simulator below it. The scenarios are in the simulator's **AI Accelerator**
menu group, after the long games. Each opens in the finest profile a
browser handles:

| Scenario | Section | Opens in |
| --- | --- | --- |
| `ArrayMultiplier<4>` | [6.1.1](6.1.md#examples) | Gate |
| `ArrayMulDivUnit<4>` | [6.1.2](6.1.md#examples_1) | Gate |
| `MacCell` | [6.1.3](6.1.md#examples_2) | Gate |
| `Requantizer` | [6.1.4](6.1.md#examples_3) | Gate |
| `SystolicArray<2>` | [6.2](6.2.md#examples) | Gate |
| `MatrixUnit<4>`: a 2 × 4 → 3 layer | [6.4](6.4.md#examples) | Logic (Gate does not fit a browser) |
| Accelerator machine: one layer on the CPU and the unit | [6.5](6.5.md#examples) | Compact (a finer unit does not fit 69,671 cycles) |
