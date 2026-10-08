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
SINGLE_CYCLE_REVISION = "rv32i-single-cycle-stage-order-v3-store-intent"
ADVANCED_REVISION = "rv32i-advanced-grouping-v2"
ALU_AUTOMATIC_TYPES = {
    "ALU32", "ALU32Core", "AddSub32", "Logic32", "Shifter32", "ALU32ControlDecoder",
    "ALU32CompareUnit", "ALU32ResultSelector", "ALU32FlagUnit", "ZeroDetect32",
}
BLOCK_TYPES = {
    "Decoder5to32",
    "Mux2to1_4bit", "Mux2to1_32bit",
    "RV32IDecodeControlField",
    "AddSub32", "ZeroDetect32", "ALU32CompareUnit", "ALU32FlagUnit",
    "RV32IDecodeControlUnit", "RV32IInstructionRecognition", "RV32IDecodeControlSignals",
    "RV32IDecodeControlField", "RV32IControlFlowUnit", "RV32IMemoryAlignmentUnit",
    "RV32IMemoryLanes", "RV32IMemoryAddress", "RV32ILoadExtender", "RV32ILoadByte",
    "RV32IMemoryLanesDataOnly", "RV32IMemoryAddressDataOnly", "RV32IFetchRequest",
    "IncrementBy4", "RV32IWritebackBypass",
    "RV32IByteWriteMask", "RV32IMemoryRequest", "RV32IRequestTagMatch",
    "RV32IExecutionControlStatusUnit", "RV32IStatusPriority",
    # Ancestors must refit their changed child proportions too.
    "RV32ISingleCycleDecodeStage", "RV32ISingleCycleMemoryStage", "RV32IDecodeStage",
    "RV32IFiveStageCore", "RV32ISingleCycleCore", "RV32ISingleCycleFetchStage",
}
AUTOMATIC_TYPES = {
    *ALU_AUTOMATIC_TYPES,
    "RV32IForwardingUnit", "RV32IHazardDetectionUnit", "RV32IPipelineCoordinator",
    "RV32IPipelineRetirementUnit",
    *(f"RV32ISingleCycle{stage}Stage" for stage in
      ("Fetch", "Decode", "Execute", "Memory", "Writeback", "NextPC")),
    "RV32IControlFlowUnit", "RV32IMemoryAlignmentUnit",
    "RV32IMemoryLanes", "RV32IMemoryAddress", "RV32IMemoryLanesDataOnly",
    "RV32IMemoryAddressDataOnly", "RV32ILoadExtender", "RV32IStatusPriority",
    "IncrementBy4", "RV32IWritebackBypass",
    "RV32IExecutePacketStatus",
}
ASPECTS = {
    "Decoder5to32": 2.1,
    "Mux2to1_4bit": .8, "Mux2to1_32bit": 2.1,
    "RV32IPredictionPath": .60, "RV32IBranchPredictor": .80,
    "RV32IPredictionTable": 1.6, "RV32IPredictionEntry": .8,
    "RV32IForwardedOperands": .75, "RV32IExecutePacketStatus": .75,
    "RV32IBaselinePipelineCore": .55,
    "RV32IBaselineFetchStage": .85, "RV32IBaselineDecodeStage": .9,
    "RV32IBaselineExecuteStage": .95, "RV32IBaselineMemoryStage": 1.0,
    "RV32IBaselineDependencyUnit": .65, "RV32IBaselineControl": .6,
    "RV32IBaselinePacketStatus": .65,
    "AddSub32": .61,
    "ZeroDetect32": .38, "ALU32CompareUnit": .84, "ALU32FlagUnit": .725,
    "RV32ISingleCycleCore": .52, "RV32IFiveStageCore": .52,
    "RV32IFetchStage": .8, "RV32IDecodeStage": 1.0,
    "RV32IExecuteStage": .95, "RV32IMemoryStage": .95,
    "RV32IWritebackStage": 1.1, "RV32IPipelineExecutionStatus": .75,
    # Pipeline boundaries are narrow, stage-height banks, not small word boxes.
    "RV32IIFIDPipelineRegister": 2.6, "RV32IIDEXPipelineRegister": 2.6,
    "RV32IEXMEMPipelineRegister": 2.6, "RV32IMEMWBPipelineRegister": 2.6,
    "RV32IControlFlowUnit": .75,
    "RV32IMemoryAlignmentUnit": .5, "RV32IDecodeControlUnit": .9,
    "RV32IBitPatternMatcher": .8, "RV32IForwardingUnit": .9,
    "RV32IHazardDetectionUnit": .7, "RV32IPipelineCoordinator": .38,
    "RV32IExecutionControlStatusUnit": .95, "RV32IPipelineRetirementUnit": .65,
    "RV32IMemoryLanes": .85,
    "RV32IMemoryLanesDataOnly": .80,
    "RV32IMemoryRequest": 1.5,
    "RV32IFetchRequest": 1.15,
    "IncrementBy4": .72, "RV32IWritebackBypass": .65,
    "RV32IInstructionRecognition": 1.0, "RV32IDecodeControlSignals": 1.1,
    "RV32IDecodeControlField": .9, "RV32IMemoryAddress": .8,
    "RV32ILoadExtender": .8, "RV32ILoadByte": 1.0, "RV32IByteWriteMask": .8,
    "RV32IRequestTagMatch": 1.0, "RV32IStatusPriority": .65,
    "RV32ISingleCycleFetchStage": .9, "RV32ISingleCycleDecodeStage": .9,
    "RV32ISingleCycleExecuteStage": .9, "RV32ISingleCycleMemoryStage": 1.1,
    "RV32ISingleCycleWritebackStage": .7, "RV32ISingleCycleNextPCStage": .9,
}


