# § 5.1. RV32IM Core

The processor of this chapter's computer is the advanced RV32I pipeline of
[§ 4.6](../../rv32i/4.6/index.md) extended with the RISC-V **M** extension:
integer multiply, divide and remainder. The M results follow the ratified
[M 2.0 specification](https://docs.riscv.org/reference/isa/v20240411/unpriv/m-st-ext.html).

## Contents

| Section | Coverage |
| --- | --- |
| [5.1.1. Multiply and Divide Units](arithmetic.md) | `MultiplyStep<W>`, `DivideStep<W>`, `Multiplier<W>`, `Divider<W>`, `MulDivUnit<W>` |
| [5.1.2. Pipeline Parts](pipeline.md) | `RV32IMDecodeControlUnit`, `RV32IMDecodeStage`, `RV32IMExecuteStage`, `RV32IMPipelineCoordinator` |
| [5.1.3. RV32IMFiveStageCore](core.md) | The complete core and its two ports |
| [5.1.4. Recorded Programs](programs.md) | Programs 23–27: every M instruction, dependencies, divide boundaries, Fibonacci, GCD |

## Design choices

- **Iterative arithmetic.** One bit per clock edge: a 32-bit multiply or
  divide takes 33 edges. A reused step datapath performs each iteration;
  its intermediate state is visible in the simulator.
- **Shared RV32I structures.** Decode control, execute and the coordinator
  wrap the corresponding integer part. The decode stage reuses the shared
  blueprint with the M-aware decoder, and the core reuses the common
  five-stage assembly. The RV32I contracts remain separate.
- **Wait, do not reorder.** The pipeline stalls while arithmetic runs.
  Nothing executes out of order, so retirement stays simple to check.

## Software

`tests/fixtures/rv32im/` holds a bare-metal C smoke program, its startup,
linker script, pinned toolchain details, disassembly and ELF hash;
`tools/rv32im/build-smoke.py` builds it with `-march=rv32im -mabi=ilp32`.
The independent M instruction oracle (`src/rv32im/`) uses the RV32I oracle
for integer instructions and is never called by a simulated component. The
games of [§ 5.6](../5.6.md) are compiled the same way.
