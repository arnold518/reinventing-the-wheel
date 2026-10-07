# § 4.3.2. Register File

`RegisterFile32x32` provides the RV32I core's two source-register reads and one destination-register write. It uses the [whole-word FF register](register.md), with register-address decoding and two read selectors around a shared bank. It is not a transaction memory endpoint: register operands are read directly, without requests, responses, or a memory controller.

Its fixed register count, two-read/one-write interface, and hardwired x0 make it an RV32I-oriented building block. The FF cells are described in [3.1.1](../../memory/3.1.1.md), and the whole-word register family in [4.3.1](register.md); this page defines the CPU-facing organization built around them.

The [single-cycle Decode stage](../4.4/decode.md) owns this component at `CORE.DECODE.REGISTER_FILE`. Writeback only selects its input value; it does not own a second register file. Execution status qualifies the actual write enable.

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| `RegisterFile32x32` | 32 architectural registers, two asynchronous reads, one clocked write; x0 is always zero | Yes | Yes |
| `RegisterFileStorageBank` | Parallel bank of 31 writable whole-word registers | Yes | No |
| `Register<32>` | One resettable, whole-word register per writable architectural register | Yes | Yes |
| `FlipFlopCell` | Reused write-enabled, resettable storage bit | Yes | Yes |

The bank fixes its depth at 31 because x0 is a constant. `Register<32>` is reusable independently of this RV32I-specific decoder and read-port arrangement.

## 4.3.2.1. `RegisterFile32x32`

### IO pins

| Pin | Direction | Width | Meaning |
| --- | --- | ---: | --- |
| `RS1_ADDR` | Input | 5 | First source register, x0–x31 |
| `RS2_ADDR` | Input | 5 | Second source register, x0–x31 |
| `RD_ADDR` | Input | 5 | Destination register, x0–x31 |
| `WRITE_DATA` | Input | 32 | Full replacement value for the destination |
| `REG_WRITE` | Input | 1 | Enable destination write |
| `CLK` | Input | 1 | Capture on a known low-to-high transition |
| `RST` | Input | 1 | Active-high asynchronous reset; clears x1–x31 |
| `RS1_DATA` | Output | 32 | Value selected by RS1_ADDR |
| `RS2_DATA` | Output | 32 | Value selected by RS2_ADDR |

Addresses are **register indices**, not byte addresses. There is no external byte mask: every enabled write replaces all 32 bits.

### Behavior

| Condition | Effect |
| --- | --- |
| Power-up, before reset | x1–x31 are unknown; x0 is zero |
| `RST = 1` | Clear every writable register, without waiting for a clock edge |
| `RST = 0`, rising CLK, `REG_WRITE = 1`, `RD_ADDR != 0` | Capture WRITE_DATA in the selected register |
| Write to x0 | Ignore the write; both read ports still return zero when selecting x0 |
| `REG_WRITE = 0`, or no rising edge | Retain storage |
| Source address changes | Update the corresponding read output after propagation; no clock edge required |
| Read and write the same register | Read the old value before capture and the new value after capture and propagation |

Reset has priority over writing. There is no separate write-through bypass or same-cycle forwarding circuit inside this component; pipeline forwarding belongs to the CPU. Both read ports can select the same register or two different registers independently.

#### C++ example

This standalone reference shows the stored state, clock detection, reset priority, decoder, and both read-mux trees, including `X`/`Z`. Bits and address bits are least-significant first. Call `evaluate` after inputs change; prepare data and controls before a rising edge. This example returns settled values immediately: it omits simulator events, propagation delays, and history recording, not the state-transition rules.

```cpp
#include <array>
#include <cstddef>
#include <utility>

namespace register_file_example {
enum class Bit { L, H, X, Z };
using Word = std::array<Bit, 32>;
using Address = std::array<Bit, 5>;

Bit known(Bit v) { return v == Bit::Z ? Bit::X : v; }
Bit invert(Bit v) {
    return v == Bit::L ? Bit::H : v == Bit::H ? Bit::L : Bit::X;
}
Bit both(Bit a, Bit b) {
    if (a == Bit::L || b == Bit::L) return Bit::L;
    return a == Bit::H && b == Bit::H ? Bit::H : Bit::X;
}
Bit either(Bit a, Bit b) {
    if (a == Bit::H || b == Bit::H) return Bit::H;
    return a == Bit::L && b == Bit::L ? Bit::L : Bit::X;
}
Bit mux(Bit a, Bit b, Bit select) {
    return either(both(a, invert(select)), both(b, select));
}
Bit matches(const Address& address, std::size_t index) {
    Bit result = Bit::H;
    for (std::size_t b = 0; b < 5; ++b)
        result = both(result, (index >> b) & 1 ? known(address[b]) : invert(address[b]));
    return result;
}

class RegisterFile {
    std::array<Word, 32> registers;
    Bit previous_clock = Bit::X;

    Word read(const Address& address) const {
        auto level = registers;
        std::size_t count = 32;
        for (std::size_t a = 0; a < 5; ++a) {
            for (std::size_t pair = 0; pair < count / 2; ++pair)
                for (std::size_t b = 0; b < 32; ++b)
                    level[pair][b] = mux(level[2 * pair][b], level[2 * pair + 1][b], address[a]);
            count /= 2;
        }
        return level[0];
    }
public:
    RegisterFile() {
        for (auto& word : registers) word.fill(Bit::X);
        registers[0].fill(Bit::L);
    }
    std::pair<Word, Word> evaluate(const Address& rs1, const Address& rs2,
                                  const Address& rd, const Word& data,
                                  Bit write, Bit clock, Bit reset) {
        clock = known(clock);
        reset = known(reset);
        for (std::size_t reg = 1; reg < 32; ++reg) {
            auto& word = registers[reg];
            if (reset == Bit::H) word.fill(Bit::L);
            else if (reset != Bit::L) {
                for (auto& b : word) if (b != Bit::L) b = Bit::X;
            } else if (previous_clock == Bit::L && clock == Bit::H) {
                const Bit enable = both(write, matches(rd, reg));
                if (enable != Bit::L)
                    for (std::size_t b = 0; b < 32; ++b)
                        word[b] = mux(word[b], known(data[b]), enable);
            }
        }
        previous_clock = clock;
        return {read(rs1), read(rs2)};
    }
};
} // namespace register_file_example
```

