# § 4.4.3. Execute

`RV32ISingleCycleExecuteStage` selects the ALU operands, computes the arithmetic/address result, evaluates a branch, and supplies the control-transfer target. In the core it is named `EXECUTE`. It is combinational: it neither stores operands nor updates architectural state.

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| `RV32ISingleCycleExecuteStage` | Operand muxes, one shared ALU, branch condition, branch/jump target selection | Yes | Yes |

## 4.4.3.1. `RV32ISingleCycleExecuteStage`

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `PC` | Input | 32 | Current instruction's old PC |
| `RS1_VALUE`, `RS2_VALUE`, `IMM` | Input | 32 each | Register operands and decoded immediate |
| `ALU_OP` | Input | 5 | [ALU32](../4.3/alu32.md) opcode |
| `ALU_A_SEL`, `ALU_B_SEL` | Input | 2 each | Operand selectors, defined below |
| `BRANCH_TYPE` | Input | 3 | Branch-condition selector |
| `JUMP_TYPE` | Input | 2 | Target selector: 0 branch, 1 JAL, 2 JALR, 3 zero |
| `ALU_RESULT` | Output | 32 | Shared ALU result |
| `CONTROL_TARGET` | Output | 32 | Candidate transfer target, before Next PC selection |
| `BRANCH_TAKEN` | Output | 1 | Selected ALU comparison condition |

### Behavior

| Selector | 0 | 1 | 2 | 3 |
| --- | --- | --- | --- | --- |
| `ALU_A_SEL` | RS1_VALUE | PC | Zero | Zero |
| `ALU_B_SEL` | RS2_VALUE | IMM | Zero | Zero |
| `JUMP_TYPE` → CONTROL_TARGET | PC+IMM | ALU_RESULT | ALU_RESULT with bit 0 cleared | Zero |

