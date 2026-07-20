# Behavioral RV32I: A Beginner's Guide

Last updated: 2026-07-19

## 1. What This Machine Is

RV32I is the 32-bit base integer RISC-V instruction set. "RV32" means that the architectural registers and addresses are 32 bits wide. "I" means the base integer instruction set: integer arithmetic, loads and stores, branches, jumps, upper-immediate instructions, and a small system-instruction surface.

This repository contains two different ways to model that machine:

- The **behavioral RV32I** computes the result of a whole instruction in C++. It is compact, easy to test, and fast enough to run small programs.
- The **structural RV32I** is the learning destination. It must build the same behavior from visible hardware structures such as a PC register, register file, decoder, immediate generator, ALU, muxes, and control logic.

The behavioral implementation is therefore an **executable answer sheet**. It tells structural tests what the architectural result should be after each instruction. It is not a structural template, and the structural core must never call it to obtain an answer.

The answer sheet has three layers:

```text
RV32IInstructionOracle
    Pure one-instruction architectural semantics

BehavioralRV32ICore
    Clocked simulator component that applies the oracle
    and publishes instruction/data-memory transactions

RV32ISystem
    Top-level component containing the core plus separate
    instruction and data memories
```

The main implementation files are:

- `src/rv32i/RV32IInstructionOracle.cpp`
- `src/modules/rv32i/BehavioralRV32ICore.cpp`
- `src/modules/rv32i/RV32ISystem.cpp`
- `src/tests/RV32IInstructionOracleTests.cpp`
- `src/tests/RV32IInstructionLockstepTests.cpp`
- `src/tests/RV32ISystemTests.cpp`

### Oracle inputs and outputs

The preferred pure semantic call is:

```cpp
RV32IInstructionTrace RV32IInstructionOracle::step(
    RV32IState& state,
    RV32IFunctionalMemory& instruction_memory,
    RV32IFunctionalMemory& data_memory);
```

Its inputs are the state **before** an instruction and separate instruction/data memories. It mutates `state` to the architectural state **after** the instruction attempt, mutates data memory for a valid store, and returns a detailed trace describing what it decoded, read, calculated, accessed, wrote, and decided for the PC.

Other forms have narrower uses:

| Call | Use |
| --- | --- |
| `step(state, memory)` | Legacy unified-memory convenience overload for small oracle unit tests. System/structural grading should use separate memories. |
| `step(state, instruction_memory, data_memory)` | Preferred Harvard semantic step. |
| `stepWithoutDataMemoryWrite(...)` | Behavioral-core adapter: calculate a store but let the simulated memory bus perform it. |
| `run(state, memory, max_instructions)` | Standalone convenience loop returning final state and all traces; traps with the harness-only `InstructionLimit` condition if needed. |

## 2. The Complete System at a Glance

`RV32ISystem` is a composite circuit:

```text
                         RV32ISystem

       CLK ────────────────┐
       RST ────────────────┤
    ENABLE ────────────────┤
                           v
                    BehavioralRV32ICore
                    │                 │
          instruction bus             data bus
                    │                 │
                    v                 v
       BehavioralMemory64Kx32   BehavioralMemory64Kx32
          INSTRUCTION_MEMORY        DATA_MEMORY

       PC <────────────────── core architectural PC
   HALTED <────────────────── core halt state
  TRAPPED <────────────────── core trap state
```

It is a Harvard-style machine: instruction memory and data memory are separate objects. A byte stored to data address `0x00000000` does not overwrite the instruction at instruction address `0x00000000`.

Each memory contains 64 Ki 32-bit words:

```text
65,536 words × 4 bytes = 262,144 bytes = 256 KiB
valid byte addresses: 0x00000000 through 0x0003ffff
first invalid address: 0x00040000
```

Multi-byte values are little-endian. For example, storing `0x87654321` at address `0x100` produces:

```text
address  0x100  0x101  0x102  0x103
byte       21     43     65     87
```

## 3. Public `RV32ISystem` Inputs and Outputs

The public interface is intentionally small so that a testbench or visualizer sees a CPU-shaped component rather than every internal debug value.

