"""Refresh saved byte-lane write-distributor layouts from current topologies."""

from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "visualizer-v2"))
import recalculate_layouts as refresh
import server


def main():
    manager = server.LayoutManager(server.LAYOUT_PATH)
    changed = 0
    for kind, entry in list(manager.type_layouts.items()):
        match = re.fullmatch(r"MatrixWriteDistributor<(8|16|32),(\d+)>", kind)
        if not match:
            continue
        width, slots = map(int, match.groups())
        component = server.circuit_backend.create_memory_device_component(
            "matrix", f"write-distributor.{width}", "WRITE_DISTRIBUTOR",
            {"words_per_row": str(slots)})
        children = list(component.get_children())
        scopes = [("explicit", entry, None)]
        scopes.extend((fingerprint, entries[kind], None) for fingerprint, entries in manager.profile_layouts.items()
                      if kind in entries)
        if kind == "MatrixWriteDistributor<8,2>":
            scopes.extend((key.split("@", 1)[1] if "@" in key else "explicit", saved, key)
                          for key, saved in manager.root_layouts.items()
                          if key.split("@", 1)[0] == "matrix-write-distributor")
        for fingerprint, saved, root_key in scopes:
            if saved.get("layout_mode") == "manual":
                continue
            view = refresh.ProfileLayoutView(manager, fingerprint)
            placement = server._custom_child_layout(*refresh.layout_arguments(view, component, children, root_key))
            if placement is None:
                raise RuntimeError(f"No byte-lane floorplan for {kind}")
            refresh._assert_child_bounds(view, component, children, placement, root_key)
            refresh._assert_no_child_overlap(view, component, children, placement, 0 if root_key else 1, root_key or kind)
            refresh.refresh_entry(saved, placement)
            saved["children"] = {name: saved["children"][name] for name in placement}
            saved["layout_mode"] = "floorplan"
            changed += 1
    if changed:
        manager.save()
    print(f"Refreshed {changed} matrix write-distributor layout scopes")


if __name__ == "__main__":
    main()
