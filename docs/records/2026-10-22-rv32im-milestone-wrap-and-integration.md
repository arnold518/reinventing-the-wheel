# RV32IM milestone wrap and branch integration

Started: 2026-10-22. Status: open.
Code state: uncommitted `rv32im-phase3` from `60acda2`; primary `realistic-timing` at `0d498c7`; `alpha` at `0d498c7`.

## Summary

The owner requested wrapping the current worktree and merging it back to
the `alpha` branch. The owner merged the newer visualizer work into `alpha`,
which now matches the primary committed head. Preserve the primary
checkout's pending planning files and four newer visualizer/documentation
commits. Phase 6 acceptance remains open; phase 7 has not started. This wrap
must preserve those boundaries rather than label interactive play complete.

## 2026-10-22 — Integration inventory and scope

- Checkpoint the implemented RV32IM/device/scripted-game platform, 64-bit
  time, configurable queues/displays and observational graphics/keyboard UI.
- Current native/Python/WASM proofs and strict focused site builds are in
  the linked [parameter record](2026-10-21-device-parameters-and-visuals.md)
  and [milestone record](2026-10-21-scripted-games-phase4-6-implementation.md).
- The preceding native scope is 535/543, with seven focused follow-ups
  resolved and the large SRAM case resource limited. The portable refresh
  passes 54 checks, 11 system checks, all 22 device/native scenario comparisons
  and exact wide-time transport. All 175 old Python API entries are preserved.
- The strict site package regenerates all 22 new groups and retains 135
  previously generated legacy exports. Full current-source legacy regeneration
  stops at the 512 MiB host headroom guard, including in fresh processes.
- The first browser launch used a Python without Playwright. The documented
  system Python launches it; a missing device-system embed was found, fixed
  and rebuilt strictly. A later full browser scope stops at the memory guard
  after four M pages pass. This is not a complete browser pass.
- Add an explicit browser scenario filter for bounded display/keyboard checks;
  default full coverage and all page assertions remain. Record selected scope.
- Full revised-game repetition/fidelity/quantum/performance and compiled-game
  WASM parity remain unrun. Do not start that long campaign during this wrap.
  Phase 7 remains a subsequent milestone after acceptance.
- Primary pending files are STATUS, CHANGELOG, the future record, two device/
  playability records and original playability tools. Capture exact backups
  and inspect their relationship to this topic before integration. Do not
  reset, stash away or overwrite ongoing work as a shortcut.

## Merge procedure

Prepare the feature checkpoint, integrate with the chosen target in an
isolated checkout, resolve overlaps while retaining both changes, run serial
checks that fit the current RAM limit, then advance the requested branches
only after their current state is verified. No push or deployment is implied.

### Target confirmed and primary snapshot

The owner selected `alpha`, now at `0d498c7`, and no worktree has it checked
out. Integrate its four newer commits into the feature checkout, then advance
`alpha` only after checks and a fresh expected-head check. Leave the primary
`realistic-timing` checkout and its dirty files untouched. Exact copies and
SHA-256 receipts for all primary pending planning files and eight playability
tools are retained in the isolated build. The eight tools are copied unchanged
into this milestone so earlier audit references remain usable; the original
2026-10-19 future-plan sections are preserved before the later discussion.