| Pin | Direction | Width | Meaning |
| --- | --- | ---: | --- |
| `CLK` | input | 1 | Clock. An instruction step begins on a known low-to-high edge. |
| `RST` | input | 1 | Active-high core reset. Reset is handled whenever the core evaluates; it does not wait for a rising clock edge. |
| `ENABLE` | input | 1 | Clock-enable. A rising edge commits a step only when this is `HIGH`. |
| `PC` | output | 32 | Current architectural program counter. |
| `HALTED` | output | 1 | `HIGH` after this project executes `EBREAK`. |
| `TRAPPED` | output | 1 | `HIGH` after an architectural fault or exception. |

Important rules:

- `RST=HIGH` has priority over stepping.
- `ENABLE=LOW` holds the PC, registers, halt state, and trap state even if `CLK` rises.
- An unknown clock or enable value does not count as an enabled rising edge.
- Reset clears the core state but does **not** erase either memory. Tests clear or reload memory explicitly.
- Once `HALTED` or `TRAPPED` is high, later clocks do not execute more instructions. Reset is required to run again.

The public outputs are circuit pins, so they can be connected to wires and inspected in simulator history. `snapshotState()` is the richer debug/test view.

## 4. System Helper Methods

Tests normally configure a system through these methods before driving its pins.

| Method | Purpose |
| --- | --- |
| `setInitialPC(pc)` | Sets both the reset PC and current PC. |
| `setRegister(index, value)` | Initializes one architectural register; writes to `x0` are ignored. |
| `resetCore()` | Direct helper reset for setup/debug code. The `RST` pin remains the circuit-level reset. |
| `clearInstructionMemory()` | Zeros instruction memory and clears its recorded history. |
| `clearDataMemory()` | Zeros data memory and clears its recorded history. |
| `loadProgram(program, base)` | Loads encoded 32-bit instructions into instruction memory. |
| `loadInstructionBytes(base, bytes)` | Loads raw bytes into instruction memory. |
| `loadDataBytes(base, bytes)` | Loads raw initial data into data memory. |
| `loadDataWords(base, words)` | Loads aligned little-endian 32-bit data words. |
| `snapshotState()` | Returns PC, all registers, halt/trap state, cause, and instruction count. |
| `lastDataMemoryAccess()` | Returns the last instruction's logical read/write description. |
| `lastDataMemoryWrites()` | Expands the last intended store into an address-to-byte dictionary. |

The memories can also be obtained with `instructionMemory()` and `dataMemory()` for inspection. Those direct methods are test/setup conveniences, not CPU instructions.

## 5. Architectural State

`RV32IState` is the programmer-visible result of execution:

| Field | Meaning |
| --- | --- |
| `pc` | Address of the current/next instruction, depending on whether the previous step completed. |
| `x[0..31]` | Thirty-two 32-bit integer registers. |
| `halted` | Project halt latch, set by `EBREAK`. |
| `trapped` | Trap latch, set by a fault or exception. |
| `trap_cause` | Why execution trapped. |
| `instruction_count` | Number of instruction attempts, including an instruction that halts or traps. |

Register `x0` is special. Reading it always returns zero and writing it has no effect. The implementation calls `forceX0()` defensively around every step so that even a bad setup cannot make it nonzero.

The other registers have no built-in ABI meaning in the simulator. RISC-V software conventions may call `x1` `ra`, `x2` `sp`, and `x10` `a0`, but the hardware itself only sees register numbers.

All arithmetic is 32-bit. Addition and subtraction wrap modulo `2^32`. For example:

```text
0xffffffff + 1 = 0x00000000
```

## 6. What Happens on One Clock Step

One enabled rising edge represents one architectural instruction attempt:

1. The core confirms a `LOW -> HIGH` clock transition and `ENABLE=HIGH`.
2. If reset is active, it resets instead of executing.
3. If already halted or trapped, it leaves the state unchanged.
4. It checks the current PC for 4-byte alignment and instruction-memory range.
5. It fetches one 32-bit little-endian instruction from instruction memory.
6. It decodes the instruction and creates control signals.
7. It reads `rs1` and `rs2`, forms the immediate, and selects ALU inputs.
8. It computes the ALU result or comparison.
9. If required, it checks and performs a data-memory read or describes a store.
10. It calculates the next PC, including branch or jump behavior.
11. It checks a taken branch/jump target for 4-byte alignment.
12. If no trap occurred, it performs register writeback and commits the next PC.
13. It publishes state and memory-bus outputs through simulator wires.
14. A valid store is performed by data memory from the simulated write bus.

