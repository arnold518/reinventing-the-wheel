# Provisioned FF images during controller startup

Started: 2026-10-21. Status: open.
Code state: isolated `rv32im-phase3` worktree from `60acda2`, with staged time/game work.

## Summary

Investigate a pre-existing mixed-profile startup hazard: an expanded FF endpoint
with a compact controller can expose unknown MEM_RST before its constant zero
settles, destroying provisioned bytes in the native FF device. New default game
profiles avoid that mixture, but arbitrary fidelity choices should retain the
same endpoint image contract. Add a focused structural/fused/mixed provisioning
check before changing the constant's startup scheduling. Primary ongoing work
is untouched.

## 2026-10-21 — Evidence and check

The initial framebuffer integration found unwritten byte 4 had known mask zero
after pre-simulation clearing, rather than 255. Fully structural and fused
endpoints work; the hybrid selects the controller's delayed constant reset.
Native FF storage correctly invalidates bytes when its own reset is unknown;
do not weaken that device rule. Interface reset must retain stored images.

Staged MemoryProvisioningStartupTest checks 16 known bytes through initialization,
interface reset release/reassertion and unknown interface reset, without any
memory write or clock edge, in structural/fused/mixed-controller configurations.
Execution and any fix are pending. See the linked phase 4–6 record for the
original framebuffer observation and subsequent default-profile checks.

### Reproduction and scoped fix

The WASM startup check passes structural and fused profiles and fails the mixed
profile before any clock edge or request: `Cannot read unknown memory byte as
uint8_t` (`/tmp/circuitsim-memory-startup-before.log`). This confirms the image
loss independently of the new framebuffer/system.

The compact FF controller now schedules its native MEM_RST constant zero at
initialization time, matching the structural controller's zero-delay constant.
It does not change the native FF device's unknown-reset rule, clear image bytes,
run a transaction or create a clock edge. Unwired outputs also record the
constant at that time. Other technologies and operational publication paths
are unchanged. Fresh startup, old-memory and full native/WASM checks follow.

### Focused WASM validation

The rebuilt check passes all three profiles through initialization and interface
reset release, reassertion and unknown reset, retaining all 16 known bytes.
Log `/tmp/circuitsim-memory-startup-after.log`. This confirms the scoped startup
fix in WASM; fresh native, old-memory and timing regressions remain pending.

The fresh native provisioning check also passes (0.09 s), with all three
profiles retaining bytes through startup and interface reset. The full old
memory regression panel remains pending.
