# MultiplyStep

Masked partial-product adder. Supported widths are 4 and 32 bits.

Catalog: `arithmetic.multiply-step.4`. The implemented form is structural; selectable lower-level children may be compact. No new whole-component behavioral form is registered.

Width variants use the corresponding catalog suffix; bus widths below show the recorded small form.

## Pins

| Pin | Direction | Bits |
| --- | --- | --- |
| `A` | Input | 4 |
| `BIT` | Input | 1 |
| `HI` | Input | 4 |
| `CARRY` | Output | 1 |
| `SUM` | Output | 4 |

## Behavior

`HI + (BIT ? A : 0)` produces a width-limited `SUM` and one-bit `CARRY`. Inputs are combinational. The 4-bit form is the representative exhaustive slice; the 32-bit form uses the existing ParallelPrefixAdder<32> family.

## Internal structure

Masked AND gates feed full-adder cells (4-bit) or ParallelPrefixAdder<32> (32-bit). No multiplication instruction or host multiplication executes in the component.

## Edge cases and integration

`BIT=0` suppresses A. X/Z controls follow the constituent four-state gates; this does not promise an integer result for unknown operands.

## Recorded example and checks

`MultiplyStepTest` owns the independent functional checks; `MultiplyStepScenarioTest` retains this smaller recorded example. Both use the same logical component owner.

<!-- simulator-examples: multiply-step MultiplyStepScenarioTest -->

<div class="simulator-embed"><iframe src="../../simulator/?scenario=multiply-step&amp;embed=1" title="MultiplyStep" loading="lazy" allowfullscreen></iframe></div>

<a href="../../simulator/?scenario=multiply-step" class="md-button">Open the recorded circuit</a>