This is a **single-step architectural model**, not a pipelined processor. Fetch, decode, execute, memory, and writeback are useful logical stages, but the behavioral core does not contain separate stage registers or take five clock cycles.

The implementation deliberately uses `stepWithoutDataMemoryWrite()`: the oracle calculates the store transaction but does not secretly modify the component memory. The real `BehavioralMemory64Kx32` receives the core's address, data, size, enable, and clock outputs and performs the write. Tests inspect that real bus-write history.

## 7. Internal Core Pins

You normally use the six public system pins. The internal pins explain the contract that the future structural core must eventually satisfy.

### Core control and state pins

| Pin | Direction | Width | Meaning |
| --- | --- | ---: | --- |
| `CLK` | input | 1 | Core clock. |
| `RST` | input | 1 | Core reset. |
| `ENABLE` | input | 1 | Instruction-step enable. |
| `PC` | output | 32 | Current architectural PC. |
| `HALTED` | output | 1 | Halt status. |
| `TRAPPED` | output | 1 | Trap status. |

### Instruction-memory interface

| Pin | Direction | Width | Meaning |
| --- | --- | ---: | --- |
| `IMEM_ADDR` | output | 32 | Address of the attempted instruction fetch. |
| `IMEM_WRITE_DATA` | output | 32 | Always zero; this core never writes instruction memory. |
| `IMEM_READ_EN` | output | 1 | High for an active fetch. |
| `IMEM_WRITE_EN` | output | 1 | Always low. |
| `IMEM_SIZE` | output | 2 | `2`, meaning a 4-byte word fetch. |
| `IMEM_SIGN_EXTEND` | output | 1 | Low; instruction words are not sign-extended loads. |
| `IMEM_CLK` | output | 1 | Low; instruction memory is read combinationally. |
| `IMEM_RST` | output | 1 | Low; core reset does not clear program memory. |
| `IMEM_READ_DATA` | input | 32 | Mirrored memory read result. |
| `IMEM_READY` | input | 1 | Mirrored memory ready status. |
| `IMEM_FAULT` | input | 1 | Mirrored memory fault status. |

### Data-memory interface

| Pin | Direction | Width | Meaning |
| --- | --- | ---: | --- |
| `DMEM_ADDR` | output | 32 | Effective load/store byte address. |
| `DMEM_WRITE_DATA` | output | 32 | Source register value for a store. Low bytes are used for `SB`/`SH`. |
| `DMEM_READ_EN` | output | 1 | High for a valid load transaction. |
| `DMEM_WRITE_EN` | output | 1 | High for a valid store transaction. |
| `DMEM_SIZE` | output | 2 | `0`=byte, `1`=halfword, `2`=word. |
| `DMEM_SIGN_EXTEND` | output | 1 | High for `LB`/`LH`; low for `LBU`/`LHU` and stores. |
| `DMEM_CLK` | output | 1 | Pulses high to commit a store on the memory's rising edge. |
| `DMEM_RST` | output | 1 | Low; core reset does not clear data memory. |
| `DMEM_READ_DATA` | input | 32 | Mirrored memory load result. |
| `DMEM_READY` | input | 1 | Mirrored memory ready status. |
| `DMEM_FAULT` | input | 1 | Mirrored memory fault status. |

### An important behavioral-adapter detail

The current behavioral core reads the two attached memory objects directly while computing its architectural answer. It does not use `IMEM_READ_DATA`, `DMEM_READ_DATA`, `READY`, or `FAULT` as the semantic source of that answer. Those return pins are wired and visible for component compatibility and visualization.

This is acceptable for the answer-sheet adapter, but it is **not** how the structural core may work. The structural implementation must obtain instructions, load data, readiness, and faults through its explicit datapath/interface and must execute independently of the oracle.

## 8. Memory Component Inputs and Outputs

Both internal memories use the same `BehavioralMemory64Kx32` contract.

