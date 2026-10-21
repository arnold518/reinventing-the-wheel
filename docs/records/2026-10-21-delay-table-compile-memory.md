# Bounded compiler memory for tables and Python bindings

Started: 2026-10-21. Status: open.
Code state: checkpoint `8c90c1f`, integrating `alpha` at `0d498c7` in the isolated `rv32im-phase3` checkout.

## Summary

Optional whole-entry table partitions and smaller Python registration units
build within the local caps. Complete native/WASM table snapshots match for
all 808 entries per profile; all 754 legacy entries per profile retain their
values. All 175 old Python API entries are preserved, with five additions.
Optional isolated example generation passes its identity/publication checks;
full real legacy regeneration still reaches the host-headroom guard. Strict
focused packaging passes with 22 current new groups and 135 retained legacy
exports. Integrated build checks are in the [wrap record](2026-10-22-rv32im-milestone-wrap-and-integration.md).

## 2026-10-21 — Design and initial evidence

- The three tracked generated includes each contain 754 entries and about 22,247 lines (0.92–0.97 MB text). None of their values/text is changed by the partitioner.
- The optional CMake path emits eight chunks plus a dispatcher per profile into the build directory. Each entry is copied verbatim; duplicate keys/malformed boundaries are rejected. Dispatchers preserve the existing `provenPartDelaysUnit/Embedded/Desktop` return functions and lazy immutable maps. Python is required only when this optional path is selected.
- Initial parsing/partitioning succeeded: largest initializer payload 116,531 bytes Unit, 122,562 Embedded, 121,837 Desktop. No chunk has yet passed compilation.
- Prepare a complete value snapshot from the preceding native archive before it is replaced, then compare all fields with fresh partitioned native/WASM builds. Round-trip/determinism checks, compiler memory measurements and existing timing/catalog checks remain required. Value identity is distinct from rerunning gate timing analysis.
- Build and test serially, keep the host-memory guardian and compiler cap. Stop only this task's build group if headroom falls below its threshold. The cap failure is recorded rather than bypassed by increasing host pressure.

### Lossless partition checks and preceding value snapshot

`DelayTableChunksTest.py` passes for all three profiles: 754 entries each, verbatim round-trip in original order, repeatable generated files, and rejection of malformed/duplicate entries. Linked a read-only snapshot probe against the preceding native archive before replacement. Its complete deterministic table-value snapshot is 1,690,771 bytes, SHA256 `b54f3f44f1c8e7ef81c502ff64365a5145b03c318423a79703a6244ccc7df02f`. It includes every arc, conditional arc, launch/setup bound, fixed input, evidence string and timing flag. Fresh partitioned native/WASM comparison is pending. The old archive uses native 64-bit size_t with the same underlying native layout as the explicit tick fields; no wasm32 old-object reuse is attempted.

### Compiled wasm32 value identity

The partitioned WASM snapshot is byte-for-byte identical to the saved preceding
native compiled snapshot: 1,690,771 bytes, SHA256
`b54f3f44f1c8e7ef81c502ff64365a5145b03c318423a79703a6244ccc7df02f`.
Artifact `build-rv32im/partitioned-wasm-delay-snapshot.txt`. This covers all 754
entries in each profile and every serialized field. Fresh native comparison,
compiler-memory measurement and old timing checks remain. No tracked table
entry has changed at this point; new compact M forms may add separately proven
entries during their later complete regeneration.

### Fresh native value identity and bounded build failure

Fresh native DelayTableSnapshot matches the preceding complete compiled
snapshot byte for byte: 1,690,771 bytes, SHA256
`b54f3f44f1c8e7ef81c502ff64365a5145b03c318423a79703a6244ccc7df02f`.
Together with the earlier WASM result, all three profiles and every field
retain their compiled values after partitioning. Artifact
`build-rv32im/partitioned-native-delay-snapshot.txt`. Tracked tables are still
unchanged before the new-component regeneration.

All 24 native table chunks and the core library compile under the 768 MiB
address-space cap. The later Python ModulesBinding.cpp compile hits that cap
(`virtual memory exhausted`), so this is not a successful complete native
build. The capped invocation's peak is 751,104 KiB including the binding;
it does not isolate table-compiler memory. Log
`/tmp/circuitsim-native-time-build.log`, resource report
`build-rv32im/native-build-time-rss.txt`. No host OOM occurred. The native
verification tools then build separately under the same cap, including the
updated core tests and timing generator. Binding retry and isolated final
table-compiler memory measurement remain pending.

### New tables and inventory before the final rebuild

Full `--clocked` regeneration yields 808 entries per profile: all 754 old
entries remain byte-identical, plus 54 new M/device defaults and predictor
variants. The three preservation reports are in `build-rv32im/*-table-preservation.json`
and key results are copied in the phase 4–6 record. No-gate-list regeneration
returns 26 entries; the only change is the M core (170,612 gate nodes), with
all preceding entries/reasons retained. It takes 19.13 s / 289,032 KiB RSS.
The final native library/table rebuild is running under the 768 MiB cap; its
resource report isolates this build from the later binding retry.

### Final native table/library compile memory

The final SimulatorLib rebuild succeeds under the 768 MiB address-space cap,
including all regenerated chunks and the updated runtime/core/test sources.
Elapsed 4:32.46, peak 468,744 KiB RSS (~457.76 MiB), exit 0. This isolates the
library build from the failed binding attempt. Resource report
`build-rv32im/native-final-library-rss.txt`; log
`/tmp/circuitsim-native-final-library-build.log`. Final native probes relink
successfully. Full table verification and final WASM refresh remain pending.

