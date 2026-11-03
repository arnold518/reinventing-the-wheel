# Local preview simulator package recovery

Started: 2026-10-29. Status: closed. Code state: `alpha` at `5549ee0` plus the
existing uncommitted working tree.

## Summary

Port 8893's MkDocs service was active but HTTP requests timed out after
repeated failed live builds. Restarting exposed a missing `not-gate` example:
the preview selected the old `build-wasm` simulator package, while current
pages and verified examples were built in `build-wasm-milestone`. The preview
configuration now selects the current package for both example rendering and
simulator copying. The strict build and all ten live HTTP checks pass; five
served assets match their source hashes. The normal service is active with
zero automatic restarts and zero OOM events.

## 2026-10-29 — Diagnosis and correction

- `reinventing-the-wheel-pages.service` had been running since October 26;
  requests to `http://127.0.0.1:8893/` timed out after 10 seconds. Its journal
  reported repeated `BuildError` live-rebuild failures and a stuck server.
- A clean restart failed on `basic/1.3.md`: `Cannot render simulator examples
  ... 'not-gate'`. Stop the restart loop while correcting the source path.
- `build-wasm/pages/simulator/example-rows.json` was dated October 27 and
  lacked both `not-gate` and `bus-router`; the October 29 milestone package
  contains both, alongside the current Computer System site.
- `extra.preview_simulator_dir` in `mkdocs.preview.yml` selects the milestone
  package. Both hooks use that setting; the explicit example-file environment
  override and production default remain supported. The watch path follows
  the same package. Update the preview guide with recovery steps.
- Preserve the existing disk-backed cache, 768 MiB memory cap and disabled
  swap. No native or WASM compilation is needed for this package-path fix.

## 2026-10-29 — Verification and restoration

- `prlimit --as=805306368 /home/arnold/venv/bin/python -m mkdocs build
  --strict --config-file mkdocs.preview.yml --site-dir
  build-milestone/preview-recovery-20261029/site` passes in 44.52 seconds.
  All 145 current example markers match the selected bundle's scenario and
  test metadata; every one of the package's 32 files copies byte-for-byte.
- Start the normal `reinventing-the-wheel-pages.service`. Ten live routes
  return HTTP 200: `/`, `/basic/1.3/`, `/system/`, `/system/5.7/`,
  `/simulator/`, its WASM JavaScript, WASM binary and `example-rows.json`,
  and the Snake-with-LEDs recording and layout under
  `/assets/system/recordings/`.
- SHA-256 comparisons verify the served WASM JavaScript, WASM binary,
  example rows, recording and layout against their current source files.
  The live example bundle contains `not-gate`, and the chapter page contains
  the current Computer System content.
- Service state: `active/running`, `NRestarts=0`, peak memory 316,182,528
  bytes (about 302 MiB), `MemoryMax=805306368`, `MemorySwapMax=0` and
  `TMPDIR=/home/arnold/.cache/circuitsim-preview`. Its cgroup reports
  `max=0`, `oom=0`, `oom_kill=0`.
- No full native suite, browser suite, simulation campaign or WASM rebuild
  was run; verification covers the strict preview build and local HTTP
  delivery of the existing current package.
