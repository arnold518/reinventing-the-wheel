"""Geometry-only SRAM floorplans. These do not change pins or connectivity."""
import re


def row_aspect(columns):
    return max(0.1, 0.07 + 0.725 * 0.84 * 0.74 / columns)


def defaults(component_type):
    row = re.fullmatch(r"SRAMRow<(\d+)>", component_type)
    bank = re.fullmatch(r"SRAMStorageBank<(\d+),(\d+),(\d+)>", component_type)
    precharge = re.fullmatch(r"SRAMPrecharge<(\d+)>", component_type)
    columns = re.fullmatch(r"SRAMColumnSelector<(\d+),(\d+)>", component_type)
    if row:
        count = int(row[1])
        aspect, pins, title = row_aspect(count), 3 * count + 1, 0.025
    elif bank:
        width, depth, k = map(int, bank.groups())
        count, rows = width * k, depth // k
        aspect = 0.18 + rows * (0.86 * row_aspect(count) + 0.035)
        pins, title = rows + 4 * count, 0.04
    elif precharge and int(precharge[1]) > 2:
        aspect, pins, title = 0.14, int(precharge[1]), 0.025
    elif columns:
        width, k = map(int, columns.groups())
        aspect, pins, title = 0.18, 3 * width * (k + 1) + 1, 0.025
    elif re.fullmatch(r"SRAM<(8|16|32)>", component_type):
        aspect, pins, title = 1.02, 6, 0.05
    else:
        return {}
    return {
        "aspect_ratio": round(aspect, 6),
        "min_aspect_ratio": round(min(aspect, 0.1), 6),
        "aspect_ratio_floor": 0.08,
        "title_bar_ratio": title,
        "pin_size_ratio": min(0.055, (aspect - title) * 0.7 / (1.15 * (pins + 1))),
        "boundary_area_ratio": 0.035,
    }


def placements(component_type, child_aspects, parent_aspect):
    """Return name-keyed placements, or None for the normal graph layout."""
    row = re.fullmatch(r"SRAMRow<(\d+)>", component_type)
    bank = re.fullmatch(r"SRAMStorageBank<(\d+),(\d+),(\d+)>", component_type)
    device = re.fullmatch(r"SRAM<(8|16|32)>", component_type)
    result = {}

    def place(name, x, y, width):
        result[name] = {"rel_pos": [x, y / parent_aspect], "rel_width": width}

    if row:
        count = int(row[1])
        pitch, width = 0.84 / count, 0.84 / count * 0.74
        for c in range(count):
            name = f"CELL_{c}"
            height = width * child_aspects[name]
            place(name, 0.08 + (c + 0.5) * pitch - width / 2,
                  0.025 + (parent_aspect - 0.025 - height) / 2, width)
    elif bank:
        w, n, k = map(int, bank.groups())
        count, rows = w * k, n // k
        row_width = 0.86
        y = 0.075
        for r in range(rows):
            name = f"ROW_{r}"
            place(name, 0.07, y, row_width)
            y += row_width * child_aspects[name] + 0.035
        pitch, width = row_width * 0.84 / count, row_width * 0.84 / count * 0.74
        for c in range(count):
            # Exactly the same global x center as CELL_c in every row.
            place(f"BITLINE_{c}", 0.07 + row_width * 0.08 + (c + 0.5) * pitch - width / 2, y, width)
    elif device:
        width = int(device[1])
        place("ACCESS_CONTROL", 0.06, 0.115, 0.13)
        place("ROW_DECODE", 0.08, 0.36, 0.12)
        place("PRECHARGE", 0.27, 0.10, 0.63)
        place("STORAGE", 0.27, 0.235, 0.63)
        bottom = 0.235 + 0.63 * child_aspects["STORAGE"]
        selector_y = max(0.58, bottom + 0.05)
        place("COLUMN_SELECT", 0.27, selector_y, 0.63)
        driver_y = selector_y + 0.63 * child_aspects["COLUMN_SELECT"] + 0.055
        place("WRITE_DATA_BITS", 0.08, driver_y, 0.045)
        if "WRITE_MASK_BITS" in child_aspects:
            place("WRITE_MASK_BITS", 0.16, driver_y, 0.04)
        for bit in range(width):
            lane_width = 0.52 / width * 0.74
            x = 0.30 + (bit + 0.5) * 0.52 / width - lane_width / 2
            place(f"WRITE_DRIVER_{bit}", x, driver_y, lane_width)
            place(f"SENSE_{bit}", x, driver_y + 0.085, lane_width)
        place("READ_DATA_BITS", 0.85, driver_y + 0.085, 0.025)
        place("OUTPUT", 0.91, driver_y + 0.085, 0.045)
    else:
        return None
    # Refuse partial geometry if a later circuit revision adds/removes blocks.
    if result.keys() != child_aspects.keys():
        return None
    # A taller runtime bank (e.g. four rows rather than two) moves the column
    # selector and sense/write lanes down. Preserve their relative geometry,
    # but fit the whole floorplan inside the device instead of spilling out.
    bottom = max(p["rel_pos"][1] * parent_aspect + p["rel_width"] * child_aspects[n]
                 for n, p in result.items())
    if bottom > parent_aspect - 0.02:
        scale = (parent_aspect - 0.04) / (bottom - 0.02)
        for p in result.values():
            x, y = p["rel_pos"]
            p["rel_pos"] = [0.5 + (x - 0.5) * scale,
                            (0.02 + (y * parent_aspect - 0.02) * scale) / parent_aspect]
            p["rel_width"] *= scale
    return result
