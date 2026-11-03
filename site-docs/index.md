# CircuitSim

CircuitSim is a digital logic simulator that builds a computer from the
bottom up: gates first, then adders and an ALU, memories from single cells
to complete devices, RV32I processors that run real programs, and finally a
computer with buses and devices that runs small games.
Every larger part is built from smaller ones, and most parts also have a fast
behavioral form that is checked against its gates and runs on the delays
proven on them.

| Chapter | What it builds |
| --- | --- |
| [§ 1 Basic](basic/index.md) | The engine (values, wires, events, structural and compact forms), how long every part takes, and the basic parts: gates, word gates, wiring, constants, muxes, decoders and the clock |
| [§ 2 ALU](alu/2.1.md) | Ripple and prefix adders, logic, shifts, comparison, flags and the 29-operation `ALU<W>` at 8, 16 or 32 bits |
| [§ 3 Memories](memory/architecture.md) | Flip-flop storage, SRAM, DRAM, ROM and NOR flash from cell to device, a common transaction protocol, controllers, measured timing, and caches |
| [§ 4 RV32I](rv32i/4.1/index.md) | The instruction set, building blocks, a single-cycle core, a baseline and an advanced five-stage pipeline, memory systems with caches, and experiments comparing them |
| [§ 5 Computer System](system/index.md) | Multiply and divide, buses and bridges, a memory subsystem, devices built from register tables, machines described in JSON, and C games running on them |

Each component page gives its pins, behavior, structure, timing and a
recorded example you can replay in the embedded simulator. The
[interactive simulator](visualizer.md) opens any scenario full size, in any
timing profile.

<a href="simulator/" class="md-button md-button--primary">Open the simulator</a>
