"""Fill missing pipeline layout scopes for static/WASM previews, topology only.

The Python server normally fills these lazily. GitHub Pages has no server, so
all child placements must be included in layout.json before publication.
Existing placements (including manual geometry) are never moved by this tool.
"""
import argparse
from pathlib import Path
import copy
import gc
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "visualizer-v2"))
import server
import recalculate_layouts as refresh
import memory_device_layout
import register_file_layout
import rv32i_layout

manager = server.LayoutManager(server.LAYOUT_PATH)
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--test", action="append", help="Refresh only this registered test topology (repeatable)")
parser.add_argument("--reflow-type", action="append", default=[],
                    help="Recalculate this changed component type in the selected topologies, except manual layouts")
args = parser.parse_args()
tests = [f"RV32IBaseline{block}Test" for block in (
    "FetchStage", "DecodeStage", "ExecuteStage", "MemoryStage",
    "DependencyUnit", "Control", "PipelineCore")]
tests += ["RV32IWritebackStageTest", "RV32IPipelineExecutionStatusTest", "RV32ISingleCycleSystemTest/program-01",
          "RV32IFiveStageCoreTest", "RV32IBaselinePipelineProgramTest/program-01",
          "RV32IFiveStageCoreProgramTest/program-01", "RegisterTest", "RegisterFile32x32Test",
          "RV32IMemoryRequestTest", "MemoryControllerTest", "FFAlwaysReadyControllerTest",
          "RV32IDecodeControlUnitTest", "RV32IExecutionControlStatusUnitTest", "ALU32CoreTest"]

cases = [(test, {}) for test in tests]
for architecture in ("RV32ISingleCycleSystemTest", "RV32IFiveStageCoreProgramTest", "RV32IBaselinePipelineProgramTest"):
    cases.append((architecture + "/program-01", {"RV32I_PROGRAM_ROOT.CORE": "behavioral",
        "RV32I_PROGRAM_ROOT.INSTRUCTION_MEMORY": "structural", "RV32I_PROGRAM_ROOT.DATA_MEMORY": "structural"}))
if args.test:
    registered = set(server.circuit_backend.get_registered_test_names())
    unknown = set(args.test) - registered
    if unknown:
        parser.error("Unknown prepared topology: " + ", ".join(sorted(unknown)))
    cases = [(test, overrides) for test, overrides in cases if test in args.test]
    cases.extend((test, {}) for test in args.test if test not in {name for name, _ in cases})

for test, overrides in cases:
    root = server.circuit_backend.create_layout_topology(test, overrides)
    key = server.scenario_layout_key(server.scenario_canonical_alias(test), root)
    components = list(refresh._walk(root))
    for component, depth in components:
        type_key = server._component_layout_type(component)
        if (component.get_type_name() in args.reflow_type or
                component.get_type_name().split("<")[0] in args.reflow_type or
                type_key in args.reflow_type):
            entry = manager.type_layouts.setdefault(type_key, {})
            if entry.get("layout_mode") != "manual":
                entry.update({**memory_device_layout.defaults(type_key),
                              **rv32i_layout.defaults(component.get_type_name()),
                              **register_file_layout.defaults(type_key)})
        manager.ensure_component_layout_defaults(component, scenario_key=key, is_root=depth == 0)
    filled = 0
    for component, depth in reversed(components):
        children = list(component.get_children())
        if not children:
            continue
        existing = {child.get_name(): manager.get_child_layout_for_component(
            component, child.get_name(), scenario_key=key, is_root=depth == 0) for child in children}
        missing = {name for name, entry in existing.items()
                   if "rel_pos" not in entry or "rel_width" not in entry}
        # Pages can create a new profile hash after a fidelity selection.
        # Publish a type fallback for a newly introduced topology, rather
        # than requiring every override combination to carry its own copy.
        # Existing type/manual floorplans remain untouched.
        endpoint_child = overrides and any(part in component.get_id()
            for part in (".INSTRUCTION_MEMORY", ".DATA_MEMORY"))
        type_key = server._component_layout_type(component)
        type_entry = manager.type_layouts.get(type_key, {})
        fallback = (not type_entry.get("children") and type_entry.get("layout_mode") != "manual"
                    and (endpoint_child or bool(children)))
        reflow = (component.get_type_name() in args.reflow_type or
                  component.get_type_name().split("<")[0] in args.reflow_type or
                  type_key in args.reflow_type)
        if reflow and type_entry.get("layout_mode") == "manual":
            print(f"Preserved manual layout: {type_key}", flush=True)
            continue
        if not missing and not reflow:
            if fallback:
                refresh._assert_child_bounds(manager, component, children, existing)
                refresh._assert_no_child_overlap(manager, component, children, existing, depth, key)
                manager.type_layouts.setdefault(type_key, {})["children"] = copy.deepcopy(existing)
            continue
        arguments = refresh.layout_arguments(manager, component, children, key if depth == 0 else None)
        generated = server._layered_graph_layout(*arguments)
        # Refit requesters as a whole; mixing old and new control rails would
        # overlap their children after the compact fetch specialization.
        requester = component.get_type_name() in {"RV32IMemoryRequest", "RV32IFetchRequest"}
        entry = manager.get_layout_for_component(component)
        if requester and entry.get("layout_mode") == "manual":
            raise RuntimeError("Manual requester layout needs explicit review")
        merged = generated if requester or reflow else {name: generated[name] if name in missing else existing[name] for name in existing}
        refresh._assert_child_bounds(manager, component, children, merged, key if depth == 0 else None)
        refresh._assert_no_child_overlap(manager, component, children, merged, depth, key)
        manager.set_child_layouts(component, generated if requester or reflow else {name: generated[name] for name in missing},
                                  scenario_key=key, is_root=depth == 0)
        if reflow:
            destination = (manager.root_layouts.setdefault(key, {}) if depth == 0 else
                           manager.profile_layouts.setdefault(server._component_profile_fingerprint(component), {}).setdefault(type_key, {})
                           if server._component_profile_fingerprint(component) != "explicit" else
                           manager.type_layouts.setdefault(type_key, {}))
            destination["children"] = copy.deepcopy(generated)
            manager.is_dirty = True
        if fallback:
            manager.type_layouts.setdefault(type_key, {})["children"] = copy.deepcopy(merged)
        filled += len(children) if reflow else len(missing)
    print(f"{test}: {filled} missing placements filled; existing geometry preserved", flush=True)
    del root, components, component, children
    gc.collect()

# All programs of an architecture use the same RV32IProgramRoot topology (the
# loader changes the image, not the circuit). Materialize each registered alias
# without constructing/simulating the same large hierarchy 22 times. Never
# replace an existing manual or scenario-specific placement.
for test in (() if args.test else server.circuit_backend.get_registered_test_names()):
    if "/program-" not in test: continue
    architecture = test.split("/program-")[0]
    if architecture not in ("RV32ISingleCycleSystemTest", "RV32IFiveStageCoreProgramTest", "RV32IBaselinePipelineProgramTest"): continue
    source = server.scenario_canonical_alias(architecture + "/program-01")
    target = server.scenario_canonical_alias(test)
    for key, entry in list(manager.root_layouts.items()):
        if key.split("@")[0] != source: continue
        suffix = key[len(source):]
        destination = manager.root_layouts.setdefault(target + suffix, {})
        for field, value in entry.items():
            if field != "children": destination.setdefault(field, copy.deepcopy(value))
        for name, placement in entry.get("children", {}).items():
            destination.setdefault("children", {}).setdefault(name, copy.deepcopy(placement))
manager.save()
