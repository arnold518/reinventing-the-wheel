"""Semantic default colors for visualizer component families.

The layout file may override any color.  These defaults keep newly added and
previously uncolored component types meaningful without making hierarchy depth
change a component's semantic hue.
"""

from __future__ import annotations

import json
from pathlib import Path

_PALETTE_PATH = Path(__file__).with_name("palette.json")
_PALETTE = json.loads(_PALETTE_PATH.read_text(encoding="utf-8"))
if _PALETTE.get("schema_version") != 1:
    raise ValueError("Unsupported visualizer palette schema")

DEFAULT_COMPONENT_COLOR = tuple(_PALETTE["default_color"])
TYPE_COLORS: dict[str, tuple[int, int, int, int]] = {
    name: tuple(color)
    for name, color in _PALETTE["type_colors"].items()
}
PREFIX_COLORS: dict[str, tuple[int, int, int, int]] = {
    prefix: tuple(color)
    for prefix, color in _PALETTE["prefix_colors"].items()
}


def color_for_layout_type(layout_type: str) -> list[int]:
    """Return a fresh RGBA list for one component layout type."""

    for prefix, color in PREFIX_COLORS.items():
        if layout_type.startswith(prefix):
            return list(color)
    return list(TYPE_COLORS.get(layout_type, DEFAULT_COMPONENT_COLOR))
