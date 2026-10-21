.section .text.start,"ax",@progbits
.globl _start
_start:
    lui sp, 1
    call main
    ebreak
1:  jal zero, 1b
