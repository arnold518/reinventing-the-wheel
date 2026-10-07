# Register32: FF word integration

## Contract and construction

`Register32` retains its name, `memory.register.width32` family, selectable
structural/behavioral implementations, and D/WE/CLK/RST/Q pins. Its structural
implementation now contains `STORAGE: FFWord<32>` and `FULL_WRITE_MASK = 1111`.
There is no second state array or memory controller inside the wrapper.

All existing RV32I family-based uses resolve to the new implementation: the
single-cycle and pipeline PCs, standalone control flow, pipeline payload fields,
and retirement state. The architectural register file continues to share an
`FFStorageBank<32,31>`; it does not gain extra per-word register wrappers.
The old small-memory consumers retain their public interfaces and also reuse
the new register through the same family.

The behavioral register previously preserved agreed ones under an uncertain
write enable. That was an optimistic merge, unlike the gate-built feedback mux
already used by FFWord and FlipFlopCell. It now preserves only agreed zeros in
that case; the new X/Z scenarios explicitly test the correction. Binary reset,
capture and hold behavior is unchanged. Cross-fidelity checks use settled
observations with stable setup inputs, not ambiguous clock edges or transient
gate-by-gate equivalence.

## Documentation and visualizer

4.3.1 is the new Register32 reference; the register file is now 4.3.2, followed
by ALU32 (4.3.3), decoder (4.3.4), control flow (4.3.5), memory access (4.3.6),
and execution status (4.3.7). Existing page filenames remain stable. Chapter 3
and single-cycle cross-references link to the new register page.

The reference follows the existing component-table, IO, behavior/C++ example,
implementation, complete checkpoint table/embed, and tests pattern. The exact
C++ example is compiled and checked against all 56 declared checkpoints.

`tools/generate-register32-layouts.py` updates only Register32 type/root layouts.
Its old geometry is retained in automatic-layout migration history, and all
other type/profile/root entries remain unchanged. Running it twice is
byte-idempotent. The FF word retains its existing byte/bit-ordered floorplan.
Known standalone-register and RV32I state-word fidelity paths migrate from
`BIT_n` to `STORAGE.CELL_n`; existing canonical paths take priority. Native and
WASM migration functions are tested together. Unrelated payload-register paths
are intentionally not renamed by this compatibility rule.

## Validation

- `Register32Test`: 56 structural/behavioral checkpoints, including each bit,
  disabled writes, capture/hold, X/Z data/control, reset priority and recovery.
- `RegisterFFIntegrationTest`: context-free construction, structural word,
  behavioral word, behavioral cells, and reverse-time Q history against the
  behavioral register. Both CPU cores pass programs 03/05/07 with structural
  wrappers; program 03 additionally uses structural FF words with compact cells.
  The single-cycle core exercises one Register32, and the pipeline exercises 43.
- Focused regressions: old small memories/register file, PC/control-flow units,
  all four pipeline register blocks, retirement, Fetch, and single-cycle stages.
- Documentation: register and register-file C++ examples plus 4.3/4.4 references;
  clean strict MkDocs build after renumbering headings and fragment links.
- `RegisterWasmParityTest.mjs`: native/WASM topology, timestamps, checkpoints,
  every expected output and final internal state agree for three register modes,
  single-cycle Fetch and IF/ID.
- Browser checks on port 8893: all seven 4.3 pages pass navigation, rendered-table,
  embed and mobile-width checks. Live WASM passes 56 register, 47 register-file,
  and 31 control-flow checkpoints. Tests start with the previous saved layout;
  the new register shell and focused FF word/cell views also pass geometry checks.
  The shell and byte-organized storage screenshots were visually inspected.

Builds used one compilation job. The expanded integration run took 14.26 seconds
and peaked at 131.0 MiB RSS with no process swaps. This is a focused regression
run, not a claim that the entire repository test suite was rerun.