| Pin | Direction | Width | Meaning |
| --- | --- | ---: | --- |
| `ADDR` | input | 32 | Byte address. |
| `WRITE_DATA` | input | 32 | Data to write. |
| `READ_EN` | input | 1 | Requests a read. |
| `WRITE_EN` | input | 1 | Requests a write. |
| `SIZE` | input | 2 | `0` byte, `1` halfword, `2` word; `3` is invalid. |
| `SIGN_EXTEND` | input | 1 | Extends byte/halfword read sign into 32 bits when high. |
| `CLK` | input | 1 | A valid write commits on a low-to-high edge. |
| `RST` | input | 1 | High clears memory; unknown reset makes contents unknown. The RV32I core holds this low. |
| `READ_DATA` | output | 32 | Current read result. |
| `READY` | output | 1 | Memory response status. |
| `FAULT` | output | 1 | Invalid, misaligned, out-of-range, or unknown request indication. |

Byte accesses may use any address. Halfword accesses must be divisible by 2. Word accesses must be divisible by 4. A range is invalid if any requested byte falls beyond `0x0003ffff`.

## 9. Supported Instructions

### Register-register arithmetic (`opcode 0x33`)

These instructions read `rs1` and `rs2` and normally write `rd`.

| Instruction | Result written to `rd` |
| --- | --- |
| `ADD` | `rs1 + rs2`, low 32 bits |
| `SUB` | `rs1 - rs2`, low 32 bits |
| `SLL` | `rs1 << rs2[4:0]` |
| `SLT` | 1 if signed `rs1 < rs2`, otherwise 0 |
| `SLTU` | 1 if unsigned `rs1 < rs2`, otherwise 0 |
| `XOR` | bitwise exclusive OR |
| `SRL` | logical right shift; zeros enter the top |
| `SRA` | arithmetic right shift; the sign bit is copied |
| `OR` | bitwise OR |
| `AND` | bitwise AND |

Only the low five bits of a register shift amount are used, because a 32-bit value has shift positions 0 through 31.

### Immediate arithmetic (`opcode 0x13`)

| Instruction | Result written to `rd` |
| --- | --- |
| `ADDI` | `rs1 + sign_extend(imm12)` |
| `SLTI` | signed comparison with sign-extended immediate |
| `SLTIU` | unsigned comparison with the sign-extended immediate reinterpreted as unsigned |
| `XORI` | bitwise XOR with sign-extended immediate |
| `ORI` | bitwise OR with sign-extended immediate |
| `ANDI` | bitwise AND with sign-extended immediate |
| `SLLI` | logical left shift by the encoded five-bit `shamt` |
| `SRLI` | logical right shift by `shamt` |
| `SRAI` | arithmetic right shift by `shamt` |

The ordinary I-type immediate is 12-bit signed, from `-2048` through `+2047`.

### Loads

The effective address is `rs1 + sign_extend(imm12)`.

| Instruction | Bytes read | Register result |
| --- | ---: | --- |
| `LB` | 1 | Sign-extended 8-bit value |
| `LH` | 2 | Sign-extended 16-bit value |
| `LW` | 4 | Full 32-bit value |
| `LBU` | 1 | Zero-extended 8-bit value |
| `LHU` | 2 | Zero-extended 16-bit value |

Alignment and range are checked before reading or writing `rd`. A load to `x0` still performs its memory checks and can trap; discarding the result does not cancel the access.

### Stores

The effective address is `rs1 + sign_extend(S-immediate)`. The value comes from `rs2`.

| Instruction | Bytes written | Source bits |
| --- | ---: | --- |
| `SB` | 1 | `rs2[7:0]` |
| `SH` | 2 | `rs2[15:0]` |
| `SW` | 4 | `rs2[31:0]` |

Faulting stores produce no byte writes. Tests check physical bus transactions rather than inferring them from the final memory value.

### Conditional branches

Branches never write a register. If the condition is true, the next PC is `pc_before + B-immediate`; otherwise it is `pc_before + 4`.

| Instruction | Branch condition |
| --- | --- |
| `BEQ` | `rs1 == rs2` |
| `BNE` | `rs1 != rs2` |
| `BLT` | signed `rs1 < rs2` |
| `BGE` | signed `rs1 >= rs2` |
| `BLTU` | unsigned `rs1 < rs2` |
| `BGEU` | unsigned `rs1 >= rs2` |

Signed and unsigned comparisons can disagree. For example, `0xffffffff` is `-1` signed but `4,294,967,295` unsigned.

### Jumps and upper-immediate instructions

