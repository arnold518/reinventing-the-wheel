"""Semantic default colors for visualizer component families.

The layout file may override any color.  These defaults keep newly added and
previously uncolored component types meaningful without making hierarchy depth
change a component's semantic hue.
"""

from __future__ import annotations


ALPHA = 190


def _rgba(red: int, green: int, blue: int) -> tuple[int, int, int, int]:
    return red, green, blue, ALPHA


DEFAULT_COMPONENT_COLOR = _rgba(61, 90, 128)


TYPE_COLORS: dict[str, tuple[int, int, int, int]] = {
    # Primitive logic. Negated variants remain in the same hue family.
    "ANDGate": _rgba(54, 162, 235),
    "AND8": _rgba(37, 99, 235),
    "NANDGate": _rgba(68, 132, 190),
    "NAND8": _rgba(49, 105, 160),
    "ORGate": _rgba(72, 187, 120),
    "OR8": _rgba(47, 145, 91),
    "NORGate": _rgba(55, 150, 105),
    "NOR8": _rgba(39, 114, 78),
    "XORGate": _rgba(245, 158, 11),
    "XOR8": _rgba(217, 119, 6),
    "NOTGate": _rgba(239, 83, 80),
    "NOT8": _rgba(190, 52, 50),

    # Selection and routing. Larger mux wrappers stay orange but become more
    # subdued so their smaller orange children remain distinct.
    "Mux2to1": _rgba(190, 105, 45),
    "Mux4to1": _rgba(165, 80, 35),
    "Mux8to1": _rgba(140, 65, 30),
    "Mux16to1": _rgba(115, 55, 30),
    "Mux32to1": _rgba(95, 48, 28),
    "Mux2to1_4bit": _rgba(175, 90, 35),
    "Mux2to1_8bit": _rgba(175, 90, 35),
    "Mux2to1_32bit": _rgba(175, 90, 35),
    "Mux4to1_8bit": _rgba(145, 65, 30),
    "Mux4to1_32bit": _rgba(145, 65, 30),
    "Mux8to1_8bit": _rgba(120, 55, 28),
    "Mux8to1_32bit": _rgba(120, 55, 28),
    "Mux16to1_8bit": _rgba(95, 45, 25),
    "Mux32to1_32bit": _rgba(80, 42, 22),
    "Rewire": _rgba(100, 116, 139),

    # Arithmetic. A larger structural wrapper is a darker member of the same
    # violet family, so parent and child remain related but not identical.
    "HalfAdder": _rgba(145, 95, 180),
    "FullAdder": _rgba(100, 70, 140),
    "Adder8": _rgba(80, 60, 120),
    "Adder32": _rgba(90, 60, 140),
    "Incrementer8": _rgba(100, 65, 140),
    "TwosComplement8": _rgba(135, 75, 150),
    "Subtractor8": _rgba(125, 55, 145),
    "SubtractorWithBorrow8": _rgba(110, 45, 130),
    "Decrementer8": _rgba(95, 40, 115),
    "AddSub32": _rgba(125, 58, 135),

    # Detection, comparison, shifting, and whole datapaths.
    "Decoder2to4": _rgba(20, 184, 166),
    "Decoder5to32": _rgba(20, 184, 166),
    "EqualityChecker8": _rgba(20, 184, 166),
    "Comparator8": _rgba(13, 148, 136),
    "SignedComparator8": _rgba(15, 118, 110),
    "Comparator32": _rgba(42, 95, 90),
    "ZeroDetect8": _rgba(20, 184, 166),
    "ZeroDetect32": _rgba(40, 105, 95),
    "ShiftLeftLogical8": _rgba(56, 189, 248),
    "ShiftRightLogical8": _rgba(56, 189, 248),
    "ShiftRightArithmetic8": _rgba(56, 189, 248),
    "Shifter32": _rgba(38, 105, 135),
    "Logic32": _rgba(45, 110, 75),
    "ALU8": _rgba(55, 65, 115),
    "ALU32": _rgba(68, 70, 125),

    # State and storage move from rose at the bit/cell level toward indigo at
    # word, register-file, and memory-array scale.
    "SRLatch": _rgba(145, 45, 75),
    "GatedDLatch": _rgba(165, 50, 90),
    "DFlipFlop": _rgba(236, 72, 153),
    "MemoryBit": _rgba(180, 55, 85),
    "Register32": _rgba(105, 70, 145),
    "RegisterFile4x32": _rgba(82, 58, 120),
    "RegisterFile32x32": _rgba(76, 58, 125),
    "Memory4x32": _rgba(40, 65, 110),
    "Memory32x32": _rgba(40, 65, 110),
    "Memory64Kx32": _rgba(40, 65, 110),

    # System and RV32I macro blocks. Pipeline stages receive stable functional
    # colors; the four inter-stage registers form one blue-to-violet family.
    "ClockGenerator": _rgba(34, 211, 238),
    "RV32IProgramRoot": _rgba(51, 65, 85),
    "RV32ISingleCycleCore": _rgba(44, 78, 145),
    "RV32IFiveStageCore": _rgba(64, 58, 125),
    "RV32IFetchStage": _rgba(35, 120, 155),
    "RV32IDecodeStage": _rgba(35, 115, 105),
    "RV32IExecuteStage": _rgba(105, 55, 140),
    "RV32IMemoryStage": _rgba(150, 90, 25),
    "RV32IWritebackStage": _rgba(145, 50, 85),
    "RV32IIFIDPipelineRegister": _rgba(59, 130, 246),
    "RV32IIDEXPipelineRegister": _rgba(99, 102, 241),
    "RV32IEXMEMPipelineRegister": _rgba(139, 92, 246),
    "RV32IMEMWBPipelineRegister": _rgba(168, 85, 247),
    "RV32IControlFlowUnit": _rgba(2, 132, 199),
    "RV32IDecodeControlUnit": _rgba(13, 148, 136),
    "RV32IExecutionControlStatusUnit": _rgba(202, 138, 4),
    "RV32IForwardingUnit": _rgba(6, 182, 212),
    "RV32IHazardDetectionUnit": _rgba(234, 179, 8),
    "RV32IPipelineControlFlowUnit": _rgba(14, 116, 144),
    "RV32IMemoryAlignmentUnit": _rgba(245, 158, 11),
    "RV32IPipelineRetirementUnit": _rgba(225, 29, 72),
    "RV32IPipelineCoordinator": _rgba(100, 116, 139),
}


def color_for_layout_type(layout_type: str) -> list[int]:
    """Return a fresh RGBA list for one component layout type."""

    if layout_type.startswith("BitSplitter<"):
        return [124, 58, 237, ALPHA]
    if layout_type.startswith("BitJoiner<"):
        return [147, 51, 234, ALPHA]
    if layout_type.startswith("ConstantValue<"):
        return [100, 116, 139, ALPHA]
    if layout_type.startswith("RV32IBitPatternMatcher<"):
        return [20, 184, 166, ALPHA]
    return list(TYPE_COLORS.get(layout_type, DEFAULT_COMPONENT_COLOR))
