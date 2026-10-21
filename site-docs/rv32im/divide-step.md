# DivideStep

One restoring-division step. Supported widths are 4 and 32 bits.

Catalog: `arithmetic.divide-step.4`. The implemented form is structural; selectable lower-level children may be compact. No new whole-component behavioral form is registered.

Width variants use the corresponding catalog suffix; bus widths below show the recorded small form.

## Pins

| Pin | Direction | Bits |
| --- | --- | --- |
| `BIT` | Input | 1 |
| `DIVISOR` | Input | 4 |
| `REMAINDER` | Input | 4 |
| `NEXT_REMAINDER` | Output | 4 |
| `QUOTIENT_BIT` | Output | 1 |

## Behavior

Shift `REMAINDER` left, append `BIT`, compare the resulting W+1-bit quantity with `DIVISOR`, and subtract when greater or equal. `QUOTIENT_BIT` reports the decision; `NEXT_REMAINDER` contains the low W result.

## Internal structure

The original remainder high bit and the subtractor carry represent the extra comparison bit. Supported pin widths suffice; no 33-bit pin is introduced.

## Edge cases and integration

All binary input combinations are defined, including divisor zero. A full divider establishes the usual remainder invariant; the standalone step does not assume it. Unknown inputs follow the gates.

## Recorded example and checks

`DivideStepTest` owns the independent functional checks; `DivideStepScenarioTest` retains this smaller recorded example. Both use the same logical component owner.

<!-- simulator-examples: divide-step DivideStepScenarioTest -->

<div class="simulator-embed"><iframe src="../../simulator/?scenario=divide-step&amp;embed=1" title="DivideStep" loading="lazy" allowfullscreen></iframe></div>

<a href="../../simulator/?scenario=divide-step" class="md-button">Open the recorded circuit</a>
