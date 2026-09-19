# § 4.1.5. Programs and Verification

[Reference index](../index.md) · [Instruction catalogue](../instructions/index.md) · [Executable reference](../implementation.md)

This catalogue documents **all 22 registered RV32I program fixtures**: sixteen correctness cases and six workloads. Every program has the same sections: **Purpose · Setup · Assembly · Execution · Expected result · Simulation and checkpoints**.

The explanations describe instruction-level behavior. They do not require the later single-cycle or pipeline construction sections.

## Program catalogue

Program numbers are the existing fixture identifiers. There are no omitted programs in the 1–22 range. Together, the programs contain **650 instruction-attempt checkpoints**.

<!-- rv32i-program-catalogue -->
| Program | Category | Attempts |
| --- | --- | ---: |
| [1. Array sum, loop, and stores](program-01.md) | Instruction behavior | 29 |
| [2. Arithmetic, logic, shifts, and comparisons](program-02.md) | Instruction behavior | 35 |
| [3. Signed and unsigned memory accesses](program-03.md) | Instruction behavior | 17 |
| [4. Conditional branches and fallthrough](program-04.md) | Instruction behavior | 24 |
| [5. Direct call, indirect jump, and return](program-05.md) | Instruction behavior | 11 |
| [6. Upper immediates, PC-relative values, and FENCE](program-06.md) | Instruction behavior | 10 |
| [7. Fibonacci sequence](program-07.md) | Instruction behavior | 61 |
| [8. Byte copy and checksum](program-08.md) | Instruction behavior | 42 |
| [9. Illegal instruction and precise stopping](program-09.md) | Traps | 2 |
| [10. Environment-call trap](program-10.md) | Traps | 2 |
| [11. Misaligned word load](program-11.md) | Traps | 2 |
| [12. Misaligned halfword store](program-12.md) | Traps | 3 |
| [13. Out-of-range word load](program-13.md) | Traps | 2 |
| [14. Out-of-range word store](program-14.md) | Traps | 3 |
| [15. Misaligned jump target](program-15.md) | Traps | 2 |
| [16. Instruction-fetch access fault](program-16.md) | Traps | 3 |
| [17. Independent ALU chains](program-17.md) | Workload | 73 |
| [18. Dependent ALU chain](program-18.md) | Workload | 66 |
| [19. Repeated load-use pairs](program-19.md) | Workload | 67 |
| [20. Branch-heavy loop](program-20.md) | Workload | 51 |
| [21. Sixteen-word array sum](program-21.md) | Workload | 84 |
| [22. Word copy and checksum](program-22.md) | Workload | 61 |

## Shared setup

The fixture clears both memories, loads the instruction image and the listed initial data, resets PC and registers to zero, and supplies the clock for the embedded run. Every program page repeats its image size, initial state, data bytes, and register roles.

Instruction and data memories are separate, each with the default 256 KiB capacity. Identical numeric addresses in the two memories do not alias. Unlisted data bytes start at zero. A stack is not allocated by the core; none of these hand-written fixtures uses the standard ABI stack.

The circuit runs use SRAM-model `MemoryContainer<sram,32>` endpoints through the common memory transaction interface. The cores directly expose this interface. Their internal load/store logic formats byte and halfword accesses; request tracking handles stalls and discards obsolete fetches after redirects. The memory endpoint holds each response until the core acknowledges it. A completed request is distinct from request acceptance: both cores can wait multiple clock cycles for memory. The zero-filled fixture image is prepared before simulation, not produced by resetting SRAM.

Each embedded visualizer starts with `RV32I_PROGRAM_ROOT.CORE` at **behavioral** fidelity, with both memories visible. This URL-only selection does not overwrite saved fidelity preferences. Choosing S/B and pressing Apply remains an explicit profile edit.

## Checkpoints and time

- `I0` is the first attempted instruction. Repeated loop instructions get separate checkpoints on every visit.
- A loaded word is not necessarily executed. Conversely, a failed fetch can produce a checkpoint without a successfully fetched word; Program 16 demonstrates this.
- Counts include the terminating EBREAK or fault. A trap is the expected outcome for Programs 9–16.
- Times are absolute simulator units from the behavioral-core recording, not milliseconds or architectural instruction latencies. Changing fidelity or clock scheduling may change them.
- Full checkpoint tables scroll horizontally and vertically. No row sampling or truncation is applied.
- The expected-state tables quote the fixtures' explicit final assertions. They do not imply that unlisted registers are zero. The store tables list all written bytes, including writes of zero.

