# Legacy array container: implementation contract

This is a **historical legacy array-port** implementation report; its generic
container and old array aliases have been retired. See [cleanup](rv32i-cleanup.md).
Current controllers and scalable technology models are
documented in [3.1.6. Scalable Memory Container](../site-docs/memory/3.1.6.md),
[3.2. Memory Transactions](../site-docs/memory/3.2.md), and
[3.3. Memory Controllers](../site-docs/memory/3.3.md).
Statements below about missing native technologies describe this earlier step.

## Scope

`LegacyMemoryContainer<DataWidth, BehaviorModel>` separates fixed pin width and device
behavior from runtime capacity. The initial models are FF arrays, asynchronous
SRAM, ROM, and an explicitly named ideal RAM reference. The shared backing store
also implements the existing `Memory64Kx32` storage/history APIs.

The FF device is now named `FFMemory<DataWidth>`. Its structural implementation
uses `FFStorageBank<DataWidth>`, `FFWord<DataWidth>`, and `FlipFlopCell`; its
compact behavioral implementation reuses `LegacyMemoryContainer<DataWidth,
FlipFlopArrayModel>`. These names distinguish FF construction from future
technology-specific SRAM, DRAM, and flash designs. A common backing store does
not imply that all physical memory technologies share FF pins or behavior.

`MemoryArray<DataWidth, Cell>` remains a compatibility entry point and the
existing structural implementation for the latch/constant representatives.
The FF specialization delegates to `FFMemory` without an extra hierarchy level.
`Register32`, `Memory4x32`, and `Memory32x32` remain unchanged; this
step does not migrate their consumers or claim their different ports are
interchangeable with the array port.

`MemoryBusAdapter32` connects a request/response client to a **separate** 32-bit
array. It contains request/response registers and control logic, not storage.
The existing RV32I systems still use `Memory64Kx32` and their original interfaces.
CPU adapters, cache controllers, DRAM, flash, and variable-latency device
controllers are not implemented by this change.

## Construction and capacity

```cpp
#include "modules/memory/LegacyMemoryContainer.hpp"

SRAMConfig config{
    .depth_words = 4096,
    .read_delay_ticks = 3,
    .initial_value = LogicValue::UNKNOWN,
    .record_history = true,
};
auto memory = Component::create<LegacyMemoryContainer<32, SRAMModel>>("SRAM", config);
memory->loadMemoryBytes(0, {0x78, 0x56, 0x34, 0x12});
// 4096 words * 4 bytes/word = 16384 bytes (16 KiB).
```

Currently supported widths are 8, 16, and 32 bits. Depth need not be a power of
two. Zero depth, invalid delays, and overflowing geometry are rejected before
use. Capacity is limited to fewer than 2^32 bytes and available host memory.
Backing data is dense: constructing a large memory still allocates its capacity.

The registered contract IDs are `memory.container.{ff,sram,rom,ideal-reference}.{8,16,32}`.
Their construction parameters are `depth_words`, `read_delay_ticks`,
`initial_value` (`zero`, `ones`, `unknown`), and `record_history` (`0` or `1`).
Catalog/profile construction records these parameters and fidelity selections.
The direct C++ example above does not request catalog selection.

For a structural array, use the same runtime depth parameter:

```cpp
#include "modules/memory/FFMemory.hpp"

auto array = Component::create<FFMemory<32>>(
    "WORDS", MemoryConfig{.depth_words = 32});
auto single = Component::create<FFMemory<32>>(
    "ONE_WORD", MemoryConfig{.depth_words = 1});
```

Width remains a template parameter; word count is not. The default depth is
still four. A one-word array has no word-selection decoder or read mux, but
retains address validation, byte write enables, and read-enable gating from
the common array port. It does not yet have `Register32`'s simpler interface.

`FFMemory<W>` explicitly selects the FF architecture; callers need not supply a
behavioral model or a cell type. The old `MemoryArray<W, FlipFlopCell>` and
`MemoryWord<W, FlipFlopCell>` C++ spellings still work. The other generic arrays
retain their existing `MemoryCellTraits` wiring. Their interfaces are digital
representatives, not a required universal cell interface for future devices.
The shared port/configuration lives in `MemoryArrayInterface.hpp`.

