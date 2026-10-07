# § 4.3.1. `Register<W>`

`Register<W>` is the width-parametric, whole-word register used by the RV32I circuitry. It groups W [FlipFlopCell](../../memory/3.1.1.md) storage bits under one write enable. RV32I uses it for the program counter, pipeline payloads, register-file words, request payloads, and compact control/status fields. It has no register addressing, byte mask, or memory transaction interface.

Supported widths are **1, 2, 3, 4, 5, 8, 16, and 32 bits**. W is the number of bits in one stored value, not the number of registers; other widths are not registered. The embedded simulation uses `Register<32>` as a representative instance.

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| `Register<W>` | One resettable W-bit register with a whole-word write enable | Yes | Yes |
| `FlipFlopCell` | Reused write-enabled, resettable storage bit | Yes | Yes |

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

[`RegisterDirect`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/modules/memory/RegisterDirect.cpp) stores one W-bit logic vector and the previous clock value, applying the rules above with one abstract output-delay tick. The structural register can instead select behavioral `FlipFlopCell` children. Both choices retain the same external pins. Simulator output history supplies `Q` when scrubbing the timeline.

#### RV32I integration

| Consumer | Width / use |
| --- | --- |
| Single-cycle Fetch | 32: `PC_STATE`, the current instruction address |
| Standalone control-flow unit | 32: retained PC for its combined reference datapath |
| Pipeline Fetch | 32: `PC_STATE`, the next fetch address |
| IF/ID, ID/EX, EX/MEM, MEM/WB registers | 32 for full-width fields; supported smaller widths for packed status, control, and register addresses |
| Execution status | 4: `TRAP_CAUSE_WORD` |
| Memory requesters | Retained request fields, sized to their actual payload widths |
| Register file | 31 instances of `Register<32>`, one for each writable architectural register |

Users select `circuit::families::registerWord(W)`, registered as `memory.register.W` with W replaced by the supported width. The component is introduced here as an RV32I building block; its implementation is also reused by response buffers. Neither use changes its pins or adds a transaction protocol. The standalone structural source is [`Register.hpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/include/modules/memory/Register.hpp) and [`Register.cpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/modules/memory/Register.cpp).

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
