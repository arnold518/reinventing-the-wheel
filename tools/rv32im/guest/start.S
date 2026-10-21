.file "start.S"
.section .text.start,"ax",@progbits
.globl _start
_start:
    lui sp, 0x3e
    la t0, __bss_start
    la t1, __bss_end
1:  bgeu t0, t1, 2f
    sw zero, 0(t0)
    addi t0, t0, 4
    jal zero, 1b
2:  call main
    ebreak
3:  jal zero, 3b
