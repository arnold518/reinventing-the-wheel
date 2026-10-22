# `BusErrorTarget<W>`

A protocol target for a deliberately absent address region.

W is **8, 16 or 32**. The protocol uses 32-bit byte addresses, W-bit data
and W/8 byte-mask bits. The game platform selects W=32 for its MMIO ABI.

Catalog: `bus.word.error-target.W`. Structural fidelity is available at every width. There is no whole-component behavioral form.

## Pins

| Pin | Direction | Bits |
| --- | --- | --- |
| `CLK` | Input | 1 |
| `REQ_ADDR` | Input | 32 |
| `REQ_OP` | Input | 3 |
| `REQ_VALID` | Input | 1 |
| `REQ_WRITE_DATA` | Input | W |
| `REQ_WRITE_MASK` | Input | W/8 |
| `RSP_READY` | Input | 1 |
| `RST` | Input | 1 |
| `REQ_READY` | Output | 1 |
| `RSP_READ_DATA` | Output | W |
| `RSP_STATUS` | Output | 3 |
| `RSP_VALID` | Output | 1 |

## Behavior

Uses `DeviceRegisterPort<W>` and returns Range (2) for otherwise valid requests. Normal frontend validation can return Alignment, Unsupported or InvalidRequest first. No storage or side effects exist.

## Internal structure

A structural device register frontend with constant backend status/data and ready.

## Edge cases and integration

Responses are retained under backpressure. Reset drops pending ownership and responses. Do not connect a target that might still complete an old request across a partial reset.

## Recorded example and checks

`BusErrorTargetWidth32Test` owns the independent functional checks; `BusErrorTargetWidth32ScenarioTest` retains this smaller recorded example. Both use the same logical component owner. This recording selects W=32;
`WidthComponentsTest` exercises the other supported widths.

<!-- simulator-examples: bus-error-target-w32 BusErrorTargetWidth32ScenarioTest -->

<div class="simulator-embed"><iframe src="../../simulator/?scenario=bus-error-target-w32&amp;embed=1" title="BusErrorTarget<32>" loading="lazy" allowfullscreen></iframe></div>

<a href="../../simulator/?scenario=bus-error-target-w32" class="md-button">Open the recorded circuit</a>
