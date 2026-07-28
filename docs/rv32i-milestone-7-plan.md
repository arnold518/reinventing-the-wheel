# RV32I Milestone 7 Plan: Behavioral RV32I System Component

Last updated: 2026-07-19

## Status

Milestones 7A, 7B, and 7C are implemented: the reusable lockstep harness, composite behavioral system, and numbered program suite now form the behavioral answer-sheet baseline.

Implemented:

- reusable `RV32IInstructionLockstepTest`
- `RV32ISystemProgramCase`
- component-state adapter method
- data initialization helper records
- logical data-memory access comparison through `RV32IMemoryTrace`
- actual per-instruction data-memory write comparison through a byte-write dictionary
- `Simulator::advanceAndRecord(target_time)` for incremental simulation
- `SimulationTest::runSimulation()` hook for custom test loops
- `SimulatorAdvanceAndRecordTest`
- `RV32IInstructionLockstepHarnessTest`
- `RV32IReferenceCore`
- `RV32IReferenceSystem`
- two-memory `RV32IInstructionOracle::step` overload
- `RV32ISingleCycleSystemTest/program-01`
- `RV32ISingleCycleSystemTest/program-02` through `RV32ISingleCycleSystemTest/program-16`
- visualizer scenario aliases `rv32i-program1` through `rv32i-program16`
- separate oracle instruction and data memories matching the system's Harvard organization
- real simulated byte-write transaction comparison, including unchanged zero-byte writes
- `RV32IInstructionLockstepMismatchDetectionTest`
- `RV32IReferenceSystemContractTest` for reset, enable, public pins, Harvard separation, halt, and recovery
- precise taken branch/JAL/JALR instruction-address-misalignment behavior
- specification-derived oracle edge vectors
- hard-coded final PC/count/register/trap/write outcomes for all 16 numbered programs
- release-safe semantic test checks that do not disappear with `NDEBUG`
- the complete beginner guide at `docs/behavioral-rv32i-beginners-guide.md`

The Milestone 7C program suite is listed in `docs/rv32i-milestone-7c-program-tests.md`.

Latest focused 7A/7B/7C verification:

```bash
ctest --test-dir build --output-on-failure -R "Memory64Kx32Test|RV32IInstructionOracleTest|RV32IInstructionLockstep|RV32IReferenceSystem"
```

Results:

- Current hardened answer-sheet focus passed: 21/21 in 1.52 seconds.
- Release RV32I-related suite passed: 25/25 in 6.64 seconds.
- Current full repository regression passed: 109/109 in 62.02 seconds with four parallel test jobs.

The target is a reusable behavioral RV32I system component:

```text
RV32IReferenceSystem
  contains:
    RV32IReferenceCore
    Memory64Kx32 instruction memory
    Memory64Kx32 data memory

  public pins:
    CLK
    RST
    ENABLE
    PC
    HALTED
    TRAPPED
```

The system is the first circuit-simulator component that should run real RV32I programs. It is also the component shape we want to preserve when a structural core is built later.

## Role In Structural RV32I Work

Milestone 7 intentionally produced the executable answer sheet for the structural RV32I, not the final implementation architecture.

- `RV32IInstructionOracle` supplies expected one-instruction architectural transitions.
- `RV32IReferenceCore` and `RV32IReferenceSystem` expose those answers through the simulator-facing clock and memory contract.
- The numbered program cases, state adapter, and lockstep harness provide shared inputs and observable checkpoints for grading the structural system.
- The structural core must execute independently through visible datapath components; it must not call the oracle or behavioral core to obtain its results.
- The behavioral system stays available as a regression reference until the structural system passes the same program suite.

This preserves what the behavioral milestone was designed to provide: an answer sheet and stable boundary, without turning its hidden whole-instruction execution into the source of the structural design.

## Decisions

### Decision 1: The System Owns Memory

`RV32IReferenceSystem` owns:

- one behavioral core
- one instruction memory
- one data memory

The top-level system does not expose memory pins. Memory wiring is internal.

Reason:

- tests and visualizer users should interact with one reusable machine
- programs can be loaded once into the system
- the same system component can run many instruction/program tests
- memory internals can still be inspected by expanding the system or using helper getters

### Decision 2: Clock, Reset, And Enable Stay External

The system exposes:

- `CLK`
- `RST`
- `ENABLE`

Reason:

- tests control reset timing and clock edges directly
- the visualizer can show CPU stepping at clock edges
- the component stays reusable inside future testbenches

Reset policy:

- `RST` resets the core only
- `RST` must not clear instruction memory
- instruction memory contains the preloaded program
- data memory is cleared through helper methods when tests need that

### Decision 3: The Core Owns PC

`RV32IReferenceCore` owns:

- `pc`
- `x[32]`
- halt/trap state
- execution FSM state
- `instruction_count` inside `RV32IState`

The system does not decide instruction sequencing. The system only wires `core.IMEM_ADDR` to instruction memory and to the public `PC` output.

Reason:

- `PC` is architectural CPU state
- branch/jump/next-PC decisions belong to the CPU core
- future structural core can replace the behavioral core while preserving the same system surface

### Decision 4: Public Pins Stay Minimal

Public pins are only for real circuit interaction and essential status.

Do not expose every trace/debug field as a pin. Use getters for test/debug instrumentation.

Reason:

- fewer pins keep the visualizer readable
- debug pins would become noisy and brittle
- future structural implementation can keep the same public appearance

### Decision 5: Internal Core Memory Pins Are Still Needed

Even though the system does not expose memory pins, the core still needs an internal memory interface.

Reason:

- instruction fetch needs an address and instruction word
- loads/stores need address, data, size, read/write controls, sign-extension control, and fault feedback
- the future structural core should have the same core memory-interface contract

### Decision 6: Address Pins Stay 32 Bits

The current memory stores:

```text
64K words * 4 bytes = 256 KiB
valid byte addresses: 0x00000000 through 0x0003ffff
```

That capacity only needs 18 address bits, but RV32I addresses are architecturally 32-bit.

Therefore:

- core address pins are 32 bits
- memory accepts 32-bit addresses
- memory faults when an address is outside its implemented range

### Decision 7: Getters Are Instrumentation

Tests can use getters for internal behavioral state:

- registers
- trap cause
- `instruction_count` inside `RV32IState`
- memory contents

These getters are not hardware behavior and do not need visual pins.

For a future structural system, the same getters can be implemented by reading internal register-file components, memories, and wires.

## System Public Pins

`RV32IReferenceSystem` public appearance:

| Pin | Direction | Width | Meaning |
| --- | --- | ---: | --- |
| `CLK` | input | 1 | External rising-edge clock. |
| `RST` | input | 1 | Active-high core reset. Does not clear instruction memory. |
| `ENABLE` | input | 1 | Freezes core stepping when low. |
| `PC` | output | 32 | Current architectural PC. Driven from core instruction address. |
| `HALTED` | output | 1 | High after `EBREAK`. |
| `TRAPPED` | output | 1 | High after a trap. |

No public memory pins.

No public debug pins for:

- instruction word
- ALU result
- writeback register/data
- branch decision
- FSM state
- trap cause

Those are available through getters and internal inspection.

## Core Internal Pins

`RV32IReferenceCore` is internal to the system. Its logical address, data, request, fault, and status contract should guide the future structural core, while implementation-specific behavioral timing signals must not be copied automatically.

Inputs:

| Pin | Width | Meaning |
| --- | ---: | --- |
| `CLK` | 1 | Rising-edge clock. |
| `RST` | 1 | Active-high core reset. |
| `ENABLE` | 1 | Step enable. |
| `IMEM_READ_DATA` | 32 | Instruction word from instruction memory. |
| `IMEM_FAULT` | 1 | Instruction fetch fault from instruction memory. |
| `DMEM_READ_DATA` | 32 | Load result from data memory. |
| `DMEM_FAULT` | 1 | Data-memory fault from data memory. |

Outputs:

| Pin | Width | Meaning |
| --- | ---: | --- |
| `IMEM_ADDR` | 32 | Instruction byte address. This is the core PC. |
| `DMEM_ADDR` | 32 | Load/store byte address. |
| `DMEM_WRITE_DATA` | 32 | Store data. |
| `DMEM_READ_EN` | 1 | Load request. |
| `DMEM_WRITE_EN` | 1 | Store request. |
| `DMEM_SIZE` | 2 | Access size: `0=byte`, `1=halfword`, `2=word`. |
| `DMEM_SIGN_EXTEND` | 1 | Load sign-extension control. |
| `HALTED` | 1 | Halt status. |
| `TRAPPED` | 1 | Trap status. |