| Structural array | Cells constructed | Matching scalable implementation |
| --- | --- | --- |
| `FFMemory<W>` (also `MemoryArray<W, FlipFlopCell>`) | `FlipFlopCell` (`D`, `WE`, `CLK`, `RST`, `Q`) | `LegacyMemoryContainer<W, FlipFlopArrayModel>` |
| `MemoryArray<W, GatedDLatch>` | `GatedDLatch` (`D`, `EN`, `Q`), cell reset tied low | `LegacyMemoryContainer<W, SRAMModel>` |
| `MemoryArray<W, ConstantValue<1>>` | Fixed zero constants | `LegacyMemoryContainer<W, ROMModel>` |

These are the existing digital storage implementations, not newly introduced
SRAM/ROM transistor cells. Behavioral models remain separate and unchanged;
the catalog explicitly pairs each model with its structural cell type. The
FF rename preserves pin contracts and persisted selection identifiers.

### Structural hierarchy

The FF device exposes four structural blocks, following the grouped ALU design:

```text
FFMemory<Width>
├── ADDRESS_DECODE : MemoryAddressDecoder
├── ACCESS_CONTROL : MemoryAccessControl
├── STORAGE : FFStorageBank<Width>
│   ├── WORD_0 : FFWord<Width>
│   │   ├── CELL_0 : FlipFlopCell
│   │   └── ... CELL_(Width - 1)
│   └── ... WORD_(depth_words - 1)
└── READ_SELECT : MemoryReadSelector<Width>
```

| Block | Responsibility | Structural | Behavioral |
| --- | --- | --- | --- |
| `MemoryAddressDecoder` | Full 32-bit range check, valid word selection, mux address bits | Yes | No |
| `MemoryAccessControl` | Read permission, per-word write permission, range/ROM-write faults | Yes | No |
| `FFMemory<W>` | Device address/read/write contract; owns all four blocks | Yes | Yes |
| `FFStorageBank<W>` | Requested number of FF words, shared data/mask/clock/reset wiring | Yes | Yes |
| `FFWord<W>` | Exactly W FF cells, byte write enables, data splitting and Q packing | Yes | Yes |
| `FlipFlopCell` | One write-enabled bit, rising-edge capture, asynchronous clear | Yes | Yes |
| `MemoryReadSelector<W>` | Word mux tree, zero padding for missing leaves, read-output gating | Yes | No |

These are real component boundaries, not visual folders. `FFStorageBank` is
a selectable group of parallel FF words, not a bank controller. The existing SRAM/ROM representatives still use `MemoryStorageBank<W,
Cell>` and `MemoryWord<W, Cell>`; neither has a new behavioral implementation.

The FF word interface is `WRITE_DATA<W>`, `WRITE_MASK<W/8>`, `WE`, `CLK`, `RST`,
and `Q<W>`. Reset asynchronously clears every bit, independent of write enables.
Otherwise, a known rising edge captures only enabled byte lanes; disabled lanes
hold. Changing data while the clock remains high does not write again. The
behavioral word implements the same operation with a compact bit vector and a
one-tick output delay. Its equivalence contract is known-binary inputs at settled
checkpoints, not identical internal gate traces or all ambiguous-control cases.
Initial contents are unknown until established by reset or writes.

SRAM retains state across reset; ROM ignores reset and its words expose only Q.

Input splitters retain their original one-tick delay. Word output packing and
read-selector unpacking use zero-delay bus wiring; the extra hierarchy does not
add a pipeline stage or a memory cycle. A one-word array still omits all word
decoder gates and read-selection muxes. Gate count is not the visual hierarchy
count: grouping adds boundary pins and some bus plumbing, not fewer cells.

The FF rename preserves root names, device contract/implementation IDs, and
`STORAGE.WORD_n.CELL_b` instance paths. Both native and WASM visualizers display
`FFMemory8`, `FFWord<8>`, and `FFStorageBank<8,4>`, but use the existing
`FFArray8`, `MemoryWord<FFArray,8>`, and `MemoryStorageBank<FFArray,8,4>` layout
keys. Saved placements therefore remain usable. Cells, words, and banks are built
through their registered families, so exact-instance fidelity overrides work.
An override below a behavioral word/bank/device is inactive because those children
are not constructed; restoring structural fidelity makes those paths available.

