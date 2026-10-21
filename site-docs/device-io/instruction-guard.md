# InstructionAccessGuard32

Fetch-side low-RAM guard.

Catalog: `bus.instruction-guard.32`. Structural and compact implementations are selectable. The compact form runs
this component's own four-state gate list with characterized storage and
same-circuit delay bounds. Fidelity and protocol validation is in progress.

## Pins

| Pin | Direction | Bits |
| --- | --- | --- |
| `CLK` | Input | 1 |
| `MEM_REQ_READY` | Input | 1 |
| `MEM_RSP_READ_DATA` | Input | 32 |
| `MEM_RSP_STATUS` | Input | 3 |
| `MEM_RSP_VALID` | Input | 1 |
| `REQ_ADDR` | Input | 32 |
| `REQ_OP` | Input | 3 |
| `REQ_VALID` | Input | 1 |
| `REQ_WRITE_DATA` | Input | 32 |
| `REQ_WRITE_MASK` | Input | 4 |
| `RSP_READY` | Input | 1 |
| `RST` | Input | 1 |
| `MEM_CLK` | Output | 1 |
| `MEM_REQ_ADDR` | Output | 32 |
| `MEM_REQ_OP` | Output | 3 |
| `MEM_REQ_VALID` | Output | 1 |
| `MEM_REQ_WRITE_DATA` | Output | 32 |
| `MEM_REQ_WRITE_MASK` | Output | 4 |
| `MEM_RSP_READY` | Output | 1 |
| `MEM_RST` | Output | 1 |
| `REQ_READY` | Output | 1 |
| `RSP_READ_DATA` | Output | 32 |
| `RSP_STATUS` | Output | 3 |
| `RSP_VALID` | Output | 1 |

## Behavior

Only aligned READ requests within [0,0x40000) reach MEM_. All device/video/unmapped instruction addresses return Range. Writes and other operation encodings return Unsupported. No device request port exists.

## Internal structure

The same structural registered routing assembly as AddressRouter32, with one read-only RAM destination.

## Edge cases and integration

Place before any instruction-side arbiter or cache. This prevents speculative fetches from reaching devices; it does not implement a general privilege system. Outstanding RAM responses remain owned until consumed.

## Recorded example and checks

`InstructionAccessGuard32Test` owns the independent functional checks; `InstructionAccessGuard32ScenarioTest` retains this smaller recorded example. Both use the same logical component owner.

<!-- simulator-examples: instruction-access-guard32 InstructionAccessGuard32ScenarioTest -->

<div class="simulator-embed"><iframe src="../../simulator/?scenario=instruction-access-guard32&amp;embed=1" title="InstructionAccessGuard32" loading="lazy" allowfullscreen></iframe></div>

<a href="../../simulator/?scenario=instruction-access-guard32" class="md-button">Open the recorded circuit</a>
