# `InstructionAccessGuard<W>`

A read-only low-RAM fetch guard.

W is **8, 16 or 32**. The protocol uses 32-bit byte addresses, W-bit data
and W/8 byte-mask bits. The game platform selects W=32 for its MMIO ABI.

Catalog: `bus.word.instruction-guard.W`. Structural fidelity is available at every width. At W=32, compact fidelity executes the same four-state gates with characterized
register storage and same-circuit delay bounds.

## Pins

| Pin | Direction | Bits |
| --- | --- | --- |
| `CLK` | Input | 1 |
| `MEM_REQ_READY` | Input | 1 |
| `MEM_RSP_READ_DATA` | Input | W |
| `MEM_RSP_STATUS` | Input | 3 |
| `MEM_RSP_VALID` | Input | 1 |
| `REQ_ADDR` | Input | 32 |
| `REQ_OP` | Input | 3 |
| `REQ_VALID` | Input | 1 |
| `REQ_WRITE_DATA` | Input | W |
| `REQ_WRITE_MASK` | Input | W/8 |
| `RSP_READY` | Input | 1 |
| `RST` | Input | 1 |
| `MEM_CLK` | Output | 1 |
| `MEM_REQ_ADDR` | Output | 32 |
| `MEM_REQ_OP` | Output | 3 |
| `MEM_REQ_VALID` | Output | 1 |
| `MEM_REQ_WRITE_DATA` | Output | W |
| `MEM_REQ_WRITE_MASK` | Output | W/8 |
| `MEM_RSP_READY` | Output | 1 |
| `MEM_RST` | Output | 1 |
| `REQ_READY` | Output | 1 |
| `RSP_READ_DATA` | Output | W |
| `RSP_STATUS` | Output | 3 |
| `RSP_VALID` | Output | 1 |

## Behavior

Only aligned READ requests within [0,0x40000) reach MEM_. All device/video/unmapped instruction addresses return Range. Writes and other operation encodings return Unsupported. No device request port exists.

## Internal structure

The same structural registered routing assembly as AddressRouter<32>, with one read-only RAM destination.

## Edge cases and integration

Place before any instruction-side arbiter or cache. This prevents speculative fetches from reaching devices; it does not implement a general privilege system. Outstanding RAM responses remain owned until consumed.

## Recorded example and checks

`InstructionAccessGuardWidth32Test` owns the independent functional checks; `InstructionAccessGuardWidth32ScenarioTest` retains this smaller recorded example. Both use the same logical component owner. This recording selects W=32;
`WidthComponentsTest` exercises the other supported widths.

<!-- simulator-examples: instruction-access-guard-w32 InstructionAccessGuardWidth32ScenarioTest -->

<div class="simulator-embed"><iframe src="../../simulator/?scenario=instruction-access-guard-w32&amp;embed=1" title="InstructionAccessGuard<32>" loading="lazy" allowfullscreen></iframe></div>

<a href="../../simulator/?scenario=instruction-access-guard-w32" class="md-button">Open the recorded circuit</a>
