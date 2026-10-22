# Alpha local documentation preview

Started: 2026-10-22. Status: closed. Code state: `alpha` at `88c0a95`,
with this documentation-only operational record.

## Summary

Port 8893 now serves the dedicated `.worktrees/alpha-integration` checkout
and matching simulator artifacts. The strict preview build passes; eight
documentation routes and all 27 served simulator files are checked. The
service is capped at 768 MiB with no swap. Its observed peak after the HTTP
checks is 279.72 MiB, with zero OOM events or automatic restarts. The primary
and feature checkouts are unchanged. GitHub Pages and milestone acceptance
are outside this local preview update.

## 2026-10-22 — Investigation and plan

- `reinventing-the-wheel-pages.service` uses the primary repository as its
  working directory and runs `mkdocs serve --config-file mkdocs.preview.yml
  --dev-addr 0.0.0.0:8893 --strict`. Its absolute startup checks also point
  at that checkout. It follows files in a directory, not a named branch.
- The primary HEAD reflog records the switch from `alpha` to
  `realistic-timing` on 2026-10-16 at 01:54:10 KST. The milestone merge
  occurred in the isolated `alpha` checkout and therefore did not change
  the served files. The old local `/rv32im/`, `/device-io/` and
  `/simulator/games.html` routes returned 404 in the preceding inspection.
- Both isolated checkouts start at `88c0a95`. Reuse the matching simulator
  package from `.worktrees/rv32im-phase3/build-wasm-phase3/pages/simulator`
  rather than recompiling or rerunning the full example campaign. Compare
  its frontend, layouts, palette, default profiles and compiled artifacts
  before staging it in alpha's ignored `build-wasm/pages/simulator`.
- Keep the existing preview hook and durable `RuntimeDirectory`/`TMPDIR`.
  A user-unit drop-in will override the working directory and both startup
  checks. Bound this preview service to 768 MiB with no service swap, and
  run the serial strict preflight under the existing host-memory guard.
- The example bundle retains the milestone's stated verification limits:
  five currently refreshed device groups and 152 earlier exports. This
  operation does not establish a full regenerated-example or browser pass,
  complete phase 6, implement phase 7, or publish GitHub Pages.

Verification results and final service configuration will be appended after
the preflight and local HTTP checks. Ignored evidence is retained under
`build-wasm/preview-alpha-update/` in the alpha checkout.

## 2026-10-22 — Change and verification

### Artifact preparation and preflight

- Checked the two isolated checkouts' identical `88c0a95` source revision.
  All 26 frontend/configuration/compiled-artifact comparisons pass. The
  native-development `runtime-config.js` is deliberately replaced with
  `runtime-config.pages.js`, following `tools/build-pages.sh`; the initial
  blanket comparison stopped at this expected override before staging.
- Copied 27 files (66,953,327 bytes) into alpha's previously absent
  `build-wasm/pages/simulator`, verifying every copied file by SHA-256 before
  renaming the staging directory. No compiler or example generator ran.
- The staged and HTTP-served WASM SHA-256 is
  `aca334499d0ff55615d3a3f70c9eb3e35044dbc4acd7105e2a1ad9529b860f0d`
  (39,730,223 bytes). The retained example bundle SHA-256 is
  `9a22c43610b81c3d23a81ff19b7fdded9f40f904f18a2eabab47f275ea322bcd`
  (10,983,286 bytes).
- Ran `/home/arnold/venv/bin/python3 -m mkdocs build --config-file
  mkdocs.preview.yml --strict --site-dir build-wasm/preview-alpha-update/site`
  under the existing `guarded-command-next.py`, with a 768 MiB address-space
  cap, 900 MiB starting headroom, a 512 MiB stop threshold and a 30-second
  maximum initial wait. Exit status 0; wall time 46.15 seconds; peak RSS
  180,176 KiB. Existing informational relative-link messages remain; there
  are no strict-build warnings or errors.

### Local service

Installed a local user-unit drop-in at
`~/.config/systemd/user/reinventing-the-wheel-pages.service.d/alpha.conf`:

```ini
[Service]
WorkingDirectory=/home/arnold/arnold/github/reinventing-the-wheel/.worktrees/alpha-integration
ExecStartPre=
ExecStartPre=/usr/bin/test -f /home/arnold/arnold/github/reinventing-the-wheel/.worktrees/alpha-integration/site-docs/index.md
ExecStartPre=/usr/bin/test -f /home/arnold/arnold/github/reinventing-the-wheel/.worktrees/alpha-integration/build-wasm/pages/simulator/index.html
MemoryMax=768M
MemorySwapMax=0
```

- Reloaded the user manager and restarted only
  `reinventing-the-wheel-pages.service`. The original unit, preview hook,
  `RuntimeDirectory` and `TMPDIR` are preserved. Selected systemctl commands
  used `XDG_RUNTIME_DIR=/run/user/1001` and
  `DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1001/bus`.
- The service built the documentation in 47.41 seconds and became HTTP-ready
  in 48.19 seconds. Restart monitoring sampled at least 1,198 MiB host
  headroom, with a 512 MiB threshold for stopping this preview if necessary.
  That threshold was not reached.
- Confirmed the process's actual current directory, active/running state,
  effective cgroup `memory.max=805306368`, `memory.swap.max=0`, zero OOM
  events/kills and zero automatic restarts. Peak service memory after the
  HTTP checks was 279.72 MiB.

### Local HTTP and preservation checks

- All eight routes return HTTP 200 and their expected current content:
  `/`, `/rv32im/`, `/rv32im/device-system/`, `/rv32im/recorded-games/`,
  `/device-io/`, `/device-io/framebuffer/`, `/visualizer/` and
  `/rv32i/memory-systems/`. An initial case-sensitive expectation used
  "Memory systems" instead of the actual "Memory Systems" heading; the
  corrected content check passes.
- Fetched every simulator artifact over HTTP and matched all 27 SHA-256
  values and byte counts. For the two HTML files, removed only the expected
  injected live-reload script before comparing. All checked HTML retains
  the top-level-only polling guard. WASM and JavaScript module MIME types
  are correct. This includes `/simulator/games.html`, the current device
  view adapters, layouts, palette, profiles, runtime configuration and WASM.
- Compared primary/feature HEADs, statuses and all pending-file hashes with
  the pre-operation snapshots: both are unchanged, including all 13 dirty
  primary files. No native Python module, primary build artifact or other
  service was changed.
- Local evidence: `baseline.json`, `package-manifest.json`,
  `strict-build.time`, `service-before.txt`, selected before/after service
  properties, `restart-readiness.json`, `local-http-verification.json`,
  `preservation-verification.json` and a copy of `alpha.conf` under the
  ignored evidence directory above.

### Maintenance and limits

The service now watches files in the dedicated alpha checkout. It does not
pull Git or follow a branch checked out elsewhere. Documentation edits there
reload automatically; simulator source changes still require a matching
artifact refresh through the existing build/package process. Removing this
one drop-in, reloading the user manager and restarting this service restores
the original primary-checkout configuration.

No browser runtime/pixel checks, native regressions, full example regeneration,
GitHub push or remote deployment ran. The retained example verification
limits and open phase-6/phase-7 status stated above remain in force. Final
documentation whitespace validation is recorded by `git diff --check`.
