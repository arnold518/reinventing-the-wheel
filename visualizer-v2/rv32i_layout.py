"""RV32I floorplans: datapaths, pipeline payloads and numeric control lanes.

Geometry only; the simulator hierarchy and all real wires remain unchanged.
Slots include room for child pins, and use the actual child aspect (including
mixed-fidelity profiles). Unrecognized topologies use the graph-layout fallback.
"""
import math
import re

REVISION = "rv32i-stages-v2"
AUTOMATIC_REVISION = "rv32i-single-cycle-auto-v3"
BLOCKS_REVISION = "rv32i-blocks-v4"
ALU_AUTOMATIC_REVISION = "rv32i-alu-auto-v6"
SINGLE_CYCLE_REVISION = "rv32i-single-cycle-stage-order-v1"
ALU_AUTOMATIC_TYPES = {
    "ALU32", "AddSub32", "Logic32", "Shifter32", "ALU32ControlDecoder",
    "ALU32CompareUnit", "ALU32ResultSelector", "ALU32FlagUnit", "ZeroDetect32",
}
BLOCK_TYPES = {
    "AddSub32", "ZeroDetect32", "ALU32CompareUnit", "ALU32FlagUnit",
    "RV32IDecodeControlUnit", "RV32IInstructionRecognition", "RV32IDecodeControlSignals",
    "RV32IDecodeControlField", "RV32IControlFlowUnit", "RV32IMemoryAlignmentUnit",
    "RV32IMemoryLanes", "RV32IMemoryAddress", "RV32ILoadExtender", "RV32ILoadByte",
    "RV32IByteWriteMask", "RV32IMemoryRequest", "RV32IRequestTagMatch",
    "RV32IExecutionControlStatusUnit", "RV32IStatusPriority",
    # Ancestors must refit their changed child proportions too.
    "RV32ISingleCycleDecodeStage", "RV32ISingleCycleMemoryStage", "RV32IDecodeStage",
    "RV32IFiveStageCore", "RV32ISingleCycleCore", "RV32ISingleCycleFetchStage",
}
AUTOMATIC_TYPES = {
    *ALU_AUTOMATIC_TYPES,
    *(f"RV32ISingleCycle{stage}Stage" for stage in
      ("Fetch", "Decode", "Execute", "Memory", "Writeback", "NextPC")),
    "RV32IControlFlowUnit", "RV32IMemoryAlignmentUnit",
    "RV32IMemoryLanes", "RV32IMemoryAddress", "RV32ILoadExtender", "RV32IStatusPriority",
}
ASPECTS = {
    "AddSub32": .61,
    "ZeroDetect32": .38, "ALU32CompareUnit": .84, "ALU32FlagUnit": .725,
    "RV32ISingleCycleCore": .52, "RV32IFiveStageCore": .62,
    "RV32IFetchStage": .8, "RV32IDecodeStage": 1.0,
    "RV32IExecuteStage": .95, "RV32IMemoryStage": .95,
    "RV32IWritebackStage": 1.1,
    "RV32IIFIDPipelineRegister": 1.5, "RV32IIDEXPipelineRegister": 1.5,
    "RV32IEXMEMPipelineRegister": 1.5, "RV32IMEMWBPipelineRegister": 1.5,
    "RV32IControlFlowUnit": .75, "RV32IPipelineControlFlowUnit": .75,
    "RV32IMemoryAlignmentUnit": .5, "RV32IDecodeControlUnit": .9,
    "RV32IBitPatternMatcher": .8, "RV32IForwardingUnit": .9,
    "RV32IHazardDetectionUnit": .7, "RV32IPipelineCoordinator": .38,
    "RV32IExecutionControlStatusUnit": .95, "RV32IPipelineRetirementUnit": .65,
    "RV32IMemoryLanes": .85,
    "RV32IMemoryRequest": 1.1,
    "RV32IInstructionRecognition": 1.0, "RV32IDecodeControlSignals": 1.1,
    "RV32IDecodeControlField": .9, "RV32IMemoryAddress": .8,
    "RV32ILoadExtender": .8, "RV32ILoadByte": 1.0, "RV32IByteWriteMask": .8,
    "RV32IRequestTagMatch": 1.0, "RV32IStatusPriority": .65,
    "RV32ISingleCycleFetchStage": .9, "RV32ISingleCycleDecodeStage": .9,
    "RV32ISingleCycleExecuteStage": .9, "RV32ISingleCycleMemoryStage": 1.1,
    "RV32ISingleCycleWritebackStage": .7, "RV32ISingleCycleNextPCStage": .9,
}


