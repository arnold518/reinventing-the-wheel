# TimerDevice

Read-only simulation-clock timer.

Catalog: `device.timer`. The implemented form is structural; selectable lower-level children may be compact. No whole-component behavioral form is registered.

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
| `REQ_READY` | Output | 1 |
| `RSP_READ_DATA` | Output | 32 |
| `RSP_STATUS` | Output | 3 |
| `RSP_VALID` | Output | 1 |

## Behavior

Local registers: 0 COUNT, 4 DIVISOR, 8 FREQUENCY_HZ. Counter ticks every divisor global rising edges. Reads sample the register at the backend execution edge, then hold that value even while time advances or response consumption is delayed.

## Internal structure

DeviceRegisterPort<32> and ClockedCounter<32>. Catalog parameters divisor (positive, default 1) and frequency_hz (uint32, default 0).

## Edge cases and integration

Zero frequency means the configuration has not declared a tick rate; it is never inferred from host speed. The caller must supply the actual tick rate when known. Writes to mapped read-only registers return ReadOnly. No interrupt or deadline comparator exists yet.

## Recorded example and checks

`TimerDeviceTest` owns the independent functional checks; `TimerDeviceScenarioTest` retains this smaller recorded example. Both use the same logical component owner.

<!-- simulator-examples: timer-device TimerDeviceScenarioTest -->

<div class="simulator-embed"><iframe src="../../simulator/?scenario=timer-device&amp;embed=1" title="TimerDevice" loading="lazy" allowfullscreen></iframe></div>

<a href="../../simulator/?scenario=timer-device" class="md-button">Open the recorded circuit</a>