Programs 17–22 are also used for pipeline workload tests. Their reference pages specify results and dependency patterns, not pipeline cycle counts, forwarding behavior, or stall penalties.

## One-step examples for all 40 instructions

Each row is an **independent** attempt at `P=0x00000040`, with `x6=A`, `x7=B`, every other register zero, and separate instruction/data memory. The instruction word is loaded at `P`. Initially `R32(0x100)=0x80FF7F01`, so bytes at `0x100`–`0x103` are `01 7F FF 80`; all other data bytes are zero.

The columns give the resulting `x5`, next PC, word at `0x100`, and terminal status. All arithmetic/register-destination examples use `rd=x5`; stores and branches have no destination. `A`/`B` remain unchanged. Values are hexadecimal words. `Run` means neither halted nor trapped; `Halt` means the project's EBREAK stop; `EnvironmentCall` is a trap. These explicit expectations are executed by `RV32IReferenceDocumentationTest`, not inferred by copying the oracle output.

<!-- rv32i-step-examples -->
| Instruction | Word | A | B | Expected x5 | Expected PC | Expected memory[0x100] | Status |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `ADD` | `0x007302B3` | `0x7FFFFFFF` | `0x00000001` | `0x80000000` | `0x00000044` | `0x80FF7F01` | Run |
| `SUB` | `0x407302B3` | `0x00000000` | `0x00000001` | `0xFFFFFFFF` | `0x00000044` | `0x80FF7F01` | Run |
| `SLL` | `0x007312B3` | `0x00000001` | `0x00000021` | `0x00000002` | `0x00000044` | `0x80FF7F01` | Run |
| `SLT` | `0x007322B3` | `0xFFFFFFFF` | `0x00000001` | `0x00000001` | `0x00000044` | `0x80FF7F01` | Run |
| `SLTU` | `0x007332B3` | `0xFFFFFFFF` | `0x00000001` | `0x00000000` | `0x00000044` | `0x80FF7F01` | Run |
| `XOR` | `0x007342B3` | `0x000000AA` | `0x0000000F` | `0x000000A5` | `0x00000044` | `0x80FF7F01` | Run |
| `SRL` | `0x007352B3` | `0x80000000` | `0x00000001` | `0x40000000` | `0x00000044` | `0x80FF7F01` | Run |
| `SRA` | `0x407352B3` | `0x80000000` | `0x00000001` | `0xC0000000` | `0x00000044` | `0x80FF7F01` | Run |
| `OR` | `0x007362B3` | `0x000000A0` | `0x0000000F` | `0x000000AF` | `0x00000044` | `0x80FF7F01` | Run |
| `AND` | `0x007372B3` | `0x000000AA` | `0x0000000F` | `0x0000000A` | `0x00000044` | `0x80FF7F01` | Run |
| `ADDI` | `0xFFF30293` | `0x00000000` | `0x00000000` | `0xFFFFFFFF` | `0x00000044` | `0x80FF7F01` | Run |
| `SLTI` | `0x00032293` | `0xFFFFFFFF` | `0x00000000` | `0x00000001` | `0x00000044` | `0x80FF7F01` | Run |
| `SLTIU` | `0xFFF33293` | `0xFFFFFFFE` | `0x00000000` | `0x00000001` | `0x00000044` | `0x80FF7F01` | Run |
| `XORI` | `0xFFF34293` | `0x12345678` | `0x00000000` | `0xEDCBA987` | `0x00000044` | `0x80FF7F01` | Run |
| `ORI` | `0x80036293` | `0x00000001` | `0x00000000` | `0xFFFFF801` | `0x00000044` | `0x80FF7F01` | Run |
| `ANDI` | `0x7FF37293` | `0x12345678` | `0x00000000` | `0x00000678` | `0x00000044` | `0x80FF7F01` | Run |
| `SLLI` | `0x01F31293` | `0x00000001` | `0x00000000` | `0x80000000` | `0x00000044` | `0x80FF7F01` | Run |
| `SRLI` | `0x01F35293` | `0x80000000` | `0x00000000` | `0x00000001` | `0x00000044` | `0x80FF7F01` | Run |
| `SRAI` | `0x41F35293` | `0x80000000` | `0x00000000` | `0xFFFFFFFF` | `0x00000044` | `0x80FF7F01` | Run |
| `LB` | `0x00230283` | `0x00000100` | `0x00000000` | `0xFFFFFFFF` | `0x00000044` | `0x80FF7F01` | Run |
| `LH` | `0x00231283` | `0x00000100` | `0x00000000` | `0xFFFF80FF` | `0x00000044` | `0x80FF7F01` | Run |
| `LW` | `0x00032283` | `0x00000100` | `0x00000000` | `0x80FF7F01` | `0x00000044` | `0x80FF7F01` | Run |
| `LBU` | `0x00234283` | `0x00000100` | `0x00000000` | `0x000000FF` | `0x00000044` | `0x80FF7F01` | Run |
| `LHU` | `0x00235283` | `0x00000100` | `0x00000000` | `0x000080FF` | `0x00000044` | `0x80FF7F01` | Run |
| `SB` | `0x007300A3` | `0x00000100` | `0x000000AB` | `0x00000000` | `0x00000044` | `0x80FFAB01` | Run |
| `SH` | `0x00731123` | `0x00000100` | `0x00001234` | `0x00000000` | `0x00000044` | `0x12347F01` | Run |
| `SW` | `0x00732023` | `0x00000100` | `0x12345678` | `0x00000000` | `0x00000044` | `0x12345678` | Run |
| `BEQ` | `0x00730463` | `0x00000005` | `0x00000005` | `0x00000000` | `0x00000048` | `0x80FF7F01` | Run |
| `BNE` | `0x00731463` | `0x00000005` | `0x00000005` | `0x00000000` | `0x00000044` | `0x80FF7F01` | Run |
| `BLT` | `0x00734463` | `0xFFFFFFFF` | `0x00000001` | `0x00000000` | `0x00000048` | `0x80FF7F01` | Run |
| `BGE` | `0x00735463` | `0xFFFFFFFF` | `0x00000001` | `0x00000000` | `0x00000044` | `0x80FF7F01` | Run |
| `BLTU` | `0x00736463` | `0xFFFFFFFF` | `0x00000001` | `0x00000000` | `0x00000044` | `0x80FF7F01` | Run |
| `BGEU` | `0x00737463` | `0xFFFFFFFF` | `0x00000001` | `0x00000000` | `0x00000048` | `0x80FF7F01` | Run |
| `JAL` | `0x008002EF` | `0x00000000` | `0x00000000` | `0x00000044` | `0x00000048` | `0x80FF7F01` | Run |
| `JALR` | `0x003302E7` | `0x00000046` | `0x00000000` | `0x00000044` | `0x00000048` | `0x80FF7F01` | Run |
| `LUI` | `0x123452B7` | `0x00000000` | `0x00000000` | `0x12345000` | `0x00000044` | `0x80FF7F01` | Run |
| `AUIPC` | `0x12345297` | `0x00000000` | `0x00000000` | `0x12345040` | `0x00000044` | `0x80FF7F01` | Run |
| `FENCE` | `0x0FF0000F` | `0x00000000` | `0x00000000` | `0x00000000` | `0x00000044` | `0x80FF7F01` | Run |
| `ECALL` | `0x00000073` | `0x00000000` | `0x00000000` | `0x00000000` | `0x00000040` | `0x80FF7F01` | EnvironmentCall |
| `EBREAK` | `0x00100073` | `0x00000000` | `0x00000000` | `0x00000000` | `0x00000040` | `0x80FF7F01` | Halt |

