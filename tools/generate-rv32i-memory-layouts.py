"""Reflow RV32I core internals and the 44 transaction-memory program roots.

Uses native topology only (no simulation/history or complete visualizer session).
Unrelated memory/ALU layouts and styling are preserved. Previous geometry is
retained so browsers can upgrade unchanged defaults without discarding edits.
"""
import argparse
from copy import deepcopy
import gc
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "visualizer-v2"))
import server
import rv32i_layout


def walk(component):
    for child in component.get_children():
        yield from walk(child)
    yield component


def reflow(entry, style, children=None, previous=None, kind=None, revision=None):
    revision = revision or rv32i_layout.revision(kind or "")
    if entry.get("layout_revision") != revision:
        entry["previous_default"] = {k: deepcopy(v) for k,v in (previous or entry).items()
                                     if k not in {"layout_revision", "previous_default", "color"}}
    entry.update(style)
    if children is not None:
        if revision in {rv32i_layout.BLOCKS_REVISION, rv32i_layout.ALU_AUTOMATIC_REVISION}:
            # Retired flat children remain recoverable in previous_default.
            entry["children"] = {name: entry.get("children", {}).get(name, {}) for name in children}
        for name, placement in children.items():
            entry.setdefault("children", {}).setdefault(name, {}).update(placement)
    entry["layout_revision"] = revision


def placements(manager, component, alias=None):
    children = list(component.get_children())
    root = alias is not None
    aspect = manager.aspect_for_component(component, scenario_key=alias, is_root=root)
    def ratio(key):
        return manager.visual_ratio_for_component(component,key,scenario_key=alias,is_root=root)
    return server._layered_graph_layout(component, children, aspect, min(.7,ratio("title_bar_ratio")/aspect),
        {c.get_id(): manager.aspect_for_component(c) for c in children},
        ratio("boundary_area_ratio"), ratio("pin_size_ratio"), manager.child_stub_ratios(children))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--layout", type=Path, default=ROOT / "visualizer-v2/layout.json")
    parser.add_argument("--single-cycle-only", action="store_true",
                        help="Refresh only the single-cycle core and its six stages")
    args = parser.parse_args()
    layout = server.LayoutManager(args.layout)
    old_types = deepcopy(layout.type_layouts)
    def selected(kind):
        return not args.single_cycle_only or kind.startswith("RV32ISingleCycle")
    for types in [layout.type_layouts, *layout.profile_layouts.values()]:
        for kind, entry in types.items():
            style = rv32i_layout.defaults(kind)
            if style and selected(kind):
                old = {**layout.settings, **old_types.get(kind, {}), **entry}
                reflow(entry, style, previous=old, kind=kind)
    seen = set()
    for family, prefix in (("RV32ISingleCycleSystemTest", "rv32i-program"),
                           ("RV32IFiveStageCoreProgramTest", "rv32i-five-stage-program")):
        if args.single_cycle_only and family == "RV32IFiveStageCoreProgramTest":
            continue
        test = server.circuit_backend.create_test_by_name(family + "/program-03")
        test.setup_circuit()
        root = test.get_root()
        for component in walk(root):
            kind = server._component_layout_type(component)
            style = rv32i_layout.defaults(kind)
            if not style or not selected(kind):
                continue
            layout.ensure_component_layout_defaults(component)
            children = placements(layout,component)
            reflow(layout.type_layouts[kind],style,children,kind=kind)
            for profile in layout.profile_layouts.values():
                if kind in profile:
                    reflow(profile[kind],style,children,kind=kind)
            seen.add(kind)
        if args.single_cycle_only:
            del test, root, component
            gc.collect()
            continue
        children = placements(layout,root,prefix+"3")
        assert len(children) == 4
        for number in range(1,23):
            alias = prefix+str(number)
            for key in {alias, *(k for k in layout.root_layouts if k.startswith(alias+"@"))}:
                entry = layout.root_layouts.setdefault(key,{})
                reflow(entry,{},children)
        del test, root, component
        gc.collect()
        print(f"{prefix}: 22 roots reflowed",flush=True)
    aliases = {"rv32i-core":"RV32ISingleCycleCore", "rv32i-five-stage-core":"RV32IFiveStageCore",
               "rv32i-control-flow":"RV32IControlFlowUnit", "rv32i-decode-control":"RV32IDecodeControlUnit",
               "rv32i-execution-status":"RV32IExecutionControlStatusUnit"}
    for key, entry in layout.root_layouts.items():
        kind = aliases.get(key.split("@")[0])
        if kind and selected(kind):
            old = {**layout.settings, **old_types.get(kind,{}), **entry}
            reflow(entry,rv32i_layout.defaults(kind),layout.type_layouts[kind]["children"],previous=old,kind=kind)
    layout.is_dirty = True
    layout.save_if_dirty()
    print(f"{len(seen)} RV32I types/variants reflowed",flush=True)


if __name__ == "__main__":
    main()
