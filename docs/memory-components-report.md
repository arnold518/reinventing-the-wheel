# Memory Components Report

Last updated: 2026-07-19

## Status

The memory-cell foundation is complete on the `memory-components` branch. The branch contains both lower-level visualizable storage components and behavioral storage components used when full structural expansion becomes too large.

For compact CPU tests, `BehavioralRegisterFile32x32` remains the fast register-file option. The educational structural RV32I core now plans to use the same-contract `RegisterFile32x32`, with a direct equivalence test between the two. Instruction and data memory should still be separate `BehavioralMemory64Kx32` instances because the existing smaller structural memories provide the lower-level proof while a fully expanded 256 KiB memory would be impractical.

The component order below follows the implementation/evolution path:

1. `MemoryBit`
2. `BehavioralMemoryBit`
3. `Register32`
4. `BehavioralRegister32`
5. `Decoder2to4` and `Decoder5to32`
6. `RegisterFile4x32`
7. `RegisterFile32x32`
8. `BehavioralRegisterFile32x32`
9. `Memory4x32`
10. `Memory32x32`
11. `BehavioralMemory64Kx32`

The structural storage path uses the structural `DFlipFlop` implementation documented in `docs/structural-dff-report.md`. The behavioral path stores state directly in compact `BasicComponent` state, but keeps the same external contracts where possible.

## Components

### `MemoryBit`

Purpose:

`MemoryBit` is the visualizable one-bit storage cell. It is the first memory component in the branch and the structural teaching version of a write-enabled storage bit.

Files:

- `include/modules/memory/MemoryBit.hpp`
- `src/modules/memory/MemoryBit.cpp`

Pins:

| Pin | Direction | Width | Meaning |
| --- | --- | --- | --- |
| `D` | input | 1 | Data bit to write. |
| `WE` | input | 1 | Write enable. High selects `D`; low selects current `Q` feedback. |
| `CLK` | input | 1 | Rising-edge clock. |
| `RST` | input | 1 | Reset. High clears stored state to low through the internal D flip-flop. |
| `Q` | output | 1 | Stored bit. |

Implementation:

- One `Mux2to1` named `WRITE_MUX`
- One structural `DFlipFlop` named `STATE`
- Feedback wire from `STATE.Q` back to `WRITE_MUX.A`

Structure:

```text
                 +-------------+
Q feedback ----> | A           |
D -------------> | B  Mux2to1  | ----> DFlipFlop.D
WE ------------> | SEL         |
                 +-------------+

CLK ---------------------------> DFlipFlop.CLK
RST ---------------------------> DFlipFlop.RST
DFlipFlop.Q -------------------> Q and feedback to mux A
```

Behavior:

```text
STATE.D = WE ? D : Q

if RST = 1:
    Q becomes 0
else on rising CLK:
    if WE = 1:
        Q becomes D
    if WE = 0:
        Q keeps its previous value
```

The feedback path is what lets `WE=0` hold state. The flip-flop still captures on the rising clock edge, but it captures the current `Q` value again.

Rising-edge table:

| `RST` | `WE` | `D` | Previous `Q` | Next `Q` |
| --- | --- | --- | --- | --- |
| 1 | X | X | X | 0 |
| 0 | 0 | 0 | 0 | 0 |
| 0 | 0 | 1 | 0 | 0 |
| 0 | 0 | 0 | 1 | 1 |
| 0 | 0 | 1 | 1 | 1 |
| 0 | 1 | 0 | X | 0 |
| 0 | 1 | 1 | X | 1 |
| 0 | 1 | X | X | X |

Tests and visualizer:

- Test: `MemoryBitTest`
- Visualizer scenario: `memory-bit`
- Checkpoints: reset, hold, write one, no level capture, write zero, hold zero, unknown data, reset clear, unknown write enable.
- Hosted visualizer stats: 28 components, 89 pins, 55 wires, 129 timestamps.

### `BehavioralMemoryBit`

Purpose:

`BehavioralMemoryBit` is the compact counterpart to `MemoryBit`. It exists so wider registers and register files can be built without expanding every bit into muxes, latches, and gates.

Files:

- `include/modules/memory/BehavioralMemoryBit.hpp`
- `src/modules/memory/BehavioralMemoryBit.cpp`

Pins:

| Pin | Direction | Width | Meaning |
| --- | --- | --- | --- |
| `D` | input | 1 | Data bit to write. |
| `WE` | input | 1 | Write enable. High stores `D`; low holds the previous value. |
| `CLK` | input | 1 | Rising-edge clock. |
| `RST` | input | 1 | Reset. High clears stored state to low. |
| `Q` | output | 1 | Stored bit. |

Implementation:

- Inherits from `BasicComponent`.
- Has no child components.
- Stores the bit in `stored_value`.
- Tracks rising clock edges with `previous_clk`.
- Uses a one-tick output delay.

Behavior:

```text
if RST = 1:
    Q becomes 0
else if RST is unknown:
    Q becomes X
else on rising CLK:
    if WE = 1:
        Q becomes D
    if WE = 0:
        Q keeps its previous value
    if WE is unknown:
        Q becomes X
```

Important timing behavior:

- Writes happen only when the previous `CLK` value was low and the current value is high.
- Falling `CLK` edges do not write.
- Data changes while `CLK` is already high do not write again.
- Unknown `D` under `WE=1` is captured as unknown.
- Reset is asynchronous in the component evaluation model.

Tests and visualizer:

- Test: `BehavioralMemoryBitTest`
- Visualizer scenario: `behavioral-memory-bit`
- Checkpoints: reset, hold, write one, no level capture, write zero, hold zero, unknown data, reset clear, unknown write enable.
- Hosted visualizer stats: 1 component, 5 pins, 5 wires, 39 timestamps.

### `Register32`

Purpose:

`Register32` is one 32-bit storage word made from 32 structural `MemoryBit` cells. It is not the RV32I register file.

Files:

- `include/modules/memory/Register32.hpp`
- `src/modules/memory/Register32.cpp`

Pins:

| Pin | Direction | Width | Meaning |
| --- | --- | --- | --- |
| `D` | input | 32 | Data word to write. Bit 0 is the least significant bit. |
| `WE` | input | 1 | Shared write enable for all 32 bit cells. |
| `CLK` | input | 1 | Shared rising-edge clock for all 32 bit cells. |
| `RST` | input | 1 | Shared reset for all 32 bit cells. |
| `Q` | output | 32 | Stored output word. |

Implementation:

- One `BitSplitter<32>` named `D_SPLIT`
- Thirty-two structural `MemoryBit` cells named `BIT_0` through `BIT_31`
- One `BitJoiner<32>` named `Q_JOIN`
- Shared fanout wires for `WE`, `CLK`, and `RST`

Structure:

```text
D[31:0] ----> D_SPLIT ----> BIT_0.D ... BIT_31.D
WE -----------------------> BIT_0.WE ... BIT_31.WE
CLK ----------------------> BIT_0.CLK ... BIT_31.CLK
RST ----------------------> BIT_0.RST ... BIT_31.RST
BIT_0.Q ... BIT_31.Q ----> Q_JOIN ----> Q[31:0]
```

Behavior:

```text
if RST = 1:
    Q[31:0] becomes 0x00000000
else on rising CLK:
    if WE = 1:
        Q[31:0] becomes D[31:0]
    if WE = 0:
        Q[31:0] keeps its previous value
```

Every bit lane behaves like an independent `MemoryBit`:

```text
Q[n]_next = WE ? D[n] : Q[n]
```

Tests and visualizer:

- Test: `Register32Test`
- Visualizer scenario: `register32`
- Checkpoints: reset, hold, full-word writes, all 32 walking-bit writes, unknown-lane capture, final reset.
- Hosted visualizer stats: 899 components, 2919 pins, 1674 wires, 790 timestamps.

### `BehavioralRegister32`

Purpose:

`BehavioralRegister32` is one compact 32-bit storage word made from 32 `BehavioralMemoryBit` cells. It is the scalable counterpart to structural `Register32`.

Files:

- `include/modules/memory/BehavioralRegister32.hpp`
- `src/modules/memory/BehavioralRegister32.cpp`

Pins:

| Pin | Direction | Width | Meaning |
| --- | --- | --- | --- |
| `D` | input | 32 | Data word to write. |
| `WE` | input | 1 | Shared write enable for all 32 bit cells. |
| `CLK` | input | 1 | Shared rising-edge clock. |
| `RST` | input | 1 | Shared reset. |
| `Q` | output | 32 | Stored output word. |

Implementation:

- One `BitSplitter<32>` named `D_SPLIT`
- Thirty-two `BehavioralMemoryBit` cells named `BIT_0` through `BIT_31`
- One `BitJoiner<32>` named `Q_JOIN`

Behavior:

```text
if RST = 1:
    Q[31:0] becomes 0x00000000
else on rising CLK:
    if WE = 1:
        Q[31:0] becomes D[31:0]
    if WE = 0:
        Q[31:0] keeps its previous value
```

Each bit lane is independent, so an unknown data bit only contaminates the matching output lane.

Tests and visualizer:

- Covered indirectly by `RegisterFile4x32Test`, `RegisterFile32x32Test`, and `Memory4x32Test`.
- Bound as a visualizable module for hierarchy inspection.

### `Decoder2to4` and `Decoder5to32`

Purpose:

The decoder components are reusable enabled one-hot address decoders. They convert one address plus one enable signal into exactly one active output line. Register files and memory arrays use them to choose which word may accept a write.

Files:

- `include/modules/basic/Decoder.hpp`
- `src/modules/basic/Decoder.cpp`

Pins:

| Component | Pin | Direction | Width | Meaning |
| --- | --- | --- | --- | --- |
| `Decoder2to4` | `ADDR` | input | 2 | Selects `OUT0..OUT3`. |
| `Decoder2to4` | `ENABLE` | input | 1 | Gates every output low when disabled. |
| `Decoder2to4` | `OUT0..OUT3` | output | 1 each | One-hot enabled outputs. |
| `Decoder5to32` | `ADDR` | input | 5 | Selects `OUT0..OUT31`. |
| `Decoder5to32` | `ENABLE` | input | 1 | Gates every output low when disabled. |
| `Decoder5to32` | `OUT0..OUT31` | output | 1 each | One-hot enabled outputs. |

Implementation:

- One `BitSplitter<N>` for the address input.
- One `NOTGate` per address bit.
- Per output, a structural AND chain checks `ADDR == output_index`.
- A final `ANDGate` combines the address match with `ENABLE`.

Behavior:

```text
if ENABLE = 1:
    OUT[ADDR] = 1
    every other OUT = 0
else:
    every OUT = 0
```

Examples:

```text
Decoder2to4:  ADDR=2, ENABLE=1 -> OUT2=1, OUT0/OUT1/OUT3=0
Decoder2to4:  ADDR=2, ENABLE=0 -> OUT0..OUT3=0
Decoder5to32: ADDR=5, ENABLE=1 -> OUT5=1, every other OUT=0
```

Tests and visualizer:

- Tests: `Decoder2to4Test`, `Decoder5to32Test`
- Visualizer scenarios: `decoder2to4`, `decoder5to32`
- Hosted visualizer stats: `Decoder2to4` has 12 components, 37 pins, 20 wires, 39 timestamps; `Decoder5to32` has 167 components, 530 pins, 206 wires, 309 timestamps.

### `RegisterFile4x32`

Purpose:

`RegisterFile4x32` is a four-entry teaching prototype of an RV32I-style register file. It has two independent read ports and one write port, but only exposes `x0..x3`.

Files:

- `include/modules/memory/RegisterFile4x32.hpp`
- `src/modules/memory/RegisterFile4x32.cpp`

Pins:

| Pin | Direction | Width | Meaning |
| --- | --- | --- | --- |
| `RS1_ADDR` | input | 2 | First read-port address, selecting `x0..x3`. |
| `RS2_ADDR` | input | 2 | Second read-port address, selecting `x0..x3`. |
| `RD_ADDR` | input | 2 | Write address, selecting `x0..x3`. |
| `WRITE_DATA` | input | 32 | Data word for the write port. |
| `REG_WRITE` | input | 1 | Global write enable. |
| `CLK` | input | 1 | Shared rising-edge write clock. |
| `RST` | input | 1 | Shared reset for writable registers. |
| `RS1_DATA` | output | 32 | First read-port data. |
| `RS2_DATA` | output | 32 | Second read-port data. |

Implementation:

- One `ConstantValue<32, 32>` named `X0_ZERO`
- Three writable `BehavioralRegister32` words named `X1`, `X2`, and `X3`
- Two `Mux4to1_32bit` read muxes named `RS1_MUX` and `RS2_MUX`
- One `Decoder2to4` named `RD_DECODER` for write-address decode

Structure:

```text
WRITE_DATA ---------------------> X1.D, X2.D, X3.D
RD_ADDR ----+
            +--> RD_DECODER ----> X1.WE, X2.WE, X3.WE
REG_WRITE --+
CLK ----------------------------> X1.CLK, X2.CLK, X3.CLK
RST ----------------------------> X1.RST, X2.RST, X3.RST

X0_ZERO ----+
X1.Q -------+--> RS1_MUX -- RS1_DATA
X2.Q -------+
X3.Q -------+
             selected by RS1_ADDR

X0_ZERO ----+
X1.Q -------+--> RS2_MUX -- RS2_DATA
X2.Q -------+
X3.Q -------+
             selected by RS2_ADDR
```

Behavior:

```text
RS1_DATA = register[RS1_ADDR]
RS2_DATA = register[RS2_ADDR]

if RST = 1:
    x1, x2, x3 become 0x00000000
    x0 remains 0x00000000
else on rising CLK:
    if REG_WRITE = 1 and RD_ADDR != 0:
        register[RD_ADDR] becomes WRITE_DATA
```

Important behavior:

- `x0` always reads as `0x00000000`.
- Writes to `RD_ADDR=0` are ignored.
- `RS1_ADDR` and `RS2_ADDR` may select the same register or two different registers.
- `REG_WRITE=0` blocks writes even if `CLK` rises.
- Unknown data lanes remain lane-local inside the selected destination register.

Tests and visualizer:

- Test: `RegisterFile4x32Test`
- Visualizer scenario: `register-file4x32`
- Checkpoints: reset, x0 write ignore, x1/x2/x3 writes, independent reads, disabled write hold, x2 overwrite, unknown-lane capture, final reset.
- Hosted visualizer stats: 1219 components, 4539 pins, 2562 wires, 155 timestamps.

### `RegisterFile32x32`

Purpose:

`RegisterFile32x32` is the full RV32I architectural register-file shape. It has 32 architectural register names, two read ports, and one write port.

Files:

- `include/modules/memory/RegisterFile32x32.hpp`
- `src/modules/memory/RegisterFile32x32.cpp`

Pins:

| Pin | Direction | Width | Meaning |
| --- | --- | --- | --- |
| `RS1_ADDR` | input | 5 | First read-port address, selecting `x0..x31`. |
| `RS2_ADDR` | input | 5 | Second read-port address, selecting `x0..x31`. |
| `RD_ADDR` | input | 5 | Write address, selecting `x0..x31`. |
| `WRITE_DATA` | input | 32 | Data word for the write port. |
| `REG_WRITE` | input | 1 | Global write enable. |
| `CLK` | input | 1 | Shared rising-edge write clock. |
| `RST` | input | 1 | Shared reset for writable registers. |
| `RS1_DATA` | output | 32 | First read-port data. |
| `RS2_DATA` | output | 32 | Second read-port data. |

Implementation:

- One `ConstantValue<32, 32>` named `X0_ZERO`
- Thirty-one `BehavioralRegister32` words named `X1` through `X31`
- Two `Mux32to1_32bit` read muxes named `RS1_MUX` and `RS2_MUX`
- One `Decoder5to32` named `RD_DECODER` for write-address decode

Behavior:

```text
RS1_DATA = register[RS1_ADDR]
RS2_DATA = register[RS2_ADDR]

if RST = 1:
    x1..x31 become 0x00000000
    x0 remains 0x00000000
else on rising CLK:
    if REG_WRITE = 1 and RD_ADDR != 0:
        register[RD_ADDR] becomes WRITE_DATA
```

Important behavior:

- `x0` always reads as `0x00000000`.
- Writes to `RD_ADDR=0` are ignored.
- `x1..x31` are independent `BehavioralRegister32` instances.
- `RS1_ADDR` and `RS2_ADDR` can read any two registers independently.
- `REG_WRITE=0` blocks writes even if `CLK` rises.
- Unknown data lanes remain lane-local inside the selected destination register.

Tests and visualizer:

- Test: `RegisterFile32x32Test`
- Visualizer scenario: `register-file32x32`
- Checkpoints: reset, x0 write ignore, all `x1..x31` writes, disabled write hold, second x0 ignore, unknown-lane capture, final reset.
- Hosted visualizer stats: 11370 components, 42268 pins, 22876 wires, 985 timestamps.

### `BehavioralRegisterFile32x32`

Purpose:

`BehavioralRegisterFile32x32` is the compact behavioral version of the full RV32I register file. It keeps the same external contract as `RegisterFile32x32`, but stores all 32 architectural words inside one `BasicComponent` instead of expanding into decoders, muxes, and per-word storage components.

Files:

- `include/modules/memory/BehavioralRegisterFile32x32.hpp`
- `src/modules/memory/BehavioralRegisterFile32x32.cpp`

Pins:

| Pin | Direction | Width | Meaning |
| --- | --- | --- | --- |
| `RS1_ADDR` | input | 5 | First read-port address, selecting `x0..x31`. |
| `RS2_ADDR` | input | 5 | Second read-port address, selecting `x0..x31`. |
| `RD_ADDR` | input | 5 | Write address, selecting `x0..x31`. |
| `WRITE_DATA` | input | 32 | Data word for the write port. |
| `REG_WRITE` | input | 1 | Global write enable. |
| `CLK` | input | 1 | Rising-edge write clock. |
| `RST` | input | 1 | Asynchronous reset for writable registers. |
| `RS1_DATA` | output | 32 | First read-port data. |
| `RS2_DATA` | output | 32 | Second read-port data. |

Implementation:

- Inherits from `BasicComponent`.
- Has no child components.
- Stores state as 32 vectors of 32 `LogicValue` lanes.
- Tracks rising clock edges with `previous_clk`.
- Uses a one-tick output delay, matching the existing behavioral storage style.

Behavior:

