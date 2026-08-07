/* CircuitSim bare-metal entry and Embench pass/fail adapter. */
.section .text.start, "ax", @progbits
.globl _start
_start:
    la sp, __stack_top
    call main
    bnez a0, .Lfail

    li t1, 1
    j .Lreport

.Lfail:
    li t1, 3

.Lreport:
    la t0, tohost
    sw t1, 0(t0)
    sw zero, 4(t0)

.Ldone:
    j .Ldone

.section .tohost, "aw", @progbits
.balign 8
.globl tohost
tohost:
    .dword 0
.balign 8
.globl fromhost
fromhost:
    .dword 0
