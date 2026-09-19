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


for model, type_prefix in (("ff", "FFMemory"), ("sram", "SRAM"),
                           ("rom", "ROM"), ("ideal-reference", "IdealRAMReference")):
    for width in (8, 16, 32):
        for depth in (1, 4, 7, 4096):
            memory = backend.create_memory_array("M", model, width, depth)
            assert memory.get_type_name() == f"{type_prefix}{width}"
            assert not memory.get_children()
            config = backend.memory_configuration(memory)
            assert config == dict(capacity_bytes=depth * width // 8, depth_words=depth,
                                  data_width=width, record_history=True)
            address = (depth - 1) * width // 8
            image = list(range(width // 8))
            backend.load_memory_bytes(memory, address, image)
            snapshot = backend.memory_snapshot(memory, 0, address, 1)
            bits = snapshot["words"][0][1]
            assert all(str(bit).endswith("HIGH" if image[i // 8] & (1 << (i % 8)) else "LOW")
                       for i, bit in enumerate(bits))
            assert snapshot["touched_count"] == 1
            rejects(lambda: backend.load_memory_bytes(memory, config["capacity_bytes"], [1]))
        if model != "ideal-reference":
            for depth in (1, 3, 4, 7, 32):
                structural = backend.create_memory_array("M", model, width, depth, fidelity="structural")
                blocks = {child.get_name(): child for child in structural.get_children()}
                assert set(blocks) == {"ADDRESS_DECODE", "ACCESS_CONTROL", "STORAGE", "READ_SELECT"}
                words = blocks["STORAGE"].get_children()
                assert [word.get_name() for word in words] == [f"WORD_{n}" for n in range(depth)]
                cells = [child for word in words for child in word.get_children()
                         if child.get_name().startswith("CELL_")]
                assert len(cells) == depth * width
                expected_type = {"ff": "FlipFlopCell", "sram": "GatedDLatch", "rom": "ConstantValue"}[model]
                assert all(cell.get_type_name() == expected_type for cell in cells)
                if depth == 1:
                    assert not any(child.get_type_name() == "Mux2to1" for child in blocks["READ_SELECT"].get_children())
                rejects(lambda: backend.memory_configuration(structural))
            rejects(lambda: backend.create_memory_array("M", model, width, 4,
                                                             read_delay_ticks=2, fidelity="structural"))
        else:
            rejects(lambda: backend.create_memory_array("M", model, width, 4, fidelity="structural"))
        rejects(lambda: backend.create_memory_array("M", model, width, 4096 // width + 1, fidelity="structural"))

for arguments in (("sram", 64, 4), ("dram", 32, 4), ("sram", 32, 0), ("sram", 32, 2**32)):
    rejects(lambda: backend.create_memory_array("M", *arguments))
rejects(lambda: backend.create_memory_array("M", "sram", read_delay_ticks=0))
memory = backend.create_memory_array("M", "sram", record_history=False)
backend.load_memory_bytes(memory, 0, [42])
assert not backend.memory_configuration(memory)["record_history"]
rejects(lambda: backend.memory_snapshot(memory, 0))

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
adapter = backend.create_memory_bus_adapter("ADAPTER")
assert adapter.get_type_name() == "MemoryBusAdapter32"
assert adapter.get_children()
rejects(lambda: backend.memory_configuration(adapter))  # An adapter is not storage.
with tempfile.TemporaryDirectory() as directory:
    for name in ("FFMemoryContainerTest", "SRAMContainerTest", "ROMContainerTest", "MemoryBusAdapter32Test"):
        scenario = server.scenario_canonical_alias(name)
        root_id = "MEMORY_BUS_ADAPTER_ROOT" if name == "MemoryBusAdapter32Test" else "MEMORY_CONTAINER_ROOT"
        for fidelity in (("structural",) if name == "MemoryBusAdapter32Test" else ("structural", "behavioral")):
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
