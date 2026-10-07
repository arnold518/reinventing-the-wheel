"""Configurable memory construction and Python/portable inspector parity."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile

import circuit_backend as backend
import server


def rejects(function):
    try:
        function()
    except (ValueError, RuntimeError, IndexError, OverflowError):
        return
    raise AssertionError("Invalid memory operation was accepted")


for width in (8, 16, 32):
    for depth in (1, 4, 7, 4096):
        memory = backend.create_ff_memory("M", width, depth, fidelity="behavioral")
        config = backend.memory_configuration(memory)
        assert config == dict(capacity_bytes=depth * width // 8, depth_words=depth,
                             data_width=width, record_history=True)
        address = (depth - 1) * width // 8
        image = list(range(width // 8))
        backend.load_memory_bytes(memory, address, image)
        snapshot = backend.memory_snapshot(memory, 0, address, 1)
        bits = snapshot["words"][0][1]
        assert all(str(bit).endswith("HIGH" if image[i // 8] & (1 << (i % 8)) else "LOW") for i, bit in enumerate(bits))
        assert snapshot["touched_count"] == 1
        rejects(lambda: backend.load_memory_bytes(memory, config["capacity_bytes"], [1]))
    rejects(lambda: backend.create_ff_memory("M", width, 4, read_delay_ticks=2))
rejects(lambda: backend.create_ff_memory("M", 64))
rejects(lambda: backend.create_ff_memory("M", 32, 0))
rejects(lambda: backend.create_ff_memory("M", read_delay_ticks=0))
memory = backend.create_ff_memory("M", record_history=False, fidelity="behavioral")
backend.load_memory_bytes(memory, 0, [42])
assert not backend.memory_configuration(memory)["record_history"]
rejects(lambda: backend.memory_snapshot(memory, 0))
# Removed technology-agnostic factories must not silently select a different port.
assert not hasattr(backend, "create_memory_array")
assert not hasattr(backend, "create_memory_bus_adapter")

probe = Path(sys.argv[1]).resolve()
for width in (8, 16, 32):
    device = backend.create_ff_memory("FF", width, 3)
    assert device.get_type_name() == f"FFMemory{width}"
    assert server._component_layout_type(device) == f"FFArray{width}"
    bank = next(c for c in device.get_children() if c.get_name() == "STORAGE")
    assert bank.get_type_name() == f"FFStorageBank<{width},3>"
    assert server._component_layout_type(bank) == f"MemoryStorageBank<FFArray,{width},3>"
    for word in bank.get_children():
        assert word.get_type_name() == f"FFWord<{width}>"
        assert word.get_contract_id() == f"memory.ff-word.{width}"
        assert server._component_layout_type(word) == f"MemoryWord<FFArray,{width}>"
    for fidelity in ("structural", "behavioral"):
        word = backend.create_ff_word("W", width, fidelity)
        assert bool(word.get_children()) == (fidelity == "structural")
        assert word.get_type_name() == f"FFWord<{width}>"
        for depth in (1, 3, 7):
            bank = backend.create_ff_storage_bank("BANK", width, depth, fidelity)
            assert bank.get_type_name() == f"FFStorageBank<{width},{depth}>"
            assert bank.get_contract_id() == f"memory.ff-storage-bank.{width}"
            assert len(bank.get_input_pins()) == 4 + depth
            assert len(bank.get_output_pins()) == depth
            assert len(bank.get_children()) == (depth if fidelity == "structural" else 0)
        rejects(lambda: backend.create_ff_storage_bank("BAD", width, 0, fidelity))
        rejects(lambda: backend.create_ff_storage_bank("BAD", width, 4096 // width + 1, fidelity))
    large = backend.create_ff_memory("BIG", width, 4096, fidelity="behavioral")
    assert not large.get_children()
rejects(lambda: backend.create_ff_word("BAD", 64))
rejects(lambda: backend.create_ff_word("BAD", 8, "guess"))
rejects(lambda: backend.create_ff_storage_bank("BAD", 64))
rejects(lambda: backend.create_ff_storage_bank("BAD", 8, 3, "guess"))
rejects(lambda: backend.create_ff_memory("BIG", 32, 4096))
timing_baseline = json.loads((Path(__file__).parent / "fixtures" / "memory-array-timing.json").read_text())["traces"]
with tempfile.TemporaryDirectory() as directory:
    for name in ("FFMemoryTest",):
        scenario = server.scenario_canonical_alias(name)
        root_id = "MEMORY_CONTAINER_ROOT"
        for fidelity in ("structural", "behavioral"):
            overrides = {root_id: fidelity}
            session = server.CircuitSession(scenario, server.LayoutManager(Path(directory) / "layout.json"),
                                            profile_overrides=overrides)
            topology = session.topology_response()
            if fidelity == "structural" and scenario in timing_baseline:
                # Check every recorded time, not just settled checkpoints. The
                # word bus boundaries must not add a tick to the old gate paths.
                trace = []
                ports = session.root.get_output_pins()
                for timestamp in session.timestamps:
                    session.simulator.set_circuit_state_at_time(timestamp)
                    values = [server._signal_value(ports[pin]) for pin in ("READ_DATA", "FAULT")]
                    if not trace or trace[-1][1:] != values:
                        trace.append([timestamp, *values])
                assert trace == timing_baseline[scenario], (scenario, "Structural output timing changed", trace)
            options = ["--override", root_id, fidelity]
            for index in sorted({0, len(topology["timestamps"]) // 2, len(topology["timestamps"]) - 1}):
                actual = json.loads(subprocess.check_output([
                    str(probe), "component", name, scenario, str(index), topology["rootId"], *options]))
                assert actual == session.component_state_at_index(index, topology["rootId"]), (name, fidelity, index)
print("Memory container factory and inspector parity passed")
