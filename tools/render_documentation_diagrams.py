"""Render the hand-designed documentation figures, not a graph auto-layout.

Edit the semantic compositions below, then run this file. --check verifies that
the committed, self-contained HTML assets match their source. MkDocs inlines
only the marked figure, so rendering needs neither Mermaid nor an iframe.
"""

import argparse
import json
from html import escape
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DESTINATION = ROOT / "site-docs/assets/diagrams"
# Timing numbers measured by documentation_timing_data; charts read them, so a
# figure cannot drift from the code (DocumentationDiagramsTest re-renders).
TIMING = ROOT / "tools/timing/documentation-timing.generated.json"


def timing_profiles():
    return {p["name"]: p for p in json.loads(TIMING.read_text(encoding="utf-8"))["profiles"]}


def grid(value):
    """Round a coordinate to the 4 px grid."""
    return int(round(value / 4.0)) * 4

STYLE = """
.circuit-figure {
  --dg-paper: var(--md-default-bg-color, #fff);
  --dg-ink: var(--md-default-fg-color, #202735);
  --dg-muted: #566174;
  --dg-rule: #c8ced8;
  --dg-surface: #f5f6f9;
  --dg-accent: #4051b5;
  --dg-tint: #eef0fc;
  margin: 1.5rem 0; max-width: 100%; text-align: left;
  font-family: var(--md-text-font-family, 'Noto Sans', sans-serif);
}
[data-md-color-scheme="slate"] .circuit-figure {
  --dg-muted: #b4bdd0; --dg-rule: #4b5569; --dg-surface: #262c38;
  --dg-accent: #acb7ff; --dg-tint: #303853;
}
@media (prefers-color-scheme: dark) {
  .diagram-standalone .circuit-figure {
    --dg-paper: #1e2129; --dg-ink: #e4e7ef; --dg-muted: #b4bdd0;
    --dg-rule: #4b5569; --dg-surface: #262c38;
    --dg-accent: #acb7ff; --dg-tint: #303853;
  }
  .diagram-standalone body { background: #1e2129; color: #e4e7ef; }
}
.circuit-figure .diagram-viewport {
  max-width: 100%; overflow: auto; overscroll-behavior-x: contain;
}
.circuit-figure .diagram-viewport:focus-visible {
  outline: 2px solid var(--dg-accent); outline-offset: 3px;
}
.circuit-figure svg { display: block; width: 640px; min-width: 640px; max-width: none; margin: 0 auto; }
.circuit-figure figcaption {
  display: block; margin: .6rem 0 0; max-width: none; text-align: left;
  color: var(--dg-muted); font-style: normal; font-size: 14px; line-height: 1.6;
}
.circuit-figure figcaption strong { color: var(--dg-ink); font-weight: 600; }
.circuit-figure .dg-zone { fill: var(--dg-paper); stroke: var(--dg-rule); stroke-width: 1.2; }
.circuit-figure .dg-box { fill: var(--dg-paper); stroke: var(--dg-rule); stroke-width: 1.2; }
.circuit-figure .dg-store { fill: var(--dg-surface); stroke: var(--dg-rule); stroke-width: 1.2; }
.circuit-figure .dg-focus { fill: var(--dg-tint); stroke: var(--dg-accent); stroke-width: 1.4; }
.circuit-figure .dg-label { fill: var(--dg-paper); stroke: none; }
.circuit-figure text { fill: var(--dg-ink); font-family: var(--md-text-font-family, 'Noto Sans', sans-serif); font-size: 16px; }
.circuit-figure .dg-name { font-weight: 600; }
.circuit-figure .dg-small { font-size: 14px; fill: var(--dg-muted); }
.circuit-figure .dg-code { font-family: var(--md-code-font-family, 'Ubuntu Mono', monospace); font-size: 14px; fill: var(--dg-muted); }
.circuit-figure .dg-tag { font-size: 12px; font-weight: 600; letter-spacing: .06em; fill: var(--dg-muted); }
.circuit-figure .dg-edge { fill: none; stroke: var(--dg-muted); stroke-width: 1.5; }
.circuit-figure .dg-marker { fill: var(--dg-muted); }
.circuit-figure .dg-marker-accent { fill: var(--dg-accent); }
.circuit-figure .dg-rule { stroke: var(--dg-rule); stroke-width: 1; }
.circuit-figure .dg-wave { fill: none; stroke: var(--dg-ink); stroke-width: 1.5; }
.circuit-figure .dg-wave-focus { fill: none; stroke: var(--dg-accent); stroke-width: 2; }
""".strip()


