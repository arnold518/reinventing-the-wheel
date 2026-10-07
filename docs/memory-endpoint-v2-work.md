# Memory endpoint v2 implementation work

Scope: retain endpoint = controller + native device; remove mandatory transaction
latency, preserve native timing, keep caches out of scope. Existing dirty work is
preserved. This file records the implementation and its verification boundaries.

## Contract

- Existing request/response fields; transfers on rising edges.
- Fast devices may accept and respond on the same edge.
- One outstanding request; response consumption may coincide with next acceptance.
- Every accepted operation executes exactly once. Held responses remain stable.
- Reset discards interface state, never clears storage or rolls back launched writes.
- FF uses shared-clock writes and combinational reads. Slow native operations keep
  their timing and refresh behavior.

## Work order

1. Structural fall-through response buffer and revised transaction frontend.
2. FF controller fast path; revised sequencer handshake for other technologies.
3. Behavioral endpoint equivalence, contract/timing tests.
4. CPU requester and memory integration; preserve strict single-cycle timing.
5. Documentation examples, catalogs/bindings/WASM, intuitive controller layouts.
6. Focused serial builds/tests and cycle-throughput measurements (OOM precautions).

User confirmed: migrate all cores, including strict single-cycle and baseline, to
the zero-wait FF endpoint without changing their cycle timing. Slow memory support
remains specific to the transaction-capable pipeline.

## Implemented changes

- `MemoryResponseBuffer<W>` is a structural one-entry fall-through buffer built
  from FF payload registers and muxes. Empty/ready buffers add no cycle. A held
  response can be consumed while its replacement is accepted.
- `MemoryTransactionPort<W>` keeps one pending bit, live backend request pins,
  and START/FINISH handshake signals. The native sequencer captures requests;
  there is no duplicate frontend payload bank or mandatory SETUP cycle.
- FF controllers use structural address/operation checks, shared-clock masked
  writes, combinational reads, and the response buffer. No native sequencer is
  placed in this path. Illegal operations and native faults return status codes.
- SRAM/DRAM/ROM/NOR controllers keep their native sequencing. DRAM refresh is
  inserted even under continuous traffic and continues after completed response
  data has moved into the response buffer.
- `MemoryContainer` remains a scalable **native device**. A structural endpoint
  can expose its controller and use a container internally; a
  `BehavioralMemoryEndpoint` compresses the entire tested assembly.
- `RV32IMemoryRequest` bypasses its request registers when idle and captures only
  incomplete work. Redirected accepted requests are drained, not duplicated.
- All three implemented datapaths expose universal REQ/RSP ports and default to
  65,536-word FF endpoints. The fixed-cycle cores have combinational
  `INSTRUCTION_PORT`/`DATA_PORT` formatters, not request buffers. Their stage-local
  word-addressed pins remain unchanged. The optimized pipeline also supports
  delayed technology endpoints. The isolated instruction answer-sheet system
  and standalone legacy memory tests keep their existing contracts.
- The six-stage single-cycle and baseline pipeline floorplans retain stage order.
  Revised controller/buffer/requester groups have dedicated layouts; native
  storage row/column layouts remain intact.
- Sections 3.1–3.3 and the affected 4.1–4.5 references distinguish native device,
  controller, endpoint, and stage-local ports. C++ snippets are executable test
  inputs, and diagram sources are regenerated with the existing themed renderer.

## Validation and measurements (2026-09-23)

Focused tests only, serial execution. No full gate-level suite or cache work.

- The 96-test native selection initially passed 94 tests; the two failures were
  stale requester geometry and duplicate catalog test ownership. Both were fixed
  and passed separate rechecks, including the complete expected type inventory.
  Behavioral memory/CPU tests all passed. Final focused controller/timing and
  catalog reruns also passed after the portability fix below.
- The native selection includes all 22 single-cycle and all 22 optimized pipeline
  programs, baseline structural/behavioral equivalence, endpoint technology and
  width equivalence, migration, documentation snippets, bindings, and layouts.
- Zero-wait timing regression: 24 consecutive FF reads/writes complete at the
  accepting edge; response hold and consume/replace are checked independently.
  DRAM retention/refresh is checked under continuous traffic and backpressure.
- The 96-test run took 249.68 seconds; maximum resident set size reported by
  `/usr/bin/time -v` was 339,736 KiB. This is peak process RSS, not total host RAM.
- Native benchmark medians (one warmup, three samples; build + simulation +
  history): FF expanded 300.569 ms, fused 3.148 ms; SRAM 70.409/1.414 ms;
  DRAM 102.236/1.419 ms; ROM 20.512/0.482 ms; NOR 75.256/1.602 ms.
  These are host simulation comparisons between fidelities, not physical memory
  latency or before/after speedup measurements. Other host work can affect them.
- A single touched page uses 8,320 content bytes at logical capacities of 4 KiB,
  1 MiB, and 256 MiB, with history disabled. Controller state does not scale with
  capacity. This number excludes container/map/allocator overhead.

## Browser and portability validation

- Native/WebAssembly equality checks passed for 20 memory configurations,
  16 single-cycle stage/core and optimized core configurations, and 20 baseline
  stage/core configurations. They compare exported topology, timing, states,
  checkpoint rows, and inspector data, not only final program results.
