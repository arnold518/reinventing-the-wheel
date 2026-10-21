# RV32IM components

The RV32IM family adds the eight integer multiply/divide instructions to the
existing five-stage pipeline. RV32I families, ALU encodings, packets and memory
ports retain their contracts. All new components currently have structural
forms; their lower-level children remain selectable by profile. MulDivUnit32
and the five-stage core also have compact gate-list forms under validation.

## Arithmetic

- [MultiplyStep](multiply-step.md) and [DivideStep](divide-step.md): 4-bit
  representative slices and 32-bit datapath steps.
- [Multiplier32](multiplier.md), [Divider32](divider.md) and
  [MulDivUnit32](muldiv.md): launch, 32 enabled iteration edges and retained
  completion, with synchronous cancellation.

## Pipeline

- [Decode control](decode-control.md) and [decode stage](decode-stage.md).
- [Execute stage](execute-stage.md) and [coordinator](coordinator.md).
- [Five-stage core](core.md), with the existing retirement/state capabilities.

The architectural M results follow the ratified [M 2.0 specification](https://docs.riscv.org/reference/isa/v20240411/unpriv/m-st-ext.html).

## Software and limits

`tests/fixtures/rv32im/` contains a bare-metal C smoke program, startup, linker
script, pinned toolchain details, disassembly and ELF hash. The scoped
`tools/rv32im/build-smoke.py` script uses `-march=rv32im -mabi=ilp32`; it does
not alter RV32I compiler paths. The independent M oracle lives in `rv32im/`,
uses the existing I oracle for I instructions and is never called by a
simulated arithmetic component.

There is no new single-cycle M core, privilege/interrupt implementation,
compressed instruction support or floating point. The standalone core and
[device components](../device-io/index.md) are available separately. The new [device-system assembly](device-system.md) and
[recorded-game path](recorded-games.md) are undergoing integration validation. Existing recorded playback remains available.
