# § 4.3.1. `Register<W>`

`Register<W>` is the width-parametric, whole-word register used by the RV32I circuitry. It groups W [FlipFlopCell](../../memory/3.1.1.md) storage bits under one write enable. RV32I uses it for the program counter, pipeline payloads, register-file words, request payloads, and compact control/status fields. It has no register addressing, byte mask, or memory transaction interface.

Supported widths are **1, 2, 3, 4, 5, 8, 16, and 32 bits**. W is the number of bits in one stored value, not the number of registers; other widths are not registered. The embedded simulation uses `Register<32>` as a representative instance.

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| `Register<W>` | One resettable W-bit register with a whole-word write enable | Yes | Yes |
| `ClockedStateBit` | One resettable bit that captures on every clock edge, for handshake state | Yes | Yes |
| `FlipFlopCell` | Reused write-enabled, resettable storage bit | Yes | Yes |

### Storage and memory

The RV32I circuitry keeps state in two different kinds of component.

| | Storage (this section) | Memory ([chapter 3](../../memory/3.1/index.md)) |
| --- | --- | --- |
| Access | Every bit is used every cycle, in parallel | One word at a time, chosen by an address |
| Width | Any supported width, including 1 to 5 bits | Whole bytes: 8, 16, or 32 bits |
| Write control | One enable for the whole value | An enable plus one mask bit per byte |
| Technology | Flip-flops only | Flip-flop, SRAM, DRAM, ROM, or NOR flash |

`Register<W>` and `ClockedStateBit` are storage: the program counter, pipeline fields, flags, and handshake state have no address and are always flip-flops. Instruction and data memory are memory, and their technology can be chosen. The register file ([4.3.2](register-file.md)) sits between the two: its words are registers because they are only ever written whole, and the decoder and read selectors around them supply the addressing.

Storage components are catalogued as `storage.register.W` and `storage.state-bit`. `FlipFlopCell` is the flip-flop technology's memory cell (`memory.ff.cell`); registers are built from it, and the CPU also uses it directly for single flags.

## 4.3.1.1. `Register<W>`

### IO pins

| Pin | Direction | Width | Meaning |
| --- | --- | ---: | --- |
| `D` | Input | W | Next value to capture |
| `WE` | Input | 1 | Enable replacement of the entire word |
| `CLK` | Input | 1 | Capture on a known low-to-high transition |
| `RST` | Input | 1 | Active-high asynchronous reset to zero |
| `Q` | Output | W | Currently stored value |

There is no external address, byte mask, or request/response handshake. `D` is a proposed next value; changing it does not immediately change `Q`.

### Behavior

| Condition | Effect |
| --- | --- |
| Power-up, before reset or capture | Stored bits are unknown |
| `RST = 1` | Clear all W bits without waiting for a clock edge |
| `RST = 0`, rising `CLK`, `WE = 1` | Capture `D` in all W bits |
| `RST = 0`, rising `CLK`, `WE = 0` | Retain the previous value |
| No rising edge, with reset inactive | Retain the previous value, including while CLK stays high |

Reset has priority over writing. Before capture, consumers see the old `Q`; after capture and propagation, they see the new `Q`. A PC update does not need to read this new value to finish the instruction: Fetch supplies the current PC to the combinational datapath, and Next PC prepares the value for the later capture edge.

#### C++ example

This standalone reference covers every supported width, including initialization, edge detection, reset priority, write enable, and `X`/`Z` handling. Bits are least-significant first. Call `evaluate` after inputs change, preparing data and controls before a rising edge. It returns the settled result immediately; simulator scheduling, propagation delays, and history are omitted.

