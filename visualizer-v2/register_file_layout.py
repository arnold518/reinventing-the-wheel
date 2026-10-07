"""FF register-file floorplans: decode -> storage -> two parallel read muxes."""
from rv32i_layout import Floorplan

REVISION = "register-file-ff-v1"
KINDS = {"RegisterFile32x32": 1.07, "RegisterFileStorageBank": 1.2,
         "FFStorageBank<32,31>": 1.2, "FFWord<32>": .8,
         "MemoryStorageBank<FFArray,32,31>": 1.2, "MemoryWord<FFArray,32>": .8}


def defaults(kind):
    if kind not in KINDS:
        return {}
    return dict(aspect_ratio=KINDS[kind], min_aspect_ratio=.08, aspect_ratio_floor=.08,
                title_bar_ratio=.10, pin_size_ratio=.018, boundary_area_ratio=.025)


def placements(kind, aspects, parent_aspect):
    if kind not in KINDS or not aspects:
        return None
    p = Floorplan(aspects, parent_aspect, .10)
    if kind == "RegisterFile32x32":
        if "STORAGE" not in aspects:
            return None
        for name, rect in {
            "RD_DECODER": (.03,.21,.18,.70), "STORAGE": (.24,.21,.39,.70),
            "RS1_MUX": (.66,.21,.14,.70), "RS2_MUX": (.83,.21,.14,.70),
            "X0_ZERO": (.70,.05,.18,.13),
        }.items():
            p.slot(name, *rect)
    elif "StorageBank" in kind:
        p.grid([f"WORD_{i}" for i in range(31)], (.05,.05,.90,.91), 4)
    else:
        p.slot("DATA", .06,.05,.30,.15)
        p.slot("MASK", .42,.05,.25,.15)
        p.slot("ZERO", .74,.05,.15,.15)
        for byte in range(4):
            p.slot(f"BYTE_{byte}", .03,.23+byte*.18,.14,.15)
            p.grid([f"CELL_{byte*8+bit}" for bit in range(8)], (.20,.23+byte*.18,.63,.15), 8)
        p.slot("OUTPUT", .87,.22,.10,.72)
    return p.result
