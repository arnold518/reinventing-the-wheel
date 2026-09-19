# § 4.3.7. Complete Core

`RV32ISingleCycleCore` connects the six stage contracts to execution status and external instruction/data endpoints. Its structural hierarchy is stage-wise; selecting the core's behavioral implementation replaces the whole hierarchy with the compact reference core. Selecting only one stage's behavior keeps the surrounding structural wiring.

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| `RV32ISingleCycleCore` | Non-pipelined RV32I datapath with two universal memory requester ports | Yes | Yes |

## 4.3.7.1. `RV32ISingleCycleCore`

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `CLK`, `RST`, `ENABLE` | Input | 1 each | Clock, active-high reset, permission to execute |
| `IMEM_REQ_READY`, `DMEM_REQ_READY` | Input | 1 each | Endpoint request acceptance permission |
| `IMEM_RSP_VALID`, `DMEM_RSP_VALID` | Input | 1 each | Endpoint response availability |
| `IMEM_RSP_READ_DATA`, `DMEM_RSP_READ_DATA` | Input | 32 each | Instruction word; data word |
| `IMEM_RSP_STATUS`, `DMEM_RSP_STATUS` | Input | 3 each | Zero success; nonzero access fault |
| `IMEM_REQ_VALID`, `DMEM_REQ_VALID` | Output | 1 each | Held requester valid |
| `IMEM_RSP_READY`, `DMEM_RSP_READY` | Output | 1 each | Response consumption/draining permission |
| `IMEM_REQ_OP`, `DMEM_REQ_OP` | Output | 3 each | Instruction read; data read/write |
| `IMEM_REQ_ADDR`, `DMEM_REQ_ADDR` | Output | 32 each | Byte address of the endpoint word transaction |
| `IMEM_REQ_WRITE_DATA`, `DMEM_REQ_WRITE_DATA` | Output | 32 each | Zero for instruction fetch; placed store data |
| `IMEM_REQ_WRITE_MASK`, `DMEM_REQ_WRITE_MASK` | Output | 4 each | Captured instruction/data byte masks |
| `PC` | Output | 32 | Current architectural PC |
| `HALTED`, `TRAPPED` | Output | 1 each | Sticky terminal status |
| `TRAP_CAUSE` | Output | 4 | Project trap code, not privileged mcause |
| `INSTRUCTION_ATTEMPT` | Output | 1 | Readiness-qualified attempt indication before the active edge |

The core exposes no direct memory read/write enable, legacy adapter, or individual register pins. Registers are available through the architectural-state inspection capability. `INSTRUCTION_ATTEMPT` is not a clock or stored instruction count; it can stay high between readiness and its capture edge.

### Behavior

#### Connections

| Producer | Consumer | Purpose |
| --- | --- | --- |
| Fetch.INSTRUCTION | Decode.INSTRUCTION | Keep the current instruction available through completion |
| Fetch.PC / PC_PLUS_4 | Execute / Writeback and Next PC | Old PC, fallthrough, and jump link |
| Decode source values, immediate, selectors | Execute | Select and perform one ALU operation |
| Decode.RS2_VALUE and memory controls | Memory | Store data, width, direction, extension |
| Execute.ALU_RESULT | Memory.ADDR, Writeback, status.ALU_ADDRESS | Effective address or result; status alignment checks |
| Execute.CONTROL_TARGET / BRANCH_TAKEN, Decode.JUMP_TYPE | Next PC | Select an active transfer |
| Memory.READ_DATA | Writeback | Completed load value |
| Writeback.WB_VALUE | Decode.WB_VALUE | Destination-register input |
| Next PC.NEXT_PC | Fetch.NEXT_PC | PC-register input |
| Fetch/Memory.DONE and FAULT | Execution status | Qualify attempts and access faults |
| Fetch.PC_MISALIGNED, Next PC.TARGET_MISALIGNED, decoded intent | Execution status | Current/target alignment, legality, halt/trap decisions |
| Status.PC_WRITE / REGISTER_WRITE | Fetch / Decode | Accept normal architectural updates |
| Status.FETCH_REQUEST_ACTIVE / MEMORY_REQUEST_ACTIVE | Fetch.ISSUE / Memory.ISSUE | Start requests only when permitted |
| Status.INSTRUCTION_ATTEMPT | Both stages' TAKE | Consume responses at the same completion edge |

