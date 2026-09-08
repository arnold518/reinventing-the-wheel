"""Parity checks for the portable C++ visualizer facade used by WebAssembly."""

from __future__ import annotations

import json
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

        native = native_session("memory64kx32", layout_path)
        expected = native.topology_response()
        root_id = expected["rootId"]
        actual_state = probe(
            "component",
            "Memory64Kx32Test",
            "memory64kx32",
            "0",
            root_id,
        )
        assert actual_state == native.component_state_at_index(0, root_id)


if __name__ == "__main__":
    main()
