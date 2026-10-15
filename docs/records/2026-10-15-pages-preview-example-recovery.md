# Port 8893 preview recovery

Started: 2026-10-15. Status: closed. Code state: `alpha` at `1d861c0`, with uncommitted cache work.

## Summary

The local port 8893 MkDocs preview became unresponsive after the new cache
page requested generated example rows absent from its older default bundle.
Adding the 12 verified native scenario rows to that ignored local bundle
restored the preview. The packaged WebAssembly simulator is still older and
does not contain the cache scenario; a full Pages rebuild remains part of
the open cache work.

## 2026-10-15 — Investigation

The `reinventing-the-wheel-pages.service` process was active, but a local
request to `http://127.0.0.1:8893/` timed out after 15 seconds. The user
service log reported `Cannot render simulator examples in memory/3.4.md:
'cache-read-hit'` and said the server would appear stuck after the build
error. The public cache page had added the corresponding example marker,
while `build-wasm/pages/simulator/example-rows.json` was generated before
that scenario existed.

The native `VisualizerSession` export used for the strict MkDocs check
contained `cache-read-hit` with 12 checkpoints. Comparing it with the
preview's existing JSON bundle showed exactly one added scenario; the
top-level metadata and every previously bundled scenario were unchanged.
The older packaged `scenarios.json` still has no cache scenario alias.

## 2026-10-15 — Recovery and verification

Saved the prior bundle at `/tmp/circuitsim-8893-example-rows.before.json`,
then copied the native-verified bundle into the ignored local file
`build-wasm/pages/simulator/example-rows.json`. A strict build using
`mkdocs.preview.yml` and the preview's default example path passed in
74.98 seconds. Local HTTP checks then returned 200 for the home page,
`/memory/3.4/`, and `/simulator/`; the served cache page includes the 12
generated example rows.

This restores the documentation preview. It does not build or test a new
WebAssembly module. The embedded cache scenario remains unavailable until
`tools/build-pages.sh` can run with Emscripten and its browser checks pass.
The generated bundle is ignored and will be replaced by a future full build.
