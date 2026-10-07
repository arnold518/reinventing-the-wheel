"""Public SRAM factories, exact geometry, fidelity, and setup/history capability."""
import circuit_backend as backend


def rejects(call):
    try:
        call()
    except (ValueError, RuntimeError, IndexError, OverflowError):
        return
    raise AssertionError("Unsupported SRAM construction accepted")


for width in (8, 16, 32):
    for depth, k in ((1, 1), (4, 2), (6, 2), (8, 4)):
        for fidelity in ("structural", "behavioral"):
            memory = backend.create_sram("M", width, depth, k, fidelity=fidelity)
            assert memory.get_type_name() == f"SRAM<{width}>"
            assert memory.get_contract_id() == f"memory.sram.device.{width}"
            assert memory.get_selected_fidelity() == fidelity
            assert set(memory.get_input_pins()) == {"WORD_ADDR", "WRITE_DATA", "WRITE_MASK", "CE_N", "WE_N", "OE_N"}
            assert memory.get_input_pins()["WRITE_MASK"].get_width() == width // 8
            if fidelity == "structural":
                bank = next(c for c in memory.get_children() if c.get_name() == "STORAGE")
                rows = [c for c in bank.get_children() if c.get_name().startswith("ROW_")]
                bitlines = [c for c in bank.get_children() if c.get_name().startswith("BITLINE_")]
                assert len(rows) == depth // k and len(bitlines) == width * k
                assert all(len(r.get_children()) == width * k for r in rows)
                assert sum(len(r.get_children()) for r in rows) == depth * width
            else:
                assert not memory.get_children()
                assert backend.memory_configuration(memory)["capacity_bytes"] == depth * width // 8
            bank = backend.create_sram_storage_bank("B", width, depth, k, fidelity)
            assert len(bank.get_input_pins()) == depth // k + 4 * width * k
            assert len(bank.get_output_pins()) == 3 * width * k
    large = backend.create_sram("L", width, 4096, 4, fidelity="behavioral")
    address = 4095 * width // 8
    backend.load_memory_bytes(large, address, list(range(width // 8)))
    assert backend.memory_snapshot(large, 0, address)["touched_count"] == 1
    rejects(lambda: backend.load_memory_bytes(large, 4096 * width // 8, [1]))
    rejects(lambda: backend.create_sram("L", width, 4096, 4, fidelity="structural"))
    rejects(lambda: backend.create_sram_storage_bank("L", width, 4096, 4, "behavioral"))
for kind, params in (("cell", {}), ("row", {"columns": "3"}), ("bitline-pair", {"rows": "3"}),
                     ("precharge", {"columns": "3"}), ("write-driver", {}), ("sense-amplifier", {}),
                     ("row-decoder", {"depth_words": "6", "words_per_row": "2"}),
                     ("access-control.8", {}), ("column-selector.8", {"words_per_row": "1"}),
                     ("output-buffer.8", {})):
    component = backend.create_sram_component(kind, "P", params)
    assert component.get_contract_id() == "memory.sram." + kind
for args in ((8, 0, 1), (8, 5, 2), (8, 8, 3), (64, 4, 2), (8, 4, 0)):
    rejects(lambda: backend.create_sram("BAD", *args))
rejects(lambda: backend.create_sram("BAD", phase_ticks=1))
rejects(lambda: backend.create_sram_component("cell", "BAD", {"unexpected": "1"}))
rejects(lambda: backend.create_sram_component("sense-amplifier", "BAD", fidelity="behavioral"))
print("SRAM factory, geometry, fidelity, and memory setup/history tests passed")
