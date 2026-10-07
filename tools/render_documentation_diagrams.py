"""Render the hand-designed documentation figures, not a graph auto-layout.

Edit the semantic compositions below, then run this file. --check verifies that
the committed, self-contained HTML assets match their source. MkDocs inlines
only the marked figure, so rendering needs neither Mermaid nor an iframe.
"""

import argparse
from html import escape
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DESTINATION = ROOT / "site-docs/assets/diagrams"

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
               "MemoryEndpoint contains a MemoryController and one native memory device. The controller exposes the universal transaction interface and connects to the device through technology-specific native pins. The device is either expanded storage or a scalable MemoryContainer, for FF, SRAM, DRAM, ROM, or NOR flash.", 556,
               "<strong>One endpoint = controller + one device.</strong> The two device representations are alternatives, with the same native contract for the selected technology. Lines denote interfaces, not one-way data flow. Direct native access omits the controller. BehavioralMemoryEndpoint is the fused behavioral implementation of the whole assembly.")
    f.rect(24, 112, 592, 420, "zone", node="endpoint")
    f.text(48, 144, "MemoryEndpoint", "name")
    f.line(320, 88, 320, 176)
    f.box("transaction-interface", 64, 24, 512, 64,
          "Universal transaction interface", "REQ_* / RSP_* · 3.2", "focus")
    f.box("controller", 64, 176, 512, 64,
          "MemoryController", "FF bypass · timed native control · response retention")
    f.line(320, 240, 320, 254)
    f.text(320, 278, "Technology-specific native pins", "small", "middle")
    f.line(320, 288, 320, 312)
    f.rect(48, 312, 544, 196, "store", node="native-device")
    f.text(320, 340, "Native memory device · 3.1", "name", "middle")
    f.box("expanded-device", 64, 366, 232, 76,
          "Expanded device", "bank · words / rows · cells")
    f.text(320, 410, "or", "small", "middle")
    f.box("container", 344, 366, 232, 76,
          "MemoryContainer", "scalable behavioral storage")
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


def figures():
    return [fn() for fn in (memory_storage, memory_endpoints, ff_memory, register32,
                            register_file, memory_access, core_stages, core_writeback,
                            core_next_pc, core_completion, baseline_pipeline, advanced_prediction)]


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
