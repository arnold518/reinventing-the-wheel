"""Refresh standalone 4.2 defaults and newly grouped structural descendants.

Run after generate-rv32i-memory-layouts.py to refit CPU ancestors too.
Topology construction only; one scenario at a time, without waveform history.
Apply automatic layout to every 4.2.3 component and structural descendant.
Keep unrelated register placements and preserve previous defaults for migration.
"""
import argparse
import gc
from copy import deepcopy
from pathlib import Path
import runpy
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "visualizer-v2"))
import server
import rv32i_layout

helpers = runpy.run_path(str(ROOT / "tools/generate-rv32i-memory-layouts.py"))
walk, placements, reflow = (helpers[key] for key in ("walk", "placements", "reflow"))
ALU_SCENARIOS = {
    "addsub32": "AddSub32Test",
    "logic32": "Logic32Test", "shifter32": "Shifter32Test",
    "alu32-control": "ALU32ControlDecoderTest", "alu32-compare": "ALU32CompareUnitTest",
    "alu32-result-selector": "ALU32ResultSelectorTest", "alu32-flags": "ALU32FlagUnitTest",
    "alu32": "ALU32Test",
}
SCENARIOS = {
    "register32": "Register32Test",
    "rv32i-register-file": "RegisterFile32x32Test", **ALU_SCENARIOS,
    "rv32i-decode-control": "RV32IDecodeControlUnitTest",
    "rv32i-pattern-matcher": "RV32IBitPatternMatcherTest",
    "rv32i-control-flow": "RV32IControlFlowUnitTest",
    "rv32i-memory-lanes": "RV32IMemoryLanesTest",
    "rv32i-memory-alignment": "RV32IMemoryAlignmentUnitTest",
    "rv32i-memory-request": "RV32IMemoryRequestTest",
    "rv32i-execution-status": "RV32IExecutionControlStatusUnitTest",
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--alu32-only", action="store_true", help="Refresh all eight 4.2.3 scenarios and their descendants")
    args = parser.parse_args()
    manager = server.LayoutManager(ROOT / "visualizer-v2/layout.json")
    for alias, scenario in (ALU_SCENARIOS if args.alu32_only else SCENARIOS).items():
        automatic = alias in ALU_SCENARIOS
        revision = rv32i_layout.ALU_AUTOMATIC_REVISION if automatic else None
        test = server.circuit_backend.create_test_by_name(scenario)
        test.setup_circuit()
        root = test.get_root()
        old_root = deepcopy(manager.get_root_layout_for_component(alias, root))
        for component in walk(root):
            kind = server._component_layout_type(component)
            manager.ensure_component_layout_defaults(component)
            entry = manager.type_layouts[kind]
            style = rv32i_layout.defaults(kind)
            previous = {**manager.settings, **entry}
            if style:
                reflow(entry, style, kind=kind, previous=previous, revision=revision)
            children = list(component.get_children())
            if not children:
                continue
            generated = placements(manager, component)
            if style or automatic:
                reflow(entry, style, generated, kind=kind, previous=previous, revision=revision)
                for profile in manager.profile_layouts.values():
                    if kind in profile:
                        reflow(profile[kind], style, generated, kind=kind, revision=revision)
            else:
                for name, geometry in generated.items():
                    entry.setdefault("children", {}).setdefault(name, geometry)
        kind = server._component_layout_type(root)
        style = rv32i_layout.defaults(kind)
        if style or automatic:
            for key in {alias, server.scenario_layout_key(alias, root),
                        *(k for k in manager.root_layouts if k.startswith(alias + "@"))}:
                entry = manager.root_layouts.setdefault(key, {})
                previous = {**manager.settings, **old_root, **entry}
                reflow(entry, style, manager.type_layouts[kind]["children"], previous=previous, kind=kind, revision=revision)
        manager.is_dirty = True
        manager.save_if_dirty()
        print(f"{alias}: {len(root.get_children())} root children; defaults ready", flush=True)
        del test, root, component, children
        gc.collect()


if __name__ == "__main__":
    main()