The earlier grouping changed flat paths: old per-cell overrides at
`WORD_n_BIT_b` paths do not identify the new instances. Existing saved entries
are retained, and fresh grouped-layout defaults are generated. Block layout
types include width/depth/cell kind where relevant, preventing different
geometries from sharing incompatible internal layouts. Legacy `Register32`,
`Memory4x32`, and `Memory32x32` are not retired or rewired by this regrouping.

Python uses the same catalog:

```python
memory = circuit_backend.create_memory_array(
    "SRAM", "sram", data_width=32, depth_words=4096,
    read_delay_ticks=3, record_history=True, fidelity="behavioral")
circuit_backend.load_memory_bytes(memory, 0, [0x78, 0x56, 0x34, 0x12])
configuration = circuit_backend.memory_configuration(memory)
snapshot = circuit_backend.memory_snapshot(memory, time=0, address=0, count=16)

# FF-specific factories default to structural, small, inspectable circuits.
word = circuit_backend.create_ff_word("WORD", data_width=16)
bank = circuit_backend.create_ff_storage_bank("BANK", data_width=16, depth_words=3)
ff = circuit_backend.create_ff_memory("FF", data_width=32, depth_words=4)
large_ff = circuit_backend.create_ff_memory(
    "LARGE_FF", data_width=32, depth_words=4096, fidelity="behavioral")
```

Image loading on a container is a setup operation and is rejected after its
first evaluation. Array images use byte addresses and little-endian word order.
Legacy `Memory64Kx32` debug/setup APIs remain available with their existing names.

### Fidelity selection

`circuit::families::FFMemory<W>()`, `circuit::families::FFStorageBank<W>()`, and `circuit::families::FFWord<W>()` are
registered families for widths 8, 16, and 32. Use `builder.add(family, name)`
inside a catalog/profile construction to allow selection, rather than directly
constructing a structural class. The root device accepts the capacity parameters
above; word width is encoded in `memory.ff.word.W` and words take no parameters.
The bank contract `memory.ff.bank.W` accepts only `depth_words` (default 4).
Both bank implementations retain the existing limit of 4096 stored bits and
one enable/output pair per word. Parameterized contract schemas validate the
exact pin count and widths at the requested depth. A behavioral bank reduces
internal component count; it does not replace the scalable addressable device.
Structural remains the default.

| Selection | What remains visible |
| --- | --- |
| Structural throughout | Device blocks, words, cells, DFFs, latches, and gates |
| Behavioral `FlipFlopCell` children | Same device blocks and words, compact one-bit cells |
| Behavioral `FFWord` children | Same device blocks and word boundaries, compact word state |
| Behavioral `FFStorageBank` | Same four device blocks, with one compact bank and no word/cell children |
| Behavioral `FFMemory` | One device, backed by `LegacyMemoryContainer<W, FlipFlopArrayModel>` |

These choices preserve the same settled FF port behavior, not identical
propagation delays. Exact paths can mix them within one memory; for example,
`MEM.STORAGE.WORD_0` can be behavioral while `MEM.STORAGE.WORD_1` stays
structural. Selecting `MEM.STORAGE` as behavioral hides both words until the
bank is expanded again; their saved choices remain intact. Byte capacity
is always `depth_words * Width / 8`, independent of fidelity.

## Array port

| Pin | Direction | Width | Meaning |
| --- | --- | ---: | --- |
| `WORD_ADDR` | Input | 32 | Word index, **not a byte address** |
| `WRITE_DATA` | Input | DataWidth | Full word, low byte in bits 7:0 |
| `WRITE_MASK` | Input | DataWidth/8 | Bit n enables byte n |
| `WE` | Input | 1 | Write enable |
| `READ_EN` | Input | 1 | Read enable |
| `CLK` | Input | 1 | FF write clock; ignored by SRAM/ROM |
| `RST` | Input | 1 | FF reset; ignored by SRAM/ROM |
| `READ_DATA` | Output | DataWidth | Selected word, or zero while disabled |
| `FAULT` | Output | 1 | Invalid active access / ROM write attempt |

There is one shared address port. There are no ISA size/sign-extension pins or
request/response handshakes inside the array. For a partial store, position the
bytes in `WRITE_DATA` and set their mask bits; the untouched bytes retain state.

