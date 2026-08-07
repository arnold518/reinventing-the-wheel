# RV32I Five-Stage Core Implementation Plan

Last updated: 2026-08-05

## Goal

Add an educational, single-issue, in-order RV32I pipeline without changing or
replacing the completed single-cycle CPU.

The new root is `RV32IFiveStageCore`. It keeps the same separate instruction-
and data-memory boundary as `RV32ISingleCycleCore`, while exposing extra
pipeline and retirement signals for tests and visualization.

This milestone deliberately focuses on the core. The existing program test
fixture supplies instruction and data memories, so a second near-identical
five-stage system wrapper is not required yet.

## Pipeline

```text
IF             ID             EX             MEM            WB
fetch          decode         execute        memory         retire
PC + IMEM  ->  registers  ->  ALU/branch  -> load data  -> reg write
        IF/ID          ID/EX           EX/MEM         MEM/WB
```

- **IF** requests the instruction at the fetch PC.
- **ID** decodes it and reads `rs1` and `rs2`.
- **EX** performs the ALU operation, address calculation, comparison, and
  branch/jump decision.
- **MEM** completes loads and performs aligned, non-faulting stores.
- **WB** retires the oldest instruction, writes the register file, or records
  halt/trap state. It also publishes the stable retirement trace used by tests
  and the visualizer.

## Required Blocks

- Five semantic stage families:
  - `RV32IFetchStage`
  - `RV32IDecodeStage`
  - `RV32IExecuteStage`
  - `RV32IMemoryStage`
  - `RV32IWritebackStage`
- Four stage-specific pipeline-register families:
  - `RV32IIFIDPipelineRegister`
  - `RV32IIDEXPipelineRegister`
  - `RV32IEXMEMPipelineRegister`
  - `RV32IMEMWBPipelineRegister`
- `RV32IForwardingUnit`
- `RV32IHazardDetectionUnit`
- `RV32IPipelineControlFlowUnit`
- `RV32IMemoryAlignmentUnit`
- `RV32IPipelineRetirementUnit`
- `RV32IPipelineCoordinator`
- `RV32IFiveStageCore`

Each family has one external contract and structural and behavioral
implementations selected by the existing recursive build profile.

The structural core itself has exactly ten direct children:

```text
FETCH -> IF_ID -> DECODE -> ID_EX -> EXECUTE
      -> EX_MEM -> MEMORY -> MEM_WB -> WRITEBACK

                         COORDINATOR
```

The stage families are real circuit boundaries, not layout-only folders.
Consequently, a profile can collapse one stage to its behavioral answer sheet
or expand it structurally without changing the core class or its wiring.

## Control Rules

### Data dependencies

- Forward the newest ready value from EX/MEM.
- Otherwise forward a matching value from MEM/WB.
- Never forward register `x0`.
- Stall IF and IF/ID for the exact one-cycle load-use case, and inject a bubble
  into ID/EX.
- Bypass the WB result into ID so a register read at the writeback boundary
  sees the value being committed.

### Branches and jumps

- Fetch predicts the next instruction as `PC + 4`.
- Branches and jumps resolve in EX.
- A taken branch or jump redirects fetch and flushes younger instructions.
- `JALR` clears target bit 0.
- A taken misaligned target traps when that instruction reaches retirement.

### Memory and terminal events

- Memory waits hold the necessary pipeline state.
- Loads become forwardable only after their data is ready.
- Stores use the normal MEM-stage data port, as in a conventional five-stage
  pipeline. A store is suppressed when it is misaligned, faults, or an older
  WB instruction is becoming terminal, so younger work cannot escape a
  precise halt/trap boundary.
- Halt and trap requests drain or flush younger work and become architectural
  state only at the ordered retirement boundary.

## Testing Model

The tests follow the repository's component rule:

1. Each new block has one reusable test scenario.
2. That scenario drives the same inputs into both fidelities and checks each
   against explicit expected outputs.
3. The five stage scenarios expand their structural implementations through
   the normal recursive profile, so their internal lower-level components are
   tested independently of the parent.
4. `RV32IFiveStageCoreTest` checks the ten-child parent wiring while assuming
   its tested child contracts are correct.
5. Each of the 22 program fixtures runs:
   - the behavioral five-stage core as an answer sheet;
   - the selected-profile core;
   - the independent instruction oracle.
6. Program comparisons happen at semantic commit checkpoints, not at equal
   simulator timestamps. Structural propagation delays are allowed to differ.

The default program profile selects non-storage component contracts in the
`rv32i.` namespace structurally. The four storage families with dedicated
visualizer drawings—`Memory64Kx32`, `RegisterFile32x32`, `Register32`, and
`MemoryBit`—select their tested behavioral implementations. Other reusable
implementation details outside the RV32I namespace also select behavioral
implementations when available; fixed gates and rewires remain structural
primitives. This keeps the pipeline, control, decoder, and ALU visible while
showing memory and registers through their compact storage views. A
handwritten or visualizer profile may still collapse or expand any selectable
subtree.

## Visualizer

- Register all new block and program scenarios through the shared test
  registry.
- Export the new component families through the Python backend.
- Add convenient aliases for the core and all 22 programs.
- Recalculate deterministic default layouts.
- Place the nine datapath/state blocks in left-to-right pipeline order and put
  the coordinator on a separate control rail.
- Give the five-stage core a wide aspect ratio, with component-specific title
  and pin sizing so its dense public interface remains legible.
- Expose fetch PC, architectural PC, all four stage valid/PC pairs, pipeline
  stall, commit, retired instruction, retired memory access, halt, and trap.

## Acceptance Checklist

- [x] The new core is a separate family; the single-cycle core remains intact.
- [x] Structural and behavioral implementations exist for every new block.
- [x] Forwarding, load-use stalls, memory waits, redirects, flushes, halt, and
  precise traps are implemented.
- [x] Dedicated block tests pass at both fidelities.
- [x] Core-level structural/behavioral contract testing passes.
- [x] All 22 shared RV32I programs pass instruction-oracle lockstep.
- [x] The backend and visualizer discover the new scenarios.
- [x] Default layouts are regenerated.
- [x] The structural core overview contains ten meaningful children rather
  than a flat internal netlist.
- [x] Repository-wide coverage is green: all 175 tests pass, including both
  sets of 22 RV32I programs, fidelity-toggle coverage, and visualizer
  integration.

## Deferred Work

- A unified-memory system and structural cache hierarchy.
- Branch prediction beyond static next-PC fetch.
- Multi-cycle execution units.
- Pipeline performance dashboards and cache/pipeline comparison experiments.
- Privileged state, interrupts, and ISA extensions outside current RV32I scope.
