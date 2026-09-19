"""Add layouts only for the new matrix, DRAM, ROM and NOR-flash scenarios."""
from copy import deepcopy
import argparse
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / "visualizer-v2"))
import server
import memory_device_layout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reflow", action="store_true", help="Explicitly refresh only new-family matrix placements")
    parser.add_argument("--transactions", action="store_true", help="Only transaction/controller/container documentation")
    args = parser.parse_args()
    layout = server.LayoutManager(root / "visualizer-v2/layout.json")
    sections = ("3.2", "3.3", "3.1.6") if args.transactions else ("3.1.0", "3.1.3", "3.1.4", "3.1.5")
    source = "\n".join((root / "site-docs/memory" / f"{section}.md").read_text() for section in sections)
    for scenario, _ in re.findall(r"<!-- simulator-examples: (\S+) (\S+) -->", source):
        overrides = ({"MEMORY_ENDPOINT_ROOT": "behavioral" if scenario == "scalable-memory-container" else "structural"}
                     if args.transactions else {"MEMORY_ROOT": "structural"})
        session = server.CircuitSession(scenario, layout, profile_overrides=overrides)
        transaction_kind = lambda kind: kind.startswith(("MemoryEndpoint<", "MemoryController<", "MemoryTransactionPort<", "MemoryPayloadRegister<", "MemoryNativeSequencer<", "MemoryContainer<"))
        if args.transactions and args.reflow:
            # Only the newly introduced endpoint families belong to this reflow.
            # Native FF/SRAM/DRAM/ROM/flash layouts may contain user edits.
            for component in session.component_handles.values():
                kind = component.get_type_name()
                if transaction_kind(kind):
                    layout.type_layouts.setdefault(kind, {}).update(memory_device_layout.defaults(kind))
                    profile = layout.profile_layouts.setdefault(server._component_profile_fingerprint(component), {})
                    profile.setdefault(kind, {}).update(memory_device_layout.defaults(kind))
            root_defaults = memory_device_layout.defaults(session.root.get_type_name())
            layout.root_layouts.setdefault(scenario, {}).update(root_defaults)
            layout.root_layouts.setdefault(session.layout_key, {}).update(root_defaults)
        for component in session.component_handles.values():
            kind = component.get_type_name()
            if args.transactions and not transaction_kind(kind):
                continue
            if not memory_device_layout.defaults(kind) or not component.get_children():
                continue
            if args.reflow and kind.startswith(("MatrixColumnMux<", "MatrixWriteDistributor<")):
                # The revised bit-lane floorplan needs a geometry-derived height.
                # Explicit reflow updates only these new peripheral proportions;
                # unrelated families, colors and saved user layouts are untouched.
                layout.type_layouts.setdefault(kind, {}).update(memory_device_layout.defaults(kind))
            children = session._default_child_layouts(component, list(component.get_children()),
                                                      session.component_depths[component.get_id()])
            generic = layout.type_layouts.setdefault(kind, {}).setdefault("children", {})
            for name, placement in children.items():
                if args.reflow:
                    generic.setdefault(name, {}).update(deepcopy(placement))
                else:
                    generic.setdefault(name, deepcopy(placement))
            if args.reflow:
                if session.component_depths[component.get_id()] == 0:
                    entry = layout.root_layouts.setdefault(session.layout_key, {})
                else:
                    entry = layout.profile_layouts.setdefault(server._component_profile_fingerprint(component), {}).setdefault(kind, {})
                for name, placement in children.items():
                    entry.setdefault("children", {}).setdefault(name, {}).update(deepcopy(placement))
        entry = layout.get_root_layout_for_component(session.layout_key, session.root)
        fallback = layout.root_layouts.setdefault(scenario, {})
        for key, value in entry.items():
            if args.reflow and memory_device_layout.defaults(session.root.get_type_name()) and key == "children":
                for name, placement in value.items():
                    fallback.setdefault("children", {}).setdefault(name, {}).update(deepcopy(placement))
            else:
                fallback.setdefault(key, deepcopy(value))
        layout.is_dirty = True
        layout.save_if_dirty()
        print(f"{scenario}: structural matrix layout ready", flush=True)
        del session


if __name__ == "__main__":
    main()
