# IndexedPixelSelector32

Little-endian packed-pixel selection and fixed RGB332 interpretation.

Catalog: `video.indexed-pixel.32`. The implemented form is structural; selectable lower-level children may be compact. No new whole-component behavioral form is registered.

## Pins

| Pin | Direction | Bits |
| --- | --- | --- |
| `OFFSET` | Input | 2 |
| `WORD` | Input | 32 |
| `ARGB` | Output | 32 |
| `INDEX` | Output | 8 |

## Behavior

OFFSET 0..3 selects one byte of WORD, with byte0 in bits7:0. INDEX is that byte. ARGB has alpha 0xff, red INDEX[7:5], green [4:2], blue [1:0], expanded to eight bits by bit replication.

## Internal structure

Byte-selection muxes and wires/constants for palette expansion. There is no programmable palette RAM, scanout clock or host renderer in this block.

## Edge cases and integration

Unknown pixel bits propagate to the corresponding color bits; alpha stays known. Unknown offsets follow mux gate behavior. The four recorded primary-color/white examples establish byte order and channel mapping.

## Recorded example and checks

`IndexedPixelSelector32Test` owns the independent functional checks; `IndexedPixelSelector32ScenarioTest` retains this smaller recorded example. Both use the same logical component owner.

<!-- simulator-examples: indexed-pixel-selector32 IndexedPixelSelector32ScenarioTest -->

<div class="simulator-embed"><iframe src="../../simulator/?scenario=indexed-pixel-selector32&amp;embed=1" title="IndexedPixelSelector32" loading="lazy" allowfullscreen></iframe></div>

<a href="../../simulator/?scenario=indexed-pixel-selector32" class="md-button">Open the recorded circuit</a>
