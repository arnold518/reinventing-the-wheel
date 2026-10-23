# RV32IM program numbering, algorithms and inspection fixes

Started: 2026-10-23. Status: closed. Code state: `alpha` based on `7b154e3`,
with this inspection/program change.

## Summary

The owner requested fixing execute placement inside the core, supporting M
instructions in the instruction-memory display, numbering programs from 23
and adding more general programs that use multiply/divide. These changes
are implemented and live on port 8893. Eight focused native checks, layout/
alias/migration checks, five WASM program exports and two browser sessions
pass. The final preview passes route/asset checks and live Fibonacci
inspection/playback. This follows the closed
[grouping/program record](2026-10-23-rv32im-grouping-and-program-scenarios.md).

## 2026-10-23 — Reproduction and scope

Fresh port-8893 inspection shows all pipeline stages centered at relative
y=0.2942 except EXECUTE, whose center is 0.1933. Its saved width/position
still assume the old 2.91 aspect; the current stage aspect is 0.95.
The instruction-memory serializer calls RV32IDecoder::disassemble for every
CPU, which correctly rejects M in I-only contexts but cannot display M code.
The new program fixtures are short straight-line checks with unnumbered
menu titles. Preserve these checks, number them 23–25 and add general
algorithm programs with real multiply/divide use, loops, memory results and
independent directed expectations. Keep validation focused.

## 2026-10-23 — Implementation and native acceptance

Added `rv32im::disassemble` with all eight M mnemonics and strict I fallback.
Memory inspection selects it from the containing machine's contract metadata,
including a compact CORE sibling; generic/I-only memories still use the I
decoder. The semantic check covers all eight register-form encodings, reserved
encodings and I fallback. The memory-observer check verifies the same M word
is invalid in an I context and displayed as `mul x3, x1, x2` in an M context,
without advancing simulation or changing signal history.

Programs are numbered 23–27 in the menu, documentation and registry scenario
IDs. Numbered URLs are available for all five. The first three keep their
existing canonical descriptive aliases so saved profiles/bookmarks/layouts
remain valid; 26/27 use numbered canonical aliases. The catalog's focused
Python/JS alias check passes.

Program 26 uses fast doubling to compute F(10)=55 and F(11)=89. It derives
the highest input bit, processes four bits, writes four intermediate pairs,
and retires exactly 12 MUL and five DIVU instructions. Program 27 computes
gcd(1071,462)=21 with Euclid's loop, stores quotient/remainder pairs, and
uses MUL to check q*b+r=a inside the program; it retires three each of MUL,
DIVU and REMU. Both retain the existing independent oracle/register/terminal/
full-memory checks, plus explicit expected arrays and M retirement counts.
Branch labels resolve against the actual instruction image. Per-program
cycle limits remain bounded (2,500 for Fibonacci, 1,800 for GCD).

All eight selected native checks pass in 19.59 seconds: the five programs,
M semantic/display decoding, memory observation and registry coverage.
Fibonacci and GCD take 7.51 and 4.31 seconds respectively. No broad component
or benchmark suite was run; no hardware circuit or timing table changed.

Refreshed four M-core layout scopes with the existing five-stage floorplanner
using current child aspects. EXECUTE now has width 0.10563 and y=0.21055,
matching the other stages' center at 0.2942. Automatic-layout history retains
the actual former effective placements for both type and root overlays, so
unchanged shipped views upgrade while custom/manual choices survive.
The six core/program static layouts and layout migration check pass.
An initial static-check invocation used secondary numbered aliases, which
that tool does not accept; the corrected run uses canonical aliases.

The serial WASM/site rebuild selects only the five affected program example
groups, retaining unrelated verified exports. The generator now also checks
M instruction-memory text on the compiled module. Browser checks will inspect
Fibonacci/GCD, numbered menu entries, pipeline centers and actual memory text.

## 2026-10-23 — WASM, rendered site and browser acceptance

The serial WASM build and five selected program exports pass, retaining
unrelated verified exports. The instruction-memory serializer decodes each
M word to its exact expected mnemonic/register form in the compiled module,
including the old all-eight-M program. Fibonacci and GCD export 82 and 45
checkpoints, with every checkpoint's PC/terminal pins verified. The strict
combined site build passes in 94.15 seconds.

The two selected browser sessions pass in 75.54 seconds. They verify all
numbered menu entries, a structural CPU, halt/rewind, the visible grouped
scopes, exact alignment of the five pipeline stage centers and execute width
at least 90% of fetch width. Actual instruction-memory canvas painting contains
M mnemonics; both algorithms expose DIVU and the GCD image exposes REMU.
Screenshots confirm the centered execute stage inside the running program's
core. Native Fibonacci retires 80 instructions in 657 cycles (561 M waits);
GCD retires 43 in 346 cycles (297 M waits). These describe the fixtures,
not a performance campaign.

The verified 28-file simulator package is being installed on the local 8893
preview; prior assets are retained. Live route/asset checks and final records
are pending. No circuit/timing-table change, full suite, game campaign or
published-site deployment is part of this selection.

## 2026-10-24 — Local preview and completion

Installed the verified package atomically, preserving its predecessor at
`build-wasm/rv32im-program-expansion-refresh/20261023T145634Z/simulator-before`,
then restarted `reinventing-the-wheel-pages.service`. The strict preview
rebuild completes in 90.97 seconds. All eight selected routes respond and
all 26 non-HTML simulator assets match exact package hashes. The program
page has five generated program tables and one embedded viewer. A fresh
browser on actual port 8893 verifies Program 26's structural core, numbered
menu, aligned stages, rendered M instructions, halt and rewind.

Only the M-core type/profile floorplans change; unrelated type/profile
geometry is semantically identical to the starting revision. Live package
inputs match the verified staging. Existing descriptive program aliases
and saved custom/manual placements remain available.

Evidence is retained under `build-milestone/rv32im-program-expansion/`:
`native-checks.log`, `layout-check-final.log`, `layout-migration.log`,
`catalog-alias-check.log`, `build-pages.log`, `web/results.json`,
`screens/rv32im-program26-core.png`, `preview-refresh.json`,
`preview-checks.json` and `live-program.log`. Key results are copied above.
This closes the four requested changes. No production circuit, timing table,
full test suite, benchmark/game campaign, push or published-site deployment
was performed.
