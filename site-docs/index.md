# CircuitSim

CircuitSim is a digital logic simulator that builds a computer from the
bottom up: gates first, then adders and an ALU, memories from single cells
to complete devices, and finally RV32I processors that run real programs.
Every larger part is built from smaller ones, and most parts also have a fast
behavioral form that is checked against its gates and runs on the delays
proven on them.

| Chapter | What it builds |
| --- | --- |
| [§ 1 Time and Delay](timing.md) | How long every part takes: timing profiles, gate delays, proven delay tables, clocked parts and certified clocks |
| [§ 2 ALU](alu/2.1.md) | Width-parameterized adders, logic, shifts, comparison and flags; extended and integer ALU operation contracts |
| [§ 3 Memories](memory/architecture.md) | Flip-flop storage, SRAM, DRAM, ROM and NOR flash from cell to device, a common transaction protocol, controllers, measured timing, and caches |
| [§ 4 RV32I](rv32i/4.1/index.md) | The instruction set, building blocks, a single-cycle core, a baseline and an advanced five-stage pipeline, memory systems with caches, and experiments comparing them |

Each component page gives its pins, behavior, structure, timing and a
recorded example you can replay in the embedded simulator. The
[interactive simulator](visualizer.md) opens any scenario full size, in any
timing profile.

<a href="simulator/" class="md-button md-button--primary">Open the simulator</a>
