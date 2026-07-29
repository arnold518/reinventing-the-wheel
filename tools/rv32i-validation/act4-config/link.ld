TEST_BASE = 0x00000000;
NUM_HARTS = 1;
STACK_SIZE = 0x00001000;
TOHOST_BASE = 0x0003ffc0;

OUTPUT_ARCH("riscv")
ENTRY(rvtest_entry_point)

PROVIDE(__stack_size = STACK_SIZE);
PROVIDE(__num_harts = NUM_HARTS);

SECTIONS
{
  . = TEST_BASE;

  .text.init   : { *(.text.init) }
  .text.rvtest : { *(.text.rvtest) *(.text.rvtest.*) }

  . = ALIGN(0x1000);
  .rodata : {
    *(.rodata) *(.rodata.*) *(.srodata) *(.srodata.*)
  }
  .data : {
    *(.data) *(.data.*) *(.sdata) *(.sdata.*)
  }

  . = ALIGN(16);
  .bss : {
    __bss_start = .;
    *(.sbss) *(.sbss.*) *(.bss) *(.bss.*) *(COMMON)
    . = ALIGN(16);
    __bss_end = .;
  }

  . = ALIGN(0x1000);
  .text.rvmodel : {
    *(.text.rvmodel) *(.text.rvmodel.*) *(.text) *(.text.*)
  }

  . = ALIGN(16);
  __stack_bottom = .;
  . += __stack_size * __num_harts;
  __stack_top = .;

  _image_end = .;

  .tohost TOHOST_BASE : {
    KEEP(*(.tohost))
  }

  /DISCARD/ : {
    *(.comment)
  }
}

ASSERT(rvtest_entry_point == TEST_BASE,
       "CircuitSim structural reset vector must remain zero")
ASSERT(_image_end <= TOHOST_BASE,
       "ACT4 image does not fit CircuitSim's 256 KiB memory")
ASSERT((fromhost + 8) <= 0x00040000,
       "ACT4 host interface exceeds CircuitSim memory")