| Instruction | Architectural effect |
| --- | --- |
| `JAL` | Writes `pc_before + 4` to `rd`, then jumps to `pc_before + J-immediate`. |
| `JALR` | Writes `pc_before + 4` to `rd`, then jumps to `(old_rs1 + I-immediate) & ~1`. |
| `LUI` | Writes the encoded upper 20 bits followed by 12 zeros. |
| `AUIPC` | Writes `pc_before + upper_immediate`. |

`JAL x0, target` is an unconditional PC-relative jump with the link discarded. `JALR x0, 0(x1)` is a common return-like indirect jump. If `rd` and `rs1` are the same register, the target uses the **old** `rs1` value before link writeback.

### Fence and system instructions

| Instruction | This project's behavior |
| --- | --- |
| `FENCE` | Legal no-op in this simple memory model; PC advances by 4. |
| `ECALL` | Traps with `EnvironmentCall`. |
| `EBREAK` | Sets `HALTED`. This is a test-program sentinel. |

Real RISC-V hardware normally reports `EBREAK` as a breakpoint exception. This project intentionally treats it as a convenient successful-program halt. Do not confuse that project policy with the ISA's normal privileged/debug handling.

Unsupported or malformed encodings trap as `IllegalInstruction`. Compressed 16-bit instructions and optional extensions are not implemented by this RV32I answer sheet.

## 10. Jumps, Links, and Precise Target Traps

Suppose a `JAL x5, +16` is at PC `0x100`:

```text
link written to x5 = 0x104
new PC             = 0x110
```

Suppose `x6=0x201` and `JALR x5, 4(x6)` is at PC `0x100`:

```text
sum before masking = 0x205
bit 0 cleared      = 0x204
link written to x5 = 0x104
new PC             = 0x204
```

RV32I in this project has `IALIGN=32`, so every instruction address must be divisible by 4. Clearing JALR bit 0 is not enough to guarantee this. A target ending in binary `10` is still misaligned.

For a **taken** branch, `JAL`, or `JALR` with a non-word-aligned target:

- `InstructionAddressMisaligned` is raised on the control-transfer instruction itself.
- The PC remains at the faulting instruction.
- A jump link register is not written.
- The target is not fetched.

A non-taken conditional branch does not trap merely because its encoded target would be misaligned; it falls through to `PC+4`.

This precise timing is important. An aligned but out-of-range jump is different: the jump itself commits, and the following fetch traps with `InstructionAccessFault`.

## 11. Halt and Trap Behavior

A trap is a stopped architectural state with a reason. The current model does not implement machine-mode trap-vector CSRs or a trap handler, so it latches `TRAPPED` and waits for reset.

| Cause | When it occurs |
| --- | --- |
| `IllegalInstruction` | Decoder/control rejects the fetched word. |
| `EnvironmentCall` | `ECALL` executes. |
| `InstructionAddressMisaligned` | Current fetch PC is not word-aligned, or a taken branch/jump calculates a non-word-aligned target. |
| `InstructionAccessFault` | Four instruction bytes are outside instruction memory. |
| `LoadAddressMisaligned` | Halfword address is odd or word address is not divisible by 4. |
| `LoadAccessFault` | The requested load bytes are outside data memory. |
| `StoreAddressMisaligned` | Halfword address is odd or word address is not divisible by 4. |
| `StoreAccessFault` | The requested store bytes are outside data memory. |
| `InstructionLimit` | The standalone oracle `run()` reaches its safety limit without halt/trap. |

`InstructionLimit` is a harness safety condition, not an instruction-generated architectural exception.

On a trap:

- `trapped=true` and the cause are latched.
- `halted` remains false unless it was already true.
- The PC identifies the faulting instruction, except that an already-committed aligned jump followed by a bad fetch leaves PC at the bad fetch address.
- Faulting loads do not write `rd`.
- Faulting stores write no bytes.
- Misaligned jumps do not write their link register.
- Later clocks do nothing until reset.

`EBREAK` is a halt, not a trap in this project:

```text
EBREAK result: halted=true, trapped=false, PC remains on EBREAK
```

## 12. Instruction Trace: How to Read an Answer

Each oracle step returns an `RV32IInstructionTrace`. It explains both the inputs to the instruction and the resulting effects.

