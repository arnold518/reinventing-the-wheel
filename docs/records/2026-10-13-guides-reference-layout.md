# Organize maintained internal documentation by purpose

Started: 2026-10-13. Status: closed. Code state: `github-pages` at `448e4e3`.

## Summary

Eleven maintained internal documents now live under `docs/guides/` or
`docs/reference/`. `AGENTS.md` defines their purpose, and indexes, links and
the exact-path documentation test point to the new locations.

## 2026-10-13 — Decision

Classify by what a reader uses the page for. `guides/` holds procedures and
guided explanations. `reference/` holds contracts, architecture, and test or
benchmark specifications. Keep the living status, changelog, index, dated
records, and historical archive at their existing paths. Preserve the content
of maintained pages during this move, including version-specific notes; deal
with any stale claims in separate, evidence-based revisions.

## 2026-10-13 — Change and verification

- Moved three task or explanation pages to `docs/guides/` and eight contract,
  architecture or verification pages to `docs/reference/`. Added an index in
  each folder and updated `docs/README.md` and `AGENTS.md` with the placement
  rule. `docs/` now keeps only its index, status and changelog at the root.
- Repaired relative links in moved pages and archive forward links. Updated
  `tests/checks/RV32IReferenceDocumentationTest.py` and the benchmark tool's
  README to use the new paths. Earlier dated records and changelog entries
  retain their original path text as historical evidence.
- Checked 1,024 source Markdown links: none missing. Five relative simulator
  directory links point to generated site output and were excluded from that
  source-file check. The targeted `RV32IReferenceDocumentationTest` passed
  through `ctest --test-dir build-cleanup -R '^RV32IReferenceDocumentationTest$'
  (1/1). `git diff --check` passed. No C++ build, full test suite, or site
  build was run for this documentation layout change.