Dropped from the core pin contract:

- `IMEM_READ_EN`: instruction memory is wired as always-read.
- `IMEM_READY`: current memory is always ready.
- `DMEM_READY`: current memory is always ready.
- `PC`: duplicated by `IMEM_ADDR`.
- `TRAP_CAUSE`: getter only.
- `STATE`: getter/internal only if ever needed.
- writeback/ALU/branch debug pins: trace getter only.

## Memory Wiring

### Instruction Memory

Instruction memory is always read as a 32-bit word at the core PC.

```text
core.IMEM_ADDR      -> imem.ADDR
constant HIGH       -> imem.READ_EN
constant LOW        -> imem.WRITE_EN
constant WORD       -> imem.SIZE
constant LOW        -> imem.SIGN_EXTEND
constant LOW        -> imem.RST
imem.READ_DATA      -> core.IMEM_READ_DATA
imem.FAULT          -> core.IMEM_FAULT
core.IMEM_ADDR      -> system.PC
```

Instruction memory reset stays low so program preload is not erased.

### Data Memory

Data memory is driven by the core's load/store controls.

```text
core.DMEM_ADDR        -> dmem.ADDR
core.DMEM_WRITE_DATA  -> dmem.WRITE_DATA
core.DMEM_READ_EN     -> dmem.READ_EN
core.DMEM_WRITE_EN    -> dmem.WRITE_EN
core.DMEM_SIZE        -> dmem.SIZE
core.DMEM_SIGN_EXTEND -> dmem.SIGN_EXTEND
system.CLK            -> dmem.CLK
constant LOW          -> dmem.RST
dmem.READ_DATA        -> core.DMEM_READ_DATA
dmem.FAULT            -> core.DMEM_FAULT
```

Data memory is cleared directly by a system helper before tests that need a clean data region.

## Getter Contract

These getters are test/debug instrumentation, not circuit pins.

### System Getters

```cpp
std::shared_ptr<RV32IReferenceCore> getCore() const;
std::shared_ptr<Memory64Kx32> getInstructionMemory() const;
std::shared_ptr<Memory64Kx32> getDataMemory() const;

void loadProgram(const rv32i::RV32IProgram& program, uint32_t base_address = 0);
void clearDataMemory();
void loadDataBytes(uint32_t base_address, const std::vector<uint8_t>& data);
void loadDataWords(uint32_t base_address, const std::vector<uint32_t>& words);

uint32_t readRegister(uint8_t index) const;
uint32_t readInstructionWord(uint32_t address) const;
uint32_t readDataWord(uint32_t address) const;
std::vector<uint8_t> readDataBytes(uint32_t address, size_t count) const;

uint32_t getPC() const;
bool isHalted() const;
bool isTrapped() const;
rv32i::RV32IExecutionTrapCause trapCause() const;
rv32i::RV32IState snapshotState() const;
```

### Core Getters

```cpp
uint32_t readRegister(uint8_t index) const;
uint32_t getPC() const;
bool isHalted() const;
bool isTrapped() const;
rv32i::RV32IExecutionTrapCause trapCause() const;
rv32i::RV32IState snapshotState() const;
```

Do not add public getters for every transient private variable unless tests prove they are needed.

## Internal Inspection In Tests

Tests can also inspect component internals without public debug pins:

- `root->getChildren()`
- `component->getAllWires()`
- `io_component->getAllInputPins()`
- `io_component->getAllOutputPins()`
- `pin->getInternalWireBase()`
- `pin->getExternalWireBase()`

Use helper getters for architectural state. Use internal traversal only when a test specifically needs topology or wire-level evidence.

Important nuance:

The outer test `ComponentBuilder` only knows wires/components created through that builder. Internals built by a composite's own `buildInternals()` are still accessible through the component tree, but not necessarily through the outer builder's name registry.

## Timing Model

Use a conservative clocked FSM. The FSM is internal, not a public pin.

Recommended internal states:

| State | Meaning |
| --- | --- |
| `Reset` | Initialize `pc`, registers, status, and trace state. |
| `Fetch` | Drive instruction-memory address. |
| `Execute` | Decode and execute ALU/branch/jump/control behavior. |
| `LoadWait` | Wait one cycle for data-memory read result, then write back. |
| `StoreWait` | Hold store controls stable for the memory commit edge. |
| `Halted` | Stop after `EBREAK`. |
| `Trapped` | Stop after illegal instruction or memory/fetch fault. |

First implementation policy:

- one instruction progresses through visible simulator time
- ALU, branch, jump, `FENCE`, `ECALL`, and `EBREAK` do not need data-memory wait
- loads use `LoadWait`
- stores use `StoreWait`
- `ENABLE=LOW` freezes stepping

## Relationship To Milestone 6

Milestone 6 remains the source of expected architectural outcomes.

Milestone 7 tests should compare against `RV32IInstructionOracle` in instruction lockstep:

```text
same RV32IProgram
same initial pc/register/data memory setup
  -> clock RV32IReferenceSystem until snapshotState().instruction_count advances
  -> call RV32IInstructionOracle::step once
  -> compare architectural state
  -> compare logical data-memory access
  -> compare actual data-memory writes for that instruction
  -> repeat until halt, trap, or instruction limit
```

This is not final-state-only testing. It checks every committed instruction.

This is also not cycle-by-cycle testing. Milestone 7 does not need a separate cycle oracle because component internals may later become single-cycle, multi-cycle, or pipelined. The instruction oracle remains reusable because it checks architectural commit behavior.

## Milestone 7A: Instruction-Lockstep Test Base

Milestone 7A adds a reusable test base before adding many program-specific system tests. It does not implement the CPU. It defines how future RV32I components prove that each committed instruction matches the instruction oracle.

The base class shape:

```cpp
class RV32IInstructionLockstepTest : public SimulationTest {
protected:
    virtual RV32ISystemProgramCase getCase() const = 0;
    virtual void initializeComponentForLockstep(const RV32ISystemProgramCase& test_case) = 0;
    virtual void clockComponentOneCycle(size_t cycle_index, size_t cycle_start_time) = 0;
    virtual rv32i::RV32IState snapshotComponentState() const = 0;
    virtual rv32i::RV32IMemoryTrace lastDataMemoryAccess() const = 0;
    virtual std::map<uint32_t, uint8_t> lastDataMemoryWrites() const = 0;
};
```

The case describes program inputs, not hand-written answers:

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
};
```

The base test owns the comparison loop:

```text
setup component system
setup RV32IInstructionOracle state and memory
load the same program/data into both
for each expected instruction:
  clock component until snapshotComponentState().instruction_count advances
  run RV32IInstructionOracle::step once
  compare component RV32IState against oracle RV32IState
  compare component lastDataMemoryAccess() against oracle trace.memory
  derive expected byte writes from oracle trace.memory
  compare expected byte writes against component lastDataMemoryWrites()
  stop when both halt or both trap
