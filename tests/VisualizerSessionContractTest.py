"""Parity checks for the portable C++ visualizer facade used by WebAssembly."""

from __future__ import annotations

import json
import re
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

import circuit_backend
import server


PROBE = Path(sys.argv[1]).resolve()
SOURCE_LAYOUT = Path(__file__).resolve().parents[1] / "visualizer-v2" / "layout.json"


def probe(*arguments: str) -> object:
    completed = subprocess.run(
        [str(PROBE), *arguments],
        check=True,
        capture_output=True,
        text=True,
    )
    return json.loads(completed.stdout)


def without_presentation(component: dict[str, object]) -> dict[str, object]:
    ignored = {
        "aspectRatio",
        "minAspectRatio",
        "color",
        "inputPinCount",
        "outputPinCount",
    }
    return {key: value for key, value in component.items() if key not in ignored}


def indexed(records: list[dict[str, object]]) -> list[dict[str, object]]:
    return sorted(records, key=lambda record: int(record["stateIndex"]))


def native_session(scenario: str, layout_path: Path) -> server.CircuitSession:
    return server.CircuitSession(scenario, server.LayoutManager(layout_path))


def compare_topology(registered_test: str, scenario: str, layout_path: Path) -> None:
    native = native_session(scenario, layout_path)
    expected = native.topology_response()
    actual = probe("topology", registered_test, scenario)
    assert isinstance(actual, dict)

    for field in (
        "scenario",
        "layoutKey",
        "profileFingerprint",
        "rootId",
        "rootType",
        "topologyEncoding",
        "loadedScopeIds",
        "timestamps",
        "checkpoints",
        "performanceMetrics",
        "stateEncoding",
        "stats",
        "profile",
    ):
        assert actual[field] == expected[field], field

    assert [without_presentation(item) for item in actual["components"]] == [
        without_presentation(item) for item in expected["components"]
    ]
    assert indexed(actual["pins"]) == indexed(expected["pins"])
    assert indexed(actual["wires"]) == indexed(expected["wires"])
    assert actual["state"] == expected["state"]

    root_slice = probe("slice", registered_test, scenario, actual["rootId"])
    assert root_slice["scopeIds"] == expected["loadedScopeIds"]
    assert indexed(root_slice["pins"]) == indexed(expected["pins"])
    assert indexed(root_slice["wires"]) == indexed(expected["wires"])

    last_index = len(expected["timestamps"]) - 1
    assert probe("state", registered_test, scenario, str(last_index)) == native.state_at_index(last_index)


def main() -> None:
    descriptors = probe("--list")
    wasm_names = [descriptor["name"] for descriptor in descriptors]
    native_names = [
        descriptor["name"]
        for descriptor in circuit_backend.get_registered_test_descriptors()
        if descriptor["visualizable"]
        and descriptor["name"] not in server.NON_VISUALIZABLE_TESTS
    ]
    assert wasm_names == native_names

    with tempfile.TemporaryDirectory() as directory:
        layout_path = Path(directory) / "layout.json"
        shutil.copy2(SOURCE_LAYOUT, layout_path)
        compare_topology("Adder8Test", "adder8", layout_path)
        compare_topology("ALU8Test", "alu8", layout_path)
        compare_topology("AddSub8Test", "addsub8", layout_path)
        compare_topology("ArithmeticUnit8Test", "arithmetic8", layout_path)
        compare_topology("SRLatchTest", "sr-latch", layout_path)
        compare_topology("GatedDLatchTest", "gated-d-latch", layout_path)
        compare_topology("DFlipFlopTest", "dff", layout_path)
        compare_topology("FlipFlopCellTest", "flip-flop-cell", layout_path)
        compare_topology("RewireTest", "rewire", layout_path)

        # A saved global Rewire aspect must not make every pin configuration
        # inherit the proportions of the largest one. Styles remain reusable.
        manager = server.LayoutManager(Path(directory) / "rewire-layout.json")
        manager.type_layouts["Rewire"] = {"aspect_ratio": 9, "min_aspect_ratio": 9, "color": [1, 2, 3, 190]}
        for width in (8, 16, 32):
            memory = circuit_backend.create_ff_memory("M", width, 1, fidelity="structural")
            read = next(c for c in memory.get_children() if c.get_name() == "READ_SELECT")
            rewire = next(c for c in read.get_children() if c.get_type_name() == "Rewire")
            key = f"Rewire<in=1,out={width}>"
            assert server._component_layout_type(rewire) == key
            assert "aspect_ratio" not in manager.get_layout_for_component(rewire)
            manager.ensure_component_layout_defaults(rewire)
            minimum = manager.minimum_aspect_for_component(rewire)
            assert manager.aspect_for_component(rewire) == minimum < 9
            assert manager.get_layout_for_component(rewire)["color"] == [1, 2, 3, 190]
            manager.type_layouts[key]["aspect_ratio"] = minimum + 0.5
            assert manager.aspect_for_component(rewire) == minimum + 0.5
        assert manager.type_layouts["Rewire"]["aspect_ratio"] == 9

        # Reset is clocked during setup for RV32I program fixtures. Loading
        # the viewer must continue that history, not erase it. A terminal
        # status that changes only at the end must still be known before it.
        for number in (1, 5, 9):
            scenario = f"rv32i-program{number}"
            test = f"RV32ISingleCycleSystemTest/program-{number:02}"
            overrides = {"RV32I_PROGRAM_ROOT.CORE": "behavioral"}
            native = server.CircuitSession(scenario, server.LayoutManager(layout_path),
                                           profile_overrides=overrides)
            native_topology = native.topology_response()
            extra = ("--override", "RV32I_PROGRAM_ROOT.CORE", "behavioral")
            topology = probe("topology", test, scenario, *extra)
            assert topology["timestamps"] == native_topology["timestamps"]
            assert topology["checkpoints"] == native_topology["checkpoints"]
            pins = {p["name"]: p for p in topology["pins"] if p["componentId"] == topology["rootId"]}
            for checkpoint in topology["checkpoints"]:
                index = checkpoint["index"]
                state = probe("state", test, scenario, str(index), *extra)
                assert state == native.state_at_index(index)
                detail = checkpoint["detail"]
                expected = {"PC": f'{int(re.search(r"pc_after=0x([0-9a-f]+)", detail)[1], 16):032b}',
                            "HALTED": "1" if "halted=true" in detail else "0",
                            "TRAPPED": "1" if "trapped=true" in detail else "0"}
                for name, value in expected.items():
                    assert state["pins"][pins[name]["stateIndex"]] == value, (scenario, checkpoint, name)

        native = native_session("rv32i-program-loader", layout_path)
        expected = native.topology_response()
        root_id = expected["rootId"]
        actual_state = probe(
            "component",
            "RV32IProgramLoaderTest",
            "rv32i-program-loader",
            "0",
            root_id,
        )
        assert actual_state == native.component_state_at_index(0, root_id)


if __name__ == "__main__":
    main()
