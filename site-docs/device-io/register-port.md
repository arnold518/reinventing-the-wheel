# DeviceRegisterPort32

Registered command frontend with stable sampled responses.

Catalog: `device.register-port.32`. Structural and compact implementations are selectable. The compact form runs
this component's own four-state gate list with characterized storage and
same-circuit delay bounds. Fidelity and protocol validation is in progress.

## Pins

| Pin | Direction | Bits |
| --- | --- | --- |
| `BACK_READY` | Input | 1 |
| `BACK_READ_DATA` | Input | 32 |
| `BACK_STATUS` | Input | 3 |
| `CLK` | Input | 1 |
| `REQ_ADDR` | Input | 32 |
| `REQ_OP` | Input | 3 |
| `REQ_VALID` | Input | 1 |
| `REQ_WRITE_DATA` | Input | 32 |
| `REQ_WRITE_MASK` | Input | 4 |
| `RSP_READY` | Input | 1 |
| `RST` | Input | 1 |
| `ADDRESS` | Output | 32 |
| `IS_READ` | Output | 1 |
| `IS_WRITE` | Output | 1 |
| `READ_STROBE` | Output | 1 |
| `REQ_READY` | Output | 1 |
| `RSP_READ_DATA` | Output | 32 |
| `RSP_STATUS` | Output | 3 |
| `RSP_VALID` | Output | 1 |
| `WRITE_DATA` | Output | 32 |
| `WRITE_MASK` | Output | 4 |
| `WRITE_STROBE` | Output | 1 |

## Behavior

Accept one request into held fields. READ_STROBE or WRITE_STROBE identifies the backend execution edge, only when status is OK. BACK_READY gates nonzero writes; reads complete without waiting for it. BACK_READ_DATA/BACK_STATUS sample on that completion edge; the response is retained under upstream backpressure. ADDRESS, WRITE_DATA, WRITE_MASK and IS_READ/IS_WRITE describe the held request.

## Internal structure

Payload/status and pending/executed registers plus the existing structural MemoryResponseBuffer. Existing RequestKnownBits supplies simulator X/Z metadata checks. No new behavioral peripheral evaluates commands.

## Edge cases and integration

READ ignores write data/mask. WRITE accepts mask 15; mask 0 has no backend write strobe and retains address/permission validation, and other nonzero masks return InvalidRequest. Unknown address/op or a required unknown mask is invalid. Unsupported operations and misalignment fail without side effects. READY never depends on current command fields or same-cycle RSP_READY.

## Recorded example and checks

`DeviceRegisterPort32Test` owns the independent functional checks; `DeviceRegisterPort32ScenarioTest` retains this smaller recorded example. Both use the same logical component owner.

<!-- simulator-examples: device-register-port32 DeviceRegisterPort32ScenarioTest -->

<div class="simulator-embed"><iframe src="../../simulator/?scenario=device-register-port32&amp;embed=1" title="DeviceRegisterPort32" loading="lazy" allowfullscreen></iframe></div>

<a href="../../simulator/?scenario=device-register-port32" class="md-button">Open the recorded circuit</a>
