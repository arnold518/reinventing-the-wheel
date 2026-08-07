# RV32I Five-Stage Core Implementation Report

Last updated: 2026-08-05

## Result

`RV32IFiveStageCore` is implemented and maintained alongside the timing and
performance work on `rv32i-pipeline-performance`.

It is a single-issue, in-order IF/ID/EX/MEM/WB pipeline with:

- four explicit pipeline registers;
- EX/MEM and MEM/WB forwarding;
- exact load-use detection and bubble insertion;
- EX-stage branch and jump resolution;
- redirect flushing;
- instruction- and data-memory ready/fault handling;
- byte, halfword, and word alignment checks;
- conventional MEM-stage stores and ordered register retirement;
- precise halt and trap retirement; and
- stage, stall, commit, memory, and trap observation pins.

The completed single-cycle implementation was not modified into a pipeline and
remains available for correctness and later efficiency comparisons.

## External Contract

Inputs:

| Signal | Meaning |
| --- | --- |
| `CLK` | Pipeline clock. |
| `RST` | Reset architectural and pipeline state. |
| `ENABLE` | Permit fetch, movement, and retirement. |
| `IMEM_READ_DATA`, `IMEM_READY`, `IMEM_FAULT` | Instruction-memory response. |
| `DMEM_READ_DATA`, `DMEM_READY`, `DMEM_FAULT` | Data-memory response. |

Memory request outputs:

| Signal | Meaning |
| --- | --- |
| `IMEM_ADDR`, `IMEM_READ_EN` | Current instruction fetch request. |
| `DMEM_ADDR`, `DMEM_WRITE_DATA` | Current data address and store value. |
| `DMEM_READ_EN`, `DMEM_WRITE_EN` | Load/store request enables. |
| `DMEM_SIZE`, `DMEM_SIGN_EXTEND` | Byte/halfword/word width and load extension. |

Architectural and retirement outputs:

| Signal | Meaning |
| --- | --- |
| `PC` | PC of the most recently retired architectural instruction. |
| `FETCH_PC` | Current speculative/sequential fetch PC. |
| `HALTED`, `TRAPPED`, `TRAP_CAUSE` | Terminal architectural state. |
| `INSTRUCTION_ATTEMPT`, `COMMIT_VALID` | Attempt/retirement observations. |
| `RETIRED_COUNT` | Number of retired instructions. |
| `RETIRED_PC`, `RETIRED_INSTRUCTION` | Identity of the retiring instruction. |
| `RETIRED_MEM_*` | Logical memory action associated with that retirement. |

Pipeline outputs:

| Signal | Meaning |
| --- | --- |
| `PIPELINE_STALL` | Some dependency or memory condition is holding progress. |
| `IF_ID_VALID`, `IF_ID_PC` | IF/ID stage occupancy and PC. |
| `ID_EX_VALID`, `ID_EX_PC` | ID/EX stage occupancy and PC. |
| `EX_MEM_VALID`, `EX_MEM_PC` | EX/MEM stage occupancy and PC. |
| `MEM_WB_VALID`, `MEM_WB_PC` | MEM/WB stage occupancy and PC. |

These are ordinary simulator pins. Tests and the HTML visualizer observe the
same history; no testing-only mutation path is required.

## Structural Shape

The structural core wires ten peer components:

```text
FETCH -> IF_ID -> DECODE -> ID_EX -> EXECUTE
      -> EX_MEM -> MEMORY -> MEM_WB -> WRITEBACK

                         COORDINATOR
```

`FETCH`, `DECODE`, `EXECUTE`, `MEMORY`, and `WRITEBACK` are semantic component
families, not visual grouping placeholders. Their structural implementations
contain the detailed PC, register-file, forwarding, ALU, alignment, memory,
retirement, and architectural-state circuits. Their behavioral
implementations expose the same pins as compact answer sheets.

The old prototype placed 144 implementation details directly under the core.
The final default core has ten direct children (11 components including the
root), so its overview shows pipeline architecture before gate detail.

The profile is recursive. Selecting the core as structural exposes this
wiring. Each child family then independently follows the same profile and can
be structural or behavioral. There are no structural/behavioral class names in
the profile.

The compact core implementation,
`RV32IFiveStageCoreDirect`, implements the same pin and architectural-state
contract. It is the answer-sheet/fast fidelity, not a child hidden inside the
structural core.

## Pipeline Register Payloads

- IF/ID: PC, instruction, fetch status.
- ID/EX: PC, instruction, register values, immediate, packed controls,
  register addresses, fetch status.
