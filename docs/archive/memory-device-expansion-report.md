# DRAM, ROM, and NOR-flash device implementation

## Scope

The implementation follows [the expansion plan](memory-device-expansion-plan.md).
Existing FF/SRAM devices, CPU memories, and bus adapters are retained. The new
families reach the same cell/row/bank/device level, with technology-specific
pins rather than a common writable-cell fiction.

- `memory.dram.*`: destructive digital charge sampling, sense/restore feedback,
  row buffer, explicit ACT/READ/WRITE/PRECHARGE, round-robin one-row refresh,
  and lazy retention expiry on activation.
- `memory.rom.*`: immutable construction image, constant cells, row/column
  selection, asynchronous active-low chip/output controls.
- `memory.nor-flash.*`: initially erased digital cells, one-way programming,
  block erase, write-enable latch, latched requests, busy/error/read-valid state.
- `memory.matrix.*`: shared full-address row decode, column selection,
  byte-masked write distribution, static column sensing, and output buffering.

Small expanded components are structural by default. Rows, banks, and complete
devices have tested behavioral alternatives; ROM cells do too. Charge and
sensing primitives are explicit digital abstractions, not transistor-level
simulation. No DDR/SPI compatibility, NAND behavior, calibrated hardware timing,
power/endurance model, or CPU/cache integration is claimed.

W is 8/16/32, N and K are runtime configuration, and banks use N/K rows of W*K
cells. Expanded devices and scalar-pin banks are capped at 4096 cells; compact
devices at 2^20 words. Compact devices expose read-only memory history, with
`record_history=0` available to avoid growing backing-store traces. ROM uses
`image_hex`; debug loading does not bypass DRAM writes or flash programming.

## Documentation and visualization

- [3.1](../../site-docs/memory/3.1/index.md): universal roles, geometry, fidelity, and
  controller/bus boundary.
- [3.1.0](../../site-docs/memory/3.1.0.md): helper references and reuse boundaries.
- [3.1.1](../../site-docs/memory/3.1.1.md): existing FF reference, renumbered.
- [3.1.2](../../site-docs/memory/3.1.2.md): existing SRAM reference, renumbered.
- [3.1.3](../../site-docs/memory/3.1.3.md): DRAM.
- [3.1.4](../../site-docs/memory/3.1.4.md): ROM.
- [3.1.5](../../site-docs/memory/3.1.5.md): NOR flash.

There are 21 newly documented component scenarios. Tables are generated from
their complete registered checkpoint sequences, not hand-selected examples.
Python factories and the native/WASM visualizers use the same catalog.

The reference pages now follow the existing FF/SRAM/ALU pattern per component:
IO pins; behavior with a scoped, compilable C++ example and unknown/timing rules;
structural implementation and behavioral implementation only where registered;
complete examples with an embed and full-screen link; source links and test coverage.
The focused documentation test verifies every documented pin name, direction,
and width against the native topology, and compiles/executes all 21 C++ examples,
including instantiation of the compositional row/bank examples.

The implementation revision makes unknown idle commands invalidate stale read
validity in DRAM and NOR flash, consistently with reserved binary commands.
In-flight commands are not canceled. New recorded regression rows cover that
case and NOP error preservation. ROM address control rejects storage-only
parameters and its examples cover output disable and unknown controls separately.

`memory_device_layout.py` follows the SRAM matrix convention: numeric cell
order left-to-right, rows top-to-bottom, aligned column circuits below, and
grouped device peripherals. `generate-memory-device-layouts.py` adds missing
new-family defaults without reflowing FF/SRAM or overwriting custom placements.
Its explicit `--reflow` option refreshes only the newly added families. Fresh-bank
initialization seeds row proportions before saving placements; a regression test
and browser parent-containment assertions guard against rows outside the bank.
The shared mux/distributor now use numeric bit lanes and separate word-slot rows,
with controls in a left gutter. Focused geometry tests cover W=8/16/32 and
K=1/2/4/16, including bounds and non-overlap. Semantic palette entries distinguish
cells, rows, sensing, decoding, and control without overriding saved colors.

Behavioral memory-content overlays recognize the new bracketed device names in
both the native server and WASM frontend. A bounded initial window makes erased
flash and untouched unknown DRAM visible, alongside touched-word history.
Command devices show storage snapshots with byte addresses and recorded status;
they do not mislabel external command address pins as latched linear addresses.
Native Python and C++ snapshots are compared at initial and final checkpoints.
Content formatting uses the actual word width (two/four/eight hexadecimal
digits for 8/16/32 bits) instead of unconditionally padding to 32 bits.

