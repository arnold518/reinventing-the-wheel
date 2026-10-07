"""Validate 4.5 navigation, complete IO tables, snippets, and scenario exports."""
from pathlib import Path
import json
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
PROBE = sys.argv[1] if len(sys.argv) > 1 else str(ROOT / "build/tests/VisualizerSessionProbe")
sys.path.insert(0, str(ROOT / "tools"))
from mkdocs_examples import render_table

docs = ROOT / "site-docs/rv32i/4.5"
nav = (ROOT / "mkdocs.yml").read_text()
index = (docs / "index.md").read_text()
examples = (docs / "examples.hpp").read_text()
saved_layout = json.loads((ROOT / "visualizer-v2/layout.json").read_text())


def verify_static_layout(topology):
    """A static preview cannot ask the Python server to fill missing geometry."""
    components = topology["components"]
    for child in components:
        if child["parentIndex"] is None:
            continue
        parent = components[child["parentIndex"]]
        kind = parent.get("layoutType", parent["type"])
        scopes = [saved_layout["type_layouts"].get(kind, {}),
                  saved_layout["profile_layouts"].get(parent.get("profileFingerprint"), {}).get(kind, {})]
        if parent["id"] == topology["rootId"]:
            scopes.append(saved_layout["root_layouts"].get(topology["layoutKey"], {}))
        placement = {}
        for scope in scopes:
            placement.update(scope.get("children", {}).get(child["name"], {}))
        assert "rel_pos" in placement and placement.get("rel_width", 0) > 0, child["id"]


def verify_control_actions(source, topology, rows, override=()):
    """Keep the small explanatory signal table tied to real simulated values."""
    table = source.split("<!-- baseline-control-actions:start -->")[1].split(
        "<!-- baseline-control-actions:end -->")[0]
    entries = re.findall(r"(?m)^\| `([^`]+)` \| ([01]) \| ([01]) \| ([01]) \| "
                         r"([01]) / ([01]):[^|]+\| ([01]) / ([01]):[^|]+\|$", table)
    assert [entry[0] for entry in entries] == ["normal-flow", "raw-wait", "branch-waits-for-operand",
                                             "branch-enters-execute", "branch-resolves"]
    pins = {pin["name"]: pin["stateIndex"] for pin in topology["pins"]
            if pin["componentId"] == topology["rootId"]}
    checkpoints = {row["checkpoint"]: row for row in rows["rows"]}
    names = ["FETCH_VALID", "PC_WRITE", "PC_REDIRECT", "IF_ID_WRITE", "IF_ID_FLUSH",
             "ID_EX_WRITE", "ID_EX_FLUSH"]
    for label, *values in entries:
        row = checkpoints[label]
        state = json.loads(subprocess.check_output([
            PROBE, "state", "RV32IBaselineControlTest", "rv32i-baseline-control",
            str(row["timelineIndex"]), *override], text=True, timeout=90))
        assert state["time"] == row["time"]
        expected = dict(zip(names, values))
        expected.update(EX_MEM_WRITE="1", MEM_WB_WRITE="1", EX_MEM_FLUSH="0", MEM_WB_FLUSH="0")
        for name, value in expected.items():
            assert state["pins"][pins[name]] == value, (label, name, value, override)
    print(f"Control walkthrough: all five signal rows match {override[-1] if override else 'structural'} simulation", flush=True)