All additions wrap to 32 bits. ALU operation codes 0–12 and reserved 13–31 retain [ALU32's complete contract](../4.3/alu32.md): reserved operations produce result zero. `ALU_RESULT` is not replaced by the branch target when branching, and a jump's link value is not computed here.

| BRANCH_TYPE | BRANCH_TAKEN |
| ---: | --- |
| 0, 7 | False |
| 1 | EQ |
| 2 | NOT EQ |
| 3 | LT_SIGNED |
| 4 | NOT LT_SIGNED |
| 5 | LT_UNSIGNED |
| 6 | NOT LT_UNSIGNED |

These flags compare the **selected ALU operands**, not necessarily the original RS1/RS2 pins. EQ/LT flags are enabled only for SUB, SLT, and SLTU. Incoherent manually supplied controls still follow the table: for example, inverted comparison flags can be true when the underlying flags are disabled. The instruction decoder selects SUB and register operands for every legal branch.

#### Shared arithmetic

| Instruction group | Main ALU work | Other arithmetic needed at the same time |
| --- | --- | --- |
| Register/immediate ALU, LUI, AUIPC | Selected operation | Fetch computes PC+4 |
| Load/store | RS1+IMM | Fetch computes PC+4 |
| Conditional branch | RS1−RS2, including signed/unsigned flags | Branch-target adder computes PC+IMM; Fetch computes PC+4 |
| JAL | PC+IMM | Fetch computes PC+4 for the link |
| JALR | RS1+IMM; target bit 0 is then cleared | Fetch computes PC+4 for the link |

The branch adder remains combinational hardware even during other instructions, but its result is selected only for `JUMP_TYPE=0`. There is no dedicated RS1+IMM adder. Next PC chooses target versus fallthrough and checks alignment; execution status decides whether to capture anything.

#### C++ example

Reuse the complete `alu32_example::alu` model from [4.3.3](../4.3/alu32.md), including its comparison-flag qualifications. Each unsigned selector is supplied within its pin width. The single ALU call represents sharing, not six successive uses of a clocked ALU.

```cpp
#include <cstdint>
namespace single_cycle_example {
struct ExecuteInputs {
    uint32_t PC, RS1_VALUE, RS2_VALUE, IMM;
    unsigned ALU_OP, ALU_A_SEL, ALU_B_SEL, BRANCH_TYPE, JUMP_TYPE;
};
struct ExecuteOutputs { uint32_t ALU_RESULT, CONTROL_TARGET; bool BRANCH_TAKEN; };
ExecuteOutputs execute(const ExecuteInputs& i) {
    const uint32_t a=i.ALU_A_SEL==0 ? i.RS1_VALUE : i.ALU_A_SEL==1 ? i.PC : 0;
    const uint32_t b=i.ALU_B_SEL==0 ? i.RS2_VALUE : i.ALU_B_SEL==1 ? i.IMM : 0;
    const auto r=alu32_example::alu(a,b,i.ALU_OP);
    const bool conditions[]{false,r.EQ,!r.EQ,r.LT_SIGNED,!r.LT_SIGNED,
                            r.LT_UNSIGNED,!r.LT_UNSIGNED,false};
    const uint32_t branch_target=i.PC+i.IMM;
    const uint32_t target=i.JUMP_TYPE==0 ? branch_target
        : i.JUMP_TYPE==1 ? r.OUT : i.JUMP_TYPE==2 ? r.OUT&~uint32_t{1} : 0;
    return {r.OUT,target,conditions[i.BRANCH_TYPE]};
}
}
```

#### Unknown inputs and timing

Structural muxes/gates may isolate unused unknown data. The behavioral stage instead sets all three outputs to X if **any input bit** is nonbinary, even an unused operand or selector. Equivalence therefore covers settled binary inputs only. There is no clock or stored state. The catalog-selected compact stage uses gate-derived delays, while its settled-value computation does not reproduce intermediate gate glitches.

### Implementation

#### Structural implementation

`OPERAND_A` and `OPERAND_B` feed one `ALU32Core` child named `ALU`. Its EQ/LT outputs and three inversions feed `BRANCH_CONDITION`. The general `ALU32` flag block is absent here, but the comparison network retains the internal arithmetic carry and overflow it needs. `BRANCH_TARGET_ADD` computes PC+IMM. `JALR_MASK` is a rewire clearing bit 0 of the ALU result; `TARGET_SELECT` chooses branch/JAL/JALR/zero. Constants supply zero words and low control inputs.

#### Behavioral implementation

`BehavioralStage`'s Execute branch computes the selected operation, comparison qualification, branch mux, and target selection directly, without gate objects or an instruction oracle. The family is `rv32i.single-cycle.execute`.

### Examples

The recorded run covers all 32 ALU opcodes for a fixed operand pair, all eight branch selectors, signed versus unsigned comparison, equality, wrapped targets, JAL sharing, and JALR masking. For JALR, `0x101+4` yields ALU result `0x105` but target `0x104`.

<!-- simulator-examples: rv32i-single-cycle-execute RV32ISingleCycleExecuteStageTest -->

#### Embedded simulation

<div class="simulator-embed"><iframe src="../../../simulator/?scenario=rv32i-single-cycle-execute&amp;embed=1" title="RV32ISingleCycleExecuteStage" loading="lazy" allowfullscreen></iframe></div>

<a href="../../../simulator/?scenario=rv32i-single-cycle-execute" class="md-button">Open Execute stage</a>

### Tests

`RV32ISingleCycleExecuteStageTest` checks both implementations; these are representative operand values, not an exhaustive 64-bit operand space. `RV32ISingleCycleStageIntegrationTest` asserts one main ALU and one branch adder, and verifies JALR when its destination is also its source. `RV32ISingleCycleDocumentationTest.py` compiles/replays this example. Source: `RV32ISingleCycleStages.cpp` and `RV32ISingleCycleStagesBehavioral.cpp`; scenarios: `src/tests/RV32ISingleCycleStageTests.cpp`.