```text
RS1_DATA = register[RS1_ADDR]
RS2_DATA = register[RS2_ADDR]

if RST = 1:
    x1..x31 become 0x00000000
    x0 remains 0x00000000
else if RST = X:
    x1..x31 become unknown
    x0 remains 0x00000000
else on rising CLK:
    if REG_WRITE = 1 and RD_ADDR != 0:
        register[RD_ADDR] becomes WRITE_DATA
    if REG_WRITE or RD_ADDR is unknown:
        every register that may match the write becomes unknown
```

Important behavior:

- `x0` is hardwired to `0x00000000`.
- Writes to `RD_ADDR=0` are ignored.
- Read ports are combinational from the current stored state.
- Unknown read-address bits merge every possible matching register bitwise; an output lane remains known only when all possible selected registers agree.
- Unknown write selection is conservative: any writable register that may be selected is marked unknown.
- This is the scalable register-file component for compact tests and future fast CPU configurations when the structural shape is too large for routine execution.
- The educational structural core uses `RegisterFile32x32`; a direct pairwise equivalence suite must prove that both contracts agree.

Tests and visualizer:

- Test: `BehavioralRegisterFile32x32Test`
- X-state test: `BehavioralRegisterFile32x32UnknownTest`
- Visualizer scenario: `behavioral-register-file32x32`
- X-state visualizer scenario: `behavioral-register-file32x32-unknown`
- Alias: `brf32x32`
- Checkpoints: inherits the full `RegisterFile32x32Test` sequence: reset, x0 write ignore, all `x1..x31` writes, disabled write hold, second x0 ignore, unknown-lane capture, final reset.
- X-state checkpoints: ambiguous read with agreeing values, ambiguous read with disagreeing values, ambiguous write contamination, unknown `REG_WRITE`, unknown reset, and `x0` preservation.
- Hosted visualizer stats: 1 component, 9 pins, 9 wires, 217 timestamps.
- X-state visualizer stats: 1 component, 9 pins, 9 wires, 39 timestamps.

### `Memory4x32`

Purpose:

`Memory4x32` is the first small addressable memory slice. It uses the same CPU-facing pins planned for larger instruction/data memory, while keeping the implementation small enough to inspect in the visualizer.

Files:

- `include/modules/memory/Memory4x32.hpp`
- `src/modules/memory/Memory4x32.cpp`

Pins:

| Pin | Direction | Width | Meaning |
| --- | --- | --- | --- |
| `ADDR` | input | 32 | Byte address. Current slice accepts word addresses `0x0`, `0x4`, `0x8`, and `0xc`. |
| `WRITE_DATA` | input | 32 | 32-bit store data. |
| `READ_EN` | input | 1 | Marks a read access for status/fault reporting. |
| `WRITE_EN` | input | 1 | Enables a rising-edge write when the access is valid. |
| `SIZE` | input | 2 | Access size. Current slice accepts only `10` = word. |
| `SIGN_EXTEND` | input | 1 | Future load-extension control; currently connected but does not change behavior. |
| `CLK` | input | 1 | Shared rising-edge write clock. |
| `RST` | input | 1 | Shared reset for all four stored words. |
| `READ_DATA` | output | 32 | Combinational word selected by `ADDR[3:2]`. |
| `READY` | output | 1 | Always high after initialization. |
| `FAULT` | output | 1 | High for unsupported size, unaligned word address, or address outside the 16-byte window while an access is requested. |

Implementation:

- Four `BehavioralRegister32` words named `WORD_0` through `WORD_3`
- One `Mux4to1_32bit` named `READ_MUX`
- One `BitSplitter<32>` for address bits
- One `BitJoiner<2>` for read mux selection
- One `Decoder2to4` named `WRITE_DECODER` for selected word write enable
- Structural NOT/AND/OR gates for write-valid gating, high-address detection, alignment checking, and `FAULT`
- One `ConstantValue<1, 1>` named `READY_ONE`

Address map:

```text
ADDR 0x0 -> WORD_0
ADDR 0x4 -> WORD_1
ADDR 0x8 -> WORD_2
ADDR 0xc -> WORD_3
```

Current valid access:

```text
SIZE = 10 and ADDR[1:0] = 00 and ADDR[31:4] = 0
```

Fault cases:

```text
SIZE = 00, 01, or 11 -> FAULT
ADDR[1:0] != 00      -> FAULT
ADDR[31:4] != 0      -> FAULT
```

Behavior:

```text
if RST = 1:
    WORD_0..WORD_3 become 0x00000000
else on rising CLK:
    if WRITE_EN = 1 and FAULT = 0:
        WORD_[ADDR[3:2]] becomes WRITE_DATA

READ_DATA = WORD_[ADDR[3:2]]
READY = 1
FAULT = (READ_EN or WRITE_EN) and invalid_access
```

