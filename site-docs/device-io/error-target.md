# BusErrorTarget32

Protocol target for a deliberately absent address region.

Catalog: `bus.error-target.32`. The implemented form is structural; selectable lower-level children may be compact. No new whole-component behavioral form is registered.

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

Uses DeviceRegisterPort32 and returns Range (2) for otherwise valid requests. Normal frontend validation can return Alignment, Unsupported or InvalidRequest first. No storage or side effects exist.

## Internal structure

A structural device register frontend with constant backend status/data and ready.

## Edge cases and integration

Responses are retained under backpressure. Reset drops pending ownership and responses. Do not connect a target that might still complete an old request across a partial reset.

## Recorded example and checks

`BusErrorTarget32Test` owns the independent functional checks; `BusErrorTarget32ScenarioTest` retains this smaller recorded example. Both use the same logical component owner.

<!-- simulator-examples: bus-error-target32 BusErrorTarget32ScenarioTest -->

<div class="simulator-embed"><iframe src="../../simulator/?scenario=bus-error-target32&amp;embed=1" title="BusErrorTarget32" loading="lazy" allowfullscreen></iframe></div>

<a href="../../simulator/?scenario=bus-error-target32" class="md-button">Open the recorded circuit</a>