- EX/MEM: PC, instruction, ALU/store/link/next-PC values, packed controls,
  register addresses, fetch status, execution status.
- MEM/WB: the EX/MEM payload plus memory data and memory status.

`VALID` says whether the payload represents a real instruction. A flush clears
only `VALID`; the payload bits become don't-care and need not pass through a
large bank of zero-select muxes. Write-disable still holds the previous
payload.

## Hazard Behavior

### Forwarding

For each EX operand:

1. Use the EX/MEM result when it writes the same nonzero register and the value
   is ready.
2. Otherwise use MEM/WB when it writes the same nonzero register.
3. Otherwise use the value read in ID.

The closer EX/MEM producer wins over an older MEM/WB producer.

### Load-use stall

A load result is not ready in EX. If the instruction in ID actually uses that
load's destination:

- hold fetch PC;
- hold IF/ID;
- flush/bubble ID/EX;
- allow the older load to continue.

The decoder's `USES_RS1` and `USES_RS2` signals prevent false stalls on bit
fields that happen to look like register addresses but are not operands for
that instruction.

### Control-flow flush

Fetch uses `PC + 4`. A taken branch, `JAL`, or `JALR` redirects in EX. The
younger instructions in IF and ID are invalidated before they can retire.

## Precise Halt and Traps

Terminal decisions become architectural only at WB:

- older instructions may finish;
- younger instructions are prevented from committing;
- faulting stores do not modify memory;
- a trap records one stable project trap cause;
- `EBREAK` halts;
- `ECALL`, illegal encodings, instruction faults/misalignment, data
  faults/misalignment, and misaligned taken targets trap.

The retirement unit defines the priority when more than one error signal is
present. Its dedicated test checks that priority independently of the core.

Stores themselves use the MEM-stage port. The coordinator suppresses a younger
MEM request when an older WB instruction is becoming terminal. Misaligned
requests are also suppressed, and the memory contract rejects faulting writes.
The stable `RETIRED_MEM_*` pins describe the store later, when it retires; they
are observation state, not a second data-memory path.

## Tests

New component scenarios:

- five semantic stage tests;
- four pipeline-register tests;
- forwarding-unit test;
- hazard-detection test;
- pipeline control-flow test;
- memory-alignment test;
- retirement-unit test;
- coordinator test;
- five-stage core contract test.

Each component test is reused by CTest and the visualizer and checks structural
and behavioral fidelity under one scenario name.

Program validation reuses the original 16 fixtures documented in
`docs/rv32i-milestone-7c-program-tests.md`. They cover:

- arithmetic, logic, shifts, comparisons, `LUI`, and `AUIPC`;
- all load/store widths and sign-extension behavior;
- all six branch conditions;
- `JAL` and `JALR`;
- back-to-back dependencies and loops;
- illegal instruction and `ECALL`;
- instruction/data misalignment and access faults.

Programs 17-22 add dependency, control-flow, load-use, and mixed-memory
performance workloads. All 22 run through the same correctness path.

For every program:

1. a behavioral five-stage core runs independently;
2. the selected-profile core runs independently;
3. both lockstep against `RV32IInstructionOracle`;
4. semantic commit checkpoint details are compared;
5. hard-coded final registers, memory writes, halt, and trap expectations are
   checked.

Per-retirement store comparison uses `RETIRED_MEM_*`. This matters because a
real five-stage store changes physical memory in MEM, one cycle before that
instruction reaches WB. The test still compares the complete physical memory
write history at program completion, so the trace cannot hide a missing or
extra write.

## Educational Datapath Resource Cleanup

The 2026-08-05 cleanup removed several accidental circuit duplications while
keeping the same public component contracts:

- `ALU32` now contains one shared `AddSub32`, not separate add, subtract, and
  comparator subtraction chains. Equality and signed/unsigned less-than flags
  come from that shared subtraction result.
- `ALU32` contains one controlled `Shifter32`. The shifter contains one
  five-level barrel network; input/output reversal reuses it for left shifts,
  and one fill control selects logical versus arithmetic right shift.
- EX branch comparison reuses the ALU comparison flags instead of owning a
  second `Comparator32`.
- Pipeline flushes clear only valid bits rather than muxing zero through every
  payload bit.
- Stores use the MEM-stage address/data/control already present in EX/MEM;
  the old WB-to-MEM feedback path and its duplicate alignment/control logic
  were removed.

The default program-09 visual hierarchies changed as follows:

| Scenario | Before | After | Reduction |
| --- | ---: | ---: | ---: |
| Single-cycle system | 14,337 | 9,826 | 4,511 (31.5%) |
| Five-stage core fixture | 21,635 | 13,243 | 8,392 (38.8%) |
| Structural single-cycle `ALU32` subtree | 9,940 | 4,329 | 5,611 (56.4%) |

These counts describe simulator/visualizer components, not transistor count or
silicon area. They are useful evidence that the educational diagram now maps
more closely to shared hardware resources. Retirement counters and stable
trace registers remain intentionally visible instrumentation in WRITEBACK;
the architecture profile keeps storage families behavioral by default so
they do not expand into thousands of cells unless the user requests it.

The parent/core program profile now selects non-storage `rv32i.*` contracts
structurally. `Memory64Kx32`, `RegisterFile32x32`, `Register32`, and
`MemoryBit` select behavioral fidelity by default so their specialized
visualizer drawings are used. Other reusable non-RV32I contracts also select
behavioral implementations when available, while fixed gates and rewires
remain structural primitives. This makes the pipeline stages, control units,
decoder, and ALU visible without expanding thousands of storage cells. A user
may still collapse or expand any selectable subtree through a handwritten or
visualizer profile.

## Simulator Ordering Correction

Pipeline bring-up exposed an existing event-engine ambiguity: events with the
same time and priority had no deterministic order. Two zero-delay evaluations
could schedule opposite values to one wire at the same timestamp, and the
priority queue could apply them in the wrong order.

Events now receive a monotonically increasing scheduling order. Ordering is:

1. earlier simulation time;
2. wire updates before component evaluations at equal time;
3. FIFO order for otherwise equal events.

`SimulatorDrainUntilIdleTest` now verifies that the last same-time scheduled
wire value wins. This fixes a simulator-wide race rather than adding a
pipeline-specific workaround.

## Visualizer Integration

- All new block and program scenarios are registry-discovered.
- `RV32IFiveStageCore` and its blocks are exported through pybind11.
- Aliases include `rv32i-five-stage-core` and
  `rv32i-five-stage-program1` through `rv32i-five-stage-program20`.
- The 20 program layouts share one representative topology calculation.
- The core layout reads from left to right in pipeline order, with thin
  pipeline-register separators and a separate coordinator control rail.
- The core aspect ratio is `0.60` height per unit width instead of `4.29`.
  Its dense public pins use a component-specific marker size, and the
  coordinator uses a wide `0.35` control-rail aspect.
- Component titles are capped by their actual title-bar height, preventing a
  wide component name from obscuring its contents.
- Default layouts are regenerated for the final profile fingerprints.

## Final Validation

The implementation baseline passed in one serial repository-wide CTest run:

```text
all repository tests: 169/169 passed
new pipeline block/core scenarios: 16/16 passed
single-cycle programs: 20/20 passed
five-stage programs: 20/20 passed
visualizer integration tests: 2/2 passed
total real test time: 1141.05 seconds
```

After changing the default five-stage profile and layout, the directly
affected regressions were rerun:

```text
recursive profile generator: 1/1 passed
five-stage core contract: 1/1 passed
five-stage programs: 16/16 passed in 204.62 seconds
visualizer integration: 1/1 passed in 72.61 seconds
Python syntax, JavaScript syntax, and git diff checks: passed
```

After applying the shared behavioral-storage rule to all RV32I visualizer
scenarios, the affected validation passed again:

```text
profile and RV32I block/core scenarios: 9/9 passed
single-cycle and five-stage programs: 32/32 passed in 136.69 seconds
visualizer integration: 2/2 passed in 23.90 seconds
```

All 16 program scenarios also passed instruction-by-instruction oracle
lockstep at both root fidelities. Their measured pipeline cycles were:

| Program | Retired instructions | Pipeline cycles | CPI |
|---:|---:|---:|---:|
| 1 | 29 | 43 | 1.483 |
| 2 | 35 | 39 | 1.114 |
| 3 | 17 | 21 | 1.235 |
| 4 | 24 | 42 | 1.750 |
| 5 | 11 | 25 | 2.273 |
| 6 | 10 | 14 | 1.400 |
| 7 | 61 | 79 | 1.295 |
| 8 | 42 | 59 | 1.405 |
| 9 | 2 | 6 | 3.000 |
| 10 | 2 | 6 | 3.000 |
| 11 | 2 | 6 | 3.000 |
| 12 | 3 | 7 | 2.333 |
| 13 | 2 | 6 | 3.000 |
| 14 | 3 | 7 | 2.333 |
| 15 | 2 | 6 | 3.000 |
| 16 | 3 | 9 | 3.000 |
| **Total** | **248** | **375** | **1.512 weighted** |

