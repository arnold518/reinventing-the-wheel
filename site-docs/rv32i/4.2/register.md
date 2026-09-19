# § 4.2.1. Register32

`Register32` holds one 32-bit value for the program counter, pipeline payloads, and other retained CPU state. It uses the same [FF word and cell hierarchy](../../memory/3.1.1.md) as the [architectural register file](register-file.md), without register addressing or read selectors. It is a direct register, not a memory transaction endpoint.

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| `Register32` | One resettable 32-bit register with a whole-word write enable | Yes | Yes |
| `FFWord<32>` | Reused word holding all 32 bits; all four byte lanes are enabled | Yes | Yes |
| `FlipFlopCell` | Reused write-enabled, resettable storage bit | Yes | Yes |

## 4.2.1.1. `Register32`

### IO pins

| Pin | Direction | Width | Meaning |
| --- | --- | ---: | --- |
| `D` | Input | 32 | Next value to capture |
| `WE` | Input | 1 | Enable replacement of the entire word |
| `CLK` | Input | 1 | Capture on a known low-to-high transition |
| `RST` | Input | 1 | Active-high asynchronous reset to zero |
| `Q` | Output | 32 | Currently stored value |

There is no external address, byte mask, or request/response handshake. `D` is a proposed next value; changing it does not immediately change `Q`.

### Behavior

| Condition | Effect |
| --- | --- |
| Power-up, before reset or capture | Stored bits are unknown |
| `RST = 1` | Clear all 32 bits without waiting for a clock edge |
| `RST = 0`, rising `CLK`, `WE = 1` | Capture `D` in all 32 bits |
| `RST = 0`, rising `CLK`, `WE = 0` | Retain the previous value |
| No rising edge, with reset inactive | Retain the previous value, including while CLK stays high |

Reset has priority over writing. Before capture, consumers see the old `Q`; after capture and propagation, they see the new `Q`. A PC update does not need to read this new value to finish the instruction: Fetch supplies the current PC to the combinational datapath, and Next PC prepares the value for the later capture edge.

#### C++ example

This standalone reference includes initialization, edge detection, reset priority, write enable, and `X`/`Z` handling. Bits are least-significant first. Call `evaluate` after inputs change, preparing data and controls before a rising edge. It returns the settled result immediately; simulator scheduling, propagation delays, and history are omitted.

```cpp
#include <array>
#include <cstddef>

namespace register32_example {
enum class Bit { L, H, X, Z };
using Word = std::array<Bit, 32>;
Bit known(Bit b) { return b == Bit::L || b == Bit::H ? b : Bit::X; }

class Register {
    Word stored;
    Bit previous_clock = Bit::X;
public:
    Register() { stored.fill(Bit::X); }
    Word evaluate(const Word& data, Bit write, Bit clock, Bit reset) {
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
} // namespace register32_example
```

#### Unknown inputs and timing

Captured `Z` data becomes `X`. An uncertain reset (`X` or `Z`) preserves stored zeros and makes other bits unknown. With reset inactive, an uncertain write enable follows the FF cell's AND/OR feedback mux: old zero/new zero stays zero; every other pair becomes `X`, including old one/new one. This is intentionally not an optimistic merge of two candidate values.

The behavioral implementation recognizes only known `CLK = 0 → 1` edges. Ambiguous clocks and simultaneous clock/data/control changes are outside the cross-fidelity timing contract; this is not an analog setup/hold or metastability model. Allow the structural data/enable paths to settle before capture. Structural and behavioral implementations are compared at settled checkpoints, not at each intermediate gate transition.

### Implementation

#### Structural implementation

```text
Register32
├── FULL_WRITE_MASK       constant 1111
└── STORAGE               FFWord<32>
    ├── DATA / MASK       data and byte-mask splitters
    ├── BYTE_0 … BYTE_3   WE AND the corresponding mask bit
    ├── CELL_0 … CELL_31  FlipFlopCell
    └── OUTPUT            joins the stored bits into Q
```

| Part | Connection / responsibility |
| --- | --- |
| `STORAGE.WRITE_DATA` | Receives the external `D` bus |
| `STORAGE.WRITE_MASK` | Receives `1111` from `FULL_WRITE_MASK`; every enabled write replaces the whole word |
| `STORAGE.WE`, `CLK`, `RST` | Connected directly to the corresponding external controls |
| `STORAGE.Q` | Drives the external `Q` bus |

The wrapper contains no independent storage. Expanding `STORAGE` reveals the same byte-organized FF word used inside `RegisterFile32x32`. A single register does not need a storage bank, address decoder, or memory controller.

#### Behavioral implementation

[`Register32Direct`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/modules/memory/Register32Direct.cpp) stores one 32-bit logic vector and the previous clock value, applying the rules above with one abstract output-delay tick. The structural wrapper can instead select a behavioral `STORAGE` word, or behavioral cells within a structural word. All choices retain the same external pins. Simulator output history supplies `Q` when scrubbing the timeline.

#### RV32I integration

| Consumer | Use |
| --- | --- |
| Single-cycle Fetch | `PC_STATE`: current instruction address |
| Standalone control-flow unit | `PC_STATE`: retained PC for its combined reference datapath |
| Pipeline Fetch | `PC_STATE`: next fetch address |
| IF/ID, ID/EX, EX/MEM, MEM/WB registers | One `Register32` per 32-bit payload field; valid bits use separate cells |
| Pipeline retirement | Committed PC, trap cause, retirement count, and recorded instruction/memory metadata |

These users retain the `memory.register.width32` family and now resolve to this implementation. `RegisterFile32x32` continues to use a shared FF bank directly; it does not construct 31 separate `Register32` wrappers.

### Examples

Every checkpoint of `Register32Test` appears below, with checkpoint number and simulation time. The rows are successive observations of one stateful sequence, not independent input combinations. Clock preparation occurs between checkpoints. Times belong to the embedded simulation's default profile, not a universal register latency.

<!-- simulator-examples: register32 Register32Test -->

#### Embedded simulation

The sequence checks reset, disabled writes, capture versus clock-high hold, alternating and walking-bit patterns, `X`/`Z` data, uncertain enables, and asynchronous reset priority/recovery. Expand `STORAGE` to inspect the four byte lanes and their cells.

<div class="simulator-embed">
  <iframe src="../../../simulator/?scenario=register32&amp;embed=1" title="Register32 simulation" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../simulator/?scenario=register32" class="md-button">Open the full visualizer</a>

### Tests

| Test | Coverage |
| --- | --- |
| [`Register32Test`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/tests/MemoryComponentTests.cpp) | Registered structural/behavioral scenario; every bit, whole-word writes, hold, reset, selected X/Z cases |
| [`Register32FFIntegrationTest`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/tests/Register32FFIntegrationTest.cpp) | Context-free construction, FF word/cell hierarchy, three storage fidelity choices against the behavioral register, reverse history, and programs 03/05/07 in both RV32I cores |
| [`Register32DocumentationTest`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/tests/Register32DocumentationTest.py) | Compiles this exact C++ example and checks every declared scenario checkpoint |
| `FFWordContractTest`, `FlipFlopCellTest` | Reused lower-level storage contracts |