class Figure:
    def __init__(self, slug, title, description, height, caption):
        self.slug, self.title, self.description = slug, title, description
        self.height, self.caption = height, caption
        self.parts = []

    def rect(self, x, y, width, height, kind="box", radius=6, node=None):
        metadata = f' data-node="{escape(node)}"' if node else ""
        self.parts.append(f'<rect{metadata} x="{x}" y="{y}" width="{width}" height="{height}" rx="{radius}" class="dg-{kind}"/>')

    def text(self, x, y, value, kind="", anchor="start"):
        self.parts.append(f'<text x="{x}" y="{y}" class="dg-{kind or "body"}" text-anchor="{anchor}">{escape(value)}</text>')

    def line(self, x1, y1, x2, y2, arrow=False):
        assert x1 == x2 or y1 == y2, "Connectors must be orthogonal"
        marker = f' marker-end="url(#{self.slug}-arrow)"' if arrow else ""
        self.parts.append(f'<path class="dg-edge" d="M {x1} {y1} L {x2} {y2}"{marker}/>')

    def path(self, points, kind="edge", arrow=False):
        """An orthogonal polyline (each segment horizontal or vertical)."""
        for (x1, y1), (x2, y2) in zip(points, points[1:]):
            assert x1 == x2 or y1 == y2, "Connectors must be orthogonal"
        marker = f' marker-end="url(#{self.slug}-arrow)"' if arrow else ""
        d = " ".join(("M" if i == 0 else "L") + f" {x} {y}" for i, (x, y) in enumerate(points))
        self.parts.append(f'<path class="dg-{kind}" d="{d}"{marker}/>')

    def box(self, key, x, y, width, height, title, subtitle=None, kind="box"):
        self.rect(x, y, width, height, kind, node=key)
        cy = y + height / 2
        self.text(x + width / 2, cy - 3 if subtitle else cy + 5, title, "name", "middle")
        if subtitle:
            self.text(x + width / 2, cy + 19, subtitle, "small", "middle")

    def html(self):
        svg = f'''<svg viewBox="0 0 640 {self.height}" xmlns="http://www.w3.org/2000/svg" role="img" aria-labelledby="{self.slug}-title {self.slug}-desc">
<title id="{self.slug}-title">{escape(self.title)}</title>
<desc id="{self.slug}-desc">{escape(self.description)}</desc>
<style>{STYLE}</style>
<defs>
<marker id="{self.slug}-arrow" markerWidth="8" markerHeight="6" refX="7" refY="3" orient="auto"><path d="M 0 0 L 8 3 L 0 6 Z" class="dg-marker"/></marker>
<marker id="{self.slug}-arrow-accent" markerWidth="8" markerHeight="6" refX="7" refY="3" orient="auto"><path d="M 0 0 L 8 3 L 0 6 Z" class="dg-marker-accent"/></marker>
<marker id="{self.slug}-arrow-link" markerWidth="8" markerHeight="6" refX="7" refY="3" orient="auto"><path d="M 0 0 L 8 3 L 0 6 Z" class="dg-marker"/></marker>
</defs>
<rect width="640" height="{self.height}" fill="var(--dg-paper)"/>
{chr(10).join(self.parts)}
</svg>'''
        return f'''<!DOCTYPE html>
<html lang="en" class="diagram-standalone">
<head>
<meta charset="UTF-8"/>
<meta name="viewport" content="width=device-width, initial-scale=1"/>
<title>{escape(self.title)}</title>
<link href="https://fonts.googleapis.com/css2?family=Noto+Sans:wght@400;600&amp;family=Ubuntu+Mono&amp;display=swap" rel="stylesheet"/>
<style>body {{ margin: 24px auto; max-width: 688px; padding: 0 16px; }} * {{ box-sizing: border-box; }}</style>
</head>
<body>
<!-- diagram:start -->
<figure class="circuit-figure" data-diagram="{self.slug}">
<div class="diagram-viewport" role="region" aria-label="{escape(self.title)}" tabindex="0">
{svg}
</div>
<figcaption>{self.caption}</figcaption>
</figure>
<!-- diagram:end -->
</body>
</html>
'''


def memory_storage():
    f = Figure("memory-storage", "Where storage lives", "A native memory device wraps a bank of words or rows of cells with technology-specific addressing, access, and output circuitry.", 440,
               "<strong>Containment, not a signal path.</strong> Words or rows group cells; a bank organizes those groups. Native access circuitry makes the bank an addressed device.")
    f.rect(40, 24, 560, 392, "zone")
    f.text(64, 56, "Native memory device", "name")
    f.text(64, 84, "Address decoding · access / command control", "small")
    f.rect(64, 108, 512, 240, "store")
    f.text(88, 140, "Storage bank", "name")
    f.text(88, 164, "Word organization, or rows × bit columns", "small")
    f.rect(88, 188, 464, 96, "zone")
    f.text(112, 220, "One word / row", "name")
    f.rect(296, 208, 232, 52, "focus")
    f.text(412, 240, "Storage cells", "name", "middle")
    f.text(88, 324, "Column sensing / row buffer where the technology needs it", "small")
    f.text(64, 380, "Read selection · write distribution* · output enable", "small")
    f.text(64, 400, "* Writable devices only", "small")
    return f


def memory_endpoints():
    f = Figure("memory-endpoints", "Memory architecture: device, controller, endpoint",
               "MemoryEndpoint contains a MemoryController and one native memory device. The controller exposes the universal transaction interface and connects to the device through technology-specific native pins. The same device contract selects structural or behavioral fidelity for FF, SRAM, DRAM, ROM, or NOR flash.", 556,
               "<strong>One endpoint = controller + one device.</strong> The two device representations are alternatives, with the same native contract for the selected technology. Lines denote interfaces, not one-way data flow. Direct native access omits the controller. BehavioralMemoryEndpoint is the fused behavioral implementation of the whole assembly.")
    f.rect(24, 112, 592, 420, "zone", node="endpoint")
    f.text(48, 144, "MemoryEndpoint", "name")
    f.line(320, 88, 320, 176)
    f.box("transaction-interface", 64, 24, 512, 64,
          "Universal transaction interface", "REQ_* / RSP_* · 3.4", "focus")
    f.box("controller", 64, 176, 512, 64,
          "MemoryController", "FF bypass · timed native control · response retention")
    f.line(320, 240, 320, 254)
    f.text(320, 278, "Technology-specific native pins", "small", "middle")
    f.line(320, 288, 320, 312)
    f.rect(48, 312, 544, 196, "store", node="native-device")
    f.text(320, 340, "Native memory device · 3.3", "name", "middle")
    f.box("expanded-device", 64, 366, 232, 76,
          "Expanded device", "bank · words / rows · cells")
    f.text(320, 410, "or", "small", "middle")
    f.box("behavioral-device", 344, 366, 232, 76,
          "Behavioral device", "same pins · scalable storage")
    f.text(320, 480, "FF · SRAM · DRAM · ROM · NOR flash", "small", "middle")
    return f


def ff_memory():
    f = Figure("ff-memory", "FF memory: bank, word, cell", "FFMemory contains address and access helpers plus an FFStorageBank of N FFWord instances, each containing W FlipFlopCell instances.", 432,
               "<strong>N words × W bits.</strong> One representative word is expanded. Address decoding and read selection act around the bank; they are not additional storage layers.")
    f.rect(40, 24, 560, 384, "zone")
    f.text(64, 56, "FFMemory<W>", "name")
    f.text(64, 88, "ADDRESS_DECODE  ·  ACCESS_CONTROL  ·  READ_SELECT", "code")
    f.rect(64, 112, 512, 272, "store")
    f.text(88, 148, "STORAGE · FFStorageBank<W>", "name")
    f.text(552, 148, "N words", "small", "end")
    f.rect(88, 176, 464, 184, "zone")
    f.text(112, 212, "WORD_n · FFWord<W>", "name")
    f.text(528, 212, "W bits", "small", "end")
    for x, label in [(112, "b0"), (208, "b1"), (400, "b(W−1)")]:
        f.rect(x, 240, 120 if x == 400 else 72, 52, "focus" if x == 112 else "box")
        f.text(x + (60 if x == 400 else 36), 272, label, "code", "middle")
    f.text(344, 272, "…", "name", "middle")
    f.text(112, 332, "FlipFlopCell × W  ·  labels are bit positions, not values", "small")
    return f