```cpp
#include <array>
#include <cstddef>

namespace register_example {
enum class Bit { L, H, X, Z };
template<std::size_t W> using Word = std::array<Bit, W>;
Bit known(Bit b) { return b == Bit::L || b == Bit::H ? b : Bit::X; }

template<std::size_t W>
class Register {
    static_assert(W == 1 || W == 2 || W == 3 || W == 4 ||
                  W == 5 || W == 8 || W == 16 || W == 32);
    Word<W> stored;
    Bit previous_clock = Bit::X;
public:
    Register() { stored.fill(Bit::X); }
    Word<W> evaluate(const Word<W>& data, Bit write, Bit clock, Bit reset) {
        if (reset == Bit::H) stored.fill(Bit::L);
        else if (reset != Bit::L) {
            for (auto& bit : stored)
                if (bit != Bit::L) bit = Bit::X;
        } else if (previous_clock == Bit::L && clock == Bit::H) {
            for (std::size_t bit = 0; bit < stored.size(); ++bit) {
                if (write == Bit::H) stored[bit] = known(data[bit]);
                else if (write != Bit::L)
                    stored[bit] = stored[bit] == Bit::L && data[bit] == Bit::L
                        ? Bit::L : Bit::X;
            }
        }
        previous_clock = clock;
        return stored;
    }
};
} // namespace register_example
```

#### Unknown inputs and timing

Captured `Z` data becomes `X`. An uncertain reset (`X` or `Z`) preserves stored zeros and makes other bits unknown. With reset inactive, an uncertain write enable follows the FF cell's AND/OR feedback mux: old zero/new zero stays zero; every other pair becomes `X`, including old one/new one. This is intentionally not an optimistic merge of two candidate values.

The behavioral implementation recognizes only known `CLK = 0 → 1` edges. Ambiguous clocks and simultaneous clock/data/control changes are outside the cross-fidelity timing contract; this is not an analog setup/hold or metastability model. Allow the structural data/enable paths to settle before capture. Structural and behavioral implementations are compared at settled checkpoints, not at each intermediate gate transition.

### Implementation

#### Structural implementation

<!-- diagram: register32 -->

| Part | Connection / responsibility |
| --- | --- |
| `INPUT_BITS` | Splits external `D[W]` into individual bits when W > 1 |
| `BIT_0`–`BIT_(W-1)` | W `FlipFlopCell` instances sharing `WE`, `CLK`, and `RST` |
| `OUTPUT_BITS` | Joins cell outputs into `Q[W]` when W > 1 |

W=1 connects `D` and `Q` directly to `BIT_0`, without a splitter or joiner. There is no nested register wrapper: expanding any structural register directly reveals its W resettable FF cells. Selecting behavioral fidelity keeps one word-level state object.

Unlike [FFWord&lt;W&gt;](../../memory/3.1.1.md), this register has no byte mask or byte-enable gates. `WE` updates every bit together, including for sub-byte control fields. Both components reuse the same FF cell; they provide different write interfaces, not different storage technologies.

#### Behavioral implementation

[`RegisterBehavioral`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/modules/storage/RegisterBehavioral.cpp) stores one W-bit logic vector and the previous clock value, applying the rules above. Catalog construction attaches gate-derived clock-to-output and setup timing; a supported delay twin can time the current state and inputs. The structural register can instead select behavioral `FlipFlopCell` children. Both choices retain the same external pins. Simulator output history supplies `Q` when scrubbing the timeline.

#### RV32I integration

| Consumer | Width / use |
| --- | --- |
| Single-cycle Fetch | 32: `PC_STATE`, the current instruction address |
| Pipeline Fetch | 32: `PC_STATE`, the next fetch address |
| IF/ID, ID/EX, EX/MEM, MEM/WB registers | 32 for full-width fields; supported smaller widths for packed status, control, and register addresses |
| Execution status | 4: `TRAP_CAUSE_WORD` |
| Memory requesters | Retained request fields, sized to their actual payload widths |
| Register file | 31 instances of `Register<32>`, one for each writable architectural register |

Users select `circuit::families::registerWord(W)`, registered as `storage.register.W` with W replaced by the supported width. The component is introduced here as an RV32I building block; its implementation is also reused by response buffers. Neither use changes its pins or adds a transaction protocol. The standalone structural source is [`Register.hpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/include/modules/storage/Register.hpp) and [`Register.cpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/modules/storage/Register.cpp).

### Examples

Every checkpoint of the 32-bit `RegisterTest` appears below, with checkpoint number and simulation time. It illustrates the same edge/reset/enable rules used at every supported width; `RegisterWidthsTest` checks the other widths. The rows are successive observations of one stateful sequence, not independent input combinations. Clock preparation occurs between checkpoints. Times belong to the embedded simulation's default profile, not a universal register latency.