| Trace group | Important fields |
| --- | --- |
| Identity | instruction index, `pc_before`, raw instruction, decoded instruction |
| Register inputs | `rs1`, `rs2`, `rd`, `rs1_value`, `rs2_value` |
| Immediate/control | decoded immediate and generated control signals |
| ALU | `alu_a`, `alu_b`, `alu_result` |
| Memory | kind, size, sign extension, address, write/read data, fault |
| Writeback | enabled, destination, value, ignored-`x0` flag |
| Control flow | branch-taken flag and `pc_after` |
| Stop state | halted, trapped, and trap cause |

For debugging a structural mismatch, read the trace in that order. First verify fetch/decode, then operands/immediate, then ALU or comparison, then memory, then writeback and next PC. That narrows a whole-CPU failure to one contract boundary.

## 13. How Lockstep Grading Works

The lockstep harness runs two independent executions from the same program and initial state:

```text
functional oracle                  component under test
separate instruction memory        RV32ISystem instruction memory
separate data memory               RV32ISystem data memory
          │                                  │
          └──────── compare after each instruction ────────┘
```

It compares:

- PC;
- all 32 registers;
- halt and trap flags;
- trap cause;
- instruction count;
- logical data-memory access kind, address, size, data, sign-extension, and fault;
- the exact address/value dictionary of bytes written by the oracle;
- actual byte-write transactions recorded by the simulated component memory.

Instruction and data memory are separate on both sides. This catches accidental unified-memory behavior. Real memory transaction history catches omitted stores, incorrect byte lanes, and zero-valued writes that a before/after value comparison would miss.

There is also an intentional-mismatch test. Its fake component corrupts `x31`; the outer test succeeds only if the lockstep harness rejects that component. This proves that a green suite is not merely the result of a comparison path that never fails.

Each numbered system program also has a hard-coded final result independent of the per-instruction oracle run: final PC, instruction count, selected registers, halt/trap state and cause, and the exact aggregate memory-bus byte writes. These checks keep the behavioral adapter from passing only because both sides called the same instruction-step function.

The oracle has specification-derived edge vectors for wraparound, signed immediates, shift masking, `rd==rs1` JALR behavior, load-to-`x0` faults, backward branches, and precise misaligned control transfers. The repository does not currently have Spike, Sail, or a RISC-V architectural-test toolchain installed, so external differential validation remains a valuable future strengthening step.

## 14. The 16 Behavioral System Programs

Programs 1 through 8 are successful programs that stop at the project's `EBREAK` sentinel. Programs 9 through 16 intentionally trap. Every instruction checkpoint is compared to the independent oracle; the descriptions below highlight what a beginner should look for.

### Program 1: Sum a word array and use three store widths

Input data at `0x100` is four little-endian words: `3, 5, 7, 11`.

The program uses `LW` to read each word, `ADD` to accumulate it, pointer/count `ADDI` instructions, and a backward `BNE` loop. The sum is `26` (`0x1a`). It then demonstrates:

- `SW` writes `0x0000001a` at `0x120`;
- `SB` writes byte `0x1a` at `0x124`;
- `ADDI x6, x3, 0x34` produces `78` (`0x4e`);
- `SH` writes little-endian bytes `4e 00` at `0x126`.

This is the first whole-program test of fetch, looping, loads, arithmetic, and mixed-width stores.

### Program 2: Arithmetic, logic, shifts, comparisons, and `x0`

Inputs are `a=0x55`, `b=0x0f`, and `neg=-8` (`0xfffffff8`). Fourteen results are stored from `0x200` onward:

| Offset | Operation | Expected word |
| ---: | --- | ---: |
| `0` | `a+b` | `0x00000064` |
| `4` | `a-b` | `0x00000046` |
| `8` | `a&b` | `0x00000005` |
| `12` | `a\|b` | `0x0000005f` |
| `16` | `a^b` | `0x0000005a` |
| `20` | `b<<4` | `0x000000f0` |
| `24` | previous result `>>2` logically | `0x0000003c` |
| `28` | `-8>>1` arithmetically | `0xfffffffc` |
| `32` | signed `-8 < 0x55` | `1` |
| `36` | unsigned `0xfffffff8 < 0x55` | `0` |
| `40` | `(a^0x33)\|0x100` | `0x00000166` |
| `44` | `a&0x3f` | `0x00000015` |
| `48` | signed `-8 < 0` | `1` |
| `52` | unsigned `0xfffffff8 < 1` | `0` |

