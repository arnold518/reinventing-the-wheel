"""4.3 grouping, hierarchy-local wiring and generated layout regression checks."""
import gc
from pathlib import Path
import runpy
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.append(str(ROOT / "visualizer-v2"))
import server
import rv32i_layout

generators = runpy.run_path(str(ROOT / "tools/generate-rv32i-building-block-layouts.py"))
SCENARIOS, ALU_SCENARIOS = generators["SCENARIOS"], generators["ALU_SCENARIOS"]
default_placements = generators["placements"]
COUNTS = {"RV32IDecodeControlUnit": 5, "RV32IMemoryLanes": 9,
          "RV32IMemoryRequest": 45, "RV32IExecutionControlStatusUnit": 68}


def walk(component):
    yield component
    for child in component.get_children():
        yield from walk(child)


def check_stub_regressions():
    """Exercise mixed child styles through both automatic placement paths."""
    class Parent:
        def get_type_name(self): return "UnwiredTestParent"
        def get_wires(self): return []

    test = server.circuit_backend.create_test_by_name("ALU32CompareUnitTest")
    test.setup_circuit()
    zero = next(c for c in test.get_root().get_children() if c.get_name() == "ARITHMETIC_ZERO")
    children = list(zero.get_children())
    with tempfile.TemporaryDirectory() as directory:
        manager = server.LayoutManager(Path(directory) / "layout.json")
        for c in [zero, *children]: manager.ensure_component_layout_defaults(c)
        # A child override must win over both the parent's compact pins and
        # the child's base type. Include a pin wider than its boundary stub.
        splitter = next(c for c in children if c.get_name() == "A_SPLIT")
        fingerprint = server._component_profile_fingerprint(splitter)
        store = (manager.type_layouts if fingerprint == "explicit" else
                 manager.profile_layouts.setdefault(fingerprint, {}))
        store.setdefault(server._component_layout_type(splitter), {}).update(
            boundary_area_ratio=.07, pin_size_ratio=.20)
        ratios = manager.child_stub_ratios(children)
        assert ratios[splitter.get_id()] == .10, "child profile/pin extent ignored"
        for parent in (zero, Parent()):
            for margin in (.025, .055):
                generated = server._layered_graph_layout(parent, children, .8, .1,
                    {c.get_id(): manager.aspect_for_component(c) for c in children},
                    margin, .018, ratios)
                rectangles = []
                for child in children:
                    p = generated[child.get_name()]
                    x, y = p["rel_pos"]; w = p["rel_width"]; stub = w * ratios[child.get_id()]
                    assert x-stub >= margin-1e-12, (parent.get_type_name(), child.get_name(), "left stub")
                    assert x+w+stub <= 1-margin+1e-12, (parent.get_type_name(), child.get_name(), "right stub")
                    rectangles.append((x-stub,y,w+2*stub,w*manager.aspect_for_component(child)/.8))
                for i,a in enumerate(rectangles):
                    for b in rectangles[i+1:]:
                        dx=min(a[0]+a[2],b[0]+b[2])-max(a[0],b[0])
                        dy=min(a[1]+a[3],b[1]+b[3])-max(a[1],b[1])
                        assert dx<=1e-12 or dy<=1e-12, "child stub footprints overlap"
    print("Mixed-style graph/grid stub regressions passed", flush=True)


