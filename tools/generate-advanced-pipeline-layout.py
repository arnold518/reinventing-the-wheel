"""Refresh advanced pipeline groups; keep unrelated and manual layouts intact.

Topology-only, one hierarchy at a time. Publish profile and type fallbacks so
static WASM pages need no layout service, including predictor-policy changes.
"""
import argparse
from pathlib import Path
import copy
import gc
import sys

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/"visualizer-v2"))
import server
import recalculate_layouts as refresh

manager=server.LayoutManager(server.LAYOUT_PATH)
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument("--program-roots-only",action="store_true",
                    help="Publish existing reviewed policy roots for every fixed program")
args=parser.parse_args()
targets={"RV32IFiveStageCore","RV32IExecuteStage","RV32IForwardedOperands","RV32IExecutePacketStatus",
         "RV32IForwardingUnit","RV32IHazardDetectionUnit","RV32IPipelineCoordinator",
         "RV32IPipelineRetirementUnit",
         "RV32IPredictionCounter","RV32IBranchPredecode","RV32IBranchPredictor","RV32IBranchRecovery",
         "RV32IPredictionPath","RV32IPredictionEntry"}
tests=["RV32I"+name+"Test" for name in ("PredictionCounter","BranchPredecode","BranchPredictor","BranchRecovery","PredictionPath",
    "ForwardingUnit","HazardDetectionUnit","ExecuteStage","PipelineCoordinator","FiveStageCore")]
tests += ["RV32IPredictionProgramTest/"+policy+"/program-20" for policy in ("not-taken","taken","backward-taken","one-bit","two-bit")]

for test in ([] if args.program_roots_only else tests):
    root=server.circuit_backend.create_layout_topology(test)
    key=server.scenario_layout_key(server.scenario_canonical_alias(test),root)
    nodes=list(refresh._walk(root))
    for component,depth in nodes:
        manager.ensure_component_layout_defaults(component,scenario_key=key,is_root=depth==0)
    changed=0
    for component,depth in reversed(nodes):
        children=list(component.get_children())
        if not children: continue
        kind=component.get_type_name(); type_key=server._component_layout_type(component)
        effective=manager.get_layout_for_component(component)
        if depth==0: effective={**effective,**manager.root_layouts.get(key,{})}
        if effective.get("layout_mode")=="manual": continue
        existing={c.get_name():manager.get_child_layout_for_component(component,c.get_name(),scenario_key=key,is_root=depth==0) for c in children}
        force=kind in targets or kind.startswith("RV32IPredictionTable<")
        if not force and all("rel_pos" in v and "rel_width" in v for v in existing.values()): continue
        arguments=refresh.layout_arguments(manager,component,children,key if depth==0 else None)
        generated=server._custom_child_layout(*arguments)
        if generated is None: generated=server._layered_graph_layout(*arguments)
        refresh._assert_child_bounds(manager,component,children,generated,key if depth==0 else None)
        refresh._assert_no_child_overlap(manager,component,children,generated,depth,key)
        manager.set_child_layouts(component,generated,scenario_key=key,is_root=depth==0)
        # Refresh old topology/profile variants too, except explicit handmade ones.
        if force:
            entries=[manager.type_layouts.setdefault(type_key,{})]
            entries += [types[type_key] for types in manager.profile_layouts.values() if type_key in types]
            if depth==0: entries += [entry for k,entry in manager.root_layouts.items() if k.split("@")[0]==key.split("@")[0]]
            for entry in entries:
                if entry.get("layout_mode")=="manual": continue
                if kind=="RV32IBranchPredictor":
                    # Static and dynamic policies share the family layout type.
                    # Their mutually exclusive helper names must both survive.
                    entry.setdefault("children",{}).update(copy.deepcopy(generated))
                else: entry["children"]=copy.deepcopy(generated)
                entry["layout_revision"]="advanced-pipeline-v1"
        changed+=1
    print(f"{test}: {changed} scopes verified",flush=True)
    del root,nodes,component,children
    gc.collect()

# Program images do not change topology. Share the reviewed root geometry, but
# retain each policy's core hierarchy and all manually positioned program roots.
for name in server.circuit_backend.get_registered_test_names():
    if not name.startswith("RV32IPredictionProgramTest/"): continue
    policy=name.split("/")[1]
    source=server.scenario_canonical_alias(f"RV32IPredictionProgramTest/{policy}/program-20")
    target=server.scenario_canonical_alias(name)
    for key,entry in list(manager.root_layouts.items()):
        if key.split("@")[0]!=source: continue
        destination=target+key[len(source):]
        if manager.root_layouts.get(destination,{}).get("layout_mode")!="manual": manager.root_layouts[destination]=copy.deepcopy(entry)
manager.save()
