# § 4.3.5. Program Counter and Control Flow

`RV32IControlFlowUnit` is the standalone, combined PC/control-flow building block. It stores a PC and computes sequential, branch, and jump candidates. It does not decode instructions or decide whether an instruction may commit.

`RV32IControlFlowUnit` is a standalone combined reference block. In the stage-wise `RV32ISingleCycleCore`, [Fetch](../4.4/fetch.md) owns PC and PC+4, [Execute](../4.4/execute.md) computes comparisons and targets using the shared ALU, and [Next PC](../4.4/next-pc.md) selects the next address. The combined block's dedicated jump adders are part of its standalone circuit, not the stage-wise core.

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| `RV32IControlFlowUnit` | PC register, branch selection, jump targets, and alignment indications | Yes | Yes |

## 4.3.5.1. `RV32IControlFlowUnit`

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `CLK` | Input | 1 | PC captures on a low-to-high edge |
| `RST` | Input | 1 | Asynchronous active-high reset to PC=0 |
| `PC_WRITE` | Input | 1 | Permit capture of the candidate |
| `RS1_VALUE`, `IMM` | Input | 32 each | Register value and decoded immediate |
| `BRANCH_TYPE` | Input | 3 | Branch comparison selector |
| `JUMP_TYPE` | Input | 2 | Jump selector; overrides branch candidate |
| `EQ`, `LT_SIGNED`, `LT_UNSIGNED` | Input | 1 each | Comparisons from ALU subtraction |
| `PC` | Output | 32 | Stored instruction address |
| `PC_PLUS_4` | Output | 32 | Sequential address and link value |
| `NEXT_PC_CANDIDATE` | Output | 32 | Selected next address, whether or not it will be captured |
| `BRANCH_TAKEN` | Output | 1 | Branch condition alone, independent of jump selection |
| `PC_MISALIGNED` | Output | 1 | Current PC has a nonzero low two bits |
| `TARGET_MISALIGNED` | Output | 1 | Active branch/jump candidate has a nonzero low two bits |

### Behavior

| BRANCH_TYPE | Condition |
| ---: | --- |
| 0, 7 | False |
| 1 / 2 | EQ / not EQ |
| 3 / 4 | LT_SIGNED / not LT_SIGNED |
| 5 / 6 | LT_UNSIGNED / not LT_UNSIGNED |

| JUMP_TYPE | Candidate |
| ---: | --- |
| 0 | Taken branch: PC+IMM; otherwise PC+4 |
| 1 | PC+IMM (JAL) |
| 2 | `(RS1_VALUE + IMM) & ~1` (JALR) |
| 3 | Zero (reserved control encoding) |

All address arithmetic wraps to 32 bits. JALR clears bit 0, **not** bit 1. RV32I here requires four-byte instruction alignment. `TARGET_MISALIGNED` is checked only for an active transfer (`BRANCH_TAKEN` or nonzero `JUMP_TYPE`); an untaken branch's offset cannot cause it.

The block reports misalignment but does not prevent an update itself. [Execution control](execution-status.md) must suppress `PC_WRITE` when necessary. A falling edge, a stable high clock, or `PC_WRITE=0` leaves PC unchanged. Reset overrides all of these.

#### C++ example

This binary reference requires reset before use, just as the initially unknown simulator PC does. Call `evaluate` for input changes; its returned combinational outputs reflect the updated PC after an edge.

```cpp
#include <array>
#include <cstdint>
namespace flow_example {
struct Inputs {
    bool CLK, RST, PC_WRITE;
    uint32_t RS1_VALUE, IMM;
    unsigned BRANCH_TYPE, JUMP_TYPE;
    bool EQ, LT_SIGNED, LT_UNSIGNED;
};
struct Outputs {
    uint32_t PC, PC_PLUS_4, NEXT_PC_CANDIDATE;
    bool BRANCH_TAKEN, PC_MISALIGNED, TARGET_MISALIGNED;
};
class ControlFlow {
    uint32_t pc=0; // Binary example's reset state; hardware starts unknown.
    bool previous_clock=false;
    Outputs combinational(const Inputs& i) const {
        const std::array<bool,8> branches{false,i.EQ,!i.EQ,i.LT_SIGNED,!i.LT_SIGNED,i.LT_UNSIGNED,!i.LT_UNSIGNED,false};
        const bool branch=branches.at(i.BRANCH_TYPE);
        const std::array<uint32_t,4> candidates{
            branch ? pc+i.IMM : pc+4U, pc+i.IMM, (i.RS1_VALUE+i.IMM)&~uint32_t{1}, 0};
        const uint32_t next=candidates.at(i.JUMP_TYPE);
        return {pc,pc+4U,next,branch,bool(pc&3),bool((branch || i.JUMP_TYPE!=0) && (next&3))};
    }
public:
    Outputs evaluate(const Inputs& i) {
        if (i.RST) pc=0;
        else if (!previous_clock && i.CLK && i.PC_WRITE) pc=combinational(i).NEXT_PC_CANDIDATE;
        previous_clock=i.CLK;
        return combinational(i);
    }
};
}
```

#### Unknown inputs and timing

PC initially contains `X`. The behavioral implementation clears it on reset, poisons it on unknown reset, and captures unknown on a rising edge with unknown write enable. With known PC it can still produce PC+4 and current-PC alignment when other controls are unknown, but its branch/candidate/target outputs require all their input words and comparisons to be binary. Structural gates can resolve some of these cases more precisely. Equivalence is specified for binary inputs after reset, not identical `X` propagation or delays. Inputs must settle before the active clock edge.

### Implementation

#### Structural implementation

`PC_STATE` is [`Register<32>`](register.md); its storage cells are direct children of that component. Three `Adder32` blocks form PC+4, PC+IMM, and RS1+IMM in this standalone reference unit. `JALR_MASK` clears bit 0. A branch-condition mux and word muxes choose the candidate; splitter/OR networks report alignment. This single-word PC register is distinct from the two-read-port architectural register file.

#### Behavioral implementation

`RV32IControlFlowBehavioral` stores a 32-bit four-state PC vector and previous clock level, updates PC at reset/edges, then recomputes outputs. It has one abstract delay tick and is registered under `rv32i.control-flow`.

### Examples

<!-- simulator-examples: rv32i-control-flow RV32IControlFlowUnitTest -->

#### Embedded simulation

<div class="simulator-embed"><iframe src="../../../simulator/?scenario=rv32i-control-flow&amp;embed=1" title="RV32IControlFlowUnit" loading="lazy" allowfullscreen></iframe></div>

<a href="../../../simulator/?scenario=rv32i-control-flow" class="md-button">Open control flow</a>

### Tests

`RV32IControlFlowUnitTest` exercises reset, sequential capture, branch/jump targets, holds, and alignment in both fidelities. `RV32IBuildingBlocksDocumentationTest` replays its actions through the exact C++ example. Source: `src/modules/rv32i/RV32IControlFlowUnit.cpp` and `RV32IControlFlowBehavioral.cpp`; scenarios: `src/tests/RV32IBlockStandaloneTests.cpp`.
