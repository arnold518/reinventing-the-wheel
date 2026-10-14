# Circuit logic simplification, 2026-09-24

This pass preserves component interfaces, instruction timing in cycles, the five
pipeline stages, memory transaction semantics and primitive delays. It does not
add buffers, predictor policies, arithmetic units or timing-analysis exemptions.

- ALU32 result selection expands the old gate mux equations into parallel opcode
  minterms and nine source masks per result bit, followed by balanced OR trees.
  The 32 bit slices remain expandable. Repeated opcode terms are retained so an
  unknown select does not acquire unjustified binary certainty.
- NextPC checks only the two low target bits for fixed RV32I word alignment,
  replacing a general size-controlled data-alignment circuit.
- MemoryRequest prepares request/running/response qualifications before combining
  them with the late TAKE/discard decision. State and handshakes are unchanged.
- ExecutionStatus uses balanced reductions for multi-input qualification and
  event collection; exception priority and stored status are unchanged.

The stopping point deliberately excludes operand-forwarding redesign, direct
arithmetic-result exposure, added equality hardware, fetch queues and registered
redirects. Those involve broader interface, area, or cycle tradeoffs.

## Focused verification

The final native build passes the 44 selected CTest cases. The first run exposed
two stale topology assertions: ALU32 still expected the former ripple-adder
cells, and single-cycle integration omitted the existing `STORE_AUTHORIZED`
constant. The assertions now check the actual prefix hierarchy and exact core
child names; both tests passed on rerun. No functional expectation was relaxed.

Coverage includes arithmetic, the ALU32 hierarchy, individual single-cycle and
pipeline stages, instruction examples, safe-clock analysis, binary-startup CLI
checks, documentation contracts, and memory migration/handshake integration.
This is a focused regression run, not the entire repository test suite.

`RV32ILogicOptimizationTest` keeps independently constructed copies of the old
selector and NextPC circuits as references. It checks 4,096 selector input rows
(all four-state select combinations with four data patterns) and 1,024 NextPC
rows. Checks compare settled values, require no worse combinational depth, and
require fewer combinational nodes:

| Circuit | Old depth | New depth | Old combinational nodes | New nodes |
| --- | ---: | ---: | ---: | ---: |
| ALU32 result selector | 14 | 12 | 2,137 | 611 |
| Single-cycle NextPC | 6 | 6 | 142 | 137 |

These counts and delays describe the simulator's logic graph, not transistor
area or post-synthesis timing. Four-state equivalence concerns settled outputs;
transient event sequences intentionally change with the circuit structure.

## Clock certificates

The unchanged safe-clock calculator, with the existing binary-startup contract,
reports these sufficient full-cycle periods:

| Core | Previous period | New period | Modeled time reduction at equal cycles |
| --- | ---: | ---: | ---: |
| Single-cycle | 124 | 116 | 6.45% |
| Baseline pipeline | 66 | 62 | 6.06% |
| Advanced pipeline | 88 | 78 | 11.36% |

Periods are normalized simulation ticks, not nanoseconds, physical Fmax, or
proven minimum periods. The common structural timing reference and primitive
delays are unchanged. Behavioral implementations use these same structural
certificates; using fewer simulation events does not earn a faster clock.

The limiting captures remain execution-status trap cause for single-cycle,
IF/ID instruction state for baseline, and fetch-request busy state for advanced.
All five advanced predictor configurations receive the same 78-tick period.

## Measured comparison

Four completed campaigns cover all 22 bundled programs, a 128-iteration
dependent loop, a 128-iteration alternating-branch loop, and a 32-iteration
nested loop. Each workload runs on single-cycle, baseline, and all five advanced
predictor policies: **175 configuration/workload cases** in total.

Every case includes an oracle-checked run, a structural-reference comparison,
one host warmup, and three measured repetitions. Each process starts with a cold
predictor. Every native process has a 512 MiB address-space limit. Across the
1,050 recorded runs, maximum reported RSS was 85,320 KiB (about 83.3 MiB).
That figure excludes compilation and WASM validation.

Comparison against the corresponding `20260923-rerun-*` campaigns verifies
identical workloads, cycle counts, retired-instruction counts, CPI,
observations, and final-state fingerprints in all 175 cases. Thus the modeled
execution-time reductions above come from the certified period changes, not
from skipped work or altered cycle accounting.

The following values are **modeled execution ticks** (`cycles × period`), where
smaller is better. The first column sums 22 separately started programs, so it
includes pipeline fill and cold prediction for each program.

| Core / prediction | 22-program total | Dependent ×128 | Alternating ×128 | Nested ×32 |
| --- | ---: | ---: | ---: | ---: |
| Single-cycle | 75,400 | 74,704 | 67,280 | 100,688 |
| Baseline | 94,550 | 127,844 | 115,940 | 147,498 |
| Advanced / not-taken | 71,994 | 70,356 | 75,348 | 107,796 |
| Advanced / taken | 64,194 | 50,700 | 55,692 | 73,164 |
| Advanced / backward-taken | 64,974 | 50,700 | 55,692 | 73,164 |
| Advanced / one-bit | 65,910 | 50,856 | 65,832 | 78,312 |
| Advanced / two-bit | 66,846 | 51,012 | 56,004 | 73,788 |

With two-bit prediction, advanced uses 11.34% less modeled time than single-cycle
on the bundled-program total, and 31.71%, 16.76%, and 26.72% less on the three
longer workloads. Always-taken wins this particular small panel; that does not
establish a generally superior predictor. Always-not-taken still loses to
single-cycle on the alternating and nested loops. A shorter clock does not
remove branch penalties, data stalls, or pipeline fill.

Artifacts, each with a `COMPLETE` marker and source/binary provenance:

- `build/experiments/20260924-logic-programs22/`
- `build/experiments/20260924-logic-dependent128/`
- `build/experiments/20260924-logic-alternating128/`
- `build/experiments/20260924-logic-nested32/`
- `build/experiments/20260924-logic-comparison.csv` — all 175 before/after rows.

Campaigns use `tools/rv32i-performance/experiment.py` with
`--clock-mode binary-safe --allocation-limit-mib 512 --reference-check --warmups 1 --repetitions 3`.
The workload selection is respectively `--programs 1,2,...,22` (all numbers
explicitly supplied), `--workload dependent --iterations 128`,
`--workload alternating --iterations 128`, and
`--workload nested --iterations 32`. Use a fresh `--output` directory.
Raw logs and summaries also retain host-time measurements; host wall time is
separate from modeled circuit execution time and is not a clock-frequency claim.

## Documentation and visualization

Native bindings and the WebAssembly package were rebuilt. **31 native/WASM parity
cases passed**: nine ALU32/Execute cases and 22 advanced component/program cases
across structural and behavioral implementations. These compare hierarchy,
pins/wires, timestamps, checkpoints, examples or performance metrics, and state.

Changed component interiors have recalculated layouts, with bounds/overlap
checks; explicit handmade positions and complete-core stage order are retained.
Affected ALU32/RV32I documentation tables were regenerated from the new WASM
package, retaining unrelated verified tables. The strict MkDocs build and
packaged memory-layout validation passed. The generated preview is at
`build-wasm/site/`.

The stopping point is a low architectural-tradeoff improvement, not a proof of
physical optimality. Parallel selection changes fanout and routing; this
simulator's abstract primitive-delay model does not price their electrical
effects. No physical area, power, or maximum-frequency improvement is claimed.