def register32():
    f = Figure("register32", "Register<W> is one whole-word FF register", "Register<W> directly contains W FlipFlopCell instances with one shared whole-word write enable. W=1 contains only BIT_0.", 328,
               "<strong>One word, W cells.</strong> The single write enable fans out to all cells; W=1 uses only BIT_0. No byte-mask gates or nested register are needed.")
    f.rect(40, 24, 560, 280, "zone")
    f.text(64, 60, "Register<W>", "name")
    f.text(576, 60, "D · WE · CLK · RST → Q", "code", "end")
    f.rect(64, 88, 512, 144, "store")
    f.text(88, 120, "W × FlipFlopCell", "name")
    for x, label in [(88, "BIT_0"), (212, "BIT_1"), (436, "BIT_(W−1)")]:
        f.rect(x, 148, 116, 56, "focus" if x == 88 else "box")
        f.text(x + 58, 182, label, "code", "middle")
    f.text(384, 182, "…", "name", "middle")
    f.text(64, 272, "WE applies to all W bits  ·  no byte mask", "code")
    return f


def register_file():
    f = Figure("register-file", "31 stored words, two read ports", "The register file writes one of 31 whole-word registers for x1 to x31 and has two independent read selectors; x0 comes from a constant, not storage cells.", 432,
               "<strong>x0 is not stored.</strong> The two read selectors choose independently from x0 or the 31 stored words. The x0-to-selector wiring is omitted here; the connection table below specifies it.")
    f.text(40, 40, "RegisterFile32x32", "name")
    f.line(168, 160, 192, 160, True)
    f.line(408, 144, 456, 144, True)
    f.line(408, 272, 456, 272, True)
    f.box("write-decoder", 40, 124, 128, 72, "RD_DECODER", "one word")
    f.text(40, 100, "ONE WRITE PORT", "tag")
    f.rect(192, 80, 216, 256, "focus")
    f.text(216, 112, "STORAGE", "name")
    f.text(216, 136, "31 × Register<32>", "code")
    for y, word, register in [(156, "WORD_0", "x1"), (204, "WORD_1", "x2"), (280, "WORD_30", "x31")]:
        f.rect(208, y, 184, 36, "box", radius=4)
        f.text(224, y + 24, word, "code")
        f.text(376, y + 24, register, "code", "end")
    f.text(300, 264, "…", "name", "middle")
    f.text(456, 100, "TWO READ PORTS", "tag")
    f.box("rs1", 456, 112, 144, 64, "RS1_MUX", "RS1_VALUE")
    f.box("rs2", 456, 240, 144, 64, "RS2_MUX", "RS2_VALUE")
    f.rect(192, 360, 216, 48, "store")
    f.text(300, 390, "X0_ZERO = 0 · no cells", "code", "middle")
    return f


def memory_access():
    f = Figure("memory-access", "Memory requests out, load values back", "A request travels from the CPU through lane formatting and request tracking to an endpoint; returning load data is tracked, extracted, and extended before reaching the CPU.", 364,
               "<strong>Pipeline transaction path.</strong> Lanes = RV32IMemoryLanes; Request = RV32IMemoryRequest. Endpoint = controller plus native storage, or its fused MemoryEndpoint. The single-cycle and baseline cores re-use Lanes with combinational formatting to zero-wait endpoints. Arrows show payload flow, not clock cycles or the complete handshake wiring.")
    names = ["CPU", "Lanes", "Request", "Endpoint"]
    xs = [40, 184, 328, 472]
    f.text(40, 40, "OUTGOING REQUEST", "tag")
    f.text(40, 212, "RETURNING LOAD DATA", "tag")
    for y, reverse in [(96, False), (268, True)]:
        for left, right in zip(xs, xs[1:]):
            f.line(right if reverse else left + 128, y, left + 128 if reverse else right, y, True)
    for index, (x, name) in enumerate(zip(xs, names)):
        f.box("request-" + name, x, 64, 128, 64, name, kind="focus" if name == "Request" else "box")
        f.box("response-" + name, x, 236, 128, 64, name, kind="box")
        f.text(x + 64, 156, ["address + data", "place / mask", "hold until done", "access storage"][index], "small", "middle")
        f.text(x + 64, 328, ["load value", "extract / extend", "response word", "return word"][index], "small", "middle")
    return f


def core_stages():
    f = Figure("single-cycle-stages", "Five roles in logical order", "Fetch obtains the instruction, Decode reads registers, Execute computes, Memory accesses data if needed, and Writeback selects the destination value, without inter-stage pipeline registers.", 192,
               "<strong>Logical order, not five clock cycles.</strong> These are responsibilities within the single-cycle core, not pipeline stages separated by registers. Not every value passes through Memory; the writeback diagram shows the bypass paths.")
    stages = [("FETCH", "Instruction", "and PC"), ("DECODE", "Decode and", "register read"), ("EXECUTE", "ALU and", "targets"), ("MEMORY", "Data access", "if required"), ("WRITEBACK", "Destination", "value")]
    for index in range(4):
        f.line(128 + index * 124, 76, 144 + index * 124, 76, True)
    for index, (name, a, b) in enumerate(stages):
        x = 20 + index * 124
        f.box(name, x, 48, 108, 56, name)
        f.text(x + 54, 136, a, "small", "middle")
        f.text(x + 54, 156, b, "small", "middle")
    return f


