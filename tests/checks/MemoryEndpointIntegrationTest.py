"""Focused transaction documentation, bindings, inspector and floorplan checks."""
from pathlib import Path
import json
import re
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
PROBE = str(Path(sys.argv.pop(1)).resolve()) if len(sys.argv) > 1 else str(ROOT / "build/tests/VisualizerSessionProbe")
sys.path.append(str(ROOT / "visualizer-v2"))
sys.path.insert(0, str(ROOT / "tools"))
import circuit_backend as backend
import server
import memory_device_layout as geometry
from mkdocs_examples import render_table

# Transaction protocol, controllers and endpoints, scalable native devices.
SECTIONS = {"transactions": "transactions", "controllers": "controllers", "scalable": "technologies/scalable"}
SOURCES = {key: (ROOT / f"site-docs/memory/{page}.md").read_text() for key, page in SECTIONS.items()}
CASES = re.findall(r"<!-- simulator-examples: (\S+) (\S+) -->", "\n".join(SOURCES.values()))


class MemoryEndpointIntegrationTest(unittest.TestCase):
    def test_factories_and_limits(self):
        for model in ("ff", "sram", "dram", "rom", "nor-flash"):
            for width in (8, 16, 32):
                device = backend.create_memory_device("M", model, width, 4096)
                self.assertFalse(device.get_children())
                expected = {"ff": f"FFMemory{width}", "sram": f"SRAM<{width}>",
                            "dram": f"DRAM<{width}>", "rom": f"ROM<{width}>",
                            "nor-flash": f"NORFlash<{width}>"}
                self.assertEqual(device.get_type_name(), expected[model])
                self.assertEqual(backend.memory_configuration(device)["capacity_bytes"], 4096 * width // 8)
                outputs={"ff":{"READ_DATA","FAULT"},"sram":{"READ_DATA","READY"},"rom":{"READ_DATA","READY"},
                         "dram":{"READ_DATA","BUSY","ERROR","READ_VALID","ROW_OPEN"},
                         "nor-flash":{"READ_DATA","BUSY","ERROR","READ_VALID","WRITE_ENABLED"}}
                self.assertEqual(set(device.get_output_pins()),outputs[model])
                if model not in ("ff", "sram"):
                    with self.assertRaises(RuntimeError):
                        backend.load_memory_bytes(device, 0, [0])
                controller = backend.create_memory_controller("C", model, width)
                children={c.get_name() for c in controller.get_children()}
                if model == "ff":
                    self.assertTrue({"REQUEST_CHECK", "RESPONSE", "DEVICE_CLOCK"} <= children)
                    self.assertNotIn("DEVICE_CONTROL", children)
                else:
                    self.assertEqual(children, {"TRANSACTION", "DEVICE_CONTROL"})
                for fidelity in ("structural", "behavioral"):
                    endpoint = backend.create_memory_endpoint("E", model, width, fidelity=fidelity)
                    self.assertEqual(bool(endpoint.get_children()), fidelity == "structural")
                with self.assertRaises(ValueError):
                    backend.create_memory_endpoint("BIG", model, width, 4096)
        with self.assertRaises(ValueError):  # the old relay technology is retired
            backend.create_memory_controller("C", "container", 8)
        for parameters in ({"depth_words": "0"}, {"paged": "2"}, {"oops": "1"},
                           {"max_resident_bytes": "0"}, {"guard_ticks": "1"}):
            # depth_words is a dedicated argument and intentionally overrides the map.
            if "depth_words" in parameters:
                with self.assertRaises(ValueError): backend.create_memory_device("M", "sram", 8, 0)
            else:
                with self.assertRaises(ValueError): backend.create_memory_device("M", "sram", parameters=parameters)
        huge = backend.create_memory_device("HUGE", "sram", 8, 1 << 28,
                    {"record_history": "0", "max_resident_bytes": "16384"})
        self.assertEqual(backend.memory_configuration(huge)["capacity_bytes"], 1 << 28)

    def test_documents_and_exported_rows(self):
        self.assertEqual(len(CASES), 9)
        for source in SOURCES.values():
            self.assertIn("| Component | Description | Structural | Behavioral |", source)
            for section in re.split(r"(?m)^## [\d.]+ `", source)[1:]:
                self.assertEqual(re.findall(r"(?m)^### (.+)$", section),
                                 ["IO pins", "Behavior", "Implementation", "Timing", "Examples", "Source and tests"])
                behavior = section.split("### Behavior\n")[1].split("### Implementation\n")[0]
                self.assertEqual(re.findall(r"(?m)^#### (.+)$", behavior)[-2:],
                                 ["C++ example", "Unknown inputs"])
                self.assertEqual(section.count("```cpp\n"), 1)
        for scenario, test in CASES:
            topology = json.loads(subprocess.check_output([PROBE, "topology", test, scenario], text=True))
            examples = json.loads(subprocess.check_output([PROBE, "examples", test, scenario], text=True))
            self.assertEqual(len(examples["rows"]), len(topology["checkpoints"]))
            self.assertIn('class="example-table-scroll"', render_table(examples, scenario, test))
            for row, checkpoint in zip(examples["rows"], topology["checkpoints"]):
                self.assertEqual((row["checkpoint"], row["time"], row["timelineIndex"]),
                                 (checkpoint["label"], checkpoint["time"], checkpoint["index"]))

    def test_python_portable_inspector(self):
        for scenario, test in CASES[2:]:
            if not scenario.startswith("memory-endpoint-"): continue
            if scenario == "native-memory-device": continue
            with tempfile.TemporaryDirectory() as tmp:
                session = server.CircuitSession(scenario, server.LayoutManager(Path(tmp) / "layout.json"),
                                               profile_overrides={"MEMORY_ENDPOINT_ROOT": "behavioral"})
                for index in (0, len(session.timestamps) - 1):
                    state = session.component_state_at_index(index, "MEMORY_ENDPOINT_ROOT")
                    native = json.loads(subprocess.check_output([PROBE, "component", test, scenario, str(index),
                        "MEMORY_ENDPOINT_ROOT", "--override", "MEMORY_ENDPOINT_ROOT", "behavioral"], text=True))
                    self.assertEqual(state, native)
                    self.assertTrue(state["supported"])
                    self.assertIsNotNone(state["transaction"])
                    self.assertIn(state["transaction"]["phase"], ("reset", "idle", "offered", "request-wait", "pending", "response", "response-held"))
                    self.assertIn("RSP_STATUS", state["ports"])
                    self.assertEqual(len(state["windowWords"]), 4)

    def test_native_device_inspector_and_cpp_example(self):
        scenario, test = "native-memory-device", "NativeMemoryDeviceTest"
        topology = json.loads(subprocess.check_output([PROBE, "topology", test, scenario], text=True))
        root = topology["rootId"]
        with tempfile.TemporaryDirectory() as tmp:
            session = server.CircuitSession(scenario, server.LayoutManager(Path(tmp) / "layout.json"))
            for index in (0, len(session.timestamps) - 1):
                state = session.component_state_at_index(index, root)
                native = json.loads(subprocess.check_output([PROBE, "component", test, scenario,
                                    str(index), root], text=True))
                self.assertEqual(state, native)
                self.assertTrue(state["supported"])
                self.assertIn("FAULT", state["ports"])
                self.assertEqual(len(state["windowWords"]), 4)
            example = re.findall(r"```cpp\n(.*?)\n```", SOURCES["scalable"], re.S)[0]
            rows = json.loads(subprocess.check_output([PROBE, "examples", test, scenario], text=True))["rows"]
            cpp = [example, '#include <cassert>', '''
                using namespace memory_native_example;
                Bits parse(std::string s) {
                    Bits b; for(auto it=s.rbegin();it!=s.rend();++it)
                        b.push_back(*it=='0'?L:*it=='1'?H:*it=='Z'?Z:X);
                    return b;
                }
                int main() { FF<8> model(4); FF<8>::Inputs in;
            ''']
            for row in rows:
                for name, value in row["inputs"].items():
                    expr = f'parse("{value}")' + ('[0]' if len(value) == 1 and name != "WRITE_MASK" else '')
                    cpp.append(f'in.{name}={expr};')
                cpp.append(f'{{auto out=model.evaluate({row["time"]},in);')
                for name, value in row["expectedOutputs"].items():
                    cpp.append(f'assert(out.{name}==parse("{value}"){ "[0]" if name == "FAULT" else ""});')
                cpp.append('}')
            cpp.append('}')
            binary = Path(tmp) / "native-example"
            library = Path(PROBE).resolve().parent.parent / "libSimulatorLib.a"
            subprocess.run(["c++", "-std=c++20", "-Wall", "-Wextra", "-Werror", "-pedantic",
                            "-I", str(ROOT / "include"), "-x", "c++", "-", "-x", "none",
                            str(library), "-o", str(binary)],
                           input="\n".join(cpp), text=True, check=True, timeout=60)
            subprocess.run([str(binary)], check=True, timeout=30)

    def test_layouts(self):
        def walk(c):
            yield c
            for child in c.get_children(): yield from walk(child)
        for model, width in ((m, w) for m in ("ff", "sram", "dram", "rom", "nor-flash") for w in (8, 16, 32)):
            root = backend.create_memory_endpoint("E", model, width)
            with tempfile.TemporaryDirectory() as tmp:
                manager = server.LayoutManager(Path(tmp) / "layout.json")
                nodes = list(walk(root))
                for c in nodes: manager.ensure_component_layout_defaults(c)
                for c in nodes:
                    kind = c.get_type_name()
                    if not c.get_children(): continue
                    if not kind.startswith(("MemoryEndpoint<", "MemoryController<", "MemoryTransactionPort<", "Register<")): continue
                    aspects = {ch.get_name(): manager.aspect_for_component(ch) for ch in c.get_children()}
                    aspect = manager.aspect_for_component(c)
                    positions = geometry.placements(kind, aspects, aspect)
                    self.assertEqual(set(positions), set(aspects))
                    rectangles = []
                    for n, p in positions.items():
                        x, y = p["rel_pos"]; w = p["rel_width"]; h = w * aspects[n] / aspect
                        self.assertGreaterEqual(min(x, y), 0)
                        self.assertLessEqual(max(x+w, y+h), 1.00001)
                        rectangles.append((n, x, y, w, h))
                    for i, a in enumerate(rectangles):
                        for b in rectangles[i+1:]:
                            self.assertFalse(min(a[1]+a[3], b[1]+b[3]) > max(a[1], b[1]) and
                                             min(a[2]+a[4], b[2]+b[4]) > max(a[2], b[2]), (kind, a, b))
                    if kind.startswith("Register<"):
                        bits = [n for n in positions if n.startswith("BIT_")]
                        self.assertEqual(sorted(bits, key=lambda n: (round(positions[n]["rel_pos"][1], 7), positions[n]["rel_pos"][0])),
                                         sorted(bits, key=lambda n: int(n[4:])))
                        rows = {round(positions[n]["rel_pos"][1], 7) for n in bits}
                        self.assertEqual(len(rows), (len(bits) + 7) // 8)
                    if kind.startswith("MemoryTransactionPort<"):
                        self.assertIn("PENDING", positions)
                        self.assertIn("RESPONSE", positions)
                        self.assertNotIn("PHASE_0", positions)
                        names=("REQUEST_OP", "REQUEST_ADDR", "REQUEST_DATA", "REQUEST_MASK", "RESPONSE")
                        self.assertEqual(sorted(names, key=lambda n: positions[n]["rel_pos"][0]), list(names))

    def test_native_sram_inside_controller_inspector(self):
        # A native device can be compact while its controller stays expanded.
        overrides={"MEMORY_ENDPOINT_ROOT":"structural", "MEMORY_ENDPOINT_ROOT.MEMORY":"behavioral"}
        with tempfile.TemporaryDirectory() as tmp:
            session=server.CircuitSession("memory-endpoint-sram",server.LayoutManager(Path(tmp)/"layout.json"),
                                          profile_overrides=overrides)
            for index in (0,len(session.timestamps)-1):
                state=session.component_state_at_index(index,"MEMORY_ENDPOINT_ROOT.MEMORY")
                native=json.loads(subprocess.check_output([PROBE,"component","MemoryEndpointSRAMTest",
                    "memory-endpoint-sram",str(index),"MEMORY_ENDPOINT_ROOT.MEMORY",
                    *[x for p,f in overrides.items() for x in ("--override",p,f)]],text=True))
                self.assertEqual(state,native)
                self.assertTrue(state["supported"])
                self.assertEqual(len(state["windowWords"]),4)
                self.assertIn("WE_N",state["ports"])

    def test_cpp_examples_compile(self):
        port_example = re.findall(r"```cpp\n(.*?)\n```", SOURCES["transactions"], re.S)[0]
        check, endpoint = re.findall(r"```cpp\n(.*?)\n```", SOURCES["controllers"], re.S)
        native_device = re.findall(r"```cpp\n(.*?)\n```", SOURCES["scalable"], re.S)[0]
        snippets = [port_example, check, "\n".join((check, endpoint)), native_device]
        # References must state the rules, not call the code under test.
        for snippet in (check, endpoint, native_device):
            self.assertNotRegex(snippet, r"\b(?:NativeSequence|DeviceModel|makeDeviceModel|validateRequest)\b")
        for snippet in snippets:
            # Compiler stdin avoids generated source files in the checkout.
            subprocess.run(["c++", "-std=c++20", "-I", str(ROOT / "include"),
                            "-I", str(ROOT / "src/modules/memory"), "-x", "c++", "-fsyntax-only", "-"],
                           input=snippet, text=True, check=True)

    def run_fixture(self, fixture, sections):
        snippets = re.findall(r"```cpp\n(.*?)\n```", "\n".join(SOURCES[s] for s in sections), re.S)
        harness = (ROOT / "tests/fixtures/memory" / fixture).read_text()
        library = Path(PROBE).resolve().parent.parent / "libSimulatorLib.a"
        with tempfile.TemporaryDirectory(prefix="memory-doc-examples-") as tmp:
            binary = Path(tmp) / "example"
            subprocess.run(["c++", "-std=c++20", "-Wall", "-Wextra", "-Werror", "-pedantic",
                            "-I", str(ROOT / "include"), "-I", str(ROOT / "src/modules/memory"),
                            "-x", "c++", "-", "-x", "none", str(library), "-o", str(binary)],
                           input="\n".join(snippets) + "\n" + harness, text=True, check=True, timeout=120)
            subprocess.run([str(binary)], check=True, timeout=60)

    def test_compact_endpoint_cpp_example_behavior(self):
        # The request check against the production check; the endpoint reference's answers.
        self.run_fixture("compact-endpoint-example.cpp", ("transactions", "controllers"))

    def test_transaction_controller_cpp_examples_behavior(self):
        self.run_fixture("transaction-controller-examples.cpp", ("transactions",))

    def test_documented_public_pin_tables(self):
        for section,scenario,test in (("transactions","memory-transaction-port","MemoryTransactionPortTest"),
                                      ("scalable","native-memory-device","NativeMemoryDeviceTest")):
            topology=json.loads(subprocess.check_output([PROBE,"topology",test,scenario],text=True))
            pins={p["name"]:(p["type"],p["width"]) for p in topology["pins"] if p["componentId"]==topology["rootId"]}
            documented={}
            io=SOURCES[section].split("### IO pins\n")[1].split("### Behavior\n")[0]
            for names,direction,width in re.findall(r"(?m)^\| ([^|]+) \| (Input|Output) \| ([^|]+) \|",io):
                width=width.strip();size={"W":8,"W/8":1}.get(width)
                if size is None:size=int(width)
                for name in re.findall(r"`([^`]+)`",names):documented[name]=(direction.lower(),size)
            self.assertEqual(documented,pins)


if __name__ == "__main__": unittest.main()