It also executes `ADDI x0, x0, 123`; `x0` must remain zero.

### Program 3: Signed and unsigned loads

Initial bytes are:

```text
0x100: 80 7f 00 00 80 ff 00 00 21 43 65 87
```

The important register/load results are:

| Load | Expected value |
| --- | ---: |
| `LB 0x100` | `0xffffff80` |
| `LBU 0x100` | `0x00000080` |
| `LB 0x101` | `0x0000007f` |
| `LH 0x104` | `0xffffff80` because little-endian halfword is `0xff80` |
| `LHU 0x104` | `0x0000ff80` |
| `LW 0x108` | `0x87654321` |

The program stores these six 32-bit results at `0x120..0x137`, then uses `SB` at `0x138` and `SH` at `0x13a`. It proves both extension modes and little-endian lane selection.

### Program 4: Every RV32I branch condition

The program awards one point for each correct control-flow decision:

- equal values with `BEQ`;
- unequal values with `BNE`;
- signed less-than with `BLT`;
- signed greater-or-equal with `BGE`;
- unsigned less-than with `BLTU`;
- unsigned greater-or-equal with `BGEU`;
- a deliberately non-taken `BEQ` fallthrough.

Wrong paths add 100, making an incorrect branch obvious. The correct score is `7`, stored at `0x120`. A `JAL x0` skips the wrong-path block before the final store.

### Program 5: Nested calls with `JAL` and returns with `JALR`

The main code calls `helper_a` using `JAL x5`; `helper_a` calls `helper_b` using `JAL x6`. `helper_b` adds 35 to 7, returns through `x6`, and `helper_a` adds 1 before returning through `x5`.

Key values are:

```text
x5 link from main call     = 0x00000008
x6 link from nested call   = 0x00000018
returned result in x10     = 43 (0x2b)
word stored at 0x120       = 43
```

It also checks event timing: the data-memory word must become visible only when the delayed simulated write bus commits it.

### Program 6: `LUI`, `AUIPC`, and `FENCE`

`LUI` plus `ADDI` constructs `0x12345678`. Two `AUIPC` instructions demonstrate that the input is the PC of the `AUIPC` itself:

```text
AUIPC at PC 0x00000008 with immediate 0       -> 0x00000008
AUIPC at PC 0x0000000c with immediate 0x1000  -> 0x0000100c
```

The three values are stored at `0x120`, `0x124`, and `0x128`. `FENCE` is verified as a legal no-op in this simple, non-cached memory model.

### Program 7: Fibonacci loop

Starting with `a=0` and `b=1`, eight iterations store:

```text
address: 0x120 0x124 0x128 0x12c 0x130 0x134 0x138 0x13c
value:       0     1     1     2     3     5     8    13
```

After the eighth update, the live pair is `a=21`, `b=34`. This longer loop stresses repeated word stores, backward branches, pointer increments, and register dependencies.

### Program 8: Byte copy and unsigned checksum

Input bytes at `0x100` are `01 02 03 04 ff`. `LBU` is essential: `0xff` contributes 255, not signed `-1`.

The program copies the five bytes to `0x140..0x144` with `SB`, then stores the sum at `0x160`:

```text
1 + 2 + 3 + 4 + 255 = 265 = 0x00000109
```

This checks unaligned byte addresses, repeated byte writes, unsigned extension, and a final word store.

### Program 9: Illegal instruction

`ADDI` first sets `x1=1`. The next word is `0xffffffff`, which is not a legal implemented RV32I instruction.

Expected result:

```text
PC = 0x00000004
x1 = 1
x2 = 0                    (the following ADDI never executes)
trapped = true
cause = IllegalInstruction
halted = false
```

### Program 10: Environment call

After `x1=1`, `ECALL` traps at PC `0x4`. The later instruction that would write `x2=2` never executes.

```text
trapped = true
cause = EnvironmentCall
PC remains 0x00000004
```

### Program 11: Misaligned word load

The program calculates address `0x101` and attempts `LW x2, 0(x1)`. A word needs 4-byte alignment, so the load traps at PC `0x4`.

`x2` remains unchanged and no later `EBREAK` executes. This demonstrates precise load exceptions: address checking occurs before writeback.

