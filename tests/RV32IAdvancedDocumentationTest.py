"""Check incremental documentation, actual pins, embeds and static placements."""
from pathlib import Path
import json
import re
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]
PROBE=Path(sys.argv[1]) if len(sys.argv)>1 else ROOT/"build/tests/VisualizerSessionProbe"
DOCS=ROOT/"site-docs/rv32i/4.6"
layout=json.loads((ROOT/"visualizer-v2/layout.json").read_text())
nav=(ROOT/"mkdocs.yml").read_text()
index=(DOCS/"index.md").read_text()
for number,slug in enumerate(("forwarding","hazards","prediction","recovery","memory-waits","core"),1):
    text=(DOCS/(slug+".md")).read_text()
    assert text.startswith(f"# § 4.6.{number}.")
    assert f"rv32i/4.6/{slug}.md" in nav and f"({slug}.md)" in index
    assert "| Component | Description | Structural | Behavioral |" in text
    assert "Implementation" in text and "Examples" in text or slug=="core"
    for alias,test in re.findall(r"<!-- simulator-examples: ([\w-]+) (\w+) -->",text):
        result=json.loads(subprocess.check_output([str(PROBE),"topology",test,alias],text=True,timeout=120))
        assert f"scenario={alias}&amp;embed=1" in text
        pin_text=text
        if test=="RV32IExecuteStageTest": pin_text += (ROOT/"site-docs/rv32i/4.5/execute.md").read_text()
        for pin in result["pins"]:
            if pin["componentId"]==result["rootId"]:
                assert f'`{pin["name"]}`' in pin_text,(slug,pin["name"])
        rows=json.loads(subprocess.check_output([str(PROBE),"examples",test,alias],text=True,timeout=120))
        assert len(rows["rows"])==len(result["checkpoints"])
        for child in result["components"]:
            if child["parentIndex"] is None: continue
            parent=result["components"][child["parentIndex"]]
            kind=parent.get("layoutType",parent["type"])
            entry={}
            scopes=[layout["type_layouts"].get(kind,{}),layout["profile_layouts"].get(parent.get("profileFingerprint"),{}).get(kind,{})]
            if parent["id"]==result["rootId"]: scopes.append(layout["root_layouts"].get(result["layoutKey"],{}))
            for scope in scopes: entry.update(scope.get("children",{}).get(child["name"],{}))
            assert "rel_pos" in entry and entry.get("rel_width",0)>0,child["id"]
        print(f"{test}: documented pins, checkpoint rows, and saved layouts checked",flush=True)
core=json.loads(subprocess.check_output([str(PROBE),"topology","RV32IFiveStageCoreTest","rv32i-five-stage-core"],text=True,timeout=120))
text=(DOCS/"core.md").read_text()
for pin in core["pins"]:
    if pin["componentId"]==core["rootId"]: assert f'`{pin["name"]}`' in text,pin["name"]
print("Advanced documentation checks passed")
