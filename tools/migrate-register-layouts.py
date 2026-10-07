"""Move saved register geometry to the unified Register<W> component types."""
from copy import deepcopy
import os
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "visualizer-v2"))
sys.path.insert(0, str(Path(os.environ.get("CIRCUITSIM_BACKEND_DIR", ROOT / "visualizer-v2")).resolve()))
import server

manager = server.LayoutManager(ROOT / "visualizer-v2/layout.json")
scopes = [manager.type_layouts, *manager.profile_layouts.values()]
for scope in scopes:
    for width in (1, 2, 3, 4, 5, 8, 16, 32):
        old = scope.pop(f"MemoryPayloadRegister<{width}>", None)
        if old is not None:
            scope[f"Register<{width}>"] = old
    scope.pop("Register32", None)
    if "RegisterFile32x32" in scope:
        scope["RegisterFile32x32"].get("children", {}).pop("FULL_WRITE_MASK", None)

# The former Register32 root was a wrapper. Its child geometry cannot be
# reused; the former 32-bit payload geometry is exactly the new direct cell
# hierarchy. Preserve that explicit cell ordering in all root scopes.
word = deepcopy(manager.type_layouts["Register<32>"])
word["layout_mode"] = "manual"
for key in list(manager.root_layouts):
    if key == "register32" or key.startswith("register32@"):
        manager.root_layouts[key] = deepcopy(word)
    if key.split("@", 1)[0] in {"rv32i-register-file", "register-file32x32", "register-file-32x32"}:
        manager.root_layouts[key].get("children", {}).pop("FULL_WRITE_MASK", None)
root = server.circuit_backend.create_layout_topology("RegisterTest")
manager.root_layouts[server.scenario_layout_key("register32", root)] = deepcopy(word)
manager.save()
print("Register<W> type/profile layouts and 32-bit example root migrated")