| Model | Write condition | Reset | Structural representative |
| --- | --- | --- | --- |
| `FlipFlopArrayModel` | Known rising CLK edge, WE=1 | High clears to zero | Configurable words of `FlipFlopCell` |
| `SRAMModel` | WE=1 opens the selected write path | Retains contents | Configurable words of `GatedDLatch` |
| `ROMModel` | Never; a write attempt faults | Retains contents | Configurable words of fixed zero constants |
| `IdealRAMModel` | Same as FF array | Same as FF array | Reference abstraction of the FF contract |

FF/SRAM initialize unknown by default; ROM initializes zero by default. Explicit
configuration can choose a different initial fill. SRAM is a **digital static
array abstraction**, not a 6T transistor or electrical model. WE, address, mask,
and data must be stable around the intended write interval; changing them with
WE high changes the addressed SRAM contents.

Reads are asynchronous. `read_delay_ticks` applies to scheduled output changes,
including FAULT; it is a propagation delay in simulator ticks, **not CPU cycles**.
Writes update internal storage at the evaluation time. Read-after-write at that
evaluation observes the updated contents. Structural arrays have their
own gate propagation timing, not the behavioral model's configurable delay.

An enabled, known out-of-range read returns zero and faults. Disabled accesses
ignore address. Unknown relevant controls/address/mask report unknown FAULT;
unknown read enable/address produces unknown read data. Ambiguous writes are
suppressed, not redirected to address zero. Written X/Z bits are stored as X.
Unknown FF reset poisons contents; SRAM/ROM ignore reset. Equivalence with the
structural arrays is promised for known-binary controls and settled checkpoints,
not all transient or X-propagation cases.

Structural selection is available for positive depths up to 4096 storage cells
(512 words at width 8, 256 at width 16, or 128 at width 32), delay=1, and the
default fill. This is a simulator construction guard, not a hardware capacity
limit. Non-power-of-two depths allocate only the requested cells; unused address
codes and all out-of-range high bits fault instead of aliasing another word.
Larger capacities retain the compact behavioral implementation. The Python
factory rejects an unsupported explicit structural request. Structural ROM has a zero image only;
custom image loading and storage inspection require the behavioral implementation.

The FF logical type changes from `FFArray8` to `FFMemory8` (and likewise for 16
and 32); SRAM/ROM logical names remain unchanged. Device contract IDs and
persisted `.structural-slice` implementation IDs remain stable. That suffix is
a compatibility identifier, not a separate fixed-depth implementation.
Visualizer layout-key aliases preserve the grouped FF placements. Fidelity
selection changes internal topology and may use a separate layout variant.

## Bus adapter

The adapter is a structural implementation built from gates, `FlipFlopCell`,
and `Register32`. Its register children retain their existing fidelity options;
there is no behavioral implementation of the entire adapter.

```text
request/response client <-> MemoryBusAdapter32 <-> FFMemory<32>
                                     CLK/RST wired to both components
```

| Client pin | Direction at adapter | Meaning |
| --- | --- | --- |
| `REQ_VALID`, `REQ_READY` | Input, output | Request transfers on a rising edge when both are high |
| `REQ_ADDR<32>` | Input | Byte address, aligned to four bytes |
| `REQ_WRITE` | Input | 0=read, 1=write |
| `REQ_WRITE_DATA<32>` | Input | Positioned byte lanes for a write |
| `REQ_WRITE_MASK<4>` | Input | Byte enables for a write |
| `RSP_VALID`, `RSP_READY` | Output, input | Response transfers on a rising edge when both are high |
| `RSP_READ_DATA<32>` | Output | Full read word; zero for writes/misalignment |
| `RSP_FAULT` | Output | Misalignment or attached-array fault |
| `CLK`, `RST` | Input | Shared rising-edge clock and active-high reset |

The client must hold a pending request stable until accepted. There is at most
one outstanding request. The adapter latches the request, so subsequent client
pin changes cannot alter the accepted operation. Responses remain stable under
backpressure; fields are only meaningful while RSP_VALID is high. Read data on
an array fault must be ignored.

The attached FF device may use either implementation or mixed word/cell
fidelity. Existing SRAM/ROM containers still connect through the same digital
port. This does not require future DRAM/flash devices to use that port; their
technology-specific controllers can implement the client-facing protocol.

The adapter is shared interface/controller documentation, not part of the
[FF storage construction in §3.1.1](../site-docs/memory/3.1.1.md). Its name does not
mean that it supports every memory technology or port:

| Attached memory | Direct compatibility |
| --- | --- |
| `FFMemory<32>` | Yes, including structural, behavioral, and mixed child fidelities |
| Existing 32-bit SRAM/ROM array models | Yes, with a clock period long enough for their propagation |
| Different capacities using this same port | Yes; address range checking belongs to the memory |
| 8-bit or 16-bit arrays | No; width/address conversion is needed |
| Legacy `Memory64Kx32` | No; its byte-address/size/sign-extension port is different |
| Future variable-latency caches, DRAM, or flash | Not directly; suitable completion handling and device control are needed |

The client-facing handshake can accommodate waiting for a response, but the
current memory-facing port has no completion handshake. Reusing the protocol
does not mean reusing this exact fixed-timing controller for every device.

The memory-side outputs `WORD_ADDR`, `WRITE_DATA`, `WRITE_MASK`, `WE`, and
`READ_EN` connect to the like-named array inputs. Array `READ_DATA` and `FAULT`
connect to adapter `MEMORY_READ_DATA` and `MEMORY_FAULT`. Wire CLK/RST to both.
The adapter shifts the aligned byte address right by two; alignment failures
suppress both memory enables and return a fault. Capacity checking belongs to
the array.

```text
IDLE --request accepted--> SETUP --next rising edge--> ACCESS
  ^                                                    |
  | response accepted                     next rising edge
  +---------------------- RESPONSE <-------------------+
                               | hold while RSP_READY=0
                               +-----------------------+
```

SETUP lets the latched address/data settle before a level-sensitive SRAM write
opens. ACCESS gives the array a cycle to respond. FF writes commit on the edge
ending ACCESS; SRAM writes during ACCESS. Responses become valid after that
edge and its propagation delay. No request is accepted on the response-consume
edge; this first implementation deliberately has an idle bubble.

The clock period must accommodate adapter propagation, array read delay, and
response-register setup. This adapter has **fixed access timing**; it does not
wait for an array READY signal, detect a too-short clock period, or implement
DRAM commands. A future variable-latency endpoint can keep the client-facing
request/response protocol while replacing this fixed-timing controller.

Reset cancels pending responses and requests that have not accessed memory.
It cannot undo a store already performed. Shared reset also clears an attached
FF array but does not erase SRAM/ROM. Existing RV32I load/store formatting and
stall/flush integration still need a separate CPU-side adapter.

## Storage, history, and visualization

`MemoryStorage` stores a value and known-bit mask: two host bytes per simulated
byte, independent of capacity. History maps exist only for touched bytes.
Reset history uses an ordered fill marker rather than one record per byte;
filling current contents still takes O(capacity) host work.

`record_history=false` disables memory snapshots and write tracing, not the
simulator's separate wire/pin history. Re-enabling recording through the storage
API starts a new time-zero epoch with the retained contents as its baseline;
earlier snapshots are discarded. That uncommon operation takes a capacity-sized
baseline copy. History is for observation, not rewinding and resuming execution.

Native and WASM sessions expose memory capacity, width, touched count, active
word, and recorded contents through `MemoryStateView`. Structural arrays show
their actual pins/cells rather than a fictitious compact backing store. New
container inspector responses cap touched-word lists at 256, mark truncation,
and also include the active word. Python `memory_snapshot` supports address-window
queries capped at 256 words. Complete GUI paging is not yet implemented.
Existing `Memory64Kx32` display behavior remains unchanged.

## Verification

- `MemoryStorageTest`: deterministic randomized byte writes, historical reads,
  reset ordering, unknown bits, range checks, history-off and re-enabled epochs.
- `FFMemoryContainerTest`, `SRAMContainerTest`, `ROMContainerTest`: visualizable
  four-word scenarios, run through their registered implementations.
- `FFWordTest`: visualizable 16-bit word, all byte masks, hold behavior and reset.
- `FFWordContractTest`: 8/16/32-bit structural/behavioral word equivalence,
  behavioral cell selection, unknown startup, and captured unknown data.
- `FFStorageBankTest`: 134 standalone checkpoints for three 16-bit words,
  covering every word-enable combination and byte mask, including broadcast writes.