- These checks caught unspecified C++ function-argument evaluation order in FF
  controller construction. Creating OK and UNSUPPORTED constants in separate
  statements now gives GCC and Clang the same component ordering. The corrected
  native export matches the WebAssembly export.
- The ten memory-page browser cases passed: table checkpoints, WASM controls,
  structural/behavioral switching, native-device views, and endpoint inspectors.
  SRAM row/column and FF-controller screenshots were inspected. No backend API
  requests or browser script errors occurred in these cases.
- Strict MkDocs builds passed, and the existing user service on port 8893 returned
  HTTP 200 again after example regeneration. During the migration it had been
  waiting on failed rebuilds caused by missing/stale exported examples; no service
  configuration or restart was needed.
- WASM builds use one compiler job. The final catalog build peaked at 431,556 KiB
  RSS; parity processes peaked at 305,344 KiB. Neither is total-system memory.

### Final artifact checks

- Rebuilt WebAssembly after the construction-order fix, then reran all 56 parity
  comparisons successfully. The memory, core, and baseline parity processes used
  peak RSS of 209,964, 322,136, and 262,808 KiB respectively.
- The final serial WASM build took 3:11.97 and peaked at 436,456 KiB RSS. The binary
  SHA-256 is `5ba2984a1521c01ddce25ee40ce87717d08066bb2908fecdeb469a23664abced`.
- Four single-cycle/optimized browser configurations and two baseline browser
  scenarios passed stage bounds/order, terminal state, camera focus, and endpoint
  inspection checks with no backend API traffic. The live single-cycle program 3
  displayed 17 retired instructions, 17 cycles, CPI 1.000. Baseline documentation
  checks covered all nine pages, full tables, light/dark themes, and narrow screens.
- Inspected the actual single-cycle and baseline core screenshots: the stage row
  remains left-to-right, pipeline registers stay between their stages, and control
  logic stays below. Port formatters do not displace the primary stage row.
- The production site includes the new WASM, layout, and example bundle; no commit
  or deployment to GitHub was performed.
- Reran all ten memory-page browser cases against the final artifact: all passed.
  The binary served by port 8893 has the same SHA-256 as both the build output and
  the production site copy. The live preview therefore serves this implementation,
  not a stale simulator build.

## Follow-up: endpoint visualization feature audit and fixes

The preceding parity checks were insufficient: identical snapshots could still
omit active-address highlighting, operation decoding, or instruction context.
The follow-up adds independent feature assertions and repairs those omissions.

- Python and WASM now share `MemoryInspector`. It reconstructs accepted request
  context from pre-edge signal history, including zero-wait completion, held
  responses, consume/replace, and reset. Later request-pin changes cannot move a
  pending request's highlight. Observation never issues a memory transaction.
- Fetch/load/store highlights, operation colors, active-address headers, and
  instruction disassembly work on endpoints and nested native stores. The
  instruction highlight denotes a fetch, not retirement or pipeline occupancy.
- Address paging reaches the whole store; the 256-entry sparse summary is no
  longer a browsing limit. Follow access centers the active row; manual scrolling
  suspends it. Snapshot contents, activity, and selection remain rewindable.
- Fused endpoints expose recorded native phases and technology-specific status,
  including DRAM row-open, flash write-enable, and refresh activity. Internal
  phase changes enter the visual timeline even without external bus changes.
  No controller state, hardware pins, latency, or events were added for display.
- Scalable endpoint contracts remain behavioral by default but can expand their
  controller around compact native storage. This does not expand 65,536 words
  into cells. A contract-local default preserves existing profile fingerprints.
  Pre-simulation image provisioning is exposed separately in Python.
- Missing static root layouts for baseline programs 2–22 are filled; all 66 CPU
  program aliases are packaged. Default controller layout fallbacks also support
  expanding only one endpoint under a newly generated profile hash. Existing
  manual layouts remain intact. Device placement reserves pin-stub clearance.

Final follow-up validation (focused, not the full suite):

- 14 native logic/selection/UI-unit tests and six integration/bindings/layout/
  inspector tests passed, including all three CPU migrations and four-state
  memory contracts. The independent trace fixture checks pre-edge sampling and
  proves inspection does not advance simulation or change event counters.
- All three CPU fetch/read/write/page/rewind comparisons and 20 endpoint
  configurations match native/WASM. The endpoint contract, timing, and native
  device contract tests also pass inside WASM.
- Rendered browser tests passed for single-cycle, optimized, and baseline cores:
  active rows and colors, disassembly, address paging, actual scroll-button
  clicks, Follow access, rewind, and one-endpoint expansion.
- All ten existing memory-page browser cases passed against the final artifact,
  including embedded example checkpoints, technology endpoints, native-device
  views, and structural/behavioral switches.
- Documentation examples were regenerated from WASM. Strict MkDocs build and
  the 66-program static-layout packaging check passed. Build, Pages package,
  production-site copy, and the binary served on port 8893 have SHA-256
  `e229049fcad988f7752edc31d71f1b81a2548d5b322449a86e28b2a82cf87955`.
  This supersedes the earlier artifact hash above. No GitHub deployment or
  commit was made.