The single-cycle core nominally completes one instruction per cycle. These
counts do not yet prove a wall-clock hardware speedup: the single-cycle and
pipeline clock periods need calibrated critical-path delay models before that
comparison is meaningful.

### Performance-focused program scenarios

Programs 17 through 22 combine isolated workloads with two application-style
loops for comparing the single-cycle and five-stage cores. They remain
correctness tests:
both cores run the same program case, compare every retired instruction with
the functional oracle, and check an independently specified final state.

| Program | Focus | Retired instructions | Single-cycle cycles | Five-stage cycles | Five-stage CPI |
|---:|---|---:|---:|---:|---:|
| 17 | Interleaved independent ALU chains | 73 | 73 | 77 | 1.055 |
| 18 | Adjacent dependent ALU chain | 66 | 66 | 70 | 1.061 |
| 19 | Unrolled load followed immediately by use | 67 | 67 | 103 | 1.537 |
| 20 | Branch-heavy counted loop | 51 | 51 | 85 | 1.667 |
| 21 | 16-word array sum | 84 | 84 | 134 | 1.595 |
| 22 | Copy plus checksum | 61 | 61 | 87 | 1.426 |
| **Total** | | **402** | **402** | **556** | **1.383 weighted** |

Programs 17 and 18 differ by dependency distance but have nearly identical
CPI. This is evidence that the current forwarding paths remove adjacent ALU
read-after-write stalls. Program 19 adds one load-use dependency per pair and
therefore pays about one extra pipeline cycle per load. Program 20 takes 15
backward branches; its additional 30 cycles match the current two-cycle taken
branch redirect penalty.

The cycles above are modeled CPU cycles, not host execution time. They were
read from the shared instruction checkpoints after the eight structural and
behavioral regressions passed. The memory model still answers immediately, so
program 19 measures the core's load-use interlock rather than cache or main
memory latency.

Run only these performance scenarios with:

```bash
ctest --test-dir build --output-on-failure \
  -R 'RV32I(SingleCycleSystemTest|FiveStageCoreProgramTest)/program-(17|18|19|20|21|22)$'
```

The final deterministic layout pass completed without an overlap exception:

```text
scenarios: 154
representative topologies: 112
components traversed: 165,231
parent topologies: 196
type layouts: 131
root layouts: 154
```

The current deterministic pass traversed 165,231 components across its 112
representative topologies. The default five-stage program scenario now
contains 13,243 components: 20 selectable non-storage `rv32i.*` components
are structural, 57 storage
components are behavioral, and the remaining nodes are fixed primitives
inside those structural blocks. Immediate subtree sizes are 931 for Fetch,
7 for IF/ID, 3,260 for Decode, 12 for ID/EX, 6,591 for Execute, 14 for
EX/MEM, 596 for Memory, 16 for MEM/WB, 1,746 for Writeback, and 63 for the
coordinator. The compact pipeline registers reflect the valid-only flush and
behavioral-storage profile; their structural forms remain selectable and are
tested independently.

`VisualizerModuleBindingsTest` verifies the ten-child order, absence of
default overlap, wide core aspect, lower coordinator rail, structural
selection for non-storage RV32I components, and behavioral selection for
every encountered storage family. It also verifies live profile rebuild and
preserved simulation checkpoints.

The earlier live-service verification covered the original 142-scenario,
16-program visualizer state. The current local backend and regenerated layout
advertise 154 scenarios and all 22 programs, and
`VisualizerModuleBindingsTest` passes against that state. The systemd service
was restarted after the timing/counter work so port 8765 now serves the new
registry and performance metrics.

## Honest Boundary

“Structural RV32I architecture” means the core and its non-storage RV32I
datapath/control descendants are structural. Storage is deliberately
behavioral in the default visualizer scenarios because its lower-level
implementations are already independently tested and its compact drawings are
more useful at CPU scale. It also does not mean every reusable non-RV32I
descendant must be expanded to gates. Doing that would retest child circuits,
add their propagation delays to the core timing contract, and create an even
larger simulation.

The lower-level-first proof is instead compositional:

- structural children are tested directly;
- their behavioral abstractions are checked against the same expected
  contract;
- the structural parent is tested using those child contracts;
- recursive profiles still permit selected or complete structural expansion
  for learning and experiments.

## Remaining Follow-Up Work

- Use the completed timing/counter baseline in
  `rv32i-timing-performance-report.md` for the next experiments.
- Add caches and memory-latency experiments on top of the stable pipeline
  contract.
