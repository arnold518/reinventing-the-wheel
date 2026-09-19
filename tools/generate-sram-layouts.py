"""Generate SRAM defaults, or explicitly reflow only SRAM with --reflow.

The previous shipped geometry is retained for conservative browser upgrades:
unchanged old defaults move to the matrix, while edited placements are retained.
"""
import argparse
from copy import deepcopy
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / "visualizer-v2"))
import server
import sram_layout

REVISION = "sram-matrix-v1"


def reflow_entry(entry, style, children=None):
    if entry.get("layout_revision") != REVISION:
        entry["previous_default"] = {key: deepcopy(value) for key, value in entry.items()
                                     if key not in {"previous_default", "layout_revision", "color"}}
    entry.update(style)
    if children is not None:
        for name, placement in children.items():
            entry.setdefault("children", {}).setdefault(name, {}).update(placement)
    entry["layout_revision"] = REVISION


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reflow", action="store_true")
    parser.add_argument("--layout", type=Path, default=root / "visualizer-v2/layout.json")
    args = parser.parse_args()
    layout = server.LayoutManager(args.layout)
    source = (root / "site-docs/memory/3.1.2.md").read_text()
    if args.reflow:
        # Apply proportions before sessions compute pin-safe minima and layouts.
        for types in [layout.type_layouts, *layout.profile_layouts.values()]:
            for kind, entry in types.items():
                style = sram_layout.defaults(kind)
                if style:
                    reflow_entry(entry, style)
        scenario_types = {"sram": "SRAM<8>", "sram-row": "SRAMRow<2>",
                          "sram-storage-bank": "SRAMStorageBank<8,4,2>",
                          "sram-column-selector": "SRAMColumnSelector<8,2>"}
        for key, entry in layout.root_layouts.items():
            kind = scenario_types.get(key.split("@")[0])
            if kind:
                reflow_entry(entry, sram_layout.defaults(kind))

    for scenario, _test in re.findall(r"<!-- simulator-examples: (\S+) (\S+) -->", source):
        modes = ("structural", "behavioral") if scenario in {"sram-cell", "sram-row", "sram-storage-bank", "sram"} else ("structural",)
        for fidelity in modes:
            session = server.CircuitSession(scenario, layout, profile_overrides={"SRAM_ROOT": fidelity})
            if args.reflow:
                for component in session.component_handles.values():
                    kind = component.get_type_name()
                    if not sram_layout.defaults(kind):
                        continue
                    children = list(component.get_children())
                    if not children:
                        continue
                    depth = session.component_depths[component.get_id()]
                    placements = session._default_child_layouts(component, children, depth)
                    if depth == 0:
                        entry = layout.root_layouts.setdefault(session.layout_key, {})
                    else:
                        profile = layout.profile_layouts.setdefault(server._component_profile_fingerprint(component), {})
                        entry = profile.setdefault(kind, {})
                    reflow_entry(entry, sram_layout.defaults(kind), placements)
                    # Generic types cover arbitrary mixed-fidelity profiles.
                    reflow_entry(layout.type_layouts.setdefault(kind, {}), sram_layout.defaults(kind), placements)
            if fidelity == "structural":
                source_layout = layout.get_root_layout_for_component(session.layout_key, session.root)
                fallback = layout.root_layouts.setdefault(scenario, {})
                if args.reflow and sram_layout.defaults(session.root.get_type_name()):
                    reflow_entry(fallback, sram_layout.defaults(session.root.get_type_name()), source_layout["children"])
                else:
                    for key, value in source_layout.items():
                        fallback.setdefault(key, deepcopy(value))
            layout.is_dirty = True
            layout.save_if_dirty()
            print(f"{scenario}: {fidelity} layout ready", flush=True)


if __name__ == "__main__":
    main()
