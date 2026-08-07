OUTPUT_ARCH(riscv)
ENTRY(_start)

MEMORY
{
    ram (rwx) : ORIGIN = 0x00000000, LENGTH = 0x00040000
}

SECTIONS
{
    . = ORIGIN(ram);

    .text :
    {
        KEEP(*(.text.start))
        *(.text .text.*)
    } > ram

    .rodata :
    {
        *(.rodata .rodata.*)
        *(.srodata .srodata.*)
    } > ram

    .data :
    {
        *(.data .data.*)
        *(.sdata .sdata.*)
    } > ram

    .bss (NOLOAD) :
    {
        __bss_start = .;
        *(.sbss .sbss.*)
        *(.bss .bss.* COMMON)
        __bss_end = .;
    } > ram

    . = ALIGN(16);
    __stack_bottom = .;
    . += 0x00004000;
    __stack_top = .;
    _image_end = .;

    .tohost 0x0003ffc0 :
    {
        KEEP(*(.tohost))
    } > ram

    /DISCARD/ :
    {
        *(.comment)
        *(.riscv.attributes)
    }
}

ASSERT(_start == 0x00000000,
       "CircuitSim RV32I reset vector must remain zero")
ASSERT(_image_end <= 0x0003ffc0,
       "Embench image and stack exceed CircuitSim memory")
ASSERT((fromhost + 8) <= 0x00040000,
       "Embench host interface exceeds CircuitSim memory")