for number, slug in enumerate(("registers", "fetch", "decode", "execute", "memory", "writeback", "control", "core"), 1):
    source = (docs / (slug + ".md")).read_text()
    assert source.startswith(f"# § 4.5.{number}.")
    assert f"rv32i/4.5/{slug}.md" in nav and f"({slug}.md)" in index
    assert "| Component | Description | Structural | Behavioral |" in source
    for title in ("IO pins", "Behavior", "Implementation", "Structural implementation", "Behavioral implementation", "Examples", "Tests"):
        assert re.search(r"(?m)^#{2,4} " + title + r"$", source), (slug, title)
    for tag in re.findall(r'examples.hpp:([a-z-]+)"', source):
        assert f"// --8<-- [start:{tag}]" in examples and f"// --8<-- [end:{tag}]" in examples
    markers = re.findall(r"<!-- simulator-examples: (\S+) (\S+) -->", source)
    tables = re.findall(r"#{2,3} IO pins\n(.*?)#{2,3} Behavior\n", source, re.S)
    assert len(tables) == (2 if slug in {"control", "writeback"} else 1)
    for number, (alias, test) in enumerate(markers):
        topology = json.loads(subprocess.check_output([PROBE, "topology", test, alias], text=True, timeout=90))
        verify_static_layout(topology)
        root = next(c for c in topology["components"] if c["id"] == topology["rootId"])
        assert set(root["availableFidelities"]) == {"structural", "behavioral"}
        pins = {p["name"]: (p["type"], p["width"]) for p in topology["pins"] if p["componentId"] == root["id"]}
        if slug in {"writeback", "core"}:
            assert not any(name.startswith("RETIRED_") for name in pins)
        if test == "RV32IWritebackStageTest":
            root_index = topology["components"].index(root)
            children = [c for c in topology["components"] if c["parentIndex"] == root_index]
            assert not any(c["name"].startswith("RETIRED_") for c in children)
            assert not any(c["type"] in {"Register<32>", "FlipFlopCell", "DFlipFlop"} for c in topology["components"])
            assert not ({"CLK", "RST", "PC", "IN_NEXT_PC", "HALTED", "TRAPPED", "IN_PC", "IN_INSTRUCTION", "IN_STORE_DATA"} & pins.keys())
        if test == "RV32IPipelineExecutionStatusTest":
            root_index = topology["components"].index(root)
            children = [c for c in topology["components"] if c["parentIndex"] == root_index]
            assert {c["name"] for c in children if c["type"] == "FlipFlopCell"} == {
                "HALTED_STATE", "TRAPPED_STATE"}
            assert any(c["name"] == "TRAP_CAUSE_WORD" and
                       c["type"] == "Register<4>" for c in children)
        if slug == "core":
            assert "PC" not in pins
            assert any(c["name"] == "EXECUTION_STATUS" for c in topology["components"])
        table = tables[number if slug in {"control", "writeback"} else 0]
        documented = {}
        for names, direction, widths in re.findall(r"(?m)^\| ([^|]+) \| (Input|Output) \| ([^|]+) \|", table):
            names = re.findall(r"`([^`]+)`", names)
            widths = [int(w) for w in re.findall(r"\d+", widths)]
            if len(widths) == 1:
                widths *= len(names)
            assert len(widths) == len(names), (slug, names, widths)
            for pin, width in zip(names, widths):
                if "<field>" in pin:
                    prefix = pin.split("<field>")[0]
                    expanded = [p for p, (d, w) in pins.items() if p.startswith(prefix) and d == direction.lower() and w == width]
                    if slug == "memory":
                        expanded = [p for p in expanded if p not in {"OUT_MEMORY_DATA", "OUT_MEMORY_STATUS"}]
                else:
                    expanded = [pin]
                for name in expanded:
                    assert name not in documented, (slug, name)
                    documented[name] = (direction.lower(), width)
        assert documented == pins, (slug, set(pins.items()) ^ set(documented.items()))
        rows = json.loads(subprocess.check_output([PROBE, "examples", test, alias], text=True, timeout=90))
        assert len(rows["rows"]) == len(topology["checkpoints"])
        assert 'class="example-table-scroll"' in render_table(rows, alias, test)
        assert f'scenario={alias}&amp;embed=1' in source
        print(f"{test}: all {len(pins)} pins, both fidelities, {len(rows['rows'])} exported rows", flush=True)
        if test == "RV32IBaselineControlTest":
            verify_control_actions(source, topology, rows)
            override = ["--override", topology["rootId"], "behavioral"]
            behavioral_topology = json.loads(subprocess.check_output(
                [PROBE, "topology", test, alias, *override], text=True, timeout=90))
            behavioral_rows = json.loads(subprocess.check_output(
                [PROBE, "examples", test, alias, *override], text=True, timeout=90))
            verify_control_actions(source, behavioral_topology, behavioral_rows, override)
print("4.5 documentation contracts passed")