```

`RV32IState` comparison fields:

- `pc`
- all `x[0..31]` registers
- `halted`
- `trapped`
- `trap_cause`
- committed instruction count through `RV32IState::instruction_count`

Logical data-memory access comparison fields:

- access kind: `None`, `Read`, or `Write`
- access size: byte, halfword, or word
- sign-extension flag for loads
- byte address
- read data for loads
- write data for stores
- fault flag

Actual data-memory write comparison:

- the oracle derives a byte-write dictionary from successful `SB`, `SH`, and `SW` traces
- the component/system exposes a byte-write dictionary for the last committed instruction
- the dictionary means "bytes written this instruction", not "bytes changed"
- non-store instructions, loads, and faulted stores must produce an empty dictionary

The test base should fail if:

- the component commits when the oracle would already be halted/trapped
- the component fails to commit within `max_cycles_per_instruction`
- the component commits more than one instruction before the test can sample it
- the component and oracle disagree on state, data-memory access, data-memory writes, halt, trap, or trap cause

Manual memory probes are intentionally not part of the final 7A design. They are easy to forget and only check selected ranges. Per-instruction memory access plus a write dictionary gives a stronger and less manual contract.

## Milestone 7A: Simulator Scope

The simulator work for 7A is intentionally small and generic.

`Simulator::advanceAndRecord(target_time)` advances the existing event queue forward to an absolute target time:

```text
continue from current_time
process events with time <= target_time
keep already-recorded wire history
leave future events in the queue
do not reset the circuit
do not clear history
```

`SimulationTest::runSimulation()` is a virtual hook. The default behavior remains:

```cpp
virtual void runSimulation() {
    sim->runAndRecord(getRunDuration());
}
```

Normal tests and the visualizer keep the old full-run behavior. RV32I lockstep tests override `runSimulation()` so they can advance the component one chunk at a time and compare after each committed instruction.

The simulator does not gain RV32I-specific behavior in 7A:

- no oracle logic
- no memory dictionary
- no private component history
- no observable timeline
- no rollback

Visualizer behavior is unchanged in 7A. It still displays wire and pin history. Private behavioral component data should not be exposed historically until a later, explicit observable-history design is added.

## Future Structural Compatibility

The future structural implementation should preserve the same system public pins:

```text
CLK
RST
ENABLE
PC
HALTED
TRAPPED
```

The future structural core should preserve the same logical memory-interface pins:

```text
IMEM_ADDR
IMEM_READ_DATA
IMEM_FAULT
DMEM_ADDR
DMEM_WRITE_DATA
DMEM_READ_DATA
DMEM_READ_EN
DMEM_WRITE_EN
DMEM_SIZE
DMEM_SIGN_EXTEND
DMEM_FAULT
```

System getters should also remain available. In the behavioral system, getters read private state. In the structural system, getters can read internal register-file components, memory components, and sampled wires.

This keeps tests and visualizer expectations stable while implementation detail changes.

Only the logical transaction contract is normative. The behavioral core's internal `IMEM_CLK` and `DMEM_CLK` pulse workaround is not a structural template; the structural system should use the system clock and allow its visible combinational datapath to settle before the active edge.

## Tests

Recommended Milestone 7 tests:

- reset behavior
- `ADDI`/`ADD` short program
- `x0` write ignored
- load/store word program
- byte and halfword load/store program
- branch taken and not taken
- simple loop counter
- `JAL`
- `JALR`
- `FENCE`
- `EBREAK` halt
- illegal instruction trap
- `ECALL` trap
- data-memory fault propagation
- instruction-fetch fault propagation

Test style:

- create `RV32IReferenceSystem`
- preload instruction memory through `loadProgram`
- optionally initialize data memory through helper methods
- drive external `CLK`, `RST`, and `ENABLE`
- run until `snapshotState().instruction_count` advances, `HALTED`, `TRAPPED`, or max cycle count
- call `RV32IInstructionOracle::step` once per committed component instruction
- compare state after every committed instruction
- compare `lastDataMemoryAccess()` with oracle `RV32IInstructionTrace::memory`
- compare `lastDataMemoryWrites()` with byte writes derived from the oracle memory trace

## Visualizer

Expose one scenario:

```text
rv32i-program1
```

Default visual structure:

```text
RV32I_SYSTEM
  CORE
  IMEM
  DMEM
```

Visualizer policy:

- show the system at block level
- keep public pins compact
- allow expanding internals when inspecting memory communication
- keep memories compact

## Done When

Milestone 7 is complete when:

- `RV32IReferenceCore` is implemented.
- `RV32IReferenceSystem` is implemented.
- the system owns instruction and data memory.
- public system pins are limited to `CLK`, `RST`, `ENABLE`, `PC`, `HALTED`, and `TRAPPED`.
- instruction memory can be preloaded without being reset away.
- reusable `RV32IInstructionLockstepTest` exists.
- tests execute multiple raw RV32I programs through the system component.
- instruction-lockstep tests match `RV32IInstructionOracle` after every committed instruction.
- visualizer exposes the complete `rv32i-program1` through `rv32i-program16` family.
- docs are updated with behavior, timing, and known limits.

## Known Limits

This milestone does not implement:

- paired structural/behavioral control-flow components
- paired structural/behavioral instruction decoder/control components
- paired structural/behavioral execution-control/status components
- behavioral fidelity for `ALU32` and explicit ALU/register-file equivalence tests
- the structural core's direct data-memory wiring
- cycle oracle
- cycle-by-cycle internal timing comparator
- ELF loading
- assembler workflow
- pipeline behavior
- caches

Those come after the behavioral system is stable.