The table is vertically and horizontally scrollable. These one-step semantic examples have no simulator timestamps: `step()` itself is not clocked. Each program page provides its own timestamped checkpoint table.

### Boundary and exception answer sheet

These rows use the **same independent initial state** as the 40 examples above. Trap names replace `Run`/`Halt` in the status column. In particular, the last JALR row succeeds now; its next fetch would fail. A reserved-field FENCE is accepted, while an all-zero instruction is not.

<!-- rv32i-edge-examples -->
| Instruction | Word | A | B | Expected x5 | Expected PC | Expected memory[0x100] | Status |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `JAL` | `0x002002EF` | `0x00000000` | `0x00000000` | `0x00000000` | `0x00000040` | `0x80FF7F01` | InstructionAddressMisaligned |
| `JALR` | `0x000302E7` | `0x00000046` | `0x00000000` | `0x00000000` | `0x00000040` | `0x80FF7F01` | InstructionAddressMisaligned |
| `BEQ` | `0x00730163` | `0x00000001` | `0x00000001` | `0x00000000` | `0x00000040` | `0x80FF7F01` | InstructionAddressMisaligned |
| `BNE` | `0x00731163` | `0x00000001` | `0x00000001` | `0x00000000` | `0x00000044` | `0x80FF7F01` | Run |
| `LW` | `0x00032283` | `0x00000101` | `0x00000000` | `0x00000000` | `0x00000040` | `0x80FF7F01` | LoadAddressMisaligned |
| `LW` | `0x00032283` | `0x00040000` | `0x00000000` | `0x00000000` | `0x00000040` | `0x80FF7F01` | LoadAccessFault |
| `SW` | `0x00732023` | `0x00000102` | `0x11223344` | `0x00000000` | `0x00000040` | `0x80FF7F01` | StoreAddressMisaligned |
| `SW` | `0x00732023` | `0x00040000` | `0x11223344` | `0x00000000` | `0x00000040` | `0x80FF7F01` | StoreAccessFault |
| `LB` | `0x00030283` | `0x00040000` | `0x00000000` | `0x00000000` | `0x00000040` | `0x80FF7F01` | LoadAccessFault |
| `ADDI` | `0x00130013` | `0x00000005` | `0x00000000` | `0x00000000` | `0x00000044` | `0x80FF7F01` | Run |
| `LW` | `0x00032003` | `0x00040000` | `0x00000000` | `0x00000000` | `0x00000040` | `0x80FF7F01` | LoadAccessFault |
| `JALR` | `0x000302E7` | `0x00040000` | `0x00000000` | `0x00000044` | `0x00040000` | `0x80FF7F01` | Run |
| `FENCE` | `0xFFF3028F` | `0x00000001` | `0x00000002` | `0x00000000` | `0x00000044` | `0x80FF7F01` | Run |
| `INVALID` | `0x00000000` | `0x00000000` | `0x00000000` | `0x00000000` | `0x00000040` | `0x80FF7F01` | IllegalInstruction |