### Binding headroom stop and source partition

The 1 GiB binding retry is stopped by the host-memory guardian at its 512 MiB
remaining threshold (exit 75); no host OOM or unrelated process termination.
Log `/tmp/circuitsim-native-binding-retry.log`. Instead of raising pressure,
the Python registrations now compile in six groups (basic, storage, pipeline,
CPU, arithmetic, utility) plus the existing timing/memory coordinator. Private
templates live in `src/bindings/ModuleBindingHelpers.hpp`. The coordinator
calls these groups in their original registration order. Names, constructors,
method bodies, defaults, documentation strings and class policies are copied
verbatim; the original registration function body can be reconstructed byte
for byte (`build-rv32im/binding-split-preservation.json`). The guide and CMake
source list are updated. The split build is running under the original
768 MiB address-space cap.

Saved the preceding module before replacement for a serial runtime API
comparison: SHA256 `ecf2a86f816ec1f49880a44201d05065f0d9e6e9315851e2e5988101ec0f80c9`,
`build-rv32im/binding-before-module/`. That binary predates the time/game
refresh; compare preserved public API, not new feature support or performance.
Fresh binding/runtime/regression checks remain required.

### Bounded binding build and public API preservation

The split binding builds successfully at the original 768 MiB cap: 3:40.87,
peak 538,172 KiB RSS (~525.56 MiB), exit 0. No deployment. Log
`/tmp/circuitsim-native-binding-split-build.log`; resource report
`build-rv32im/native-binding-split-rss.txt`.

Separate Python processes import the saved preceding module and the fresh
isolated module, respectively. All 175 old public names, kinds, class bases,
constructor/class documentation and public member signatures/docs are unchanged.
Exactly two new names appear: `recorded_guest_names` and
`run_recorded_guest_json`. Evidence
`build-rv32im/binding-api-preservation.json`; this checks API shape rather than
full runtime behavior. Final all-target native build and registered regressions
follow.

### Final browser optimizer exceeds current headroom

The final CLI verification/guest/table/gate-list/system-timing/startup WASM
targets build. The main browser module reaches its final O3 link, then the
host-memory guardian stops our build at 512 MiB available (exit 75). No host
OOM occurred and unrelated processes are untouched. The owner requested
continuing within current memory limits.

Added a validated Emscripten-only cache parameter,
CIRCUITSIM_WASM_LINK_OPTIMIZATION (0/1/2/3/s/z), default 3. The Pages script
forwards an environment value of the same name. Existing default final-link
flags remain unchanged; the local retry selects 1. Component fidelity,
compiled gate behavior and timing data are unaffected. Throughput and parity
must be verified on this artifact; no success is claimed before it links.

### Bounded final link and complete 808-entry value parity

The final-link level-1 retry passes in 9.36 s at 303864 KiB peak RSS,
without a guard stop. Default level 3 remains unchanged. The table snapshot
CLI initially traps during table initialization: it had the small default
Emscripten stack, unlike the other verification targets. Giving this probe
the same 8 MiB stack and exception handling as those targets fixes it.
The complete native/WASM table snapshots now match byte for byte:
2171241 bytes, SHA256
b0c61d9e7aa62175cfa56f8654b5a95d89e1ac551e88a358dc767a7825ead98f.
This covers all 808 entries per profile and every serialized table field.

The first local orchestration also selected a native standalone timing
executable as though it were a registered scenario. The WASM runner correctly
rejects that unknown name; restrict this local list to SimulationTestRunner
commands. Existing native timing evidence is separate. Shared JS image and
keyboard validation, gate-list observation and FF provisioning pass before
that harness stop. The corrected full portable scope is running serially.

## 2026-10-22 — Bounded complete example generation

The full single-process example generator reaches the 512 MiB host headroom
stop during legacy examples; no host OOM. Added optional
CIRCUITSIM_EXAMPLES_ISOLATE=1: each requested test group runs the existing
row/state assertions in a fresh Node process. Full builds start from an
empty bundle and regenerate every group. Filtered builds preserve only
explicitly unselected exports. Publication remains atomic after all groups
succeed. The default in-process path is unchanged.

ExampleRowsIsolationTest passes: full and filtered exports (including program
markers and aliases sharing a test) are byte-identical to the in-process
path; injected failures retain the last destination bundle. The complete
real-engine isolated Pages retry is now running. No site/browser pass is
claimed before it completes.

The refreshed Python API audit initially selected the current module twice
because the snapshot script's directory preceded PYTHONPATH. The corrected
launcher asserts each module's real path before comparing: all 175 preceding
names/signatures/documentation are unchanged, with exactly five additions
(component_layout_variant, create_device_component, device_state_json,
recorded_guest_names, run_recorded_guest_json). Two queued small guards were
intended to be stopped to avoid simultaneous starts; both had finished before
the verified stop command found their PIDs, so no signal was sent. Future
checks use a single serial queue.

## 2026-10-22 — Milestone checkpoint

Checkpoint `8c90c1f` contains this work. The owner requested merging it into
`alpha`; integration verification and remaining acceptance limits are recorded
in the [wrap record](2026-10-22-rv32im-milestone-wrap-and-integration.md).
Earlier dated evidence above is retained. Phase 6 is not declared complete.
