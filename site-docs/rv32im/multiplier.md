# `Multiplier<W>`


`Multiplier<W>` supports W = 4, 8, 16 or 32. The structural iterative
recipe and handshake are shared across widths. The recorded example uses W=32.

Catalog: `arithmetic.iterative.multiplier.W`. The implemented form is structural; selectable lower-level children may be compact. No whole-component behavioral form is registered.

## Pins

| Pin | Direction | Bits |
| --- | --- | --- |
| `A` | Input | W |
| `B` | Input | W |
| `CANCEL` | Input | 1 |
| `CLK` | Input | 1 |
| `ENABLE` | Input | 1 |
| `RST` | Input | 1 |
| `SIGNED_A` | Input | 1 |
| `SIGNED_B` | Input | 1 |
| `START` | Input | 1 |
| `TAKE` | Input | 1 |
| `BUSY` | Output | 1 |
| `DONE` | Output | 1 |
| `HI` | Output | W |
| `LO` | Output | W |
| `READY` | Output | 1 |

## Behavior

On a rising edge with ENABLE, START and READY high, latch A, B and signedness. The following W enabled rising edges perform one shift/add step each. DONE then stays high until an enabled TAKE edge. LO is the raw low product; SIGNED_A and SIGNED_B select the interpretation of each operand for HI.

## Internal structure

Four structural groups expose the data flow:

- `CONTROL`: busy/done ownership, a log2(W)-bit iteration counter and enables.
- `OPERANDS`: original operands and captured signedness.
- `DATAPATH`: low/high product registers, `MultiplyStep<W>` and next-value selectors.
- `RESULT`: signed high correction, subtracting original operands from the unsigned high half.

The groups contain the gates and selectable lower-level components; they
are wiring hierarchy with no separate behavioral implementation.

## Edge cases and integration

ENABLE pauses iteration and completion consumption. CANCEL synchronously discards ownership even while ENABLE=0; RST asynchronously clears state. READY is low during RST/CANCEL, BUSY or unconsumed DONE. Result pins are meaningful only with DONE. START while unavailable is ignored.

## Recorded example and checks

`MultiplierWidth32Test` owns the independent functional checks; `MultiplierWidth32ScenarioTest` retains this smaller recorded example. Both use the same logical component owner.
The recording covers unsigned, signed and mixed-sign products, high-word
boundaries, paused execution, held completion, consumption, cancellation
while paused and reset during an operation.

<!-- simulator-examples: multiplier-w32 MultiplierWidth32ScenarioTest -->

<div class="simulator-embed"><iframe src="../../simulator/?scenario=multiplier-w32&amp;embed=1" title="Multiplier<32>" loading="lazy" allowfullscreen></iframe></div>

<a href="../../simulator/?scenario=multiplier-w32" class="md-button">Open the recorded circuit</a>
