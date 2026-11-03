# § 1. Basic

This chapter describes the engine every circuit on this site runs on, and the
small parts every later chapter builds from. Later chapters assume it: they
name a gate, a splitter or a two-input mux without describing it again.

| Section | What it covers |
| --- | --- |
| [1.1. Engine](1.1.md) | Four-state values, pins and wires, components, how circuits are built, events and history, structural and compact forms |
| [1.2. Time and Delay](1.2.md) | Simulated time, timing profiles, gate and compact-part delays, clocked parts, memories and clocks |
| [1.3. Logic Gates](1.3.md) | `NOTGate`, `ANDGate`, `ORGate`, `NANDGate`, `NORGate`, `XORGate` and the word gates |
| [1.4. Wiring and Constants](1.4.md) | `ConstantValue<W>`, `BitSplitter<W>`, `BitJoiner<W>` and `Rewire` |
| [1.5. Selection and Decoding](1.5.md) | `Mux<N,W>` and `Decoder<N>` |
| [1.6. Clock Generator](1.6.md) | `ClockGenerator` |

Storage starts in the memory chapter: the latches and the D flip-flop are in
[§ 3.3.1 Flip-Flop Storage](../memory/technologies/flip-flop.md), and the
register and state bit of the processors in
[§ 4.3 Registers](../rv32i/4.3/register.md).
