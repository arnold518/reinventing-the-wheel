# § 3.3.2. SRAM

A single-port SRAM, from a one-bit cell to an addressable device, in two forms: an **asynchronous chip** that times its own access and raises `READY`, and a **clocked on-chip macro** that registers each request at a clock edge. Shared decoding, selection and output-enable circuits are described in [3.2 Shared Building Blocks](../building-blocks.md); the SRAM-specific bitline and access circuits are here.

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| `SRAMCell` | Stores one bit using a feedback latch and symbolic write access | Yes | Yes |
| `SRAMRow` | Groups C cells sharing one wordline | Yes | Yes |
| `SRAMBitlinePair` | Digital primitive for one shared column's precharge, sensing, and write excitation | Yes | No |
| `SRAMStorageBank<W>` | Assembles R rows and C shared bitline pairs | Yes | Yes |
| `SRAMRowDecoder` | Decodes the row portion of a word address | Yes | No |
| `SRAMPrecharge` | Distributes precharge control to the physical columns | Yes | No |
| `SRAMWriteDriver` | Generates one bit's complementary write values and masked drive enable | Yes | No |
| `SRAMSenseAmplifier` | Digital primitive that accepts a valid symbolic read differential | Yes | No |
| `SRAMColumnSelector<W>` | Routes one word's read pairs and write enables among K column groups | Yes | No |
| `SRAMAccessControl<W>` | Sequences an access with gates and delay lines and raises `READY` | Yes | No |
| `SRAMOutputBuffer<W>` | Compatibility wrapper around the common output-enable primitive | Yes | No |
| `SRAM<W>` | Complete word-addressed SRAM device, asynchronous or clocked | Yes | Yes |

“Structural” includes named **memory primitives** for the bitline pair, the sense amplifier and the output enable: they stand for electrical circuits and evaluate their digital contract directly, with a delay from the timing profile. The access sequencing is built from gates and delay lines. Cells, rows, banks and devices also have a behavioral form; structural remains the default for small geometries.

Compile the C++ examples in page order. They describe settled binary-control behavior; empty data optionals represent unknown bits, and construction geometry is assumed valid. X/Z rules are in each component's “Unknown inputs” section and delays in its “Timing” section.

## Geometry and interface conventions

- `W` is the word width: 8, 16, or 32 bits, fixed by the template.
- `N = depth_words` is the number of addressable words, supplied at construction.
- `K = words_per_row` is a power of two from 1 through 64 that divides N. Non-power-of-two depths such as N=6, K=2 are supported.
- A gate-level array keeps each bitline to at most 32 rows. A larger array is built from 32-row **subarrays**, each a complete SRAM with its own timing, joined by a global decoder, global wires (`MemoryWire`) and a shared return bus (`MemoryBus`), as real on-chip arrays are divided.
- `clocked=1` selects the on-chip macro: the device gains a `CLK` input, registers its inputs at a rising edge with `CE_N` low, and holds its data and `READY` until the next access.
- There are `R = N/K` physical rows and `C = W*K` bit columns. Capacity is `N*W/8` bytes. A word is not necessarily a whole row.
- Word address a selects row `a/K` and slot `a%K`. Slot j occupies columns `j*W` through `j*W+W−1` in this model.
- `SRAM<32>` with N=1024, K=4 has 256 rows, 128 columns, 1024 four-byte words, and 4096 bytes. Use behavioral device fidelity for that capacity rather than expanding 32,768 cells.
- The device configuration is `SRAMDeviceConfig`.
- The 32-bit word-address port and separate write/read buses are simulator-facing conventions. They are **not a pin-for-pin package model** with bidirectional DQ. The engine's single-source wires and INPUT/OUTPUT pin types are unchanged.
- `CE_N`, `WE_N`, and `OE_N` are active-low; internal enables and byte masks are active-high. X means unknown and Z means high impedance. Storage has no guaranteed initial binary value.

In the visualizer, rows run horizontally and are stacked in row-number order. Cells run left-to-right in column-number order; `BITLINE_c` sits beneath `CELL_c` in every row. The complete device places the row decoder to the left, precharge above the bank, and column selection, write drivers, and sensing below. This layout shows the organization of the digital model, not a transistor floorplan.

