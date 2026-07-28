# RV32I Milestone 7 Report: System Harness And Behavioral System Preparation

Last updated: 2026-07-29

## Status

Milestone 7A and 7B are complete. This is a historical bring-up report; the
later unified structural/behavioral system and complete 16-program suite are
documented in `docs/rv32i-structural-core-report.md` and
`docs/unified-component-migration-report.md`.

Implemented:

- Milestone 7A instruction-lockstep test harness
- incremental forward simulation support for instruction-boundary testing
- final test contract for comparing RV32I components against `RV32IInstructionOracle`
- `RV32IReferenceCore`
- `RV32IReferenceSystem`
- two-memory `RV32IInstructionOracle::step` overload for separate instruction/data memories
- real RV32I assembly program lockstep test
- visualizer scenario alias for the RV32I system program

The broader RV32I system suite that was still planned when this report was
first written is now implemented as
`RV32ISingleCycleSystemTest/program-01` through `program-16`.

## Objective

Milestone 7 is the bridge from the pure instruction oracle to an actual circuit-simulator RV32I component.

Earlier milestones can decode, control, load, and execute RV32I instructions in pure C++. Milestone 7 defines how a real simulator component will be tested against that oracle.

The target component shape is:

```text
RV32IReferenceSystem
  contains:
    RV32IReferenceCore CORE
    Memory64Kx32 INSTRUCTION_MEMORY
    Memory64Kx32 DATA_MEMORY

  public pins:
    CLK
    RST
    ENABLE
    PC
    HALTED
    TRAPPED
```

The system should eventually run real RV32I programs while still behaving like a normal simulator component with pins, wires, events, tests, and visualizer support.

## Implemented Files

Milestone 7A harness:

- `include/tests/RV32IInstructionLockstepTests.hpp`
- `src/tests/RV32IInstructionLockstepTests.cpp`

Milestone 7B system and core:

- `include/modules/rv32i/RV32IReferenceCore.hpp`
- `src/modules/rv32i/RV32IReferenceCore.cpp`
- `include/modules/rv32i/RV32IReferenceSystem.hpp`
- `src/modules/rv32i/RV32IReferenceSystem.cpp`
- `include/tests/RV32ISystemTests.hpp`
- `src/tests/RV32ISystemTests.cpp`

Instruction oracle update:

- `include/rv32i/RV32IInstructionOracle.hpp`
- `src/rv32i/RV32IInstructionOracle.cpp`

Simulator/test support:

- `include/simulator/Simulator.hpp`
- `src/simulator/Simulator.cpp`
- `include/simulator/SimulationTest.hpp`
- `src/bindings/SimulatorBinding.cpp`
- `include/tests/CorePrimitiveTests.hpp`
- `src/tests/CorePrimitiveTests.cpp`

Build and registry:

- `CMakeLists.txt`
- `tests/CMakeLists.txt`
- `src/tests/TestRegistry.cpp`
- `src/bindings/ModulesBinding.cpp`
- `src/bindings/TestsBinding.cpp`
- `visualizer-v2/server.py`

Planning document:

- `docs/rv32i-milestone-7-plan.md`

## 7A Result

Milestone 7A adds `RV32IInstructionLockstepTest`.

It is a reusable base class for future RV32I system tests. It does not implement a CPU. Instead, it defines the comparison loop that any RV32I system component must pass.

The test flow is:

```text
load the same program and initial data into oracle and component
snapshot initial component state
compare initial state against oracle state

for each instruction:
  clock the component until it commits one instruction
  execute one instruction in RV32IInstructionOracle
  compare architectural state
  compare logical data-memory access
  compare actual data-memory byte writes
  repeat until halt, trap, or instruction limit
```

This is instruction-lockstep testing. It is not final-state-only testing and it is not cycle-by-cycle testing.

## 7B Result

Milestone 7B adds `RV32IReferenceSystem`, the first simulator system component that can execute an RV32I program while exposing the system shape in the visualizer.

Current implementation shape:

```text
RV32IReferenceSystem : IOComponent
  children:
    CORE : RV32IReferenceCore
    INSTRUCTION_MEMORY : Memory64Kx32
    DATA_MEMORY : Memory64Kx32

RV32IReferenceCore : BasicComponent
  private architectural state:
    RV32IState state
    RV32IMemoryTrace last_data_memory_access
    map<uint32_t, uint8_t> last_data_memory_writes

RV32IReferenceSystem public pins:
    CLK
    RST
    ENABLE
    PC
    HALTED
    TRAPPED
```

Execution behavior:

- `RST=1` resets core state to the configured initial PC.
- Reset does not clear instruction memory or data memory.
- On rising `CLK`, if `ENABLE=1`, the system commits at most one instruction.
- If the core is halted or trapped, additional clocks do not commit more instructions.
- `PC`, `HALTED`, and `TRAPPED` are published as normal output pins.
- The system exposes `snapshotState()`, `lastDataMemoryAccess()`, and `lastDataMemoryWrites()` for the 7A harness.
- The top-level visualizer root is `RV32IReferenceSystem`, not a behavioral black box.
- The visualizer shows `CORE`, `INSTRUCTION_MEMORY`, and `DATA_MEMORY` as child components.
- Program loading writes into the visible instruction memory component.
- Data initialization and stores write into the visible data memory component.

