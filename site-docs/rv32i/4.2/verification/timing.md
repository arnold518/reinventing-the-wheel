# Clock and Timing Tests

[Verification index](index.md) · [Clock calculation](../timing.md)

## SafeClockTimingTest

Small constructed circuits check same/opposite-edge setup and hold, shortest-path reconvergence, pulse widths, input/output timing constraints, contract validation, missing/unsupported assumptions, and dependency-cycle diagnostics. A structural full adder is also executed dynamically at the computed bound over binary input combinations.

The expected result is either the known bound/status or explicit rejection. In particular, increasing a clock period cannot repair a same-edge hold violation.

**Source:** `tests/SafeClockTimingTest.cpp`.

## MemoryEndpointSafeClockTest

Checks the zero-wait FF endpoint's timing contract across supported widths/configurations, rejects unsupported endpoint contracts, and checks hold constraints. Dynamic four-state requests compare outputs/storage against a model, verify that outputs do not change before their declared delay, and require a witness for every declared dependency.

The endpoint must drain without autonomous delayed work. This validates the **zero-wait FF contract**, not a blanket latency bound for DRAM/SRAM/flash.

**Source:** `tests/MemoryEndpointSafeClockTest.cpp`.

## MemoryStateTimingTest

Uses the request/response storage and control path to test bounded state-aware timing. Binary and unrestricted four-state reachability differ; unknown registered states must not disappear from the latter.

Checks complete state-case coverage, dynamic agreement between the computed clock and a deliberately slow clock, and rejection on enumeration/settling-budget exhaustion or unresolved cycles. State simplification must not conceal a hold failure.

**Source:** `tests/MemoryStateTimingTest.cpp`.

## MemoryFeedbackTimingTest

Checks value-dependent request validation and byte-mask feedback. Known lane/address cases must settle and match a slow-clock execution; replacing a recognized contract with an unsupported one must fail closed.

A deliberate unknown-address fixture demonstrates actual feedback oscillation, then recovery when the address becomes binary. Core-level unknown-lane store tests check stable InvalidRequest signaling, precise trapping, and unchanged memory, guarding against the earlier self-dependent write-selection bug.

**Boundary:** this does not declare every graph cycle harmless. It distinguishes proven settling, a concrete counterexample, and unknown analysis.

**Source:** `tests/MemoryFeedbackTimingTest.cpp`.

## BinaryStartupTimingTest

Checks binary initialization of dense/sparse images, including unknown last words/pages and zero-fill clearing; the sparse check must not materialize a huge image merely to inspect it. Tests endpoint and register capture induction, supported controlling-value feedback, dynamic settled behavior, and rejection of unsafe premises.

A binary-startup proof cannot hide hold violations or be attached to unsupported contracts.

**Source:** `tests/BinaryStartupTimingTest.cpp`.

## RV32IBinaryClockCLITest

For each core, request a binary-startup certificate, require complete state-case coverage and no diagnostics, then run all 22 fixed programs structurally at the returned period. Check predictor policy variants and invalid/unsupported memory assumptions. It also exercises unrestricted single/baseline analysis separately from binary startup.

The returned number is a sufficient bound under the recorded contract, not an experimentally found minimum. The all-program run is a regression of the analysis against concrete executions, not the reason the mathematical bound holds.

**Source:** `tests/RV32IBinaryClockCLITest.py`. Additional report/advanced state checks live in `RV32IExperimentCLITest`.

## Reproduction

```sh
ctest --test-dir build --output-on-failure -j1 \
  -R '^(SafeClockTimingTest|MemoryEndpointSafeClockTest|MemoryStateTimingTest|MemoryFeedbackTimingTest|BinaryStartupTimingTest|RV32IBinaryClockCLITest)$'
```

Do not copy a historical numeric period into a new circuit configuration. Rerun the analyzer, preserve its assumptions and manifest, and separately run the relevant dynamic regressions.