Important behavior:

- `READ_DATA` is combinational and selected by `ADDR[3:2]`.
- `READ_DATA` is not tri-stated when `READ_EN=0`.
- Invalid writes are blocked before they reach row write-enable pins.
- `READ_EN=0` and `WRITE_EN=0` suppress `FAULT` reporting.
- Byte stores, halfword stores, byte loads, halfword loads, and sign/zero extension are not implemented yet.

Tests and visualizer:

- Test: `Memory4x32Test`
- Visualizer scenario: `memory4x32`
- Checkpoints: reset read, four word writes, read mux selection, disabled write hold, unsupported size faults, misalignment fault, out-of-range fault, no-access fault suppression, blocked faulted write, final reset.
- Hosted visualizer stats: 751 components, 3039 pins, 1556 wires, 270 timestamps.

### `Memory32x32`

Purpose:

`Memory32x32` is the 32-word structural memory slice. It keeps the same CPU-facing memory pins as `Memory4x32`, but expands the storage array to 32 addressable 32-bit words.

Files:

- `include/modules/memory/Memory32x32.hpp`
- `src/modules/memory/Memory32x32.cpp`

Pins:

| Pin | Direction | Width | Meaning |
| --- | --- | --- | --- |
| `ADDR` | input | 32 | Byte address. Current slice accepts word addresses `0x00` through `0x7c`. |
| `WRITE_DATA` | input | 32 | 32-bit store data. |
| `READ_EN` | input | 1 | Marks a read access for status/fault reporting. |
| `WRITE_EN` | input | 1 | Enables a rising-edge write when the access is valid. |
| `SIZE` | input | 2 | Access size. Current slice accepts only `10` = word. |
| `SIGN_EXTEND` | input | 1 | Future load-extension control; currently connected but does not change behavior. |
| `CLK` | input | 1 | Shared rising-edge write clock. |
| `RST` | input | 1 | Shared reset for all thirty-two stored words. |
| `READ_DATA` | output | 32 | Combinational word selected by `ADDR[6:2]`. |
| `READY` | output | 1 | Always high after initialization. |
| `FAULT` | output | 1 | High for unsupported size, unaligned word address, or address outside the 128-byte window while an access is requested. |

Implementation:

- Thirty-two `BehavioralRegister32` words named `WORD_0` through `WORD_31`
- One `Mux32to1_32bit` named `READ_MUX`
- One `Decoder5to32` named `WRITE_DECODER` for selected word write enable
- One `BitJoiner<5>` for `ADDR[6:2]` word selection
- Structural NOT/AND/OR gates for write-valid gating, high-address detection, alignment checking, and `FAULT`
- One `ConstantValue<1, 1>` named `READY_ONE`

Address map:

```text
ADDR 0x00 -> WORD_0
ADDR 0x04 -> WORD_1
...
ADDR 0x7c -> WORD_31
```

Current valid access:

```text
SIZE = 10 and ADDR[1:0] = 00 and ADDR[31:7] = 0
```

Behavior:

```text
if RST = 1:
    WORD_0..WORD_31 become 0x00000000
else on rising CLK:
    if WRITE_EN = 1 and FAULT = 0:
        WORD_[ADDR[6:2]] becomes WRITE_DATA

READ_DATA = WORD_[ADDR[6:2]]
READY = 1
FAULT = (READ_EN or WRITE_EN) and invalid_access
```

Important behavior:

- `READ_DATA` is combinational and selected by `ADDR[6:2]`.
- The memory is byte-addressed, so adjacent words are four bytes apart.
- Invalid writes are blocked before they reach row write-enable pins.
- The 32-row write path reuses `Decoder5to32`.
- The 32-row read path reuses `Mux32to1_32bit`.
- Byte stores, halfword stores, byte loads, halfword loads, and sign/zero extension are not implemented yet.

Tests and visualizer:

- Test: `Memory32x32Test`
- Visualizer scenario: `memory32x32`
- Checkpoints: reset read, writes and readbacks for all `WORD_0..WORD_31`, disabled write hold, unsupported size faults, misalignment fault, out-of-range fault, no-access fault suppression, blocked faulted write, final reset.
- Hosted visualizer stats: 6391 components, 25378 pins, 12783 wires, 1209 timestamps.

### `BehavioralMemory64Kx32`

Purpose:

`BehavioralMemory64Kx32` is the practical RV32I memory component. It stores 64K 32-bit words, or 256 KiB total, behind the same CPU-facing memory pins used by `Memory4x32` and `Memory32x32`.

Files:

- `include/modules/memory/BehavioralMemory64Kx32.hpp`
- `src/modules/memory/BehavioralMemory64Kx32.cpp`

Pins:

| Pin | Direction | Width | Meaning |
| --- | --- | --- | --- |
| `ADDR` | input | 32 | Byte address. Valid byte range is `0x00000000` through `0x0003ffff`. |
| `WRITE_DATA` | input | 32 | Store data. Byte and halfword stores use the low byte/halfword lanes. |
| `READ_EN` | input | 1 | Marks a read access for status/fault reporting. |
| `WRITE_EN` | input | 1 | Enables a rising-edge write when the access is valid. |
| `SIZE` | input | 2 | `00` byte, `01` halfword, `10` word, `11` invalid. |
| `SIGN_EXTEND` | input | 1 | Sign-extends byte/halfword loads when high; zero-extends when low. |
| `CLK` | input | 1 | Rising-edge write clock. |
| `RST` | input | 1 | Clears the memory contents to zero. |
| `READ_DATA` | output | 32 | Loaded value after byte/halfword extension. |
| `READY` | output | 1 | Always high. |
| `FAULT` | output | 1 | High for invalid size, misalignment, or out-of-range access while an access is requested. |

Implementation:

- Inherits from `BasicComponent`.
- Has no child components.
- Stores bytes as `LogicValue` lanes, so unknown data bits can be preserved.
- Uses little-endian layout: the low byte of a word is stored at the lowest address.
- Uses a one-tick output delay.

Address map:

```text
ADDR 0x00000000 -> WORD_0 byte 0
ADDR 0x00000004 -> WORD_1 byte 0
...
ADDR 0x0003fffc -> WORD_65535 byte 0
```

Supported RV32I-style accesses:

```text
SIZE=00: byte load/store
SIZE=01: halfword load/store, requires ADDR[0]=0
SIZE=10: word load/store, requires ADDR[1:0]=00
SIZE=11: FAULT
```

Behavior:

```text
if RST = 1:
    all bytes become 0
else on rising CLK:
    if WRITE_EN = 1 and FAULT = 0:
        SIZE=00 stores WRITE_DATA[7:0]
        SIZE=01 stores WRITE_DATA[15:0]
        SIZE=10 stores WRITE_DATA[31:0]

READ_DATA = loaded byte/halfword/word, extended according to SIGN_EXTEND
READY = 1
FAULT = (READ_EN or WRITE_EN) and invalid_access
```

Important behavior:

- This is the memory size intended for the first practical RV32I runner.
- The capacity is 256 KiB because `64Kx32` means 64K 32-bit words.
- Byte and halfword accesses are supported here, unlike the structural teaching slices.
- Invalid writes are blocked.
- `READ_EN=0` and `WRITE_EN=0` suppress `FAULT` reporting.

Tests and visualizer:

- Test: `BehavioralMemory64Kx32Test`
- Visualizer scenario: `behavioral-memory64kx32`
- Checkpoints: reset read, word store/load at base and final word, disabled write hold, byte store, halfword store, LBU/LB/LHU/LH extension behavior, invalid-size fault, misalignment faults, out-of-range fault, no-access fault suppression, blocked faulted write, final reset.
- Hosted visualizer stats: 1 component, 11 pins, 11 wires, 83 timestamps.

## Shared Test And Visualizer Coverage

Memory test files:

- `include/tests/MemoryComponentTests.hpp`
- `src/tests/MemoryComponentTests.cpp`
- `include/tests/ArithmeticLogicTests.hpp`
- `src/tests/ArithmeticLogicTests.cpp`

Visualizer and binding files touched:

- `src/bindings/ModulesBinding.cpp`
- `src/bindings/TestsBinding.cpp`
- `src/tests/TestRegistry.cpp`
- `visualizer-v2/server.py`

Registered scenarios:

- `decoder2to4` -> `Decoder2to4Test`
- `decoder5to32` -> `Decoder5to32Test`
- `memory-bit` -> `MemoryBitTest`
- `behavioral-memory-bit` -> `BehavioralMemoryBitTest`
- `behavioral-memory64kx32` -> `BehavioralMemory64Kx32Test`
- `register32` -> `Register32Test`
- `register-file4x32` -> `RegisterFile4x32Test`
- `register-file32x32` -> `RegisterFile32x32Test`
- `behavioral-register-file32x32` -> `BehavioralRegisterFile32x32Test`
- `behavioral-register-file32x32-unknown` -> `BehavioralRegisterFile32x32UnknownTest`
- `memory4x32` -> `Memory4x32Test`
- `memory32x32` -> `Memory32x32Test`

`Mux4to1_32bitTest` exists because `RegisterFile4x32` and `Memory4x32` rely on 32-bit 4-to-1 muxing. `Mux32to1_32bitTest` exists because `RegisterFile32x32` and `Memory32x32` rely on 32-bit 32-to-1 muxing. `Decoder2to4Test` and `Decoder5to32Test` directly cover the reusable write-selection decoders used by the register-file and memory components.