Conventional 6T SRAM uses wordlines, paired bitlines, precharge, write drivers, and sensing. Here the cell uses a latch-based digital representation, and a bitline pair carries a **symbolic differential**, not an electrical voltage. The block organization follows those roles without claiming transistor accuracy. See [SRAM cells and row/column organization, slides 31–42](https://islab.soe.uoguelph.ca/sareibi/TEACHING_dr/ENG241_html_dr/outline_FALL_20xx/docs/LECTURE_dr/PPT_SLIDES_dr/WEEK11_dr/ENG241-Memory-Week11-Part1.pdf).

## Construction

Native Python factories return fully initialized components; the same registered contracts are used by WebAssembly scenarios:

```python
import circuit_backend as simulator

small = simulator.create_sram("SMALL", data_width=16, depth_words=8,
                              words_per_row=2, fidelity="structural")
large = simulator.create_sram("LARGE", data_width=32, depth_words=1024,
                              words_per_row=4, fidelity="behavioral")
cell = simulator.create_sram_component("cell", "CELL")
row = simulator.create_sram_component("row", "ROW", {"columns": "16"})
```

The device contract is `memory.sram.device.W`; the bank is `memory.sram.bank.W`. Cell and row contracts are `memory.sram.cell` and `memory.sram.row`. The remaining contract suffixes are `bitline-pair`, `row-decoder`, `precharge`, `write-driver`, `sense-amplifier`, `column-selector.W`, `access-control.W`, and `output-buffer.W`. Replace W with 8, 16, or 32. Standalone row/bitline/precharge geometry uses `columns`/`rows`/`columns` respectively.

Factories reject unsupported widths, invalid geometry, unknown parameters, and unsupported requested fidelities. They do not silently expand a large device or substitute behavioral fidelity for an explicit structural request.

## Access timing

**Asynchronous chip.** No clock is needed. `SRAMAccessControl` watches the
address, `CE_N`, `WE_N`, and during a write the data and mask. Any change
restarts the access; when the inputs have been steady for the settle
interval, a chain of stages runs (each started by the previous one):

| Stage | What happens | Ends after |
| --- | --- | --- |
| Settle | The new inputs are copied to the held buses the array reads; old values stay there until now | Settle interval (never shorter than the change detector's own path) |
| Precharge | All bitline pairs are precharged | Precharge interval |
| Decode | The row decoder settles; a write starts driving its bitlines | A replica chain of gates as deep as the decoder |
| Bitline | The selected row opens; cells are written, or develop a read differential | Bitline interval, base plus a per-row term |
| Sense and ready | Sensing and, for a read with `OE_N` low, the output enable; then `READY` | Two memory-primitive delays and a few gates |

Hold the inputs steady until `READY` rises. A change before the row opens
cancels the access without writing; a change after the row has opened can
leave a finished write, which is not rolled back. `OE_N` only gates the
output and does not restart the access.

**Clocked macro.** The inputs are registered at a rising `CLK` edge with
`CE_N` low, and the same stages run from that edge, with no change detection
and no settle interval. Data and `READY` then hold until the next access, so a
controller can capture them at a later edge. The controller counts the cycles
an access needs at its clock ([3.6](../timing.md#clocked-sram-macro)).

The intervals come from the active profile. In the realistic profiles they
are multiples of the FO4 delay, compared against the CACTI SRAM model:

<!-- profile-table: memory-sram -->

<a id="timing-intervals"></a>The compact device and endpoint publish at the
delays proven on this gate-level sequence. Tests compare settled checkpoints,
not equal transient waveforms between the two forms.

## 3.3.2.1. `SRAMCell`

Stores one bit using a feedback latch and symbolic write access.

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `WL` | Input | 1 | Selects the cell's row |
| `DRIVE` | Input | 1 | Strong write excitation; not asserted for reads or precharge |
| `BL, BL_BAR` | Input | 1 each | Symbolic complementary write values |
| `Q, Q_BAR` | Output | 1 each | Stored bit and its complement |

### Behavior

With `WL = DRIVE = 1`, a complementary binary pair writes `Q = BL`. If either enable is `0`, the cell holds its previous state. Merely selecting the row does not write. The `DRIVE` signal is an explicit digital distinction between write excitation and read sensing; it is not an extra physical pin on a 6T cell.

#### C++ example

```cpp
#include <optional>
struct SRAMCellReference {
    std::optional<bool> q; // Empty means unknown, including power-up.
    std::optional<bool> q_bar() const {
        return q ? std::optional(!*q) : std::nullopt;
    }
    void update(bool wl, bool drive, std::optional<bool> bl,
                std::optional<bool> bar) {
        if (wl && drive)
            q = bl && bar && *bl != *bar ? bl : std::nullopt;
    }
};
```

#### Unknown inputs

Starts unknown. An enabled noncomplementary or X/Z write pair makes storage unknown; a later valid write restores a known bit. Unknown enables can poison state when a write is possible. The structural access primitive takes one memory-primitive delay, followed by the latch's gate delays.

### Implementation

#### Structural implementation

`ACCESS` converts symbolic write excitation into active-low set/reset controls. `STATE` is the existing `SRLatch`, containing two feedback NAND gates. This is a digital latch-based representation, **not a six-transistor electrical circuit**.

#### Behavioral implementation

A stored four-state value implements the same settled cell contract without the latch topology. `SRAMContractTest` compares both implementations.

The behavioral cell's visualizer displays recorded `Q` and `Q_BAR` values, with `WL` and `DRIVE` indicators. These follow timeline scrubbing; the structural view continues to show its access circuit and latch.

### Examples

<!-- simulator-examples: sram-cell SRAMCellTest -->

The sequence establishes zero, holds it, selects the cell for a read without writing, writes one, rejects an unselected write, captures unknown data, and recovers with a valid write.

#### Embedded simulation

The standalone `SRAMCellTest` opens this component as `SRAM_ROOT`. Use the explorer to compare structural and behavioral fidelity.

<div class="simulator-embed">
  <iframe src="../../../simulator/?scenario=sram-cell" title="SRAMCell recorded simulation" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../simulator/?scenario=sram-cell" class="md-button">Open SRAMCell in the simulator</a>

### Source and tests

- [Public interface: `SRAM.hpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/include/modules/memory/SRAM.hpp)
- [Primitive and non-template implementations: `SRAM.cpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/modules/memory/SRAM.cpp)
- [Width-dependent structural implementation: `SRAM.tpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/include/modules/memory/SRAM.tpp)
- [Behavioral implementations: `SRAMBehavioral.hpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/modules/memory/SRAMBehavioral.hpp)

| Test | Coverage |
| --- | --- |
| [`SRAMCellTest`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/tests/SRAMTests.cpp) | The complete standalone sequence shown above |
| `SRAMClockedTest`, `SRAMClockedSubarrayTest` | The clocked macro, small and split into subarrays, against its compact form |
| `SRAMContractTest` | Structural/behavioral equivalence; device and bank widths 8/16/32, multiple geometries and byte masks, mixed bank selection, and invalid configuration rejection |

## 3.3.2.2. `SRAMRow`

Groups C cells sharing one wordline.

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `WL` | Input | 1 | Shared wordline |
| `DRIVE_c` | Input | 1 each | Write excitation for column c |
| `BL_c, BL_BAR_c` | Input | 1 each | Column write values |
| `Q_c` | Output | 1 each | Each cell's stored value; feeds the column bitline model |

### Behavior

The construction parameter `columns = C` creates exactly C cells. Each cell receives the shared wordline and its own column excitation. There is no address decoder, clock, reset, or word read port inside a row. The row may contain several addressable words.

#### C++ example

```cpp
#include <cstddef>
#include <vector>
struct SRAMRowReference {
    std::vector<SRAMCellReference> cells;
    explicit SRAMRowReference(std::size_t columns) : cells(columns) {}
    void update(bool wl, const std::vector<bool>& drive,
                const std::vector<std::optional<bool>>& bl,
                const std::vector<std::optional<bool>>& bar) {
        for (std::size_t c = 0; c < cells.size(); ++c)
            cells[c].update(wl, drive.at(c), bl.at(c), bar.at(c));
    }
    std::optional<bool> q(std::size_t c) const { return cells.at(c).q; }
};
```

#### Unknown inputs

Startup and enabled unknown writes follow `SRAMCell`. `columns` accepts 1–4096; this is a simulator expansion limit, not a physical row-size claim. Inputs must settle before expecting the row's outputs to settle.

### Implementation

#### Structural implementation

Contains `CELL_0` through `CELL_(C−1)`. Cells are registered families, so a structural row can contain behavioral cells.

#### Behavioral implementation

Stores C four-state values and applies the per-column write controls directly. It keeps the same expanded pin interface.

### Examples

<!-- simulator-examples: sram-row SRAMRowTest -->

The two-column example writes different bits, deselects the row, then changes only column 1 while column 0's drive is disabled.

#### Embedded simulation

The standalone `SRAMRowTest` opens this component as `SRAM_ROOT`. Use the explorer to compare structural and behavioral fidelity.

<div class="simulator-embed">
  <iframe src="../../../simulator/?scenario=sram-row" title="SRAMRow recorded simulation" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../simulator/?scenario=sram-row" class="md-button">Open SRAMRow in the simulator</a>

### Source and tests

- [Public interface: `SRAM.hpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/include/modules/memory/SRAM.hpp)
- [Primitive and non-template implementations: `SRAM.cpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/modules/memory/SRAM.cpp)
- [Width-dependent structural implementation: `SRAM.tpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/include/modules/memory/SRAM.tpp)
- [Behavioral implementations: `SRAMBehavioral.hpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/modules/memory/SRAMBehavioral.hpp)

| Test | Coverage |
| --- | --- |
| [`SRAMRowTest`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/tests/SRAMTests.cpp) | The complete standalone sequence shown above |
| `SRAMClockedTest`, `SRAMClockedSubarrayTest` | The clocked macro, small and split into subarrays, against its compact form |
| `SRAMContractTest` | Structural/behavioral equivalence; device and bank widths 8/16/32, multiple geometries and byte masks, mixed bank selection, and invalid configuration rejection |

## 3.3.2.3. `SRAMBitlinePair`

Digital primitive for one shared column's precharge, sensing, and write excitation.

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `PRECHARGE` | Input | 1 | Charge/equalize the pair while all rows and write drivers are inactive |
| `DRIVE` | Input | 1 | Enable strong write drive |
| `WRITE_BL, WRITE_BL_BAR` | Input | 1 each | Write driver's complementary values |
| `WL_r, Q_r` | Input | 1 each | Wordline and stored bit contribution from row r |
| `BL, BL_BAR` | Output | 1 each | Symbolic bitline state, not measured voltage |
| `VALID` | Output | 1 | Known read differential is available |
| `CELL_DRIVE` | Output | 1 | Write excitation sent to the column's cells |
| `CELL_BL, CELL_BL_BAR` | Output | 1 each | Write values delivered to cells |

### Behavior

The construction parameter `rows = R` supplies one contribution per row. Precharge sets the symbolic pair to `(1,1)` and prepares it for reading. Releasing precharge retains that state. Selecting one row develops `(Q, !Q)`; `VALID` is high only for a known bit. Releasing a used row requires another precharge before the next read. During writes, the driver determines the pair and `CELL_DRIVE` is high. Cells still decide whether their wordline permits writing.

Multiple selected rows, an unknown wordline/drive/precharge, or precharge overlapping a selected row/write drive invalidate the pair and suppress cell drive. These are conservative digital rules, not a calculation of electrical contention.

#### C++ example

```cpp
#include <optional>
#include <vector>
struct SRAMBitlineReference {
    std::optional<bool> bl, bar, cell_bl, cell_bar;
    bool valid = false, cell_drive = false;
    bool prepared = false;
    std::optional<std::size_t> reading_row;
    void update(bool precharge, bool drive, std::optional<bool> write_bl,
                std::optional<bool> write_bar, const std::vector<bool>& wl,
                const std::vector<std::optional<bool>>& cells) {
        valid = cell_drive = false; cell_bl.reset(); cell_bar.reset();
        std::size_t count = 0, row = 0;
        for (std::size_t r = 0; r < wl.size(); ++r)
            if (wl[r]) { ++count; row = r; }
        auto invalidate = [&] {
            bl.reset(); bar.reset(); prepared = false; reading_row.reset();
        };
        if (count > 1 || (precharge && (drive || count))) { invalidate(); return; }
        if (precharge) {
            bl = bar = true; prepared = true; reading_row.reset();
        } else if (drive) {
            bl = cell_bl = write_bl; bar = cell_bar = write_bar;
            cell_drive = true; prepared = false; reading_row.reset();
        } else if (count) {
            if (!prepared || (reading_row && *reading_row != row)) {
                invalidate(); return;
            }
            reading_row = row; bl = cells.at(row);
            bar = bl ? std::optional(!*bl) : std::nullopt;
            valid = bl.has_value();
        } else if (reading_row) {
            prepared = false; reading_row.reset(); // Pair retains its last value.
        }
    }
};
```

#### Unknown inputs

The pair starts `(X,X)`. The example covers all binary control combinations, including conflicts, and uses empty optionals for unknown data. X/Z control inputs additionally invalidate the pair. Outputs follow after one bitline interval, omitted from the settled reference.

### Implementation

#### Structural implementation

A **memory primitive**: the explicit abstraction boundary for shared bitline behavior, with state recording through ordinary output wires. There is no multi-driver net resolver, capacitance, voltage, transistor strength, or leakage model. No separate compact implementation is registered because this primitive already performs direct evaluation.

### Examples

<!-- simulator-examples: sram-bitline-pair SRAMBitlinePairTest -->

The sequence includes precharge, retained charge, reads from both rows, a missing-precharge failure, two-row contention, write drive, and a precharge/write conflict.

#### Embedded simulation

The standalone `SRAMBitlinePairTest` opens this component as `SRAM_ROOT`.

<div class="simulator-embed">
  <iframe src="../../../simulator/?scenario=sram-bitline-pair" title="SRAMBitlinePair recorded simulation" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../simulator/?scenario=sram-bitline-pair" class="md-button">Open SRAMBitlinePair in the simulator</a>

### Source and tests

- [Public interface: `SRAM.hpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/include/modules/memory/SRAM.hpp)
- [Primitive and non-template implementations: `SRAM.cpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/modules/memory/SRAM.cpp)
- [Width-dependent structural implementation: `SRAM.tpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/include/modules/memory/SRAM.tpp)

| Test | Coverage |
| --- | --- |
| [`SRAMBitlinePairTest`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/tests/SRAMTests.cpp) | The complete standalone sequence shown above |

## 3.3.2.4. `SRAMStorageBank<W>`

Assembles R rows and C shared bitline pairs.

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `WL_r` | Input | 1 each | Row selections supplied by the decoder |
| `PRECHARGE_c` | Input | 1 each | Precharge enable per bit column |
| `DRIVE_c` | Input | 1 each | Column write drive |
| `WRITE_BL_c, WRITE_BL_BAR_c` | Input | 1 each | Column write values |
| `BL_c, BL_BAR_c, VALID_c` | Output | 1 each | Symbolic pair and read-valid result per column |

### Behavior

For W-bit words, N words, and K words per row, the bank has `R = N/K` rows and `C = W*K` columns. It contains exactly `N*W` cells. There is no complete word-address interface: row decoding, column selection, sensing, and access sequencing remain outside the bank. It does not expose every stored word as `WORD_n`, unlike `FFStorageBank`.

#### C++ example

```cpp
#include <cstddef>
#include <vector>
struct SRAMBankReference {
    std::vector<SRAMRowReference> rows;
    std::vector<SRAMBitlineReference> columns;
    SRAMBankReference(std::size_t r, std::size_t c)
        : rows(r, SRAMRowReference(c)), columns(c) {}
    void update(const std::vector<bool>& wl, const std::vector<bool>& precharge,
                const std::vector<bool>& drive,
                const std::vector<std::optional<bool>>& bl,
                const std::vector<std::optional<bool>>& bar) {
        for (std::size_t c = 0; c < columns.size(); ++c) {
            std::vector<std::optional<bool>> q;
            for (const auto& row : rows) q.push_back(row.q(c));
            auto& pair = columns[c];
            pair.update(precharge.at(c), drive.at(c), bl.at(c), bar.at(c), wl, q);
            for (std::size_t r = 0; r < rows.size(); ++r)
                rows[r].cells[c].update(wl.at(r), pair.cell_drive, pair.cell_bl, pair.cell_bar);
        }
    }
    // BL_c / BL_BAR_c / VALID_c are columns[c].bl / bar / valid.
};
```

#### Unknown inputs

The example composes the cell and bitline references, including unknown startup, precharge, reads, writes, and binary control conflicts. It assumes controls are settled and binary; X/Z controls follow the rules above. Both bank implementations are limited to 4096 cells because their interface exposes every row and column. A large compact **device** uses the word-address port instead.

### Implementation

#### Structural implementation

`ROW_r` owns C `SRAMCell` instances. `BITLINE_c` receives that column's cell values from every row and distributes write excitation back to those cells. Rows own cells; bitline components do not duplicate storage.

#### Behavioral implementation

A flat four-state cell array plus per-column bitline state replaces the rows and cells. The same digital bitline primitive rules are reused. This proves equivalence to the documented abstraction, not independently to analog SRAM physics. Bank replacement leaves the surrounding decoder, precharge, drivers, selector, and sense amplifiers visible.

### Examples

<!-- simulator-examples: sram-storage-bank SRAMStorageBankTest -->

The 4×8 bank is arranged as two rows of 16 cells. Every cell is written in a row-dependent alternating pattern, then both rows are read through explicit precharge/release/select sequences.

#### Embedded simulation

The standalone `SRAMStorageBankTest` opens this component as `SRAM_ROOT`. Use the explorer to compare structural and behavioral fidelity.

<div class="simulator-embed">
  <iframe src="../../../simulator/?scenario=sram-storage-bank" title="SRAMStorageBank&lt;W&gt; recorded simulation" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../simulator/?scenario=sram-storage-bank" class="md-button">Open SRAMStorageBank&lt;W&gt; in the simulator</a>

### Source and tests

- [Public interface: `SRAM.hpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/include/modules/memory/SRAM.hpp)
- [Primitive and non-template implementations: `SRAM.cpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/modules/memory/SRAM.cpp)
- [Width-dependent structural implementation: `SRAM.tpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/include/modules/memory/SRAM.tpp)
- [Behavioral implementations: `SRAMBehavioral.hpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/modules/memory/SRAMBehavioral.hpp)

| Test | Coverage |
| --- | --- |
| [`SRAMStorageBankTest`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/tests/SRAMTests.cpp) | The complete standalone sequence shown above |
| `SRAMClockedTest`, `SRAMClockedSubarrayTest` | The clocked macro, small and split into subarrays, against its compact form |
| `SRAMContractTest` | Structural/behavioral equivalence; device and bank widths 8/16/32, multiple geometries and byte masks, mixed bank selection, and invalid configuration rejection |

## 3.3.2.5. `SRAMRowDecoder`

Decodes the row portion of a word address.

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `WORD_ADDR` | Input | 32 | Complete word index |
| `ENABLE` | Input | 1 | Permit row activation |
| `WL_r` | Output | 1 each | One-hot row selection |

### Behavior

For K words per row, the row number is `WORD_ADDR / K`. The low `log2(K)` bits select a word within the row and are ignored here. A known address outside N words selects no row. This decoder is not a DRAM ACTIVATE command or an externally retained open-row protocol.

#### C++ example

```cpp
#include <cstddef>
#include <cstdint>
#include <vector>
std::vector<bool> sram_rows_ref(std::uint32_t address, bool enable,
                                std::size_t words, std::size_t words_per_row) {
    std::vector<bool> result(words / words_per_row, false);
    if (enable && address < words) result[address / words_per_row] = true;
    return result;
}
```

#### Unknown inputs

Unknown address bits propagate through the gates and may produce unknown wordlines; the bitline model then suppresses writes. A known low `ENABLE` deselects all rows even with an unknown address. The structural decoder accepts up to 512 words; complete structural devices apply the stricter W-dependent cell limit.

### Implementation

#### Structural implementation

An address splitter, complemented address bits, and balanced AND trees implement the row matches. The shared `decodeMatch` construction routine builds those trees after this wrapper extracts the row-address bits. No storage or direct host indexing is used; gate names and hierarchy remain unchanged. See [helper reuse boundaries](../building-blocks.md#reuse-boundaries).

### Examples

<!-- simulator-examples: sram-row-decoder SRAMRowDecoderTest -->

Six words arranged two per row demonstrate both word slots in each of three rows, an out-of-range address, and an address with high bits set.

#### Embedded simulation

The standalone `SRAMRowDecoderTest` opens this component as `SRAM_ROOT`.

<div class="simulator-embed">
  <iframe src="../../../simulator/?scenario=sram-row-decoder" title="SRAMRowDecoder recorded simulation" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../simulator/?scenario=sram-row-decoder" class="md-button">Open SRAMRowDecoder in the simulator</a>

### Source and tests

- [Public interface: `SRAM.hpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/include/modules/memory/SRAM.hpp)
- [Primitive and non-template implementations: `SRAM.cpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/modules/memory/SRAM.cpp)
- [Width-dependent structural implementation: `SRAM.tpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/include/modules/memory/SRAM.tpp)

| Test | Coverage |
| --- | --- |
| [`SRAMRowDecoderTest`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/tests/SRAMTests.cpp) | The complete standalone sequence shown above |

## 3.3.2.6. `SRAMPrecharge`

Distributes precharge control to the physical columns.

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `ENABLE` | Input | 1 | Request precharge |
| `PRECHARGE_c` | Output | 1 each | Buffered request for column c |

### Behavior

The construction parameter `columns = C` creates C buffered precharge controls. This block distributes control; the corresponding `SRAMBitlinePair` owns the symbolic charged state. Precharge must occur with wordlines and write drivers inactive.

#### C++ example

```cpp
#include <cstddef>
#include <vector>
std::vector<bool> sram_precharge_ref(bool enable, std::size_t columns) {
    return std::vector<bool>(columns, enable);
}
```

#### Unknown inputs

Each buffer propagates X/Z as unknown logic. Outputs settle one gate delay after the input. Precharge completion in the complete device includes the downstream bitline delay.

### Implementation

#### Structural implementation

One AND buffer with a constant-high input per column. These outputs actually drive the bitline primitives; the block is not a visual-only grouping.

### Examples

<!-- simulator-examples: sram-precharge SRAMPrechargeTest -->

The two-column example asserts and releases precharge and shows an unknown control propagating to both outputs.

#### Embedded simulation

The standalone `SRAMPrechargeTest` opens this component as `SRAM_ROOT`.

<div class="simulator-embed">
  <iframe src="../../../simulator/?scenario=sram-precharge" title="SRAMPrecharge recorded simulation" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../simulator/?scenario=sram-precharge" class="md-button">Open SRAMPrecharge in the simulator</a>

### Source and tests

- [Public interface: `SRAM.hpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/include/modules/memory/SRAM.hpp)
- [Primitive and non-template implementations: `SRAM.cpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/modules/memory/SRAM.cpp)
- [Width-dependent structural implementation: `SRAM.tpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/include/modules/memory/SRAM.tpp)

| Test | Coverage |
| --- | --- |
| [`SRAMPrechargeTest`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/tests/SRAMTests.cpp) | The complete standalone sequence shown above |

## 3.3.2.7. `SRAMWriteDriver`

Generates one bit's complementary write values and masked drive enable.

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `D` | Input | 1 | Data bit |
| `ENABLE` | Input | 1 | Write phase active |
| `MASK` | Input | 1 | Enable this bit's byte lane |
| `BL, BL_BAR` | Output | 1 each | Data and complementary data |
| `DRIVE` | Output | 1 | ENABLE AND MASK |

### Behavior

`BL = D`, `BL_BAR = !D`, and `DRIVE = ENABLE && MASK`. The values may remain present while `DRIVE = 0`; that does not write storage. There is one driver per external data bit. All eight drivers in a byte receive the same byte-mask input.

#### C++ example

```cpp
struct SRAMDriveReference { bool bl, bar, drive; };
SRAMDriveReference sram_drive_ref(bool data, bool enable, bool mask) {
    return {data, !data, enable && mask};
}
```

#### Unknown inputs

Unknown data produces an unknown pair. Unknown drive control is rejected by the bitline primitive and does not authorize a write. Complement generation includes its own gate delay; the access sequencer establishes write drive before activating the row.

### Implementation

#### Structural implementation

`ENABLED_BYTE` is an AND gate, `DATA` is a buffer, and `COMPLEMENT` is a NOT gate. No electrical drive strength is calculated.

### Examples

<!-- simulator-examples: sram-write-driver SRAMWriteDriverTest -->

All eight binary combinations of data, enable, and byte mask are included.

#### Embedded simulation

The standalone `SRAMWriteDriverTest` opens this component as `SRAM_ROOT`.

<div class="simulator-embed">
  <iframe src="../../../simulator/?scenario=sram-write-driver" title="SRAMWriteDriver recorded simulation" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../simulator/?scenario=sram-write-driver" class="md-button">Open SRAMWriteDriver in the simulator</a>

### Source and tests

- [Public interface: `SRAM.hpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/include/modules/memory/SRAM.hpp)
- [Primitive and non-template implementations: `SRAM.cpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/modules/memory/SRAM.cpp)
- [Width-dependent structural implementation: `SRAM.tpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/include/modules/memory/SRAM.tpp)

| Test | Coverage |
| --- | --- |
| [`SRAMWriteDriverTest`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/tests/SRAMTests.cpp) | The complete standalone sequence shown above |

## 3.3.2.8. `SRAMSenseAmplifier`

Digital primitive that accepts a valid symbolic read differential.

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `BL, BL_BAR` | Input | 1 each | Selected symbolic bitline pair |
| `VALID` | Input | 1 | The bitline model has a known read differential |
| `ENABLE` | Input | 1 | Sensing phase active |
| `Q` | Output | 1 | Resolved read bit, or X |

### Behavior

A known complementary pair produces `Q = BL` only when `VALID = ENABLE = 1`. Equal pairs, disabled sensing, and invalid input produce X. In particular, `(1,1)` from precharge must not be mistaken for a stored one.

#### C++ example

```cpp
#include <optional>
std::optional<bool> sram_sense_ref(bool bl, bool bar, bool valid, bool enable) {
    if (enable && valid && bl != bar) return bl;
    return std::nullopt;
}
```

#### Unknown inputs

X/Z or a noncomplementary pair cannot produce a known bit. This model does not calculate differential voltage, gain, noise, or sense-amplifier offset.

### Implementation

#### Structural implementation

A memory primitive. It provides the sensing boundary used by the structural device; there is no separate behavioral replacement.

### Examples

<!-- simulator-examples: sram-sense-amplifier SRAMSenseAmplifierTest -->

Disabled sensing, the equalized pair, both valid bit values, and an invalid pair are tested.

#### Embedded simulation

The standalone `SRAMSenseAmplifierTest` opens this component as `SRAM_ROOT`.

<div class="simulator-embed">
  <iframe src="../../../simulator/?scenario=sram-sense-amplifier" title="SRAMSenseAmplifier recorded simulation" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../simulator/?scenario=sram-sense-amplifier" class="md-button">Open SRAMSenseAmplifier in the simulator</a>

### Source and tests

- [Public interface: `SRAM.hpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/include/modules/memory/SRAM.hpp)
- [Primitive and non-template implementations: `SRAM.cpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/modules/memory/SRAM.cpp)
- [Width-dependent structural implementation: `SRAM.tpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/include/modules/memory/SRAM.tpp)

| Test | Coverage |
| --- | --- |
| [`SRAMSenseAmplifierTest`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/tests/SRAMTests.cpp) | The complete standalone sequence shown above |

## 3.3.2.9. `SRAMColumnSelector<W>`

Routes one word's read pairs and write enables among K column groups.

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `WORD_ADDR` | Input | 32 | Low address bits select the word slot |
| `WRITE_BL_b, WRITE_BL_BAR_b, WRITE_DRIVE_b` | Input | 1 each | W write-driver lanes |
| `BL_c, BL_BAR_c, VALID_c` | Input | 1 each | Read state from all C = W*K columns |
| `DRIVE_c, DRIVE_BL_c, DRIVE_BL_BAR_c` | Output | 1 each | Write route to each column |
| `READ_BL_b, READ_BL_BAR_b, READ_VALID_b` | Output | 1 each | W selected read pairs |

### Behavior

The slot is `WORD_ADDR % K`. For lane b, the selected column is `slot*W + b`; this implementation uses contiguous word groups. The selector routes W read pairs and enables only that slot's write drivers. A real layout may interleave columns differently; that placement detail is not modeled here.

#### C++ example

```cpp
#include <cstddef>
#include <cstdint>
#include <vector>
struct SRAMColumnReference {
    std::vector<std::optional<bool>> read_bl, read_bar, drive_bl, drive_bar;
    std::vector<bool> read_valid, drive;
};
SRAMColumnReference sram_select_ref(std::uint32_t address, std::size_t width,
    std::size_t slots, const std::vector<SRAMBitlineReference>& columns,
    const std::vector<bool>& write_bl, const std::vector<bool>& write_bar,
    const std::vector<bool>& write_drive) {
    SRAMColumnReference out;
    const auto slot = address % slots;
    for (std::size_t c = 0; c < width * slots; ++c) {
        auto b = c % width;
        out.drive_bl.push_back(write_bl.at(b)); // Data broadcast, drive decoded.
        out.drive_bar.push_back(write_bar.at(b));
        out.drive.push_back(c / width == slot && write_drive.at(b));
    }
    for (std::size_t b = 0; b < width; ++b) {
        const auto& col = columns.at(slot * width + b);
        out.read_bl.push_back(col.bl); out.read_bar.push_back(col.bar);
        out.read_valid.push_back(col.valid);
    }
    return out;
}
```

#### Unknown inputs

Unknown slot bits follow the gate/multiplexer rules. Full-address range checking belongs to the row decoder, not this selector. `K = 1` uses buffered pass-through paths rather than a multiplexer tree.

### Implementation

#### Structural implementation

A low-bit slot decoder gates write excitation. Separate mux trees select `BL`, `BL_BAR`, and `VALID` for each read lane. The shared `selectMuxTree` construction routine creates those existing trees; it does not replace them with host indexing or a post-sense word mux. Buffers carry write values to the column groups.

### Examples

<!-- simulator-examples: sram-column-selector SRAMColumnSelectorTest -->

Two word slots contain opposite bit patterns. The example verifies every selected read lane, every write-enable route, and the complementary write values.

#### Embedded simulation

The standalone `SRAMColumnSelectorTest` opens this component as `SRAM_ROOT`.

<div class="simulator-embed">
  <iframe src="../../../simulator/?scenario=sram-column-selector" title="SRAMColumnSelector&lt;W&gt; recorded simulation" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../simulator/?scenario=sram-column-selector" class="md-button">Open SRAMColumnSelector&lt;W&gt; in the simulator</a>

### Source and tests

- [Public interface: `SRAM.hpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/include/modules/memory/SRAM.hpp)
- [Primitive and non-template implementations: `SRAM.cpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/modules/memory/SRAM.cpp)
- [Width-dependent structural implementation: `SRAM.tpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/include/modules/memory/SRAM.tpp)

| Test | Coverage |
| --- | --- |
| [`SRAMColumnSelectorTest`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/tests/SRAMTests.cpp) | The complete standalone sequence shown above |

## 3.3.2.10. `SRAMAccessControl<W>`

Sequences an asynchronous access with gates and delay lines, and reports
when the data is valid.

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `WORD_ADDR` | Input | 32 | Requested word index |
| `WRITE_DATA` | Input | W | Write data |
| `WRITE_MASK` | Input | W/8 | Byte write enables |
| `CE_N`, `WE_N`, `OE_N` | Input | 1 each | Active-low chip, write and output enables |
| `ADDRESS` | Output | 32 | Held word address |
| `DATA` | Output | W | Held write data |
| `MASK` | Output | W/8 | Held byte mask |
| `PRECHARGE`, `ROW_ENABLE`, `WRITE_ENABLE`, `SENSE_ENABLE`, `OUTPUT_ENABLE` | Output | 1 each | Stage controls for the bank and periphery |
| `READY` | Output | 1 | The access has finished: read data is valid, or the write is done |

### Behavior

The stages of the [access timing](#access-timing) table, as outputs:

| Output | High while |
| --- | --- |
| `PRECHARGE` | Settled and selected, until the precharge interval has passed |
| `WRITE_ENABLE` | After precharge, for a write (`WE_N` low) |
| `ROW_ENABLE` | After the decoder replica |
| `SENSE_ENABLE` | After the bitline interval, for a read |
| `OUTPUT_ENABLE` | Sensing, for a read, with `OE_N` low |
| `READY` | After sensing and the output path |

A change of the address, `CE_N` or `WE_N`, or of the data or mask during a
write, clears every stage at once; the sequence restarts when the inputs are
steady again. The held buses keep the old values until the new inputs have
settled, so the array never sees a change in progress. Data and mask changes
during a read are ignored. `CE_N` high deselects: every stage stays low.

#### C++ example

The stage outputs as a function of how long the inputs have been steady,
for stage durations taken from the profile (gate delays inside each stage
are left out).

```cpp
#include <cstddef>
struct SRAMControlReference {
    // Settle, precharge, decode replica, bitline and sense, output path.
    std::size_t settle, precharge, decode, bitline, output;
    struct Outputs {
        bool precharge = false, write_enable = false, row_enable = false;
        bool sense_enable = false, output_enable = false, ready = false;
    };
    // `steady`: ticks since the address, CE_N, WE_N (and, during a write,
    // the data or mask) last changed.
    Outputs at(std::size_t steady, bool ce_n, bool we_n, bool oe_n) const {
        if (ce_n) return {};
        const std::size_t s1 = settle + precharge, s2 = s1 + decode,
                          s3 = s2 + bitline, s4 = s3 + output;
        const bool reading = we_n;
        return {steady >= settle && steady < s1, steady >= s1 && !reading,
                steady >= s2, steady >= s3 && reading,
                steady >= s3 && reading && !oe_n, steady >= s4};
    }
};
```

#### Unknown inputs

An unknown address, `CE_N` or `WE_N` makes the stages, and so the controls
and `READY`, unknown. Unknown `OE_N` makes `OUTPUT_ENABLE` unknown.

### Implementation

#### Structural implementation

A [`MemoryTimer`](../building-blocks.md#memorytimer) with one input per
address, data and mask bit plus `CE_N` and `WE_N`. Data and mask restart the
access only while the held `WE_N` is low (the timer's condition input). Its
stages are: the precharge delay line; a replica of the row decoder (the
longer of the decoder and the precharge release, plus one gate); the bitline
delay line with a replica of the sense path; and the output path. A few
gates turn the stages into the six controls in the table above. The settle
copies of the inputs drive `ADDRESS`, `DATA` and `MASK`.

### Examples

<!-- simulator-examples: sram-access-control SRAMAccessControlTest -->

The standalone checkpoints show deselection, a completed read, an `OE_N`-only disable, a completed write, and deselection. The stages in between are visible on the timeline. `SRAMTimingTest` also checks interrupted accesses.

#### Embedded simulation

The standalone `SRAMAccessControlTest` opens this component as `SRAM_ROOT`. Expand `ACCESS_TIMER` to follow the stages.

<div class="simulator-embed">
  <iframe src="../../../simulator/?scenario=sram-access-control" title="SRAMAccessControl&lt;W&gt; recorded simulation" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../simulator/?scenario=sram-access-control" class="md-button">Open SRAMAccessControl&lt;W&gt; in the simulator</a>

### Source and tests

- [Public interface: `SRAM.hpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/include/modules/memory/SRAM.hpp)
- [Primitive and non-template implementations: `SRAM.cpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/modules/memory/SRAM.cpp)
- [Width-dependent structural implementation: `SRAM.tpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/include/modules/memory/SRAM.tpp)

| Test | Coverage |
| --- | --- |
| [`SRAMAccessControlTest`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/tests/SRAMTests.cpp) | The complete standalone sequence shown above |
| `SRAMTimingTest` | Interrupted writes and reads, superseded addresses, and a quiet event queue afterwards |

## 3.3.2.11. `SRAMOutputBuffer<W>`

Digital output-enable primitive with high-impedance disabled output.

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `DATA` | Input | W | Read word from sense amplifiers |
| `ENABLE` | Input | 1 | Present read data |
| `OUT` | Output | W | Data when enabled, Z when disabled |

### Behavior

A high enable forwards the input word. A low enable produces Z on every output bit. This models an output driver being disabled; it does not make the simulator wire bidirectional or resolve other drivers.

#### C++ example

```cpp
#include <cstdint>
#include <optional>
std::optional<std::uint32_t> sram_output_ref(std::uint32_t data, bool enable) {
    if (enable) return data;
    return std::nullopt; // Disabled output, not a stored zero.
}
```

#### Unknown inputs

An unknown/Z enable produces X rather than choosing between driving and disconnecting. Enabled data preserves its four-state values.

### Implementation

#### Structural implementation

A compatibility wrapper inheriting the common [MemoryOutputBuffer implementation](../building-blocks.md#325-memoryoutputbufferw). It retains the SRAM catalog ID, display name, pins and delay without adding a child component. It is a memory primitive because ordinary Boolean gates cannot produce the required Z output. There is no separate behavioral replacement.

### Examples

<!-- simulator-examples: sram-output-buffer SRAMOutputBufferTest -->

The example checks disabled Z, enabled data, unknown enable, and returning to disabled Z.

#### Embedded simulation

The standalone `SRAMOutputBufferTest` opens this component as `SRAM_ROOT`.

<div class="simulator-embed">
  <iframe src="../../../simulator/?scenario=sram-output-buffer" title="SRAMOutputBuffer&lt;W&gt; recorded simulation" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../simulator/?scenario=sram-output-buffer" class="md-button">Open SRAMOutputBuffer&lt;W&gt; in the simulator</a>

### Source and tests

- [Public interface: `SRAM.hpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/include/modules/memory/SRAM.hpp)
- [Primitive and non-template implementations: `SRAM.cpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/modules/memory/SRAM.cpp)
- [Width-dependent structural implementation: `SRAM.tpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/include/modules/memory/SRAM.tpp)

| Test | Coverage |
| --- | --- |
| [`SRAMOutputBufferTest`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/tests/SRAMTests.cpp) | The complete standalone sequence shown above |

## 3.3.2.12. `SRAM<W>`

Complete word-addressed SRAM device: an asynchronous chip, or with
`clocked=1` an on-chip clocked macro.

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `WORD_ADDR` | Input | 32 | Word index, not byte address |
| `WRITE_DATA` | Input | W | Data for the selected word |
| `WRITE_MASK` | Input | W/8 | Bit b enables byte b; bit 0 is the least-significant byte |
| `CE_N` | Input | 1 | Low selects the device |
| `WE_N` | Input | 1 | Low requests writing; high requests reading |
| `OE_N` | Input | 1 | Low enables a completed read output |
| `CLK` | Input | 1 | Clocked macro only: inputs are registered at its rising edge |
| `READ_DATA` | Output | W | Read result, or Z while disabled, writing or accessing |
| `READY` | Output | 1 | The access has finished |

### Behavior

`CE_N = 1` disables access without clearing memory. With `CE_N = 0`, `WE_N = 0`
writes the enabled bytes once the selected row opens; `WE_N = 1` reads. `READY`
rises when the read data is valid or the write is done, and falls when a new
access starts. `OE_N` controls only the read output. A zero write mask changes
no bits. Reads are nondestructive. There is no reset or refresh, and no
fault pin: a known out-of-range read returns X and an out-of-range write
changes no cells.

The asynchronous chip starts an access whenever its inputs change and have
settled ([access timing](#access-timing)). The clocked macro registers its
inputs at a rising `CLK` edge with `CE_N` low and holds the result until the
next access. The SRAM [memory controller](../controllers.md) connects either
form to the transaction protocol.

#### C++ example

One access whose inputs have been steady for `steady` ticks, with the stage
durations of [`SRAMControlReference`](#33210-sramaccesscontrolw). A write lands
once the row is open with the write enabled, so an access interrupted earlier
writes nothing.

```cpp
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>
template<std::size_t W> struct SRAMReference {
    static_assert(W == 8 || W == 16 || W == 32);
    using Word = std::vector<std::optional<bool>>;
    struct Inputs {
        std::uint32_t address = 0, data = 0;
        unsigned mask = 0;
        bool ce_n = true, we_n = true, oe_n = true;
    };
    std::vector<Word> words;
    Word read_data = Word(W);
    bool high_z = true, ready = false;
    explicit SRAMReference(std::size_t count) : words(count, Word(W)) {}
    void update(const SRAMControlReference& control, std::size_t steady, const Inputs& in) {
        const auto stage = control.at(steady, in.ce_n, in.we_n, in.oe_n);
        const bool valid = in.address < words.size();
        if (stage.write_enable && stage.row_enable && valid)
            for (unsigned b = 0; b < W; ++b)
                if (in.mask & (1u << (b / 8)))
                    words[in.address][b] = bool((in.data >> b) & 1u);
        ready = stage.ready;
        high_z = !stage.output_enable;
        read_data = !high_z && valid ? words[in.address] : Word(W);
    }
};
```

#### Unknown inputs

Storage starts unknown. Unknown write data makes only the enabled bits
unknown; an unknown byte enable suppresses writes to that lane. An unknown
address, `CE_N` or `WE_N` makes the stages unknown, so `READY` and the output
are unknown. `high_z` in the example distinguishes a released output from an
enabled unknown word.

### Implementation

#### Structural implementation

<!-- diagram: sram-device -->

Combines `ACCESS_CONTROL`, `ROW_DECODE`, `PRECHARGE`, `STORAGE`, `COLUMN_SELECT`, W `WRITE_DRIVER_b` and `SENSE_b` instances, bus wiring, and `OUTPUT`. Each `SRAMWriteDriver` keeps its own write-enable and mask gate and complementary data pair, so the drive and bitline paths keep their relative timing. Precharge control and repeated column-select complements are shared, while each bitline pair and sense amplifier remains visible. The default 4×8 example has two rows, 16 bitline pairs and 32 cells.

An array of more than 32 rows is built from 32-row subarrays, each a complete `SRAM<W>` with its own access control. The asynchronous form sends the address over a global wire and lets the high address bits select one subarray; the clocked form lets every subarray take each access in parallel. Each subarray drives a shared return bus for `READ_DATA` and `READY` (`READY` is pulled low while no subarray drives it). The wire and bus delays grow with the side of the subarray grid. Gate-level devices are limited to 4,096 cells in all.

#### Behavioral implementation

The compact device keeps the native pins and unknown startup with a packed, optionally paged `MemoryStorage` instead of rows and cells, and publishes `READY` and the data at the delays proven on the gate-level device. Capacity is bounded by 32-bit byte addressing and the allocation and history budgets. An image can be loaded only before simulation starts. Selecting behavioral fidelity at `STORAGE` instead keeps the device's visible peripheral circuits and its small-bank limit.

**Timing.** An address change restarts the device's whole access chain (settling,
precharge, row decode, bitline development, sensing and the output), so the
address is the slowest input,
and a deeper array is slower because its bitlines are longer. `OE_N` only
enables the output buffer: a requester that keeps the address and toggles
`OE_N` gets the data back after one primitive delay.

<!-- timing-rows
Address to data, 4 words | memory.sram.device.8.behavioral | WORD_ADDR → READ_DATA
Address to data, 64 words | memory.sram.device.8.behavioral@depth=64 | WORD_ADDR → READ_DATA
Address to data, 128 words | memory.sram.device.8.behavioral@depth=128 | WORD_ADDR → READ_DATA
Output enable to data | memory.sram.device.8.behavioral | OE_N → READ_DATA
-->

### Examples

<!-- simulator-examples: sram SRAMTest -->

Every recorded row is included: unknown startup, all four addresses, both 8-bit masks, `OE_N` disable, ignored read-mode write inputs, invalid accesses, retention readback, unknown writes and unknown addressing. Wider 16/32-bit masks, other geometries and the clocked macro are covered by the contract and clocked tests.

#### Embedded simulation

The standalone `SRAMTest` opens this component as `SRAM_ROOT`. Use the explorer to compare structural and behavioral fidelity.

<div class="simulator-embed">
  <iframe src="../../../simulator/?scenario=sram" title="SRAM&lt;W&gt; recorded simulation" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../simulator/?scenario=sram" class="md-button">Open SRAM&lt;W&gt; in the simulator</a>

### Source and tests

- [Public interface: `SRAM.hpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/include/modules/memory/SRAM.hpp)
- [Primitive and non-template implementations: `SRAM.cpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/modules/memory/SRAM.cpp)
- [Width-dependent structural implementation: `SRAM.tpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/include/modules/memory/SRAM.tpp)
- [Behavioral implementations: `SRAMBehavioral.hpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/modules/memory/SRAMBehavioral.hpp)

| Test | Coverage |
| --- | --- |
| [`SRAMTest`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/tests/SRAMTests.cpp) | The complete standalone sequence shown above |
| `SRAMClockedTest`, `SRAMClockedSubarrayTest` | The clocked macro, small and split into subarrays, against its compact form |
| `SRAMContractTest` | Structural/behavioral equivalence; device and bank widths 8/16/32, multiple geometries and byte masks, mixed bank selection, and invalid configuration rejection |
| `SRAMTimingTest` | Interrupted writes and reads, superseded addresses, and a quiet event queue afterwards |
| [`DocumentationExamplesTest`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/tests/DocumentationExamplesTest.py) | Compile the exact page snippets; exercise cell/bitline conflicts, bank reads and writes, column routing, masks and interrupted accesses; replay the device scenario's binary-input prefix against recorded outputs |
