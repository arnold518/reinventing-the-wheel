# `Divider<W>`


`Divider<W>` supports W = 4, 8, 16 or 32. The structural iterative
recipe and handshake are shared across widths. The recorded example uses W=32.

Catalog: `arithmetic.iterative.divider.W`. The implemented form is structural; selectable lower-level children may be compact. No whole-component behavioral form is registered.

## Pins

| Pin | Direction | Bits |
| --- | --- | --- |
| `A` | Input | W |
| `B` | Input | W |
| `CANCEL` | Input | 1 |
| `CLK` | Input | 1 |
| `ENABLE` | Input | 1 |
| `RST` | Input | 1 |
| `SIGNED` | Input | 1 |
| `START` | Input | 1 |
| `TAKE` | Input | 1 |
| `BUSY` | Output | 1 |
| `DONE` | Output | 1 |
| `QUOTIENT` | Output | W |
| `READY` | Output | 1 |
| `REMAINDER` | Output | W |

## Behavior

Launch and completion use the same handshake as Multiplier<32>: one launch edge, then W enabled iteration edges. SIGNED selects signed or unsigned operands. Signed quotient truncates toward zero; remainder has the dividend sign.

## Internal structure

`DivideStep<W>`, quotient/remainder and divisor registers, sign correction, iteration and ownership gates. The extra remainder comparison bit stays separate from supported-width buses.

## Edge cases and integration

Division by zero returns quotient 2^W−1 and the original dividend as remainder. INT_MIN/-1 returns INT_MIN and zero. Neither raises a trap. ENABLE, TAKE, CANCEL and RST follow Multiplier<32>; outputs are meaningful only at DONE.

## Recorded example and checks

`DividerWidth32Test` owns the independent functional checks; `DividerWidth32ScenarioTest` retains this smaller recorded example. Both use the same logical component owner.

<!-- simulator-examples: divider-w32 DividerWidth32ScenarioTest -->

<div class="simulator-embed"><iframe src="../../simulator/?scenario=divider-w32&amp;embed=1" title="Divider<32>" loading="lazy" allowfullscreen></iframe></div>

<a href="../../simulator/?scenario=divider-w32" class="md-button">Open the recorded circuit</a>