## Verification Results

Commands run:

```bash
cmake -S . -B build
cmake --build build -j 8
ctest --test-dir build --output-on-failure -R "Decoder2to4Test|Decoder5to32Test|RegisterFile4x32Test|RegisterFile32x32Test|Memory4x32Test"
ctest --test-dir build --output-on-failure -R "BehavioralRegisterFile32x32Test|BehavioralRegisterFile32x32UnknownTest|RegisterFile32x32Test"
ctest --test-dir build --output-on-failure -R "BehavioralMemoryBitTest|MemoryBitTest|Register32Test|RegisterFile4x32Test|RegisterFile32x32Test|BehavioralRegisterFile32x32Test|BehavioralRegisterFile32x32UnknownTest|Memory4x32Test|Decoder2to4Test|Decoder5to32Test"
ctest --test-dir build --output-on-failure -R "Memory4x32Test|Memory32x32Test|Decoder2to4Test|Decoder5to32Test|Mux4to1_32bitTest|Mux32to1_32bitTest"
ctest --test-dir build --output-on-failure -R "BehavioralMemory64Kx32Test|Memory32x32Test|Memory4x32Test"
ctest --test-dir build -N
ctest --test-dir build --output-on-failure
curl -fsSL http://127.0.0.1:8765/api/health
curl -fsSL http://127.0.0.1:8765/api/scenarios
```

Results:

- Build passed.
- `Decoder2to4Test` passed.
- `Decoder5to32Test` passed.
- `MemoryBitTest` passed.
- `BehavioralMemoryBitTest` passed.
- `BehavioralMemory64Kx32Test` passed.
- `Register32Test` passed.
- `RegisterFile4x32Test` passed.
- `RegisterFile32x32Test` passed.
- `BehavioralRegisterFile32x32Test` passed.
- `BehavioralRegisterFile32x32UnknownTest` passed.
- `Memory4x32Test` passed.
- `Memory32x32Test` passed.
- Focused decoder/register/memory subset passed: 5/5 in 48.99 seconds.
- Focused structural/behavioral 32x32 register-file subset passed: 3/3 in 9.73 seconds.
- Previous memory-stack subset passed: 10/10 in 11.55 seconds.
- Current focused memory/mux/decoder subset passed: 6/6 in 32.38 seconds.
- Current behavioral/structural memory subset passed: 3/3 in 6.35 seconds.
- Current CTest registry contains 84 tests.
- Full branch regression passed: 84/84 in 128.80 seconds.
- Hosted visualizer health check passed.
- Hosted visualizer exposes `behavioral-memory64kx32`.
- Hosted visualizer exposes `behavioral-register-file32x32`.
- Hosted visualizer exposes `behavioral-register-file32x32-unknown`.
- Hosted visualizer exposes `memory4x32`.
- Hosted visualizer exposes `memory32x32`.
- Previous full branch regression, before adding `BehavioralRegisterFile32x32Test`, passed: 80/80 in 1646.85 seconds.

## Branch Closeout

Memory component work is complete for this branch:

- Structural teaching components exist for one-bit storage, one-word storage, small register files, and small memories.
- Behavioral CPU-scale components exist for the 32-entry RV32I register file and 64K-word memory.
- Tests and visualizer scenarios are registered for the new memory components.
- The first RV32I runner should instantiate two `BehavioralMemory64Kx32` components: one instruction memory and one data memory.

Deferred to the RV32I integration branch:

- Optional program/data preload API for `BehavioralMemory64Kx32`, used by test loaders before simulation starts.
- Optional debug readback helpers for memory contents, used by tests after simulation ends.
- CPU/system wiring that connects instruction memory and data memory instances to the RV32I datapath.

RV32I runner gaps that remain outside this branch:

- Program counter.
- Instruction memory integration.
- Instruction decoder and control unit.
- Load/store unit.
- Branch/jump control.
- CPU top-level datapath.
- Single-cycle runner tests with real assembly programs.

Scale and visualization notes:

- `Register32` is one 32-bit storage word, not the 32-entry RV32I register file.
- `RegisterFile4x32` is a four-entry teaching prototype, not the full 32-entry RV32I register file.
- `RegisterFile32x32` is the full RV32I register-file shape, but its storage cells are behavioral.
- `BehavioralRegisterFile32x32` is the same external register-file contract compressed into one behavioral component for CPU-scale use.
- `Memory4x32` is addressable RAM, but only a 16-byte teaching slice.
- `Memory32x32` is addressable RAM with 32 words / 128 bytes, still too small for real program execution.
- `BehavioralMemory64Kx32` is the practical 256 KiB memory for actual RV32I program execution.
