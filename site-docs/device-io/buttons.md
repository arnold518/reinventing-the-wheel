# ButtonDevice

Button levels and an explicit-pop event queue.

Catalog: `device.buttons`. The implemented form is structural; selectable lower-level children may be compact. No whole-component behavioral form is registered.

## Parameters

`fifo_depth` accepts 1–7 entries, default 4. It configures the existing
structural FIFO child; register offsets and pin widths stay the same.
Occupancy, backpressure and overflow use the selected capacity.

## Pins

| Pin | Direction | Bits |
| --- | --- | --- |
| `CLK` | Input | 1 |
| `INPUT_BUTTON` | Input | 5 |
| `INPUT_DOWN` | Input | 1 |
| `INPUT_VALID` | Input | 1 |
| `REQ_ADDR` | Input | 32 |
| `REQ_OP` | Input | 3 |
| `REQ_VALID` | Input | 1 |
| `REQ_WRITE_DATA` | Input | 32 |
| `REQ_WRITE_MASK` | Input | 4 |
| `RSP_READY` | Input | 1 |
| `RST` | Input | 1 |
| `REQ_READY` | Output | 1 |
| `RSP_READ_DATA` | Output | 32 |
| `RSP_STATUS` | Output | 3 |
| `RSP_VALID` | Output | 1 |

## Behavior

At each rising edge with INPUT_VALID, update one of 32 button levels from INPUT_BUTTON/INPUT_DOWN and enqueue an event. Registers: 0 LEVELS; 4 HEAD (button bits0..4, down bit8); 8 COUNT; 12 STATUS (valid bit0, overflow bit1); 16 POP command; 20 CLEAR_OVERFLOW command. Commands act only when bit0=1.

## Internal structure

DeviceRegisterPort<32>, a 32-bit level register and SynchronousFifo<32>. Predetermined pin transitions use the same boundary intended for later live input.

## Edge cases and integration

HEAD reads never pop. A press and release between guest polls remain queued. Overflow may drop events but still updates levels. POP at empty is a no-op. Reserved/unknown command bits are invalid. Full simultaneous input/pop follows the FIFO drop policy; reset clears levels, events and overflow.

## Recorded example and checks

`ButtonDeviceTest` owns the independent functional checks; `ButtonDeviceScenarioTest` retains this smaller recorded example. Both use the same logical component owner.

<!-- simulator-examples: button-device ButtonDeviceScenarioTest -->

<div class="simulator-embed"><iframe src="../../simulator/?scenario=button-device&amp;embed=1" title="ButtonDevice" loading="lazy" allowfullscreen></iframe></div>

<a href="../../simulator/?scenario=button-device" class="md-button">Open the recorded circuit</a>

## Visual abstraction

Device views shows all 32 logical input IDs as keys: held, released or unknown.
The inspector also shows queue occupancy, retained head event and sticky
overflow at the selected recorded time. Viewing and scrubbing inject no input.
The game platform assigns names such as Up/Right/Action to these IDs; the
device itself accepts logical IDs and does not depend on a browser key map.
