# RV32I Program Root Exposure Report

Last updated: 2026-08-09

## Result

All visual RV32I program scenarios now use one fixed testbench container:
`RV32IProgramRoot`, named `RV32I_PROGRAM_ROOT`.

Its direct hierarchy is always:

```text
RV32I_PROGRAM_ROOT
|- CLOCK
|- CORE
|- INSTRUCTION_MEMORY
`- DATA_MEMORY
```

There is no extra `MACHINE` component. The single-cycle and five-stage views
therefore look the same from the outside. The only family-level difference is
the implementation selected for `CORE`:

| Scenario family | `CORE` family |
| --- | --- |
| `RV32ISingleCycleSystemTest/program-XX` | `RV32ISingleCycleCore` |
| `RV32IFiveStageCoreProgramTest/program-XX` | `RV32IFiveStageCore` |

Both memory children use the same `Memory64Kx32` family and the normal RV32I
visual profiles select its compact behavioral implementation.

## Why The Root Is Fixed

`RV32IProgramRoot` is testbench wiring, not another CPU implementation. It
owns the common clock, connects the memory buses, and exposes reset, enable,
PC, halt, and trap observations. Consequently:

- the root has no structural/behavioral arrow;
- `CLOCK` is also fixed;
- recursive profile selection begins at `CORE` and other registered family
  children; and
- choosing a behavioral core does not make the clock or memories disappear.

This separates two questions cleanly: the testbench says what is connected,
while the recursive profile says how each selectable component is built.

## External Pins

Inputs:

| Pin | Meaning |
| --- | --- |
| `RST` | Reset the CPU state on a clock edge. |
| `ENABLE` | Permit CPU progress. |

Outputs:

| Pin | Meaning |
| --- | --- |
| `PC` | Current architectural PC observation. |
| `HALTED` | The program stopped through the project halt condition. |
| `TRAPPED` | Execution stopped because of a trap. |
| `TRAP_CAUSE` | Encoded reason for the trap. |

The clock is intentionally internal to the testbench. `CLOCK.CLK_OUT` is the
real source of the shared clock wire, which drives the core and both memory
components. The tests start that component at a known time, hold reset through
the first rising edge, then release reset before normal execution.

## Program Data And Observations

The shared root provides fixture helpers to:

- clear and load instruction memory;
- clear, load, and read data memory;
- enable memory-history recording;
- collect byte writes over a time range; and
- snapshot the architectural state through the core's `RV32IStateView`
  capability.

These helpers do not bypass execution. Program inputs are loaded before the
run, and correctness is still checked from the core state, memory history, and
normal circuit outputs.

## Visualizer Integration

The HTML visualizer gives `RV32IProgramRoot` a wide system-diagram layout:
instruction memory on the left, the CPU in the middle, data memory on the
right, and the clock in the lower-left service area. Child widths are fitted
from their real aspect ratios, so the tall single-cycle core and horizontal
five-stage core preserve the same outer organization.

The profile store translates these old path prefixes while loading:

```text
RV32I_SINGLE_CYCLE_SYSTEM_ROOT -> RV32I_PROGRAM_ROOT
RV32I_FIVE_STAGE_PROGRAM_ROOT  -> RV32I_PROGRAM_ROOT
```

Only the prefix changes. A saved subtree choice such as `.CORE.ALU` or
`.CORE.FETCH` remains attached to the same logical child. A legacy fidelity
choice on the old root itself is moved to `RV32I_PROGRAM_ROOT.CORE`, because
the new testbench root is fixed and the core is now the selectable CPU node.

## Verification

The implementation is covered by:

- every numbered program scenario for both CPU families;
- the visualizer module-binding test, including the exact four-child shape,
  fixed root, core fidelity switching, and default non-overlap checks;
- the profile-store migration test for both legacy root names; and
- live visualizer API and browser checks after layout regeneration.

Measured results on 2026-08-09:

- `cmake --build build -j4`: passed; the rebuilt Python module was deployed to
  `visualizer-v2` by the normal post-build step.
- `ctest --test-dir build --output-on-failure -j2`: 175/175 passed in
  597.40 seconds. This includes all 44 single-cycle/five-stage numbered
  program scenarios, `RV32IProfileToggleSweepTest`, and both visualizer tests.
- Post-cleanup focused rerun of single-cycle programs 9 and 16 plus five-stage
  program 9: 3/3 passed in 5.63 seconds.
- Full layout regeneration: 154 scenarios, 112 representative builds,
  165,227 component visits, 196 parent topologies, and 154 root layouts.
- Live port-8765 API check: both Program 9 scenarios reported
  `RV32IProgramRoot`, the exact four-child list, and a `CLK` wire sourced by
  `RV32I_PROGRAM_ROOT.CLOCK.CLK_OUT` with three sinks.
- Live browser screenshots completed after the blocking loading overlay had
  disappeared. The single-cycle view contained 9,824 components and the
  five-stage view contained 13,241 components; both used the same outside
  memory/core/clock arrangement.
