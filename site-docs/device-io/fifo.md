# SynchronousFifo

Parameterized synchronous FIFO, widths 8, 16 and 32.

Catalog: `device.fifo.8`. Structural and compact implementations are selectable. The compact form runs
this component's own four-state gate list with characterized storage and
same-circuit delay bounds. Fidelity and protocol validation is in progress.

Width variants use the corresponding catalog suffix; pins below show the recorded
eight-bit form. The 16-bit variant is structural; existing eight-/32-bit
compact forms keep their configured timing contracts.

## Pins

| Pin | Direction | Bits |
| --- | --- | --- |
| `CLEAR_OVERFLOW` | Input | 1 |
| `CLK` | Input | 1 |
| `POP` | Input | 1 |
| `PUSH` | Input | 1 |
| `PUSH_DATA` | Input | 8 |
| `RST` | Input | 1 |
| `COUNT` | Output | 3 |
| `DATA` | Output | 8 |
| `FULL` | Output | 1 |
| `OVERFLOW` | Output | 1 |
| `PUSH_READY` | Output | 1 |
| `VALID` | Output | 1 |

## Behavior

The `depth` construction parameter accepts 1–7 entries (default 4). The
three-bit COUNT pin is unchanged. Unsupported keys and capacities are rejected.

PUSH with PUSH_READY appends PUSH_DATA; POP with VALID removes DATA at the same rising edge. COUNT is 0..depth. VALID/FULL/PUSH_READY depend on registered occupancy. Rejected pushes set sticky OVERFLOW; CLEAR_OVERFLOW clears it unless a new rejected push occurs.

## Internal structure

`depth` register words, read/write pointers with wrap at the chosen capacity,
occupancy counter, muxes and control gates. The default four-entry structure
is retained. Non-default compact variants compile their own configured
structural circuit and derive its timing bounds at construction; they do not
reuse the four-entry delay table. The circuit viewer shows count, head and
overflow at the inspected time.

## Edge cases and integration

At full with simultaneous POP/PUSH, the pop succeeds and the new push is dropped (overflow sets). At empty with simultaneous POP/PUSH, the new item remains. DATA=0 while empty. RST discards queue and overflow. No same-cycle pop-to-ready dependency is introduced.

## Recorded example and checks

`SynchronousFifoTest` owns the independent functional checks; `SynchronousFifoScenarioTest` retains this smaller recorded example. Both use the same logical component owner.

<!-- simulator-examples: synchronous-fifo SynchronousFifoScenarioTest -->

<div class="simulator-embed"><iframe src="../../simulator/?scenario=synchronous-fifo&amp;embed=1" title="SynchronousFifo" loading="lazy" allowfullscreen></iframe></div>

<a href="../../simulator/?scenario=synchronous-fifo" class="md-button">Open the recorded circuit</a>
