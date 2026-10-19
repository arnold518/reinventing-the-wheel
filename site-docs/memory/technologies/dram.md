# § 3.3.3. DRAM

One-bank digital DRAM with explicit activation, a sensed row buffer, restoration, precharge, and refresh. It follows the [common device architecture](../architecture.md), but does not reuse FF/SRAM cell semantics.

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| `DRAMCell` | Charge bit with destructive sampling and restoration | Yes | No |
| `DRAMRow<C>` | C cells sharing a wordline | Yes | Yes |
| `DRAMSenseColumn<R>` | One row-buffer bit and restoration signal | Yes | No |
| `DRAMStorageBank<W>` | Rows connected to shared sense columns | Yes | Yes |
| `DRAMCommandControl<W>` | Clocked command acceptance and the timing chains of each command, as gates and delay lines | Yes | No |
| `DRAM<W>` | Complete single-bank device | Yes | Yes |

Real DRAM controllers track open rows, issue precharge and activate commands, and respect the device's intervals. This device keeps those distinctions, with intervals from the active [timing profile](../../timing.md): in `embedded` an SDR SDRAM (Micron MT48LC16M16A2), in `desktop` a DDR4 part (Micron MT40A1G8). It is not a pin-compatible SDRAM or DDR interface. See [Microchip's SDRAM controller description](https://onlinedocs.microchip.com/oxy/GUID-B822915F-C375-4172-91BD-AB6F326EB783-en-US-1/GUID-A9CFAA65-9F00-4E1D-AB82-84F228BBF5B6.html).

## Configuration

W, N, and K follow [3.1](../architecture.md#width-capacity-and-geometry). Defaults are W=8 in examples, N=4, K=2.

| Parameter | Default | Meaning / constraints |
| --- | ---: | --- |
| `retention_ticks` | From the profile | How long a closed row keeps its charge; 1,024 … 10¹². In the realistic profiles 64 ms (tREF) |
| `block_words` | 2 | Columns read by one READ_BURST: a power of two, at most the row's words and 16 |
| `phase_ticks` | 32 | Accepted (32…10,000) for compatibility; it no longer sets any timing |

Storage starts `X`. Retention is checked **lazily at the next activation**, avoiding periodic decay events for every cell. A cell closed for at least `retention_ticks` samples `X`; refresh after expiry cannot recover its old data. While a row is open, the enabled sense amplifiers restore it. A displayed idle cell Q is not a continuously updated analog charge measurement.

Compile the C++ examples in page order: larger references compose the earlier ones. They cover binary controls, stored state and outputs. Their command steps use one placeholder duration, `ticks`, where the device uses the profile's intervals (see [Timing](#timing-intervals)); they do not model gate propagation or analog behavior.

Configuration is `DRAMDeviceConfig`. Catalog contracts are `memory.dram.cell`, `memory.dram.row`, `memory.dram.sense-column`, `memory.dram.bank.W`, `memory.dram.command-control.W`, and `memory.dram.device.W`. Standalone rows use `columns` and `retention_ticks`; sense columns use `rows`. Only the public bank/device wrappers are C++ width templates. The command controller is catalog-created, not a public C++ template.

## 3.3.3.1. `DRAMCell`

One digital charge bit with destructive sampling and restoration.

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `WL` | Input | 1 | Wordline; activation samples charge |
| `RESTORE` | Input | 1 | Rewrite charge from D while WL is high |
| `D` | Input | 1 | Restore/write value |
| `SAMPLE` | Output | 1 | Value sampled on activation; held until close |
| `Q` | Output | 1 | Modeled cell charge |

### Behavior

A newly selected cell copies retained charge into SAMPLE and invalidates Q. With WL=RESTORE=1, D restores Q. Closing clears SAMPLE and starts the retention interval; closing without restoration loses the bit. Reading a cell alone is destructive: the bank's sense column supplies restoration.

#### C++ example

Binary controls with `std::optional<bool>` representing known charge or X. Retention is tested at activation.

```cpp
#include <cstddef>
#include <optional>
struct DRAMCellReference {
    std::optional<bool> q, sample;
    bool selected = false, restored = false;
    std::size_t closed_at = 0;
    void update(std::size_t now, std::size_t retention,
                bool wl, bool restore, std::optional<bool> data) {
        if (wl && !selected) {
            sample = now - closed_at >= retention ? std::nullopt : q;
            q.reset();
            restored = false;
        }
        if (wl && restore) { q = data; restored = true; }
        if (!wl && selected) {
            if (!restored) q.reset();
            sample.reset();
            closed_at = now;
            restored = false;
        }
        selected = wl;
    }
};
```

#### Unknown inputs

Starts X. Unknown WL makes charge and sample unknown; unknown RESTORE while selected makes charge unknown. D=Z stores X. WL=0 ignores restore/data. `retention_ticks` accepts 1,024–10¹². Expiry has no periodic event: the next activation observes loss.

### Implementation

#### Structural implementation

Terminal digital charge primitive, not a flip-flop or an RC electrical simulation. Q and SAMPLE make the storage/sensing distinction explicit. No redundant behavioral-fidelity replacement is registered.

The visualizer displays separate **charge (Q)** and **read sample (SAMPLE)** panels, with WL/RESTORE indicators, using recorded pin values at the selected time. It does not invent an analog charge level or retention countdown; expiry is checked on activation as described above.

### Timing

A memory primitive: its outputs follow its inputs after one memory-primitive delay (one FO4 inverter delay in the realistic profiles). Retention is a time, `retention_ticks`, not a delay: a row closed for that long samples X.

### Examples

<!-- simulator-examples: dram-cell DRAMCellTest -->

The sequence starts unknown, restores one, closes, samples that one destructively without restoring, observes loss, then writes zero. Device timing tests separately cover retention and refresh.

#### Embedded simulation

The standalone `DRAMCellTest` opens this component as `MEMORY_ROOT`. This component has one registered structural implementation.

<div class="simulator-embed">
  <iframe src="../../../simulator/?scenario=dram-cell" title="DRAMCell recorded simulation" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../simulator/?scenario=dram-cell" class="md-button">Open DRAMCell in the simulator</a>

### Source and tests

- [Public interfaces: `DRAM.hpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/include/modules/memory/DRAM.hpp)
- [Implementation and registration: `DRAM.cpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/modules/memory/DRAM.cpp)

| Test | Coverage |
| --- | --- |
| [`DRAMCellTest`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/tests/MemoryDeviceTests.cpp) | The complete standalone sequence shown above |
| `MemoryDeviceTimingTest` | Retention, refresh before expiry, and loss after expiry through structural and compact devices |

## 3.3.3.2. `DRAMRow<C>`

C charge cells sharing one wordline.

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `WL` | Input | 1 | Shared wordline |
| `RESTORE_c` | Input | 1 each | Column c restoration enable |
| `D_c` | Input | 1 each | Column c restore/write value |
| `Q_c`, `SAMPLE_c` | Output | 1 each | Charge and sample, 0 ≤ c < C |

### Behavior

`columns=C` creates 1–512 independent cells (default 4). Each follows the DRAMCell rule with shared WL and per-column restore/data. A row may contain K words; it has no address decoder or command port.

#### C++ example

Row composition. Instantiate `Cell` with the `DRAMCellReference` above; each record owns its charge, sample, and closed timestamp.

```cpp
#include <array>
#include <cstddef>
#include <optional>
template<class Cell, std::size_t C>
void dram_row_update(std::array<Cell, C>& cells, std::size_t now,
                     std::size_t retention, bool wl,
                     const std::array<bool, C>& restore,
                     const std::array<std::optional<bool>, C>& data) {
    for (std::size_t c = 0; c < C; ++c)
        cells[c].update(now, retention, wl, restore[c], data[c]);
}
```

#### Unknown inputs

Each column applies the cell's X/Z and lazy-retention rules independently. An unknown shared WL affects every cell. Use separate settled activation and restoration steps; this is not an instantaneous atomic row read.

### Implementation

#### Structural implementation

`CELL_0` through `CELL_(C−1)` are DRAMCell primitives. WL fans out to all cells; restoration and output pins remain per-column. Cells are laid out horizontally.

#### Behavioral implementation

Stores C charge records and applies sampling, restoration, close timestamps, and expiry directly. It preserves expanded row pins but removes the cell objects.

### Timing

<!-- timing-table: memory.dram.row.behavioral -->

### Examples

<!-- simulator-examples: dram-row DRAMRowTest -->

The two-column example restores one and zero, closes the row, then reactivates it and checks both samples.

#### Embedded simulation

The standalone `DRAMRowTest` opens this component as `MEMORY_ROOT`. Use the explorer to compare structural and behavioral implementations.

<div class="simulator-embed">
  <iframe src="../../../simulator/?scenario=dram-row" title="DRAMRow<C> recorded simulation" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../simulator/?scenario=dram-row" class="md-button">Open DRAMRow&lt;C&gt; in the simulator</a>

### Source and tests

- [Public interfaces: `DRAM.hpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/include/modules/memory/DRAM.hpp)
- [Implementation and registration: `DRAM.cpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/modules/memory/DRAM.cpp)

| Test | Coverage |
| --- | --- |
| [`DRAMRowTest`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/tests/MemoryDeviceTests.cpp) | The complete standalone sequence shown above |
| `MemoryDeviceContractTest` | Structural/behavioral agreement at settled checkpoints, including mixed hierarchy selection |

## 3.3.3.3. `DRAMSenseColumn<R>`

One shared column's row-buffer bit and restoration control.

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `PRECHARGE` | Input | 1 | Prepare the column with all wordlines low |
| `SENSE` | Input | 1 | Enable sensing/restoration |
| `WL_r`, `SAMPLE_r` | Input | 1 each | Selection and sampled charge, 0 ≤ r < R |
| `WRITE` | Input | 1 | Replace the active row-buffer bit |
| `D` | Input | 1 | Write value |
| `Q` | Output | 1 | Latched row-buffer bit |
| `RESTORE` | Output | 1 | Feed Q into the selected cell |

### Behavior

After precharge, sensing exactly one selected row latches its SAMPLE and asserts RESTORE. Subsequent SAMPLE changes do not overwrite the latch; WRITE changes it. No active sensing means RESTORE=0 while the buffer can remain latched. Switching to another row without precharging invalidates the buffer. `rows=R` accepts 1–512, default 2.

#### C++ example

Binary controls, including zero/multiple selected rows. An empty data optional means X. The reference retains the buffer when sensing is disabled.

```cpp
#include <cstddef>
#include <optional>
#include <vector>
struct DRAMSenseReference {
    bool prepared = true, restore = false;
    std::optional<std::size_t> sensed_row;
    std::optional<bool> q;
    void update(bool precharge, bool sense, const std::vector<bool>& wl,
                const std::vector<std::optional<bool>>& samples,
                bool write, std::optional<bool> data) {
        std::optional<std::size_t> row;
        unsigned count = 0;
        for (std::size_t r = 0; r < wl.size(); ++r)
            if (wl[r]) { row = r; ++count; }
        restore = false;
        if (precharge && !count) {
            prepared = true;
            sensed_row.reset(); q.reset();
        } else if (precharge || count > 1) {
            prepared = false;
            sensed_row.reset(); q.reset();
        } else if (sense && row) {
            if (!prepared || (sensed_row && sensed_row != row)) {
                prepared = false; sensed_row.reset(); q.reset();
                return;
            }
            if (!sensed_row) { sensed_row = row; q = samples.at(*row); }
            if (write) q = data;
            restore = true;
        }
    }
};
```

#### Unknown inputs

Starts prepared with Q=X. Unknown wordlines, more than one selected row, unknown SENSE, or invalid precharge conditions invalidate preparation and Q. Unknown WRITE during sensing makes Q unknown. PRECHARGE=1 with all wordlines low prepares regardless of SENSE. It is a memory primitive; samples must settle before SENSE.

### Implementation

#### Structural implementation

Terminal stateful sense primitive. Its latched Q forms one bit of the bank's row buffer; RESTORE feeds the selected cells. It models the result of sensing/restoration, not voltage differences or transistor gain.

### Timing

A memory primitive: its outputs follow its inputs after one sense interval, the profile's base value plus a per-row term for its R rows; see the DRAM intervals under the device's [Timing](#timing-intervals).

### Examples

<!-- simulator-examples: dram-sense-column DRAMSenseColumnTest -->

Precharge, activate row zero, sense one, change SAMPLE to X while retaining the latch, write zero into the buffer, then assert conflicting wordlines and check invalidation.

#### Embedded simulation

The standalone `DRAMSenseColumnTest` opens this component as `MEMORY_ROOT`. This component has one registered structural implementation.

<div class="simulator-embed">
  <iframe src="../../../simulator/?scenario=dram-sense-column" title="DRAMSenseColumn<R> recorded simulation" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../simulator/?scenario=dram-sense-column" class="md-button">Open DRAMSenseColumn&lt;R&gt; in the simulator</a>

### Source and tests

- [Public interfaces: `DRAM.hpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/include/modules/memory/DRAM.hpp)
- [Implementation and registration: `DRAM.cpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/modules/memory/DRAM.cpp)

| Test | Coverage |
| --- | --- |
| [`DRAMSenseColumnTest`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/tests/MemoryDeviceTests.cpp) | The complete standalone sequence shown above |
| `MemoryDeviceContractTest` | Integration in the new structural devices and comparison with their compact implementations |

## 3.3.3.4. `DRAMStorageBank<W>`

A matrix of charge cells connected to a shared row buffer.

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `PRECHARGE`, `SENSE` | Input | 1 each | Shared column controls |
| `WL_r` | Input | 1 each | Row wordlines, 0 ≤ r < N/K |
| `DRIVE_c`, `DATA_c` | Input | 1 each | Column write enables and values |
| `COL_c` | Output | 1 each | Row-buffer bits, 0 ≤ c < W×K |

### Behavior

Activate one wordline, allow cell samples to settle, then enable SENSE. Each column latches and restores its sample. DRIVE_c writes that column's row-buffer bit and restores it into the selected cell. Close the wordline before precharge. Address translation, byte masks, and legal command sequencing belong to the device.

#### C++ example

The bank composes the preceding cell and sense-column references. Each call settles charge sampling, column sensing, and restoration in that order; it does not model the individual gate delays between them.

```cpp
#include <vector>
struct DRAMBankReference {
    std::vector<std::vector<DRAMCellReference>> rows;
    std::vector<DRAMSenseReference> columns;
    DRAMBankReference(std::size_t r, std::size_t c)
        : rows(r, std::vector<DRAMCellReference>(c)), columns(c) {}
    void update(std::size_t now, std::size_t retention, bool precharge, bool sense,
                const std::vector<bool>& wl, const std::vector<bool>& drive,
                const std::vector<std::optional<bool>>& data) {
        for (std::size_t c = 0; c < columns.size(); ++c) {
            std::vector<std::optional<bool>> samples;
            for (std::size_t r = 0; r < rows.size(); ++r) {
                rows[r][c].update(now, retention, wl.at(r), false, std::nullopt);
                samples.push_back(rows[r][c].sample);
            }
            auto& column = columns[c];
            column.update(precharge, sense, wl, samples, drive.at(c), data.at(c));
            for (std::size_t r = 0; r < rows.size(); ++r)
                rows[r][c].update(now, retention, wl.at(r), column.restore, column.q);
        }
    }
    // COL_c = columns[c].q. Cell charge and samples remain independently visible.
};
```

#### Unknown inputs

Selection conflicts and unknown controls follow DRAMSenseColumn; individual charge records follow DRAMCell. Closing without valid restoration can lose data. Both bank fidelities retain scalar pins and the 4,096-cell limit. Compare settled checkpoints, not transient gate waveforms or the exact retention boundary, across fidelities.

### Implementation

#### Structural implementation

`ROW_r` contains W×K cells. `COLUMN_c` is a DRAMSenseColumn connected to SAMPLE_c from every row, with Q/RESTORE feedback to every cell in that column. WL qualifies which cell accepts restoration. Rows are stacked and sense columns align beneath them.

#### Behavioral implementation

Uses charge records plus a row-buffer vector, with independent selection/preparation state. It preserves destructive sampling, restoration, precharge, per-column writes, and retention without expanding the cells.

### Timing

<!-- timing-table: memory.dram.bank.8.behavioral -->

### Examples

<!-- simulator-examples: dram-storage-bank DRAMStorageBankTest -->

Writes alternating bits across row zero, releases write drive, closes/precharges, reopens the row, and checks that restoration preserved all 16 columns.

#### Embedded simulation

The standalone `DRAMStorageBankTest` opens this component as `MEMORY_ROOT`. Use the explorer to compare structural and behavioral implementations.

<div class="simulator-embed">
  <iframe src="../../../simulator/?scenario=dram-storage-bank" title="DRAMStorageBank<W> recorded simulation" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../simulator/?scenario=dram-storage-bank" class="md-button">Open DRAMStorageBank&lt;W&gt; in the simulator</a>

### Source and tests

- [Public interfaces: `DRAM.hpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/include/modules/memory/DRAM.hpp)
- [Implementation and registration: `DRAM.cpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/modules/memory/DRAM.cpp)

| Test | Coverage |
| --- | --- |
| [`DRAMStorageBankTest`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/tests/MemoryDeviceTests.cpp) | The complete standalone sequence shown above |
| `MemoryDeviceContractTest` | Structural/behavioral agreement at settled checkpoints, including mixed hierarchy selection |

## 3.3.3.5. `DRAMCommandControl<W>`

Takes a command at a rising clock edge and runs that command's timing chain
of gates and delay lines.

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `CLK` | Input | 1 | Rising edge takes a command |
| `CMD` | Input | 3 | NOP, ACTIVATE, READ, WRITE, PRECHARGE, REFRESH, READ_BURST |
| `ADDR` | Input | 32 | Row for ACTIVATE, word slot for READ, WRITE and READ_BURST |
| `WRITE_DATA` | Input | W | Write payload |
| `WRITE_MASK` | Input | W/8 | Byte enables |
| `BUSY`, `ERROR`, `READ_VALID`, `ROW_OPEN` | Output | 1 each | Device status |
| `ROW_ADDR`, `COLUMN_ADDR` | Output | 32 each | Latched row and word slot |
| `DATA` | Output | W | Latched write payload |
| `MASK` | Output | W/8 | Latched write byte enables |
| `PRECHARGE`, `SENSE` | Output | 1 each | Column controls |
| `ROW_ENABLE`, `WRITE_ENABLE` | Output | 1 each | Row and write controls |
| `OUTPUT_ENABLE` | Output | 1 | A finished read's data is on the output |

### Behavior

Carries out the [command table](#commands-and-timing) of `DRAM<W>` without
storing array data. A legal command at a rising edge latches what it needs,
clears an old error and read-valid state, raises `BUSY`, and starts its
chain: each step of the command is a delay line from the profile, gated by
the command's run signal. The end of the chain is sampled at the next rising
edge by a one-stage sampler; the completion is then shown at once (`BUSY`
low, `READ_VALID` for a read, the new `ROW_OPEN` state) and takes effect at
the following edge, which may already take the next command, as a real SDRAM
takes its next command when the interval is over. NOP keeps the status. An
illegal command while idle reports `ERROR`; a command while busy reports
`ERROR` without disturbing the running operation.

#### C++ example

Binary pin values; geometry is prevalidated. Call on input changes and at every nonzero `due`. `output` supplies both READ_VALID and OUTPUT_ENABLE; `wl` supplies ROW_ENABLE. Every step takes the same placeholder `ticks`, and a new command waits until the previous one has finished (the device can take it in the completing cycle). READ_BURST is not modelled here.

```cpp
#include <cstdint>
#include <cstddef>
#include <optional>
#include <vector>
template<std::size_t W> struct DRAMControlReference {
    struct Inputs {
        bool clock = false;
        unsigned cmd = 0;
        std::uint32_t address = 0, data = 0;
        unsigned mask = 0;
    };
    std::size_t rows, slots, ticks, due = 0;
    unsigned command = 0, stage = 0;
    std::size_t row = 0, col = 0, refresh_row = 0;
    bool busy = false, error = false, open = false, precharge = true;
    bool sense = false, wl = false, write = false, output = false;
    std::optional<bool> previous_clock; // First observed high is not a 0->1 edge.
    std::vector<std::optional<bool>> data = std::vector<std::optional<bool>>(W);
    unsigned mask = 0;
    DRAMControlReference(std::size_t r, std::size_t k, std::size_t t)
        : rows(r), slots(k), ticks(t) {}
    void update(std::size_t now, const Inputs& in) {
        // Finish the current phase before accepting a simultaneous command edge.
        if (due && now >= due) {
            due = 0; ++stage;
            switch (command) {
            case 1: // ACT: select -> sense/restore -> row open.
                if (stage == 1) wl = true;
                else if (stage == 2) sense = true;
                else { open = true; busy = false; }
                break;
            case 2: output = true; busy = false; break;
            case 3:
                if (stage == 1) write = true;
                else if (stage == 2) write = false;
                else busy = false;
                break;
            case 4:
                if (stage == 1) { wl = false; sense = false; }
                else if (stage == 2) precharge = true;
                else { open = false; busy = false; }
                break;
            case 5: // Refresh exactly one row, then advance the cursor.
                if (stage == 1) wl = true;
                else if (stage == 2) sense = true;
                else if (stage == 3) { wl = false; sense = false; }
                else if (stage == 4) precharge = true;
                else { refresh_row = (refresh_row + 1) % rows; busy = false; }
                break;
            }
            if (busy) due = now + ticks;
        }
        const bool edge = previous_clock == false && in.clock;
        previous_clock = in.clock;
        if (!edge || in.cmd == 0) return; // NOP preserves status, even while busy.
        if (busy) { error = true; return; } // Do not disturb the active operation.
        bool allowed = false;
        switch (in.cmd) {
        case 1: allowed = !open && in.address < rows; break;
        case 2: case 3: allowed = open && in.address < slots; break;
        case 4: allowed = open; break;
        case 5: allowed = !open; break;
        }
        output = false;
        if (!allowed) { error = true; return; }
        error = false; busy = true; command = in.cmd; stage = 0; due = now + ticks;
        if (command == 1 || command == 5) {
            row = command == 1 ? in.address : refresh_row;
            precharge = wl = sense = false;
        }
        if (command == 2 || command == 3) col = in.address;
        if (command == 3) {
            for (unsigned b = 0; b < W; ++b) data[b] = bool((in.data >> b) & 1u);
            mask = in.mask;
        }
    }
};
```

#### Unknown inputs

Only CLK=0 followed by CLK=1 takes a command; X/Z→1 is not an edge. An unknown CMD reports `ERROR`. ACTIVATE, READ and WRITE need a binary ADDR; WRITE also needs a binary mask. Unknown payload bits become unknown storage in the enabled bytes. Inputs a command does not use are ignored.

### Implementation

#### Structural implementation

Flip-flops hold the command, row, column, payload and status bits; gates
decide legality and the next state at each edge. Each command's steps are
`MemoryDelayLine`s in a chain (ACTIVATE: wordline, sense, restore; READ:
column; WRITE: drive on, drive off, recovery; PRECHARGE: close, precharge,
equalize; REFRESH: an activation and a precharge of the next refresh row,
plus the refresh cycle's remaining time in the realistic profiles). The sense
and precharge steps also carry a replica of the row decoder, so they outlast
it in every profile. A one-stage `Synchronizer` samples the end of the chain;
a round-robin counter chooses the refresh row. A device with no reset pin
starts idle through a `PowerOnReset`.

### Timing

Built from gates, flip-flops and delay lines, so it has no delay table of its
own. Its intervals are the DRAM intervals listed under the device's
[Timing](#timing-intervals); the device table below includes it.

### Examples

<!-- simulator-examples: dram-command-control DRAMCommandControlTest -->

The sequence checks a read while closed, activation, write/read, byte mask, unknown and reserved commands, NOP status preservation, invalid column, precharge, and one refresh.

#### Embedded simulation

The standalone `DRAMCommandControlTest` opens this component as `MEMORY_ROOT`. Expand it to follow the command chains.

<div class="simulator-embed">
  <iframe src="../../../simulator/?scenario=dram-command-control" title="DRAMCommandControl<W> recorded simulation" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../simulator/?scenario=dram-command-control" class="md-button">Open DRAMCommandControl&lt;W&gt; in the simulator</a>

### Source and tests

- [Public interfaces: `DRAM.hpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/include/modules/memory/DRAM.hpp)
- [Implementation and registration: `DRAM.cpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/modules/memory/DRAM.cpp)

| Test | Coverage |
| --- | --- |
| [`DRAMCommandControlTest`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/tests/MemoryDeviceTests.cpp) | The complete standalone sequence shown above |
| `MemoryDeviceTimingTest` | Intermediate BUSY and ROW_OPEN, and the command chains in the device |

## 3.3.3.6. `DRAM<W>`

Complete single-bank command-addressed DRAM device.

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `CLK` | Input | 1 | Strict 0→1 command edge |
| `CMD` | Input | 3 | Command code below |
| `ADDR` | Input | 32 | Row for ACT; word slot for READ/WRITE |
| `WRITE_DATA` | Input | W | Latched write payload |
| `WRITE_MASK` | Input | W/8 | Byte enables |
| `BUSY` | Output | 1 | Command in progress |
| `ERROR` | Output | 1 | Rejected request; next accepted non-NOP clears it |
| `ROW_OPEN` | Output | 1 | Activation completed; row available |
| `READ_VALID` | Output | 1 | Completed read available |
| `READ_DATA` | Output | W | Selected word while valid, otherwise Z |

### Behavior

Commands operate on one open row at a time. A word's linear index is `row*K + slot`; ADDR itself is not always a linear word address.

#### Commands and timing

A command is taken at a rising `CLK` edge while the device is idle, or in the
cycle the previous command completes. Each step lasts one of the profile's
DRAM intervals (listed under [Timing](#timing-intervals)).

| CMD | Operation | Legal when / ADDR | Steps |
| ---: | --- | --- | --- |
| 0 | NOP | Inputs ignored | None; status kept |
| 1 | ACTIVATE | No row open; `ADDR < N/K` | Wordline, sense, restore; then the row is open |
| 2 | READ | Row open; `ADDR < K` | Column read; then `READ_VALID` |
| 3 | WRITE | Row open; `ADDR < K`; binary mask | Drive on, drive off, recovery |
| 4 | PRECHARGE | Row open; ADDR ignored | Close, precharge, equalize; then no row is open |
| 5 | REFRESH | No row open; ADDR ignored | Activation and precharge of the next refresh row (and the rest of tRFC) |
| 6 | READ_BURST | Row open; `ADDR < K` | As READ; then the next columns of the aligned `block_words` block, wrapping within it, one per edge |
| 7 | Reserved | Always rejected | `ERROR`; no storage operation |

REFRESH services **one row** at an internal round-robin cursor starting at
zero; N/K refresh commands cover the bank. The device does not refresh
itself; the [DRAM controller](../controllers.md) issues REFRESH, and a direct
user must do the same. Refresh samples an expired row as X and cannot repair
it.

Write data, address and mask are latched when the command is taken. A zero
mask is a valid write that changes nothing. Unknown payload bits become
unknown storage only in enabled byte lanes. An initial `CLK=X` followed by
high is not an edge. `ROW_OPEN` stays high during a precharge until it
completes. A finished read stays valid until another command is taken or an
idle illegal command clears it.

#### C++ example

Complete native-port reference composed from the command control and bank above, for W=8/16/32: command rejection, refresh, retention, destructive sampling and restoration, byte writes, status and disabled read output, with the placeholder step duration. Binary inputs and validated geometry are assumed; empty data optionals preserve unknown stored bits.

```cpp
#include <cstddef>
#include <optional>
#include <vector>
template<std::size_t W> struct DRAMReference {
    static_assert(W == 8 || W == 16 || W == 32);
    DRAMControlReference<W> control;
    DRAMBankReference bank;
    std::size_t retention;
    std::vector<std::optional<bool>> read_data = std::vector<std::optional<bool>>(W);
    bool high_z = true;
    DRAMReference(std::size_t words, std::size_t slots, std::size_t ticks,
                  std::size_t retention_ticks)
        : control(words / slots, slots, ticks), bank(words / slots, W * slots),
          retention(retention_ticks) {}
    void update(std::size_t now, const typename DRAMControlReference<W>::Inputs& in) {
        control.update(now, in);
        std::vector<bool> wl(control.rows, false), drive(W * control.slots, false);
        std::vector<std::optional<bool>> data(W * control.slots);
        if (control.wl) wl[control.row] = true;
        for (std::size_t c = 0; c < data.size(); ++c) {
            data[c] = control.data[c % W];
            drive[c] = control.write && c / W == control.col &&
                       (control.mask & (1u << ((c % W) / 8)));
        }
        bank.update(now, retention, control.precharge, control.sense, wl, drive, data);
        high_z = !control.output;
        for (std::size_t b = 0; b < W; ++b)
            read_data[b] = high_z ? std::nullopt : bank.columns[control.col * W + b].q;
    }
    // BUSY / ERROR / READ_VALID / ROW_OPEN = control.busy / error / output / open.
    // Next scheduled device phase: control.due (0 means none).
};
```

#### Unknown inputs

Command validation follows `DRAMCommandControl`. A rejected unknown idle command clears an earlier read just like a reserved command; a command rejected while busy leaves the operation running. Masked-off bytes ignore unknown payload. Retention is measured from when a row closes to its next activation.

### Implementation

#### Structural implementation

`COMMAND_CONTROL` drives `ROW_DECODE` and storage phases. `STORAGE` exposes cell rows and sense/restore columns; `COLUMN_SELECT` reads one word from the row buffer; `WRITE_DRIVERS` distributes byte-qualified writes; `OUTPUT` publishes only valid data. No FF or SRAM memory is hidden underneath.

#### Behavioral implementation

Uses packed `MemoryStorage`, per-row close times and a W×K row buffer. Activation samples the row destructively, sensing restores it, and writes merge enabled bytes into the buffer and storage. Its command control (`DRAMSyncControl`) follows the gate-level control edge for edge, with the same chain lengths, so it takes the same cycles; the storage and sense data path is implemented separately. History is read-only; initialize through ACTIVATE and WRITE, and disable history for large runs.

### Timing

<a id="timing-intervals"></a>The steps of each command are delay lines with
these intervals. In the realistic profiles they reproduce the datasheet's
tRCD, CL and tRP (the ACTIVATE steps add up to tRCD, the READ step is CL,
the PRECHARGE steps add up to tRP):

<!-- profile-table: memory-dram -->

The compact device's proven delays. The device times itself, so no clock can
be certified around it alone; its controller samples it at the controller's
clock (see [3.6 Memory Timing](../timing.md#dram)).

<!-- timing-table: memory.dram.device.8.behavioral -->

### Examples

<!-- simulator-examples: dram DRAMTest -->

The W=8 device writes `0x81`, changes its enabled byte to `0x5A`, exercises invalid commands and status preservation, closes, refreshes, and reads `0x5A` again. Wider byte-mask coverage is in the contract test. The table contains setup rows as well as command-completion rows.

#### Embedded simulation

The standalone `DRAMTest` opens this component as `MEMORY_ROOT`. Use the explorer to compare structural and behavioral implementations.

<div class="simulator-embed">
  <iframe src="../../../simulator/?scenario=dram" title="DRAM<W> recorded simulation" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../simulator/?scenario=dram" class="md-button">Open DRAM&lt;W&gt; in the simulator</a>

### Source and tests

- [Public interfaces: `DRAM.hpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/include/modules/memory/DRAM.hpp)
- [Implementation and registration: `DRAM.cpp`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/modules/memory/DRAM.cpp)

| Test | Coverage |
| --- | --- |
| [`DRAMTest`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/src/tests/MemoryDeviceTests.cpp) | The complete standalone sequence shown above |
| `MemoryDeviceContractTest` | W=8/16/32, K=1/2/4, mixed fidelity, a masked-write oracle, geometry rejection, and compact history/allocation |
| `MemoryDeviceTimingTest` | Early BUSY, refresh preservation, expiry, and event-queue quiescence |
| [`MemoryDeviceDocumentationTest`](https://github.com/arnold518/reinventing-the-wheel/blob/github-pages/tests/MemoryDeviceDocumentationTest.py) | Compile the exact references; test cell/bank feedback, W=8/16/32 device behavior, refresh/expiry, busy rejection and simultaneous completion/command edges; replay native command/device binary-input prefixes |