The core executes against attached `Memory64Kx32` memory components. To keep the instruction oracle as the behavior source, `RV32IInstructionOracle` now has two-memory overloads:

```cpp
RV32IInstructionTrace step(
    RV32IState& state,
    RV32IFunctionalMemory& instruction_memory,
    RV32IFunctionalMemory& data_memory
);

RV32IInstructionTrace step(
    RV32IState& state,
    Memory64Kx32& instruction_memory,
    Memory64Kx32& data_memory
);
```

The original one-memory API remains and delegates to this overload by passing the same memory as both instruction and data memory.

## 7B Assembly Test Program

`RV32ISingleCycleSystemTest/program-01` runs a real RV32I instruction stream through `RV32IReferenceSystem` and compares every committed instruction against `RV32IInstructionOracle`.

Initial data memory at `0x100`:

```text
uint32_t input[4] = {3, 5, 7, 11};
```

Assembly:

```asm
    addi x1, x0, 0x100     # x1 = input pointer
    addi x2, x0, 4         # x2 = element count
    addi x3, x0, 0         # x3 = sum

loop:
    lw   x4, 0(x1)         # x4 = *input
    add  x3, x3, x4        # sum += x4
    addi x1, x1, 4         # input++
    addi x2, x2, -1        # count--
    bne  x2, x0, loop      # continue until count == 0

    addi x5, x0, 0x120     # x5 = output pointer
    sw   x3, 0(x5)         # output word: 26
    sb   x3, 4(x5)         # output byte: 26
    addi x6, x3, 0x34      # x6 = 26 + 52 = 78
    sh   x6, 6(x5)         # output halfword: 78
    ebreak
```

Equivalent C:

```c
#include <stdint.h>

void rv32i_7b_program(void) {
    uint32_t input[4] = {3, 5, 7, 11};
    uint32_t sum = 0;

    for (uint32_t i = 0; i < 4; ++i) {
        sum += input[i];
    }

    uint8_t memory[0x130] = {0};

    memory[0x120] = (uint8_t)(sum & 0xff);
    memory[0x121] = (uint8_t)((sum >> 8) & 0xff);
    memory[0x122] = (uint8_t)((sum >> 16) & 0xff);
    memory[0x123] = (uint8_t)((sum >> 24) & 0xff);

    memory[0x124] = (uint8_t)sum;

    uint16_t half = (uint16_t)(sum + 0x34);
    memory[0x126] = (uint8_t)(half & 0xff);
    memory[0x127] = (uint8_t)((half >> 8) & 0xff);
}
```

Expected key results:

- `x3 = 26`
- `x6 = 78`
- `SW` writes `0x1a 0x00 0x00 0x00` at `0x120`
- `SB` writes `0x1a` at `0x124`
- `SH` writes `0x4e 0x00` at `0x126`

The lockstep test checks these memory effects through per-instruction memory traces and byte-write maps rather than final whole-memory comparison.

## Component Test Contract

Future RV32I system tests inherit from `RV32IInstructionLockstepTest` and provide these adapter methods:

```cpp
virtual RV32ISystemProgramCase getCase() const = 0;
virtual void initializeComponentForLockstep(const RV32ISystemProgramCase& test_case) = 0;
virtual void clockComponentOneCycle(size_t cycle_index, size_t cycle_start_time) = 0;
virtual rv32i::RV32IState snapshotComponentState() const = 0;
virtual rv32i::RV32IMemoryTrace lastDataMemoryAccess() const = 0;
virtual std::map<uint32_t, uint8_t> lastDataMemoryWrites() const = 0;
```

The component under test must expose enough getter state to answer those methods. This avoids adding debug pins just for tests.

## Test Case Model

`RV32ISystemProgramCase` describes inputs, not expected answers:

```cpp
struct RV32IMemoryInit {
    uint32_t address = 0;
    std::vector<uint8_t> bytes;
};

struct RV32ISystemProgramCase {
    std::string name;
    rv32i::RV32IProgram program;
    uint32_t program_base = 0;
    uint32_t initial_pc = 0;
    std::array<uint32_t, 32> initial_registers{};
    std::vector<RV32IMemoryInit> initial_data;
    size_t max_instructions = 128;
    size_t max_cycles_per_instruction = 16;
    size_t cycle_time_step = 10;
    size_t memory_size_bytes = rv32i::RV32IFunctionalMemory::DefaultCapacityBytes;
};
```

Expected behavior comes from `RV32IInstructionOracle`, not from hand-written expected rows.

## State Comparison

Each committed instruction compares `RV32IState`:

| Field | Meaning |
| --- | --- |
| `pc` | Address of the next instruction. |
| `x[0..31]` | Architectural integer registers. |
| `halted` | Project-level halt, currently from `EBREAK`. |
| `trapped` | Trap state for illegal instruction, `ECALL`, memory fault, fetch fault, or instruction limit. |
| `trap_cause` | Exact trap reason. |
| `instruction_count` | Number of committed/attempted instruction executions. |

