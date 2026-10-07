# § 4.1.1. Machine Model and Common Conventions

[Reference index](index.md) · [Instruction catalogue](instructions/index.md)

This page defines the state and common rules assumed by every instruction entry. Rules marked **CircuitSim policy** describe this execution environment, not requirements imposed on all RISC-V machines.

## Architectural state

| State | Width / extent | Contract |
| --- | --- | --- |
| `x0` | 32 bits | Reads as zero; destination writes are discarded |
| `x1`–`x31` | 31 × 32 bits | General-purpose integer registers; interpretation depends on the instruction |
| `pc` | 32 bits | Byte address of the instruction being executed; successful ordinary execution advances by four |
| Memory addresses | 32 bits | Byte addresses; address arithmetic wraps modulo `2^32`, but an access must still lie inside the implemented memory |
| Condition-code register | None | No architectural carry, overflow, zero, or sign flags; internal ALU flags are not persistent ISA state |

Register values are bit patterns. Signed operations interpret bit 31 as the two's-complement sign bit; unsigned operations interpret all 32 bits as a nonnegative integer. Both interpretations use the same registers.

## Notation and operation helpers

Instruction operations use Python-style pseudocode. Each block starts **after successful instruction fetch and decoding**, with an active machine. It is a binary instruction-level specification, not gate-level timing or a second registered simulator.

| Name | Definition |
| --- | --- |
| `x[i]` | Unsigned 32-bit value in register `i`; `x[0]` is always zero |
| `pc` | Address of this instruction until the block updates it |
| `rd`, `rs1`, `rs2` | Decoded register numbers; only operands listed in the entry are used |
| `imm` | Decoded immediate: signed I/S/B/J value; for U format the already shifted upper value |
| `shamt` | Five-bit immediate shift count, 0–31 |
| `u32(v)` | `v & 0xFFFFFFFF`, keeping the low 32 bits |
| `s32(v)` | Interpret `u32(v)` as signed: subtract `2^32` when bit 31 is set |
| `sext(v, n)` | Interpret the low `n` bits as a signed two's-complement value |
| `write_rd(v)` | Write `u32(v)` to `x[rd]` unless `rd=0` |
| `read8/16/32(address)` | Validate alignment and the whole byte range, then read an unsigned little-endian value |
| `store8/16/32(address, value)` | Validate alignment and the whole byte range, then write the corresponding low bits, least-significant byte first |
| `check_target(address)` | Raise `InstructionAddressMisaligned` unless the target is divisible by four; do not check instruction-memory range here |
| `trap(cause)` | Stop the current operation with the named terminal trap; no later statements execute |

