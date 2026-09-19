# Memory transaction endpoints

Implement a versioned, aligned, single-outstanding request/response port for
8/16/32-bit words. READ/WRITE/PROGRAM/ERASE_BLOCK retain distinct semantics.
Native FF/SRAM/DRAM/ROM/NOR controllers and a transaction-to-transaction relay
share the port; controllers own no device contents. A compact MemoryContainer
is the behavioral equivalent of a controller plus device, not of raw device
pins. Preserve old array contracts as LegacyMemoryContainer.

Build shared request/response registers and handshake control structurally;
make timed native sequencing an explicit terminal digital control primitive.
Use existing structural devices as the lower-level reference. Register and
test both complete endpoint fidelities. Keep technology state in separate
behavior models. Do not silently make flash writable like RAM or make reset
erase nonvolatile contents.

Add bounded allocation/paged backing and configurable history for large
containers. Interface reset cancels delivery, not already issued operations;
drain native work before reuse. Refresh runs while the controller clock runs,
including response backpressure. Use finite-time tests, no full-suite builds.

Documentation: 3.1.6 scalable container under memory devices; 3.2 transaction
protocol and shared frontend; 3.3 controllers. Preserve IO / behavior / C++ example / implementation /
all example checkpoints / embeds / source and tests pattern. Native matrix
layouts remain unchanged. New endpoint diagrams show frontend, sequencer and
device separately. Verify native, WASM and browser paths with sequential builds.

## Implemented boundary

- `MemoryTransactionPort`: actual phase FFs, gates, request and response FF registers.
- `MemoryController`: that frontend plus an explicitly terminal timed native sequencer; no contents.
- `MemoryEndpoint`: controller and native FF/SRAM/DRAM/ROM/NOR device assembly.
- `MemoryContainer<W,Model>`: compact controller-plus-device equivalent. Technology models remain separate.
- Container controller: transaction relay, including an explicit zero-delay clock Rewire across the hierarchy boundary; no extra device timing.
- Old array contracts retained under `LegacyMemoryContainer` / `create_memory_array`; no RV32I rewiring.

The protocol is version 1, aligned full-word addressing with byte write enables,
one outstanding transaction, explicit READ/WRITE/PROGRAM/ERASE_BLOCK and held responses.
Interface reset discards delivery, drains launched work, and never implies rollback.

## Focused native evidence (2026-09-18)

14/14 targeted CTests passed: frontend, standalone controller, five device
endpoints, container relay, standalone scalable container, expanded contract
and timing tests, backing storage, legacy binding compatibility, and
documentation/binding/layout integration (seven focused Python checks).
Contract comparisons cover five technologies at 8/16/32 bits plus multiword rows
and non-power-of-two capacity; unknown data/masks and ignored read fields are
checked in addition to ordinary accesses, errors and partial writes. Timing
tests cover reset during NOR programming and refresh during prolonged response
backpressure; stopping the clock causes DRAM retention loss as intended.

The timing rig owns its requested wall-clock position: Simulator's current time
is the last processed event, so using it as an idle-wait cursor was incorrect.

Host benchmark: release native build, one warm-up and median of three measured
build+run trials, content and wire history enabled, 4 words × 8 bits. The
request stream is the same for both representations of each technology.

| Endpoint | Checkpoints | Expanded components | Expanded / compact events | Expanded ms | Compact ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| FF | 254 | 2,744 | 421,016 / 2,529 | 223.807 | 1.198 |
| SRAM | 254 | 2,086 | 407,534 / 2,519 | 217.498 | 1.101 |
| DRAM | 254 | 1,999 | 396,523 / 2,587 | 210.637 | 1.089 |
| ROM | 74 | 1,955 | 115,313 / 730 | 83.621 | 0.399 |
| NOR flash | 308 | 1,999 | 480,408 / 3,105 | 255.468 | 1.278 |

Each compact endpoint has one component. Benchmark peak RSS was 35,576 KiB.
Separate sparse-store trials at 4 KiB, 1 MiB and 256 MiB logical capacities each
allocated 8,320 accounted content bytes after one last-address write. This is
one page plus accounting overhead, not the process RSS and not a hardware area
or latency estimate. Run `build/tests/MemoryEndpointBenchmark` to reproduce.

## Deliberate limits

No caches, coherence, bursts, outstanding IDs, atomics, arbitration, DMA or CPU
adapter migration. Existing CPU/array implementations remain available. Device
models are digital functional/timing abstractions, not calibrated DDR PHYs or
analog/power/endurance models. Flash persistence lasts within an instance.
History budgets fail fast rather than silently truncating; the content budget
does not bound simulator-wide wire history. Native matrix geometry remains the
tested K=1/2/4/8/16 range. Gate-expanded storage retains the 4096-cell safety cap.

## Portable and browser evidence

- 19 native/WASM comparisons passed: nine public scenarios, all five compact
  endpoint alternatives, and all five mixed-fidelity native devices under an
  expanded controller. Topology, profiles, recorded values, checkpoint exports
  and time-scrubbed memory inspection agree. The full endpoint contract and
  timing checks also passed when executed by WASM.
- Nine Chrome scenario checks passed against the local Pages preview on 8893:
  responsive documentation, all table checkpoint/time metadata, sampled pin
  values, visible explorer controls, fidelity switching, rendered contents and
  no backend API requests. The three new pages contain 1,700 verified example
  rows, all exported from the published WASM engine.
- Strict MkDocs build passed. A mobile overflow from Material's hidden long
  page-title topic was fixed by clipping that header title container.
- Native SRAM inspection inside a controller now works in both frontends.
  Transaction containers display snapshot/transaction status instead of a
  misleading live raw-address label. Endpoint/controller floorplans were
  tightened without reflowing existing native device layouts. The new four-row
  SRAM endpoint has its own profile-specific matrix placements; tall SRAM
  floorplans fit their parent bounds. The pre-existing two-row SRAM layouts
  were retained. `SRAMLayoutTest` additionally passed, and the new browser test
  checks all SRAM child bounds after focusing the storage bank.
- Builds used one compilation job and native address-space guards. The measured
  WASM build peak RSS was 377,948 KiB (about 369 MiB); the runs completed without
  OOM. No full test suite or CPU-program batch was run.

Reproduction:

```sh
ctest --test-dir build --output-on-failure -j1 --timeout 120 \
  -R '^(MemoryTransactionPortTest|MemoryControllerTest|MemoryEndpoint.*Test|ScalableMemoryContainerTest|MemoryStorageTest|MemoryContainerBindingsTest)$'
node --max-old-space-size=512 tests/MemoryEndpointWasmParityTest.mjs \
  build-wasm/circuitsim_wasm.js build-wasm/circuitsim_wasm.wasm build/tests/VisualizerSessionProbe
/usr/bin/python3 tests/MemoryEndpointBrowserTest.py http://127.0.0.1:8893/
```
