# Computer System compact availability correction

Started: 2026-10-29. Status: closed. Code state: `alpha` at `5549ee0` plus the
existing uncommitted working tree.

## Summary

Follow-up to the [chapter completeness review](2026-10-29-computer-system-documentation-review.md).
A final whole-catalog check finds four component groups whose compact forms
are added after their module registration. Correct their public availability
and timing explanations. The compact equivalence check, final strict site
build and all 22 live chapter routes pass. The earlier review's explicit
implementation/acceptance gaps still apply, except for the compact
availability assessment corrected here.

## 2026-10-29 — Catalog evidence and correction

This replaces the earlier review's assessment that every separate M-stage
component and arithmetic step is structural-only. Module-local factories
alone do not describe final catalog availability:

- `BuiltinComponentCatalog.cpp` calls `registerGateListCompacts` after
  registering the normal modules. `GateListCompacts.generated.inc` includes
  `arithmetic.multiply-step.{4,8,16,32}`,
  `arithmetic.divide-step.{4,8,16,32}`, `rv32im.decode-control` and
  `rv32im.pipeline.coordinator`.
- These combinational parts have a generic compact leaf that evaluates
  their exact gate list, selected by explicit path/required fidelity rather
  than an ordinary broad compact preference. Decode/execute stage roots and
  standalone multiplier/divider roots remain structural.
- Correct the arithmetic and pipeline availability tables, implementation
  notes, source links and equivalence-test scope. The section index also
  removes an unsupported smallest-circuit claim and describes decode's shared
  blueprint accurately.
- There are no stored `.gate-list` table entries for these particular
  contracts in the current files. `gateListDelays` derives their timing at
  first construction. Numeric path publication therefore needs a concrete
  width/configuration export and verification, rather than an invented
  stored marker. No generated inventory or timing value was edited.
- `GateListCompactEquivalenceTest` passes: 36 default/recorded parameter
  configurations, 4,032 vectors (binary, dense four-state, single unknown).
  This is a fresh bounded native gate-value comparison, not a new numerical
  timing export or a full native/WASM suite.

## 2026-10-29 — Final site verification

- Final strict preview build passes in 62.71 s under the same 768 MiB
  address-space cap, after the arithmetic, pipeline and section-index
  corrections. Output remains
  `build-milestone/chapter5-doc-review-20261029/site`.
- All 22 chapter pages on port 8893 return 200. The arithmetic and pipeline
  pages show the corrected explicit compact availability and link their
  actual equivalence evidence. Generated example and timing markers are
  rendered; no new scenario or timing-table marker was needed for this
  correction.
- All 70 source links resolve to local repository paths; chapter code fences,
  whitespace and `git diff --check` pass. The earlier C++/C compilation,
  schema checks and timing-documentation checks are unaffected by these
  prose/availability corrections. No browser/WASM suite or numeric runtime
  delay export was run.
