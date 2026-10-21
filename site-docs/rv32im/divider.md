# Divider32

Iterative 32-bit divider with retained quotient and remainder.

Catalog: `arithmetic.divider.32`. The implemented form is structural; selectable lower-level children may be compact. No new whole-component behavioral form is registered.

## Pins

| Pin | Direction | Bits |
| --- | --- | --- |
| `A` | Input | 32 |
| `B` | Input | 32 |
| `CANCEL` | Input | 1 |
| `CLK` | Input | 1 |
| `ENABLE` | Input | 1 |
| `RST` | Input | 1 |
| `SIGNED` | Input | 1 |
| `START` | Input | 1 |
| `TAKE` | Input | 1 |
| `BUSY` | Output | 1 |
| `DONE` | Output | 1 |
| `QUOTIENT` | Output | 32 |
| `READY` | Output | 1 |
| `REMAINDER` | Output | 32 |

## Behavior

Launch and completion use the same handshake as Multiplier32: one launch edge, then 32 enabled iteration edges. SIGNED selects signed or unsigned operands. Signed quotient truncates toward zero; remainder has the dividend sign.

## Internal structure

DivideStep<32>, quotient/remainder and divisor registers, sign correction, iteration and ownership gates. The extra remainder comparison bit stays separate from supported-width buses.

## Edge cases and integration

Division by zero returns quotient 0xffffffff and the original dividend as remainder. INT_MIN/-1 returns INT_MIN and zero. Neither raises a trap. ENABLE, TAKE, CANCEL and RST follow Multiplier32; outputs are meaningful only at DONE.

## Recorded example and checks

`Divider32Test` owns the independent functional checks; `Divider32ScenarioTest` retains this smaller recorded example. Both use the same logical component owner.

<!-- simulator-examples: divider32 Divider32ScenarioTest -->

<div class="simulator-embed"><iframe src="../../simulator/?scenario=divider32&amp;embed=1" title="Divider32" loading="lazy" allowfullscreen></iframe></div>

<a href="../../simulator/?scenario=divider32" class="md-button">Open the recorded circuit</a>