CLK and RST reach Fetch, Decode, Memory, and execution status. ENABLE reaches status. Memory's diagnostic MISALIGNED is not separately wired to status: status derives the same bad-access condition from ALU_ADDRESS and MEM_SIZE, including invalid size.

#### Completion and stalls

The exact priority rules are in [4.2.7](../4.2/execution-status.md). In short:

1. Fetch captures a request; acceptance and response occur on the endpoint's schedule. PC/register state holds.
2. A matching instruction response supplies Decode and Execute. A load/store starts the data requester; the instruction response stays held throughout that transaction.
3. Once required responses exist, status permits an instruction attempt. A normal attempt enables PC capture and, if decoded intent requests it, a register write. A halt/trap attempt suppresses both and captures terminal status instead.
4. Both requesters see TAKE at that same attempt edge. A consumed store is acknowledged, not read back. Fetch may begin the next instruction only after the previous request is no longer outstanding.

`ENABLE=0` prevents new execution and architectural completion; it does not retract an already presented/accepted transaction. Requesters can continue acceptance/draining bookkeeping. An already accepted store can affect the endpoint while execution is paused. Reset clears PC, registers, status, and requester state, but does not undo writes already applied by external storage.

The core retains its existing EBREAK-as-halt and terminal-trap conventions. There is no trap-handler vector, privilege mode, interrupt machinery, CSR implementation, or return from trap here. Memory faults prevent normal destination/PC updates; the failing instruction's PC remains visible.

#### C++ example

This binary composition uses all six examples in 4.3 plus `status_example::Status` from [4.2.7](../4.2/execution-status.md). Their 4.2 dependencies are unchanged. `observe` only evaluates combinational outputs; `evaluate` applies reset or a clock transition. Call evaluate after inputs change, starting with reset asserted. External endpoints supply the two response interfaces and own their storage.

The important ordering is **observe every input from old state, then update all state owners, then observe the new outputs**. The C++ call order therefore cannot feed a newly written register into the same instruction's PC calculation. This is a binary reference composition, not the production core's event-driven implementation.