The [documentation tests](../4.2/verification/workloads.md#fixed-programs-and-isa-references) execute these operation blocks against the compiled reference. The complete production C++ implementation is in [4.1.4](implementation.md).

## Register and PC updates

- Read source operands from the old state before writing a destination. Using the same register as source and destination is allowed.
- All register results and calculated addresses are reduced to 32 bits. Integer arithmetic never traps merely because of signed overflow or unsigned carry.
- Successful ordinary instructions advance PC by four. Branches and jumps select their next PC explicitly; terminal instructions stop without advancing it.
- Only the listed destination, memory bytes, PC, or terminal status changes. There are no persistent arithmetic condition-code flags.
- A destination of `x0` discards only the register write. Loads still access memory and jumps still redirect execution.
- FENCE and accepted no-effect/HINT encodings have no hidden architectural register effect. Performance hints are not implemented.

## CircuitSim execution environment

| Property | Current reference environment |
| --- | --- |
| Instruction width / alignment | 32-bit instructions, four-byte-aligned instruction addresses (`IALIGN=32`) |
| Byte order | Little-endian for instructions and data |
| Memory organization | Separate instruction and data memories in the visual testbench; a data store does not modify instruction memory |
| Default capacity | Each FF-model instruction/data device has 65,536 **words** (256 KiB): byte addresses `0x00000000`–`0x0003FFFF` |
| Memory interface | All cores: universal REQ/RSP endpoints. Single-cycle and baseline require zero-wait FF timing; the transaction-capable pipeline also supports delayed technologies. These timing interfaces do not change instruction semantics |
| Reset | Visual program core resets PC and all registers to zero, clears halt/trap state; program/data memory is retained |
| Initial memory | Fixtures explicitly clear memory and load their instruction/data images before running |
| Execution status | Simulator state `halted`, `trapped`, and `trap_cause`; these are not privileged RISC-V CSRs |
| Instruction count | Reference bookkeeping counts attempted instructions, including terminal faults and `EBREAK`; it is not an implementation of `minstret` |
| Extensions/platform features | No M, A, F/D, C, vector, Zicsr, Zifencei, privilege modes, interrupts, virtual memory, or resumable trap handlers |

The 32-bit address space is an ISA property; the small address range, Harvard memory organization, initial state, and halt policy are project choices. The standalone oracle permits caller-sized functional memory and either unified or separate instruction/data memories. That does not enlarge the supported address map of the current pin-level reference core.

## Data-memory rules

For an `n`-byte access, the address must be divisible by `n` and the entire byte range must fit inside memory. Byte accesses require no extra alignment. **CircuitSim policy:** misaligned loads/stores trap; an implementation in another execution environment may support them.

In the default 256 KiB memory, the last word begins at `0x0003FFFC`. Address `0x0003FFFF` is usable for one byte, not four. Effective-address arithmetic wraps to 32 bits, but a memory access does not wrap around the end of the memory.

Little-endian example: storing word `0x12345678` at `a` places bytes `78 56 34 12` at `a`, `a+1`, `a+2`, `a+3`. A store changes only its addressed bytes. A rejected store changes none.

## Common exceptions

Every instruction is subject to these checks **before** its operation block:

1. Check the current PC's four-byte alignment.
2. Check that all four instruction bytes are accessible.
3. Fetch and decode the instruction. Rejected encodings produce an illegal-instruction trap.

For data accesses, alignment is checked before range. For a taken branch/jump, target alignment is checked before any link write. An **aligned but inaccessible target** is different: the transfer completes, and the next instruction fetch faults at that target.

| Project cause | `TRAP_CAUSE` code | Trigger |
| --- | ---: | --- |
| `None` | 0 | No trap; also used with successful EBREAK halt |
| `IllegalInstruction` | 1 | Decoder rejects the fetched instruction |
| `EnvironmentCall` | 2 | Exact ECALL instruction |
| `InstructionAddressMisaligned` | 3 | Current PC or taken branch/jump target is not four-byte aligned |
| `InstructionAccessFault` | 4 | Current fetch is outside instruction memory or receives a fetch fault |
| `LoadAddressMisaligned` | 5 | Load address violates the selected access alignment |
| `LoadAccessFault` | 6 | Load cannot access its complete byte range or receives a data-memory fault |
| `StoreAddressMisaligned` | 7 | Store address violates the selected access alignment |
| `StoreAccessFault` | 8 | Store cannot access its complete byte range or receives a data-memory fault |
| `InstructionLimit` | 9 | Host oracle `run()` reaches its requested step limit without halting/trapping; not an instruction-generated exception |

These numbers are **project enum encodings**, not RISC-V `mcause` values. No `mcause`, `mepc`, or `mtvec` register exists in this environment. The behavioral system exposes the cause through its state API; the behavioral **core** and visual root also have a four-bit cause pin.

For an instruction-generated trap, PC remains at the faulting instruction; its destination register and data memory are unchanged; earlier instruction effects remain. The reference sets `trapped=true` and leaves `halted=false`. EBREAK instead sets `halted=true`, with no trap. Further steps are inert until reset or explicit host reinitialization. Reset clears core state but does not erase program/data images.

If multiple conditions apply, the oracle checks current-PC misalignment before fetch range, fetch before decode, and data alignment before data range. For example, a misaligned out-of-range load reports `LoadAddressMisaligned`. These priorities describe the current environment. External pin responses must obey the memory protocol; arbitrary conflicting fault/data signals are not an ISA-level test.

The reference `instruction_count` increments once for a terminal instruction attempt as well as for a successful instruction. Calling `step()` after stopping does not increment it. `run(..., max_instructions)` adds an `InstructionLimit` stop if the limit is exhausted while still active; it does not execute a synthetic instruction. Answer-sheet checkpoint numbers therefore must not be confused with privileged retired-instruction counters.

## Register names, calls, and the stack

The [integer ABI](https://riscv-non-isa.github.io/riscv-elf-psabi-doc/#_integer_register_convention) assigns conventional names to the same registers:

| Registers | ABI names | Conventional use |
| --- | --- | --- |
| `x0` | `zero` | Constant zero |
| `x1` | `ra` | Return address |
| `x2` | `sp` | Stack pointer |
| `x3`, `x4` | `gp`, `tp` | Global and thread pointers |
| `x5`–`x7` | `t0`–`t2` | Temporaries |
| `x8`, `x9` | `s0/fp`, `s1` | Saved registers; optional frame pointer |
| `x10`–`x17` | `a0`–`a7` | Arguments; `a0`/`a1` also carry return values |
| `x18`–`x27` | `s2`–`s11` | Saved registers |
| `x28`–`x31` | `t3`–`t6` | Temporaries |

Only `x0` has a fixed value imposed by the ISA. Calling-convention names do not create separate registers. Calls do not automatically push a return address, save other registers, or adjust the stack pointer. Software does that with ordinary instructions.

The stack is a software-managed region of **data memory**, not a third memory component. Under the standard ABI it grows toward lower addresses and `sp` remains 16-byte aligned. Reset does not automatically select a stack region.

The linker/startup code is responsible for stack setup. Our Embench [linker script](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/tools/rv32i-benchmarks/embench/link.ld) reserves 16 KiB after the image data, and [startup code](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/tools/rv32i-benchmarks/embench/start.S) loads `__stack_top` into `sp` before calling `main`. This is not necessarily the end of all data memory, and the core does not enforce that stack-region boundary separately. Hand-written programs may use `x2` as another general-purpose register if they are not following the ABI.
