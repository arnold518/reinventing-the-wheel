"""Regression checks for component types traversed by the Python visualizer."""

import circuit_backend
import server


BEHAVIORAL_MEMORY_CONTRACTS = {
    "rv32i.memory.64k-x32",
    "rv32i.register-file",
    "memory.register.width32",
    "memory.write-enabled-bit",
}


def walk_components(root):
    pending = [root]
    while pending:
        component = pending.pop()
        yield component
        pending.extend(component.get_children())


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
    parent_layout = server.COMPONENT_LAYOUT_DEFAULTS.get(
        server._component_layout_type(component), {}
    )
    parent_aspect = max(
        parent_layout.get("aspect_ratio", 0),
        server._minimum_aspect_ratio_for_pins(
            component, settings, parent_layout
        ),
    )
    title_ratio = parent_layout.get(
        "title_bar_ratio", settings["title_bar_ratio"]
    )
    title_fraction = min(0.7, title_ratio / parent_aspect)
    child_aspects = {
        child.get_id(): max(
            server.COMPONENT_LAYOUT_DEFAULTS.get(
                server._component_layout_type(child), {}
            ).get("aspect_ratio", 0),
            server._minimum_aspect_ratio_for_pins(
                child,
                settings,
                server.COMPONENT_LAYOUT_DEFAULTS.get(
                    server._component_layout_type(child), {}
                ),
            ),
        )
        for child in children
    }
    boundary = parent_layout.get(
        "boundary_area_ratio", settings["boundary_area_ratio"]
    )
    pin_size = parent_layout.get(
        "pin_size_ratio", settings["pin_size_ratio"]
    )
    return children, parent_aspect, server._layered_graph_layout(
        component,
        children,
        parent_aspect,
        title_fraction,
        child_aspects,
        boundary,
        pin_size,
    ), child_aspects


