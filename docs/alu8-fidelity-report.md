# Chapter 2 selectable fidelity

Implemented on `github-pages`, 2026-09-08. Structural circuits remain the default.

## Scope

All eleven documented contracts now provide structural and behavioral implementations:
`Adder8`, `AddSub8`, `ArithmeticUnit8`, `LogicUnit8`, `BarrelShifter8`,
`ZeroDetect8`, `CompareSelect8`, `ALU8FlagUnit`, `ALU8ControlDecoder`,
`ALU8ResultSelector`, and `ALU8`.

Public type names, pins, structural instance paths, and existing contract versions
are preserved (`ALU8` remains version 2). Chapter 2 nested units use family-based
construction, so exact-path and contract profile rules work inside the ALU.
Explicit C++/Python structural constructors still build structural components.
No separate behavioral Python class names are exposed.

The shared schemas and private evaluators live in
`src/modules/composite/ALU8Behavioral.cpp`. The four-state value functions in
`ALU8ValueModel.hpp` preserve the structural gates' Boolean factoring, including
pessimistic mux behavior, independent comparison metadata, ignored inputs, and
`LOCAL_OP` wiring that preserves Z. They allocate no internal circuit objects.

Behavioral implementations use one abstract delay tick. The equivalence boundary
is settled output values, not internal propagation, glitches, or physical timing.
Use structural fidelity for gate-level timing observations.

## Validation

- Existing eleven recorded scenarios run against both fidelities through the
  unified component-test runner; all 649 documentation rows are unchanged.
- `ALU8ContractTest` checks both variants against independent expected results:
  262,144 add/sub cases, 20,480 shift cases, and 7,260 integrated ALU cases.
- `ALU8FidelityContractTest` passes 84,100 per-family comparisons, including
  exhaustive four-state zero detection and opcode/enable enumeration with
  sampled operands. Another 13 profiles each pass 256 mixed-fidelity ALU inputs.
- Native Python bindings, catalog inventory, hierarchy, portable session,
  documentation exporter, existing arithmetic components, and ALU32 tests pass.
- `ALU8WasmParityTest.mjs` passes 16 default scenarios and 13 behavioral/mixed
  variants, comparing native/WASM topology, recorded checkpoints, and examples.
- The browser regression passes all 11 embeds, 649 example rows, camera/panel
  checks, and a real ALU8 collapse/apply/reload/expand interaction. The existing
  local layout and fidelity persistence mechanisms remain in use.
- Native build, Emscripten build, and strict MkDocs build pass.

These tests do not exhaust the complete ALU's four-state input space.

## Informal performance observations

One local run on a shared host; not calibrated benchmarks or speed guarantees.

| Workload | Structural | Behavioral |
| --- | ---: | ---: |
| Native, 1,024 binary rows, history off, construction excluded | 13.321 s | 0.0498 s |
| Events in that native workload | 9,723,261 | 28,876 |
| WASM, load/simulate/serialize the 70-row ALU8 scenario, module already loaded | 1.933 s | 0.0475 s |
| Component count | 2,406 | 1 |

A structural ALU8 root with all seven immediate units behavioral has eight
components; the same WASM scenario took 0.0448 s in that run. This small timing
difference from the single-component variant is not evidence that hybrid is
intrinsically faster. Native and WASM timings above use different workloads.

## Preview

The complete static package is in `build-wasm/site/`; port 8893 serves the live
documentation preview with the packaged WASM simulator. In the explorer, click
the component arrow and **Apply & Run** to switch S/B. Example tables describe
the default structural checkpoints; selected fidelity changes timeline times.

Pages packages `visualizer-v2/profiles.defaults.json` as `profiles.json` so a
fresh checkout does not depend on ignored native runtime preferences. The native
`visualizer-v2/profiles.json` remains local; browser edits remain in browser storage.
