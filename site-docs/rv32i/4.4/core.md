# § 4.4.7. Complete Core

`RV32ISingleCycleCore` connects the six stage contracts to execution status and external zero-wait instruction/data endpoints. Its structural hierarchy is stage-wise; selecting the core's behavioral implementation replaces the whole hierarchy with the compact reference core. Selecting only one stage's behavior keeps the surrounding structural wiring.

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| `RV32ISingleCycleCore` | Single-cycle RV32I datapath with universal zero-wait instruction/data memory ports | Yes | Yes |

## 4.4.7.1. `RV32ISingleCycleCore`

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `CLK`, `RST`, `ENABLE` | Input | 1 each | Clock, active-high reset, permission to execute |
| `IMEM_REQ_READY`, `DMEM_REQ_READY` | Input | 1 each | Endpoint can accept the request on the next edge |
| `IMEM_RSP_VALID`, `DMEM_RSP_VALID` | Input | 1 each | Endpoint response is present during this cycle |
| `IMEM_RSP_READ_DATA`, `DMEM_RSP_READ_DATA` | Input | 32 each | Instruction/data response |
| `IMEM_RSP_STATUS`, `DMEM_RSP_STATUS` | Input | 3 each | Zero success; nonzero access error |
| `IMEM_REQ_ADDR`, `DMEM_REQ_ADDR` | Output | 32 each | Aligned byte addresses |
| `IMEM_REQ_OP`, `DMEM_REQ_OP` | Output | 3 each | READ=0; data WRITE=1 |
| `IMEM_REQ_VALID`, `DMEM_REQ_VALID` | Output | 1 each | Active access |
| `IMEM_REQ_WRITE_DATA`, `DMEM_REQ_WRITE_DATA` | Output | 32 each | Positioned store data; instruction port zero |
| `IMEM_REQ_WRITE_MASK`, `DMEM_REQ_WRITE_MASK` | Output | 4 each | Store byte enables; instruction port zero |
| `IMEM_RSP_READY`, `DMEM_RSP_READY` | Output | 1 each | Always accept a response |
| `PC` | Output | 32 | Current architectural PC |
| `HALTED`, `TRAPPED` | Output | 1 each | Sticky terminal status |
| `TRAP_CAUSE` | Output | 4 | Project trap code, not privileged mcause |
| `INSTRUCTION_ATTEMPT` | Output | 1 | Active instruction-attempt indication before the rising edge |

These use the common REQ/RSP fields. This strict core requires REQ_READY and RSP_VALID for every active access before its commit edge; it does not stall for delayed endpoints. The core exposes no individual register pins. Registers are available through the architectural-state inspection capability. `INSTRUCTION_ATTEMPT` is not a clock or stored instruction count; it can stay high between settling and its capture edge.

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
| Status.FETCH_REQUEST_ACTIVE / MEMORY_REQUEST_ACTIVE | Fetch.ISSUE / Memory.ISSUE | Enable native reads/access checks only when permitted |
| Constant one | Memory.COMMIT | ISSUE excludes prior faults; the endpoint rejects invalid stores without changing the request opcode |

CLK and RST reach Fetch, Decode, and execution status. Memory has no clock or state; external FF endpoints share the CPU clock. Their controllers receive interface reset but keep native storage reset low. ENABLE reaches status. Memory's diagnostic MISALIGNED is not separately wired to status: status derives the same bad-access condition from ALU_ADDRESS and MEM_SIZE, including invalid size.

#### Completion edge

The exact priority rules are in [4.3.6](../4.3/execution-status.md). In short:

1. The current PC selects instruction memory. Decode reads old register values; Execute computes the result, address, and target.
2. A load reads the selected data word during the same combinational interval. Writeback and Next PC form capture candidates.
3. Execution status resolves legality, alignment, access faults, halt/trap, and ENABLE. It permits normal PC/register writes and gates stores with the same decision.
4. One rising edge commits PC, the destination register, and any store. A halt/trap instead records terminal status and suppresses normal writes.

`ENABLE=0` suppresses execution and stores. Reset clears PC/register/status state; the system keeps native memory reset low so images survive. There are no pending CPU memory requests to drain. A halted/trapped core performs no further stores.

