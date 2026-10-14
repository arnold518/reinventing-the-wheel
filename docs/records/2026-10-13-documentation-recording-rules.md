# Documentation recording rules

Started: 2026-10-13. Status: closed. Code state: `github-pages` at `34abf09454e4edb6f2dd2be79664797ec739e452` before this change.

## Summary

The repository now has one agent rule set and an automatic workflow for
status, changelog and dated topic records. The former agent-file roadmap is
preserved as historical context. Older root-level docs remain to be reviewed.

## 2026-10-13 — Investigation and decision

`AGENTS.md` and `CLAUDE.md` each contained 404 lines and differed only in
their title. The active architecture and build rules ended before a historical
three-year CPU/GPU roadmap. `docs/README.md` already separated current notes
from some older ones, while `site-docs/` was the public MkDocs source. The
working tree was clean at the start of this change.

Adopted the `quant-ai-v2` documentation pattern: one living status, a concise
change history and one dated record per substantive topic. Agents update the
appropriate files during work without a separate prompt. The public site
continues to describe implemented component behavior; historical evidence
stays in records and the archive.

## 2026-10-13 — Changes and verification

Moved the historical roadmap to `docs/archive/`, made `CLAUDE.md` refer to
`AGENTS.md`, and added the status, changelog and record rules and index links.
Documentation checks: pending.

Limits: this change does not migrate every older root-level document or
validate every feature listed in the former agent instructions.

## 2026-10-13 — Documentation checks

`git diff --check` passed. All 54 relative Markdown links in the edited and
new documents resolved. A direct comparison confirmed the archived roadmap
preserved the former `AGENTS.md` roadmap verbatim after its new preface. The
new record's date, status and `STATUS.md` listing agreed; five new Markdown
files had no trailing whitespace. No C++ tests or site build were run because
source and `site-docs/` were unchanged.