### Program 12: Misaligned halfword store

The program calculates address `0x101`, sets the store value to `0x123`, and attempts `SH` at PC `0x8`. A halfword needs 2-byte alignment, so the result is:

```text
cause = StoreAddressMisaligned
PC = 0x00000008
no data-memory byte transaction occurs
```

### Program 13: Out-of-range word load

`LUI` forms `0x00040000`, the first byte beyond the 256 KiB data memory. An aligned `LW` at that address traps at PC `0x4` with `LoadAccessFault`; its destination is not written.

This is different from misalignment: the address has valid word alignment but invalid capacity.

### Program 14: Out-of-range word store

The program forms address `0x00040000`, puts `0x7b` in `x2`, and attempts `SW` at PC `0x8`.

Expected result:

```text
cause = StoreAccessFault
PC = 0x00000008
no bytes are written
```

### Program 15: Misaligned JALR target

`x1` is set to `2`, then `JALR x0, 0(x1)` attempts target `0x2`. JALR clears bit 0, but `0x2` remains not divisible by 4.

The exception belongs to the `JALR` itself:

```text
PC remains 0x00000004
target 0x00000002 is never fetched
trapped = true
cause = InstructionAddressMisaligned
halted = false
```

Oracle unit tests repeat this case with a nonzero destination to prove that a misaligned JAL/JALR also suppresses link-register writeback.

### Program 16: Aligned jump to invalid instruction memory

`LUI` forms `0x00040000`. `JALR x0, 0(x1)` has a correctly aligned target, so the jump at PC `0x4` commits and PC becomes `0x00040000`.

The **next instruction attempt** cannot fetch four bytes there and traps:

```text
PC = 0x00040000
trapped = true
cause = InstructionAccessFault
halted = false
```

Programs 15 and 16 together teach the key distinction: a misaligned target faults on the jump, while an aligned but unmapped target faults on the later fetch.

## 15. Running the Tests

Build from the repository root:

```bash
cmake -S . -B build
cmake --build build -j8
```

Run the answer-sheet and memory checks:

```bash
ctest --test-dir build --output-on-failure \
  -R "BehavioralMemory64Kx32Test|RV32IInstructionOracleTest|RV32IInstructionLockstep|BehavioralRV32ISystem"
```

Run a single numbered program:

```bash
ctest --test-dir build --output-on-failure \
  -R "BehavioralRV32ISystemProgram5Test"
```

Run all tests:

```bash
ctest --test-dir build --output-on-failure
```

The visualizer test registry also provides scenario aliases:

```text
behavioral-rv32i-system-program1
...
behavioral-rv32i-system-program16
```

## 16. How to Debug a Failure as a Beginner

Start with the first mismatching instruction, not the final program result.

1. Check `pc_before` and the raw instruction. If wrong, inspect PC/fetch wiring.
2. Check the decoded instruction, register indices, and immediate. If wrong, inspect decoder/immediate generation.
3. Check `rs1_value`, `rs2_value`, and `x0`. If wrong, inspect the register file/read ports.
4. Check ALU inputs and result. Distinguish signed from unsigned comparisons and logical from arithmetic shifts.
5. For a load/store, check effective address, size, extension mode, range, alignment, and byte lanes.
6. Check writeback enable, destination, and value. A trap must suppress writeback.
7. Check branch condition and target. Use `pc_before`, not `PC+4`, as the branch/JAL base.
8. For JALR, use the old `rs1`, add the signed immediate, then clear bit 0.
9. Check whether the PC should advance, redirect, or remain on a halt/trap.

The answer sheet is strong enough to guide structural bring-up because its semantic edge cases, independent program outcomes, interface contract, Harvard memories, real stores, and mismatch detection are tested. It should still be treated as a fallible engineering reference: when structural logic and the answer sheet disagree, check the RISC-V rule and add a regression rather than blindly forcing either side to match.

## 17. Primary ISA Reference

Use the ratified [RISC-V RV32I Base Integer Instruction Set, Version 2.1](https://docs.riscv.org/reference/isa/v20260120/unpriv/rv32.html) when an implementation detail is in doubt. In particular, it defines `IALIGN=32`, reports a misaligned taken branch/jump on the control-transfer instruction, does not raise that exception for a non-taken branch, and reports an aligned target access fault on the target instruction.