The clock period must cover instruction fetch through any data read and writeback. The stage-local DONE=1 is a zero-wait contract, not a timing detector. Two combinational `RV32IZeroWaitMemoryPort` groups translate native stage fields to byte-addressed REQ/RSP pins; they store nothing. Attaching a variable-latency device requires a wait-capable core/interface; a controller cannot make this strict core stall automatically.

The core retains its existing EBREAK-as-halt and terminal-trap conventions. There is no trap-handler vector, privilege mode, interrupt machinery, CSR implementation, or return from trap here. Memory faults prevent normal destination/PC updates; the failing instruction's PC remains visible.

#### C++ example

This binary composition uses all six examples in 4.4 plus `status_example::Status` from [4.3.6](../4.3/execution-status.md). Their 4.3 dependencies are unchanged. `observe` only evaluates combinational outputs; `evaluate` applies reset or a clock transition. Call evaluate after inputs change, starting with reset asserted. External zero-wait endpoints supply the current read words/status and own their storage. Capture an authorized store using the same pre-edge outputs as the core.

The important ordering is **observe every input from old state, then update all state owners, then observe the new outputs**. The C++ call order therefore cannot feed a newly written register into the same instruction's PC calculation. This is a binary reference composition, not the production core's event-driven implementation.

```cpp
namespace single_cycle_example {
struct CoreInputs {
    bool CLK, RST, ENABLE;
    uint32_t IMEM_RSP_READ_DATA, DMEM_RSP_READ_DATA;
    unsigned IMEM_RSP_STATUS, DMEM_RSP_STATUS;
    bool IMEM_REQ_READY=true, DMEM_REQ_READY=true, IMEM_RSP_VALID=true, DMEM_RSP_VALID=true;
};
struct CoreOutputs {
    uint32_t PC;
    bool HALTED, TRAPPED;
    unsigned TRAP_CAUSE;
    bool INSTRUCTION_ATTEMPT;
    uint32_t IMEM_REQ_ADDR, DMEM_REQ_ADDR, DMEM_REQ_WRITE_DATA;
    unsigned DMEM_REQ_WRITE_MASK;
    bool IMEM_REQ_VALID, DMEM_REQ_VALID;
    unsigned DMEM_REQ_OP;
    unsigned IMEM_REQ_OP=0, IMEM_REQ_WRITE_DATA=0, IMEM_REQ_WRITE_MASK=0;
    bool IMEM_RSP_READY=true, DMEM_RSP_READY=true;
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
        s.fi={i.CLK,i.RST,false,false,0,i.IMEM_RSP_READ_DATA,i.IMEM_RSP_STATUS!=0};
        auto f=fetch.observe(s.fi);
        s.di={f.INSTRUCTION,i.CLK,i.RST,false,0};
        const auto d=decode.observe(s.di);
        const auto e=execute({f.PC,d.RS1_VALUE,d.RS2_VALUE,d.IMM,
            d.ALU_OP,d.ALU_A_SEL,d.ALU_B_SEL,d.BRANCH_TYPE,d.JUMP_TYPE});
        s.mi={false,false,d.MEM_WRITE,d.LOAD_SIGN_EXTEND,
              e.ALU_RESULT,d.RS2_VALUE,d.MEM_SIZE,i.DMEM_RSP_READ_DATA,i.DMEM_RSP_STATUS!=0};
        auto m=memory.observe(s.mi);
        const auto w=writeback({e.ALU_RESULT,m.READ_DATA,f.PC_PLUS_4,d.WRITEBACK_SEL});
        const auto n=next_pc({f.PC_PLUS_4,e.CONTROL_TARGET,e.BRANCH_TAKEN,d.JUMP_TYPE});
        s.si={i.CLK,i.RST,i.ENABLE,d.LEGAL,d.REG_WRITE,d.MEM_READ,d.MEM_WRITE,
              d.HALT_REQUEST,d.TRAP_REQUEST,d.DECODE_TRAP_CAUSE,e.ALU_RESULT,d.MEM_SIZE,
              f.PC_MISALIGNED,n.TARGET_MISALIGNED,f.DONE,f.FAULT,m.DONE,m.FAULT};
        const auto c=status.observe(s.si);
        s.fi.ISSUE=c.FETCH_REQUEST_ACTIVE;
        s.fi.PC_WRITE=c.PC_WRITE;
        s.fi.NEXT_PC=n.NEXT_PC;
        s.di.REGISTER_WRITE=c.REGISTER_WRITE;
        s.di.WB_VALUE=w.WB_VALUE;
        s.mi.ISSUE=c.MEMORY_REQUEST_ACTIVE;
        s.mi.COMMIT=true; // Endpoint validation suppresses invalid writes.
        // Enables do not alter the old-state data candidates.
        f=fetch.observe(s.fi);
        m=memory.observe(s.mi);
        s.out={f.PC,c.HALTED,c.TRAPPED,c.TRAP_CAUSE,c.INSTRUCTION_ATTEMPT,
               f.IMEM_WORD_ADDR*4,m.DMEM_WORD_ADDR*4,m.DMEM_WRITE_DATA,m.DMEM_WRITE_MASK,
               f.IMEM_READ_EN,m.DMEM_READ_EN||m.DMEM_WE,unsigned(m.DMEM_WE)};
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

Use known clock/control signals, reset before execution, and allow structural paths to settle before an edge. The example models the supported binary boundary only; it omits X/Z, simulator histories, and propagation events. Structural and behavioral forms can have different transient outputs and absolute checkpoint times. There is one active edge per instruction, with no inter-stage registers or protocol-only clocks.

### Implementation

#### Structural implementation

The core has six stages (`FETCH`, `DECODE`, `EXECUTE`, `MEMORY`, `WRITEBACK`, `NEXT_PC`), `EXECUTION_STATUS`, two combinational port-formatting groups (`INSTRUCTION_PORT`, `DATA_PORT`), and three inactive instruction-write constants. The extra wiring adds no storage or clock boundary. Wires connect their pins as listed above. Architectural-state inspection obtains PC from Fetch, registers through Decode's `register-state-view`, and terminal state from status. The old monolithic `CONTROL_FLOW` block is not instantiated.

The visualizer places `FETCH → DECODE → EXECUTE → MEMORY → WRITEBACK` left to right, with `NEXT_PC` and `EXECUTION_STATUS` in a separate lower row. Writeback returns its value to Decode; Next PC returns the selected address to Fetch. This arrangement shows logical dataflow, not separate clock cycles or pipeline registers. Expanding a stage retains its own internal layout.

#### Behavioral implementation

The core-level family `rv32i.core.educational-single-cycle` selects `RV32IReferenceCore` for behavioral fidelity. It maintains compact architectural state and applies the instruction reference transition at each enabled rising edge. It is not six hidden behavioral-stage instances. By contrast, keeping the core structural and selecting all six stages behavioral retains the six public boundaries and shared status wiring; both configurations are tested.

### Examples

| Instruction and old state | Values computed before capture | Completion effect |
| --- | --- | --- |
| `addi x1,x1,2`, x1=512 | ALU=514, fallthrough=PC+4 | x1=514; PC advances |
| `lw x1,0(x1)`, x1=0x100, word at 0x100=512 | Address=0x100; load result=512 after settling | x1 stays 0x100 before the edge, then becomes 512 |
| `jalr x5,0(x5)`, PC=16, x5=24 | Target=24, link=20 | PC=24 and x5=20 at the same edge |
| `sw x1,0(x1)`, x1=0x404 | Address=0x404 and data=0x404 | Memory stores bytes 04 04 00 00 at the same edge that advances PC |
| Taken jump to address 0x102 | Misaligned target | Trap; no link-register or PC update |

The overlap cases come from `RV32ISingleCycleStageIntegrationTest`. They illustrate the no-reading-back rule; they are not a second instruction set. The [22-program catalogue](../4.2/programs/index.md) contains the complete existing images, initial data, and expected outcomes rather than duplicating them here.

#### Embedded simulation

This is [program 3](../4.2/programs/program-03.md), with signed/unsigned loads and partial stores, running the **structural core**. Expand each stage or switch its fidelity in the explorer. The timeline contains every instruction-attempt checkpoint for this configuration. The reference program page uses a behavioral core, so its absolute times need not equal this run's times.

<div class="simulator-embed"><iframe src="../../../simulator/?scenario=rv32i-program3&amp;core_fidelity=structural&amp;embed=1" title="Stage-wise RV32I single-cycle core" loading="lazy" allowfullscreen></iframe></div>

<a href="../../../simulator/?scenario=rv32i-program3&amp;core_fidelity=structural" class="md-button">Open complete single-cycle core</a>

#### Memory integration

The program root supplies a clock, this core, and two [MemoryEndpoint&lt;ff,32&gt;](../../memory/3.3.md) instances for instructions and data, each 65,536 words (256 KiB) by default. Both use the universal REQ/RSP interface. Reads and response status settle before the next rising edge; a permitted store commits on that edge together with PC and register updates. Zero-wait means **no extra clock cycle**, not zero propagation delay.

| Memory component or assembly | Suitable for this core? | Use |
| --- | --- | --- |
| `MemoryEndpoint<ff,32>`, fused behavioral implementation | Yes; default | Scalable instruction and read/write data storage |
| `MemoryEndpoint<ff,32>`, visible FF controller plus native device | Yes | Same timing, with the controller/device boundary visible |
| `MemoryEndpoint<sram,32>`, `<dram,32>`, `<rom,32>`, `<nor-flash,32>` | No, with their current controllers | These endpoints require delayed-response handling |
| Native `MemoryContainer`, `FFMemory`, or other memory device alone | Not directly | Native pins need the matching controller to form an endpoint |

The visible FF assembly can use [MemoryContainer&lt;ff,32&gt;](../../memory/3.1.6.md) for scalable storage, or [FFMemory&lt;32&gt;](../../memory/3.1.1.md) for a small cell-expanded circuit. Keep scalable storage for program-sized images; choosing structural visualization does not require expanding every bit into a cell. Neither capacity nor behavioral/structural fidelity changes the zero-wait requirement. CPU reset resets the endpoint interface but does not erase its native storage.

!!! warning "Matching pins do not make delayed memory compatible"

    `REQ_READY` and `RSP_VALID` are required conditions, not stall inputs used by this core. `RV32ISingleCycleSystem` and the program root reject non-FF memory configurations during construction. If custom wiring bypasses that check, the core can consume stale instruction/load data or advance past an unaccepted store; it does not automatically wait or raise a special timing-contract trap. Use the transaction-capable `RV32IFiveStageCore` for delayed endpoints. The [baseline pipeline](../4.5/core.md#memory-integration) has the same zero-wait restriction as this core.

`RV32ISingleCycleSystem` is the convenience wrapper with owned memories and external CLK/RST/ENABLE. Program loading and memory inspection are host operations, not instructions. Reset alone does not prepare a stack pointer: program/ABI setup must initialize x2 when a program uses a stack.

### Tests

| Test | Coverage |
| --- | --- |
| Six `RV32ISingleCycle*StageTest` scenarios | Structural/behavioral settled stage contracts |
| `RV32ISingleCycleCoreTest` | Core pin/status execution against the instruction reference |
| `RV32ISingleCycleStageIntegrationTest` | Overlapping operands, JALR link/target, exact store bytes, no early PC/register update; each stage, all stages, whole-core behavior, and structural register-file wiring |
| `RV32ISingleCycleSystemTest/program-01` … `program-22` | Full program checkpoints against the instruction oracle |
| `RV32IProfileToggleSweepTest` | Every selectable family in the canonical structural core tree |
| `RV32IMemoryMigrationTest` | Zero-wait single-cycle endpoints and configurable advanced-pipeline endpoints |
| `RV32ISingleCycleDocumentationTest.py` | Pin schemas, recorded examples, exact C++ stage/composition references |

Stage tests assert exactly one ALU plus one concurrent branch-target adder in Execute. The integration harness permits at most one cycle per instruction and checks old-source load/store/JALR overlaps; final-state-only comparisons could miss early writes. These focused tests do not claim exhaustive instruction-stream, arbitrary X/Z, or physical timing verification.

Source: `src/modules/rv32i/RV32ISingleCycleCore.cpp`, `RV32ISingleCycleStages.cpp`, `RV32ISingleCycleStagesBehavioral.cpp`, and `RV32IReferenceCore.cpp`. Stage scenarios are in `src/tests/RV32ISingleCycleStageTests.cpp`; program/fidelity tests are in `src/tests/RV32ISingleCycleTests.cpp`.