```cpp
namespace single_cycle_example {
struct CoreInputs {
    bool CLK, RST, ENABLE;
    bool IMEM_REQ_READY, IMEM_RSP_VALID;
    uint32_t IMEM_RSP_READ_DATA;
    unsigned IMEM_RSP_STATUS;
    bool DMEM_REQ_READY, DMEM_RSP_VALID;
    uint32_t DMEM_RSP_READ_DATA;
    unsigned DMEM_RSP_STATUS;
};
struct CoreOutputs {
    uint32_t PC;
    bool HALTED, TRAPPED;
    unsigned TRAP_CAUSE;
    bool INSTRUCTION_ATTEMPT;
    bool IMEM_REQ_VALID, IMEM_RSP_READY;
    unsigned IMEM_REQ_OP;
    uint32_t IMEM_REQ_ADDR, IMEM_REQ_WRITE_DATA;
    unsigned IMEM_REQ_WRITE_MASK;
    bool DMEM_REQ_VALID, DMEM_RSP_READY;
    unsigned DMEM_REQ_OP;
    uint32_t DMEM_REQ_ADDR, DMEM_REQ_WRITE_DATA;
    unsigned DMEM_REQ_WRITE_MASK;
};
class Core {
    Fetch fetch;
    Decode decode;
    Memory memory;
    status_example::Status status;
    struct Signals {
        FetchInputs fi;
        DecodeInputs di;
        MemoryInputs mi;
        status_example::Inputs si;
        CoreOutputs out;
    };
    Signals settle(const CoreInputs& i) const {
        Signals s{};
        s.fi={i.CLK,i.RST,false,false,false,0,i.IMEM_REQ_READY,
              i.IMEM_RSP_VALID,i.IMEM_RSP_READ_DATA,i.IMEM_RSP_STATUS};
        auto f=fetch.observe(s.fi);
        s.di={f.INSTRUCTION,i.CLK,i.RST,false,0};
        const auto d=decode.observe(s.di);
        const auto e=execute({f.PC,d.RS1_VALUE,d.RS2_VALUE,d.IMM,
            d.ALU_OP,d.ALU_A_SEL,d.ALU_B_SEL,d.BRANCH_TYPE,d.JUMP_TYPE});
        s.mi={i.CLK,i.RST,false,false,d.MEM_WRITE,d.LOAD_SIGN_EXTEND,
              e.ALU_RESULT,d.RS2_VALUE,d.MEM_SIZE,i.DMEM_REQ_READY,
              i.DMEM_RSP_VALID,i.DMEM_RSP_READ_DATA,i.DMEM_RSP_STATUS};
        auto m=memory.observe(s.mi);
        const auto w=writeback({e.ALU_RESULT,m.READ_DATA,f.PC_PLUS_4,d.WRITEBACK_SEL});
        const auto n=next_pc({f.PC_PLUS_4,e.CONTROL_TARGET,e.BRANCH_TAKEN,d.JUMP_TYPE});
        s.si={i.CLK,i.RST,i.ENABLE,d.LEGAL,d.REG_WRITE,d.MEM_READ,d.MEM_WRITE,
              d.HALT_REQUEST,d.TRAP_REQUEST,d.DECODE_TRAP_CAUSE,e.ALU_RESULT,d.MEM_SIZE,
              f.PC_MISALIGNED,n.TARGET_MISALIGNED,f.DONE,f.FAULT,m.DONE,m.FAULT};
        const auto c=status.observe(s.si);
        s.fi.ISSUE=c.FETCH_REQUEST_ACTIVE;
        s.fi.TAKE=c.INSTRUCTION_ATTEMPT;
        s.fi.PC_WRITE=c.PC_WRITE;
        s.fi.NEXT_PC=n.NEXT_PC;
        s.di.REGISTER_WRITE=c.REGISTER_WRITE;
        s.di.WB_VALUE=w.WB_VALUE;
        s.mi.ISSUE=c.MEMORY_REQUEST_ACTIVE;
        s.mi.TAKE=c.INSTRUCTION_ATTEMPT;
        // TAKE changes response-ready, but not DONE, payload, or decoded data.
        f=fetch.observe(s.fi);
        m=memory.observe(s.mi);
        s.out={f.PC,c.HALTED,c.TRAPPED,c.TRAP_CAUSE,c.INSTRUCTION_ATTEMPT,
               f.IMEM_REQ_VALID,f.IMEM_RSP_READY,f.IMEM_REQ_OP,f.IMEM_REQ_ADDR,
               f.IMEM_REQ_WRITE_DATA,f.IMEM_REQ_WRITE_MASK,
               m.DMEM_REQ_VALID,m.DMEM_RSP_READY,m.DMEM_REQ_OP,m.DMEM_REQ_ADDR,
               m.DMEM_REQ_WRITE_DATA,m.DMEM_REQ_WRITE_MASK};
        return s;
    }
public:
    CoreOutputs observe(const CoreInputs& i) const { return settle(i).out; }
    CoreOutputs evaluate(const CoreInputs& i) {
        const auto before=settle(i); // Freeze ALL capture inputs before any update.
        fetch.evaluate(before.fi);
        decode.evaluate(before.di);
        memory.evaluate(before.mi);
        status.evaluate(before.si);
        return observe(i);
    }
    const std::array<uint32_t,32>& registers() const { return decode.state(); }
};
}
```

#### Unknown inputs and timing

Use known clock/control signals, reset before execution, and allow structural paths to settle before an edge. The example models the supported binary boundary only; it omits X/Z, simulator histories, and propagation events. Structural and behavioral forms can have different transient outputs and absolute checkpoint times. Several protocol clocks per instruction do not turn this into a six-stage pipeline or a microcoded ALU-reuse machine.

### Implementation

#### Structural implementation

The core has exactly seven direct children: `FETCH`, `DECODE`, `EXECUTE`, `MEMORY`, `WRITEBACK`, `NEXT_PC`, and `EXECUTION_STATUS`. Wires connect their pins as listed above. Architectural-state inspection obtains PC from Fetch, registers through Decode's `register-state-view`, and terminal state from status. The old monolithic `CONTROL_FLOW` block is not instantiated.

The visualizer places `FETCH → DECODE → EXECUTE → MEMORY → WRITEBACK` left to right, with `NEXT_PC` and `EXECUTION_STATUS` in a separate lower row. Writeback returns its value to Decode; Next PC returns the selected address to Fetch. This arrangement shows logical dataflow, not separate clock cycles or pipeline registers. Expanding a stage retains its own internal layout.

#### Behavioral implementation

