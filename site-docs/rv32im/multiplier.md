# Multiplier32

Iterative 32-bit multiplier with retained completion.

Catalog: `arithmetic.multiplier.32`. The implemented form is structural; selectable lower-level children may be compact. No new whole-component behavioral form is registered.

## Pins

| Pin | Direction | Bits |
| --- | --- | --- |
| `A` | Input | 32 |
| `B` | Input | 32 |
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
| `HI` | Output | 32 |
| `LO` | Output | 32 |
| `READY` | Output | 1 |

## Behavior

On a rising edge with ENABLE, START and READY high, latch A, B and signedness. The following 32 enabled rising edges perform one shift/add step each. DONE then stays high until an enabled TAKE edge. LO is the raw low product; SIGNED_A and SIGNED_B select the interpretation of each operand for HI.

## Internal structure

MultiplyStep<32>, low/high product registers, operand/sign registers, a five-bit iteration counter and ownership gates. Signed high correction subtracts original operands from the unsigned high half.

## Edge cases and integration

ENABLE pauses iteration and completion consumption. CANCEL synchronously discards ownership even while ENABLE=0; RST asynchronously clears state. READY is low during RST/CANCEL, BUSY or unconsumed DONE. Result pins are meaningful only with DONE. START while unavailable is ignored.

## Recorded example and checks

`Multiplier32Test` owns the independent functional checks; `Multiplier32ScenarioTest` retains this smaller recorded example. Both use the same logical component owner.

<!-- simulator-examples: multiplier32 Multiplier32ScenarioTest -->

<div class="simulator-embed"><iframe src="../../simulator/?scenario=multiplier32&amp;embed=1" title="Multiplier32" loading="lazy" allowfullscreen></iframe></div>

<a href="../../simulator/?scenario=multiplier32" class="md-button">Open the recorded circuit</a>
