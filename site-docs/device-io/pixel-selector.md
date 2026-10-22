# `IndexedPixelSelector<W>`

Little-endian packed-pixel selection with RGB332 color expansion.

W is **8, 16 or 32**. The word width is independent of the eight-bit pixel
format and 32-bit ARGB output.

Catalog: `video.pixel-selector.W`. Structural fidelity is available at every width. There is no whole-component behavioral form.

## Pins

| Pin | Direction | Bits |
| --- | --- | --- |
| `OFFSET` | Input | max(1,log2(W/8)) |
| `WORD` | Input | W |
| `ARGB` | Output | 32 |
| `INDEX` | Output | 8 |

## Behavior

OFFSET 0..W/8−1 selects one byte of WORD, with byte0 in bits7:0.
For W=8 the one-bit OFFSET=1 encoding produces index zero. INDEX is that byte. ARGB has alpha 0xff, red INDEX[7:5], green [4:2], blue [1:0], expanded to eight bits by bit replication.

## Internal structure

Byte-selection muxes and wires/constants for palette expansion. There is no programmable palette RAM, scanout clock or host renderer in this block.

## Edge cases and integration

Unknown pixel bits propagate to the corresponding color bits; alpha stays known. Unknown offsets follow mux gate behavior. The four recorded primary-color/white examples establish byte order and channel mapping.

## Recorded example and checks

`IndexedPixelSelectorWidth32Test` owns the independent functional checks; `IndexedPixelSelectorWidth32ScenarioTest` retains this smaller recorded example. Both use the same logical component owner. This recording selects W=32;
`WidthComponentsTest` exercises the other supported widths.

<!-- simulator-examples: indexed-pixel-selector-w32 IndexedPixelSelectorWidth32ScenarioTest -->

<div class="simulator-embed"><iframe src="../../simulator/?scenario=indexed-pixel-selector-w32&amp;embed=1" title="IndexedPixelSelector<32>" loading="lazy" allowfullscreen></iframe></div>

<a href="../../simulator/?scenario=indexed-pixel-selector-w32" class="md-button">Open the recorded circuit</a>
