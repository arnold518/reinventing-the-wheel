"""Focused memory observation regression tests, one simulated core at a time."""
import gc
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
PROBE = str(Path(sys.argv.pop(1)).resolve()) if len(sys.argv) > 1 else str(ROOT / "build/tests/VisualizerSessionProbe")
sys.path.append(str(ROOT / "visualizer-v2"))
import server
import circuit_backend as backend

CORES = (("rv32i-program3", "RV32ISingleCycleSystemTest/program-03"),
         ("rv32i-five-stage-program3", "RV32IFiveStageCoreProgramTest/program-03"),
         ("rv32i-baseline-program3", "RV32IBaselinePipelineProgramTest/program-03"))


class MemoryInspectorIntegrationTest(unittest.TestCase):
    def test_technology_activity_and_held_payload(self):
        expected_signals = {
            "ff": set(), "sram": {"WORD_ADDR", "CE_N", "OE_N", "WE_N"},
            "rom": {"WORD_ADDR", "CE_N", "OE_N"},
            "dram": {"ADDR", "CMD", "BUSY", "ERROR", "READ_VALID", "ROW_OPEN"},
            "flash": {"WORD_ADDR", "CMD", "BUSY", "ERROR", "READ_VALID", "WRITE_ENABLED"},
        }
        for tech, signals in expected_signals.items():
            with tempfile.TemporaryDirectory() as tmp:
                session=server.CircuitSession("memory-endpoint-"+tech,
                    server.LayoutManager(Path(tmp)/"layout.json"),
                    profile_overrides={"MEMORY_ENDPOINT_ROOT":"behavioral"})
                phases=set(); seen_signals=set(); held=[]; operations=set(); unknown=False; device_states=set()
                for index in range(len(session.timestamps)):
                    state=session.component_state_at_index(index,"MEMORY_ENDPOINT_ROOT")
                    activity=state["deviceActivity"]
                    phases.add(activity["phase"]); seen_signals.update(activity["signals"])
                    device_states.update((name,value) for name,value in activity["signals"].items()
                                         if name in ("ROW_OPEN","WRITE_ENABLED"))
                    operations.add(state["transaction"]["operation"])
                    unknown |= any("X" in row["bits"] for row in state["touchedWords"])
                    if state["transaction"]["phase"]=="response-held" and state["transaction"]["source"]=="accepted":
                        held.append((index,state))
                        # Scenario deliberately overwrites the live request
                        # address after acceptance. Highlight the saved one.
                        if state["ports"]["REQ_ADDR"]["value"]=="1"*32:
                            self.assertNotEqual(state["activeAddress"],0xffffffff)
                self.assertEqual(seen_signals,signals,tech)
                self.assertTrue(held,tech)
                self.assertIn("READ",operations)
                if tech in ("dram","flash"):
                    self.assertTrue({"command-raise","command-lower","command-wait"}<=phases,(tech,phases))
                if tech=="flash": self.assertTrue({"PROGRAM","ERASE"}<=operations)
                status_pin={"dram":"ROW_OPEN","flash":"WRITE_ENABLED"}.get(tech)
                if status_pin: self.assertTrue({(status_pin,"0"),(status_pin,"1")}<=device_states)
                if tech!="rom": self.assertTrue(unknown,tech)
                index,before=held[0]
                self.assertEqual(session.component_state_at_index(index,"MEMORY_ENDPOINT_ROOT"),before)
                del session
            gc.collect()

    def test_all_core_memory_roles_activity_and_python_native_parity(self):
        for alias, test in CORES:
            with tempfile.TemporaryDirectory() as tmp:
                overrides = {"RV32I_PROGRAM_ROOT.CORE": "behavioral"}
                session = server.CircuitSession(alias, server.LayoutManager(Path(tmp) / "layout.json"), profile_overrides=overrides)
                for name, role in (("INSTRUCTION_MEMORY", "instruction"), ("DATA_MEMORY", "data")):
                    cid = "RV32I_PROGRAM_ROOT." + name
                    found = {}
                    for index in range(len(session.timestamps)):
                        state = session.component_state_at_index(index, cid)
                        tx = state["transaction"]
                        if state["windowActive"] and state["activeAddress"] and tx["operation"] not in found:
                            found[tx["operation"]] = (index, state)
                        if len(found) == (1 if role == "instruction" else 2): break
                    self.assertIn("READ", found, (alias, name))
                    if role == "data": self.assertIn("WRITE", found)
                    for index, state in found.values():
                        native = json.loads(subprocess.check_output([PROBE, "component", test, alias, str(index), cid,
                            "--override", "RV32I_PROGRAM_ROOT.CORE", "behavioral"], text=True))
                        self.assertEqual(state, native)
                        self.assertEqual(state["memoryRole"], role)
                        self.assertIn(state["activeBaseAddress"], [row["address"] for row in state["windowWords"]])
                        if role == "instruction": self.assertTrue(any(row.get("instructionText") for row in state["touchedWords"]))
                    index, before = next(iter(found.values()))
                    session.component_state_at_index(len(session.timestamps)-1, cid)
                    self.assertEqual(session.component_state_at_index(index,cid), before)
                    self.assertEqual(set(session.component_handles[cid].get_available_fidelities()), {"structural", "behavioral"})
                with self.assertRaises(RuntimeError):
                    backend.initialize_memory_bytes(session.component_handles[cid], 0, [1])
                del session
            gc.collect()

    def test_compact_storage_in_expanded_cpu_endpoints(self):
        # Exercise the real program loader through a visible controller, and
        # inherit disassembly/bus context down to its behavioral native device.
        for alias, _ in CORES:
            with tempfile.TemporaryDirectory() as tmp:
                overrides = {"RV32I_PROGRAM_ROOT.CORE":"behavioral",
                    "RV32I_PROGRAM_ROOT.INSTRUCTION_MEMORY":"structural",
                    "RV32I_PROGRAM_ROOT.DATA_MEMORY":"structural"}
                session=server.CircuitSession(alias,server.LayoutManager(Path(tmp)/"layout.json"),profile_overrides=overrides)
                cid="RV32I_PROGRAM_ROOT.INSTRUCTION_MEMORY"
                self.assertEqual({c.get_name() for c in session.component_handles[cid].get_children()}, {"CONTROLLER","MEMORY"})
                native=session.component_handles[cid+".MEMORY"]
                self.assertEqual(native.get_type_name(),"MemoryContainer<ff,32>")
                self.assertFalse(native.get_children())
                self.assertEqual(backend.memory_configuration(native)["depth_words"],65536)
                state=session.component_state_at_index(0,cid+".MEMORY")
                self.assertEqual(state["memoryRole"],"instruction")
                self.assertTrue(any(row.get("instructionText") for row in state["touchedWords"]))
                self.assertEqual(state["transaction"]["endpointId"],cid)
                del session
            gc.collect()

    def test_large_endpoint_page_and_setup_binding(self):
        fixture=backend.create_test_by_name("MemoryEndpointFFTest")
        fixture.setup_circuit()
        memory=backend.create_memory_endpoint("INSTRUCTION_MEMORY","rom",32,512,fidelity="behavioral")
        backend.initialize_memory_bytes(memory,0,[0x13,0,0,0]*512)
        inspector=backend.MemoryInspector(memory,fixture.get_simulator())
        page=json.loads(inspector.state_json(0,0,"",1600,8))
        self.assertEqual(page["touchedWordCount"],512)
        self.assertEqual(len(page["touchedWords"]),256)
        self.assertEqual([r["address"] for r in page["windowWords"]],list(range(1600,1632,4)))
        self.assertTrue(all(r["instructionText"]=="addi x0, x0, 0" for r in page["windowWords"]))
        for address in (-2,2048,2**32):
            with self.assertRaises(ValueError): inspector.state_json(0,0,"",address,8)
        for count in (0,257):
            with self.assertRaises(ValueError): inspector.state_json(0,0,"",0,count)


if __name__ == "__main__": unittest.main()
