# Local documentation and visualizer preview

The user service `reinventing-the-wheel-pages.service` serves the documentation
and built WASM visualizer on port 8893 from this checkout. Its simulator source
is selected by `extra.preview_simulator_dir` in `mkdocs.preview.yml`, currently
`build-wasm-milestone/pages/simulator`; public recording assets come from
`site-docs`. The preview copies that package and reads its verified example
rows from the same directory. Rebuild it with
`tools/build-pages.sh build-wasm-milestone` when its code changes. Keep the
configuration's `watch` entry pointed at that directory too.

## Build storage and memory

MkDocs creates a complete temporary site before it starts listening. Use a
managed cache directory on the same disk filesystem as the checkout. Large
immutable tick recordings then use the hardlinks supplied by
`tools/mkdocs_recordings.py`. A RAM-backed `/run` build directory forces
cross-filesystem copies and can exceed the preview's 768 MiB cap before the
port opens. Keep the memory cap and swap disabled.

Install this user-service drop-in as
`~/.config/systemd/user/reinventing-the-wheel-pages.service.d/storage.conf`:

```ini
[Service]
CacheDirectory=circuitsim-preview
Environment=TMPDIR=%C/circuitsim-preview
```

`%C` expands to the user's cache root in a unit file. When testing with
`systemd-run -p Environment=...`, pass the expanded absolute path: a transient
DBus property does not expand this specifier, and Python otherwise falls back
to its default temporary directory. Check the actual process environment and
generated path as well as `systemctl show`.

Reload the user manager and restart the service. Verify HTTP on `/`,
`/simulator/`, `/system/`, and a game recording and its layout. When serving
tick recordings, also fetch an actual tick chunk. `active` alone
does not establish that MkDocs has finished building or bound its socket.

If HTTP hangs while the service stays active, inspect its journal for a
failed live rebuild. A missing simulator-example key usually means the
selected package predates the pages: compare its `example-rows.json` with
the current build before restarting. A strict build failure can leave the
live server waiting indefinitely. After fixing it, restart the service to
load changed hook modules and verify HTTP again.

## Recovery without interrupting game capture

If journals show a repeated build failure, stop the preview restart loop.
Serve a preserved successful disk build temporarily, with current `assets`
and `simulator` overlays, while validating the fix on a different loopback
port under the same memory limit. Keep gameplay services and their frozen
executors intact. Once the corrected strict build passes, stop the temporary
listener and restart the normal 8893 service. Check HTTP responses and the
restart counter after startup.

The [October 26 recovery record](../records/2026-10-26-preview-memory-recovery.md)
contains the measured failure and validation evidence.
