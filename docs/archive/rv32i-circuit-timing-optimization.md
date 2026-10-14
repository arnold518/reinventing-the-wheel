# Circuit timing optimization, 2026-09-23

This changes hardware structure, not primitive delays or the clock calculator's proof rules. Previous binary-startup bounds were single 176, baseline 112, advanced 212 simulator ticks. These are sufficient model bounds, not physical frequencies or minimum working periods.

## Implemented

- `Adder32` and `AddSub32` share a gate-built five-level parallel-prefix carry network. HalfAdder cells produce bit propagate/generate; expandable CarryCombine groups compose them. The old ripple structures remain available through the optional constructor argument `false`. Behavioral AddSub32 retains the four-state full-adder recurrence as an independent reference.
- ZeroDetect32, instruction-pattern matching, and pipeline control reductions are balanced. All cores share the arithmetic improvement.
- Branch recovery compares the saved predicted address against both fallthrough and target in parallel, then selects the mismatch bit. It still compares complete addresses, including JALR and target-equals-fallthrough. Execute exposes the candidate and decision separately; predictor training and precise older-fault priorities are unchanged.
- Advanced fetch issue depends on ENABLE and registered terminal state, not late instruction-terminal calculations. Existing request state holds a stalled request; admission still depends on the coordinator. Flush consumes an already-available wrong-path response on the same edge, avoiding an extra redirect bubble. A halt/trap drains responses without admitting instructions. IMEM must be side-effect-free instruction storage; this is not permission to speculate reads of arbitrary MMIO.
- Single/baseline store operation intent no longer depends on the store's response fault. Earlier faults and alignment still suppress issue/write authorization; the endpoint rejects invalid writes. The old unknown-byte-lane oscillation is now a regression requiring a settled invalid request and precise trap.

## Deliberately retained

The ALU remains shared for arithmetic, addresses, and comparisons. No separate operand-equality bypass, combined forwarding/operand mux, extra fetch FIFO, new pipeline stage, cache, or ISA change is introduced. Those are separate optional designs, not required to remove the measured serial carry/recovery bottlenecks. Keeping them unchanged also retains their current four-state contracts.

## Validation scope

Run after implementation, serially: prefix versus ripple binary/four-state equivalence and depth comparison; existing arithmetic/control/prediction tests; core and delayed-memory integration; documentation examples; calculator certificates and all 22 structural program cases at returned clocks, with all predictor policies on the branch loop. Keep before/after measurements separate from historical experiment artifacts.

The recalculated binary-startup sufficient periods are **124 / 66 / 88** ticks for single / baseline / advanced respectively (previously 176 / 112 / 212). The 22-program structural certificate regression and all five predictor-policy branch-loop checks pass at these returned periods. These bounds still are not claims of minimum periods or physical timing.

The isolated combinational analyzer reports Adder32 **66 → 20** ticks and AddSub32 **68 → 22** ticks. Each prefix implementation matched the retained ripple implementation on 3,200 binary/four-state rows (6,400 total). Program 20 with not-taken prediction remains 85 advanced-core cycles; the fetch optimization must not add a redirect-drain bubble. A cycle-golden regression caught and prevented precisely that extra bubble during development.

Same-edge wrong-path response draining also saves a cycle for program 20 under taken/backward-taken (58 → 57), one-bit (60 → 59), and two-bit (62 → 61). Branch-miss counts remain 1, 2, and 3 respectively; the instruction oracle and final state are unchanged. This is a front-end timing change, not improved predictor accuracy.

Final program-20 artifacts: `build/experiments/20260923-prefix-final/` (COMPLETE).
All seven configurations passed structural/behavioral comparison and oracle
validation. This is a focused regression, with one measured repetition and no
warmup, not a new comprehensive host-performance campaign. Peak RSS across its
21 runs was 91,500 KiB; every native run used a 512 MiB address-space cap.

| Core / predictor | Cycles | Period | Modeled ticks |
| --- | ---: | ---: | ---: |
| Single | 51 | 124 | 6,324 |
| Baseline | 138 | 66 | 9,108 |
| Advanced / not-taken | 85 | 88 | 7,480 |
| Advanced / taken or backward-taken | 57 | 88 | 5,016 |
| Advanced / one-bit | 59 | 88 | 5,192 |
| Advanced / two-bit | 61 | 88 | 5,368 |

Validation: 44 distinct focused native tests passed after the fixes (including
documentation and layout checks). The final hierarchy-order fix was followed
by another 22-program certified-clock/predictor-policy regression. All 22
native/WASM structural and behavioral parity cases passed, comparing hierarchy,
pins, wires, timestamps, checkpoints, counters and state. The full repository
test suite was not run.

The native module and WebAssembly package were rebuilt. Affected ALU32/RV32I
embedded tables were regenerated from the rebuilt WASM simulator while keeping
unrelated verified examples. The strict MkDocs build passed and the generated
site includes the current simulator and layouts. Intentional complete-core
stage order and explicit manual placements are retained; changed gate-level
control/recovery/arithmetic interiors have refreshed placements.
