"""Same-named input/output pins: bindings, Python/C++ topology, and state history."""
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

import server

probe_path = sys.argv[1]


def probe(command, *args):
    return json.loads(subprocess.check_output([
        probe_path, command, "RV32IMemoryLanesTest", "rv32i-memory-lanes", *args], text=True))


def indexed(records):
    return sorted(records, key=lambda record: record["stateIndex"])


with tempfile.TemporaryDirectory() as directory:
    layout = Path(directory) / "layout.json"
    shutil.copy2(Path(__file__).resolve().parents[2] / "visualizer-v2/layout.json", layout)
    session = server.CircuitSession("rv32i-memory-lanes", server.LayoutManager(layout))
    native = session.topology_response()
    portable = probe("topology")
    assert len(session.pins) == len({p["id"] for p in session.pins})
    assert len(session.pins) == len(session.pin_handles_by_component_direction_name)
    for key in ("pins", "wires"):
        assert indexed(portable[key]) == indexed(native[key]), key
    for key in ("state", "stats", "checkpoints"):
        assert portable[key] == native[key], key
    root = session.root.get_id()
    records = {p["id"]: p for p in session.pins}
    for lane in range(4):
        component_id = f"{root}.LANE_{lane}"
        handle = session.component_handles[component_id]
        inputs, outputs = handle.get_input_pins(), handle.get_output_pins()
        for direction, pins in (("input", inputs), ("output", outputs)):
            assert set(pins) == {"LOAD", "STORE"}
            for name, pin in pins.items():
                pin_id = f"{component_id}.{direction}.{name}"
                assert pin.get_id() == pin_id
                assert records[pin_id]["type"] == direction
                # The matching wire must resolve to this side, not its namesake.
                if direction == "output":
                    assert any(w["sourcePinId"] == pin_id for w in session.wires)
                else:
                    assert any(pin_id in w["sinkPinIds"] for w in session.wires)
        try:
            session._component_pin_value(component_id, "LOAD")
        except ValueError as error:
            assert "Ambiguous pin name" in str(error)
        else:
            raise AssertionError("A directionless lookup must not choose a namesake silently")
        repeated = session.topology_slice([root, component_id, component_id])
        assert len(repeated["pins"]) == len({p["id"] for p in repeated["pins"]})
        assert indexed(probe("slice", root, component_id, component_id)["pins"]) == indexed(repeated["pins"])

    # Regress against the actual shifted values, not merely the number of ports.
    for checkpoint in session.checkpoints:
        state = session.state_at_index(checkpoint["index"])
        assert probe("state", str(checkpoint["index"])) == state
        for lane in range(4):
            cid = f"{root}.LANE_{lane}"
            for name, incoming, outgoing in (
                ("LOAD", 0x80ff3412, 0x80ff3412 >> (8 * lane)),
                ("STORE", 0xdeadbeef, (0xdeadbeef << (8 * lane)) & 0xffffffff),
            ):
                for direction, expected in (("input", incoming), ("output", outgoing)):
                    value = state["pins"][records[f"{cid}.{direction}.{name}"]["stateIndex"]]
                    assert value == f"{expected:032b}", (checkpoint, cid, direction, name, value)
                    assert session._component_pin_value(cid, name, direction) == value

    # Backends must reject duplicates before overwriting a handle/state index.
    count = len(session.pins)
    pin = session.root.get_input_pins()["WORD_DATA"]
    try:
        session._add_pin(root, "WORD_DATA", pin, "input")
    except RuntimeError as error:
        assert "duplicate pin ID" in str(error)
    else:
        raise AssertionError("Duplicate pin ID accepted")
    assert len(session.pins) == count
    print(f"Four lanes: distinct input/output IDs, wires and history at {len(session.checkpoints)} checkpoints passed")
