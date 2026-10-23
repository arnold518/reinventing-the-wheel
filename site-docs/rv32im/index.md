# RV32IM components

The RV32IM core implements the eight integer multiply/divide instructions in
the five-stage pipeline. Its instruction formats, packets, memory ports and
retirement/state views follow the CPU contracts. Arithmetic hardware is
parameterized independently: the core selects W=32. Build profiles select
structural or available compact implementations.

The component scenarios open with structural architecture and functional
blocks, using descriptive root names consistent with the RV32I scenarios.
Storage and lower-level parts may use their compact implementations. In the
explorer, the triangle selects structural or behavioral fidelity where both
are available; press **Apply & Run** to rebuild the view. Expanding a component
changes that component only, so its children retain their own selections.

[Recorded RV32IM programs](programs.md) add complete instruction execution:
all eight M instructions, dependencies with memory and branches, and divide
boundaries. They have their own menu group in the scenario visualizer.

## Arithmetic

- [MultiplyStep](multiply-step.md) and [DivideStep](divide-step.md) support
  W=4/8/16/32; the four-bit recordings expose representative lower-level slices.
- [`Multiplier<W>`](multiplier.md), [`Divider<W>`](divider.md) and
  [`MulDivUnit<W>`](muldiv.md) use one launch edge, W enabled iteration edges
  and retained completion, with synchronous cancellation.
- The combined 32-bit arithmetic unit and the CPU also provide compact forms
  that execute their own structural gate lists with characterized register state.

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
