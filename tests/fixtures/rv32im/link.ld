OUTPUT_ARCH(riscv)
ENTRY(_start)
MEMORY { ram (rwx) : ORIGIN = 0, LENGTH = 4K }
PHDRS { text PT_LOAD FLAGS(5); data PT_LOAD FLAGS(6); }
SECTIONS {
    .text : { KEEP(*(.text.start)) *(.text .text.*) } > ram :text
    .rodata : { *(.rodata .rodata.*) } > ram :text
    .data : { *(.data .data.*) *(.sdata .sdata.*) } > ram :data
    .bss (NOLOAD) : { *(.bss .bss.*) *(.sbss .sbss.*) *(COMMON) } > ram :data
    ASSERT(. < 0xc00, "smoke program overlaps stack")
}
