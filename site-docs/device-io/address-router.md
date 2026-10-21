# AddressRouter32

Single-owner data-address router for RAM and device windows.

Catalog: `bus.address-router.32`. Structural and compact implementations are selectable. The compact form runs
this component's own four-state gate list with characterized storage and
same-circuit delay bounds. Fidelity and protocol validation is in progress.

## Pins

| Pin | Direction | Bits |
| --- | --- | --- |
| `BUTTONS_REQ_READY` | Input | 1 |
| `BUTTONS_RSP_READ_DATA` | Input | 32 |
| `BUTTONS_RSP_STATUS` | Input | 3 |
| `BUTTONS_RSP_VALID` | Input | 1 |
| `CLK` | Input | 1 |
| `CONSOLE_REQ_READY` | Input | 1 |
| `CONSOLE_RSP_READ_DATA` | Input | 32 |
| `CONSOLE_RSP_STATUS` | Input | 3 |
| `CONSOLE_RSP_VALID` | Input | 1 |
| `DISPLAY_REQ_READY` | Input | 1 |
| `DISPLAY_RSP_READ_DATA` | Input | 32 |
| `DISPLAY_RSP_STATUS` | Input | 3 |
| `DISPLAY_RSP_VALID` | Input | 1 |
| `RAM_REQ_READY` | Input | 1 |
| `RAM_RSP_READ_DATA` | Input | 32 |
| `RAM_RSP_STATUS` | Input | 3 |
| `RAM_RSP_VALID` | Input | 1 |
| `REQ_ADDR` | Input | 32 |
| `REQ_OP` | Input | 3 |
| `REQ_VALID` | Input | 1 |
| `REQ_WRITE_DATA` | Input | 32 |
| `REQ_WRITE_MASK` | Input | 4 |
| `RSP_READY` | Input | 1 |
| `RST` | Input | 1 |
| `TIMER_REQ_READY` | Input | 1 |
| `TIMER_RSP_READ_DATA` | Input | 32 |
| `TIMER_RSP_STATUS` | Input | 3 |
| `TIMER_RSP_VALID` | Input | 1 |
| `VIDEO_REQ_READY` | Input | 1 |
| `VIDEO_RSP_READ_DATA` | Input | 32 |
| `VIDEO_RSP_STATUS` | Input | 3 |
| `VIDEO_RSP_VALID` | Input | 1 |
| `BUTTONS_CLK` | Output | 1 |
| `BUTTONS_REQ_ADDR` | Output | 32 |
| `BUTTONS_REQ_OP` | Output | 3 |
| `BUTTONS_REQ_VALID` | Output | 1 |
| `BUTTONS_REQ_WRITE_DATA` | Output | 32 |
| `BUTTONS_REQ_WRITE_MASK` | Output | 4 |
| `BUTTONS_RSP_READY` | Output | 1 |
| `BUTTONS_RST` | Output | 1 |
| `CONSOLE_CLK` | Output | 1 |
| `CONSOLE_REQ_ADDR` | Output | 32 |
| `CONSOLE_REQ_OP` | Output | 3 |
| `CONSOLE_REQ_VALID` | Output | 1 |
| `CONSOLE_REQ_WRITE_DATA` | Output | 32 |
| `CONSOLE_REQ_WRITE_MASK` | Output | 4 |
| `CONSOLE_RSP_READY` | Output | 1 |
| `CONSOLE_RST` | Output | 1 |
| `DISPLAY_CLK` | Output | 1 |
| `DISPLAY_REQ_ADDR` | Output | 32 |
| `DISPLAY_REQ_OP` | Output | 3 |
| `DISPLAY_REQ_VALID` | Output | 1 |
| `DISPLAY_REQ_WRITE_DATA` | Output | 32 |
| `DISPLAY_REQ_WRITE_MASK` | Output | 4 |
| `DISPLAY_RSP_READY` | Output | 1 |
| `DISPLAY_RST` | Output | 1 |
| `RAM_CLK` | Output | 1 |
| `RAM_REQ_ADDR` | Output | 32 |
| `RAM_REQ_OP` | Output | 3 |
| `RAM_REQ_VALID` | Output | 1 |
| `RAM_REQ_WRITE_DATA` | Output | 32 |
| `RAM_REQ_WRITE_MASK` | Output | 4 |
| `RAM_RSP_READY` | Output | 1 |
| `RAM_RST` | Output | 1 |
| `REQ_READY` | Output | 1 |
| `RSP_READ_DATA` | Output | 32 |
| `RSP_STATUS` | Output | 3 |
| `RSP_VALID` | Output | 1 |
| `TIMER_CLK` | Output | 1 |
| `TIMER_REQ_ADDR` | Output | 32 |
| `TIMER_REQ_OP` | Output | 3 |
| `TIMER_REQ_VALID` | Output | 1 |
| `TIMER_REQ_WRITE_DATA` | Output | 32 |
| `TIMER_REQ_WRITE_MASK` | Output | 4 |
| `TIMER_RSP_READY` | Output | 1 |
| `TIMER_RST` | Output | 1 |
| `VIDEO_CLK` | Output | 1 |
| `VIDEO_REQ_ADDR` | Output | 32 |
| `VIDEO_REQ_OP` | Output | 3 |
| `VIDEO_REQ_VALID` | Output | 1 |
| `VIDEO_REQ_WRITE_DATA` | Output | 32 |
| `VIDEO_REQ_WRITE_MASK` | Output | 4 |
| `VIDEO_RSP_READY` | Output | 1 |
| `VIDEO_RST` | Output | 1 |

## Behavior

Accept and retain one request before issuing it to exactly one destination. Destination response ownership persists until the upstream response is accepted. Child addresses are offsets within the chosen window. READY depends on registered occupancy, not the current payload.

## Internal structure

Address comparisons, registered one-hot destination ownership, held payload/status and sent/busy state. Child clocks/resets pass through without clock gating. RAM is the only port intended to lead to caches; device/video ports bypass them.

## Edge cases and integration

Only aligned READ and WRITE are forwarded. Bursts/program/erase, unmapped addresses and invalid X/Z address/op fields return an error before a child request. There is no cancellation protocol after downstream acceptance; reset is a coordinated whole-bus reset.

## Recorded example and checks

`AddressRouter32Test` owns the independent functional checks; `AddressRouter32ScenarioTest` retains this smaller recorded example. Both use the same logical component owner.

<!-- simulator-examples: address-router32 AddressRouter32ScenarioTest -->

<div class="simulator-embed"><iframe src="../../simulator/?scenario=address-router32&amp;embed=1" title="AddressRouter32" loading="lazy" allowfullscreen></iframe></div>

<a href="../../simulator/?scenario=address-router32" class="md-button">Open the recorded circuit</a>

## Video aperture parameter

`video_bytes` selects a power-of-two aperture of 4–262144 bytes at
`0x20000000`, default 4096. The integrated system supplies its framebuffer
backing capacity. RAM and the four register windows keep their documented
addresses and policies. Addresses at or beyond the selected video aperture
fault before reaching the display; the router translates accepted addresses
to local byte offsets. Configured compact routers use their own structural
gates and timing bounds.
