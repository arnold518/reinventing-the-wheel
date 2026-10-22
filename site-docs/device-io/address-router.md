# `AddressRouter<W>`

A single-owner address router for RAM and device windows.

W is **8, 16 or 32**. The protocol uses 32-bit byte addresses, W-bit data
and W/8 byte-mask bits. The game platform selects W=32 for its MMIO ABI.

Catalog: `bus.word.address-router.W`. Structural fidelity is available at every width. At W=32, compact fidelity executes the same four-state gates with characterized
register storage and same-circuit delay bounds.

## Pins

| Pin | Direction | Bits |
| --- | --- | --- |
| `BUTTONS_REQ_READY` | Input | 1 |
| `BUTTONS_RSP_READ_DATA` | Input | W |
| `BUTTONS_RSP_STATUS` | Input | 3 |
| `BUTTONS_RSP_VALID` | Input | 1 |
| `CLK` | Input | 1 |
| `CONSOLE_REQ_READY` | Input | 1 |
| `CONSOLE_RSP_READ_DATA` | Input | W |
| `CONSOLE_RSP_STATUS` | Input | 3 |
| `CONSOLE_RSP_VALID` | Input | 1 |
| `DISPLAY_REQ_READY` | Input | 1 |
| `DISPLAY_RSP_READ_DATA` | Input | W |
| `DISPLAY_RSP_STATUS` | Input | 3 |
| `DISPLAY_RSP_VALID` | Input | 1 |
| `RAM_REQ_READY` | Input | 1 |
| `RAM_RSP_READ_DATA` | Input | W |
| `RAM_RSP_STATUS` | Input | 3 |
| `RAM_RSP_VALID` | Input | 1 |
| `REQ_ADDR` | Input | 32 |
| `REQ_OP` | Input | 3 |
| `REQ_VALID` | Input | 1 |
| `REQ_WRITE_DATA` | Input | W |
| `REQ_WRITE_MASK` | Input | W/8 |
| `RSP_READY` | Input | 1 |
| `RST` | Input | 1 |
| `TIMER_REQ_READY` | Input | 1 |
| `TIMER_RSP_READ_DATA` | Input | W |
| `TIMER_RSP_STATUS` | Input | 3 |
| `TIMER_RSP_VALID` | Input | 1 |
| `VIDEO_REQ_READY` | Input | 1 |
| `VIDEO_RSP_READ_DATA` | Input | W |
| `VIDEO_RSP_STATUS` | Input | 3 |
| `VIDEO_RSP_VALID` | Input | 1 |
| `BUTTONS_CLK` | Output | 1 |
| `BUTTONS_REQ_ADDR` | Output | 32 |
| `BUTTONS_REQ_OP` | Output | 3 |
| `BUTTONS_REQ_VALID` | Output | 1 |
| `BUTTONS_REQ_WRITE_DATA` | Output | W |
| `BUTTONS_REQ_WRITE_MASK` | Output | W/8 |
| `BUTTONS_RSP_READY` | Output | 1 |
| `BUTTONS_RST` | Output | 1 |
| `CONSOLE_CLK` | Output | 1 |
| `CONSOLE_REQ_ADDR` | Output | 32 |
| `CONSOLE_REQ_OP` | Output | 3 |
| `CONSOLE_REQ_VALID` | Output | 1 |
| `CONSOLE_REQ_WRITE_DATA` | Output | W |
| `CONSOLE_REQ_WRITE_MASK` | Output | W/8 |
| `CONSOLE_RSP_READY` | Output | 1 |
| `CONSOLE_RST` | Output | 1 |
| `DISPLAY_CLK` | Output | 1 |
| `DISPLAY_REQ_ADDR` | Output | 32 |
| `DISPLAY_REQ_OP` | Output | 3 |
| `DISPLAY_REQ_VALID` | Output | 1 |
| `DISPLAY_REQ_WRITE_DATA` | Output | W |
| `DISPLAY_REQ_WRITE_MASK` | Output | W/8 |
| `DISPLAY_RSP_READY` | Output | 1 |
| `DISPLAY_RST` | Output | 1 |
| `RAM_CLK` | Output | 1 |
| `RAM_REQ_ADDR` | Output | 32 |
| `RAM_REQ_OP` | Output | 3 |
| `RAM_REQ_VALID` | Output | 1 |
| `RAM_REQ_WRITE_DATA` | Output | W |
| `RAM_REQ_WRITE_MASK` | Output | W/8 |
| `RAM_RSP_READY` | Output | 1 |
| `RAM_RST` | Output | 1 |
| `REQ_READY` | Output | 1 |
| `RSP_READ_DATA` | Output | W |
| `RSP_STATUS` | Output | 3 |
| `RSP_VALID` | Output | 1 |
| `TIMER_CLK` | Output | 1 |
| `TIMER_REQ_ADDR` | Output | 32 |
| `TIMER_REQ_OP` | Output | 3 |
| `TIMER_REQ_VALID` | Output | 1 |
| `TIMER_REQ_WRITE_DATA` | Output | W |
| `TIMER_REQ_WRITE_MASK` | Output | W/8 |
| `TIMER_RSP_READY` | Output | 1 |
| `TIMER_RST` | Output | 1 |
| `VIDEO_CLK` | Output | 1 |
| `VIDEO_REQ_ADDR` | Output | 32 |
| `VIDEO_REQ_OP` | Output | 3 |
| `VIDEO_REQ_VALID` | Output | 1 |
| `VIDEO_REQ_WRITE_DATA` | Output | W |
| `VIDEO_REQ_WRITE_MASK` | Output | W/8 |
| `VIDEO_RSP_READY` | Output | 1 |
| `VIDEO_RST` | Output | 1 |

