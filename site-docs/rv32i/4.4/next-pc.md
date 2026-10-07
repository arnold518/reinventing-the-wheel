# § 4.4.6. Next PC

`RV32ISingleCycleNextPCStage` chooses fallthrough or a control-transfer target and reports a selected target's alignment. In the core it is named `NEXT_PC`. The PC register itself belongs to [Fetch](fetch.md).

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| `RV32ISingleCycleNextPCStage` | Next-address selection and qualified target-alignment check | Yes | Yes |

## 4.4.6.1. `RV32ISingleCycleNextPCStage`

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `PC_PLUS_4` | Input | 32 | Fallthrough address from Fetch |
| `CONTROL_TARGET` | Input | 32 | Branch/JAL/JALR candidate from Execute |
| `BRANCH_TAKEN` | Input | 1 | Selected branch condition |
| `JUMP_TYPE` | Input | 2 | Any nonzero code requests a transfer |
| `NEXT_PC` | Output | 32 | Transfer target if taken, otherwise PC_PLUS_4 |
| `TARGET_MISALIGNED` | Output | 1 | Selected transfer target has either low address bit set |

### Behavior

`taken = BRANCH_TAKEN OR (JUMP_TYPE != 0)`. NEXT_PC selects CONTROL_TARGET when taken and PC_PLUS_4 otherwise. TARGET_MISALIGNED is `taken AND ((CONTROL_TARGET & 3) != 0)`.

A misaligned candidate does not trap when a branch is **not taken**. This stage does not check the current PC, recalculate a target, or mask JALR bit 0: Fetch checks the current PC and Execute masks the JALR target. Clearing JALR bit 0 can still leave bit 1 set, which is misaligned for the project's four-byte instructions.

The two-bit code 3 also selects a transfer, although the decoder never emits it and Execute supplies target zero for it. No instruction legality or PC-write authorization is inferred here. Even a misaligned selected target appears on NEXT_PC; execution status suppresses PC capture and records the fault.

#### C++ example

```cpp
#include <cstdint>
namespace single_cycle_example {
struct NextPCInputs {
    uint32_t PC_PLUS_4, CONTROL_TARGET;
    bool BRANCH_TAKEN;
    unsigned JUMP_TYPE;
};
struct NextPCOutputs { uint32_t NEXT_PC; bool TARGET_MISALIGNED; };
NextPCOutputs next_pc(const NextPCInputs& i) {
    const bool taken=i.BRANCH_TAKEN || i.JUMP_TYPE!=0;
    return {taken ? i.CONTROL_TARGET : i.PC_PLUS_4,
            taken && (i.CONTROL_TARGET&3)!=0};
}
}
```

#### Unknown inputs and timing

The structural implementation follows its OR/AND gates, mux, and alignment checker. The behavioral implementation emits X on both outputs if JUMP_TYPE or BRANCH_TAKEN is nonbinary. With known controls it forwards the selected data vector; TARGET_MISALIGNED is zero for an unselected transfer, or X if the selected target word contains any nonbinary bit. Structural alignment only depends on the low two target bits. Equivalence is binary settled behavior, not identical X/Z resolution or path timing. There is no stored state.

### Implementation

#### Structural implementation

`JUMP_BITS` splits the jump code, `JUMPING` ORs its bits, and `TAKEN` includes the branch condition. `NEXT_PC_SELECT` selects the word. `TARGET_LOW_BITS` extracts target bits 0 and 1; `TARGET_UNALIGNED` ORs them and `TAKEN_MISALIGNED` qualifies the check. Instruction alignment is fixed at four bytes, so no general data-size decoder is needed. These seven children contain no PC register or adder.

#### Behavioral implementation

`DirectStage`'s NextPC branch computes selection and qualified alignment directly with one abstract output delay tick. The family is `rv32i.single-cycle.next-pc`.

### Examples

All four jump codes, both branch values, and three targets are combined into 24 rows. In particular, target `0x102` faults only when selected; `0x100` and `0xFFFFFFFC` are aligned.

<!-- simulator-examples: rv32i-single-cycle-next-pc RV32ISingleCycleNextPCStageTest -->

#### Embedded simulation

<div class="simulator-embed"><iframe src="../../../simulator/?scenario=rv32i-single-cycle-next-pc&amp;embed=1" title="RV32ISingleCycleNextPCStage" loading="lazy" allowfullscreen></iframe></div>

<a href="../../../simulator/?scenario=rv32i-single-cycle-next-pc" class="md-button">Open Next PC stage</a>

### Tests

`RV32ISingleCycleNextPCStageTest` compares all 24 rows in both implementations. Program 15 checks that a misaligned jump prevents link/PC updates and causes a precise terminal trap. `RV32ISingleCycleDocumentationTest.py` verifies this pin table and the exact C++ example. Source: `RV32ISingleCycleStages.cpp` and `RV32ISingleCycleStagesDirect.cpp`; scenarios: `src/tests/RV32ISingleCycleStageTests.cpp`.