- `FFStorageBankContractTest`: widths 8/16/32, depths 1/3/7, structural/behavioral
  and mixed-word/cell equivalence, startup/captured unknowns, reset, depth limits,
  and rejection of malformed parameterized pin schemas.
- `FFMemoryFidelityTest`: all five fidelity arrangements at widths 8/16/32 and
  depths 1/3/7; exact word/cell overrides, family construction and large compact
  device selection.
- `MemoryContainerContractTest`: 8/16/32-bit structural/behavioral comparisons
  at depths 1, 2, 3, 4, 7, 8, 16, 32, and 33; direct cell-type construction,
  four-block hierarchy, per-word cell types/counts and byte enables, ROM's
  omitted write circuitry, zero-delay read bus unpacking, absence of
  one-word selection circuitry, read-back of all words, out-of-range reads and
  rejected writes, structural expansion limits, geometry, preloading, byte masks, exact output
  delay, write-trigger differences, reset retention, ignored/unknown inputs.
- `MemoryBusAdapter32Test`: request latching, access phases, response holding,
  alignment and downstream faults.
- `MemoryBusAdapterIntegrationTest`: adapter connected to both structural and
  behavioral FF/SRAM/ROM arrays and mixed FF bank/word/cell selections; stores/loads,
  partial masks, faults, backpressure,
  reset cancellation, and retention.
- `FFMemoryBindingsTest`: Python factory/configuration/image APIs and
  Python/portable inspector agreement; all recorded output transitions for the
  three four-word structural scenarios match `tests/fixtures/memory-array-timing.json`,
  captured before grouping. This checks transient timing as well as settled results.
- `FFMemoryWasmParityTest.mjs`: native/WASM topology, histories,
  checkpoints, declared examples, and inspector agreement, including the new
  FF word, standalone bank, bank-level device selection, and exact mixed-fidelity selections.

```bash
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure -j 4
tools/build-pages.sh
node tests/web/FFMemoryWasmParityTest.mjs \
  build-wasm/pages/simulator/circuitsim_wasm.js \
  build-wasm/pages/simulator/circuitsim_wasm.wasm \
  build/tests/VisualizerSessionProbe
build/tests/MemoryStorageBenchmark
build/tests/FFMemoryBenchmark
```

`MemoryStorageBenchmark` measures 16,384 write/read pairs over a fixed 16 KiB
working set, for 16 KiB, 256 KiB, and 4 MiB capacities. It reports a median of
five measured trials after one warmup, with/without history. These are host
storage timings, not simulated SRAM access times or CPU performance predictions.

`FFMemoryBenchmark` checks the same 32-bit, 16-word workload across fully
structural, behavioral-cell, behavioral-word, behavioral-bank, and behavioral-device selections.
It verifies equal checkpoint outputs and reports component/event counts and
median host build/run timings after one warmup and three measured samples.
These are simulator costs, not physical FF memory latency estimates.

### FF storage-bank follow-up — 2026-09-17

After registering structural and behavioral `FFStorageBank<W>` implementations,
all **211 CTest entries passed**, including the new standalone bank scenario and
width/depth equivalence test. The catalog validates the exact pin schema for the
requested word count; structural remains the default.

The strict Pages build and **13 native/WASM scenario/fidelity comparisons**
passed. Chapter 3.1 now has seven standalone simulations and **263 recorded
checkpoint rows**, including the bank's own three-word, 16-bit, 134-checkpoint
scenario. Chrome on port 8893 verified every row against WASM state, the bank's
whole-component fidelity toggle, responsive layouts, and the static tables
without JavaScript. No backend API requests were made.
The memory visualizer browser suite also verified that collapsing and expanding
`STORAGE` preserves the surrounding blocks and restores saved word/cell choices.

The five-mode FF benchmark also verified identical outputs at all 136 device
checkpoints. Selecting only the behavioral bank leaves the structural decoder,
access control, and read selector in place: the 32-bit, 16-word case contains
2,572 components and processes 241,766 events, versus 17,052 components and
875,022 events when fully structural. Wall-clock measurements from this run
are not reported because builds and other tests were running concurrently.

The earlier architecture-split measurements below predate this bank-level
selection and remain a historical snapshot.

### FF architecture validation — 2026-09-17