def revision(kind):
    if kind in {"RV32IFiveStageCore", "RV32IPipelineCoordinator", "RV32IBaselinePipelineCore"} or kind.endswith("PipelineRegister"):
        return ADVANCED_REVISION
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
                               .010 if kind in {"RV32IFiveStageCore", "RV32IBaselinePipelineCore"} else .018),
            "boundary_area_ratio": .025}


def natural(name):
    return [int(part) if part.isdigit() else part for part in re.split(r"(\d+)", name)]


class Floorplan:
    def __init__(self, aspects, aspect, title, vertical_fill=.86):
        self.aspects, self.aspect, self.result = aspects, aspect, {}
        self.top = max(.05, title/aspect+.025)
        self.vertical_fill = vertical_fill

    def slot(self, name, x, y, width, height):
        if name not in self.aspects:
            return
        # Floorplan rectangles use a common .05..1 body coordinate system.
        # Reserve the real title before fitting, including in wide control rails.
        scale = (1-self.top)/.95
        y, height = self.top+(y-.05)*scale, height*scale
        # 30% horizontal clearance accommodates the widest legacy pin stubs.
        size = min(width / 1.30, height * self.aspect / self.aspects[name] * self.vertical_fill)
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
    # Dense matcher columns also reserve the legacy gates' tall pin markers.
    p = Floorplan(aspects, parent_aspect, defaults(kind)["title_bar_ratio"],
                  vertical_fill=.60 if kind == "RV32IDecodeControlField" else
                                .70 if kind == "RV32IBitPatternMatcher" else .86)
    names = list(aspects)
    if kind.endswith("PipelineRegister"):
        # Identical field ordering in all four pipeline latches. Compact
        # fields visibly pass through split -> narrow payload slices -> join;
        # full-width fields occupy the same middle storage column.
        fields = ("PC", "INSTRUCTION", "RS1_VALUE", "RS2_VALUE", "IMMEDIATE", "ALU_RESULT",
                  "STORE_DATA", "MEMORY_DATA", "PC_PLUS_4", "NEXT_PC", "CONTROL",
                  "REGISTER_ADDRESSES", "FETCH_STATUS", "EXECUTION_STATUS", "MEMORY_STATUS")
        p.grid(["NOT_FLUSH", "VALID_GATE", "VALID_STATE"], (.10,.06,.80,.12), 3)
        present = [f for f in fields if any(n.startswith(f+"_") for n in names)]
        for index, field in enumerate(present):
            row_y = .21 + index * .74 / len(present)
            row_height = .70 / len(present)
            p.slot(field+"_SPLIT",.05,row_y,.17,row_height)
            parts = sorted((n for n in names if n == field+"_STATE" or
                            n.startswith(field+"_STATE_")),key=natural)
            p.grid(parts,(.29,row_y,.42,row_height),max(1,len(parts)))
            p.slot(field+"_JOIN",.78,row_y,.17,row_height)
        if set(p.result) != set(names): return None
    elif kind == "Decoder5to32":
        # Literal preparation -> low/high predecode -> 32 product terms ->
        # output enables. Row n represents decoded register address n.
        p.slot("ADDR_SPLIT", .02,.13,.12,.77)
        p.grid([f"NOT_ADDR{i}" for i in range(5)], (.16,.15,.08,.70), 1)
        p.grid([f"LOW_{i}" for i in range(4)], (.27,.15,.09,.70), 1)
        p.grid([f"HIGH_PAIR_{i}" for i in range(8)], (.39,.12,.08,.82), 1)
        p.grid([f"HIGH_{i}" for i in range(8)], (.49,.12,.08,.82), 1)
        for i in range(32):
            col, row = i // 16, i % 16
            y = .08 + row * .054
            p.slot(f"MATCH_{i}", .60 + col*.08, y, .075,.050)
            p.slot(f"OUT{i}_ENABLE_AND", .79 + col*.08, y, .075,.050)
        p.slot("OUT0_ZERO", .785,.08,.08,.05)
        if set(p.result) != set(names): return None
    elif kind in {"Mux2to1_4bit", "Mux2to1_32bit"}:
        # Two data buses and a shared inverted select feed parallel per-bit
        # AND/OR lanes. No phantom one-bit MUX_BIT children remain.
        width = int(kind.split("_")[1][:-3])
        columns = 2 if width == 32 else 1
        p.slot("A_SPLIT", .02,.14,.10,.73)
        p.slot("B_SPLIT", .14,.14,.10,.73)
        p.slot("NOT_SEL", .27,.08,.09,.12)
        for prefix, rect in (("SELECT_A_",(.29,.21,.17,.70)),
                             ("SELECT_B_",(.47,.21,.17,.70)),
                             ("COMBINE_",(.65,.21,.17,.70))):
            p.grid([f"{prefix}{bit}" for bit in range(width)], rect, columns)
        p.slot("OUT_JOIN", .85,.14,.12,.73)
        if set(p.result) != set(names): return None
    elif kind == "RV32IDecodeControlField":
        # Recognition lines enter from the left. Balanced OR levels advance
        # across the page, then optional default inversion and output packing.
        # Allocate only the levels that exist. A two-gate MEM_WRITE equation
        # must not inherit six mostly empty columns from the larger fields.
        p.slot("MATCH_BITS",.03,.09,.20,.84)
        levels = {}
        for name in names:
            match = re.search(r"_OR_(\d+)_\d+$", name)
            if match:
                levels.setdefault(int(match[1]), []).append(name)
        groups = [sorted(levels[level], key=natural) for level in sorted(levels)]
        tail = sorted((n for n in names if n.endswith(("_DEFAULT_NOT", "_CONSTANT"))), key=natural)
        if tail: groups.append(tail)
        step = .57 / max(1, len(groups))
        for column, group in enumerate(groups):
            p.grid(group, (.27 + column*step,.11,step*.90,.78), 1)
        p.slot("OUTPUT_BITS",.86,.12,.11,.76)
        if set(p.result) != set(names): return None
    elif kind == "RV32IFetchRequest":
        # The request address register, bypass mux and identity comparison
        # form the data path. Handshake equations follow in functional rows;
        # each state bit sits beside its own next-state logic at the bottom.
        for name, rect in {
            "REQUEST_ADDR": (.04,.07,.25,.22),
            "SELECT_ADDR": (.35,.07,.21,.22),
            "TAG_MATCH": (.65,.07,.30,.22),
        }.items(): p.slot(name, *rect)
        rows = [
            # None keeps the column of a part that no longer exists, so the
            # saved positions of its neighbours stay valid.
            ["READ_OPERATION", "NO_WRITE_DATA", "FULL_READ_MASK", None, "STATUS", "FAULT_0_0", "FAULT_1_0"],
            ["RUNNING", "IDLE", "ISSUE_IDLE", "START", "REQUEST_PRESENT", "UNSENT", "SENDING", "VALID", "ACCEPTED"],
            ["STALE", "DISCARD", "MATCHING", "CONSUME", "RESPONSE_ELIGIBLE", "RESPONSE", "USABLE", "DONE_RUNNING"],
            ["RESPONSE_PRESENT", "RESPONSE_RUNNING", "READY_RUNNING", "CAN_FINISH", "FINISHED", "UNFINISHED"],
            ["BUSY_NEXT", "KEEP_BUSY", "BUSY", "SENT_NEXT", "KEEP_SENT", "SENT", "DISCARD_NEXT", "KEEP_ABANDONED", "ABANDONED"],
        ]
        for row, group in enumerate(rows):
            for column, name in enumerate(group):
                if name in p.aspects and name not in p.result:
                    p.slot(name, .04+column*.92/len(group), .32+row*.13, .92/len(group), .115)
        if set(p.result) != set(names): return None
    elif kind == "RV32IBaselineDecodeStage":
        # Main decoder and register file stay readable at stage-focus scale.
        # Packing and terminal checks occupy smaller, separate slots.
        for name, rect in {
            "CONTROL": (.04,.09,.35,.43),
            "REGISTER_FILE": (.41,.45,.37,.43),
            "REGISTER_ADDRESSES": (.43,.13,.28,.20),
            "CONTROL_PACK": (.79,.09,.17,.43),
            "PACKET_STATUS": (.04,.63,.31,.26),
            "NO_EXECUTION_FAULT": (.10,.53,.19,.07),
            "PACKET_PASS": (.81,.61,.14,.29),
        }.items(): p.slot(name,*rect)
    elif kind == "RV32IBaselineExecuteStage":
        # Operand/control input at left; shared execution datapath in the
        # middle; next-PC/status selection at right; carried fields below.
        for name, rect in {
            "CONTROL_FIELDS": (.03,.10,.15,.47),
            "DATAPATH": (.22,.09,.39,.47),
            "NEXT_PC": (.65,.09,.30,.40),
            "PC_PLUS_4_ADD": (.24,.63,.23,.19),
            "FOUR": (.05,.76,.12,.08), "LOW": (.05,.87,.12,.05),
            "STORE_DATA": (.05,.62,.12,.08),
            "PACKET_STATUS": (.67,.65,.28,.26),
            "EXECUTION_STATUS_PACK": (.70,.51,.18,.10),
            "PACKET_PASS": (.22,.85,.37,.12),
        }.items(): p.slot(name,*rect)
    elif kind == "RV32IBaselineMemoryStage":
        # ACCESS is the datapath, not one more gate in the qualification
        # chain. Incoming checks stay left; outgoing fault gates stay right.
        for name, rect in {
            "CONTROL_FIELDS": (.04,.10,.14,.37),
            "PACKET_STATUS": (.04,.55,.26,.20),
            "ACCESS": (.33,.11,.45,.52),
            "MEMORY_STATUS_PACK": (.82,.12,.13,.22),
            "PACKET_PASS": (.36,.74,.60,.20),
        }.items(): p.slot(name,*rect)
        p.grid([n for n in names if n.startswith(("ACTIVE_", "TERMINAL_"))],
               (.82,.39,.14,.30), 1)
        p.rest((.035,.78,.28,.18), 3)
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
            "FORWARDED_OPERANDS": (.04,.09,.30,.35), "DATAPATH": (.39,.09,.32,.40),
            "NEXT_PC": (.76,.11,.20,.29), "PACKET_STATUS": (.62,.63,.33,.28),
            "PC_PLUS_4": (.30,.62,.23,.20), "FOUR": (.21,.64,.08,.08),
            "NO_CARRY": (.21,.82,.06,.05), "PACKET_PASS": (.28,.86,.26,.10),
            "CONTROL_FIELDS": (.04,.49,.30,.11), "REGISTER_FIELDS": (.40,.50,.26,.10),
            "EXECUTION_STATUS": (.78,.45,.17,.12),
        }.items(): p.slot(name,*rect)
        p.grid(["JUMP_DECISION", "CONTROL_TAKEN_0_0", "CONTROL_TAKEN_1_0"],
               (.04,.63,.15,.32), 1)
        if set(p.result) != set(names): return None
    elif kind == "RV32IForwardedOperands":
        p.slot("SELECT",.04,.10,.48,.70)
        p.grid(["OPERAND_A","OPERAND_B"],(.60,.10,.35,.72),1)
        p.slot("ZERO",.20,.86,.17,.08)
    elif kind == "RV32IPredictionPath":
        p.grid(["PREDECODE","PREDICTOR","IF_ID_PREDICTION","ID_EX_PREDICTION"],(.04,.09,.92,.37),4)
        p.slot("RECOVERY",.55,.57,.39,.35)
        p.slot("ACTUAL_NEXT_PC",.06,.55,.38,.17)
        p.slot("RECOVERY_PC_SELECT",.06,.77,.38,.17)
    elif kind == "RV32IBranchPredictor":
        # Stable common slots across policies; TABLE and NO_TABLE are exclusive.
        p.slot("TABLE",.04,.10,.46,.80)
        p.slot("COUNTER_BITS",.54,.22,.11,.40)
        p.slot("NO_TABLE",.12,.30,.23,.15)
        p.slot("STATIC_DIRECTION",.12,.58,.23,.15)
        p.grid(["BRANCH_DIRECTION","TAKEN","NEXT_PC_SELECT"],(.71,.11,.24,.78),1)
    elif kind == "RV32IPredictionEntry":
        p.slot("UPDATE",.04,.12,.38,.60);p.slot("NEXT_BITS",.44,.22,.12,.37)
        p.grid(["BIT_0","BIT_1"],(.61,.10,.20,.70),1)
        p.slot("VALUE",.85,.22,.11,.37);p.slot("ONE_BIT",.15,.86,.18,.08)
    elif kind == "RV32IPredictionTable":
        rows=sorted((n for n in names if re.fullmatch(r"ROW_\d+",n)),key=natural)
        for i,name in enumerate(rows):
            y=.10+i*.78/len(rows);h=.78/len(rows);suffix=name[4:]
            for prefix,x,width in [("UPDATE_ROW_",.03,.22),("WRITE_ROW_",.27,.10),("ROW_",.40,.27),("ROW_BITS_",.69,.10)]:
                p.slot(prefix+suffix,x,y,width,h)
        p.slot("LOOKUP_INDEX",.05,.89,.16,.08);p.slot("READ_COUNTER",.86,.89,.10,.08)
        p.grid(sorted((n for n in names if n.startswith("READ_") and n!="READ_COUNTER"),key=natural),(.82,.10,.14,.76),2)
        p.rest((.28,.89,.43,.08),4)
    elif kind == "RV32IMemoryStage":
        p.slot("ALIGNMENT", .06,.10,.35,.22)
        p.slot("FORWARD_VALUE_MUX", .60,.10,.33,.22)
        p.grid([n for n in names if "PACK" in n or "PASS" in n or "SPLIT" in n or "JOIN" in n],
               (.05,.37,.9,.22), 8)
        p.grid([n for n in names if n.startswith(("LOAD_", "STORE_", "DMEM_"))], (.05,.63,.9,.12), 10)
        p.rest((.05,.80,.9,.17), 8)
    elif kind == "RV32IWritebackStage":
        # All paths are combinational: unpack -> select / qualify -> outputs.
        p.grid(["CONTROL_UNPACK", "ADDRESSES_UNPACK", "FETCH_STATUS_UNPACK",
                "EXECUTION_STATUS_UNPACK", "MEMORY_STATUS_UNPACK"], (.04,.10,.23,.80), 1)
        p.slot("WRITEBACK_MUX", .38,.09,.53,.25)
        p.slot("RETIREMENT", .36,.40,.57,.39)
        p.slot("ZERO32", .30,.83,.13,.10)
        p.rest((.49,.84,.46,.10), 6)
    elif kind == "RV32IPipelineExecutionStatus":
        p.grid(["HALTED_STATE", "TRAPPED_STATE"], (.26,.10,.61,.24), 2)
        p.slot("HIGH", .05,.16,.12,.12)
        p.slot("TRAP_CAUSE_WORD", .24,.43,.64,.46)
    elif kind == "RV32IControlFlowUnit":
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
        # The balanced reduction flows left to right, one column per level.
        # Sort gate indices numerically to preserve bit order within each level.
        p.grid(sorted((n for n in names if n.startswith("NOT_BIT_")), key=natural),
               (.19,.10,.31,.83), 4)
        levels = {}
        for name in names:
            match = re.fullmatch(r"MATCH_TREE_(\d+)_(\d+)", name)
            if match:
                levels.setdefault(int(match[1]), []).append(name)
        for column, level in enumerate(sorted(levels)):
            width = .40 / len(levels)
            p.grid(sorted(levels[level], key=natural),
                   (.55 + column * width, .10, width, .83), 1)
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
        p.grid(["BYTE_FILL","HALF_FILL"],(.20,.08,.61,.09),2)
        for bit in range(8):
            p.grid([f"BYTE_{bit}",f"HALF_{bit}",f"WORD_{bit}",f"RESULT_{bit}_0_0",f"RESULT_{bit}_1_0"],
                   (.15,.19+bit*.09,.70,.075),5)
    elif kind == "RV32IByteWriteMask":
        p.slot("OFFSET",.03,.1,.12,.6)
        p.grid(["NOT_A0","NOT_A1"],(.03,.76,.12,.20),1)
        p.slot("MASK",.87,.10,.10,.79)
        p.grid(["LOW_HALF","HIGH_HALF"],(.22,.08,.59,.09),2)
        for byte in range(4):
            p.grid([f"SELECT_LANE_{byte}",f"BYTE_MASK_{byte}",f"SUBWORD_MASK_{byte}",f"MASK_{byte}"],
                   (.20,.21+byte*.18,.62,.13),4)
    elif kind == "RV32IRequestTagMatch":
        p.slot("CURRENT_TAG",.02,.09,.10,.81)
        p.slot("SAVED_TAG",.14,.09,.10,.81)
        p.grid([f"TAG_DIFFERENT_{bit}" for bit in range(32)],(.29,.09,.20,.81),2)
        for level in range(5):
            p.grid([f"TAG_CHANGED_{level}_{bit}" for bit in range(16 >> level)],
                   (.53+level*.09,.09,.075,.81),1)
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
        p.grid(["HALTED_STATE","TRAPPED_STATE"],(.10,.84,.33,.13),2)
        p.slot("TRAP_CAUSE_WORD",.55,.81,.35,.15)
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
