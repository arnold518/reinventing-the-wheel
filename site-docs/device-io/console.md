# ConsoleDevice32

Buffered byte output device.

Catalog: `device.console.32`. The implemented form is structural; selectable lower-level children may be compact. No new whole-component behavioral form is registered.

## Parameters

`fifo_depth` accepts 1–7 entries, default 4. It configures the existing
structural FIFO child; register offsets and pin widths stay the same.
Occupancy, backpressure and overflow use the selected capacity.

## Pins

| Pin | Direction | Bits |
| --- | --- | --- |
| `CLK` | Input | 1 |
| `REQ_ADDR` | Input | 32 |
| `REQ_OP` | Input | 3 |
| `REQ_VALID` | Input | 1 |
| `REQ_WRITE_DATA` | Input | 32 |
| `REQ_WRITE_MASK` | Input | 4 |
| `RSP_READY` | Input | 1 |
| `RST` | Input | 1 |
| `TX_READY` | Input | 1 |
| `REQ_READY` | Output | 1 |
| `RSP_READ_DATA` | Output | 32 |
| `RSP_STATUS` | Output | 3 |
| `RSP_VALID` | Output | 1 |
| `TX_DATA` | Output | 8 |
| `TX_VALID` | Output | 1 |

## Behavior

Local registers: 0 TX (write-only byte in bits 7:0), 4 STATUS (read-only bit0=room), 8 COUNT (read-only). TX_VALID/TX_DATA offer the FIFO head to a sink; TX_READY consumes it at a rising edge. Full FIFO retains an accepted TX write until room exists.

## Internal structure

DeviceRegisterPort32 plus SynchronousFifo<8>. The sink acceptance policy is supplied externally and can be predetermined.

## Edge cases and integration

Nonzero bits above 7 or X/Z command data are invalid. Command writes require full mask or zero-mask no-op. Reads have no pop side effect. A full accepted TX blocks later requests until complete; software should poll STATUS before TX when it requires progress without sink acceptance. No serial RX, baud clock or host stdout side effect is implied.

## Recorded example and checks

`ConsoleDevice32Test` owns the independent functional checks; `ConsoleDevice32ScenarioTest` retains this smaller recorded example. Both use the same logical component owner.

<!-- simulator-examples: console-device32 ConsoleDevice32ScenarioTest -->

<div class="simulator-embed"><iframe src="../../simulator/?scenario=console-device32&amp;embed=1" title="ConsoleDevice32" loading="lazy" allowfullscreen></iframe></div>

<a href="../../simulator/?scenario=console-device32" class="md-button">Open the recorded circuit</a>
