# § 5.4.6. Display

A screen: pixel memory the program draws into, and a register to show a
finished frame. Pixels are one byte each in RGB332 (3 bits red, 3 green,
2 blue).

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| `FramebufferDisplay` | Pixel memory on its own port, six control registers | Yes | Yes |

Catalog: `device.framebuffer`.

| Parameter | Default | Values |
| --- | --- | --- |
| `width` | 4 | 1–4096 pixels |
| `height` | 4 | 1–4096 pixels, with width × height ≤ 262144 |
| `storage_bytes` | 0 | 0: the next power of two covering the surface; otherwise a power of two in 4–262144 covering it |

## IO pins

| Pin | Direction | Width | Meaning |
| --- | --- | ---: | --- |
| `CLK`, `RST` | Input | 1 each | Clock; reset of the device's state |
| `REQ_VALID`, `REQ_OP`, `REQ_ADDR`, `REQ_WRITE_DATA`, `REQ_WRITE_MASK`, `RSP_READY` | Input | 1, 3, 32, 32, 4, 1 | Register requests (offsets in the device's window) |
| `REQ_READY`, `RSP_VALID`, `RSP_READ_DATA`, `RSP_STATUS` | Output | 1, 1, 32, 3 | Register answers |
| `FB_REQ_VALID`, `FB_REQ_OP`, `FB_REQ_ADDR`, `FB_REQ_WRITE_DATA`, `FB_REQ_WRITE_MASK`, `FB_RSP_READY` | Input | 1, 3, 32, 32, 4, 1 | Pixel memory requests (byte masks allowed) |
| `FB_REQ_READY`, `FB_RSP_VALID`, `FB_RSP_READ_DATA`, `FB_RSP_STATUS` | Output | 1, 1, 32, 3 | Pixel memory answers |
| `FB_CLK`, `FB_RST` | Input | 1 each | Pixel memory clock and reset (the system clock) |
| `PRESENT` | Output | 1 | A frame is presented at this edge |
| `FRAME_SEQUENCE` | Output | 32 | Frames presented since reset |

## Registers

| Offset | Register | Access | Bits | Meaning |
| --- | --- | --- | ---: | --- |
| `0x00` | `WIDTH` | setting | 32 | Visible pixels per row |
| `0x04` | `HEIGHT` | setting | 32 | Visible rows |
| `0x08` | `STRIDE` | setting | 32 | Bytes from one row to the next (= width) |
| `0x0C` | `FORMAT` | setting | 32 | 1: RGB332 |
| `0x10` | `FRAME_SEQUENCE` | read-only | 32 | Frames presented |
| `0x14` | `PRESENT` | command | 1 | Show the current pixel memory as a new frame |

## Behavior

The pixel at (x, y) is byte `y × STRIDE + x` of pixel memory, which the
machine places at `VIDEO_BASE`. Drawing changes memory only; writing 1 to
`PRESENT` marks a finished frame: `FRAME_SEQUENCE` increments and the
recording captures the settled pixels. Reset clears the control state and
keeps the pixels.

`PRESENT` is the qualified command strobe, not a host refresh timer. Writing
0 does not present; writing a value other than 0 or 1 is Invalid. Each
successful command increments the sequence once modulo 2^32, even if no
pixel changed. There is one pixel store, with no back buffer, scanout engine
or automatic vertical refresh. Capture observes that store when the command
is recorded.

For RGB332, bits 7..5 are red, 4..2 green and 1..0 blue. Within a little-endian
32-bit memory word, mask bit n updates byte n; an aligned write with all
four mask bits updates four consecutive pixels. Storage padding beyond
`width * height` is addressable but is not displayed. Access past storage
capacity returns Range; the control-register port instead requires a full
or empty mask. Standalone callers must drive `FB_CLK`/`FB_RST` as well as
`CLK`/`RST`; `ComputerSystem` connects both to its one clock and reset.

```c
#include "platform.h"

void draw_pixel_example(uint32_t x, uint32_t y, uint8_t color) {
    if (x >= io_read(DISPLAY_WIDTH) || y >= io_read(DISPLAY_HEIGHT)) return;
    const uint32_t stride = io_read(DISPLAY_STRIDE);
    io_write8(VIDEO_BASE + y * stride + x, color);
    present(); // platform helper: fence, PRESENT = 1, fence
}
```

## Implementation

The structural form shows the grouped circuit. The compact form runs
compiled versions of the same groups with characterized storage, register and
queue models; control delays come from the structural groups. Contract tests
compare both forms; the `visual` preset selects the compact form.

| Part | Contents |
| --- | --- |
| `PORT` | `DeviceRegisterPort<32>` for the control registers |
| `REGISTERS` | The front end; the geometry and format are constants |
| `FRAME_COUNTER` | A `Register<32>` and an adder, stepped by `PRESENT_WRITE` |
| `PIXELS` | A flip-flop memory endpoint ([§ 3](../../memory/technologies/flip-flop.md)); the scalable native device above 512 bytes |

**Timing.** Register execution, frame-count storage and pixel-memory
transactions use their respective gate-proven timing; the composed display
does not add a whole-device delay. A PRESENT snapshot is a settled observation,
not proof that an earlier unacknowledged pixel write completed. Software
orders writes before the command. Register payloads with unknown bits are
rejected for nonzero-mask commands; unknown pixel values remain observable
as unknown data rather than being replaced with a guessed color. Binary
geometry and clock/handshake inputs after reset are the game-run boundary.

## Visual abstraction

A compact or collapsed display is drawn as its pixel surface at the
selected time; selecting it opens a preview in the inspector. Scrubbing reads
recorded pixel history and never runs the program.

## Examples

The scenario reads the four settings of a 4 × 4 display, then draws a
picture one word (one row of four pixels) at a time: a red frame around a
green middle. `present` shows it as frame 1. A byte-masked write then turns
the pixel at (1, 1) blue without touching the rest of its row, and frame 2
is presented. An access past the 16-byte pixel store is refused (Range), as
is a write to the read-only `WIDTH` setting.

<!-- simulator-examples: framebuffer-display FramebufferDisplayScenarioTest -->

<div class="simulator-embed">
  <iframe src="../../../simulator/?scenario=framebuffer-display&amp;embed=1" title="FramebufferDisplay simulation" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../simulator/?scenario=framebuffer-display" class="md-button">Open the full visualizer</a>

## Source and tests

[Parameters and bounds](https://github.com/arnold518/reinventing-the-wheel/blob/alpha/include/modules/device/DeviceConfiguration.hpp),
[display and memory wiring](https://github.com/arnold518/reinventing-the-wheel/blob/alpha/src/modules/device/DeviceDisplay.cpp),
[register definitions](https://github.com/arnold518/reinventing-the-wheel/blob/alpha/src/modules/device/DeviceRegisters.cpp),
[verification](https://github.com/arnold518/reinventing-the-wheel/blob/alpha/src/tests/RV32IMPhase3Tests.cpp).

| Test | Coverage |
| --- | --- |
| `FramebufferDisplayTest` | Geometry registers and parameters, storage sizes, pixel reads and writes, present and sequence, 64×48, 128×96, 320×200 and 97×65 surfaces (compact form) |
| `FramebufferDisplayScenarioTest` | The recorded example, structural and compact forms compared |
