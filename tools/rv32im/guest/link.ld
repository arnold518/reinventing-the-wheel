OUTPUT_ARCH(riscv)
ENTRY(_start)
MEMORY { ram (rwx) : ORIGIN = 0, LENGTH = 256K }
PHDRS { text PT_LOAD FLAGS(5); data PT_LOAD FLAGS(6); }
SECTIONS {
 .text : { KEEP(*(.text.start)) *(.text .text.*) } > ram :text
 .rodata : { *(.rodata .rodata.*) } > ram :text
 .data : { *(.data .data.*) *(.sdata .sdata.*) } > ram :data
 .bss (NOLOAD) : { . = ALIGN(4); __bss_start = .; *(.bss .bss.*) *(.sbss .sbss.*) *(COMMON) . = ALIGN(4); __bss_end = .; } > ram :data
 ASSERT(. < 0x30000, "guest image overlaps reserved stack/report space")
}