## Behavior

Accept and retain one request before issuing it to exactly one destination. Destination response ownership persists until the upstream response is accepted. Child addresses are offsets within the chosen window. READY depends on registered occupancy, not the current payload.

## Internal structure

Address comparisons, registered one-hot destination ownership, held payload/status and sent/busy state. Child clocks/resets pass through without clock gating. RAM is the only port intended to lead to caches; device/video ports bypass them.

## Edge cases and integration

Only aligned READ and WRITE are forwarded. Bursts/program/erase, unmapped addresses and invalid X/Z address/op fields return an error before a child request. There is no cancellation protocol after downstream acceptance; reset is a coordinated whole-bus reset.

## Recorded example and checks

`AddressRouterWidth32Test` owns the independent functional checks; `AddressRouterWidth32ScenarioTest` retains this smaller recorded example. Both use the same logical component owner. This recording selects W=32;
`WidthComponentsTest` exercises the other supported widths.

<!-- simulator-examples: address-router-w32 AddressRouterWidth32ScenarioTest -->

<div class="simulator-embed"><iframe src="../../simulator/?scenario=address-router-w32&amp;embed=1" title="AddressRouter<32>" loading="lazy" allowfullscreen></iframe></div>

<a href="../../simulator/?scenario=address-router-w32" class="md-button">Open the recorded circuit</a>

## Address-map parameters

`video_bytes` selects a power-of-two aperture of 4–262144 bytes, default
4096. The integrated system supplies its framebuffer backing capacity.
The original default base remains `0x20000000`.

The six named windows also accept `<region>_base` and `<region>_bytes`,
where region is `ram`, `console`, `timer`, `buttons`, `display` or `video`.
Values are unsigned decimal strings. Each range must be aligned to its
power-of-two size, fit the 32-bit byte address space and not overlap another
range. Prefixes, ownership and offset translation are unchanged. Unsupported
keys, overlaps and invalid ranges are rejected before circuit construction.

Configured compact routers use the gates and timing of their own map. The
unchanged default map is used by `RV32IMDeviceSystem`; changing a standalone
router does not relocate the guest's MMIO ABI or the machine's RAM loader.

`AddressRouter<W>` also supports eight- and 16-bit data. Addresses stay 32
bits and masks have W/8 bits. New narrow variants are structural. See the
[word-component reference](../components/word-components.md).