The core-level family `rv32i.core.educational-single-cycle` selects `RV32IReferenceCore` for behavioral fidelity. It maintains compact architectural/request state and checks readiness before applying the instruction reference transition. It is not six hidden behavioral-stage instances. By contrast, keeping the core structural and selecting all six stages behavioral retains the six public boundaries and shared status wiring; both configurations are tested.

### Examples

| Instruction and old state | Values computed before capture | Completion effect |
| --- | --- | --- |
| `addi x1,x1,2`, x1=512 | ALU=514, fallthrough=PC+4 | x1=514; PC advances |
| `lw x1,0(x1)`, x1=0x100, word at 0x100=512 | Address=0x100; load result=512 after response | x1 remains 0x100 throughout the wait, then becomes 512 |
| `jalr x5,0(x5)`, PC=16, x5=24 | Target=24, link=20 | PC=24 and x5=20 at the same edge |
| `sw x1,0(x1)`, x1=0x404 | Address=0x404 and data=0x404 | Endpoint stores bytes 04 04 00 00; PC advances only after acknowledgment |
| Taken jump to address 0x102 | Misaligned target | Trap; no link-register or PC update |

The overlap cases come from `RV32ISingleCycleStageIntegrationTest`. They illustrate the no-reading-back rule; they are not a second instruction set. The [22-program catalogue](../4.1/programs/index.md) contains the complete existing images, initial data, and expected outcomes rather than duplicating them here.

#### Embedded simulation

This is [program 3](../4.1/programs/program-03.md), with signed/unsigned loads and partial stores, running the **structural core**. Expand each stage or switch its fidelity in the explorer. The timeline contains every instruction-attempt checkpoint for this configuration. The reference program page uses a behavioral core, so its absolute times need not equal this run's times.

<div class="simulator-embed"><iframe src="../../../simulator/?scenario=rv32i-program3&amp;core_fidelity=structural&amp;embed=1" title="Stage-wise RV32I single-cycle core" loading="lazy" allowfullscreen></iframe></div>

<a href="../../../simulator/?scenario=rv32i-program3&amp;core_fidelity=structural" class="md-button">Open complete single-cycle core</a>

#### Memory integration

The program root supplies a clock, this core, `INSTRUCTION_MEMORY`, and `DATA_MEMORY`. Its default memories are [MemoryContainer](../../memory/3.1.6.md) endpoints; expanded device/controller endpoints can use the same [transaction pins](../../memory/3.2.md). There is no core-side legacy memory adapter. Instruction fetch uses read operation 0; the data port uses operation 0 or 1. Flash/DRAM maintenance belongs behind the endpoint, not in this datapath.

`RV32ISingleCycleSystem` is the convenience wrapper with owned memories and external CLK/RST/ENABLE. Program loading and memory inspection are host operations, not instructions. Reset alone does not prepare a stack pointer: program/ABI setup must initialize x2 when a program uses a stack.

### Tests

| Test | Coverage |
| --- | --- |
| Six `RV32ISingleCycle*StageTest` scenarios | Structural/behavioral settled stage contracts |
| `RV32ISingleCycleCoreTest` | Core pin/status execution against the instruction reference |
| `RV32ISingleCycleStageIntegrationTest` | Overlapping operands, JALR link/target, exact store bytes, no early PC/register update; each stage, all stages, whole-core behavior, and structural register-file wiring |
| `RV32ISingleCycleSystemTest/program-01` … `program-22` | Full program checkpoints against the instruction oracle |
| `RV32IProfileToggleSweepTest` | Every selectable family in the canonical structural core tree |
| `RV32IMemoryMigrationTest` | Interchangeable memory endpoints, latency, and faults |
| `RV32ISingleCycleDocumentationTest.py` | Pin schemas, recorded examples, exact C++ stage/composition references |

Stage tests assert exactly one ALU plus one concurrent branch-target adder in Execute. The integration harness compares PC/register state between non-completion clocks; comparison only at final program state would miss early writes. These focused tests do not claim exhaustive instruction-stream, arbitrary X/Z, or physical timing verification.

Source: `src/modules/rv32i/RV32ISingleCycleCore.cpp`, `RV32ISingleCycleStages.cpp`, `RV32ISingleCycleStagesDirect.cpp`, and `RV32IReferenceCore.cpp`. Stage scenarios are in `src/tests/RV32ISingleCycleStageTests.cpp`; program/fidelity tests are in `src/tests/RV32ISingleCycleTests.cpp`.