def revision(kind):
    if kind == "RV32ISingleCycleCore":
        return SINGLE_CYCLE_REVISION
    if kind in ALU_AUTOMATIC_TYPES:
        return ALU_AUTOMATIC_REVISION
    if kind.split("<")[0] in BLOCK_TYPES:
        return BLOCKS_REVISION
    return AUTOMATIC_REVISION if kind in AUTOMATIC_TYPES else REVISION


def defaults(kind):
    kind = kind.split("<")[0]
    if kind not in ASPECTS:
        return {}
    aspect = ASPECTS[kind]
    title = (.06 if kind == "ZeroDetect32" else .045 if kind.endswith("Core") else .065 if kind == "RV32IPipelineCoordinator" else
             .24 if kind.endswith("PipelineRegister") else
             .14 if kind.endswith("Stage") or kind == "RV32IMemoryRequest" else .12)
    # Even the most pin-heavy stage fits its pins without stretching the box.
    # Coordinators have many outputs and intentionally occupy a wide rail.
    return {"aspect_ratio": aspect, "min_aspect_ratio": .08,
            "aspect_ratio_floor": .08, "title_bar_ratio": title,
            "pin_size_ratio": (.008 if kind == "RV32IPipelineCoordinator" else
                               .010 if kind == "RV32IFiveStageCore" else .018),
            "boundary_area_ratio": .025}


def natural(name):
    return [int(part) if part.isdigit() else part for part in re.split(r"(\d+)", name)]


