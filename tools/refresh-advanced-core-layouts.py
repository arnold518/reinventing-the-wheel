"""Reflow pipeline banks, control and enclosing cores without simulation.

Preserve manual geometry and retain old defaults for browser migration.
The baseline shares the packet banks and must be refitted too.
"""
import argparse
from copy import deepcopy
import gc
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.append(str(ROOT / "visualizer-v2"))
import server
import recalculate_layouts as refresh
import rv32i_layout

KINDS = {"RV32IFiveStageCore", "RV32IBaselinePipelineCore", "RV32IPipelineCoordinator",
         *(f"RV32I{stage}PipelineRegister" for stage in ("IFID", "IDEX", "EXMEM", "MEMWB"))}
REVISION = rv32i_layout.ADVANCED_REVISION
POLICIES = ("not-taken", "taken", "backward-taken", "one-bit", "two-bit")


def generate(path, check=False):
    manager = server.LayoutManager(path)
    original = deepcopy(manager.to_jsonable())
    changed, manual, seen = [], set(), set()
    roots = {}

    def remember(entry, effective):
        if entry.get("layout_revision") != REVISION:
            entry["previous_default"] = {k: deepcopy(v) for k, v in effective.items()
                if k not in {"previous_default", "automatic_layout_history"}}
            entry["layout_revision"] = REVISION

    # Fit parents only after updating child proportions. Record the old
    # effective style so migration preserves actual user edits.
    for fingerprint, entries in [("type", manager.type_layouts), *manager.profile_layouts.items()]:
        for kind, entry in entries.items():
            if kind.split("<")[0] not in KINDS:
                continue
            if entry.get("layout_mode") == "manual" or original["type_layouts"].get(kind, {}).get("layout_mode") == "manual":
                manual.add(f"{fingerprint}/{kind}")
                continue
            old = {**manager.settings, **original["type_layouts"].get(kind, {})}
            if fingerprint != "type":
                old.update(original["profile_layouts"].get(fingerprint, {}).get(kind, {}))
            remember(entry, old)
            entry.update(rv32i_layout.defaults(kind))

    def update(scope, entry, component, fingerprint="explicit", root_key=None):
        if scope in seen:
            return
        view = refresh.ProfileLayoutView(manager, fingerprint)
        effective = (view.get_root_layout_for_component(root_key, component) if root_key
                     else view.get_layout_for_component(component))
        if entry.get("layout_mode") == "manual" or effective.get("layout_mode") == "manual":
            manual.add(scope)
            seen.add(scope)
            return
        before = deepcopy(entry)
        remember(entry, effective)
        if component.get_type_name() in KINDS:
            entry.update(rv32i_layout.defaults(component.get_type_name()))
        children = list(component.get_children())
        arguments = refresh.layout_arguments(view, component, children, root_key)
        custom = server._custom_child_layout(*arguments)
        generated = custom if custom is not None else server._layered_graph_layout(*arguments)
        refresh._assert_child_bounds(view, component, children, generated, root_key)
        refresh._assert_no_child_overlap(view, component, children, generated, 0 if root_key else 1, root_key or "")
        refresh.refresh_entry(entry, generated)
        entry["children"] = {name: entry["children"][name] for name in generated}
        entry["layout_mode"] = "floorplan" if custom is not None else "automatic"
        if entry != before:
            changed.append(scope)
        seen.add(scope)

    tests = [f"RV32I{stage}PipelineRegisterTest" for stage in ("IFID", "IDEX", "EXMEM", "MEMWB")]
    tests += ["RV32IPipelineCoordinatorTest", "RV32IFiveStageCoreTest", "RV32IBaselinePipelineCoreTest",
              "RV32IFiveStageCoreProgramTest/program-01", "RV32IBaselinePipelineProgramTest/program-01"]
    tests += [f"RV32IPredictionProgramTest/{policy}/program-20" for policy in POLICIES]
    for test in tests:
        root = server.circuit_backend.create_layout_topology(test)
        alias = server.scenario_canonical_alias(test)
        key = server.scenario_layout_key(alias, root)
        nodes = list(refresh._walk(root))
        for component, depth in nodes:
            manager.ensure_component_layout_defaults(component, scenario_key=key, is_root=depth == 0)
        for component, depth in reversed(nodes):
            if component.get_type_name() not in KINDS or not component.get_children():
                continue
            kind = server._component_layout_type(component)
            stores = [("type", "explicit", manager.type_layouts)]
            stores += [(f"profile/{fp}", fp, entries)
                       for fp, entries in manager.profile_layouts.items()]
            actual = server._component_profile_fingerprint(component)
            if actual != "explicit" and actual not in manager.profile_layouts:
                stores.append((f"profile/{actual}", actual, manager.profile_layouts.setdefault(actual, {})))
            for scope, fingerprint, entries in stores:
                if fingerprint not in {"explicit", actual} and kind not in entries:
                    continue
                update(f"{scope}/{kind}", entries.setdefault(kind, {}), component, fingerprint)
        keys = {key, alias, *(k for k in manager.root_layouts if k.split("@")[0] == alias)}
        for root_key in sorted(keys):
            fingerprint = root_key.partition("@")[2] or server._component_profile_fingerprint(root)
            update(f"root/{root_key}", manager.root_layouts.setdefault(root_key, {}), root, fingerprint, root_key)
        roots[alias] = deepcopy(manager.root_layouts[key])
        print(f"{test}: core/control/register floorplans checked", flush=True)
        del root, nodes, component
        gc.collect()

    # Different programs share root topology; predictor policies keep their
    # own templates. Preserve every explicitly handmade root.
    for test in server.circuit_backend.get_registered_test_names():
        if test.startswith("RV32IPredictionProgramTest/"):
            source_test = f"RV32IPredictionProgramTest/{test.split('/')[1]}/program-20"
        elif test.startswith(("RV32IFiveStageCoreProgramTest/", "RV32IBaselinePipelineProgramTest/")):
            source_test = test.split("/program-")[0] + "/program-01"
        else:
            continue
        source = server.scenario_canonical_alias(source_test)
        target = server.scenario_canonical_alias(test)
        template = roots[source]
        for key in {target, *(k for k in manager.root_layouts if k.split("@")[0] == target)}:
            entry = manager.root_layouts.setdefault(key, {})
            if entry.get("layout_mode") == "manual":
                manual.add(f"root/{key}")
                continue
            remember(entry, entry)
            refresh.refresh_entry(entry, template["children"])
            entry["children"] = {n: entry["children"][n] for n in template["children"]}
            entry["layout_mode"] = template.get("layout_mode", "floorplan")

    different = manager.to_jsonable() != original
    if check and different:
        raise RuntimeError("Pipeline layouts need regeneration")
    if different:
        manager.save()
    print(f"Reflowed {len(changed)} scopes; preserved {len(manual)} manual scopes", flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--layout", type=Path, default=ROOT / "visualizer-v2/layout.json")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    generate(args.layout, args.check)
