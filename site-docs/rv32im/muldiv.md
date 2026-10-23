# `MulDivUnit<W>`


`MulDivUnit<W>` supports W = 4, 8, 16 or 32 with a shared launch/result
interface. Each operation performs W enabled iteration edges after launch.
Catalog families are `arithmetic.iterative.mul-div.W`.

The structural implementation composes the parameterized multiplier and
divider. At W=32, compact fidelity executes that circuit's own four-state gate
list with characterized register state and a delay table proven from the same
circuit. It preserves iterative latency and ownership. The other widths have
structural fidelity. See [word components](../components/word-components.md).

## Pins

| Pin | Direction | Bits |
| --- | --- | --- |
| `A` | Input | W |
| `B` | Input | W |
| `CANCEL` | Input | 1 |
| `CLK` | Input | 1 |
| `ENABLE` | Input | 1 |
| `OP` | Input | 3 |
| `RST` | Input | 1 |
| `START` | Input | 1 |
| `TAKE` | Input | 1 |
| `BUSY` | Output | 1 |
| `DONE` | Output | 1 |
| `READY` | Output | 1 |
| `RESULT` | Output | W |

## Behavior

OP uses instruction funct3: 0 MUL, 1 MULH, 2 MULHSU, 3 MULHU, 4 DIV, 5 DIVU, 6 REM, 7 REMU. Latch OP and operands on launch; expose RESULT only when DONE. Latency, pause, completion retention and cancellation match the underlying units.

## Internal structure

Multiplier<32> and Divider<32>, an operation register and result-selection gates. Exactly one arithmetic engine accepts each launch.
The structural viewer places the two engines side by side, with the operation
register and control/result-selection gates below them. Zoom into either
engine to inspect its iterative datapath.

## Edge cases and integration

Changing OP/A/B after launch does not change the owned result. Cancellation is synchronous and works while disabled. Results are not shortened to host arithmetic latency.

## Standalone timing boundary

Under the same synchronous binary input boundary as the core, the structural
unit certifies at 64 unit ticks, 2322 ps (embedded) and 1620 ps (desktop).
`RV32IMTimingTest` derives these periods independently for each profile.
Launch latency remains one launch edge plus 32 enabled iteration edges.

## Recorded example and checks

`MulDivUnitWidth32Test` owns the independent functional checks; `MulDivUnitWidth32ScenarioTest` retains this smaller recorded example. Both use the same logical component owner.

<!-- simulator-examples: muldiv-w32 MulDivUnitWidth32ScenarioTest -->

<div class="simulator-embed"><iframe src="../../simulator/?scenario=muldiv-w32&amp;embed=1" title="MulDivUnit<32>" loading="lazy" allowfullscreen></iframe></div>

<a href="../../simulator/?scenario=muldiv-w32" class="md-button">Open the recorded circuit</a>
