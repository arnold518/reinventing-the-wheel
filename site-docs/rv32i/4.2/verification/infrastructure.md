# Benchmark Infrastructure Tests

[Verification index](index.md)

## BenchmarkModelTest

**Input:** common workload/target/metric/run records, the native catalogue, and prepared fixed/generated cases.

**Checks:** schema validation and serialization, preservation of simulated ticks and host seconds, rejection of duplicate metrics, 37 unique catalogue entries, preparation of fixed program 20, and the presence of the independent expected result for scan.

**Boundary:** metadata validation does not execute every catalogue entry or establish CPU correctness.

**Source:** `tests/cpp/BenchmarkModelTest.cpp`; production `include/benchmark/Benchmark.hpp`, `src/benchmark/Benchmark.cpp`, `src/analysis/RV32IWorkloads.cpp`.

## BenchmarkCampaignCLITest

**Input:** a single-core fixed-program smoke campaign, an ELF-directory campaign over checked-in fixtures, and a backing-storage campaign using `MemoryStorageBenchmark`, each in a temporary output directory.

**Checks:** successful frontend execution; COMPLETE and summary.csv exist; extracted common records have schema 1 and completed status; domain/tag metadata distinguish RV32I, external images, and memory.

**Boundary:** this is a frontend/artifact smoke test, not execution of all four memory benchmark executables or all external source workloads. Full RV32I provenance is richer than the current memory campaign provenance.

**Source:** `tests/checks/BenchmarkCampaignCLITest.py`, `tools/benchmarks/run.py`.

## FullEmbenchCampaignTest

**Input:** cycle estimates for five fixed programs, the 19-workload ×
seven-configuration full-campaign plan, and frozen campaigns made from the
checked-in tiny ELF smoke image. The middle-profile fixture uses that same
small image under three names.

**Checks:** baseline cycle estimates against actual short runs; full and middle
matrix budgets; successful frozen workers and `COMPLETE` markers; structural
reference phases for the nine-case *fixture* matrix; rejection of changed
snapshot source; and failure bookkeeping that does not claim completion.

**Boundary:** the test never executes the real 19 Embench workloads or the
three reduced-work algorithms. [§ 4.7](../../experiments.md#full-embench-conditional-clock-only-projection)
labels the full-suite clock-only projection; the reduced-work suite has no
current-source result. Neither follows from this CTest pass.

**Source:** `tests/checks/FullEmbenchCampaignTest.py`,
`tools/benchmarks/run-full-embench-background.py`.

## BackgroundCampaignResumeTest

**Input:** a synthetic interrupted campaign with one completed case, one
partial case and one pending case.

**Checks:** recovery preserves the completed result, archives partial files,
reschedules unfinished jobs with a revised timeout, and rejects a completed
case missing its `COMPLETE` marker or required reference phase.

**Boundary:** this tests recovery bookkeeping; it does not execute an ELF or
prove a resumed real campaign has passed.

**Source:** `tests/checks/BackgroundCampaignResumeTest.py`,
`tools/benchmarks/resume-background.py`.

## RV32IExternalBenchmarkRunnerSmoke

**Input:** the checked-in self-checking ELF through the compatibility
`rv32i_benchmark_elf` command, selecting both single-cycle and advanced cores.

**Checks:** the behavioral runner loads the image, executes it to its retired
`tohost` verdict and returns success for both selected cores.

**Boundary:** this is one short external-image smoke test, with no structural
reference comparison or Embench workload.

**Source:** `tests/CMakeLists.txt`,
`tools/rv32i-benchmarks/RV32IBenchmarkRunner.cpp`.

## RV32IExperimentTest

**Input:** an absolute experiment clock, tiny timing circuits, and the shared default workload on all three cores.

**Checks:** edge/falling/end times; invalid odd/zero periods and overflow rejection; unknown sequential modules fail closed; DFF-to-gate-to-DFF timing; real combinational-loop detection; independent Rewire bit mappings do not create false loops; both asynchronous register-file read paths remain visible.

It also checks the common distinction between 51 attempts, 50 successful retirements, and one terminal attempt; core-specific cycle counts; invariance of cycles/event counts when only an already-safe scheduling period is lengthened; and event-budget rejection.

**Boundary:** the cycle goldens apply to that fixture/configuration, not all programs. A compact behavioral core is not accepted as a gate-level critical-path reference.

**Source:** `tests/cpp/RV32IExperimentTest.cpp`.

## RV32IExperimentCLITest

**Input:** clock reports, fixed programs, three legacy generated loops, explicit measurement windows, predictor choices, and FF/SRAM/DRAM memory on the advanced core.

**Checks:** valid/unsupported clock reports, model contract evidence, all fixed-program completions on the three core kinds, deterministic predictor observations, ordinary and delayed-memory oracle checks, legacy-loop completion, and retirement-window counts. Delayed instruction/data endpoints must produce response-wait observations. An advanced SRAM clock certificate must match its expected bound; structural program-20 runs with FF, SRAM and DRAM at `--period auto` must encounter only handshake states included in the timing analysis and report zero setup violations.

**Boundary:** the SRAM case validates the advanced wait path, while the state-coverage regression checks FF, SRAM and DRAM. This CTest does not dynamically cover every memory technology and predictor combination. Delayed memory remains unsupported on single-cycle/baseline cores; see the [certificate scope](../timing.md#core-and-memory-scope).

**Source:** `tests/checks/RV32IExperimentCLITest.py`. The [timing test page](timing.md) distinguishes safe certificates from candidate analysis.

## Documentation tests

`RV32IReferenceDocumentationTest` compares published encodings, answer sheets, assembly, initial state, and fixed-program expectations with compiled probes. `RV32IBenchmarkDocumentationTest` checks this chapter's catalogue coverage, page pattern, source references, navigation, and local links. Existing building-block/core documentation tests check their source examples and embedded scenarios after section renumbering.

These tests guard against documentation drift; they do not replace simulation. The browser test additionally checks rendered tables and visualizer embeds, which a Markdown-only test cannot establish.
