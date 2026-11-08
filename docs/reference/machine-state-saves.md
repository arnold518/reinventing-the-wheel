# Machine state saves

Code: `include/simulator/MachineState.hpp`, `include/simulator/StateArchive.hpp`,
`src/simulator/MachineState.cpp`, the runner in `src/runtime/RecordedGuest.cpp`.
Record: [replayable recordings](../records/2026-11-06-replayable-recordings.md).

A save holds the complete state of a simulated tree at one moment, so that a
fresh build of the same tree continues exactly as the saved run would have:
the same events, in the same order, at the same ticks. Saves let a long game
be replayed in the browser from any point (event by event), and let a long
native recording continue after an interruption.

## What a save holds

`circuit::state::saveMachine(root, simulator, writer)`:

| Section | Content |
| --- | --- |
| Header | format version; counts of components, pins and wires; a hash of every component's name and type and every pin's name and width |
| Queue | simulator time, the next scheduling order, the performance counters, every queued event that is not withdrawn (wire update: wire number and packed value; evaluation: component number) in processing order, and the last value of each output pin that has no wire (history off) |
| Wires, pins | every value: its known bits in one bit stream, then the values holding unknown or floating bits |
| Parts | for each evaluated part (`BasicComponent`): a flag byte; its delay-table memory (input values at the last evaluation, input change times, pending publish times, gate-list twin state); its inertial-delay changes still due, naming their events by queue number; then the part's own state |

Components are numbered in a fixed order: pre-order over the children, each
component's own wires, then its input and output pins in name order with
their external and internal wires. Two builds of the same machine under the
same profile number identically; a restore refuses a save whose counts or
hash differ.

The checked runner (`runRecordedGuest`, `RecordedGuestOptions::save_at`) wraps
the machine with its own state: an identity hash of the program, input and
run options; the cycle; input cursors and scheduled presses; the ISA
reference state; the reference and bus memories (as bytes that differ from
the machine's RAM); the device model; the outstanding and completed
transactions; and, for `SaveContent::WithResults`, the results so far.
A save is taken at the top of a cycle, when the previous cycle has settled
through its last tick and none of the runner's clock or input drives are
queued.

Run saves are version 2: the machine section is stored with its length, so a
session continuing on the machine it came from (`resume_in_place`) reads only
the runner's part. Version 1 saves (machine inline) are still restored in
full. `circuit::state::restoreValues` applies only a save's wire and pin
values and time, for returning to the present after viewing history.

## Declared state

Every evaluated part type answers `BasicComponent::stateSupport()`:

- `Stateless`: everything is in its pins, wires and timing state (gates,
  `Rewire`, `BitSplitter`, `BitJoiner`, `ConstantValue`, word gates,
  `GateListCompact`).
- `Saved`: it writes its own members (`RegisterBehavioral`,
  `ClockedStateBitBehavioral`, `FlipFlopCellBehavioral`,
  `RegisterFileBehavioral`, `GateListPartBehavioral` for counters, FIFOs,
  device front ends, register ports and bus routers, `RequestKnownBits`, and
  `BehavioralMemoryEndpoint` for flip-flop memory).
- `Unknown` (the default): saving a tree that contains it fails and names its
  type. This keeps a member nobody declared from silently changing a resumed
  run.

The compact RV32IM core (`RV32IMCoreBehavioral`, a gate-list model of the
five-stage core with characterized registers) saves like the other
gate-list parts. Not yet declared, so not saveable: SRAM, DRAM, ROM and
NOR-flash endpoints, and all structural memory cells. The Visual and the
Compact game machines are fully declared.

Parts that keep a history only for viewing (the register file, memory
storage) restart it at the restored state (`stateRestored`,
`MemoryStorage::restoreState`). Simulator history restarts at the restored
time with every wire's restored value.

## Guarantees and checks

- `MachineStateResume_*_Test` (`tests/cpp/MachineStateProbe.cpp`): an
  uninterrupted run saves at cycles N and M; a second run restores the save
  at N into a freshly built machine and saves at M. The two saves at M must be
  equal byte for byte, and the frames after N must be equal.
- A resumed run keeps every check of the checked runner: each instruction,
  data access, device read and frame against the independent models.
- A save is valid only for the runner build, timing profile, clock period,
  fidelity preset, program and input that made it; the identity hash and the
  structure hash refuse anything else.

## Size

A Visual game machine's save is about 1.75 MB raw and 70–90 KB gzip; a
Compact one about 0.76 MB raw and 33 KB (November 2026). Wire and pin values are about 7 KB of it compressed; the
rest is mostly the timing state of compact parts with a gate-list twin (a
value and a change time per gate node). `CIRCUITSIM_SAVE_SIZES=1` prints
each part whose state exceeds 2 KB while saving.

## Limits

- Results with wall-clock times (`WithResults`) are not byte-comparable
  between runs.
- A memory with history on restarts its history at the restored contents and
  needs a dense copy of them; a memory too large for its resident budget
  cannot be restored with history on.
- A run that streams a signal trace cannot resume.
