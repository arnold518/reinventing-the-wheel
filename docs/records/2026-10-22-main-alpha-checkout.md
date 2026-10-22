# Main alpha checkout consolidation

Started: 2026-10-22. Status: closed. Code state: main checkout on
`alpha` at `88c0a95`, with uncommitted operational/audit documentation.

## Summary

Work now uses alpha directly in the main repository, with no linked Git
worktrees. Recent documentation is restored there; older pending changes
are preserved in a verified stash and external backup. Port 8893 serves
the main directory and matching simulator artifacts. Both former checkout
directories remain as ordinary ignored archives with source/build/evidence
entries preserved. Strict preview and local HTTP/hash checks pass, with no
OOM events. No component implementation or game acceptance status changed.

## 2026-10-22 — Investigation and plan

- The main directory is still on realistic-timing. Recent preview and width
  audit documentation was written in `.worktrees/alpha-integration`, which
  owns alpha. The native/WASM evidence checkout is `.worktrees/rv32im-phase3`.
  Both linked checkouts start at `88c0a95`.
- The main directory has 13 pending files. Nine exactly match files already
  committed on alpha; the older changelog, status and two planning documents
  differ. Preserve all pending bytes and patches externally and in a named
  stash, without replaying obsolete status over the newer alpha documents.
- Preserve alpha's recent uncommitted documentation separately, then apply
  it in the main alpha checkout. Keep branch tips unchanged.
- No active compiler/build process was found in these directories. Only
  the port-8893 MkDocs service uses a linked checkout. Repoint it before
  retiring the worktree registrations. Preserve its 768 MiB limit and zero
  swap, reuse the matching simulator package, and run a guarded strict build.
- Disk headroom is only about 2.1 GiB. Archive the former checkouts using
  same-filesystem renames, not copies of their multi-gigabyte build trees.
  No native/WASM recompilation or browser simulation is needed.

Append the preservation receipts, final branch/worktree/service state and
checks below when the consolidation finishes.

## 2026-10-22 — Preservation and branch change

- Backed up all 13 main-checkout pending files and five alpha documentation
  files, plus staged/unstaged patches, SHA-256 manifests, the previous service
  override and resource guard, to
  `/home/arnold/.local/state/circuitsim/checkout-migrations/20261021T180011Z/`.
- Saved the older realistic-timing work in stash
  `7a6b46955afc5d2dd087ffed2b2f40f9c02fef28`, labeled
  `CircuitSim primary before main-alpha consolidation 20261021T180011Z`.
  Its parent is `0d498c7`. Verified all 13 pending file hashes against both
  stash contents (including the untracked-file parent) and external copies.
- Saved alpha documentation separately in stash
  `b43c1b570b00a7f9a96f4f23dcc69b13d90f9de9`, then detached the old alpha
  checkout, switched the main repository to alpha, and applied that stash
  there. Verified all five restored file hashes before updating this record.
  Both safety stashes remain; the older pre-existing stash was preserved.
- The older main status/planning edits were not applied over newer alpha
  documents. Nine pending audit/tool files already exactly matched alpha's
  committed files; the four differing documentation files remain available
  in the primary stash/backup. Neither source branch tip was rewritten.
- The main HEAD is `88c0a95013e781afafa981ea8af856ea5c073fc2` on `alpha`.
  The `realistic-timing` and `rv32im-phase3` branch references remain.

## 2026-10-22 — Main-directory preview

- Staged the same 27-file matching simulator package in the main
  `build-wasm/pages/simulator`. Compared current frontend/configuration
  sources and package hashes before publication. Preserved the previous
  23-file simulator package by rename under
  `build-wasm/preview-main-alpha-migration/simulator-before`; its file hashes
  still match. Existing native modules, CMake outputs and evidence were not
  overwritten by a build.
- Guarded strict build from the main directory passes: wall time 43.76
  seconds, peak RSS 179,736 KiB, exit 0. It used `mkdocs.preview.yml`, a
  768 MiB address-space cap, 900 MiB initial headroom, 512 MiB stop threshold
  and a 30-second maximum startup wait. No compiler or example generator ran.
- Updated the local `alpha.conf` user-service drop-in so WorkingDirectory
  and both absolute startup checks point directly at the main repository.
  Preserved the preview hook, durable runtime directory, MemoryMax=768M and
  MemorySwapMax=0. Reloaded the user manager and restarted only this preview.
- The preview became ready in 42.65 seconds; sampled host headroom during
  restart never fell below 1,324 MiB. Verified the actual process directory,
  active/running state, 768 MiB cgroup limit, zero swap allowance, zero
  automatic restarts and zero cgroup OOM events/kills. Peak service memory
  after all HTTP checks was 287.72 MiB.
- Eight current-content routes return HTTP 200: `/`, `/rv32im/`,
  `/rv32im/device-system/`, `/rv32im/recorded-games/`, `/device-io/`,
  `/device-io/framebuffer/`, `/visualizer/`, `/rv32i/memory-systems/`.
  All 27 served simulator file hashes match, accounting only for the known
  live-reload injection in HTML. The top-level-only iframe polling guard
  remains present.

## 2026-10-22 — Retired linked checkouts and final state

- Verified neither linked checkout had pending tracked/untracked changes or
  a process using it after the preview moved. Saved their worktree Git
  administration and `.git` link files in the external backup.
- Moved each former checkout's contents, except its `.git` link, into a
  temporary sibling directory on the same filesystem. Removed the now-empty
  Git checkout registration with `git worktree remove --force`, then renamed
  the preserved directory back to its original path without a `.git` link.
  The force option removed only the link/empty checkout and administration;
  source/build/evidence data was already outside that deletion target.
- Compared complete before/after entry metadata, including device/inode,
  mode, size, modification time and symlink targets: 1,739 entries in the
  former alpha directory and 4,753 in the former feature directory are
  unchanged. No multi-gigabyte build-tree copy or deletion was needed.
- `.worktrees/alpha-integration` and `.worktrees/rv32im-phase3` are now ordinary
  ignored archives, not Git worktrees or active development directories.
  Keeping the same paths preserves existing evidence references. The
  `.worktrees/.gitignore` rule already ignores them; no new ignore rule was
  necessary. Future work uses the main repository only.
- `git worktree list --porcelain` contains exactly the main checkout on
  `refs/heads/alpha`; there are zero linked worktrees. The 27-file simulator
  package and its current-source comparison remain valid after retirement.
- Detailed receipts are under `build-wasm/preview-main-alpha-migration/`:
  package/previous-package manifests, strict timing, loaded service settings,
  readiness, retired-directory metadata and final branch/HTTP/hash/memory
  verification. External backups retain both dirty-file sets and the retired
  worktree administration. Documentation whitespace is checked with
  `git diff --check`.

No commit or push was made. Documentation remains uncommitted in the main
alpha checkout. No C++/WASM recompilation, browser gameplay check or native
regression ran; this operation does not close phase 6 or start phase 7.