## Validation boundary

| Test / source | What it checks |
| --- | --- |
| `RV32IReferenceDocumentationTest` | All 40 entry layouts, bit-field diagrams, masks and visible examples; 137,728 decoder inputs; 693 executions of the documented operation blocks against C++; the explicit one-step and arithmetic expectations; compilation/output of the reference usage example; all 22 program images, setup data, expected states, and store bytes; source inclusion and deferred core pin notes |
| `RV32IDocumentationBrowserTest.py` | All 40 instruction anchors across ten family pages, rendered source listings, table scrolling and mobile widths, and all 650 checkpoints in the 22 embedded programs |
| `RV32IDecoderTest` | Field extraction, all 40 forms, immediate boundaries, invalid encodings, and disassembly |
| `RV32IControlTest` | Operand-use flags and ALU/memory/control-flow/writeback controls |
| `RV32IInstructionOracleTest` | Instruction semantics, memory effects, arithmetic/shift boundaries, control-flow alignment, trap precision, `x0`, and execution limits |
| `RV32ISingleCycleCoreTest` | Registered structural/behavioral pin contract and final architectural observations |
| `RV32ISingleCycleSystemTest/program-01` through `program-22` | Program fixtures, instruction-level comparisons, and explicit final outcomes; the first 16 are correctness programs and the final six are workload/performance programs |
| `RV32IFiveStageCoreProgramTest/program-01` through `program-22` | The same architectural program contracts through the pipeline; timing is separately evaluated |
| `RV32IExternalValidationSmokeTest` and external tooling | ELF execution and independently produced program expectations; see the [external validation report](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/docs/rv32i-external-validation-report.md) for its measured scope and limits |

Program pages retain the fixtures' explicit final-state assertions and full instruction/data images; documentation tests compare them with the compiled fixtures and run the reference against those assertions. The per-instruction checkpoint details are oracle-derived expectations; the exporter verifies their PC/status against recorded WASM pins. Register and memory effects are checked by the native program contract tests, not by inventing assertions from observed values. The behavioral core itself calls the oracle, so agreement between them is **not independent ISA validation**. Structural execution, explicit expected vectors/final states, and external tools supply additional evidence. None of these finite suites is a proof over all `2^32` encodings or all possible programs.