## Focused validation

The native checks cover every new component, structural/behavioral and mixed
bank/row fidelity, W=8/16/32, K=1/2/4, invalid parameters/images, compact capacity,
history capability, deterministic masked-write/program oracles, row/block
isolation, command timing, flash request latching, DRAM refresh, and expiry.
Geometry checks cover bounds, non-overlap, numeric ordering and column alignment.
Documentation checks compare all 21 sections with native catalog/export data.

Only new-memory tests are selected; the full test suite and CPU programs are
not rerun. Existing catalog inventory and old-document path expectations are
updated for the new registrations/numbering.

Completed checks:

- 25 focused native CTest entries passed (23 simulation/contract/timing tests,
  one layout/bindings test entry, one documentation test entry).
- 21 new component scenarios export 236 verified checkpoint rows.
- 37 native/WASM comparisons passed, including structural, behavioral, and
  mixed bank/row selections, exported rows, sampled pin histories, and memory
  inspector snapshots. The contract and timing tests also passed in WASM.
- Browser checks cover all 236 rows, actual row/column and bit-lane alignment
  and parent bounds, all three device fidelity toggles, mobile table bounds,
  no-JavaScript tables, three isolated real-document embeds, behavioral content
  overlays (including initial/final values), and no API traffic.
- Strict MkDocs production build passed; the live preview serves the new pages.
- The same focused browser checks also passed against the live preview on 8893.

```bash
cmake --build build --target SimulationTestRunner VisualizerSessionProbe circuit_backend --parallel 1
ctest --test-dir build -j1 --output-on-failure \
  -R '^(Matrix(RowDecoder|ColumnMux|WriteDistributor)|StaticReadColumn|MemoryOutputBuffer|ROM(Cell|Row|AddressControl|StorageBank)?|DRAM(Cell|Row|SenseColumn|StorageBank|CommandControl)?|NORFlash(Cell|Row|StorageBank|CommandControl)?|MemoryDevice(Contract|Timing|Layout|Documentation))Test$'

# After activating Emscripten; keep optimizer workers bounded as well.
EMCC_CORES=1 BINARYEN_CORES=1 cmake --build build-wasm --target circuitsim_wasm --parallel 1
node tests/MemoryDeviceWasmParityTest.mjs \
  build-wasm/circuitsim_wasm.js build-wasm/circuitsim_wasm.wasm build/tests/VisualizerSessionProbe
python tests/MemoryDeviceDocumentationBrowserTest.py http://127.0.0.1:8893/
```

Before the WASM packaging step, preserve the existing `example-rows.json`
bundle outside `pages/simulator` (packaging regenerates that directory). Use
the optional test-name regex and previous-bundle arguments of
`tools/generate-example-rows.mjs` to regenerate only the new component examples,
retaining existing ALU/FF/SRAM/program exports without running their tests:

```bash
node tools/generate-example-rows.mjs \
  build-wasm/circuitsim_wasm.js build-wasm/circuitsim_wasm.wasm site-docs \
  build-wasm/pages/simulator/example-rows.json \
  '^(Matrix|StaticReadColumn|MemoryOutputBuffer|DRAM|ROM|NORFlash)' \
  build-wasm/example-rows.previous.json
```

## Resource precautions

Compilation and optimizer work are single-worker. Native builds were run with
a 2.75-GiB virtual-memory limit; the native focused tests with 1.5 GiB. Browser
contexts are closed between scenarios, and the embedded-page check permits
only one active WASM frame at a time. No full-memory stress workload is used.

For this revision, the native build peak was 327,460 KiB (1m03.61s).
The 25 focused CTest entries completed in 5.86 s with 99,156 KiB peak RSS;
the serial WASM rebuild peaked at 368,680 KiB (2m32.12s). The full browser
pass with one context at a time used about 213,000 KiB peak process RSS.
These are host-process measurements, not a system-wide memory bound or
hardware access latency. Detailed results are in
`build/memory-revision-{build,tests,docs}.log` and
`build-wasm/memory-revision-{build,parity,browser}.log`.
The restored Emscripten toolchain
was moved from RAM-backed `/tmp` to disk-backed user cache, recovering roughly
1.7 GiB of RAM-backed filesystem use. The old temporary toolchain path remains
a symlink so existing build configuration stays valid.
