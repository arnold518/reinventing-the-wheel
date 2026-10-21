# ClockedCounter

Enabled virtual-clock counter, widths 4 and 32.

Catalog: `device.counter.4`. Structural and compact implementations are selectable. The compact form runs
this component's own four-state gate list with characterized storage and
same-circuit delay bounds. Fidelity and protocol validation is in progress.

Width variants use the corresponding catalog suffix; bus widths below show the recorded small form.

## Pins

| Pin | Direction | Bits |
| --- | --- | --- |
| `CLK` | Input | 1 |
| `ENABLE` | Input | 1 |
| `RST` | Input | 1 |
| `COUNT` | Output | 4 |
| `TICK` | Output | 1 |

## Behavior

COUNT increments on every divisor-th enabled rising CLK edge and wraps at its width. TICK is the enabled terminal phase before the increment edge. RST clears count and divider state. ENABLE=0 retains both.

## Internal structure

Registers, adders, a phase comparison and enable gates. Catalog parameter divisor is a positive uint32 decimal value, default 1.

## Edge cases and integration

TICK is an enable indication, not an independently generated clock. No clock domain, wall-clock source or oscillator is introduced. Unknown controls retain the constituent gate/register behavior.

## Recorded example and checks

`ClockedCounterTest` owns the independent functional checks; `ClockedCounterScenarioTest` retains this smaller recorded example. Both use the same logical component owner.

<!-- simulator-examples: clocked-counter ClockedCounterScenarioTest -->

<div class="simulator-embed"><iframe src="../../simulator/?scenario=clocked-counter&amp;embed=1" title="ClockedCounter" loading="lazy" allowfullscreen></iframe></div>

<a href="../../simulator/?scenario=clocked-counter" class="md-button">Open the recorded circuit</a>
