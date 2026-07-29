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
    } > ram

    .data :
    {
        *(.data .data.*)
    } > ram

    .bss (NOLOAD) :
    {
        *(.bss .bss.* COMMON)
    } > ram

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
       "CircuitSim structural RV32I reset vector must remain zero")
ASSERT((tohost & 3) == 0,
       "tohost must be word aligned")
ASSERT((fromhost + 8) <= (ORIGIN(ram) + LENGTH(ram)),
       "fixture exceeds CircuitSim memory")