Source files: [program fixtures](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/tests/RV32IProgramScenarios.cpp), [single-cycle program runner](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/tests/RV32ISingleCycleTests.cpp), [instruction lockstep](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/tests/RV32IInstructionLockstepTests.cpp), and [test registry](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/tests/TestRegistry.cpp).

Run the focused reference checks from the repository root after building:

```sh
ctest --test-dir build --output-on-failure -R '^(RV32IReferenceDocumentationTest|RV32IDecoderTest|RV32IControlTest|RV32IInstructionOracleTest|RV32ISingleCycleCoreTest|RV32ISingleCycleSystemTest/program-[0-9]+)$'
```

When a circuit and this answer sheet disagree, first check the instruction encoding, old-state operands, environment policy, and observation boundary against the specification. Add a concrete regression for the discrepancy; neither a familiar diagram nor a matching behavioral trace makes an incorrect architectural rule correct.

## Additional arithmetic examples

These preserved examples use independent initial register values `x6=10`, `x7=3` and destination `x5`; PC starts at `0x40`. Numbers without `0x` are decimal. Each successful result advances PC to `0x44` and changes no memory. These examples are checked along with the per-instruction entries.

<details class="note" markdown="1">
<summary>Ten register examples and nine immediate examples</summary>

<!-- rv32i-register-reference -->
| Instruction | Value written to `rd` | Example and result |
| --- | --- | --- |
| `ADD rd, rs1, rs2` | First source plus second source | `add x5, x6, x7` → `x5=13` |
| `SUB rd, rs1, rs2` | First source minus second source | `sub x5, x6, x7` → `x5=7` |
| `SLL rd, rs1, rs2` | First source shifted left; zeros enter the low bits | `sll x5, x6, x7` → `x5=80` |
| `SLT rd, rs1, rs2` | `1` if first source is less than second, **signed**; otherwise `0` | `slt x5, x6, x7` → `x5=0` |
| `SLTU rd, rs1, rs2` | Same less-than test, **unsigned** | `sltu x5, x6, x7` → `x5=0` |
| `XOR rd, rs1, rs2` | Bitwise exclusive OR of the sources | `xor x5, x6, x7` → `x5=9` |
| `SRL rd, rs1, rs2` | First source shifted right; **zeros** enter the high bits | `srl x5, x6, x7` → `x5=1` |
| `SRA rd, rs1, rs2` | First source shifted right; **copies of its sign bit** enter the high bits | `sra x5, x6, x7` → `x5=1` |
| `OR rd, rs1, rs2` | Bitwise OR of the sources | `or x5, x6, x7` → `x5=11` |
| `AND rd, rs1, rs2` | Bitwise AND of the sources | `and x5, x6, x7` → `x5=2` |

<!-- rv32i-immediate-reference -->
| Instruction | Value written to `rd` | Example and result |
| --- | --- | --- |
| `ADDI rd, rs1, imm12` | Source plus immediate | `addi x5, x6, -1` → `x5=9` |
| `SLTI rd, rs1, imm12` | `1` if source is less than immediate, **signed**; otherwise `0` | `slti x5, x6, 11` → `x5=1` |
| `SLTIU rd, rs1, imm12` | Same less-than test, **unsigned after sign extension** | `sltiu x5, x6, -1` → `x5=1` |
| `XORI rd, rs1, imm12` | Bitwise exclusive OR with the extended immediate | `xori x5, x6, -1` → `x5=0xFFFFFFF5` |
| `ORI rd, rs1, imm12` | Bitwise OR with the extended immediate | `ori x5, x6, 5` → `x5=15` |
| `ANDI rd, rs1, imm12` | Bitwise AND with the extended immediate | `andi x5, x6, 7` → `x5=2` |
| `SLLI rd, rs1, shamt` | Source shifted left by `shamt`; zeros enter low bits | `slli x5, x6, 3` → `x5=80` |
| `SRLI rd, rs1, shamt` | Source shifted right by `shamt`; zeros enter high bits | `srli x5, x6, 1` → `x5=5` |
| `SRAI rd, rs1, shamt` | Source shifted right by `shamt`; sign-bit copies enter high bits | `srai x5, x6, 1` → `x5=5` |

</details>