def assert_default_children_do_not_overlap(component):
    children, parent_aspect, placements, child_aspects = default_placements(component)
    parent_layout = server.COMPONENT_LAYOUT_DEFAULTS.get(
        server._component_layout_type(component), {}
    )
    boundary = parent_layout.get(
        "boundary_area_ratio",
        server.DEFAULT_SETTINGS["boundary_area_ratio"],
    )
    pin_size = parent_layout.get(
        "pin_size_ratio",
        server.DEFAULT_SETTINGS["pin_size_ratio"],
    )
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
    for program_number in range(1, 23):
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
        assert ("performance" in descriptor["labels"]) == (
            program_number >= 17
        )
        pipeline_test_name = (
            "RV32IFiveStageCoreProgramTest/"
            f"program-{program_number:02d}"
        )
        assert pipeline_test_name in registered
        assert (
            server.scenario_canonical_alias(pipeline_test_name)
            == f"rv32i-five-stage-program{program_number}"
        )
        assert (
            f"rv32i-five-stage-program{program_number}"
            in server.SCENARIOS
        )
        pipeline_descriptor = descriptors[pipeline_test_name]
        assert pipeline_descriptor["visualizable"]
        assert (
            pipeline_descriptor["logical_test_id"]
            == "RV32IFiveStageCoreProgramTest"
        )
        assert pipeline_descriptor["kind"] == "program"
        assert (
            pipeline_descriptor["scenario_id"]
            == f"program-{program_number:02d}"
        )
        assert pipeline_descriptor["contract_ids"] == []
        assert ("performance" in pipeline_descriptor["labels"]) == (
            program_number >= 17
        )

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
    program_metrics = {
        metric.name: metric.value
        for metric in program.get_performance_metrics()
    }
    assert program_metrics["cpu.cpi"] == 1.0
    assert program_metrics["cpu.hardware_cycles"] > 0
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
        "REGISTER_FILE": "behavioral",
        "ALU": "structural",
        "EXECUTION_STATUS": "structural",
    }
    for child_name, expected_fidelity in expected_core_fidelities.items():
        assert core_children[child_name].get_selected_fidelity() == expected_fidelity
    single_cycle_memory = [
        component
        for component in walk_components(system)
        if component.get_contract_id() in BEHAVIORAL_MEMORY_CONTRACTS
    ]
    assert single_cycle_memory
    assert all(
        component.get_selected_fidelity() == "behavioral"
        for component in single_cycle_memory
    )
    assert system.get_available_fidelities() == [
        "structural",
        "behavioral",
    ]
    assert system.is_profile_selectable()
    assert not system.used_unavailable_fidelity_exception()

    expanded_register_program = circuit_backend.create_test_by_name(
        "RV32ISingleCycleSystemTest/program-09"
    )
    expanded_register_program.set_build_profile(
        circuit_backend.profile_with_exact_overrides(
            expanded_register_program.get_build_profile(),
            {
                (
                    "RV32I_SINGLE_CYCLE_SYSTEM_ROOT"
                    ".CORE.REGISTER_FILE"
                ): "structural",
            },
            "visualizer-storage-expanded",
        )
    )
    expanded_register_program.setup_circuit()
    expanded_register_core = {
        child.get_name(): child
        for child in expanded_register_program.get_root().get_children()
    }["CORE"]
    expanded_register_file = {
        child.get_name(): child
        for child in expanded_register_core.get_children()
    }["REGISTER_FILE"]
    assert expanded_register_file.get_selected_fidelity() == "structural"
    assert list(expanded_register_file.get_children())

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

    pipeline_program = circuit_backend.create_test_by_name(
        "RV32IFiveStageCoreProgramTest/program-01"
    )
    pipeline_program.setup_circuit()
    assert pipeline_program.is_simulation_precomputed()
    pipeline_checkpoints = list(
        pipeline_program.get_checkpoints()
    )
    pipeline_timestamps = list(
        pipeline_program.get_simulator().get_unique_timestamps()
    )
    assert pipeline_checkpoints
    assert (
        pipeline_program.get_run_duration()
        == pipeline_checkpoints[-1].time
    )
    assert max(pipeline_timestamps) <= pipeline_checkpoints[-1].time
    pipeline_root = pipeline_program.get_root()
    pipeline_children = {
        child.get_name(): child
        for child in pipeline_root.get_children()
    }
    assert (
        pipeline_children["CORE"].get_selected_fidelity()
        == "structural"
    )
    pipeline_core = pipeline_children["CORE"]
    pipeline_core_children = {
        child.get_name(): child
        for child in pipeline_core.get_children()
    }
    assert list(pipeline_core_children) == [
        "FETCH",
        "IF_ID",
        "DECODE",
        "ID_EX",
        "EXECUTE",
        "EX_MEM",
        "MEMORY",
        "MEM_WB",
        "WRITEBACK",
        "COORDINATOR",
    ]
    assert all(
        child.get_selected_fidelity() == "structural"
        for child in pipeline_core_children.values()
    )
    pipeline_components = list(walk_components(pipeline_root))
    pipeline_memory = [
        component
        for component in pipeline_components
        if component.get_contract_id() in BEHAVIORAL_MEMORY_CONTRACTS
    ]
    assert pipeline_memory
    assert all(
        component.get_selected_fidelity() == "behavioral"
        for component in pipeline_memory
    )
    rv32i_components = [
        component
        for component in walk_components(pipeline_core)
        if component.get_contract_id().startswith("rv32i.")
        and component.get_contract_id() not in BEHAVIORAL_MEMORY_CONTRACTS
    ]
    assert len(rv32i_components) > len(pipeline_core_children)
    assert all(
        component.get_selected_fidelity() == "structural"
        for component in rv32i_components
    )

    pipeline_register_test = circuit_backend.create_test_by_name(
        "RV32IIFIDPipelineRegisterTest"
    )
    pipeline_register_test.setup_circuit()
    pipeline_register_memory = [
        component
        for component in walk_components(pipeline_register_test.get_root())
        if component.get_contract_id() in BEHAVIORAL_MEMORY_CONTRACTS
    ]
    assert pipeline_register_memory
    assert all(
        component.get_selected_fidelity() == "behavioral"
        for component in pipeline_register_memory
    )
    assert_default_children_do_not_overlap(pipeline_core)
    _, core_aspect, core_stage_placements, _ = default_placements(
        pipeline_core
    )
    assert core_aspect < 1.0
    pipeline_x_positions = [
        core_stage_placements[name]["rel_pos"][0]
        for name in server.FIVE_STAGE_PIPELINE_CHILD_ORDER
    ]
    assert pipeline_x_positions == sorted(pipeline_x_positions)
    assert (
        core_stage_placements["COORDINATOR"]["rel_pos"][1]
        > max(
            core_stage_placements[name]["rel_pos"][1]
            for name in server.FIVE_STAGE_PIPELINE_CHILD_ORDER
        )
    )

    fetch_expanded_program = circuit_backend.create_test_by_name(
        "RV32IFiveStageCoreProgramTest/program-01"
    )
    fetch_expanded_program.set_build_profile(
        circuit_backend.profile_with_exact_overrides(
            fetch_expanded_program.get_build_profile(),
            {
                (
                    "RV32I_FIVE_STAGE_PROGRAM_ROOT"
                    ".CORE.FETCH"
                ): "structural",
            },
            "visualizer-five-stage-fetch-expanded",
        )
    )
    fetch_expanded_program.setup_circuit()
    expanded_root = fetch_expanded_program.get_root()
    expanded_core = {
        child.get_name(): child
        for child in expanded_root.get_children()
    }["CORE"]
    expanded_fetch = {
        child.get_name(): child
        for child in expanded_core.get_children()
    }["FETCH"]
    assert expanded_fetch.get_selected_fidelity() == "structural"
    assert list(expanded_fetch.get_children())
    assert list(fetch_expanded_program.get_checkpoints())

    pipeline_layout_key = server.scenario_layout_key(
        "rv32i-five-stage-program1", pipeline_root
    )
    layout_manager.ensure_component_layout_defaults(
        pipeline_root,
        scenario_key=pipeline_layout_key,
        is_root=True,
    )
    pipeline_parent_aspect = layout_manager.aspect_for_component(
        pipeline_root,
        scenario_key=pipeline_layout_key,
        is_root=True,
    )
    pipeline_title_ratio = (
        layout_manager.visual_ratio_for_component(
            pipeline_root,
            "title_bar_ratio",
            scenario_key=pipeline_layout_key,
            is_root=True,
        )
    )
    pipeline_title_fraction = min(
        0.7,
        pipeline_title_ratio / pipeline_parent_aspect,
    )
    pipeline_child_list = list(pipeline_root.get_children())
    pipeline_child_aspects = {
        child.get_id(): layout_manager.aspect_for_component(child)
        for child in pipeline_child_list
    }
    pipeline_boundary = layout_manager.visual_ratio_for_component(
        pipeline_root,
        "boundary_area_ratio",
        scenario_key=pipeline_layout_key,
        is_root=True,
    )
    pipeline_pin_size = layout_manager.visual_ratio_for_component(
        pipeline_root,
        "pin_size_ratio",
        scenario_key=pipeline_layout_key,
        is_root=True,
    )
    pipeline_placements = server._layered_graph_layout(
        pipeline_root,
        pipeline_child_list,
        pipeline_parent_aspect,
        pipeline_title_fraction,
        pipeline_child_aspects,
        pipeline_boundary,
        pipeline_pin_size,
    )
    assert 0.35 <= pipeline_parent_aspect < 1.0
    assert pipeline_placements["CORE"]["rel_width"] > 0.15
    assert (
        pipeline_placements["INSTRUCTION_MEMORY"]["rel_pos"][0]
        < pipeline_placements["CORE"]["rel_pos"][0]
    )
    assert (
        pipeline_placements["DATA_MEMORY"]["rel_pos"][0]
        < pipeline_placements["CORE"]["rel_pos"][0]
    )

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

    for program_number in range(1, 23):
        assert (
            f"rv32i-program{program_number}@{profile_fingerprint}"
            in layout_manager.root_layouts
        )
        assert (
            f"rv32i-five-stage-program{program_number}"
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