#### Unknown inputs and timing

Captured `Z` data becomes `X`. An uncertain reset preserves stored zeros and makes other bits unknown. An uncertain destination address can affect multiple candidate registers; known address bits exclude nonmatching registers. A definitely disabled write does not modify any register.

An uncertain write enable follows the gate-built feedback mux: old zero/new zero stays zero; every other pair becomes `X`, including old one/new one. Read selection likewise follows five layers of AND/OR muxes, not an optimistic merge of possible register values. Consequently, selecting between identical ones with an unknown selector can produce `X`; identical zeros remain zero.

The behavioral implementation detects only known `CLK = 0 → 1` transitions. Ambiguous clocks and simultaneous clock/data/control changes are not a setup/hold or metastability model. Structural and behavioral implementations are compared at settled checkpoints, not at every intermediate gate transition. Absolute checkpoint times differ by fidelity.

### Implementation

#### Structural implementation

<!-- diagram: register-file -->

| Part | Connection / responsibility |
| --- | --- |
| `RD_DECODER` | `Decoder5to32`: ADDR receives RD_ADDR; ENABLE receives REG_WRITE |
| `STORAGE.WRITE_n` | Connected to decoder OUT(n+1); OUT0 is intentionally unused |
| `STORAGE.WORD_n` | Holds x(n+1); fans out to input (n+1) of both read muxes |
| Shared storage pins | WRITE_DATA, CLK, RST fan out to the 31 whole-word registers |
| `X0_ZERO` | Drives input 0 of both read muxes |
| `RS1_MUX`, `RS2_MUX` | `Mux32to1_32bit`: independent selectors drive the two data outputs |

The bank contains **992 writable bits**, not 1,024: x0 does not need flip-flops. The bank supplies parallel word outputs, so two read selectors share the stored values. Each physical word is one [whole-word `Register<32>`](register.md), whose write enable comes directly from the decoder rather than a byte mask.

#### Behavioral implementation

[`RegisterFile32x32Direct`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/modules/rv32i/RegisterFile32x32Direct.cpp) stores 32 logic-vector words, keeps x0 zero, and implements the same decoder/write/read rules without child gates. Read outputs use one abstract delay tick. Register snapshots support the visualizer's register table and timeline scrubbing.

The structural register file can select behavioral `Register<32>` words or their `FlipFlopCell` children. The external pins and register numbering do not change. Its register-state inspector reads the bank's WORD outputs after the simulator restores the requested time.

### Examples

The table contains every checkpoint of `RegisterFile32x32Test`, including its actual simulation time and checkpoint number. Preparatory clock/control changes occur between checkpoints; the table is not a list of independent combinational inputs. Scroll horizontally to inspect all pins. Times describe the embedded simulation's default profile, not universal register-file latency.

<!-- simulator-examples: rv32i-register-file RegisterFile32x32Test -->

#### Embedded simulation

The sequence resets the file, attempts writes to x0, writes and reads every writable register, checks disabled writes, captures unknown/high-impedance data, and exercises uncertain read/write addresses and controls. Expand `STORAGE`, then `WORD_n`, to inspect its `BIT_0`–`BIT_31` cells. Physical WORD_0 is x1, and WORD_30 is x31. The bank layout orders words numerically, left to right and top to bottom.

<div class="simulator-embed">
  <iframe src="../../../simulator/?scenario=rv32i-register-file&amp;embed=1" title="RegisterFile32x32 simulation" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../simulator/?scenario=rv32i-register-file" class="md-button">Open the full visualizer</a>

### Tests

| Test | Coverage |
| --- | --- |
| [`RegisterFile32x32Test`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/tests/MemoryComponentTests.cpp) | Registered structural/behavioral scenario; all 31 writable registers, both reads, x0 protection, reset, write disable, selected X/Z cases |
| [`RegisterFileFFIntegrationTest`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/tests/RegisterFileFFIntegrationTest.cpp) | Bank/register/cell hierarchy, context-free construction, cross-fidelity scenario, uncertain-enable regression, reverse history, and program-03 in both CPU cores with the structural register file |
| [`RegisterFileDocumentationTest`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/tests/RegisterFileDocumentationTest.py) | Compiles this exact C++ example and runs every declared scenario action against its expected outputs; checks numeric bank layout order |
| `RegisterTest`, `RegisterWidthsTest`, `FlipFlopCellTest` | Reused lower-level storage contracts |

The register file retains the existing `rv32i.register-file` family, public pins, and register-state capability. No memory-bus adapter or CPU instruction-interface change is needed.