def core_writeback():
    f = Figure("single-cycle-writeback", "Writeback chooses one source", "Writeback selects PC plus four, the ALU result, loaded data, or zero and sends WB_VALUE to the register file in Decode.", 344,
               "<strong>ALU and link results bypass Memory.</strong> DECODE.WRITEBACK_SEL selects the source. WB_VALUE is only a candidate: the register file captures it when REGISTER_WRITE permits completion.")
    sources = [("PC + 4", "FETCH", 32), ("ALU result", "EXECUTE", 104), ("Load value", "MEMORY", 176), ("Zero", "constant", 248)]
    for _, _, y in sources:
        f.line(224, y + 28, 280, y + 28, True)
    f.line(416, 164, 496, 164, True)
    for name, source, y in sources:
        f.box(source, 40, y, 184, 56, name, source)
    f.rect(280, 32, 136, 272, "focus", node="WRITEBACK")
    f.text(348, 148, "WRITEBACK", "name", "middle")
    f.text(348, 176, "Select one", "small", "middle")
    f.rect(416, 128, 80, 28, "label", radius=0)
    f.text(456, 148, "WB_VALUE", "code", "middle")
    f.box("register-file", 496, 132, 112, 64, "DECODE", "register file")
    return f


def core_next_pc():
    f = Figure("single-cycle-next-pc", "Select a next PC before updating it", "Next PC selects the fallthrough or branch/jump target using the decoded jump type and branch condition; Fetch owns the PC register.", 288,
               "<strong>Choosing a PC is not writing the PC.</strong> JUMP_TYPE and BRANCH_TAKEN control selection. NEXT_PC checks the active target's alignment; FETCH updates PC only when PC_WRITE is asserted.")
    f.line(224, 72, 280, 72, True)
    f.line(224, 176, 280, 176, True)
    f.line(416, 124, 480, 124, True)
    f.box("fallthrough", 40, 44, 184, 56, "PC + 4", "FETCH")
    f.box("target", 40, 148, 184, 56, "Control target", "EXECUTE")
    f.rect(280, 44, 136, 160, "focus", node="NEXT_PC")
    f.text(348, 116, "NEXT_PC", "name", "middle")
    f.text(348, 144, "Select + check", "small", "middle")
    f.box("PC", 480, 92, 120, 64, "FETCH", "PC input")
    f.text(40, 256, "Selection inputs:  DECODE.JUMP_TYPE  ·  EXECUTE.BRANCH_TAKEN", "code")
    return f


def core_completion():
    f = Figure("single-cycle-completion", "One edge commits the instruction", "Execution status combines decode legality, memory faults, and address checks to authorize PC and register updates. It authorizes memory intent before the response; the endpoint itself rejects invalid stores.", 328,
               "<strong>Compute from old state, then capture together.</strong> The endpoint validates stores independently: response faults do not feed back into the request opcode. DONE is tied high; the clock period must allow the full path to settle. Clock, reset, and ENABLE are omitted.")
    for y in (64, 164, 264):
        f.line(208, y, 248, y, True)
        f.line(412, y, 452, y, True)
    f.rect(248, 32, 164, 264, "focus", node="EXECUTION_STATUS")
    f.text(330, 148, "EXECUTION_STATUS", "code", "middle")
    f.text(330, 180, "Completion control", "small", "middle")
    for y, name, detail in [(60, "Decode", "legality / stop requests"), (160, "Fetch + Memory", "DONE / FAULT"), (260, "Address checks", "PC / target / data")]:
        f.text(40, y, name, "name")
        f.text(40, y + 24, detail, "small")
    for y, name, detail in [(60, "State writes", "PC / registers"), (160, "Memory intent", "prior-fault qualification"), (260, "Terminal state", "halt / trap / cause")]:
        f.text(464, y, name, "name")
        f.text(464, y + 24, detail, "small")
    return f


def baseline_pipeline():
    f = Figure("baseline-pipeline", "Five stages with four clock boundaries",
               "Fetch, Decode, Execute, Memory and Writeback are separated by IF/ID, ID/EX, EX/MEM and MEM/WB registers. Dependency checks and stall/bubble control form a separate control row. A separate six-bit status block remembers halt and trap events.", 490,
               "<strong>Boxes compute; narrow bars capture.</strong> Each instruction advances at most one register per edge. Dependencies wait for writeback; branches stop fetch until Execute resolves their next PC. Instruction and data memories are external. Writeback has no storage; completed PC is host-side observation.")
    f.text(20, 30, "DATAPATH", "tag")
    names = ["Fetch", "Decode", "Execute", "Memory", "Writeback"]
    notes = [("Instruction", "and fetch PC"), ("Decode and", "register read"), ("ALU and", "branch target"), ("Zero-wait load", "or store"), ("Register write", "and retirement")]
    for i, name in enumerate(names):
        x = 14 + i * 125
        f.box(name.upper(), x, 64, 96, 64, name)
        f.text(x + 48, 168, notes[i][0], "small", "middle")
        f.text(x + 48, 188, notes[i][1], "small", "middle")
        if i < 4:
            f.line(x + 96, 96, x + 107, 96, True)
            f.rect(x + 107, 54, 10, 84, "focus", radius=2, node=["IF_ID", "ID_EX", "EX_MEM", "MEM_WB"][i])
            f.line(x + 117, 96, x + 125, 96, True)
            f.text(x + 112, 48, ["IF/ID", "ID/EX", "EX/MEM", "MEM/WB"][i], "code", "middle")
    f.line(20, 220, 620, 220)
    f.text(20, 248, "CONTROL", "tag")
    f.box("DEPENDENCIES", 20, 272, 276, 68, "Dependencies", "Wait for EX / MEM / WB", "store")
    f.line(296, 306, 344, 306, True)
    f.box("CONTROL", 344, 272, 276, 68, "Advance, hold, or bubble", "Branch wait and precise stopping", "focus")
    f.box("EXECUTION_STATUS", 344, 388, 276, 68, "Execution status", "Halt / trap flags + cause · 6 bits", "store")
    f.line(482, 388, 482, 340, True)
    f.text(20, 408, "Writeback sends stop events.", "small")
    f.text(20, 432, "Status persists until reset.", "small")
    f.text(20, 456, "Completed PC: host observation.", "small")
    return f


