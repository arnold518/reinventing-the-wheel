# `DeviceRegisterPort<W>`

A registered command frontend with stable sampled responses.

W is **8, 16 or 32**. The protocol uses 32-bit byte addresses, W-bit data
and W/8 byte-mask bits. The game platform selects W=32 for its MMIO ABI.

Catalog: `device.word.register-port.W`. Structural fidelity is available at every width. At W=32, compact fidelity executes the same four-state gates with characterized
register storage and same-circuit delay bounds.

## Pins

| Pin | Direction | Bits |
| --- | --- | --- |
| `BACK_READY` | Input | 1 |
| `BACK_READ_DATA` | Input | W |
| `BACK_STATUS` | Input | 3 |
| `CLK` | Input | 1 |
| `REQ_ADDR` | Input | 32 |
| `REQ_OP` | Input | 3 |
| `REQ_VALID` | Input | 1 |
| `REQ_WRITE_DATA` | Input | W |
| `REQ_WRITE_MASK` | Input | W/8 |
| `RSP_READY` | Input | 1 |
| `RST` | Input | 1 |
| `ADDRESS` | Output | 32 |
| `IS_READ` | Output | 1 |
| `IS_WRITE` | Output | 1 |
| `READ_STROBE` | Output | 1 |
| `REQ_READY` | Output | 1 |
| `RSP_READ_DATA` | Output | W |
| `RSP_STATUS` | Output | 3 |
| `RSP_VALID` | Output | 1 |
| `WRITE_DATA` | Output | W |
| `WRITE_MASK` | Output | W/8 |
| `WRITE_STROBE` | Output | 1 |

## Behavior

Accept one request into held fields. READ_STROBE or WRITE_STROBE identifies the backend execution edge, only when status is OK. BACK_READY gates nonzero writes; reads complete without waiting for it. BACK_READ_DATA/BACK_STATUS sample on that completion edge; the response is retained under upstream backpressure. ADDRESS, WRITE_DATA, WRITE_MASK and IS_READ/IS_WRITE describe the held request.

## Internal structure

Payload/status and pending/executed registers compose a structural
`MemoryResponseBuffer<W>`. `RequestKnownBits` supplies simulator X/Z metadata
checks; command execution follows the structural frontend gates.

## Edge cases and integration

READ ignores write data/mask. WRITE accepts mask 2^(W/8)−1; mask 0 has no backend write strobe and retains address/permission validation, and other nonzero masks return InvalidRequest. Unknown address/op or a required unknown mask is invalid. Unsupported operations and misalignment fail without side effects. READY never depends on current command fields or same-cycle RSP_READY.

## Recorded example and checks

`DeviceRegisterPortWidth32Test` owns the independent functional checks; `DeviceRegisterPortWidth32ScenarioTest` retains this smaller recorded example. Both use the same logical component owner. This recording selects W=32;
`WidthComponentsTest` exercises the other supported widths.

<!-- simulator-examples: device-register-port-w32 DeviceRegisterPortWidth32ScenarioTest -->

<div class="simulator-embed"><iframe src="../../simulator/?scenario=device-register-port-w32&amp;embed=1" title="DeviceRegisterPort<32>" loading="lazy" allowfullscreen></iframe></div>

<a href="../../simulator/?scenario=device-register-port-w32" class="md-button">Open the recorded circuit</a>
