# § 4.3.7. Execution Control and Status

`RV32IExecutionControlStatusUnit` qualifies instruction completion in the single-cycle core and remembers halt/trap state. Decoder outputs express intent; this component decides when PC/register updates are permitted after readiness and fault checks. Pipeline retirement uses separate components, documented with the pipeline.

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| `RV32IExecutionControlStatusUnit` | Readiness, update enables, fault priority, sticky halt/trap status | Yes | Yes |

## 4.3.7.1. `RV32IExecutionControlStatusUnit`

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `CLK`, `RST` | Input | 1 each | Rising-edge status capture; asynchronous active-high reset |
| `ENABLE` | Input | 1 | Permit execution while not halted/trapped |
| `LEGAL` | Input | 1 | Decoder recognized instruction |
| `REG_WRITE`, `MEM_READ`, `MEM_WRITE` | Input | 1 each | Decoded update/memory intent |
| `HALT_REQUEST`, `TRAP_REQUEST` | Input | 1 each | Decoded terminal events |
| `DECODE_TRAP_CAUSE` | Input | 4 | Cause forwarded for an explicitly requested trap |
| `ALU_ADDRESS` | Input | 32 | Effective data address |
| `MEM_SIZE` | Input | 2 | 0: byte; 1: halfword; 2: word; 3: invalid |
| `PC_MISALIGNED`, `TARGET_MISALIGNED` | Input | 1 each | Current-PC and transfer-target checks |
| `IMEM_READY`, `IMEM_FAULT` | Input | 1 each | Instruction response completion/fault |
| `DMEM_READY`, `DMEM_FAULT` | Input | 1 each | Data response completion/fault |
| `PC_WRITE`, `REGISTER_WRITE` | Output | 1 each | Normal instruction update enables |
| `MEMORY_REQUEST_ACTIVE` | Output | 1 | Instruction requires a data transaction |
| `FETCH_REQUEST_ACTIVE` | Output | 1 | Core is enabled, out of reset, and not halted/trapped; permits a new fetch |
| `INSTRUCTION_ATTEMPT` | Output | 1 | Instruction reached its readiness point |
| `DATA_ADDRESS_MISALIGNED` | Output | 1 | Alignment/invalid-size predicate, independent of memory intent |
| `HALTED`, `TRAPPED` | Output | 1 each | Sticky status, cleared by reset |
| `TRAP_CAUSE` | Output | 4 | Latched project execution cause |

`IMEM_READY`/`DMEM_READY` mean the CPU has a completed response, not the endpoint's request-acceptance signal. The core connects these to requester completion, not directly to `REQ_READY`.

In the [stage-wise core](../4.4/core.md), Fetch/Memory `DONE` feed those readiness inputs. `PC_WRITE` returns to Fetch, `REGISTER_WRITE` to Decode, the two request-active outputs to their respective requesters, and `INSTRUCTION_ATTEMPT` to both `TAKE` inputs. This shared unit remains outside the six datapath stages.

### Behavior

Execution is active only if reset is low, ENABLE is high, and neither stored terminal flag is set. A data request is active when execution is active, instruction memory is ready, the instruction is legal, PC is aligned, there is no instruction fault/halt/trap request, and MEM_READ or MEM_WRITE is asserted. Data readiness and data alignment do **not** gate this request indication.

`INSTRUCTION_ATTEMPT = active AND IMEM_READY AND (NOT MEMORY_REQUEST_ACTIVE OR DMEM_READY)`. Thus a load/store waits for its data response; an ordinary ALU instruction does not. A normal attempt permits PC_WRITE, and permits REGISTER_WRITE only with REG_WRITE. The requester handles acceptance and duplicate-request prevention; MEMORY_REQUEST_ACTIVE is not a one-cycle pulse or a direct storage write enable.

At an attempt, the first matching row wins:

| Priority | Condition | Event / stored cause |
| ---: | --- | --- |
| 1 | PC_MISALIGNED | Trap 3: instruction address misaligned |
| 2 | IMEM_FAULT | Trap 4: instruction access fault |
| 3 | Not LEGAL, or TRAP_REQUEST | Trap 1 if illegal; otherwise forward DECODE_TRAP_CAUSE |
| 4 | HALT_REQUEST | Halt, no normal updates |
| 5 | Data request and data misaligned | Trap 7 if MEM_WRITE; otherwise 5 |
| 6 | Data request and DMEM_FAULT | Trap 8 if MEM_WRITE; otherwise 6 |
| 7 | TARGET_MISALIGNED | Trap 3 |
| 8 | None above | Normal PC/register update |

Data cause 5/6 means load alignment/access fault; 7/8 means store alignment/access fault. If both memory intents are high, store wins cause selection. Data misalignment is true for reserved size 3, odd halfwords, or non-word-aligned words, even without a memory request. It causes a trap only when qualified by an attempted data operation. Halt/trap events suppress both update enables before the clock edge.

On a rising edge, a trap latches TRAPPED and its cause; otherwise a halt latches HALTED. Status persists even if ENABLE or inputs change. Reset asynchronously clears both flags and cause to zero. This component does not reset the register file, PC, or memory itself. Cause numbers are **project encodings, not privileged RISC-V `mcause` values**. Cause 2 denotes ECALL. A supplied trap cause of zero is still a trap when TRAP_REQUEST is high; arbitrary four-bit requested causes are forwarded unchanged. The instruction-limit status used by the reference harness is not generated here.

#### C++ example

This complete binary reference requires reset before execution. Combinational outputs are recalculated after state capture, so an edge that latches a terminal state returns disabled execution outputs.

`observe` returns the current combinational decision without capturing an edge. `evaluate` additionally applies reset or the clock transition. The complete-core example uses observe to freeze update permissions before any architectural state changes.

```cpp
#include <cstdint>
namespace status_example {
struct Inputs {
    bool CLK, RST, ENABLE, LEGAL, REG_WRITE, MEM_READ, MEM_WRITE, HALT_REQUEST, TRAP_REQUEST;
    unsigned DECODE_TRAP_CAUSE;
    uint32_t ALU_ADDRESS;
    unsigned MEM_SIZE;
    bool PC_MISALIGNED, TARGET_MISALIGNED, IMEM_READY, IMEM_FAULT, DMEM_READY, DMEM_FAULT;
};
struct Outputs {
    bool PC_WRITE, REGISTER_WRITE, MEMORY_REQUEST_ACTIVE, HALTED, TRAPPED;
    bool DATA_ADDRESS_MISALIGNED, INSTRUCTION_ATTEMPT;
    bool FETCH_REQUEST_ACTIVE;
    unsigned TRAP_CAUSE;
};
class Status {
    bool halted=false, trapped=false, previous_clock=false;
    unsigned cause=0;
    struct Decision { Outputs pins; bool halt, trap; unsigned next_cause; };
    Decision decide(const Inputs& i) const {
        const bool mis=i.MEM_SIZE==3 || (i.MEM_SIZE==1 && (i.ALU_ADDRESS&1))
                       || (i.MEM_SIZE==2 && (i.ALU_ADDRESS&3));
        const bool active=!i.RST && i.ENABLE && !halted && !trapped;
        const bool request=active && i.IMEM_READY && i.LEGAL && !i.PC_MISALIGNED
            && !i.IMEM_FAULT && !i.TRAP_REQUEST && !i.HALT_REQUEST && (i.MEM_READ || i.MEM_WRITE);
        const bool attempt=active && i.IMEM_READY && (!request || i.DMEM_READY);
        bool halt=false, trap=false;
        unsigned next=0;
        if (attempt) {
            if (i.PC_MISALIGNED) { trap=true; next=3; }
            else if (i.IMEM_FAULT) { trap=true; next=4; }
            else if (!i.LEGAL || i.TRAP_REQUEST) { trap=true; next=!i.LEGAL ? 1 : i.DECODE_TRAP_CAUSE; }
            else if (i.HALT_REQUEST) halt=true;
            else if (request && mis) { trap=true; next=i.MEM_WRITE ? 7 : 5; }
            else if (request && i.DMEM_FAULT) { trap=true; next=i.MEM_WRITE ? 8 : 6; }
            else if (i.TARGET_MISALIGNED) { trap=true; next=3; }
        }
        const bool normal=attempt && !halt && !trap;
        return {{normal,normal && i.REG_WRITE,request,halted,trapped,mis,attempt,active,cause},halt,trap,next};
    }
public:
    Outputs observe(const Inputs& i) const { return decide(i).pins; }
    Outputs evaluate(const Inputs& i) {
        if (i.RST) { halted=trapped=false; cause=0; }
        else if (!previous_clock && i.CLK) {
            const auto event=decide(i);
            if (event.trap) { trapped=true; cause=event.next_cause; }
            else if (event.halt) halted=true;
        }
        previous_clock=i.CLK;
        return decide(i).pins;
    }
};
}
```

