# § 4.5.1. Pipeline Registers

Pipeline registers hold instruction packets, not architectural registers. Baseline and advanced cores use the same four register families.

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| `RV32IIFIDPipelineRegister` | Fetch → Decode packet | Yes | Yes |
| `RV32IIDEXPipelineRegister` | Decode → Execute packet | Yes | Yes |
| `RV32IEXMEMPipelineRegister` | Execute → Memory packet | Yes | Yes |
| `RV32IMEMWBPipelineRegister` | Memory → Writeback packet | Yes | Yes |

## IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `CLK`, `RST` | Input | 1 each | Rising-edge capture; asynchronous active-high reset |
| `WRITE_ENABLE` | Input | 1 | Capture when one; hold when zero |
| `FLUSH` | Input | 1 | Capture a bubble when WRITE_ENABLE is one |
| `D_VALID` | Input | 1 | Incoming instruction validity |
| `D_<field>` | Input | 32 each | Incoming payload fields listed below |
| `Q_VALID` | Output | 1 | Stored instruction validity |
| `Q_<field>` | Output | 32 each | Stored payload fields |

| Register | Payload fields |
| --- | --- |
| IF/ID | `PC`, `INSTRUCTION`, `FETCH_STATUS` |
| ID/EX | IF/ID fields plus `RS1_VALUE`, `RS2_VALUE`, `IMMEDIATE`, `CONTROL`, `REGISTER_ADDRESSES` |
| EX/MEM | `PC`, `INSTRUCTION`, `ALU_RESULT`, `STORE_DATA`, `PC_PLUS_4`, `NEXT_PC`, `CONTROL`, `REGISTER_ADDRESSES`, `FETCH_STATUS`, `EXECUTION_STATUS` |
| MEM/WB | EX/MEM fields plus `MEMORY_DATA`, `MEMORY_STATUS` |

## Behavior

Reset clears all fields and validity. A rising edge with WRITE_ENABLE=1 captures every payload field and sets Q_VALID to D_VALID && !FLUSH. With WRITE_ENABLE=0, **FLUSH does not override the hold**. A bubble's payload is not necessarily zero; downstream side effects must be gated by validity.

### Advance, hold, and bubble

With reset inactive, the following actions occur at a rising edge:

| WRITE_ENABLE | FLUSH | Action |
| ---: | ---: | --- |
| 0 | 0 or 1 | **Hold:** retain both the current payload and its validity |
| 1 | 0 | **Advance:** capture the incoming payload and D_VALID; an invalid input remains a bubble |
| 1 | 1 | **Bubble:** capture the incoming payload but force Q_VALID=0 |