All **209 CTest entries passed**, including the existing RV32I programs and
legacy storage tests. The structural output-transition fixtures are unchanged.
Ten native/WASM scenario/fidelity comparisons passed, including `FFWord` and a
device with independently selected word/cell fidelities. The strict Pages build
passed. Chrome on port 8893 passed all five memory scenarios, nested fidelity
switching, camera centering, inspector contents, saved-profile reload, and
collapse/re-expansion with retained child overrides, without backend API traffic.

The serverless bundle includes scenario-level FF layout fallbacks for new
profile combinations. Its profile validation now matches the native server:
unchanged saved child overrides may remain inactive beneath a compact parent;
new or changed overrides must still identify an available component/fidelity.

Measured medians for 136 identical checkpoints over a 32-bit, 16-word FF device:

| Implementation selection | Components | Processed events | Build (ms) | Build + run (ms) |
| --- | ---: | ---: | ---: | ---: |
| Fully structural | 17,052 | 875,022 | 81.91 | 794.20 |
| Behavioral cells | 3,228 | 308,074 | 19.09 | 220.09 |
| Behavioral words | 2,588 | 242,155 | 11.22 | 176.16 |
| Behavioral device | 1 | 1,540 | 0.061 | 1.21 |

These are local shared-host measurements, not a calibrated hardware timing
model. Mixed word/cell selections retain structural address and read circuitry;
whole-device selection removes that expansion as well.

### Earlier validation

Validation after the `MemoryArray` cell-type refactor on 2026-09-13: all **206 CTest
entries passed** across 194 simulation tests and 12 visualizer/documentation
checks. This includes existing RV32I
programs and unchanged legacy storage components. The strict Pages build and
seven native/WASM scenario/fidelity comparisons passed. Chrome on local port
8893 passed all four memory scenarios, including fidelity switching, inspector
contents, reload persistence, and no backend API requests. Regenerated layouts
and memory-family palette entries are included in the static preview.

Before/after SHA-256 comparisons of all seven native memory/adapter topology
exports matched exactly after replacing model parameters with cell types. The
cell-type refactor therefore needs no new layout/profile migration. A standalone
C++ compile check constructed FF, latch, and constant arrays with only
`MemoryArray.hpp` included.

The subsequent structural grouping intentionally changes internal topology,
but preserves the external interface and all 97 recorded output-transition
rows across the FF/SRAM/ROM four-word scenarios. Their immediate root child
counts drop from 122/122/109 to **four blocks each**, while storage still contains
exactly the configured words and cells. This is a readability refactor, not a
claim of reduced gate count or faster simulation. All 206 CTest entries and all
seven native/WASM scenario/fidelity comparisons pass after grouping; the strict
Pages build also passes. Chrome checks on port 8893 pass for all four memory
scenarios, including the three grouped hierarchies, array/bank/word camera
centering between the side panels, fidelity switching, memory inspection,
reload persistence, and no backend API requests. The older full-topology hash equality above refers
only to the earlier cell-type template refactor, not to this hierarchy change.

The earlier shared-storage benchmark on 2026-09-12 produced these medians
(milliseconds); these measurements were not rerun for the structural-array refactor:

| Simulated capacity | History off | History on | Allocated backing data |
| --- | ---: | ---: | ---: |
| 16 KiB | 4.71 | 31.27 | 32 KiB |
| 256 KiB | 2.65 | 36.98 | 512 KiB |
| 4 MiB | 4.37 | 39.79 | 8 MiB |

Each history-enabled run recorded the same 16,384 touched bytes, independent of
capacity; history-disabled runs recorded none. Timing differences between rows
include shared-host noise and must not be interpreted as memory hierarchy
latencies. The legacy `Memory64Kx32Test` process's sampled peak RSS decreased
from 22,988 KiB before the shared-storage refactor to 9,240 KiB afterward. This
is a small process-level check, not a full CPU performance result.

The visualizer scenario keys are `ff-memory-container`, `ff-word`, `ff-storage-bank`, `sram-container`,
`rom-container`, and `memory-bus-adapter32`.

Their generated layout defaults are bundled in `visualizer-v2/layout.json`.
After structural topology changes, run `python3 tools/generate-memory-layouts.py`
with the rebuilt native module, then rebuild Pages. This reuses the native
layout engine and preserves existing placements. The serverless UI check is:

```bash
python3 tests/web/FFMemoryBrowserTest.py http://127.0.0.1:8893/simulator/
```
