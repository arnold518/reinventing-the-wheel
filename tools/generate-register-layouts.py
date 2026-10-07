"""Refresh the hand floorplans for unified Register<W> words.

Only register-word defaults and obsolete register-file root children are
changed. Existing manual layouts elsewhere remain untouched. Use --check to
validate the saved layouts without writing them.
"""

import argparse
from copy import deepcopy
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
# Honor a caller-provided build directory for circuit_backend before the
# deployed visualizer copy (important for isolated CTest builds).
sys.path.append(str(ROOT / "visualizer-v2"))
import memory_device_layout
import recalculate_layouts
import server

WIDTHS = (1, 2, 3, 4, 5, 8, 16, 32)
FILE_ALIASES = ("rv32i-register-file", "register-file32x32", "register-file-32x32")


def register_placements(manager, width):
    component = getattr(server.circuit_backend, f"RegisterW{width}")(f"LAYOUT_REGISTER_{width}")
    kind = component.get_type_name()
    children = list(component.get_children())
    aspects = {child.get_name(): manager.aspect_for_component(child) for child in children}
    parent_aspect = manager.aspect_for_component(component)
    placements = memory_device_layout.placements(kind, aspects, parent_aspect)
    expected = {f"BIT_{bit}" for bit in range(width)}
    if width > 1:
        expected |= {"INPUT_BITS", "OUTPUT_BITS"}
    if placements is None or set(placements) != expected:
        raise RuntimeError(f"{kind}: floorplan does not match the current children")
    for name, item in placements.items():
        x, y = item["rel_pos"]
        w = item["rel_width"]
        h = w * aspects[name] / parent_aspect
        if w <= 0 or min(x, y) < 0 or x + w > 1 or y + h > 1:
            raise RuntimeError(f"{kind}.{name}: child lies outside the register")
    recalculate_layouts._assert_child_bounds(manager, component, children, placements)
    recalculate_layouts._assert_no_child_overlap(manager, component, children, placements, 1, "")
    return kind, placements


def register_file_children():
    test = server.circuit_backend.create_test_by_name("RegisterFile32x32Test")
    test.setup_circuit()
    return {child.get_name() for child in test.get_root().get_children()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="Verify saved placements without writing")
    args = parser.parse_args()
    manager = server.LayoutManager(ROOT / "visualizer-v2/layout.json")
    changes = []

    for width in WIDTHS:
        kind, placements = register_placements(manager, width)
        scopes = [("type", manager.type_layouts)]
        scopes += [(f"profile/{name}", entries) for name, entries in manager.profile_layouts.items()
                   if kind in entries]
        for label, entries in scopes:
            entry = entries.get(kind)
            if entry is None and label != "type":
                continue
            if entry is not None and entry.get("layout_mode") == "manual":
                # Explicit user geometry is not a generated default.
                continue
            if entry is None:
                entry = {}
                if not args.check:
                    entries[kind] = entry
            if entry.get("children") != placements:
                changes.append(f"{label}/{kind}")
                if not args.check:
                    entry["children"] = deepcopy(placements)

    # The register-file's live children are decoder, storage, two read muxes,
    # and x0. One older fingerprinted root still saves the retired X1..X31.
    live_file_children = register_file_children()
    for key, entry in manager.root_layouts.items():
        if not any(key == alias or key.startswith(alias + "@") for alias in FILE_ALIASES):
            continue
        children = entry.get("children", {})
        stale = set(children) - live_file_children
        if stale:
            changes.append(f"root/{key}: remove {len(stale)} retired children")
            if not args.check:
                entry["children"] = {name: value for name, value in children.items()
                                     if name in live_file_children}

    for key, entry in manager.root_layouts.items():
        if key == "register32" or key.startswith("register32@"):
            expected = {f"BIT_{bit}" for bit in range(32)} | {"INPUT_BITS", "OUTPUT_BITS"}
            if set(entry.get("children", {})) != expected:
                raise RuntimeError(f"root/{key}: obsolete or missing register children")

    if args.check:
        if changes:
            raise SystemExit("Outdated register layouts: " + ", ".join(changes))
        print("Register word and register-file layouts match the current topology")
    elif changes:
        manager.save()
        print("Refreshed " + ", ".join(changes))
    else:
        print("Register layouts already current")


if __name__ == "__main__":
    main()