This checks architectural CPU state after every committed instruction.

## Memory Comparison

Memory is outside the core state, so it needs separate checks. Comparing the whole memory every instruction would be too expensive and too noisy.

Milestone 7A uses two memory checks.

### Logical Data-Memory Access

The oracle emits one `RV32IMemoryTrace` per instruction through `RV32IInstructionTrace::memory`.

The component must expose the matching data-memory access through:

```cpp
rv32i::RV32IMemoryTrace lastDataMemoryAccess() const;
```

Compared fields:

| Field | Meaning |
| --- | --- |
| `kind` | `None`, `Read`, or `Write`. |
| `size` | `None`, `Byte`, `Halfword`, or `Word`. |
| `sign_extend` | Whether a load sign-extends the read value. |
| `address` | Effective byte address. |
| `write_data` | Source register data for stores. |
| `read_data` | Final loaded value for loads. |
| `fault` | Whether the access trapped. |

This catches load behavior, store intent, address calculation, size selection, sign extension, and data-memory faults.

### Actual Data-Memory Writes

The oracle derives expected byte writes from successful store traces.

The component must expose actual byte writes for the last committed instruction:

```cpp
std::map<uint32_t, uint8_t> lastDataMemoryWrites() const;
```

The map means "bytes written by this instruction", not "bytes changed".

Examples:

```text
SB address A, write_data 0x12345678:
  A -> 0x78

SH address A, write_data 0x12345678:
  A + 0 -> 0x78
  A + 1 -> 0x56

SW address A, write_data 0x12345678:
  A + 0 -> 0x78
  A + 1 -> 0x56
  A + 2 -> 0x34
  A + 3 -> 0x12
```

Non-store instructions, loads, and faulted stores must expose an empty write map.

Manual memory probes were removed from the final design. They are easy to forget and only check selected ranges. The trace plus write-map contract checks each instruction's memory behavior automatically.

## Simulator Changes

Milestone 7A adds incremental forward simulation.

### `Simulator::advanceAndRecord(target_time)`

This advances the event simulation from the current time to an absolute target time.

Behavior:

```text
process queued events with time <= target_time
keep already-recorded wire history
leave events after target_time in the queue
do not reset current_time to 0
do not clear history
do not clear the circuit
```

This is needed because the lockstep harness must run the simulator a little, inspect the component after one instruction commits, then continue.

### `SimulationTest::runSimulation()`

`SimulationTest::run()` now calls a virtual `runSimulation()` hook.

Default behavior remains:

```cpp
virtual void runSimulation() {
    sim->runAndRecord(getRunDuration());
}
```

Existing tests keep their old behavior. RV32I lockstep tests override `runSimulation()` with an instruction-boundary loop.

## Visualizer Decision

No visualizer behavior changes were made in 7A.

The visualizer still uses wire and pin history:

```text
run full simulation
record wire changes
setCircuitStateAtTime(t)
draw pins and wires at time t
```

This is safe for current visualizer behavior because it does not display private behavioral component state.

Important future note:

- wire/pin time travel is supported today
- private behavioral state time travel is not supported today
- future memory/register/PC inspectors need a separate observable-history design

That decision is intentionally delayed until after Milestone 7.

## Verification

Build:

```bash
cmake --build build -j$(nproc)
```

Result:

- passed
- rebuilt `circuit_backend`
- copied backend module to `visualizer-v2`

Focused 7A/7B tests:

```bash
ctest --test-dir build --output-on-failure -R "RV32ISingleCycleSystemTest/program-01|RV32IInstructionLockstepHarnessTest|RV32IInstructionOracleTest|SimulatorAdvanceAndRecordTest"
```

Result:

- passed: 4/4
- time: 0.52 seconds

Visualizer backend discovery:

```bash
python3 -c "import circuit_backend as cb; print('RV32ISingleCycleSystemTest/program-01' in cb.get_registered_test_names())"
```

Result:

- `True`

Live visualizer check:

```bash
curl -fsSL http://127.0.0.1:8765/api/circuit?scenario=rv32i-program1
```

Result:

- `rootType` is `RV32IReferenceSystem`.
- children are `CORE`, `INSTRUCTION_MEMORY`, and `DATA_MEMORY`.
- stats report 4 components, 56 pins, and 34 wires.

Whitespace check:

```bash
git diff --check
```

Result:

- passed

Last full branch regression before Milestone 7B:

```bash
ctest --test-dir build --output-on-failure
```

Result:

- passed: 90/90
- time: 1095.44 seconds

The full regression was not rerun after Milestone 7B.

## What Is Not Implemented Yet

Milestone 7 does not yet implement:

- core FSM
- broader RV32I system program suite
- private behavioral observable history
- rollback simulation
- cycle oracle
- pipeline behavior

## Next Step

The next Milestone 7 work is to broaden system tests now that the system shape is corrected.

Recommended next tests:

- reset behavior
- `x0` write ignored
- branch taken and not taken
- `JAL` and `JALR`
- signed and unsigned byte/halfword loads
- illegal instruction trap
- `ECALL` trap
- instruction-fetch fault
- data-memory alignment and range faults
