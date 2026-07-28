# RV32I Milestone 7C Program Test Plan

Last updated: 2026-06-05

## Objective

Milestone 7C broadens the behavioral RV32I system tests.

Status: the 16 numbered program tests are implemented as CTest cases, exposed through Python bindings, exposed as visualizer scenarios, and documented below.

Milestone 7A created the instruction-lockstep harness. Milestone 7B created `RV32IReferenceSystem` with a behavioral core and visible instruction/data memories. Milestone 7C should prove that the system runs more than one demo program.

Every test in this document:

- inherit from `RV32IInstructionLockstepTest`
- run on `RV32IReferenceSystem`
- use `RV32IReferenceCore`
- use `Memory64Kx32` instruction and data memories
- compare every committed instruction against `RV32IInstructionOracle`
- expose a visualizer scenario named from the test number

## Naming

Use numbered behavioral system program tests:

```text
RV32ISingleCycleSystemTest/program-01
RV32ISingleCycleSystemTest/program-02
RV32ISingleCycleSystemTest/program-03
...
```

Visualizer scenarios should match:

```text
rv32i-program1
rv32i-program2
rv32i-program3
...
```

The visualizer exposes these cases as `rv32i-program1` through `rv32i-program16`.

The C snippets below describe the intent of each program. The assembly snippets are the RV32I behavior that the test should encode.

## Program List

| # | Test Name | Scenario | Main Focus |
| ---: | --- | --- | --- |
| 1 | `RV32ISingleCycleSystemTest/program-01` | `rv32i-program1` | Existing loop, word loads, arithmetic, branch loop, word/byte/halfword stores. |
| 2 | `RV32ISingleCycleSystemTest/program-02` | `rv32i-program2` | ALU R-type and I-type operations, shifts, signed/unsigned comparisons, x0 write ignore. |
| 3 | `RV32ISingleCycleSystemTest/program-03` | `rv32i-program3` | Load/store widths, sign extension, zero extension, little-endian memory behavior. |
| 4 | `RV32ISingleCycleSystemTest/program-04` | `rv32i-program4` | All branch predicates, both taken and not-taken paths. |
| 5 | `RV32ISingleCycleSystemTest/program-05` | `rv32i-program5` | `JAL`, `JALR`, link-register writes, function-call shape. |
| 6 | `RV32ISingleCycleSystemTest/program-06` | `rv32i-program6` | `LUI`, `AUIPC`, `FENCE`, PC-relative values. |
| 7 | `RV32ISingleCycleSystemTest/program-07` | `rv32i-program7` | Fibonacci loop with repeated stores and register dependencies. |
| 8 | `RV32ISingleCycleSystemTest/program-08` | `rv32i-program8` | Byte copy and checksum with `LBU`, `SB`, pointer increments. |
| 9 | `RV32ISingleCycleSystemTest/program-09` | `rv32i-program9` | Illegal instruction trap. |
| 10 | `RV32ISingleCycleSystemTest/program-10` | `rv32i-program10` | `ECALL` trap, separate from `EBREAK` halt. |
| 11 | `RV32ISingleCycleSystemTest/program-11` | `rv32i-program11` | Misaligned load trap. |
| 12 | `RV32ISingleCycleSystemTest/program-12` | `rv32i-program12` | Misaligned store trap. |
| 13 | `RV32ISingleCycleSystemTest/program-13` | `rv32i-program13` | Out-of-range load access fault. |
| 14 | `RV32ISingleCycleSystemTest/program-14` | `rv32i-program14` | Out-of-range store access fault. |
| 15 | `RV32ISingleCycleSystemTest/program-15` | `rv32i-program15` | Misaligned instruction address trap after `JALR`. |
| 16 | `RV32ISingleCycleSystemTest/program-16` | `rv32i-program16` | Out-of-range instruction fetch access fault after `JALR`. |

## Program 1: Sum Loop And Mixed Stores

Test name:

```text
RV32ISingleCycleSystemTest/program-01
```

Scenario:

```text
rv32i-program1
```

Focus:

- existing 7B integration program
- loop with `BNE`
- word loads
- integer addition
- `SW`, `SB`, and `SH`
- `EBREAK` halt

Initial data:

```text
memory[0x100..0x10f] = uint32_t input[4] = {3, 5, 7, 11}
```