def advanced_prediction():
    f = Figure("advanced-prediction", "Prediction travels with the instruction",
               "Fetch predecodes a target and predicts a next PC. The prediction is captured in IF/ID then ID/EX alongside its instruction. Recovery compares both Execute candidate addresses in parallel, then selects the mismatch using the branch or jump decision.", 504,
               "<strong>Two metadata registers, not two extra stages.</strong> They use the real IF/ID and ID/EX write enables. Recovery asks the coordinator to correct Fetch and discard younger work; a memory wait delays that action and training together.")
    f.text(40, 34, "FETCH · CURRENT INSTRUCTION", "tag")
    f.box("predecode",40,56,160,68,"Predecode","PC + B/J immediate")
    f.box("predictor",240,56,160,68,"Predictor","choose next PC","focus")
    f.box("if-id",440,56,160,68,"IF/ID metadata","capture prediction","store")
    f.line(200,90,240,90,True); f.line(400,90,440,90,True)
    f.text(40,162,"Used immediately as Fetch's next address.","small")
    f.text(40,186,"A copy follows the accepted instruction.","small")
    f.line(520,124,520,240,True)
    f.text(536,188,"edge","small")
    f.text(40,226,"EXECUTE · THAT SAME INSTRUCTION", "tag")
    f.box("execute",40,240,160,68,"Execute","candidates + taken")
    f.box("recovery",240,240,160,68,"Recovery","parallel compares","focus")
    f.box("id-ex",440,240,160,68,"ID/EX metadata","saved prediction","store")
    f.line(200,274,240,274,True); f.line(440,274,400,274,True)
    f.line(320,308,320,374,True)
    f.box("coordinator",208,374,224,72,"Coordinator","advance · hold · flush","focus")
    f.text(40,484,"On an accepted resolution: correct if needed; train conditional branches once.","small")
    return f


def timing_profile_gates():
    data = timing_profiles()
    gates = ("NOT", "NAND", "NOR", "AND", "OR", "XOR")
    f = Figure("timing-profile-gates", "Gate delays in the realistic profiles",
               "Bars give each logic gate's delay in the embedded and desktop timing profiles, in picoseconds; the unit profile gives every gate one tick.", 404,
               "<strong>One inverter driving four copies of itself (FO4) is the unit.</strong> Each gate is a fixed multiple of it, from the logical-effort method; "
               "<code>unit</code> gives every gate one tick instead. Values from the generated timing data.")
    top, row, scale = 48, 48, 3.2
    f.text(40, 28, "PICOSECONDS PER GATE", "tag")
    for i, gate in enumerate(gates):
        y = top + i * row
        f.text(96, y + 22, gate, "code", "end")
        for k, (profile, kind) in enumerate((("embedded", "focus"), ("desktop", "store"))):
            value = data[profile]["gates"][gate]
            width = grid(value * scale)
            f.rect(112, y + k * 20, width, 16, kind, radius=2)
            f.text(112 + width + 8, y + k * 20 + 13, f"{value} ps", "small")
    y = top + len(gates) * row + 16
    f.line(40, y, 600, y)
    f.rect(40, y + 20, 16, 12, "focus", radius=2); f.text(64, y + 31, "embedded (65 nm, FO4 30 ps)", "small")
    f.rect(320, y + 20, 16, 12, "store", radius=2); f.text(344, y + 31, "desktop (32 nm, FO4 21 ps)", "small")
    return f


def sram_access_chain():
    stages = (("Settle", "settle interval"), ("Precharge", "precharge interval"), ("Decode", "decoder replica"),
              ("Bitline", "bitline interval"), ("Sense, output", "sense + output"))
    f = Figure("sram-access-chain", "How an asynchronous SRAM access spends its time",
               "The stages of an asynchronous SRAM access in order: settle, precharge, decode, bitline development, and sense and output, after which READY rises.", 200,
               "<strong>Each stage starts when the previous one ends.</strong> Any change of the inputs clears every stage at once and the chain restarts. "
               "The intervals come from the timing profile (the bitline grows with the number of rows); the decode and output stages are replica gate chains as deep as the paths they copy. "
               "Widths are not to scale. The controller counts whole clock cycles around this, chosen from the proven access time.")
    f.text(40, 40, "INPUTS STEADY", "tag")
    f.text(600, 40, "READY", "tag", "end")
    for i, (name, source) in enumerate(stages):
        x = 40 + i * 112
        f.rect(x, 72, 112, 48, "focus" if name == "Bitline" else "box", radius=4, node=name.lower())
        f.text(x + 56, 102, name, "small", "middle")
        first, rest = source.split(" ", 1)
        f.text(x + 56, 144, first, "code", "middle")
        f.text(x + 56, 162, rest, "code", "middle")
    f.line(40, 184, 600, 184)
    return f


def dram_commands():
    data = timing_profiles()["embedded"]
    m, period = data["memory_intervals"], data["period_ticks"]
    measured = {a["operation"]: a["cycles"] for a in data["accesses"] if a["memory"] == "dram"}
    trp = m["dram_close"] + m["dram_precharge"] + m["dram_equalize"]
    trcd = m["dram_wordline"] + m["dram_sense_base"] + m["dram_restore"]
    cl = m["dram_read"]
    cases = (("Row open (hit)", [("READ", cl)], "read, row open (hit)"),
             ("Row closed", [("ACTIVATE", trcd), ("READ", cl)], "read, row closed"),
             ("Other row open", [("PRECHARGE", trp), ("ACTIVATE", trcd), ("READ", cl)], "read, other row open (conflict)"))
    f = Figure("dram-commands", "DRAM reads by row state",
               "Three DRAM reads in the embedded profile: a row hit needs one READ, a closed row an ACTIVATE then a READ, and a row conflict a PRECHARGE, an ACTIVATE and a READ; each bar is labeled with the measured cycles.", 288,
               "<strong>The row state decides the cost.</strong> Each command lasts its datasheet interval (tRP, tRCD, CL: "
               f"{trp // 1000}, {trcd // 1000} and {cl // 1000} ns for the <code>embedded</code> SDRAM); the controller waits whole cycles and adds the request and response edges. "
               "Measured cycles are the generated latencies of 3.6.")
    scale = 5.6 / 1000
    for i, (label, commands, key) in enumerate(cases):
        y = 48 + i * 72
        f.text(40, y + 28, label, "name")
        x = 216
        for name, value in commands:
            w = grid(value * scale)
            f.rect(x, y, w, 44, "focus" if name == "READ" else "box", radius=4)
            f.text(x + w / 2, y + 27, name, "small", "middle")
            x += w
        f.text(x + 12, y + 27, f"{measured[key]} cycles", "code")
    f.line(40, 264, 600, 264)
    return f


