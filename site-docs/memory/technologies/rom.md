# § 3.3.4. ROM

Immutable read-only storage following the [common cell/row/bank/device architecture](../architecture.md). This is a digital mask-ROM-style model, not a writable EEPROM.

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| `ROMCell` | One fixed construction-time bit | Yes | Yes |
| `ROMRow<C>` | C fixed bits in column order | Yes | Yes |
| `ROMStorageBank<W>` | Rows plus one selected-row sensor per column | Yes | Yes |
| `ROMAddressControl` | Holds the address, splits it into row and slot, times the read and raises `READY` | Yes | No |
| `ROM<W>` | Complete asynchronous read-only device | Yes | Yes |

## Configuration

Width W, capacity N, and words-per-row K follow [3.1](../architecture.md#width-capacity-and-geometry). `image_hex` contains consecutive bytes as pairs of hexadecimal digits, starting at byte address zero. No spaces or prefixes are accepted. The remaining capacity is initialized to zero; an odd-length, non-hex, or oversized image is rejected.

For example, `image_hex="a53c81ff"` gives four 8-bit words `A5, 3C, 81, FF`, two 16-bit words `3CA5, FF81`, or one 32-bit word `FF813CA5`. This is **little-endian byte packing**. Device pins use word addresses, not byte addresses. Contents cannot be altered by reset, write commands, or a debug load after construction.

The C++ examples describe settled binary pin behavior for validated construction parameters, including invalid addresses and disabled outputs. Compile them in page order.

The cell's catalog parameter is `value`; a standalone row uses `contents`. Banks and devices use `ROMDeviceConfig` (`depth_words`, `words_per_row`, `image_hex`, `record_history`). The address controller accepts only geometry parameters; it has no image or history. Register names are `memory.rom.cell`, `row`, `address-control`, `bank.W`, and `device.W` under the `memory.rom.` prefix. Only the public bank/device wrappers are C++ width templates; row dimensions are runtime values.

## 3.3.4.1. `ROMCell`

One immutable construction-time bit.

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `Q` | Output | 1 | Fixed configured bit; no input pins |

### Behavior

The `value` parameter is 0 or 1 (default 0). It cannot be changed by an input, clock, reset, or debug load.

#### C++ example

```cpp
struct ROMCellReference {
    const bool value;
    bool read() const { return value; }
};
```

#### Unknown inputs

The output starts unresolved until the initial simulation event propagates, then is always the configured binary value. Both implementations schedule their first output even though there are no inputs.

### Implementation

#### Structural implementation

A `ConstantValue<1>` child named `MASK_BIT` drives Q. This represents a fixed connection, not a flip-flop or a transistor-level mask-ROM cell.

#### Behavioral implementation

A terminal constant-output component publishes the configured value without the child.

The behavioral cell's visualizer shows Q as the **programmed bit**, using its recorded value. Selecting the structural implementation shows the fixed connection instead.

### Timing

No inputs, so no delays: the outputs take their configured values once, when the simulation starts.

### Examples

<!-- simulator-examples: rom-cell ROMCellTest -->

The example constructs `value=1` and checks Q. Zero-valued cells also occur in the row and bank examples.

#### Embedded simulation

The standalone `ROMCellTest` opens this component as `MEMORY_ROOT`. Use the explorer to compare structural and behavioral implementations.

<div class="simulator-embed">
  <iframe src="../../../simulator/?scenario=rom-cell" title="ROMCell recorded simulation" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../simulator/?scenario=rom-cell" class="md-button">Open ROMCell in the simulator</a>

### Source and tests

- [Public interfaces: `ROM.hpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/include/modules/memory/ROM.hpp)
- [Implementation and registration: `ROM.cpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/modules/memory/ROM.cpp)

| Test | Coverage |
| --- | --- |
| [`ROMCellTest`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/tests/MemoryDeviceTests.cpp) | The complete standalone sequence shown above |
| `MemoryDeviceContractTest` | Structural/behavioral agreement at settled checkpoints, including mixed hierarchy selection |

## 3.3.4.2. `ROMRow<C>`

C fixed cells ordered by physical column.

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `Q_c` | Output | 1 each | Fixed value of column c, 0 ≤ c < C; no inputs |

### Behavior

The `contents` parameter contains 1–512 characters, each `0` or `1`; default `"0000"`. Characters are in increasing column order: `"1010"` means Q_0=1, Q_1=0, Q_2=1, Q_3=0, not an MSB-first integer. Selection belongs to the bank, not the row.

#### C++ example

```cpp
#include <string>
#include <vector>
std::vector<bool> rom_row(const std::string& contents) {
    std::vector<bool> q;
    for (char bit : contents) q.push_back(bit == '1');
    return q; // The factory has already validated a nonempty binary string.
}
```

#### Unknown inputs

Invalid characters, an empty row, or more than 512 columns are rejected at construction. Outputs settle from their initial events; valid ROM contents do not acquire X during reads.

### Implementation

#### Structural implementation

Contains `CELL_0` through `CELL_(C−1)`, each a registered `ROMCell`. Cells are arranged left to right. A structural row may select behavioral cells.

#### Behavioral implementation

Retains the construction string and publishes each column directly, with the same startup and ordering contract.

### Timing

No inputs, so no delays: the outputs take their configured values once, when the simulation starts.

### Examples

<!-- simulator-examples: rom-row ROMRowTest -->

The four-column example uses `contents="1010"` and checks every output in the initial settled checkpoint.

#### Embedded simulation

The standalone `ROMRowTest` opens this component as `MEMORY_ROOT`. Use the explorer to compare structural and behavioral implementations.

<div class="simulator-embed">
  <iframe src="../../../simulator/?scenario=rom-row" title="ROMRow<C> recorded simulation" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../simulator/?scenario=rom-row" class="md-button">Open ROMRow&lt;C&gt; in the simulator</a>

### Source and tests

- [Public interfaces: `ROM.hpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/include/modules/memory/ROM.hpp)
- [Implementation and registration: `ROM.cpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/modules/memory/ROM.cpp)

| Test | Coverage |
| --- | --- |
| [`ROMRowTest`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/tests/MemoryDeviceTests.cpp) | The complete standalone sequence shown above |
| `MemoryDeviceContractTest` | Structural/behavioral agreement at settled checkpoints, including mixed hierarchy selection |

## 3.3.4.3. `ROMStorageBank<W>`

Immutable rows with shared selected-row sensing.

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `WL_r` | Input | 1 each | Row selection, 0 ≤ r < N/K |
| `COL_c` | Output | 1 each | Sensed bit, 0 ≤ c < W×K |

### Behavior

Exactly one binary wordline must be high. Each output then gives the corresponding column of that row. All-low, multi-row, or unknown selection returns X. The bank has no address decoding, output-enable, or write interface.

#### C++ example

Binary wordlines for any validated W/N/K geometry. The image is in byte-address order, with a zero-filled tail. Output columns use LSB-first order; empty optionals denote invalid row selection.

```cpp
#include <vector>
#include <cstdint>
#include <optional>
std::vector<std::optional<bool>> rom_bank(unsigned width, unsigned slots,
    const std::vector<std::uint8_t>& image, const std::vector<bool>& wl) {
    std::vector<std::optional<bool>> columns(width * slots);
    unsigned count = 0;
    std::size_t row = 0;
    for (std::size_t r = 0; r < wl.size(); ++r) if (wl[r]) { ++count; row = r; }
    if (count != 1) return columns;
    for (std::size_t c = 0; c < columns.size(); ++c) {
        const auto byte = row * columns.size() / 8 + c / 8;
        columns[c] = byte < image.size() && ((image[byte] >> (c % 8)) & 1u);
    }
    return columns;
}
```

#### Unknown inputs

An unknown wordline invalidates the entire selection; it is not treated as zero. Unselected cell values do not affect a valid selection. Scalar row/column interfaces limit standalone banks of either fidelity to 4,096 cells. The structural sensing path adds propagation delay.

### Implementation

#### Structural implementation

`ROW_r` contains W×K fixed cells. Each `COLUMN_c` is a `StaticReadColumn` connected to column c of every row. Rows are stacked; sensing blocks align under their cells.

#### Behavioral implementation

Reads the fixed image bits directly after validating one-hot selection. It keeps the same expanded pins, geometry limits, and image interpretation.

### Timing

<!-- timing-table: memory.rom.bank.8.behavioral -->

### Examples

<!-- simulator-examples: rom-storage-bank ROMStorageBankTest -->

The image is `A5 3C 81 FF`. The test first selects no row, then selects row zero and row one, checking all 16 sensed columns for each valid row.

#### Embedded simulation

The standalone `ROMStorageBankTest` opens this component as `MEMORY_ROOT`. Use the explorer to compare structural and behavioral implementations.

<div class="simulator-embed">
  <iframe src="../../../simulator/?scenario=rom-storage-bank" title="ROMStorageBank<W> recorded simulation" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../simulator/?scenario=rom-storage-bank" class="md-button">Open ROMStorageBank&lt;W&gt; in the simulator</a>

### Source and tests

- [Public interfaces: `ROM.hpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/include/modules/memory/ROM.hpp)
- [Implementation and registration: `ROM.cpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/modules/memory/ROM.cpp)

| Test | Coverage |
| --- | --- |
| [`ROMStorageBankTest`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/tests/MemoryDeviceTests.cpp) | The complete standalone sequence shown above |
| `MemoryDeviceContractTest` | Structural/behavioral agreement at settled checkpoints, including mixed hierarchy selection |

## 3.3.4.4. `ROMAddressControl`

Holds the address while a read is in progress, splits it into a row and a
word slot, qualifies the row and output, and reports when the data is
valid.

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `WORD_ADDR` | Input | 32 | Word address A |
| `CE_N` | Input | 1 | Active-low chip enable |
| `OE_N` | Input | 1 | Active-low output enable |
| `ROW_ADDR` | Output | 32 | Row field of the held address, A / K |
| `COLUMN_ADDR` | Output | 32 | Slot field of the held address, A % K |
| `ROW_ENABLE` | Output | 1 | `CE_N` low and the held address below N |
| `OUTPUT_ENABLE` | Output | 1 | `CE_N` and `OE_N` both low |
| `READY` | Output | 1 | The read has settled since the address or `CE_N` last changed |

### Behavior

The address and `CE_N` pass through the settle copies of a
[`MemoryTimer`](../building-blocks.md#memorytimer), so the decoder sees a new
address only once it has been steady. K is a power of two, so the row and
slot are bit fields of the held address; an address at or above N enables no
row, and the read columns then give X. `OE_N` only controls the output: a
valid row can stay selected with its output disabled. `READY` rises after a
replica of the decoder, the read-column interval and the output path, and
falls as soon as the address or `CE_N` changes.

#### C++ example

Settled binary values; `ready` is the value once the read has had time to
complete.

```cpp
#include <cstdint>
struct ROMAddressReference {
    std::uint32_t row, slot;
    bool row_enable, output_enable, ready;
};
ROMAddressReference rom_address(std::uint32_t a, unsigned n, unsigned k,
                                bool ce_n, bool oe_n) {
    // k is a validated power of two: the slot is the low address bits.
    return {a / k, a % k, !ce_n && a < n, !ce_n && !oe_n, !ce_n};
}
```

#### Unknown inputs

Unknown address bits give unknown row or slot bits and an unknown
`ROW_ENABLE` (unless `CE_N` is high, which forces it low). `OUTPUT_ENABLE` is
low if either enable is high and unknown if one is unknown and the other low.
An unknown address or `CE_N` makes `READY` unknown.

### Implementation

#### Structural implementation

A `MemoryTimer` with 33 inputs (the address bits and `CE_N`) and three stages:
a replica chain as deep as the row decoder, the ROM read-column interval, and
the column multiplexer and output path. Wiring splits the held address into
the row and slot fields; gates compare it with the depth and qualify the row
and output enables. It stores no image and has no clock. The row decoder,
read columns and output buffer are separate components of the complete
device.

### Timing

Built from gates and delay lines, so it has no delay table of its own; its
intervals are the ROM intervals below. The device table includes it.

### Examples

<!-- simulator-examples: rom-address-control ROMAddressControlTest -->

The sequence checks chip deselection, the last word (A=3 → row 1, slot 1), and the first invalid address A=4 (no row, output still enabled). Further cases exercise `OE_N` on its own and unknown controls.

#### Embedded simulation

The standalone `ROMAddressControlTest` opens this component as `MEMORY_ROOT`. Expand `ACCESS_TIMER` to follow the read.

<div class="simulator-embed">
  <iframe src="../../../simulator/?scenario=rom-address-control" title="ROMAddressControl recorded simulation" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../simulator/?scenario=rom-address-control" class="md-button">Open ROMAddressControl in the simulator</a>

### Source and tests

- [Public interfaces: `ROM.hpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/include/modules/memory/ROM.hpp)
- [Implementation and registration: `ROM.cpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/modules/memory/ROM.cpp)

| Test | Coverage |
| --- | --- |
| [`ROMAddressControlTest`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/tests/MemoryDeviceTests.cpp) | The complete standalone sequence shown above |
| `MemoryDeviceContractTest` | Integration in the new structural devices and comparison with their compact implementations |

## 3.3.4.5. `ROM<W>`

Complete asynchronous, word-addressed read-only device.

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `WORD_ADDR` | Input | 32 | Word index, 0…N−1 |
| `CE_N` | Input | 1 | Active-low chip enable |
| `OE_N` | Input | 1 | Active-low output enable |
| `READ_DATA` | Output | W | Selected immutable word, X, or Z |
| `READY` | Output | 1 | The read has finished |

### Behavior

No clock or command is required. With both enables low and a valid word address, the device publishes the corresponding little-endian word and raises `READY` once the read has settled; any change of the address or `CE_N` lowers `READY` until the new read is done. Either enable high releases the output. There are no write pins or reset operation.

#### C++ example

Binary read path for W=8/16/32, including little-endian byte assembly and the zero-filled image tail. The explicit result distinguishes valid data, high impedance, and an invalid address. Capacity is in words; `image` is in bytes and has already passed construction-time length/hex validation.

```cpp
#include <cstdint>
#include <vector>
struct ROMReadReference {
    enum class State { Data, HighZ, Unknown } state;
    std::uint32_t data = 0;
};
template<unsigned W = 8>
ROMReadReference rom_read(const std::vector<std::uint8_t>& image, std::size_t words,
                          std::uint32_t address, bool ce_n, bool oe_n) {
    static_assert(W == 8 || W == 16 || W == 32);
    using S = ROMReadReference::State;
    if (ce_n || oe_n) return {S::HighZ};
    if (address >= words) return {S::Unknown};
    std::uint32_t data = 0;
    for (unsigned byte = 0; byte < W / 8; ++byte) {
        auto a = std::size_t(address) * (W / 8) + byte;
        if (a < image.size()) data |= std::uint32_t(image[a]) << (8 * byte);
    }
    return {S::Data, data};
}
```

#### Unknown inputs

Enabled reads with an unknown address or control, or an out-of-range address, return X. A known disabled enable still returns Z. Image validation occurs before building the device.

### Implementation

#### Structural implementation

`ACCESS_CONTROL` drives `ROW_DECODE` and output qualification. `STORAGE` contains fixed rows and their column sensors; `COLUMN_SELECT` chooses one W-bit slot; `OUTPUT` drives READ_DATA. There is no write distributor or erase block.

#### Behavioral implementation

Uses packed, zero-initialized `MemoryStorage` with the construction image prefix loaded once, and publishes the data and `READY` at the delays proven on the gate-level device. The memory inspector is read-only; `loadMemoryBytes` rejects changes. This implementation supports large capacities without expanding cells. `record_history=0` disables backing-store history, not wire history.

### Timing

<a id="timing-intervals"></a>The read's analog steps use these intervals
(the read column grows with the number of rows):

<!-- profile-table: memory-rom -->

The compact device's proven delays:

<!-- timing-table: memory.rom.device.8.behavioral -->

### Examples

<!-- simulator-examples: rom ROMTest -->

The example reads all four bytes of `A5 3C 81 FF`, checks out-of-range and unknown addresses, and disables the output. The displayed values and times come directly from the recorded test.

#### Embedded simulation

The standalone `ROMTest` opens this component as `MEMORY_ROOT`. Use the explorer to compare structural and behavioral implementations.

<div class="simulator-embed">
  <iframe src="../../../simulator/?scenario=rom" title="ROM<W> recorded simulation" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../simulator/?scenario=rom" class="md-button">Open ROM&lt;W&gt; in the simulator</a>

### Source and tests

- [Public interfaces: `ROM.hpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/include/modules/memory/ROM.hpp)
- [Implementation and registration: `ROM.cpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/modules/memory/ROM.cpp)

| Test | Coverage |
| --- | --- |
| [`ROMTest`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/tests/MemoryDeviceTests.cpp) | The complete standalone sequence shown above |
| `MemoryDeviceContractTest` | W=8/16/32, K=1/2/4, image packing/zero tail, mixed fidelity, immutable debug interface, invalid configurations, and compact allocation |
| [`MemoryDeviceDocumentationTest`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/tests/MemoryDeviceDocumentationTest.py) | Compile the exact references; test row conflicts, address/output controls, 8/16/32-bit image reads, little-endian packing, zero-filled tails and invalid addresses |