class Floorplan:
    def __init__(self, aspects, aspect, title):
        self.aspects, self.aspect, self.result = aspects, aspect, {}
        self.top = max(.05, title/aspect+.025)

    def slot(self, name, x, y, width, height):
        if name not in self.aspects:
            return
        # Floorplan rectangles use a common .05..1 body coordinate system.
        # Reserve the real title before fitting, including in wide control rails.
        scale = (1-self.top)/.95
        y, height = self.top+(y-.05)*scale, height*scale
        # 30% horizontal clearance accommodates the widest legacy pin stubs.
        size = min(width / 1.30, height * self.aspect / self.aspects[name] * .86)
        h = size * self.aspects[name] / self.aspect
        self.result[name] = {"rel_pos": [x + (width-size)/2, y + (height-h)/2],
                             "rel_width": size}

    def grid(self, names, rect, columns):
        names = [n for n in names if n in self.aspects and n not in self.result]
        if not names:
            return
        x, y, w, h = rect
        rows = math.ceil(len(names)/columns)
        for i, name in enumerate(names):
            self.slot(name, x+(i % columns)*w/columns, y+(i//columns)*h/rows,
                      w/columns, h/rows)

    def rest(self, rect, columns):
        self.grid(sorted(set(self.aspects)-set(self.result), key=natural), rect, columns)


def placements(kind, aspects, parent_aspect):
    kind = kind.split("<")[0]
    if kind not in ASPECTS or not aspects or kind in AUTOMATIC_TYPES | {"RV32IFiveStageCore", "RV32ISingleCycleCore"}:
        # Stage internals and ALU components use normal connectivity layout.
        # Complete cores and request payloads have separate dedicated layouts.
        return None
    p = Floorplan(aspects, parent_aspect, defaults(kind)["title_bar_ratio"])
    names = list(aspects)
    if kind.endswith("PipelineRegister"):
        # Identical ordering in all four pipeline latches, with flush/valid
        # separate from the payload. No alphabetic reordering of PC and control.
        fields = ("PC", "INSTRUCTION", "RS1_VALUE", "RS2_VALUE", "IMMEDIATE", "ALU_RESULT",
                  "STORE_DATA", "MEMORY_DATA", "PC_PLUS_4", "NEXT_PC", "CONTROL",
                  "REGISTER_ADDRESSES", "FETCH_STATUS", "EXECUTION_STATUS", "MEMORY_STATUS")
        p.grid(["NOT_FLUSH", "VALID_GATE", "VALID_STATE"], (.05,.06,.9,.13), 3)
        p.grid([f+"_STATE" for f in fields], (.08,.23,.84,.72), 2)
        if set(p.result) != set(names): return None
    elif kind == "RV32IFetchStage":
        for name, rect in {
            "PC_STATE": (.08,.12,.25,.28), "PC_PLUS_4": (.39,.13,.23,.24),
            "NEXT_PC_MUX": (.68,.13,.24,.24), "ALIGNMENT": (.08,.47,.26,.24),
            "PC_OUTPUT_MUX": (.40,.46,.24,.21),
            "INSTRUCTION_OUTPUT_MUX": (.69,.46,.24,.21),
            "STATUS_OUTPUT_MUX": (.40,.71,.24,.20),
        }.items(): p.slot(name,*rect)
        p.rest((.07,.93,.86,.05), 6)
    elif kind == "RV32IDecodeStage":
        for name, rect in {
            "CONTROL": (.06,.09,.25,.36), "REGISTER_FILE": (.37,.09,.29,.36),
            "RS1_MUX": (.73,.10,.21,.15), "RS2_MUX": (.73,.29,.21,.15),
            "HAZARD": (.08,.51,.35,.24), "WB_BYPASS": (.54,.51,.37,.24),
        }.items(): p.slot(name,*rect)
        p.grid([n for n in names if "PACK" in n or n == "PASS"], (.06,.78,.88,.13), 7)
        p.rest((.06,.935,.88,.05), 10)
    elif kind == "RV32IExecuteStage":
        for name, rect in {
            "FORWARDING": (.06,.09,.27,.31),
            "FORWARD_A_MUX": (.37,.09,.17,.14), "FORWARD_B_MUX": (.37,.26,.17,.14),
            "ALU_A_MUX": (.57,.09,.17,.14), "ALU_B_MUX": (.57,.26,.17,.14),
            "ALU": (.77,.10,.19,.30), "CONTROL_FLOW": (.27,.47,.46,.28),
        }.items(): p.slot(name,*rect)
        p.grid([n for n in names if "PACK" in n or n == "PASS"], (.06,.78,.88,.12), 5)
        p.rest((.06,.93,.88,.055), 10)
    elif kind == "RV32IMemoryStage":
        p.slot("ALIGNMENT", .06,.10,.35,.22)
        p.slot("FORWARD_VALUE_MUX", .60,.10,.33,.22)
        p.grid([n for n in names if "PACK" in n or "PASS" in n or "SPLIT" in n or "JOIN" in n],
               (.05,.37,.9,.22), 8)
        p.grid([n for n in names if n.startswith(("LOAD_", "STORE_", "DMEM_"))], (.05,.63,.9,.12), 10)
        p.rest((.05,.80,.9,.17), 8)
    elif kind == "RV32IWritebackStage":
        p.slot("RETIREMENT", .05,.09,.35,.22)
        p.slot("WRITEBACK_MUX", .43,.09,.24,.22)
        p.slot("RETIRED_COUNT_PLUS_1", .71,.09,.24,.22)
        states = ["COMMITTED_PC_STATE", "HALTED_STATE", "TRAPPED_STATE", "TRAP_CAUSE_STATE",
                  "RETIRED_COUNT_STATE", "RETIRED_PC_STATE", "RETIRED_INSTRUCTION_STATE",
                  "RETIRED_MEM_ADDR_STATE", "RETIRED_MEM_WRITE_DATA_STATE",
                  "RETIRED_MEM_READ_DATA_STATE", "RETIRED_MEM_META_STATE"]
        p.grid(states, (.06,.36,.88,.30), 4)
        p.grid([n for n in names if n.endswith("_MUX")], (.06,.69,.88,.11), 4)
        p.grid([n for n in names if "PACK" in n or "SLICE" in n or "EXPAND" in n or n == "VALID_PASS"],
               (.06,.82,.88,.09), 9)
        p.rest((.06,.93,.88,.05), 16)
    elif kind in {"RV32IControlFlowUnit", "RV32IPipelineControlFlowUnit"}:
        p.grid(["PC_STATE", "PC_PLUS_4_ADD", "PC_IMM_ADD", "PC_IMMEDIATE_ADD", "RS1_IMM_ADD",
                "RS1_IMMEDIATE_ADD", "JALR_MASK"], (.06,.10,.88,.23), 5)
        p.grid(["NOT_EQ", "NOT_LT_SIGNED", "NOT_LT_UNSIGNED"], (.06,.39,.23,.21), 1)
        p.slot("BRANCH_MUX", .34,.37,.17,.29)
        p.slot("BRANCH_NEXT_MUX", .56,.40,.17,.24)
        p.slot("JUMP_NEXT_MUX", .78,.40,.17,.24)
        p.grid([n for n in names if "SPLIT" in n], (.06,.72,.30,.22), 3)
        p.rest((.42,.74,.53,.18), 4)
    elif kind == "RV32IMemoryAlignmentUnit":
        p.grid(["ADDRESS_SPLIT", "SIZE_SPLIT"], (.05,.12,.20,.79), 2)
        p.slot("LOW_ADDRESS_OR", .31,.57,.19,.25)
        p.slot("HALF_MISALIGNED", .56,.18,.18,.25)
        p.slot("WORD_MISALIGNED", .56,.59,.18,.25)
        p.slot("MISALIGNED_OR", .79,.40,.16,.25)
    elif kind == "RV32IBitPatternMatcher":
        p.slot("INPUT_SPLIT", .05,.10,.09,.83)
        # Each tested bit owns a lane; the AND chain is numbered, not lexical.
        p.grid(sorted((n for n in names if n.startswith("NOT_BIT_")), key=natural),
               (.19,.10,.31,.83), 4)
        p.grid(sorted((n for n in names if n.startswith("AND_")), key=natural),
               (.55,.10,.40,.83), 4)
    elif kind == "RV32IDecodeControlUnit":
        # A generic graph gives the tiny constant its own full column. Keep
        # instruction fields above recognition, then controls and immediate
        # selection, with the constant tucked beside its only consumer.
        for name, rect in {
            "FIELDS_AND_IMMEDIATES": (.035,.12,.30,.30),
            "RECOGNITION": (.035,.48,.30,.48),
            "CONTROL_SIGNALS": (.36,.28,.30,.64),
            "IMMEDIATE_MUX": (.71,.25,.25,.55),
            "CONST_ZERO32": (.76,.84,.18,.10),
        }.items(): p.slot(name,*rect)
        if set(p.result) != set(names): return None
    elif kind == "RV32IInstructionRecognition":
        # Shared predicates on the left; one instruction recognition row on
        # the right, then a single pair of bundled match outputs.
        p.grid([n for n in names if n.startswith("PREDECODE_")], (.04,.08,.38,.80), 2)
        instructions = {}
        for name in names:
            m = re.match(r"(RECOGNIZE_.*)_AND_\d+$", name)
            if m: instructions.setdefault(m[1], []).append(name)
        for i, group in enumerate(instructions.values()):
            p.grid(sorted(group,key=natural), (.49,.08+i*.80/len(instructions),.32,.80/len(instructions)),2)
        p.slot("MATCH_LINES",.86,.12,.11,.72)
    elif kind == "RV32IDecodeControlSignals":
        # Related outputs share a row: ALU, register use, writeback, memory,
        # control flow, and legality/terminal status.
        groups = (("ALU_OP","ALU_A_SEL","ALU_B_SEL"),
                  ("USES_RS1","USES_RS2","IMMEDIATE_SELECT"),
                  ("REG_WRITE","WRITEBACK_SEL","LEGAL"),
                  ("MEM_READ","MEM_WRITE","MEM_SIZE"),
                  ("BRANCH_TYPE","JUMP_TYPE","LOAD_SIGN_EXTEND"),
                  ("HALT_REQUEST","TRAP_REQUEST","DECODE_TRAP_CAUSE"))
        for row, group in enumerate(groups):
            p.grid(group,(.04,.08+row*.15,.92,.135),3)
    elif kind == "RV32IDecodeControlField":
        # Fold long serial equations into numbered rows; auto layout would
        # allocate one column per OR gate and shrink every label to a speck.
        p.slot("MATCH_BITS",.025,.12,.12,.73)
        p.slot("OUTPUT_BITS",.86,.12,.12,.73)
        chains = {}
        for name in names:
            m = re.match(r"BIT_(\d+)_",name)
            if m: chains.setdefault(int(m[1]),[]).append(name)
        rows = sum(math.ceil(len(g)/8) for g in chains.values())
        step = .80/max(1,rows+.4*len(chains))
        y = .09
        for group in chains.values():
            ordered = sorted(group,key=lambda n:("DEFAULT_NOT" in n,natural(n)))
            count = math.ceil(len(group)/8)
            p.grid(ordered,(.18,y,.64,count*step),8)
            y += (count+.4)*step
    elif kind in {"RV32IForwardingUnit", "RV32IHazardDetectionUnit"}:
        p.grid([n for n in names if n.endswith("_SPLIT")], (.05,.08,.90,.13),4)
        # Comparison banks: rs1 above rs2, and EX before MEM. Each bank keeps
        # its XOR -> NOT -> AND reductions in consecutive, numbered rows.
        prefixes = (["EX_EQ_A", "MEM_EQ_A", "EX_EQ_B", "MEM_EQ_B"]
                    if kind == "RV32IForwardingUnit" else ["EX_RD_EQ_RS1", "EX_RD_EQ_RS2"])
        for i, prefix in enumerate(prefixes):
            x, y = .055 + (i%2)*.47, .26 + (i//2)*.25
            for j, suffix in enumerate(("XOR", "XNOR", "ALL")):
                group = sorted((n for n in names if n.startswith(prefix+"_"+suffix+"_")),key=natural)
                p.grid(group, (x,y+j*.07,.42,.055),5)
        p.rest((.05,.79 if len(prefixes)==4 else .57,.90,.17 if len(prefixes)==4 else .36),8)
    elif kind == "RV32ILoadByte":
        p.slot("DATA",.02,.12,.10,.74)
        p.slot("RESULT",.88,.12,.10,.74)
        for bit in range(8):
            p.grid([f"BYTE_{bit}",f"HALF_{bit}",f"WORD_{bit}",f"RESULT_{bit}_0_0",f"RESULT_{bit}_1_0"],
                   (.15,.10+bit*.105,.70,.09),5)
    elif kind == "RV32IByteWriteMask":
        p.slot("OFFSET",.03,.1,.12,.6)
        p.grid(["NOT_A0","NOT_A1"],(.03,.76,.12,.20),1)
        p.slot("MASK",.87,.10,.10,.79)
        for byte in range(4):
            p.grid([f"SELECT_LANE_{byte}",f"BYTE_MASK_{byte}",f"HALF_MASK_{byte}",f"SUBWORD_MASK_{byte}",f"MASK_{byte}"],
                   (.20,.14+byte*.19,.62,.14),5)
    elif kind == "RV32IRequestTagMatch":
        p.slot("CURRENT_TAG",.02,.09,.10,.81)
        p.slot("SAVED_TAG",.14,.09,.10,.81)
        p.grid([f"TAG_DIFFERENT_{bit}" for bit in range(32)],(.29,.09,.20,.81),2)
        for level in range(5):
            p.grid([f"TAG_CHANGED_{level}_{bit}" for bit in range(16 >> level)],
                   (.53+level*.09,.09,.075,.81),1)
    elif kind == "RV32IMemoryRequest":
        p.grid(["REQUEST_OP","REQUEST_WRITE_MASK","REQUEST_ADDR","REQUEST_WRITE_DATA","REQUEST_TAG"],
               (.03,.10,.32,.64),1)
        p.slot("TAG_MATCH",.40,.10,.25,.38)
        p.grid(["BUSY","SENT","ABANDONED"],(.40,.54,.55,.17),3)
        p.slot("RESPONSE_DATA",.71,.10,.25,.14)
        p.slot("STATUS",.71,.30,.11,.18)
        p.grid([n for n in names if n.startswith("FAULT_")],(.84,.30,.12,.18),1)
        p.rest((.04,.77,.92,.20),10)
    elif kind == "RV32IPipelineCoordinator":
        groups = [
            [n for n in names if n.startswith(("NOT_HALTED", "NOT_TRAPPED", "ACTIVE", "MEMORY_", "NOT_DMEM", "DATA_PORT_"))],
            [n for n in names if n.startswith(("TERMINAL_", "NORMAL_STEP", "KILL_", "ANY_TERMINAL", "FETCH_VALID", "REDIRECT", "IMEM_", "DMEM_"))],
            [n for n in names if any(s in n for s in ("IF_ID", "ID_EX", "EX_MEM", "MEM_WB", "COMMIT_ENABLE", "FETCH_PC"))],
        ]
        for i, group in enumerate(groups): p.grid(group,(.05,.15+i*.20,.90,.17),16)
        p.rest((.05,.80,.90,.15),16)
    elif kind == "RV32IExecutionControlStatusUnit":
        # Preparation at left, explicit priority order down the middle, cause
        # selection at right, and architectural status storage along the bottom.
        priority = ["PC_MISALIGNED","IMEM_FAULT","DECODE_TRAP","HALT",
                    "DATA_MISALIGNED","DATA_FAULT","TARGET_MISALIGNED"]
        p.grid(["PRIORITY_"+n for n in priority],(.40,.08,.23,.71),1)
        p.grid([n for n in names if "MUX" in n],(.68,.23,.28,.53),2)
        p.grid([n for n in names if n.startswith("CAUSE_") and not n.endswith(("MUX","SPLIT","JOIN"))],
               (.68,.08,.28,.12),4)
        p.grid(["HALTED_STATE","TRAPPED_STATE",*[f"TRAP_CAUSE_BIT_{i}" for i in range(4)]],
               (.12,.85,.76,.13),6)
        p.grid(["CAUSE_D_SPLIT","CAUSE_Q_JOIN"],(.69,.78,.25,.055),2)
        p.rest((.03,.08,.32,.72),5)
    elif kind == "RV32IPipelineRetirementUnit":
        p.grid([n for n in names if n.startswith("CAUSE_") and not n.endswith("_MUX")],(.05,.08,.90,.08),9)
        p.grid([n for n in names if "MUX" in n or n.endswith("_CAUSE")],(.05,.20,.90,.16),8)
        priority = [n for n in names if n.startswith("PRIORITY_")]
        p.grid(priority,(.05,.41,.90,.18),9)
        p.grid([n for n in names if "STATE" in n or n.startswith("TRAP_CAUSE_BIT_")],(.05,.83,.90,.12),6)
        p.rest((.05,.63 if priority else .41,.90,.15 if priority else .35),10)
    else:
        return None
    if set(p.result) != set(aspects):
        return None
    return p.result
