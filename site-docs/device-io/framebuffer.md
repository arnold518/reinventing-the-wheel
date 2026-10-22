# FramebufferDisplay

Persistent drawing storage plus geometry and PRESENT control.

Catalog: `device.framebuffer`. The implemented form is structural; selectable lower-level children may be compact. No whole-component behavioral form is registered.

## Pins

| Pin | Direction | Bits |
| --- | --- | --- |
| `CLK` | Input | 1 |
| `FB_CLK` | Input | 1 |
| `FB_REQ_ADDR` | Input | 32 |
| `FB_REQ_OP` | Input | 3 |
| `FB_REQ_VALID` | Input | 1 |
| `FB_REQ_WRITE_DATA` | Input | 32 |
| `FB_REQ_WRITE_MASK` | Input | 4 |
| `FB_RSP_READY` | Input | 1 |
| `FB_RST` | Input | 1 |
| `REQ_ADDR` | Input | 32 |
| `REQ_OP` | Input | 3 |
| `REQ_VALID` | Input | 1 |
| `REQ_WRITE_DATA` | Input | 32 |
| `REQ_WRITE_MASK` | Input | 4 |
| `RSP_READY` | Input | 1 |
| `RST` | Input | 1 |
| `FB_REQ_READY` | Output | 1 |
| `FB_RSP_READ_DATA` | Output | 32 |
| `FB_RSP_STATUS` | Output | 3 |
| `FB_RSP_VALID` | Output | 1 |
| `FRAME_SEQUENCE` | Output | 32 |
| `PRESENT` | Output | 1 |
| `REQ_READY` | Output | 1 |
| `RSP_READ_DATA` | Output | 32 |
| `RSP_STATUS` | Output | 3 |
| `RSP_VALID` | Output | 1 |

## Behavior

The unprefixed protocol is the control bank; FB_ is the separate byte-mask-capable pixel-memory endpoint. Control offsets: 0 WIDTH, 4 HEIGHT, 8 STRIDE, 12 FORMAT (1=RGB332), 16 FRAME_SEQUENCE, 20 PRESENT command. Full-mask write of 1 to PRESENT increments FRAME_SEQUENCE once; 0 or zero mask does nothing. PRESENT is the execution strobe before that increment edge.

## Internal structure

DeviceRegisterPort<32>, sequence counter and existing FF MemoryEndpoint storage.
The same component accepts these catalog parameters:

| Parameter | Default | Supported values |
| --- | --- | --- |
| `width` | 4 | 1–4096 pixels |
| `height` | 4 | 1–4096 pixels, with `width × height ≤ 262144` |
| `storage_bytes` | 0 | 0 selects automatic capacity; otherwise a power of two in 4–262144 covering the surface |

Stride equals width; widths need not be powers of two. Each pixel uses one
RGB332 byte. Automatic backing capacity rounds up to a power of two, with a
four-byte minimum: 64×48 uses 4096 bytes; 128×96 uses 16384; 320×200 uses
65536. Tiny arrays use the structural storage path; larger arrays select the
existing scalable native FF endpoint. Geometry does not create another memory
implementation.

The integrated system automatically reserves at least its original 4096-byte
video window, enlarging it to cover the selected surface. Its `video_bytes`
parameter can reserve a larger explicit capacity. The data router receives
the same capacity, so the aperture and backing store agree.

## Visual abstraction

The circuit viewer's **Device views** control shows a pixel surface in place
of the device's internal drawing. Turn it off to inspect the gates. Selecting
the component also opens a display preview in the inspector. Both read pixel
history and frame-sequence pins at the selected timeline time, including
unknown pixel masks; scrubbing does not run the guest or issue PRESENT.
Tiny expanded storage is observed through its existing word-output histories.
Larger storage is read through its memory observation capability.

## Edge cases and integration

Reset clears control/sequence and pending responses, and retains drawing storage. Explicit image initialization/clear is a separate host operation. Tie CLK/FB_CLK and RST/FB_RST to the same global signals. Control writes require full or zero mask; FB_ writes accept normal byte masks. Padded bytes are addressable but not visible. Native storage records memory history by default for timeline inspection; headless callers can disable it through the delegated image helpers. The PIXELS child owns MemoryStateView, so inspection follows pixel accesses rather than control-bank addresses. The root exposes image helper methods without advertising its control port as a memory endpoint. Tiny fully expanded storage has no compact backing-store view. PRESENT does not itself copy a frame or draw in the browser. Capture/host display adapters belong to the integrated platform.

## Recorded example and checks

`FramebufferDisplayTest` owns the independent functional checks; `FramebufferDisplayScenarioTest` retains this smaller recorded example. Both use the same logical component owner.

<!-- simulator-examples: framebuffer-display FramebufferDisplayScenarioTest -->

<div class="simulator-embed"><iframe src="../../simulator/?scenario=framebuffer-display&amp;embed=1" title="FramebufferDisplay" loading="lazy" allowfullscreen></iframe></div>

<a href="../../simulator/?scenario=framebuffer-display" class="md-button">Open the recorded circuit</a>

Geometry and backing capacity also select a configuration-specific visual
floorplan. The default standalone 4×4 key stays unchanged. Variant floorplans
inherit the type's colors; their internal placements are stored separately.
