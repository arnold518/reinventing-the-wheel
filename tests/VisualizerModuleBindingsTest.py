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
    descriptors = {
        descriptor["name"]: descriptor
        for descriptor in circuit_backend.get_registered_test_descriptors()
    }
    assert set(descriptors) == registered
    for program_number in range(1, 17):
        test_name = (
            f"RV32ISingleCycleSystemTest/program-{program_number:02d}"
        )
        assert test_name in registered
        assert not hasattr(circuit_backend, test_name)
        assert (
            server.scenario_canonical_alias(test_name)
            == f"rv32i-program{program_number}"
        )
        descriptor = descriptors[test_name]
        assert descriptor["visualizable"]
        assert descriptor["logical_test_id"] == "RV32ISingleCycleSystemTest"
        assert descriptor["kind"] == "program"
        assert descriptor["scenario_id"] == f"program-{program_number:02d}"
        assert descriptor["contract_ids"] == [
            "rv32i.system.educational-single-cycle"
        ]

    forbidden_test_fragments = (
        "Structural",
        "Behavioral",
        "Balanced",
        "Reference",
        "Hybrid",
        "Equivalence",
    )
    for test_name in registered:
        assert not any(
            fragment in test_name
            for fragment in forbidden_test_fragments
        )

    assert "RV32ISingleCycleCoreTest" in registered
    assert not hasattr(circuit_backend, "RV32ISingleCycleCoreTest")
    assert descriptors["RV32ISingleCycleCoreTest"]["visualizable"]
    assert descriptors["RV32ISingleCycleCoreTest"]["contract_ids"] == [
        "rv32i.core.educational-single-cycle"
    ]

    program = circuit_backend.create_test_by_name(
        "RV32ISingleCycleSystemTest/program-09"
    )
    assert program.supports_build_profile()
    program.setup_circuit()
    assert program.get_run_duration() > 0
    system = program.get_root()
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
        "REGISTER_FILE": "structural",
        "ALU": "structural",
        "EXECUTION_STATUS": "structural",
    }
    for child_name, expected_fidelity in expected_core_fidelities.items():
        assert core_children[child_name].get_selected_fidelity() == expected_fidelity
    assert system.get_available_fidelities() == [
        "structural",
        "behavioral",
    ]
    assert system.is_profile_selectable()
    assert not system.used_unavailable_fidelity_exception()

    behavioral_program = circuit_backend.create_test_by_name(
        "RV32ISingleCycleSystemTest/program-09"
    )
    behavioral_program.set_build_profile(
        circuit_backend.profile_with_exact_overrides(
            behavioral_program.get_build_profile(),
            {
                "RV32I_SINGLE_CYCLE_SYSTEM_ROOT": "behavioral",
            },
            "visualizer-binding-probe",
        )
    )
    behavioral_program.setup_circuit()
    behavioral_system = behavioral_program.get_root()
    assert behavioral_system.get_selected_fidelity() == "behavioral"
    assert list(behavioral_system.get_children()) == []

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
    program_layout_key = server.scenario_layout_key(
        "rv32i-program9", system
    )
    assert (
        program_layout_key
        == f"rv32i-program9@{profile_fingerprint}"
    )

    layout_manager = server.LayoutManager(server.LAYOUT_PATH)
    assert layout_manager.schema_version == server.LAYOUT_SCHEMA_VERSION == 2
    assert isinstance(layout_manager.profile_layouts, dict)
    assert program_layout_key in layout_manager.root_layouts

    loader_session = server.CircuitSession(
        "rv32i-program-loader",
        layout_manager,
    )
    assert loader_session.root.get_selected_fidelity() == "behavioral"
    assert loader_session.timestamps == [0]
    assert loader_session.checkpoints == [
        {
            "time": 0,
            "label": "program-loaded",
            "detail": (
                "Three RV32I instructions are preloaded at addresses "
                "0x0, 0x4, and 0x8."
            ),
            "rowIndex": 0,
            "index": 0,
        }
    ]
    loader_state = loader_session.component_state_at_index(
        0,
        loader_session.root.get_id(),
    )
    assert loader_state["memoryRole"] == "instruction"
    assert [
        (word["address"], word["hex"], word["instructionText"])
        for word in loader_state["touchedWords"]
    ] == [
        (0, "0x00100093", "addi x1, x0, 1"),
        (4, "0x00208113", "addi x2, x1, 2"),
        (8, "0x00100073", "ebreak"),
    ]

    memory_bit_session = server.CircuitSession(
        "memory-bit",
        layout_manager,
    )
    progressive_payload = memory_bit_session.topology_response()
    assert (
        progressive_payload["topologyEncoding"]
        == "progressive-v1"
    )
    assert progressive_payload["loadedScopeIds"] == [
        memory_bit_session.root.get_id()
    ]
    assert len(progressive_payload["pins"]) < len(
        memory_bit_session.pins
    )
    assert len(progressive_payload["wires"]) < len(
        memory_bit_session.wires
    )
    assert all(
        "childIds" not in component
        and "parentId" not in component
        for component in progressive_payload["components"]
    )
    assert progressive_payload["components"][0]["parentIndex"] is None
    assert all(
        component["parentIndex"] is None
        or (
            isinstance(component["parentIndex"], int)
            and component["parentIndex"] >= 0
        )
        for component in progressive_payload["components"]
    )

    memory_root_children = list(
        memory_bit_session.root.get_children()
    )
    assert memory_root_children
    child_scope = memory_bit_session.topology_slice([
        memory_root_children[0].get_id(),
        memory_root_children[0].get_id(),
    ])
    assert child_scope["scopeIds"] == [
        memory_root_children[0].get_id()
    ]
    child_scope_pin_ids = {
        pin["id"] for pin in child_scope["pins"]
    }
    for wire in child_scope["wires"]:
        assert (
            wire["sourcePinId"] is None
            or wire["sourcePinId"] in child_scope_pin_ids
        )
        assert set(wire["sinkPinIds"]).issubset(
            child_scope_pin_ids
        )
    complete_memory_bit_topology = (
        memory_bit_session.topology_slice([
            component["id"]
            for component in memory_bit_session.components
        ])
    )
    assert {
        pin["id"]
        for pin in complete_memory_bit_topology["pins"]
    } == {
        pin["id"] for pin in memory_bit_session.pins
    }
    assert {
        wire["id"]
        for wire in complete_memory_bit_topology["wires"]
    } == {
        wire["id"] for wire in memory_bit_session.wires
    }
    try:
        memory_bit_session.topology_slice(
            ["MEMORY_BIT_ROOT.UNKNOWN"]
        )
    except ValueError as error:
        assert "Unknown topology scope owner" in str(error)
    else:
        raise AssertionError(
            "unknown topology scope owner should fail"
        )

    splitter_session = server.CircuitSession(
        "bit-splitter32",
        layout_manager,
    )
    splitter_output_names = [
        pin["name"]
        for pin in splitter_session.pins
        if pin["type"] == "output"
    ]
    assert splitter_output_names == [
        f"OUT_{index}" for index in range(32)
    ]

    joiner_session = server.CircuitSession(
        "bit-joiner32",
        layout_manager,
    )
    joiner_input_names = [
        pin["name"]
        for pin in joiner_session.pins
        if pin["type"] == "input"
    ]
    assert joiner_input_names == [
        f"IN_{index}" for index in range(32)
    ]

    for program_number in range(1, 17):
        assert (
            f"rv32i-program{program_number}@{profile_fingerprint}"
            in layout_manager.root_layouts
        )
    for layout_key in layout_manager.root_layouts:
        assert not any(
            fragment in layout_key
            for fragment in (
                "structural-program",
                "balanced-program",
                "reference-program",
            )
        )

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

    register_file_mux = circuit_backend.Mux4to1_32bit("RS2_MUX")
    (
        _,
        _,
        register_file_mux_placements,
        _,
    ) = default_placements(register_file_mux)
    splitter_width = register_file_mux_placements[
        "IN0_SPLIT"
    ]["rel_width"]
    mux_cell_width = register_file_mux_placements[
        "MUX_BIT_0"
    ]["rel_width"]
    joiner_width = register_file_mux_placements[
        "OUT_JOIN"
    ]["rel_width"]
    assert mux_cell_width > splitter_width * 2
    assert joiner_width > mux_cell_width
    assert_default_children_do_not_overlap(register_file_mux)


if __name__ == "__main__":
    main()
