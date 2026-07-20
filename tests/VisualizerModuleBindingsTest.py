"""Regression checks for component types traversed by the Python visualizer."""

import circuit_backend
import server


def pin_shape(component):
    inputs = {
        name: pin.get_width()
        for name, pin in component.get_input_pins().items()
    }
    outputs = {
        name: pin.get_width()
        for name, pin in component.get_output_pins().items()
    }
    return inputs, outputs


def default_placements(component):
    children = list(component.get_children())
    settings = server.DEFAULT_SETTINGS
    parent_aspect = server._minimum_aspect_ratio_for_pins(component, settings)
    title_fraction = min(0.7, settings["title_bar_ratio"] / parent_aspect)
    child_aspects = {
        child.get_id(): server._minimum_aspect_ratio_for_pins(child, settings)
        for child in children
    }
    return children, parent_aspect, server._layered_graph_layout(
        component,
        children,
        parent_aspect,
        title_fraction,
        child_aspects,
        settings["boundary_area_ratio"],
        settings["pin_size_ratio"],
    ), child_aspects


def assert_default_children_do_not_overlap(component):
    children, parent_aspect, placements, child_aspects = default_placements(component)
    boundary = server.DEFAULT_SETTINGS["boundary_area_ratio"]
    pin_size = server.DEFAULT_SETTINGS["pin_size_ratio"]
    rects = []
    for child in children:
        placement = placements[child.get_name()]
        x, y = placement["rel_pos"]
        width = placement["rel_width"]
        height = width * child_aspects[child.get_id()] / parent_aspect
        rects.append(
            (
                child.get_name(),
                x - width * boundary,
                y - width * pin_size / 2,
                width * (1 + 2 * boundary),
                height + width * pin_size,
            )
        )

    for index, left in enumerate(rects):
        for right in rects[index + 1:]:
            overlap_x = min(left[1] + left[3], right[1] + right[3]) - max(left[1], right[1])
            overlap_y = min(left[2] + left[4], right[2] + right[4]) - max(left[2], right[2])
            assert overlap_x <= 1e-12 or overlap_y <= 1e-12, (
                f"default children overlap in {component.get_name()}: {left[0]} and {right[0]}"
            )


def main():
    word_size = circuit_backend.ConstantValue2("CONST_WORD_SIZE", 2)
    assert pin_shape(word_size) == ({}, {"OUT": 2})
    assert word_size.get_constant_value() == 2

    trap_cause = circuit_backend.ConstantValue4("CAUSE_ILLEGAL", 2)
    assert pin_shape(trap_cause) == ({}, {"OUT": 4})
    assert trap_cause.get_constant_value() == 2

    matcher = circuit_backend.RV32IBitPatternMatcher(
        "MATCH_ADDI",
        0x0000707F,
        0x00000013,
    )
    assert pin_shape(matcher) == ({"INPUT": 32}, {"MATCH": 1})
    assert matcher.mask == 0x0000707F
    assert matcher.value == 0x00000013

    ecall_matcher = circuit_backend.RV32IBitPatternMatcher(
        "MATCH_ECALL",
        0xFFFFFFFF,
        0x00000073,
    )
    assert server._component_layout_type(matcher) != server._component_layout_type(ecall_matcher)
    assert_default_children_do_not_overlap(ecall_matcher)

    immediate_mux = circuit_backend.Mux8to1_32bit("IMMEDIATE_MUX")
    assert_default_children_do_not_overlap(immediate_mux)


if __name__ == "__main__":
    main()
