# § 3.2. Shared Building Blocks

The circuits several memory technologies share: addressing, word selection, write distribution, column sensing and output enable (3.2.1–3.2.5), and the timing and control parts that sequence accesses inside devices and controllers ([3.2.6](#326-timing-and-control-parts)). None of them stores memory words or buffers transactions.

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| `MatrixRowDecoder<R>` | Select one row from a full 32-bit row address | Yes | No |
| `MatrixColumnMux<W,K>` | Select one W-bit word from K words in a sensed row | Yes | No |
| `MatrixWriteDistributor<W,K>` | Route write data and byte enables to physical columns | Yes | No |
| `StaticReadColumn<R>` | Terminal digital primitive sensing one selected ROM/NOR row | Yes | No |
| `MemoryOutputBuffer<W>` | Terminal digital primitive publishing data or releasing output as `Z` | Yes | No |

The first three are built from gates. `StaticReadColumn` and `MemoryOutputBuffer` are memory primitives: they stand for electrical circuits (a sensed bitline, a tri-state driver) and evaluate their digital contract directly, with a delay from the timing profile. None of the five has a separate behavioral form; the devices built on them have one.

## Reuse boundaries

Share a circuit when its signal contract agrees; keep technology-specific sensing and access control separate.

| Circuit | Reuse in the implementation | Boundary that remains distinct |
| --- | --- | --- |
| Output enable | SRAM's `SRAMOutputBuffer<W>` is a compatibility wrapper around `MemoryOutputBuffer`; DRAM/ROM/NOR use the latter directly | The SRAM catalog ID and display name remain unchanged; the wrapper adds no child or delay |
| Address equality decoding | FF's `MemoryAddressDecoder`, `SRAMRowDecoder`, and `MatrixRowDecoder` share the `decodeMatch` gate-construction routine | FF supplies range/read-selection signals; SRAM extracts a row from a word address; the matrix decoder accepts a row address |
| Binary mux trees | FF's `MemoryReadSelector` and `SRAMColumnSelector` share the `selectMuxTree` construction routine | FF selects stored words and pads missing leaves with zero; SRAM selects symbolic bitline pairs and validity before sensing |
| Post-sense word selection | DRAM/ROM/NOR use `MatrixColumnMux`; DRAM/NOR also use `MatrixWriteDistributor` | These operate on ordinary digital bits, not SRAM's complementary bitline interface |
| Static column sensing | ROM and NOR share `StaticReadColumn` | SRAM has its own bitline/sense components; DRAM has a destructive-read/restoration sense column |

`decodeMatch` and `selectMuxTree` in [MemoryPeripheralWiring.hpp](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/include/modules/memory/MemoryPeripheralWiring.hpp) are **C++ construction utilities, not extra simulated components**. They build the existing gates in each caller's scope, preserving their names and layout hierarchy. Technology-specific controls, cells, and sensing are not merged. `MemoryHelperContractTest` checks their X/Z behavior, address interpretation, selection and boundary geometries; the device contract tests cover their integration.

## Configuration

In this page's helper names, R, W, and K describe dimensions; the five common C++ classes themselves take those dimensions as constructor arguments, not template arguments. R is row count, W is bits per word, and K is words per row. The SRAM compatibility wrapper remains a C++ width template.

Use catalog IDs `memory.shared.row-decoder`, `memory.shared.read-column`, `memory.shared.column-mux.W`, `memory.shared.write-distributor.W`, and `memory.shared.output-buffer.W`. Row decoder/read column accept runtime `rows`; mux/distributor accept runtime `words_per_row`. The output buffer has no configuration parameters beyond the width in its ID. Defaults are R=2 and K=2. Each helper currently has only a structural registration.

## 3.2.1. `MatrixRowDecoder<R>`

Select one row from a complete 32-bit row index.

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `ROW_ADDR` | Input | 32 | Row index, not word or byte address |
| `ENABLE` | Input | 1 | Allow selection |
| `WL_r` | Output | 1 each | Row enable, 0 ≤ r < R |

### Behavior

For binary inputs, WL_r = ENABLE AND (ROW_ADDR == r). An invalid address selects no row. All 32 address bits are checked, so high bits cannot alias a valid row. `rows=R` accepts 1–512.

#### C++ example

```cpp
#include <cstdint>
#include <vector>
std::vector<bool> matrix_decode(unsigned rows, std::uint32_t address, bool enable) {
    std::vector<bool> wl(rows, false);
    if (enable && address < rows) wl[address] = true;
    return wl;
}
```

#### Unknown inputs

Gate-level four-state propagation applies. ENABLE=0 forces every WL low even if the address is unknown; otherwise uncertain equality can produce X.

### Implementation

#### Structural implementation

`ADDRESS_BITS` splits the address. One shared high-bit validation term rejects addresses outside the decoder's low row-index bits; `decodeMatch` combines that term with each row's low-bit pattern and ENABLE. SRAM and FF use the same construction utility behind their distinct address interfaces. No address truncation or behavioral decoder substitutes for those gates.

### Timing

Built from gates only, so it has no delay table of its own: each output settles when the gates on its path do, under the [gate delays of the active profile](../timing.md#timing-profiles). Its delay is part of the proven tables of the devices built on it.

### Examples

<!-- simulator-examples: matrix-row-decoder MatrixRowDecoderTest -->

Three rows are tested with addresses 0, 1, 2, 3, and 0xFFFFFFFF, each enabled and disabled. All wordlines are checked.

#### Embedded simulation

The standalone `MatrixRowDecoderTest` opens this component as `MEMORY_ROOT`.

<div class="simulator-embed">
  <iframe src="../../simulator/?scenario=matrix-row-decoder" title="MatrixRowDecoder<R> recorded simulation" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../simulator/?scenario=matrix-row-decoder" class="md-button">Open MatrixRowDecoder&lt;R&gt; in the simulator</a>

### Source and tests

- [Public interfaces: `DigitalMemory.hpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/include/modules/memory/DigitalMemory.hpp)
- [Implementation and registration: `DigitalMemory.cpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/modules/memory/DigitalMemory.cpp)

| Test | Coverage |
| --- | --- |
| [`MatrixRowDecoderTest`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/tests/MemoryDeviceTests.cpp) | The complete standalone sequence shown above |
| `MemoryDeviceContractTest` | Integration in the new structural devices and comparison with their compact implementations |

## 3.2.2. `MatrixColumnMux<W,K>`

Select a W-bit word from K groups of already sensed columns.

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `COLUMN_ADDR` | Input | 32 | Word slot within a row |
| `COL_c` | Input | 1 each | Sensed bit, 0 ≤ c < W×K |
| `DATA` | Output | W | Selected word |

### Behavior

A valid binary slot selects columns `slot*W` through `slot*W+W−1`, in least-significant-bit order. An invalid binary slot produces zero because every selection term is disabled. Device control prevents this zero from masquerading as a successful out-of-range read. K=1 still checks the full slot address.

#### C++ example

```cpp
#include <cstddef>
#include <cstdint>
#include <vector>
std::uint32_t matrix_mux(unsigned width, std::uint32_t slot,
                         const std::vector<bool>& columns) {
    const std::size_t slots = columns.size() / width; // validated W and K
    if (slot >= slots) return 0;
    std::uint32_t word = 0;
    for (unsigned b = 0; b < width; ++b)
        if (columns.at(slot * width + b)) word |= std::uint32_t{1} << b;
    return word;
}
```

#### Unknown inputs

AND/OR gate rules determine partial X propagation from unknown slot or selected column bits. This is a logic multiplexer, not a tri-state output buffer. W=8/16/32 and K=1/2/4/8/16; scalar column pins bound its expansion.

### Implementation

#### Structural implementation

`SELECT` is a MatrixRowDecoder, held enabled by `ENABLED`. `SELECT_slot_bit` gates choose each candidate column; `BIT_bit_*` OR trees combine candidates. `DATA_BITS` joins the result into W bits.

### Timing

Built from gates only, so it has no delay table of its own: each output settles when the gates on its path do, under the [gate delays of the active profile](../timing.md#timing-profiles). Its delay is part of the proven tables of the devices built on it.

### Examples

<!-- simulator-examples: matrix-column-mux MatrixColumnMuxTest -->

The two groups contain 0xA5 and 0x3C. Slots 0 and 1 select those words; slot 2 and 0xFFFFFFFF return zero.

#### Embedded simulation

The standalone `MatrixColumnMuxTest` opens this component as `MEMORY_ROOT`.

<div class="simulator-embed">
  <iframe src="../../simulator/?scenario=matrix-column-mux" title="MatrixColumnMux<W,K> recorded simulation" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../simulator/?scenario=matrix-column-mux" class="md-button">Open MatrixColumnMux&lt;W,K&gt; in the simulator</a>

### Source and tests

- [Public interfaces: `DigitalMemory.hpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/include/modules/memory/DigitalMemory.hpp)
- [Implementation and registration: `DigitalMemory.cpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/modules/memory/DigitalMemory.cpp)

| Test | Coverage |
| --- | --- |
| [`MatrixColumnMuxTest`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/tests/MemoryDeviceTests.cpp) | The complete standalone sequence shown above |
| `MemoryDeviceContractTest` | Integration in the new structural devices and comparison with their compact implementations |

## 3.2.3. `MatrixWriteDistributor<W,K>`

Fan out write data and qualify each physical column's write enable.

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `COLUMN_ADDR` | Input | 32 | Word slot |
| `ENABLE` | Input | 1 | Write/program pulse |
| `DATA` | Input | W | Write word |
| `MASK` | Input | W/8 | One enable per byte, low byte first |
| `DATA_c` | Output | 1 each | DATA[c%W], 0 ≤ c < W×K |
| `DRIVE_c` | Output | 1 each | Slot, pulse, and byte-qualified enable |

### Behavior

DATA is replicated to all K column groups. Only the selected group's enabled byte lanes receive DRIVE. An invalid binary slot selects no drive. Mask bit zero controls bits 0–7 of the selected word. Data fanout itself is not gated; DRIVE determines whether storage uses it.

#### C++ example

```cpp
#include <array>
#include <cstdint>
std::array<bool, 2> matrix_write_column(unsigned width, unsigned column,
                                      std::uint32_t slot, bool enable,
                                      std::uint32_t data, unsigned mask) {
    const unsigned bit = column % width;
    const bool drive = enable && slot == column / width &&
                       ((mask >> (bit / 8)) & 1u);
    return {bool((data >> bit) & 1u), drive}; // DATA_c, DRIVE_c
}
```

#### Unknown inputs

A known disabled enable or masked-off lane forces drive low even with uncertain other controls. Unknown enabled masks/addresses propagate through the gates. Data X/Z is copied; the receiving cell decides its storage meaning. W/K have the same bounds as MatrixColumnMux.

### Implementation

#### Structural implementation

`SELECT` decodes slot and ENABLE. `DATA_BITS` splits W data bits. Wider words use `MASK_BITS`; W=8 connects its one-bit mask directly, without a width-one splitter. One `WRITE_slot_byte` gate qualifies each selected byte and fans out to its eight physical columns.

### Timing

Built from gates only, so it has no delay table of its own: each output settles when the gates on its path do, under the [gate delays of the active profile](../timing.md#timing-profiles). Its delay is part of the proven tables of the devices built on it.

### Examples

<!-- simulator-examples: matrix-write-distributor MatrixWriteDistributorTest -->

Tests slots 0, 1, and invalid 2 with ENABLE low/high, checks data fanout and every drive, then disables the byte mask.

#### Embedded simulation

The standalone `MatrixWriteDistributorTest` opens this component as `MEMORY_ROOT`.

<div class="simulator-embed">
  <iframe src="../../simulator/?scenario=matrix-write-distributor" title="MatrixWriteDistributor<W,K> recorded simulation" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../simulator/?scenario=matrix-write-distributor" class="md-button">Open MatrixWriteDistributor&lt;W,K&gt; in the simulator</a>

### Source and tests

- [Public interfaces: `DigitalMemory.hpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/include/modules/memory/DigitalMemory.hpp)
- [Implementation and registration: `DigitalMemory.cpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/modules/memory/DigitalMemory.cpp)

| Test | Coverage |
| --- | --- |
| [`MatrixWriteDistributorTest`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/tests/MemoryDeviceTests.cpp) | The complete standalone sequence shown above |
| `MemoryDeviceContractTest` | Integration in the new structural devices and comparison with their compact implementations |

## 3.2.4. `StaticReadColumn<R>`

Digitally sense one column from exactly one selected ROM/flash row.

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `WL_r` | Input | 1 each | Row selection, 0 ≤ r < R |
| `Q_r` | Input | 1 each | Stored bit in row r |
| `OUT` | Output | 1 | Selected stored bit, otherwise X |

### Behavior

All wordlines must be binary and exactly one must be high. OUT then reproduces that row's bit; no selection or multiple selection returns X. This component has no stored row buffer or destructive-read behavior. DRAM uses its own sense column.

#### C++ example

Binary wordlines with optional stored bits. Empty output means X.

```cpp
#include <optional>
#include <vector>
std::optional<bool> static_read_column(const std::vector<bool>& wl,
                                       const std::vector<std::optional<bool>>& q) {
    unsigned selected = 0;
    std::optional<bool> out;
    for (unsigned r = 0; r < wl.size(); ++r) {
        if (!wl[r]) continue;
        ++selected;
        out = q.at(r);
    }
    return selected == 1 ? out : std::nullopt;
}
```

#### Unknown inputs

Any unknown wordline makes OUT=X, even if another row is known selected. Selected Q=X or Z produces X; unselected Q values do not matter. `rows` accepts 1–512.

### Implementation

#### Structural implementation

Terminal digital selected-row sensing primitive. Its inputs represent cell states and wordlines; it does not simulate an analog shared bitline or multi-driver voltage resolution.

### Timing

A memory primitive: `OUT` follows a change of a selected wordline or stored bit after one read-column interval, the profile's base value plus a per-row term for R rows (a longer column is slower). ROM uses the ROM read-column interval and NOR flash its own read interval; both are listed on the [ROM](technologies/rom.md#timing-intervals) and [NOR-flash](technologies/nor-flash.md#timing-intervals) pages.

### Examples

<!-- simulator-examples: static-read-column StaticReadColumnTest -->

Checks no selection, row zero, conflicting rows, row one, and an unknown row select.

#### Embedded simulation

The standalone `StaticReadColumnTest` opens this component as `MEMORY_ROOT`.

<div class="simulator-embed">
  <iframe src="../../simulator/?scenario=static-read-column" title="StaticReadColumn<R> recorded simulation" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../simulator/?scenario=static-read-column" class="md-button">Open StaticReadColumn&lt;R&gt; in the simulator</a>

### Source and tests

- [Public interfaces: `DigitalMemory.hpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/include/modules/memory/DigitalMemory.hpp)
- [Implementation and registration: `DigitalMemory.cpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/modules/memory/DigitalMemory.cpp)

| Test | Coverage |
| --- | --- |
| [`StaticReadColumnTest`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/tests/MemoryDeviceTests.cpp) | The complete standalone sequence shown above |
| `MemoryDeviceContractTest` | Integration in the new structural devices and comparison with their compact implementations |

## 3.2.5. `MemoryOutputBuffer<W>`

Publish a word or release the output as high impedance.

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `DATA` | Input | W | Candidate output word |
| `ENABLE` | Input | 1 | Output enable |
| `OUT` | Output | W | DATA, Z, or X |

### Behavior

ENABLE=1 copies DATA unchanged; ENABLE=0 emits all Z; ENABLE=X or Z emits all X. The buffer does not validate an address or store data.

#### C++ example

Binary enable with an explicit released result. The simulator also supports X/Z values per bit.

```cpp
#include <cstdint>
#include <optional>
std::optional<std::uint32_t> memory_output(std::uint32_t data, bool enable) {
    return enable ? std::optional(data) : std::nullopt; // nullopt denotes Z here
}
```

#### Unknown inputs

Enabled data preserves every bit, including X or Z. Unknown enable produces all X rather than guessing data or release. W=8/16/32. It does not resolve several drivers the way an electrical bus would.

### Implementation

#### Structural implementation

Terminal digital output-enable primitive. Device control drives ENABLE; the word selector drives DATA. `SRAMOutputBuffer<W>` inherits this same implementation while preserving its existing SRAM-facing identity; DRAM/ROM/NOR instantiate it directly. No separate behavioral replacement is registered.

### Timing

A memory primitive: `OUT` follows `DATA` or `ENABLE` after one memory-primitive delay (one FO4 inverter delay in the realistic profiles; see the [leaf delays](../timing.md#timing-profiles)).

### Examples

<!-- simulator-examples: memory-output-buffer MemoryOutputBufferTest -->

Uses DATA=0xA5 with ENABLE=0, 1, X, and Z, checking all output bits at each checkpoint.

#### Embedded simulation

The standalone `MemoryOutputBufferTest` opens this component as `MEMORY_ROOT`.

<div class="simulator-embed">
  <iframe src="../../simulator/?scenario=memory-output-buffer" title="MemoryOutputBuffer<W> recorded simulation" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../simulator/?scenario=memory-output-buffer" class="md-button">Open MemoryOutputBuffer&lt;W&gt; in the simulator</a>

### Source and tests

- [Public interfaces: `DigitalMemory.hpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/include/modules/memory/DigitalMemory.hpp)
- [Implementation and registration: `DigitalMemory.cpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/modules/memory/DigitalMemory.cpp)

| Test | Coverage |
| --- | --- |
| [`MemoryOutputBufferTest`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/tests/MemoryDeviceTests.cpp) | The complete standalone sequence shown above |
| `MemoryDeviceContractTest` | Integration in the new structural devices and comparison with their compact implementations |

## 3.2.6. Timing and control parts

The devices and controllers sequence their accesses with circuits built
from gates. A few electrical effects that gates cannot express are leaf
parts with stated contracts. These parts are private to the memory
implementation (`include/modules/memory/MemoryTimingParts.hpp`,
`src/modules/memory/MemorySequencers.hpp`) and have no scenarios of their
own; open a device or controller in the simulator and expand it to see them
at work.

| Part | Built from | Used by |
| --- | --- | --- |
| `MemoryDelayLine` | Leaf (an analog interval) | Every timing chain: precharge, bitline, sense, restore, program and erase pulses |
| `MemoryTimer` | Gates, delay lines, `SymbolicDiffer`, `KnownBits` | Asynchronous SRAM and ROM access; the clocked SRAM macro |
| `SymbolicDiffer` | Leaf (an XOR that treats X as a symbol) | Address-change detection in `MemoryTimer` |
| `KnownBits` | Leaf (simulator metadata) | `MemoryTimer`, the request checks |
| `Synchronizer` | Leaf (one or two flip-flop stages) | DRAM and NOR-flash command control: a one-stage sampler for the end of each command's chain |
| `PowerOnReset` | Leaf (a power-up pulse) | DRAM and NOR flash, which have no reset pin |
| `ProtocolStableData` | Wires only (a timing exception) | Read data and `READY` the sequencers capture only at their counted edge or after `BUSY` falls |
| `MemoryWire`, `MemoryBus` | Leaves (global on-chip wires) | Large SRAM arrays split into subarrays |
| `MemoryRequestCheck` | Gates | Every delayed controller |
| `StaticMemorySequencer` | Gates and flip-flops | SRAM and ROM controllers |
| `CommandMemorySequencer` | Gates and flip-flops | DRAM and NOR-flash controllers |

### `MemoryDelayLine`

`OUT` follows `IN` after a fixed delay chosen by the device from the timing
profile and its geometry. It stands for an analog interval of the memory:
a bitline precharging, a sense amplifier developing, a DRAM row restoring, a
flash cell being programmed. It is inertial: a change of `IN` withdraws a
pending opposite change, so a restarted access never sees a stale edge. A
delay line can also fall faster than it rises (a reset-dominant line): a rise
takes the whole interval and a fall passes quickly, which is how self-timed
pulse chains clear at once.

### `MemoryTimer`

The self-timing of an asynchronous access, built from gates and delay lines.
Each input `IN_i` is copied through a settle delay line to `HELD_i`, and the
array reads the held copies, so it keeps seeing the old value while a change
propagates. An input that differs from its held copy marks the access as
changing: `SymbolicDiffer` per bit and an OR tree form an address-transition
detector. The stages then run in order:

- `STAGE_0` = enable and not changing;
- `STAGE_k` follows `STAGE_(k-1)` through stage k's delay line, if it has
  one, and a replica chain of AND gates as deep as the real path it copies
  (for example the row decoder), each gate also ANDed with `STAGE_0`.

Any change of an input, or a disable, clears every stage at once, and the
sequence restarts when the inputs are steady again. Unknown inputs make the
stages unknown. The settle interval is never shorter than the detector's own
path plus one gate, as a real address-transition detector is sized against
its logic. The SRAM chain is settle → precharge (a write drives its bitlines from the
end of precharge) → row enable after a replica of the decoder → bitline development →
sense → `READY`; the ROM chain is settle → decoder replica → read column →
`READY`.

### `SymbolicDiffer` and `KnownBits`

`SymbolicDiffer` is the XOR of an address-transition detector with the
unknown value read as a symbol: `OUT` is 1 when A and B differ, with 0, 1, X
and Z as four symbols. A steady unknown input is steady, where a plain XOR
would report X for as long as it lasts. It has an XOR gate's delay.
`KnownBits` is 1 when every input bit is 0 or 1; detecting an unknown value
is simulator metadata, not a gate.

### `Synchronizer`

Clocked stages that bring an asynchronous signal (the end of a device's
timing chain) into a clock. `Q` shows the
second stage one flip-flop clock-to-output delay after a rising edge; `RST`
clears both stages. A stage that samples an unknown or changing input keeps
its previous value: a real synchronizer resolves to 0 or 1, and that analog
resolution is not modelled. The safe-clock analysis treats its input as an
asynchronous capture, with no setup check and no path through it. The DRAM
and NOR-flash devices use the one-stage form, a resolving sampler for the end
of their own analog steps; the two-stage form is the classic synchronizer.

### `PowerOnReset` and `ProtocolStableData`

`PowerOnReset` holds its output at 1 for a fixed time from the start of the
simulation and then at 0 for good, like the analog circuit that keeps a chip
in reset while its supply comes up; DRAM and NOR flash, which have no reset
pin, start idle through it. `ProtocolStableData` passes its input through
wires and tells the timing analysis that the path is a multicycle path: the
protocol keeps the data stable before it is captured (the SRAM and ROM
sequencers take the read data and `READY` only at the counted edge, the
command sequencer only after the device has released `BUSY` with
`READ_VALID`), as a real design states with a multicycle constraint.

### `MemoryWire` and `MemoryBus`

A large on-chip SRAM is split into 32-row subarrays. `MemoryWire` carries a
bus between a subarray and the periphery with a delay that grows with its
length; `MemoryBus` is a shared line that each subarray drives through its
own tri-state buffer (one driver gives its value, none gives Z or 0 on a
pulled-down status line, disagreeing drivers give X).

### `MemoryRequestCheck`

The request rules of a delayed controller as gates: unknown fields, the
operation, the write mask, word alignment, the address range and the
technology's permissions, checked in the same order as the
[protocol's status precedence](transactions.md#protocol-rules). Its outputs
are `ACCEPT`, the 3-bit `STATUS`, the decoded operation, and the word
address (the byte address shifted by the word size).

### `StaticMemorySequencer`

The controller sequencer of an SRAM or ROM, as gates and flip-flops. On the
transaction side it has the port's backend pins (`START`, `FINISH`, the
request fields; `BACK_READY`, `DONE`, `RESULT`, `STATUS`); on the device side
the native pins with a `MEM_` prefix. `START` loads the request registers. A
valid request enters its access, which drives the chip, write and output
enables and the held fields for `access_cycles` clock edges; the last edge
loads the result and status and raises `DONE`, with status `Device` if the
device's `READY` is not high then. `FINISH` clears `DONE`. An invalid request
completes at once with its status.

For the clocked SRAM macro the request is shown to the macro during the
cycle `START` is high (chip and write enables low for that cycle only,
address, data and mask straight from the request); the macro registers it at
that edge, `MEM_CLK` passes the clock, and `access_cycles` counts from that
edge to the capture.

### `CommandMemorySequencer`

The controller sequencer of a command-driven device (DRAM, NOR flash). The
device runs on the same clock (`MEM_CLK`) and answers with registered
`BUSY`, `ERROR`, `READ_VALID` (and `ROW_OPEN` for DRAM). Each request is
planned as a short list of device commands: for DRAM a PRECHARGE if another
row is open, an ACTIVATE if no row is open, then READ, READ_BURST or WRITE;
for NOR flash a WRITE ENABLE before PROGRAM or ERASE BLOCK. The next command
is shown whenever the device is idle and the previous one ended without a
fault, and the device takes it at the next edge. For DRAM, a cycle counter
raises a pending refresh, served from idle (PRECHARGE if a row is open, then
REFRESH) while new requests wait.

### Tests

These parts are exercised through the devices and controllers built from
them: `MemoryDeviceContractTest`, `MemoryDeviceTimingTest`,
`MemoryEndpointContractTest`, `MemoryEndpointTimingTest`, the endpoint
scenarios, and `DelayRuleTest`, which compares every device and controller
with its compact form in all three profiles.
