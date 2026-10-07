"""Matrix and transaction-endpoint floorplans; preserve FF/SRAM device defaults.

Cell c, row r and column c are ordered numerically. Column centers align with
the same cell in every row, following the SRAM matrix layout convention.
"""
import re

FAMILY = r"(?:DRAM|ROM|NORFlash)"


def peripheral_aspect(width, slots, mux):
    # Slot rows, then one OR-reduction row per node for the read mux.
    # Controls are in a separate left gutter, so numeric bit lanes remain clear.
    lanes = 2 * slots - 1 if mux else slots
    return max(0.30, 0.12 + lanes * (0.64 * 0.65 / width + 0.018))


def row_aspect(columns):
    return max(0.12, 0.075 + 1.1 * 0.84 * 0.74 / columns)


def payload_aspect(width):
    # Eight numeric bit lanes per byte row. A single-bit mask is not taller
    # than the address register, and a 32-bit field is not a microscopic strip.
    return 0.18 + 0.095 * ((width + 7) // 8)


def transaction_aspect(width):
    return 1.0 + 0.42 * payload_aspect(width)


def defaults(kind):
    row = re.fullmatch(FAMILY + r"Row<(\d+)>", kind)
    bank = re.fullmatch(FAMILY + r"StorageBank<(\d+),(\d+),(\d+)>", kind)
    if kind in {"RV32IMemoryRequest","RV32IFetchRequest"}:
        aspect, pins, title = (1.5,13,.14) if kind=="RV32IMemoryRequest" else (1.15,10,.14)
    elif kind in {"RV32IMemoryLanes","RV32IMemoryLanesDataOnly"}:
        aspect, pins, title = 1.5, 5, .05
    elif kind.startswith("MemoryEndpoint<"):
        aspect, pins, title = 0.8, 9, 0.04
    elif kind.startswith("MemoryController<"):
        width = int(re.findall(r"\d+", kind)[-1])
        aspect, pins, title = 0.22 + 0.57 * transaction_aspect(width), 20, 0.04
    elif kind.startswith(("MemoryTransactionPort<", "MemoryResponseBuffer<")):
        aspect, pins, title = transaction_aspect(int(re.findall(r"\d+", kind)[0])), 16, 0.04
    elif kind.startswith("MemoryFFRequestCheck<"):
        aspect, pins, title = 1.1, 8, .04
    elif kind.startswith("MemoryPayloadMux<"):
        aspect, pins, title = .8, 4, .04
    elif kind.startswith("Register<"):
        aspect, pins, title = payload_aspect(int(re.findall(r"\d+", kind)[0])), 5, 0.035
    elif kind == "ClockedStateBit":
        aspect,pins,title=.6,4,.08
    elif kind.startswith(("MemoryNativeSequencer<", "MemoryContainer<")):
        aspect, pins, title = 1.4, 18, 0.06
    elif row:
        cols = int(row[1])
        aspect, pins, title = row_aspect(cols), 2 * cols + 2, 0.025
    elif bank:
        width, n, k = map(int, bank.groups())
        aspect = 0.20 + n // k * (0.86 * row_aspect(width * k) + 0.035)
        pins, title = 2 * width * k + 2 * (n // k) + 2, 0.04
    elif re.fullmatch(FAMILY + r"<(8|16|32)>", kind):
        aspect, pins, title = 1.05, 8, 0.05
    elif kind in {"DRAMCell", "NORFlashCell"}:
        aspect, pins, title = 1.1, 4, 0.10
    elif re.fullmatch(r"Matrix(ColumnMux|WriteDistributor)<\d+,\d+>", kind):
        width, k = map(int, re.findall(r"\d+", kind))
        aspect, pins, title = peripheral_aspect(width, k, "ColumnMux" in kind), 2 * width * k + 4, 0.025
    else:
        return {}
    return {"aspect_ratio": round(aspect, 6), "min_aspect_ratio": min(aspect, 0.1),
            "aspect_ratio_floor": 0.08, "title_bar_ratio": title,
            "pin_size_ratio": min(0.045, (aspect - title) * 0.7 / (1.15 * (pins + 1))),
            "boundary_area_ratio": 0.035}


def placements(kind, aspects, parent_aspect):
    row = re.fullmatch(FAMILY + r"Row<(\d+)>", kind)
    bank = re.fullmatch(FAMILY + r"StorageBank<(\d+),(\d+),(\d+)>", kind)
    peripheral = re.fullmatch(r"Matrix(ColumnMux|WriteDistributor)<(\d+),(\d+)>", kind)
    result = {}

    def place(name, x, y, width):
        result[name] = {"rel_pos": [x, y / parent_aspect], "rel_width": width}

    if kind in {"RV32IMemoryRequest", "RV32IFetchRequest"}:
        # The tag comparison is now one expandable child, not 32 exposed XORs.
        # Fetch stores only the address; the generic requester retains its five
        # payload fields. Keep both variants' state and handshake path below.
        payloads = [n for n in ("REQUEST_TAG", "REQUEST_ADDR", "REQUEST_OP",
                                "REQUEST_WRITE_DATA", "REQUEST_WRITE_MASK") if n in aspects]
        if "REQUEST_ADDR" not in payloads or "TAG_MATCH" not in aspects:
            return None
        def box(name, x, y, width, height, fill=.75):
            child_width = min(width * fill, height * .75 / max(aspects[name], .01))
            place(name, x + (width - child_width) / 2,
                  y + (height - child_width * aspects[name]) / 2, child_width)
        for i, name in enumerate(payloads):
            box(name, .05 + (i % 2) * .25, .19 + (i // 2) * .18, .22, .16)
        box("TAG_MATCH", .57, .19, .30, .32)
        for i, name in enumerate(("RESPONSE_DATA", "STATUS")):
            if name in aspects:
                box(name, .59 + i * .18, .53, .17, .16)
        control_y = .77
        controls = [n for n in ("BUSY", "SENT", "ABANDONED") if n in aspects]
        controls += sorted((n for n in aspects if n not in result and n not in controls),
                           key=lambda n: [int(s) if s.isdigit() else s for s in re.split(r"(\d+)", n)])
        columns = 7
        rows = (len(controls) + columns - 1) // columns
        cell_height = (parent_aspect - control_y - .05) / max(rows, 1)
        if cell_height <= 0:
            return None
        for i, name in enumerate(controls):
            box(name, .045 + (i % columns) * .91 / columns,
                control_y + (i // columns) * cell_height, .91 / columns, cell_height, .55)
    elif kind.startswith("MemoryEndpoint<"):
        if set(aspects) != {"CONTROLLER", "MEMORY"}: return None
        place("CONTROLLER", 0.05, 0.12, 0.40)
        # Leave room for the device's right pin stub as well as the box.
        place("MEMORY", 0.52, 0.12, 0.42)
    elif kind.startswith(("MemoryController<ff,", "MemoryTransactionPort<", "MemoryResponseBuffer<")):
        # A left-to-right data path above a separate handshake/control rail.
        # Box fitting respects different payload widths and fidelity aspects.
        if kind.startswith("MemoryController<ff,"):
            major = ["REQUEST_CHECK", "READ_RESULT", "RESPONSE_STATUS"]
            if "RESPONSE" in aspects:
                major.append("RESPONSE")
        elif kind.startswith("MemoryTransactionPort<"):
            major = ["REQUEST_OP", "REQUEST_ADDR", "REQUEST_DATA", "REQUEST_MASK", "RESPONSE"]
        else:
            major = ["HELD_DATA", "SELECT_DATA", "HELD_STATUS", "SELECT_STATUS"]
        if not set(major) <= set(aspects): return None
        def box(name, x, y, width, height):
            w = min(width*.82, height*.78 / max(aspects[name], .01))
            place(name, x+(width-w)/2, y+(height-w*aspects[name])/2, w)
        # Large response/validation groups stay readable; tiny rewires do not
        # acquire the area of an address/payload register.
        top = parent_aspect * .48
        for i, name in enumerate(major):
            box(name, .04+i*.92/len(major), .07, .92/len(major), top-.07)
        preferred = ["PENDING", "OCCUPIED", "IDLE", "EMPTY", "SPACE", "RUNNING",
                     "REQUEST_SLOT", "READY", "INPUT_READY", "ACCEPT", "COMPLETION",
                     "RESPONSE_TAKEN", "FINISH", "CAPTURE", "HOLD", "NEXT_PENDING", "NEXT_FULL"]
        rest = [n for n in preferred if n in aspects and n not in result]
        rest += sorted(n for n in aspects if n not in result and n not in rest)
        columns = 6
        rows = (len(rest)+columns-1)//columns
        height = (parent_aspect-top-.04)/max(rows,1)
        for i, name in enumerate(rest):
            box(name, .04+(i%columns)*.92/columns, top+(i//columns)*height, .92/columns, height)
    elif kind.startswith("MemoryController<"):
        if set(aspects) not in ({"TRANSACTION", "DEVICE_CONTROL"}, {"TRANSACTION", "DEVICE_CONTROL", "BUS_CLOCK"}): return None
        place("TRANSACTION", 0.05, 0.10, 0.57)
        control_y = 0.10 + max(0, (0.57 * aspects["TRANSACTION"] - 0.25 * aspects["DEVICE_CONTROL"]) / 2)
        place("DEVICE_CONTROL", 0.70, control_y, 0.25)
        if "BUS_CLOCK" in aspects:
            place("BUS_CLOCK", 0.76, control_y + 0.25 * aspects["DEVICE_CONTROL"] + 0.06, 0.10)
    elif kind.startswith("Register<"):
        bits = sorted((n for n in aspects if n.startswith("BIT_")), key=lambda n: int(n[4:]))
        if not bits: return None
        columns, rows = min(8, len(bits)), (len(bits) + 7) // 8
        width = min(0.24, 0.66 * 0.82 / columns)
        pitch_y = (parent_aspect - 0.105) / rows
        for i, name in enumerate(bits):
            place(name, 0.18 + (i % columns + 0.5) * 0.66 / columns - width / 2,
                  0.065 + (i // columns + 0.5) * pitch_y - width * aspects[name] / 2, width)
        for name, x in (("INPUT_BITS", 0.055), ("OUTPUT_BITS", 0.90)):
            if name in aspects:
                adapter_width = min(0.045, (parent_aspect - 0.105) / aspects[name])
                place(name, x, 0.065 + (parent_aspect - 0.105 - adapter_width * aspects[name]) / 2, adapter_width)
    elif row:
        cols = int(row[1])
        if set(aspects) != {f"CELL_{c}" for c in range(cols)}:
            return None
        pitch, width = 0.84 / cols, 0.84 * 0.74 / cols
        for c in range(cols):
            name = f"CELL_{c}"
            height = width * aspects[name]
            place(name, 0.08 + (c + 0.5) * pitch - width / 2,
                  0.025 + (parent_aspect - 0.025 - height) / 2, width)
    elif bank:
        w, n, k = map(int, bank.groups())
        cols, rows, row_width = w * k, n // k, 0.86
        if set(aspects) != {f"ROW_{r}" for r in range(rows)} | {f"COLUMN_{c}" for c in range(cols)}:
            return None
        y = 0.075
        for r in range(rows):
            name = f"ROW_{r}"
            place(name, 0.07, y, row_width)
            y += row_width * aspects[name] + 0.035
        pitch, width = row_width * 0.84 / cols, row_width * 0.84 * 0.74 / cols
        for c in range(cols):
            place(f"COLUMN_{c}", 0.07 + row_width * 0.08 + (c + 0.5) * pitch - width / 2, y, width)
    elif peripheral:
        mux, width, slots = peripheral[1] == "ColumnMux", int(peripheral[2]), int(peripheral[3])
        controls = ["SELECT", "ENABLED", "DATA_BITS"] if mux else ["SELECT", "DATA_BITS"]
        if not mux and width > 8:
            controls.append("MASK_BITS")
        if not set(controls) <= set(aspects):
            return None
        y = 0.065
        for name in controls:
            place(name, 0.045, y, 0.10)
            y += 0.10 * aspects[name] + 0.025
        lanes = width if mux else width // 8
        pitch, gate_width = 0.64 / lanes, 0.64 * 0.65 / lanes
        gates = set(aspects) - set(controls)
        if not gates:
            return None
        # Leave vertical clearance for child pin markers as well as boxes.
        lane_pitch = gate_width * max(aspects[name] for name in gates) + 0.06
        for bit in range(lanes):
            x = 0.25 + (bit + 0.5) * pitch - gate_width / 2
            for slot in range(slots):
                name = f"SELECT_{slot}_{bit}" if mux else f"WRITE_SLOT_{slot}_BYTE_{bit}"
                if name not in aspects:
                    return None
                place(name, x, 0.065 + slot * lane_pitch, gate_width)
            if mux:
                reductions = sorted((name for name in gates if name.startswith(f"BIT_{bit}_")),
                                    key=lambda name: tuple(map(int, name.split("_")[2:])))
                for index, name in enumerate(reductions):
                    place(name, x, 0.065 + (slots + index) * lane_pitch, gate_width)
        if set(result) != set(aspects):
            return None
    elif re.fullmatch(FAMILY + r"<(8|16|32)>", kind):
        control = "ACCESS_CONTROL" if kind.startswith("ROM<") else "COMMAND_CONTROL"
        required = {control, "ROW_DECODE", "STORAGE", "COLUMN_SELECT", "OUTPUT"}
        if control == "COMMAND_CONTROL":
            required.add("WRITE_DRIVERS")
        if set(aspects) != required:
            return None
        place(control, 0.065, 0.13, 0.13)
        place("ROW_DECODE", 0.07, max(0.39, 0.17 + 0.13 * aspects[control]), 0.12)
        place("STORAGE", 0.28, 0.13, 0.64)
        y = 0.13 + 0.64 * aspects["STORAGE"] + 0.06
        place("COLUMN_SELECT", 0.28, y, 0.64)
        y += 0.64 * aspects["COLUMN_SELECT"] + 0.05
        if "WRITE_DRIVERS" in aspects:
            place("WRITE_DRIVERS", 0.28, y, 0.42)
        place("OUTPUT", 0.80, y, 0.10)
    else:
        return None
    # Capacity and peripheral pin counts can grow independently of a device's
    # shared type style. Fit a tall matrix uniformly instead of placing later
    # rows/sensors outside its parent. Uniform scaling preserves column centers.
    bottom = max(p["rel_pos"][1] * parent_aspect + p["rel_width"] * aspects[name]
                 for name, p in result.items())
    margin = min(0.05, parent_aspect * 0.1)
    if bottom > parent_aspect - margin:
        scale = (parent_aspect - 2 * margin) / (bottom - margin)
        for p in result.values():
            x, y = p["rel_pos"]
            p["rel_pos"] = [0.5 + (x - 0.5) * scale,
                            (margin + (y * parent_aspect - margin) * scale) / parent_aspect]
            p["rel_width"] *= scale
    return result