#### Unknown inputs and timing

The initial status bits/cause are unknown until reset. The structural implementation follows its gates and FF cells. The behavioral implementation requires binary combinational inputs and known stored halt/trap bits; otherwise its completion/fault decision outputs become `X` and it does not generate a halt/trap event. `FETCH_REQUEST_ACTIVE` depends only on reset, enable, and stored halt/trap bits; it is high only when those explicitly permit execution, and low otherwise. Unknown reset poisons stored status. Its registered equivalence scope is binary behavior after reset, not identical four-state resolution. Inputs must settle before a rising edge; behavioral outputs have one abstract delay tick, while structural outputs include gate delays.

### Implementation

#### Structural implementation

`HALTED_STATE` and `TRAPPED_STATE` retain sticky flags; `TRAP_CAUSE_WORD` is a four-bit payload register with a shared trap-capture enable. Address/size gates implement alignment. Balanced AND/OR reductions qualify active execution and combine events; cause muxes enforce the documented priority. Reset reaches the storage directly. State storage remains separate from the CPU datapath.

Each `PRIORITY_*` child is a fixed structural `RV32IStatusPriority` with one-bit inputs `PERMISSION`, `CONDITION` and outputs `EVENT`, `ALLOW_NEXT`. Its three gates implement `EVENT = PERMISSION & CONDITION` and `ALLOW_NEXT = PERMISSION & !CONDITION`. The seven children appear in priority order from top to bottom: PC alignment, instruction access, decode trap, halt, data alignment, data access, target alignment. These are combinational links, not clocked pipeline stages. Cause selection sits alongside them, with the retained status cells below.

#### Behavioral implementation

`RV32IExecutionStatusBehavioral` computes the same priority decision, updates four-state stored status at reset/edges, and recomputes outputs. Both forms are registered as `rv32i.execution-status`.

### Examples

<!-- simulator-examples: rv32i-execution-status RV32IExecutionControlStatusUnitTest -->

#### Embedded simulation

<div class="simulator-embed"><iframe src="../../../simulator/?scenario=rv32i-execution-status&amp;embed=1" title="RV32IExecutionControlStatusUnit" loading="lazy" allowfullscreen></iframe></div>

<a href="../../../simulator/?scenario=rv32i-execution-status" class="md-button">Open execution status</a>

### Tests

`RV32IExecutionControlStatusUnitTest` checks readiness stalls, normal writes, fault priorities, sticky status, and reset in both fidelities. `RV32IBuildingBlocksDocumentationTest` compiles the exact example and replays the declared binary actions. Source: `src/modules/rv32i/RV32IExecutionControlStatusUnit.cpp` and `RV32IExecutionStatusBehavioral.cpp`; scenarios: `src/tests/RV32IBlockStandaloneTests.cpp`.
