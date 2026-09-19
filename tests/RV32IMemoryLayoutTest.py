"""Geometry and organization of RV32I cores, stages, control and memory lanes."""
from pathlib import Path
import gc
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "visualizer-v2"))
import circuit_backend as backend
import server
import rv32i_layout


def walk(component):
    yield component
    for child in component.get_children():
        yield from walk(child)


checked = set()
for test_name, alias in (("RV32ISingleCycleSystemTest/program-03", "rv32i-program3"),
                         ("RV32IFiveStageCoreProgramTest/program-03", "rv32i-five-stage-program3"),
                         ("RV32ISingleCycleCoreTest", "rv32i-core"),
                         ("RV32IFiveStageCoreTest", "rv32i-five-stage-core"),
                         ("RV32IDecodeControlUnitTest", "rv32i-decode-control"),
                         ("RV32IControlFlowUnitTest", "rv32i-control-flow"),
                         ("RV32IExecutionControlStatusUnitTest", "rv32i-execution-status")):
    test = backend.create_test_by_name(test_name)
    test.setup_circuit()
    root = test.get_root()
    manager = server.LayoutManager(ROOT / "visualizer-v2/layout.json")
    if "/program-" in test_name:
        assert len(root.get_children()) == 4
    for component in walk(root):
        kind = component.get_type_name()
        if not kind.startswith("RV32I"):
            continue
        is_root = component.get_id() == root.get_id()
        aspect = manager.aspect_for_component(component, scenario_key=alias, is_root=is_root)
        title = manager.visual_ratio_for_component(component, "title_bar_ratio", scenario_key=alias, is_root=is_root)/aspect
        children = list(component.get_children())
        if kind in rv32i_layout.ASPECTS and kind not in rv32i_layout.AUTOMATIC_TYPES | {"RV32IFiveStageCore", "RV32ISingleCycleCore", "RV32IMemoryRequest"}:
            generated = rv32i_layout.placements(kind, {c.get_name(): manager.aspect_for_component(c) for c in children}, aspect)
            assert generated is not None, (kind, "unexpected graph-layout fallback")
            assert set(generated) == {c.get_name() for c in children}
        if kind in rv32i_layout.AUTOMATIC_TYPES:
            aspects = {c.get_id(): manager.aspect_for_component(c) for c in children}
            assert rv32i_layout.placements(kind, {c.get_name(): aspects[c.get_id()] for c in children}, aspect) is None
            def ratio(key):
                return manager.visual_ratio_for_component(component, key, scenario_key=alias, is_root=is_root)
            generated = server._layered_graph_layout(component, children, aspect, min(.7, title),
                aspects, ratio("boundary_area_ratio"), ratio("pin_size_ratio"), manager.child_stub_ratios(children))
            for name, placement in generated.items():
                saved = manager.get_child_layout_for_component(component, name, scenario_key=alias, is_root=is_root)
                assert saved["rel_pos"] == placement["rel_pos"], (kind, name, "not automatic placement")
                assert saved["rel_width"] == placement["rel_width"], (kind, name, "not automatic width")
        if kind in rv32i_layout.ASPECTS:
            assert abs(aspect-rv32i_layout.ASPECTS[kind]) < .001, (kind, "stretched by old pin/minimum settings", aspect)
        rectangles = []
        for child in component.get_children():
            placement = manager.get_child_layout_for_component(component, child.get_name(),
                                                               scenario_key=alias, is_root=is_root)
            x, y = placement["rel_pos"]
            width = placement["rel_width"]
            height = width * manager.aspect_for_component(child) / aspect
            assert min(x, y) >= 0 and max(x + width, y + height) <= 1.00001, (kind, child.get_name(), x, y, width, height)
            assert y >= title-1e-6, (kind, child.get_name(), "overlaps title")
            rectangles.append((child.get_name(), x, y, width, height))
        for i, a in enumerate(rectangles):
            for b in rectangles[i + 1:]:
                overlap_x = min(a[1] + a[3], b[1] + b[3]) - max(a[1], b[1])
                overlap_y = min(a[2] + a[4], b[2] + b[4]) - max(a[2], b[2])
                assert overlap_x <= 1e-9 or overlap_y <= 1e-9, (kind, a[0], b[0])
        positions = {name:(x,y,w,h) for name,x,y,w,h in rectangles}
        if kind == "RV32ISingleCycleCore":
            order = server.SINGLE_CYCLE_STAGE_ORDER
            assert all(positions[a][0] + positions[a][2] < positions[b][0] for a,b in zip(order,order[1:]))
            centers = [positions[n][1] + positions[n][3]/2 for n in order]
            assert max(centers) - min(centers) < 1e-9
            assert min(positions[n][2] for n in order) >= .14
            assert max(positions[n][1] + positions[n][3] for n in order) < min(positions[n][1] for n in ("NEXT_PC", "EXECUTION_STATUS"))
        if kind == "RV32IFiveStageCore":
            order = server.FIVE_STAGE_PIPELINE_CHILD_ORDER
            assert all(positions[a][0] < positions[b][0] for a,b in zip(order,order[1:]))
            assert min(positions[n][2] for n in ("FETCH","DECODE","EXECUTE","MEMORY","WRITEBACK")) >= .10
        if kind.endswith("PipelineRegister"):
            assert positions["PC_STATE"][1] == positions["INSTRUCTION_STATE"][1]
            assert positions["PC_STATE"][0] < positions["INSTRUCTION_STATE"][0]
            assert positions["VALID_STATE"][1] < positions["PC_STATE"][1]
        if kind == "RV32IDecodeControlUnit":
            assert min(p[2] for p in positions.values()) > .01
        checked.add(server._component_layout_type(component))
    del test, root, manager
    gc.collect()
    print(alias + ": all RV32I layout bounds and organization passed", flush=True)
print(f"{len(checked)} RV32I types/variants checked")
