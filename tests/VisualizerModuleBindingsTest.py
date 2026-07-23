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
    registered = set(circuit_backend.get_registered_test_names())
    for program_number in range(1, 17):
        scenario_families = (
            (
                f"RV32ISingleCycleSystemProgram{program_number}Test",
                f"rv32i-structural-program{program_number}",
            ),
            (
                f"RV32IBalancedSystemProgram{program_number}Test",
                f"rv32i-balanced-program{program_number}",
            ),
            (
                f"RV32IReferenceSystemProgram{program_number}Test",
                f"rv32i-reference-program{program_number}",
            ),
        )
        for test_name, expected_alias in scenario_families:
            assert test_name in registered
            assert hasattr(circuit_backend, test_name)
            assert server.scenario_canonical_alias(test_name) == expected_alias

    for test_name in (
        "RV32IControlFlowUnitEquivalenceTest",
        "RV32IDecodeControlUnitEquivalenceTest",
        "RV32IRegisterFileEquivalenceTest",
        "ALU32EquivalenceTest",
        "RV32IExecutionControlStatusUnitEquivalenceTest",
    ):
        assert test_name in registered
        assert test_name in server.NON_VISUALIZABLE_TESTS
        assert not hasattr(circuit_backend, test_name)

    balanced = circuit_backend.RV32IBalancedSystemProgram9Test()
    balanced.setup_circuit()
    system = balanced.get_root()
    assert system.get_type_name() == "RV32ISingleCycleSystem"
    assert system.get_selected_fidelity() == "structural"
    assert system.get_profile_fingerprint() != "explicit"
    system_children = {child.get_name(): child for child in system.get_children()}
    assert system_children["INSTRUCTION_MEMORY"].get_selected_fidelity() == "behavioral"
    assert system_children["DATA_MEMORY"].get_selected_fidelity() == "behavioral"
    core = system_children["CORE"]
    assert core.get_selected_fidelity() == "structural"
    core_children = {child.get_name(): child for child in core.get_children()}
    expected_core_fidelities = {
        "CONTROL_FLOW": "structural",
        "DECODE_CONTROL": "structural",
        "REGISTER_FILE": "behavioral",
        "ALU": "structural",
        "EXECUTION_STATUS": "structural",
    }
    for child_name, expected_fidelity in expected_core_fidelities.items():
        assert core_children[child_name].get_selected_fidelity() == expected_fidelity

    _, _, system_placements, _ = default_placements(system)
    assert (
        system_placements["INSTRUCTION_MEMORY"]["rel_pos"][0]
        < system_placements["CORE"]["rel_pos"][0]
    )
    assert (
        system_placements["DATA_MEMORY"]["rel_pos"][0]
        < system_placements["CORE"]["rel_pos"][0]
    )
    assert_default_children_do_not_overlap(system)

    profile_fingerprint = system.get_profile_fingerprint()
    balanced_layout_key = server.scenario_layout_key("rv32i-balanced-program9", system)
    assert balanced_layout_key == f"rv32i-balanced-program9@{profile_fingerprint}"

    layout_manager = server.LayoutManager(server.LAYOUT_PATH)
    assert layout_manager.schema_version == server.LAYOUT_SCHEMA_VERSION == 2
    assert isinstance(layout_manager.profile_layouts, dict)
    assert balanced_layout_key in layout_manager.root_layouts
    for program_number in range(1, 17):
        assert f"rv32i-structural-program{program_number}" in layout_manager.root_layouts
        assert (
            f"rv32i-balanced-program{program_number}@{profile_fingerprint}"
            in layout_manager.root_layouts
        )
        assert f"rv32i-reference-program{program_number}" in layout_manager.root_layouts

    base_core_children = layout_manager.type_layouts["RV32ISingleCycleCore"]["children"]
    assert set(expected_core_fidelities).issubset(base_core_children)
    original_register_file_position = base_core_children["REGISTER_FILE"]["rel_pos"]
    layout_manager.update_profile_child_layout(
        profile_fingerprint,
        "RV32ISingleCycleCore",
        "REGISTER_FILE",
        [0.123, 0.456],
        0.078,
    )
    profile_core_layout = layout_manager.get_layout_for_component(core)
    assert profile_core_layout["children"]["REGISTER_FILE"]["rel_pos"] == [0.123, 0.456]
    assert (
        layout_manager.type_layouts["RV32ISingleCycleCore"]["children"]["REGISTER_FILE"]["rel_pos"]
        == original_register_file_position
    )

    word_size = circuit_backend.ConstantValue2("CONST_WORD_SIZE", 2)
    assert pin_shape(word_size) == ({}, {"OUT": 2})
    assert word_size.get_constant_value() == 2
    assert word_size.get_contract_id() == "explicit.ConstantValue"
    assert word_size.get_implementation_id() == "explicit.construction.ConstantValue"
    assert word_size.get_selected_fidelity() == "unspecified"
    assert not word_size.is_terminal_primitive()
    assert word_size.get_profile_fingerprint() == "explicit"

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