The clock continues running during a pipeline stall. The controller can hold one register while allowing others to advance. For example, a data hazard holds IF/ID so its instruction stays in Decode, while ID/EX receives a bubble and older instructions advance through EX/MEM and MEM/WB. See the [cycle-by-cycle hazard and jump examples](control.md#data-hazard-hold-the-consumer-advance-the-producer).

### C++ example

`N` is the number of 32-bit payload fields in the table. This reference includes capture, hold, reset, and bubble behavior; reset before use.

```cpp
#include <array>
#include <cstdint>
template<unsigned N> struct PipelineRegister {
    bool valid=false, previous_clock=false;
    std::array<uint32_t,N> payload{};
    void evaluate(bool clk, bool reset, bool enable, bool flush,
                  bool incoming_valid, const std::array<uint32_t,N>& incoming) {
        if(reset) { valid=false; payload.fill(0); }
        else if(!previous_clock && clk && enable) {
            payload=incoming;
            valid=incoming_valid && !flush;
        }
        previous_clock=clk;
    }
};
```

### Packet encodings

The public pins are 32-bit wiring bundles, not additional ISA registers. Standalone packet components retain all 32 bits supplied at their inputs. Core instances use a compact internal schema because the stage producers define only the fields below.

| `CONTROL` bits | Meaning |
| --- | --- |
| 0, 1, 2, 3, 4 | LEGAL, REG_WRITE, MEM_READ, MEM_WRITE, LOAD_SIGN_EXTEND |
| 5, 6, 7, 8 | HALT_REQUEST, TRAP_REQUEST, USES_RS1, USES_RS2 |
| 10:9, 12:11 | WRITEBACK_SEL, MEM_SIZE |
| 15:13, 17:16 | BRANCH_TYPE, JUMP_TYPE |
| 21:18, 26:22 | DECODE_TRAP_CAUSE, ALU_OP |
| 28:27, 30:29 | ALU_A_SEL, ALU_B_SEL |
| 31 | Reserved |

Values of these fields are the [decoder's encodings](../4.3/instruction-decoder.md). `REGISTER_ADDRESSES` stores RS1 in bits 4:0, RS2 in 9:5, and RD in 14:10. The other bits are reserved.

| Status word | Defined bits |
| --- | --- |
| `FETCH_STATUS` | 0: PC misaligned; 1: instruction access fault |
| `EXECUTION_STATUS` | 0: selected branch/jump target misaligned |
| `MEMORY_STATUS` | 0: data misaligned; 1: data access fault |

## Implementation

### Structural implementation

Full-width payloads use `Register<32>`. In core instances, `FETCH_STATUS` and `MEMORY_STATUS` use two storage bits, `EXECUTION_STATUS` uses one, and `CONTROL` uses 31. `REGISTER_ADDRESSES` stores three 5-bit fields through ID/EX and only the RD field in EX/MEM and MEM/WB. Split/join rewires maintain the public 32-bit pins and fill unused output bits with zero. A `FlipFlopCell` stores validity, with reset/enable/flush qualification. Standalone packet instances use full-width registers for every payload field, including inputs with reserved bits set.

### Behavioral implementation

`RV32IPipelineRegisterBehavioral` models known clock edges, hold, flush, reset, and bitwise unknown merging. With the compact core schema, it zeros unstored bits at capture and preserves the same 32-bit observable packet as the structural implementation. Standalone instances retain arbitrary input bits.

## Examples

Each register has its own complete checkpoint table and simulation.

<!-- simulator-examples: rv32i-if-id RV32IIFIDPipelineRegisterTest -->

<div class="simulator-embed"><iframe src="../../../simulator/?scenario=rv32i-if-id&amp;embed=1" title="IF/ID pipeline register" loading="lazy" allowfullscreen></iframe></div>

<!-- simulator-examples: rv32i-id-ex RV32IIDEXPipelineRegisterTest -->

<div class="simulator-embed"><iframe src="../../../simulator/?scenario=rv32i-id-ex&amp;embed=1" title="ID/EX pipeline register" loading="lazy" allowfullscreen></iframe></div>

<!-- simulator-examples: rv32i-ex-mem RV32IEXMEMPipelineRegisterTest -->

<div class="simulator-embed"><iframe src="../../../simulator/?scenario=rv32i-ex-mem&amp;embed=1" title="EX/MEM pipeline register" loading="lazy" allowfullscreen></iframe></div>

<!-- simulator-examples: rv32i-mem-wb RV32IMEMWBPipelineRegisterTest -->

<div class="simulator-embed"><iframe src="../../../simulator/?scenario=rv32i-mem-wb&amp;embed=1" title="MEM/WB pipeline register" loading="lazy" allowfullscreen></iframe></div>

## Tests

`RV32IIFIDPipelineRegisterTest`, `RV32IIDEXPipelineRegisterTest`, `RV32IEXMEMPipelineRegisterTest`, and `RV32IMEMWBPipelineRegisterTest` compare both implementations. `RV32IBaselinePipelineCoreTest` checks overlap, global pause, drain, and reset with these registers connected. Sources: `RV32IPipelineRegisters.hpp/.cpp`.
