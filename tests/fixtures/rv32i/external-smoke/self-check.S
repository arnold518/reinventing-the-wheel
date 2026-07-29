/*
 * Small, externally assembled RV32I image used to validate the ELF boundary
 * and the self-checking tohost protocol. It deliberately has no dependency on
 * CircuitSim's instruction oracle.
 */

    .option norvc
    .section .text.start, "ax", @progbits
    .globl _start
    .type _start, @function
_start:
    addi x5, x0, 7
    addi x6, x0, 5
    add  x7, x5, x6
    addi x28, x0, 12
    bne  x7, x28, fail

    lui  x29, 0x1
    sw   x7, 0(x29)
    lw   x30, 0(x29)
    bne  x30, x28, fail

    jal  x1, subroutine
    addi x28, x0, 13
    bne  x7, x28, fail
    jal  x0, pass

subroutine:
    addi x7, x7, 1
    jalr x0, 0(x1)

pass:
    lui  x5, %hi(tohost)
    addi x5, x5, %lo(tohost)
    addi x6, x0, 1
    sw   x6, 0(x5)
    sw   x0, 4(x5)
pass_loop:
    jal  x0, pass_loop

fail:
    lui  x5, %hi(tohost)
    addi x5, x5, %lo(tohost)
    addi x6, x0, 3
    sw   x6, 0(x5)
    sw   x0, 4(x5)
fail_loop:
    jal  x0, fail_loop

    .size _start, .-_start

    .section .tohost, "aw", @progbits
    .balign 8
    .globl tohost
tohost:
    .word 0
    .word 0
    .globl fromhost
fromhost:
    .word 0
    .word 0