C intent:

```c
#include <stdint.h>

void program1(uint8_t *memory) {
    uint32_t *input = (uint32_t *)&memory[0x100];
    uint32_t sum = 0;

    for (uint32_t i = 0; i < 4; ++i) {
        sum += input[i];
    }

    *(uint32_t *)&memory[0x120] = sum;
    memory[0x124] = (uint8_t)sum;
    *(uint16_t *)&memory[0x126] = (uint16_t)(sum + 0x34);
}
```

Assembly:

```asm
    addi x1, x0, 0x100     # source pointer
    addi x2, x0, 4         # count
    addi x3, x0, 0         # sum

loop:
    lw   x4, 0(x1)
    add  x3, x3, x4
    addi x1, x1, 4
    addi x2, x2, -1
    bne  x2, x0, loop

    addi x5, x0, 0x120
    sw   x3, 0(x5)
    sb   x3, 4(x5)
    addi x6, x3, 0x34
    sh   x6, 6(x5)
    ebreak
```

Expected key result:

```text
x3 = 26
x6 = 78
memory[0x120..0x123] = 1a 00 00 00
memory[0x124] = 1a
memory[0x126..0x127] = 4e 00
halted = true
```

## Program 2: ALU R-Type And I-Type Coverage

Test name:

```text
RV32ISingleCycleSystemTest/program-02
```

Scenario:

```text
rv32i-program2
```

Focus:

- `ADD`, `SUB`
- `AND`, `OR`, `XOR`
- `SLL`, `SRL`, `SRA`
- `SLT`, `SLTU`
- `ADDI`, `ANDI`, `ORI`, `XORI`
- `SLLI`, `SRLI`, `SRAI`
- `SLTI`, `SLTIU`
- writes to `x0` are ignored

C intent:

```c
#include <stdint.h>

void program2(uint8_t *memory) {
    uint32_t a = 0x55;
    uint32_t b = 0x0f;
    int32_t neg = -8;

    uint32_t out[12];
    out[0] = a + b;
    out[1] = a - b;
    out[2] = a & b;
    out[3] = a | b;
    out[4] = a ^ b;
    out[5] = b << 4;
    out[6] = out[5] >> 2;
    out[7] = (uint32_t)(neg >> 1);
    out[8] = neg < (int32_t)a;
    out[9] = (uint32_t)neg < a;
    out[10] = (a ^ 0x33) | 0x100;
    out[11] = (a & 0x3f);

    for (uint32_t i = 0; i < 12; ++i) {
        *(uint32_t *)&memory[0x200 + i * 4] = out[i];
    }
}
```

Assembly:

```asm
    addi x1, x0, 0x55
    addi x2, x0, 0x0f
    addi x10, x0, -8

    add  x3, x1, x2
    sub  x4, x1, x2
    and  x5, x1, x2
    or   x6, x1, x2
    xor  x7, x1, x2
    slli x8, x2, 4
    srli x9, x8, 2
    srai x11, x10, 1
    slt  x12, x10, x1
    sltu x13, x10, x1
    xori x14, x1, 0x33
    ori  x14, x14, 0x100
    andi x15, x1, 0x3f
    slti x16, x10, 0
    sltiu x17, x10, 1
    addi x0, x0, 123       # must be ignored

    addi x20, x0, 0x200
    sw   x3, 0(x20)
    sw   x4, 4(x20)
    sw   x5, 8(x20)
    sw   x6, 12(x20)
    sw   x7, 16(x20)
    sw   x8, 20(x20)
    sw   x9, 24(x20)
    sw   x11, 28(x20)
    sw   x12, 32(x20)
    sw   x13, 36(x20)
    sw   x14, 40(x20)
    sw   x15, 44(x20)
    sw   x16, 48(x20)
    sw   x17, 52(x20)
    ebreak
```

Expected key result:

```text
x0 remains 0
signed comparisons and unsigned comparisons differ for -8
arithmetic right shift preserves sign bits
halted = true
```

## Program 3: Load And Store Widths

Test name:

```text
RV32ISingleCycleSystemTest/program-03
```

Scenario:

```text
rv32i-program3
```

Focus:

- `LB`, `LBU`
- `LH`, `LHU`
- `LW`
- `SB`, `SH`, `SW`
- sign extension vs zero extension
- little-endian byte ordering

