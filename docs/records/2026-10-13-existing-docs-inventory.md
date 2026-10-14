# Existing internal documentation inventory

Started: 2026-10-13. Status: closed. Code state: `github-pages` at `0834842` before this review, with documentation workflow files already uncommitted.

## Summary

Current developer references remain at the root of `docs/`. Fourteen completed
reports, superseded guides and old plans have moved to the indexed archive;
links and the strict public site build were checked.

## 2026-10-13 — Inventory and classification

The previous documentation-policy change archived only the former agent-file
roadmap. A later local commit and an open memory-clock record appeared in the
same worktree before this review; preserve both.

Keep current developer references at the root: the component checklist,
automatic-layout refresh procedure, benchmark system, timing analyzer, RV32I
memory integration and single-cycle notes, test organization, component
profiles and visualizer profile explorer. Preserve the single-cycle supporting
notes at their current path because the documentation test reads it directly.

Archive superseded API guides, historical roadmaps and routing proposals,
completed optimization/benchmark reports, layout and WebAssembly implementation
reports, and the RV32I cleanup history. Update incoming links and relative
links inside moved documents. No content claims in those snapshots are being
revalidated by this relocation.

## 2026-10-13 — Organization and verification

Moved 14 root-level documents into `docs/archive/`, leaving ten current
developer reference pages at the root. Added `docs/archive/README.md` with a
complete index of 61 historical documents and updated `docs/README.md` to
group the current references by purpose. Repaired incoming and moved-document
links, including the public compiled-benchmark page's report path. Three
already-broken links to a retired `Register.cpp` in the historical hardware
audit now identify that path as retired text.

The strict MkDocs build completed in 70.52 seconds using the existing example
export. A repository-wide path check found 1,010 local Markdown document links
and no missing targets. The archive index matched every archived Markdown
file, and `git diff --check` passed. C++ tests were not run because this change
only moved or edited documentation. The archived documents were not
revalidated against today's implementation.
