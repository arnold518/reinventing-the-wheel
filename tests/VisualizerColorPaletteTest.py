"""Fast contract checks for semantic visualizer component colors."""

from colorsys import rgb_to_hsv
import json
from pathlib import Path

from palette import DEFAULT_COMPONENT_COLOR, color_for_layout_type


LAYOUT_PATH = Path(__file__).resolve().parents[1] / "visualizer-v2" / "layout.json"
TYPE_LAYOUTS = json.loads(LAYOUT_PATH.read_text(encoding="utf-8"))["type_layouts"]


def effective_color(layout_type: str) -> list[int]:
    """Resolve colors in the same override-first order as the server."""

    layout_color = TYPE_LAYOUTS.get(layout_type, {}).get("color")
    return layout_color or color_for_layout_type(layout_type)


def hue(color: list[int]) -> float:
    return rgb_to_hsv(*(channel / 255 for channel in color[:3]))[0]


def circular_hue_distance(left: list[int], right: list[int]) -> float:
    delta = abs(hue(left) - hue(right))
    return min(delta, 1 - delta)


def main() -> None:
    # Related components keep a nearby hue.
    assert circular_hue_distance(
        color_for_layout_type("Mux2to1"),
        color_for_layout_type("Mux8to1"),
    ) < 0.08
    assert circular_hue_distance(
        color_for_layout_type("HalfAdder"),
        color_for_layout_type("Adder32"),
    ) < 0.08
    assert circular_hue_distance(
        color_for_layout_type("SRLatch"),
        color_for_layout_type("DFlipFlop"),
    ) < 0.08

    # Structural parent-child pairs must not collapse to the same fill.
    for parent, child in (
        ("FullAdder", "HalfAdder"),
        ("Mux8to1", "Mux2to1"),
        ("Register32", "MemoryBit"),
        ("RV32IFiveStageCore", "RV32IExecuteStage"),
    ):
        assert effective_color(parent) != effective_color(child)

    # Every concrete type in the layout catalog has a semantic color. The two
    # base framework types intentionally retain the neutral fallback.
    neutral_types = {
        layout_type
        for layout_type in TYPE_LAYOUTS
        if effective_color(layout_type) == list(DEFAULT_COMPONENT_COLOR)
    }
    assert neutral_types == {"Component", "IOComponent"}

    for layout_type in TYPE_LAYOUTS:
        color = effective_color(layout_type)
        assert len(color) == 4
        assert all(
            isinstance(channel, int) and 0 <= channel <= 255
            for channel in color
        )

    # Width-specialized routing helpers remain recognizable families.
    assert color_for_layout_type("BitSplitter<1>") == color_for_layout_type(
        "BitSplitter<32>"
    )
    assert color_for_layout_type("BitSplitter<32>") != color_for_layout_type(
        "BitJoiner<32>"
    )
    assert color_for_layout_type(
        "RV32IBitPatternMatcher<mask=0xffffffff,value=0x00000073>"
    ) == [20, 184, 166, 190]


if __name__ == "__main__":
    main()