Initial data:

```text
memory[0x100..0x10b] = 80 7f 00 00 80 ff 00 00 21 43 65 87
```

C intent:

```c
#include <stdint.h>

void program3(uint8_t *memory) {
    int32_t signed_byte = (int8_t)memory[0x100];
    uint32_t unsigned_byte = memory[0x100];
    int32_t signed_half = (int16_t)*(uint16_t *)&memory[0x104];
    uint32_t unsigned_half = *(uint16_t *)&memory[0x104];
    uint32_t word = *(uint32_t *)&memory[0x108];

    *(uint32_t *)&memory[0x120] = (uint32_t)signed_byte;
    *(uint32_t *)&memory[0x124] = unsigned_byte;
    *(uint32_t *)&memory[0x128] = (uint32_t)signed_half;
    *(uint32_t *)&memory[0x12c] = unsigned_half;
    *(uint32_t *)&memory[0x130] = word;
    memory[0x134] = (uint8_t)unsigned_byte;
    *(uint16_t *)&memory[0x136] = (uint16_t)unsigned_half;
}
```

Assembly:

```asm
    addi x1, x0, 0x100
    lb   x2, 0(x1)
    lbu  x3, 0(x1)
    lb   x4, 1(x1)
    lh   x5, 4(x1)
    lhu  x6, 4(x1)
    lw   x7, 8(x1)

    addi x8, x0, 0x120
    sw   x2, 0(x8)
    sw   x3, 4(x8)
    sw   x4, 8(x8)
    sw   x5, 12(x8)
    sw   x6, 16(x8)
    sw   x7, 20(x8)
    sb   x3, 24(x8)
    sh   x6, 26(x8)
    ebreak
```

Expected key result:

```text
LB 0x80 -> 0xffffff80
LBU 0x80 -> 0x00000080
LH 0xff80 -> 0xffffff80
LHU 0xff80 -> 0x0000ff80
LW bytes at 0x108: 21 43 65 87 -> 0x87654321
halted = true
```

## Program 4: Branch Predicate Coverage

Test name:

```text
RV32ISingleCycleSystemTest/program-04
```

Scenario:

```text
rv32i-program4
```

Focus:

- `BEQ`
- `BNE`
- `BLT`
- `BGE`
- `BLTU`
- `BGEU`
- taken branch PC update
- not-taken fallthrough
- signed vs unsigned comparison

C intent:

```c
#include <stdint.h>

void program4(uint8_t *memory) {
    int32_t neg = -1;
    int32_t pos = 1;
    uint32_t score = 0;

    if (5 == 5) score += 1;
    if (5 != 6) score += 1;
    if (neg < pos) score += 1;
    if (pos >= neg) score += 1;
    if ((uint32_t)pos < (uint32_t)neg) score += 1;
    if ((uint32_t)neg >= (uint32_t)pos) score += 1;

    *(uint32_t *)&memory[0x120] = score;
}
```

Assembly:

```asm
    addi x10, x0, 0        # score
    addi x1, x0, 5
    addi x2, x0, 5
    beq  x1, x2, beq_ok
    addi x10, x10, 100
beq_ok:
    addi x10, x10, 1

    addi x2, x0, 6
    bne  x1, x2, bne_ok
    addi x10, x10, 100
bne_ok:
    addi x10, x10, 1

    addi x3, x0, -1
    addi x4, x0, 1
    blt  x3, x4, blt_ok
    addi x10, x10, 100
blt_ok:
    addi x10, x10, 1

    bge  x4, x3, bge_ok
    addi x10, x10, 100
bge_ok:
    addi x10, x10, 1

    bltu x4, x3, bltu_ok
    addi x10, x10, 100
bltu_ok:
    addi x10, x10, 1

    bgeu x3, x4, bgeu_ok
    addi x10, x10, 100
bgeu_ok:
    addi x10, x10, 1

    beq  x1, x4, wrong_taken
    addi x10, x10, 1       # not-taken path
    jal  x0, store_score
wrong_taken:
    addi x10, x10, 100

store_score:
    addi x11, x0, 0x120
    sw   x10, 0(x11)
    ebreak
```

Expected key result:

```text
score = 7
both taken and not-taken branch paths execute correctly
halted = true
```

## Program 5: Jump And Link

Test name:

```text
RV32ISingleCycleSystemTest/program-05
```

Scenario:

```text
rv32i-program5
```

Focus:

- `JAL`
- `JALR`
- link register receives `pc + 4`
- nested call shape
- `JAL x0, label` as unconditional jump without link write

C intent:

```c
#include <stdint.h>

static uint32_t helper_b(uint32_t value) {
    return value + 35;
}

static uint32_t helper_a(void) {
    uint32_t value = 7;
    value = helper_b(value);
    return value + 1;
}

void program5(uint8_t *memory) {
    *(uint32_t *)&memory[0x120] = helper_a();
}
```

Assembly:

```asm
    addi x1, x0, 0x120
    jal  x5, helper_a
    sw   x10, 0(x1)
    jal  x0, done

helper_a:
    addi x10, x0, 7
    jal  x6, helper_b
    addi x10, x10, 1
    jalr x0, 0(x5)

helper_b:
    addi x10, x10, 35
    jalr x0, 0(x6)

done:
    ebreak
```

Expected key result:

```text
memory[0x120..0x123] = 2b 00 00 00
x5 and x6 receive valid return addresses during execution
halted = true
```

## Program 6: Upper Immediates, PC Relative Values, And Fence

Test name:

```text
RV32ISingleCycleSystemTest/program-06
```

Scenario:

```text
rv32i-program6
```

Focus:

- `LUI`
- `AUIPC`
- `FENCE` accepted as legal no-op
- 32-bit constant construction
- PC-relative value construction

C intent:

```c
#include <stdint.h>

void program6(uint8_t *memory, uint32_t pc_of_auipc0, uint32_t pc_of_auipc1) {
    uint32_t constant = 0x12345678;
    uint32_t pc0 = pc_of_auipc0;
    uint32_t pc_plus_0x1000 = pc_of_auipc1 + 0x1000;

    *(uint32_t *)&memory[0x120] = constant;
    *(uint32_t *)&memory[0x124] = pc0;
    *(uint32_t *)&memory[0x128] = pc_plus_0x1000;
}
```

Assembly:

```asm
    lui   x1, 0x12345
    addi  x1, x1, 0x678
    auipc x2, 0
    auipc x3, 1
    fence

    addi x4, x0, 0x120
    sw   x1, 0(x4)
    sw   x2, 4(x4)
    sw   x3, 8(x4)
    ebreak
```

Expected key result:

```text
memory[0x120..0x123] = 78 56 34 12
AUIPC results match the instruction PC used by the oracle
FENCE commits without changing architectural state except PC/instruction_count
halted = true
```

## Program 7: Fibonacci Store Loop

Test name:

```text
RV32ISingleCycleSystemTest/program-07
```

Scenario:

```text
rv32i-program7
```

Focus:

- loop-carried register dependencies
- repeated stores
- pointer increments
- branch loop control

C intent:

```c
#include <stdint.h>

void program7(uint8_t *memory) {
    uint32_t a = 0;
    uint32_t b = 1;

    for (uint32_t i = 0; i < 8; ++i) {
        *(uint32_t *)&memory[0x120 + i * 4] = a;
        uint32_t next = a + b;
        a = b;
        b = next;
    }
}
```

Assembly:

```asm
    addi x1, x0, 0x120     # output pointer
    addi x2, x0, 8         # count
    addi x3, x0, 0         # a
    addi x4, x0, 1         # b

fib_loop:
    sw   x3, 0(x1)
    add  x5, x3, x4
    addi x3, x4, 0
    addi x4, x5, 0
    addi x1, x1, 4
    addi x2, x2, -1
    bne  x2, x0, fib_loop
    ebreak
```

Expected key result:

```text
memory words at 0x120 = 0, 1, 1, 2, 3, 5, 8, 13
halted = true
```

## Program 8: Byte Copy And Checksum

Test name:

```text
RV32ISingleCycleSystemTest/program-08
```

Scenario:

```text
rv32i-program8
```

Focus:

- byte memory loop
- `LBU`
- `SB`
- pointer increments
- checksum accumulation

Initial data:

```text
memory[0x100..0x104] = 01 02 03 04 ff
```

C intent:

```c
#include <stdint.h>

void program8(uint8_t *memory) {
    uint32_t sum = 0;

    for (uint32_t i = 0; i < 5; ++i) {
        uint8_t value = memory[0x100 + i];
        memory[0x140 + i] = value;
        sum += value;
    }

    *(uint32_t *)&memory[0x160] = sum;
}
```

Assembly:

```asm
    addi x1, x0, 0x100     # source
    addi x2, x0, 0x140     # destination
    addi x3, x0, 5         # count
    addi x4, x0, 0         # sum

copy_loop:
    lbu  x5, 0(x1)
    sb   x5, 0(x2)
    add  x4, x4, x5
    addi x1, x1, 1
    addi x2, x2, 1
    addi x3, x3, -1
    bne  x3, x0, copy_loop

    addi x6, x0, 0x160
    sw   x4, 0(x6)
    ebreak
```

Expected key result:

```text
memory[0x140..0x144] = 01 02 03 04 ff
memory[0x160..0x163] = 09 01 00 00
halted = true
```

## Program 9: Illegal Instruction Trap

Test name:

```text
RV32ISingleCycleSystemTest/program-09
```

Scenario:

```text
rv32i-program9
```

Focus:

- decoder rejects an illegal raw instruction
- core enters trapped state
- no later instruction commits after the trap

C intent:

```c
void program9(void) {
    /* Intent: execute an invalid RV32I instruction and trap. */
    __builtin_trap();
}
```

Assembly:

```asm
    addi x1, x0, 1
    .word 0xffffffff       # illegal for this RV32I subset
    addi x2, x0, 2         # must not execute
```

Expected key result:

```text
trapped = true
trap_cause = IllegalInstruction
x1 = 1
x2 = 0
halted = false
```

## Program 10: ECALL Trap

Test name:

```text
RV32ISingleCycleSystemTest/program-10
```

Scenario:

```text
rv32i-program10
```

Focus:

- `ECALL` traps
- `ECALL` is distinct from `EBREAK`
- no instruction after `ECALL` commits

C intent:

```c
void program10(void) {
    /* Intent: make an environment call and trap. */
    __asm__ volatile("ecall");
}
```

Assembly:

```asm
    addi x1, x0, 1
    ecall
    addi x2, x0, 2         # must not execute
```

Expected key result:

```text
trapped = true
trap_cause = EnvironmentCall
halted = false
x1 = 1
x2 = 0
```

## Program 11: Misaligned Load Trap

Test name:

```text
RV32ISingleCycleSystemTest/program-11
```

Scenario:

```text
rv32i-program11
```

Focus:

- misaligned `LW`
- load address trap
- no writeback for faulting load

C intent:

```c
#include <stdint.h>

void program11(void) {
    volatile uint32_t value = *(volatile uint32_t *)0x101;
    (void)value;
}
```

Assembly:

```asm
    addi x1, x0, 0x101
    lw   x2, 0(x1)
    ebreak                 # must not execute
```

Expected key result:

```text
trapped = true
trap_cause = LoadAddressMisaligned
x2 = 0
halted = false
```

## Program 12: Misaligned Store Trap

Test name:

```text
RV32ISingleCycleSystemTest/program-12
```

Scenario:

```text
rv32i-program12
```

Focus:

- misaligned `SH` or `SW`
- store address trap
- data memory is not modified by a faulting store

C intent:

```c
#include <stdint.h>

void program12(void) {
    *(volatile uint16_t *)0x101 = 0x1234;
}
```

Assembly:

```asm
    addi x1, x0, 0x101
    addi x2, x0, 0x123
    sh   x2, 0(x1)
    ebreak                 # must not execute
```

Expected key result:

```text
trapped = true
trap_cause = StoreAddressMisaligned
no bytes are written at 0x101
halted = false
```

## Program 13: Out-Of-Range Load Access Fault

Test name:

```text
RV32ISingleCycleSystemTest/program-13
```

Scenario:

```text
rv32i-program13
```

Focus:

- load access fault outside implemented memory
- current memory capacity is 256 KiB
- address `0x00040000` is the first invalid byte address

C intent:

```c
#include <stdint.h>

void program13(void) {
    volatile uint32_t value = *(volatile uint32_t *)0x00040000;
    (void)value;
}
```

