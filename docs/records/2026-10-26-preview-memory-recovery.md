# Preview build storage and port 8893 recovery

Started: 2026-10-26. Status: closed. Code state: `alpha` at `5549ee0`, preserving existing
component, recording and layout changes and the running gameplay campaign.

## Summary

Port 8893 is restored on the normal MkDocs service. Its old tmpfs-backed build
storage forced large recordings into RAM, causing a 768 MiB OOM restart loop.
Move the build into a managed disk-backed user cache, keeping the memory cap
and swap prohibition. A private strict build and actual 8893 startup both pass;
six live routes and a recorded chunk hash pass, with no OOM or restart. Gameplay
capture and all frozen executors remain intact.

## 2026-10-26 — Evidence and recovery

The preview restart counter reached 138. Journals report repeated `oom-kill`
results and exactly 768 MiB peaks, with no listener on 8893. Stop that unit
instead of continuing the restart loop. `/run/user/1001` is tmpfs (732 MiB);
the repository and `~/.cache` share the disk-backed ext4 filesystem.
The published gameplay directory is now 1.9 GiB. Trace hardlinks fall back to
copies across filesystems, so the tmpfs-backed generated site cannot fit.

Start the temporary `circuitsim-preview-recovery-8893` unit with a 128 MiB
cap and no swap, serving a hardlinked copy of the last successful site with
current asset/simulator symlinks. Root, simulator and full Snake tick index
return HTTP 200, and 8893 listens again. The full-tick campaign continues:
Snake is verified/published and Pong is executing; other games remain queued.

## 2026-10-26 — Durable fix and verification

Install `storage.conf` in the user unit's drop-in directory:

```ini
[Service]
CacheDirectory=circuitsim-preview
Environment=TMPDIR=%C/circuitsim-preview
```

The unit file expands `%C` to `/home/arnold/.cache`; process-environment checks
confirm the actual managed disk path. The cache and repository share ext4, so
the recording hook hardlinks immutable chunks. The 768 MiB and no-swap limits
remain unchanged. Add a maintained [preview guide](../guides/local-preview.md)
with installation and recovery steps.

An initial isolated transient-unit test passed the `%C` text through DBus
without expansion. Python fell back to `/tmp`, which is also tmpfs here, and
that test reproduced the OOM. Correct the transient test to use an absolute
path; remove only its known failed temporary site after confirming its process
exited. The public recovery listener stayed available during that investigation.

The corrected private strict preview builds in 92.90 seconds under the same
768 MiB cap, opens loopback port 18893 and returns HTTP 200. Then stop both
temporary services and start the normal service on 8893. Its first HTTP 200
arrives after 131.4 seconds, with zero automatic restarts. Cgroup counters show
`oom=0`, `oom_kill=0`, `shmem=0`; about 178 MB is anonymous memory, and the
reclaimable disk cache can reach the cap without killing the process.

Six routes pass: root, simulator, recorded-games page, full-tick campaign
index, full Snake tick index and the small validation index. The first Snake
chunk matches its published SHA-256, and the main generated site's chunk
shares its source inode. The originally reported `0.0.0.0:8893` endpoint also
returns HTTP 200, listening under the normal MkDocs PID. The existing trace
asset regression check passes; no native/game/WASM rebuild or broad tests
were needed. Receipt: `build-milestone/preview-recovery-20261026-receipt.json`.