def transaction_handshake():
    f = Figure("transaction-handshake", "A delayed read through the transaction port",
               "Signal levels across six clock cycles: the request transfers at the first edge, the response appears later, waits for RSP_READY, and transfers at the edge where RSP_READY is high; REQ_READY never depends on RSP_READY in the same cycle.", 360,
               "<strong>Transfers happen at rising edges where VALID and READY are both high</strong> (marked). "
               "<code>REQ_READY</code> follows registered state only: it is high again in the cycle the response appears, since a new request may start as the old one completes, "
               "and low while the response is held because the requester keeps <code>RSP_READY</code> low.")
    signals = ("CLK", "REQ_VALID", "REQ_READY", "RSP_VALID", "RSP_READY")
    x0, step = 160, 72
    levels = {  # value per half cycle, 12 halves
        "REQ_VALID": [1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
        "REQ_READY": [1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1],
        "RSP_VALID": [0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0],
        "RSP_READY": [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1],
    }
    for edge in range(1, 6):
        f.line(x0 + edge * step, 36, x0 + edge * step, 316)
    for i, name in enumerate(signals):
        y = 56 + i * 52
        f.text(144, y + 16, name, "code", "end")
        if name == "CLK":
            pts = [(x0, y + 24)]
            for c in range(6):  # Rising edges on the edge lines.
                xa, xb, xc = x0 + c * step, x0 + c * step + step // 2, x0 + (c + 1) * step
                pts += [(xa, y), (xb, y), (xb, y + 24), (xc, y + 24)]
            f.path(pts, "wave")
            continue
        vals, pts = levels[name], []
        for h, v in enumerate(vals):
            xa, xb, yy = x0 + h * step // 2, x0 + (h + 1) * step // 2, (y if v else y + 24)
            if pts and pts[-1][1] != yy:
                pts.append((xa, yy))
            elif not pts:
                pts.append((xa, yy))
            pts.append((xb, yy))
        f.path(pts, "wave-focus" if name in ("REQ_READY",) else "wave")
    for edge, label in ((1, "request"), (5, "response")):
        f.rect(x0 + edge * step - 52, 314, 104, 28, "label", radius=0)
        f.text(x0 + edge * step, 332, label, "tag", "middle")
    return f


def sram_device():
    f = Figure("sram-device", "Inside the SRAM device",
               "The SRAM device: access control and the row decoder on the left drive the storage bank's wordlines, precharge sits above the bank, and column selection, write drivers and sense amplifiers sit below it, ending in the output buffer.", 440,
               "<strong>Rows across, bitline pairs down.</strong> <code>ACCESS_CONTROL</code> times each access and raises <code>READY</code>; "
               "the decoder opens one row; each column's bitline pair is precharged above the bank and sensed or driven below it. "
               "An array of more than 32 rows repeats this block as subarrays joined by global wires.")
    f.box("ACCESS_CONTROL", 32, 48, 176, 72, "ACCESS_CONTROL", "timing · READY", "focus")
    f.box("ROW_DECODE", 32, 180, 176, 112, "ROW_DECODE", "one wordline")
    f.box("PRECHARGE", 240, 48, 360, 48, "PRECHARGE", None)
    f.rect(240, 116, 360, 176, "store", node="STORAGE")
    f.text(256, 144, "STORAGE", "name")
    f.text(256, 168, "SRAMStorageBank: rows × bitline pairs", "code")
    for k in range(3):
        f.rect(256, 188 + k * 32, 328, 20, "box", radius=2)
    f.box("COLUMN_SELECT", 240, 312, 168, 48, "COLUMN_SELECT", None)
    f.box("WRITE_SENSE", 432, 312, 168, 48, "WRITE / SENSE", None)
    f.box("OUTPUT", 432, 380, 168, 40, "OUTPUT", None)
    f.line(208, 236, 240, 236, True)
    f.line(120, 120, 120, 180, True)
    f.line(208, 104, 240, 104, True)
    f.line(516, 360, 516, 380, True)
    f.line(408, 336, 432, 336, True)
    f.line(324, 292, 324, 312)
    f.line(516, 292, 516, 312)
    return f


def alu_datapath():
    f = Figure("alu-datapath", "Inside ALU<W>",
               "ALU<W>: the control decoder turns the opcode into local selectors; the arithmetic, logic, shifter and compare-select units compute in parallel; the result selector picks one word and the flag unit derives the flags.", 400,
               "<strong>Every unit computes; one result is selected.</strong> For comparisons the arithmetic unit's subtraction feeds "
               "<code>COMPARE_SELECT</code>, so there is no second subtractor. <code>A</code> and <code>B</code> can also pass straight to the result.")
    f.box("CONTROL", 40, 40, 144, 64, "CONTROL", "OP → selectors", "store")
    units = (("ARITHMETIC", "add · sub · neg"), ("LOGIC", "and · or · xor"), ("SHIFTER", "shift · rotate"), ("COMPARE_SELECT", "slt · min · max"))
    rows = [40 + i * 76 for i in range(4)]
    # Selectors from the decoder to every unit.
    f.path([(184, 72), (216, 72), (216, 300)])
    for y in rows:
        f.line(216, y + 30, 248, y + 30, True)
    for (name, sub), y in zip(units, rows):
        f.box(name, 248, y, 184, 60, name, sub)
    # Unit results to the result selector.
    f.path([(432, 70), (456, 70), (456, 300), (432, 300)])
    for y in rows[1:3]:
        f.line(432, y + 30, 456, y + 30)
    f.line(456, 168, 480, 168, True)
    f.box("RESULT_SELECT", 480, 132, 120, 72, "RESULT", "select", "focus")
    f.box("FLAGS", 480, 252, 120, 64, "FLAGS", "Z · N · C · V")
    f.line(540, 204, 540, 252, True)
    f.text(40, 360, "A, B and OP reach every unit; the decoder enables one result source.", "small")
    return f


def memory_latency():
    data = timing_profiles()
    names = {"ff": "Flip-flop", "sram": "SRAM chip", "sram-clocked": "SRAM macro", "rom": "ROM",
             "nor": "NOR flash"}
    rows = []
    for a in data["embedded"]["accesses"]:
        if not a["simulated"] or not a["operation"].startswith("read"):
            continue
        label = names.get(a["memory"], "DRAM " + a["operation"].split(", ", 1)[1].split(" (")[0]) if a["memory"] == "dram" else names[a["memory"]]
        desktop = next(b for b in data["desktop"]["accesses"] if (b["memory"], b["operation"]) == (a["memory"], a["operation"]))
        rows.append((label, a["cycles"], desktop["cycles"]))
    f = Figure("memory-latency", "Read latency by memory technology",
               "Bars give the measured read latency of each memory technology, in clock cycles of the advanced core, in the embedded and desktop profiles.", 96 + len(rows) * 48,
               "<strong>Cycles from the accepting edge to the edge that takes the response</strong>, for a 256 KiB memory at the advanced core's certified clock "
               f"(<code>embedded</code> {data['embedded']['period_ticks']:,} ps, <code>desktop</code> {data['desktop']['period_ticks']:,} ps). The flip-flop memory answers at the accepting edge. Values from the generated timing data; the table in 3.6 also gives writes, programs and erases.")
    scale = 8
    f.text(40, 28, "CYCLES", "tag")
    for i, (label, embedded, desktop) in enumerate(rows):
        y = 44 + i * 48
        f.text(176, y + 24, label, "small", "end")
        for k, (value, kind) in enumerate(((embedded, "focus"), (desktop, "store"))):
            width = max(grid(value * scale), 4)
            f.rect(192, y + k * 20, width, 14, kind, radius=2)
            f.text(192 + width + 8, y + k * 20 + 12, str(value), "small")
    y = 44 + len(rows) * 48 + 8
    f.line(40, y, 600, y)
    f.rect(40, y + 16, 16, 12, "focus", radius=2); f.text(64, y + 27, "embedded", "small")
    f.rect(200, y + 16, 16, 12, "store", radius=2); f.text(224, y + 27, "desktop", "small")
    return f


def cache_topologies():
    f = Figure("cache-topologies", "Five memory arrangements over one DRAM",
               "Five ways to connect a processor's instruction and data ports to DRAM main memory: two separate memories, one shared memory through an arbiter, private level-1 caches above the arbiter, a shared level-2 cache below it, and both.", 360,
               "<strong>Same requests, same DRAM, five connections.</strong> A gives each port its own copy of memory; B shares one DRAM through the two-port arbiter; "
               "C-ID adds an instruction and a data L1 above the arbiter; D-0 puts a shared L2 below it; D-ID has both.")
    xs = [40, 160, 280, 400, 520]
    names = ["A", "B", "C-ID", "D-0", "D-ID"]
    for x, name in zip(xs, names):
        f.text(x + 40, 32, name, "name", "middle")
        f.rect(x, 48, 36, 28, "box", radius=4); f.text(x + 18, 67, "I", "code", "middle")
        f.rect(x + 44, 48, 36, 28, "box", radius=4); f.text(x + 62, 67, "D", "code", "middle")
        y = 76
        if name == "A":
            f.line(x + 18, 76, x + 18, 252, True); f.line(x + 62, 76, x + 62, 252, True)
            # Each port's own DRAM, wide enough for its label.
            f.rect(x - 8, 252, 48, 40, "store", radius=4); f.text(x + 16, 276, "DRAM", "code", "middle")
            f.rect(x + 44, 252, 48, 40, "store", radius=4); f.text(x + 68, 276, "DRAM", "code", "middle")
            continue
        if name in ("C-ID", "D-ID"):
            f.line(x + 18, 76, x + 18, 100, True); f.line(x + 62, 76, x + 62, 100, True)
            f.rect(x, 100, 36, 28, "focus", radius=4); f.text(x + 18, 119, "L1", "code", "middle")
            f.rect(x + 44, 100, 36, 28, "focus", radius=4); f.text(x + 62, 119, "L1", "code", "middle")
            y = 128
        f.line(x + 18, y, x + 18, 152, True); f.line(x + 62, y, x + 62, 152, True)
        f.rect(x, 152, 80, 28, "box", radius=4); f.text(x + 40, 171, "ARB", "code", "middle")
        y = 180
        if name in ("D-0", "D-ID"):
            f.line(x + 40, 180, x + 40, 204, True)
            f.rect(x, 204, 80, 28, "focus", radius=4); f.text(x + 40, 223, "L2", "code", "middle")
            y = 232
        f.line(x + 40, y, x + 40, 252, True)
        f.rect(x, 252, 80, 40, "store", radius=4); f.text(x + 40, 276, "DRAM", "code", "middle")
    f.text(40, 320, "I, D: instruction and data ports. ARB: two-port arbiter.", "small")
    f.text(40, 342, "A: each DRAM holds the whole image.", "small")
    return f


def memory_arbiter():
    f = Figure("memory-arbiter", "Two-port memory arbiter",
               "Requests from ports I and D pass the grant and one multiplexer per field to the lower memory; responses pass the response route into one buffer per port; the state block holds busy, owner, priority and burst bits.",
               328,
               "<strong>Request path above, response path below.</strong> The grant chooses I or D and switches the request "
               "multiplexers; the state remembers who owns the request in flight and who goes first next. A response goes into "
               "its owner's buffer, so a port that is not ready cannot hold the lower memory.")
    f.box("ports", 40, 56, 96, 120, "I, D", "ports")
    f.box("grant", 200, 56, 168, 48, "GRANT", None, "focus")
    f.box("select", 200, 128, 168, 48, "SELECT_REQ_*", None)
    f.box("state", 432, 56, 168, 48, "STATE", None, "store")
    f.box("mem", 432, 128, 168, 104, "MEM_", "endpoint or cache", "store")
    f.box("route", 200, 256, 168, 48, "RESPONSE_ROUTE", None)
    f.box("buffers", 40, 256, 96, 48, "Buffers", None, "store")
    f.line(136, 80, 200, 80, True)
    f.line(136, 152, 200, 152, True)
    f.line(284, 104, 284, 128, True)
    f.line(368, 72, 432, 72, True)
    f.line(432, 92, 368, 92, True)
    f.line(368, 152, 432, 152, True)
    f.path([(480, 232), (480, 280), (368, 280)], arrow=True)
    f.line(200, 280, 136, 280, True)
    f.line(88, 256, 88, 176, True)
    f.text(400, 64, "ACCEPT", "tag", "middle")
    f.text(400, 112, "BUSY", "tag", "middle")
    f.text(400, 144, "REQ", "tag", "middle")
    f.text(492, 260, "RESPONSE", "tag")
    f.text(100, 220, "RSP", "tag")
    return f


CACHE_PAGE = ROOT / "site-docs/memory/cache-experiments.md"
RESULTS_PAGE = ROOT / "site-docs/rv32i/experiments.md"


def cache_table(after, indent="", page=None):
    """Rows of the first Markdown table after a line, as lists of cells."""
    lines = (page or CACHE_PAGE).read_text(encoding="utf-8").splitlines()
    start = lines.index(after) + 1
    rows = []
    for line in lines[start:]:
        text = line[len(indent):] if line.startswith(indent) else line
        if text.startswith("| "):
            rows.append([cell.strip() for cell in text.strip().strip("|").split("|")])
        elif rows:
            break
    return rows[2:]  # without the heading and rule rows


def cache_access_patterns():
    rows = [(r[0], [float(c.split(" / ")[0]) for c in r[1:]]) for r in cache_table("**Cycles per read** (`embedded` / `desktop`):")]
    f = Figure("cache-access-patterns", "Cycles per read by access pattern and path",
               "Bars give the cycles per read of three access patterns, read straight from DRAM, through one level-1 cache, and through a level-1 and a level-2 cache, in the embedded profile.",
               44 + len(rows) * 90 + 4,
               "<strong>Cycles per read, <code>embedded</code>, 2,048 reads.</strong> A cache helps sequential reads and slows down reads "
               "that use one word of each line; the second level adds a lookup to every miss. Values from the table above.")
    scale = 12
    paths = (("S0 DRAM", "store"), ("S1 L1", "focus"), ("S2 L1 + L2", "box"))
    f.text(40, 28, "CYCLES PER READ", "tag")
    for i, (label, values) in enumerate(rows):
        y = 44 + i * 90
        f.text(40, y + 14, label, "name")
        for k, (value, (path, kind)) in enumerate(zip(values, paths)):
            top = y + 24 + k * 20
            f.text(176, top + 12, path, "small", "end")
            width = max(grid(value * scale), 4)
            f.rect(192, top, width, 14, kind, radius=2)
            f.text(192 + width + 8, top + 12, f"{value:.2f}", "small")
    return f


def cache_speedup():
    programs = ("calls", "sort", "stream", "pointer-chase")
    groups = []
    for name in programs:
        rows = {(r[0], r[1]): int(r[2].replace(",", "")) for r in cache_table(f"    **{name}**", "    ", RESULTS_PAGE)}
        base = rows[("B", "none")]
        groups.append((name, [("A", base / rows[("A", "none")])] +
                       [(a, base / rows[(a, "Write buffer")]) for a in ("C-ID", "D-0", "D-ID")]))
    f = Figure("cache-speedup", "Speed of four memory arrangements against shared uncached DRAM",
               "For four programs, bars give the speed of separate memories, two level-1 caches, a shared level-2 cache, and both levels, the caches with a write buffer, relative to one shared uncached DRAM, in the embedded profile.",
               44 + len(groups) * 116 + 4,
               "<strong>Speed against B, one shared DRAM with no cache, <code>embedded</code>; caches with a two-entry write buffer.</strong> "
               "The line marks B; the fastest arrangement of each program is highlighted. Every behavior is in the table above.")
    scale, left = 72, 192
    f.text(40, 28, "SPEED AGAINST B (B = 1)", "tag")
    for i, (name, bars) in enumerate(groups):
        y = 44 + i * 116
        f.text(40, y + 14, name, "name")
        best = max(value for _, value in bars)
        for k, (arrangement, value) in enumerate(bars):
            top = y + 24 + k * 20
            f.text(176, top + 12, arrangement, "code", "end")
            width = grid(value * scale)
            f.rect(left, top, width, 14, "focus" if value == best else "store", radius=2)
            f.text(left + width + 8, top + 12, f"{value:.2f}×", "small")
    # B's level, drawn over the bars so it reads as one line.
    f.line(left + scale, 52, left + scale, 44 + len(groups) * 116 - 12)
    f.text(left + scale + 6, 48, "B", "code")
    return f


def figures():
    return [fn() for fn in (memory_storage, memory_endpoints, ff_memory, register32,
                            register_file, memory_access, core_stages, core_writeback,
                            core_next_pc, core_completion, baseline_pipeline, advanced_prediction,
                            timing_profile_gates, sram_access_chain, dram_commands, transaction_handshake,
                            sram_device, alu_datapath, memory_latency, cache_topologies,
                            cache_access_patterns, cache_speedup, memory_arbiter)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    if not args.check:
        DESTINATION.mkdir(parents=True, exist_ok=True)
    for figure in figures():
        path = DESTINATION / (figure.slug + ".html")
        rendered = figure.html()
        if args.check:
            if not path.exists() or path.read_text(encoding="utf-8") != rendered:
                raise SystemExit(f"Stale documentation figure: {path}")
        else:
            path.write_text(rendered, encoding="utf-8")
        print(f"{'Checked' if args.check else 'Rendered'} {figure.slug}")


if __name__ == "__main__":
    main()