Assembly:

```asm
    lui  x1, 0x40          # x1 = 0x00040000
    lw   x2, 0(x1)
    ebreak                 # must not execute
```

Expected key result:

```text
trapped = true
trap_cause = LoadAccessFault
x2 = 0
halted = false
```

## Program 14: Out-Of-Range Store Access Fault

Test name:

```text
RV32ISingleCycleSystemTest/program-14
```

Scenario:

```text
rv32i-program14
```

Focus:

- store access fault outside implemented memory
- faulting store writes no bytes

C intent:

```c
#include <stdint.h>

void program14(void) {
    *(volatile uint32_t *)0x00040000 = 0x7b;
}
```

Assembly:

```asm
    lui  x1, 0x40          # x1 = 0x00040000
    addi x2, x0, 0x7b
    sw   x2, 0(x1)
    ebreak                 # must not execute
```

Expected key result:

```text
trapped = true
trap_cause = StoreAccessFault
no bytes are written
halted = false
```

## Program 15: Misaligned Instruction Address Trap

Test name:

```text
RV32ISingleCycleSystemTest/program-15
```

Scenario:

```text
rv32i-program15
```

Focus:

- instruction address misalignment
- `JALR` target masking behavior
- trap is reported by the `JALR` before the target or link-register write is committed

C intent:

```c
void program15_misaligned_fetch(void) {
    /* Intent: attempt to jump to byte address 2; JALR itself traps. */
    ((void (*)(void))2)();
}
```

Assembly:

```asm
    addi x1, x0, 2
    jalr x0, 0(x1)         # target 0x00000002 is not 4-byte aligned
```

Expected key result:

```text
ADDI commits normally
JALR raises the trap at PC 0x00000004
PC remains 0x00000004; address 0x00000002 is never fetched
trap_cause = InstructionAddressMisaligned
halted = false
```

## Program 16: Out-Of-Range Instruction Fetch Fault

Test name:

```text
RV32ISingleCycleSystemTest/program-16
```

Scenario:

```text
rv32i-program16
```

Focus:

- instruction access fault outside implemented instruction memory
- `JALR` can move PC to a word-aligned but invalid address
- trap happens on the attempted fetch after PC is changed

C intent:

```c
void program16_out_of_range_fetch(void) {
    /* Intent: jump to the first byte outside implemented instruction memory. */
    ((void (*)(void))0x00040000)();
}
```

Assembly:

```asm
    lui  x1, 0x40          # x1 = 0x00040000
    jalr x0, 0(x1)
```

Expected key result:

```text
LUI and JALR commit normally
PC becomes 0x00040000
the next attempted fetch traps
trap_cause = InstructionAccessFault
halted = false
```

## Coverage Checklist

These programs cover the implemented RV32I base instruction groups:

| Instruction Group | Covered By |
| --- | --- |
| R-type ALU | Program 2 |
| I-type ALU | Program 2 |
| shift immediates | Program 2 |
| loads | Programs 1, 3, 8, 11, 13 |
| stores | Programs 1, 3, 7, 8, 12, 14 |
| branches | Programs 1, 4, 7, 8 |
| jumps | Programs 4, 5, 15, 16 |
| `LUI` | Programs 6, 13, 14, 16 |
| `AUIPC` | Program 6 |
| `FENCE` | Program 6 |
| `ECALL` | Program 10 |
| `EBREAK` | Programs 1 through 8 |
| illegal instruction trap | Program 9 |
| load/store alignment traps | Programs 11 and 12 |
| load/store access faults | Programs 13 and 14 |
| instruction fetch traps | Programs 15 and 16 |

## Implementation Order

Recommended order:

1. Implement programs 2 through 6 first. They cover normal instruction execution.
2. Implement programs 7 and 8 next. They are longer realistic memory loops.
3. Implement programs 9 through 16 last. They intentionally trap and are easier to debug after normal execution is stable.

Each new test should be added to:

- `include/tests/RV32ISystemTests.hpp`
- `src/tests/RV32ISystemTests.cpp`
- `src/tests/TestRegistry.cpp`
- `src/bindings/TestsBinding.cpp`
- `tests/CMakeLists.txt`
- `visualizer-v2/server.py`

Each new visualizer scenario should get a saved layout entry after regeneration.
