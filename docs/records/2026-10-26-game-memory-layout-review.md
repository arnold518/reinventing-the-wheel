# Game RAM layout and real-system comparison

Started: 2026-10-26. Status: closed. Code state: `alpha` at `5549ee0`, with
existing pending changes and frozen gameplay execution preserved.

## Summary

The shared-RAM section layout is conventional for a bare-metal RAM-resident
program. The exact 192/56/4/4 KiB budgets are project choices, not measured
consumption or hardware partitions. Current approved game ELF static images
span 5.7–6.8 KiB; runtime peak stack use has not been measured. No memory map,
program binary, capture process or simulator behavior changes in this review.

## 2026-10-26 — Source and artifact measurements

`tools/rv32im/guest/link.ld` places `.text`, `.rodata`, `.data` and `.bss` in
one 256 KiB RAM and asserts that the linked image ends below `0x30000`.
`start.S` sets SP to `0x3e000`. `platform.h` gives the ordinary-RAM report
address as `0x3f000`. The space between image ceiling and initial SP is a
56 KiB intended stack budget; no stack-limit enforcement or protected guard
region is established by these files. The additional 4 KiB gap is reserve.
The report uses nine words (36 bytes) within its 4 KiB reservation.

Read the five immutable ELFs from
`build-milestone/short-gameplays-visual-ticks-20261025/frozen/prepared/` with
the existing pinned `riscv-none-elf-size -A`. Allocated static image spans,
including alignment and excluding stack, report, separate pixel storage and
ELF-only attributes/comments, are:

| Game | Code bytes | Constant bytes | BSS bytes | Static address span |
| --- | ---: | ---: | ---: | ---: |
| Snake | 5,432 | 284 | 872 | 6,588 |
| Pong | 5,192 | 250 | 380 | 5,824 |
| 2048 | 5,820 | 304 | 872 | 6,996 |
| Sokoban | 5,480 | 348 | 872 | 6,700 |
| Tetris | 5,408 | 284 | 872 | 6,564 |

These ELFs contain no nonempty `.data` section. Pong has two alignment bytes
before `.bss`; the other spans equal the section sum. The GNU tool's overall
`Total` also includes ELF metadata and is not the guest RAM footprint.
Stack occupancy and maximum call depth were not measured by this command.

## 2026-10-26 — Comparison and judgment

- [GNU ld](https://sourceware.org/binutils/docs/ld/MEMORY.html) documents
  software-defined regions and section placement. Our use of linker-defined
  code/constants/data/BSS regions follows that mechanism.
- The [RISC-V psABI](https://riscv-non-isa.github.io/riscv-elf-psabi-doc/)
  specifies a downward-growing stack and 16-byte alignment. Our initial SP
  satisfies this alignment. Its 56 KiB budget is intentionally generous;
  appropriate sizing requires peak-stack evidence rather than ELF size alone.
- [RP2040 documentation](https://www.raspberrypi.com/documentation/microcontrollers/microcontroller-chips.html)
  describes 264 kB of SRAM and execution directly from external flash.
  Thus our RAM capacity is plausible for an MCU-sized platform, while putting
  all code/constants in RAM represents RAM-resident firmware rather than that
  flash-executing arrangement.
- [Linux process maps](https://docs.kernel.org/filesystems/proc.html)
  describe executable/library, heap and stack mappings with per-region
  permissions. A desktop process uses virtual mappings rather than this
  machine-wide fixed partition. Our fixed report and reserve are test-platform
  conventions; a 4 KiB reservation here is not an MMU-protected page.

Keep the existing layout for current scenarios unless there is a concrete
capacity need. Before tightening it, measure peak stack use and retain an
explicit margin. A smaller report reservation is possible because only
36 bytes are currently needed. No redesign or memory tightening was requested
or performed, and no gameplay runs or broad tests were started for this review.