<!-- simulator-examples: register32 RegisterTest -->

#### Embedded simulation

The sequence checks reset, disabled writes, capture versus clock-high hold, alternating and walking-bit patterns, `X`/`Z` data, uncertain enables, and asynchronous reset priority/recovery. Expand the register to inspect its cells.

<div class="simulator-embed">
  <iframe src="../../../simulator/?scenario=register32&amp;embed=1" title="Register&lt;W&gt; simulation, W=32" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../simulator/?scenario=register32" class="md-button">Open the full visualizer</a>

### Tests

| Test | Coverage |
| --- | --- |
| [`RegisterTest`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/tests/MemoryComponentTests.cpp) | Registered structural/behavioral scenario; every bit, whole-word writes, hold, reset, selected X/Z cases |
| [`RegisterFFIntegrationTest`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/tests/RegisterFFIntegrationTest.cpp) | Context-free 32-bit construction, structural cell hierarchy, fidelity choices, reverse history, and programs 03/05/07 in the single-cycle and advanced pipeline cores |
| [`RegisterDocumentationTest`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/tests/RegisterDocumentationTest.py) | Compiles this exact width-parametric C++ example, checks every declared 32-bit scenario action, and replays those bit-lane checks at every supported width |
| [`RegisterWidthsTest`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/tests/MemoryEndpointTests.cpp) | Structural/behavioral equivalence at widths 1/2/3/4/5/8/16/32, including unknown values and reset/enable behavior |
| `FlipFlopCellTest` | Reused lower-level storage-bit contract |

## 4.3.1.2. `ClockedStateBit`

`ClockedStateBit` is one flip-flop with no write enable: it takes a new value on every rising clock edge. Handshake logic computes the next state on every cycle, so it needs no hold path. Use `FlipFlopCell` or `Register<W>` when a value must be kept across cycles.

### IO pins

| Pin | Direction | Width | Meaning |
| --- | --- | ---: | --- |
| `D` | Input | 1 | Next state |
| `CLK` | Input | 1 | Capture on a known low-to-high transition |
| `RST` | Input | 1 | Active-high asynchronous reset to zero |
| `Q` | Output | 1 | Current state |

### Behavior

| Condition | Effect |
| --- | --- |
| Power-up, before reset or capture | The stored bit is unknown |
| `RST = 1` | Clear the bit without waiting for a clock edge |
| `RST = 0`, rising `CLK` | Capture `D`; `X` or `Z` data is stored as unknown |
| No rising edge, with reset inactive | Retain the previous value |

Reset has priority over capture. An uncertain reset (`X` or `Z`) leaves a stored zero unchanged and makes a stored one unknown.

### Implementation

#### Structural implementation

One `DFlipFlop` named `STATE`, with `D`, `CLK`, and `RST` wired straight in and `Q` straight out. Compared with `FlipFlopCell`, there is no write-enable multiplexer in front of the flip-flop.

#### Behavioral implementation

[`ClockedStateBitBehavioral`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/modules/storage/ClockedStateBit.cpp) stores one logic value and the previous clock level and applies the rules above. A catalog-selected instance uses clock-to-output and setup delays characterized from the structural state bit.

#### Where it is used

| Consumer | State bits |
| --- | --- |
| [Memory requesters](memory-access.md) | `BUSY`, `SENT`, `ABANDONED` |
| [`MemoryTransactionPort`](../../memory/3.2.md) | `PENDING` |
| [`MemoryResponseBuffer`](../../memory/3.2.md) | `OCCUPIED` |

Users select `circuit::families::ClockedStateBit`, registered as `storage.state-bit`.

### Examples

`ClockedStateBit` has no standalone simulation page. Its recorded behavior is visible inside the memory requester and transaction port simulations linked above: expand `BUSY` or `PENDING` to see the flip-flop.

### Tests

| Test | Coverage |
| --- | --- |
| [`ClockedStateBitTest`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/tests/MemoryEndpointTests.cpp) | Reset, capture of one and zero, hold while the clock stays high or falls, and asynchronous reset, compared between the structural and behavioral implementations |