def main():
    check_stub_regressions()
    checked = set()
    for alias, scenario in SCENARIOS.items():
        test = server.circuit_backend.create_test_by_name(scenario)
        test.setup_circuit()
        root = test.get_root()
        manager = server.LayoutManager(server.LAYOUT_PATH)
        layout_key = server.scenario_layout_key(alias, root)
        kind = root.get_type_name()
        if kind in COUNTS:
            assert len(root.get_children()) == COUNTS[kind], (kind, "flattened hierarchy")
        if kind == "RV32IMemoryRequest":
            groups = {c.get_name(): c for c in root.get_children()}
            # The reassociated response path is now a separate qualification
            # network. Preserve it, the payload and three protocol-state cells.
            assert {"RESPONSE_ELIGIBLE", "RESPONSE_PRESENT", "RESPONSE_RUNNING",
                    "READY_RUNNING", "STATUS", "BUSY", "SENT", "ABANDONED", "REQUEST_TAG",
                    "REQUEST_OP", "REQUEST_ADDR", "REQUEST_WRITE_DATA",
                    "REQUEST_WRITE_MASK"} <= groups.keys()
            assert groups["STATUS"].get_type_name() == "BitSplitter"
            assert groups["STATUS"].get_input_pins()["IN"].get_width() == 3
        if kind == "RV32IDecodeControlUnit":
            groups = {c.get_name(): c for c in root.get_children()}
            assert len(groups["CONTROL_SIGNALS"].get_children()) == 18
            fields = list(walk(groups["CONTROL_SIGNALS"]))
            assert sum(c.get_type_name() == "ORGate" for c in fields) == 423
            assert sum(c.get_type_name() == "NOTGate" for c in fields) == 7
            assert len(groups["RECOGNITION"].get_children()) == 68
        for component in walk(root):
            children = list(component.get_children())
            kind = component.get_type_name()
            automatic = alias in ALU_SCENARIOS
            if not children or (component != root and not automatic and not rv32i_layout.defaults(kind)):
                continue
            # No wire may bypass a new boundary to reach a grandchild.
            owners = {component.get_id(), *(c.get_id() for c in children)}
            for wire in component.get_wires():
                pins = list(wire.get_sink_pins())
                if wire.get_source_pin(): pins.append(wire.get_source_pin())
                for pin in pins:
                    assert pin.get_owner().get_id() in owners, (kind, wire.get_name(), pin.get_id())
            is_root = component.get_id() == root.get_id()
            if automatic:
                assert rv32i_layout.placements(kind, {c.get_name(): manager.aspect_for_component(c)
                    for c in children}, manager.aspect_for_component(component)) is None, (kind,"custom layout override")
                expected = default_placements(manager, component, layout_key if is_root else None)
                for child in children:
                    actual = manager.get_child_layout_for_component(component, child.get_name(), scenario_key=layout_key, is_root=is_root)
                    wanted = expected[child.get_name()]
                    assert "rel_pos" in actual and "rel_width" in actual, (alias, kind, child.get_name(), "missing saved placement")
                    assert all(abs(a-b)<1e-9 for a,b in zip(actual["rel_pos"],wanted["rel_pos"])), (kind,child.get_name(),"not the automatic position")
                    assert abs(actual["rel_width"]-wanted["rel_width"])<1e-9, (kind,child.get_name(),"not the automatic width")
            aspect = manager.aspect_for_component(component, scenario_key=layout_key, is_root=is_root)
            title = manager.visual_ratio_for_component(component, "title_bar_ratio", scenario_key=layout_key, is_root=is_root)/aspect
            rectangles = []
            parent_margin = server._stub_margin_ratio(
                manager.visual_ratio_for_component(component,"boundary_area_ratio",scenario_key=layout_key,is_root=is_root),
                manager.visual_ratio_for_component(component,"pin_size_ratio",scenario_key=layout_key,is_root=is_root))
            stub_ratios = manager.child_stub_ratios(children)
            for child in children:
                placement = manager.get_child_layout_for_component(component, child.get_name(), scenario_key=layout_key, is_root=is_root)
                x, y = placement["rel_pos"]
                w = placement["rel_width"]
                h = w*manager.aspect_for_component(child)/aspect
                assert w > 0 and x >= 0 and y >= title-1e-6 and x+w <= 1.000001 and y+h <= 1.000001, (kind,child.get_name(),placement,h)
                if automatic:
                    stub = w * stub_ratios[child.get_id()]
                    assert x-stub >= parent_margin-1e-9, (kind,child.get_name(),"left stub crosses parent input")
                    assert x+w+stub <= 1-parent_margin+1e-9, (kind,child.get_name(),"right stub crosses parent output")
                    x, w = x-stub, w+2*stub
                rectangles.append((child.get_name(),x,y,w,h))
            for i,a in enumerate(rectangles):
                for b in rectangles[i+1:]:
                    dx = min(a[1]+a[3],b[1]+b[3])-max(a[1],b[1])
                    dy = min(a[2]+a[4],b[2]+b[4])-max(a[2],b[2])
                    assert dx <= 1e-9 or dy <= 1e-9, (kind,a[0],b[0])
            if kind == "RV32IExecutionControlStatusUnit":
                priorities = [r for r in rectangles if r[0].startswith("PRIORITY_")]
                assert len(priorities) == 7
                assert len({r[1] for r in priorities}) == 1
                assert all(a[2] < b[2] for a,b in zip(priorities,priorities[1:]))
            checked.add(server._component_layout_type(component))
        print(f"{alias}: hierarchy, boundary wiring and layout passed", flush=True)
        del test, root, component, children
        gc.collect()
    print(f"{len(checked)} component layouts checked")


if __name__ == "__main__":
    main()
