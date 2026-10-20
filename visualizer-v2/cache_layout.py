"""Floorplan for a cache's controller and per-way memory endpoints."""

import re


def defaults(kind):
    if not re.fullmatch(r"Cache<(8|16|32)>(?:\[layout-[0-9a-f]{16}\])?", kind):
        return {}
    return {"aspect_ratio": 1.76, "min_aspect_ratio": 0.1,
            "aspect_ratio_floor": 0.08, "title_bar_ratio": 0.04,
            "pin_size_ratio": 0.006, "boundary_area_ratio": 0.035}


def placements(kind, aspects, parent_aspect):
    if not re.fullmatch(r"Cache<(8|16|32)>", kind):
        return None
    if "CONTROLLER" not in aspects or parent_aspect <= 0:
        return None
    way_ids = sorted(int(m.group(1)) for name in aspects
                     if (m := re.fullmatch(r"TAG_STORE_(\d+)", name)))
    if way_ids not in ([0], [0, 1], [0, 1, 2, 3], list(range(8))):
        return None
    if any(f"DATA_STORE_{way}" not in aspects for way in way_ids):
        return None
    line_store = "LINE_STORE" in aspects

    result = {}

    def box(name, x, y, width, height, fill=0.82):
        """Fit a child's physical aspect into a normalized parent rectangle."""
        child_width = min(width * fill, height * parent_aspect * fill / max(aspects[name], 0.01))
        child_height = child_width * aspects[name] / parent_aspect
        result[name] = {"rel_pos": [x + (width - child_width) / 2,
                                    y + (height - child_height) / 2],
                        "rel_width": child_width}

    box("CONTROLLER", 0.045, 0.13, 0.34, 0.71, 0.91)
    # One row per way (tag store, data store); the line-state store, if any,
    # gets a row of its own under the tag stores.
    rows = len(way_ids) + (1 if line_store else 0)
    if rows <= 2 and not line_store:
        centers = [0.35] if rows == 1 else [0.245 + row * 0.29 for row in range(2)]
        height = 0.22
    else:
        pitch = 0.66 / rows
        centers = [0.06 + pitch * (row + 0.5) for row in range(rows)]
        height = min(0.22, pitch * 0.92)
    for row, way in enumerate(way_ids):
        for name, x in ((f"TAG_STORE_{way}", 0.425),
                        (f"DATA_STORE_{way}", 0.695)):
            box(name, x, centers[row] - height / 2, 0.235, height, 0.87)
    if line_store:
        box("LINE_STORE", 0.425, centers[-1] - height / 2, 0.235, height, 0.87)

    helpers = set(aspects) - set(result)
    if any(not name.startswith(("TAG_", "DATA_", "META_")) for name in helpers):
        return None
    helper_rows = ((("TAG_", 0.745), ("DATA_", 0.825), ("META_", 0.905), 0.07) if line_store
                   else (("TAG_", 0.755), ("DATA_", 0.865), 0.085))
    *helper_rows, row_height = helper_rows
    for prefix, y in helper_rows:
        names = sorted(name for name in helpers if name.startswith(prefix))
        if not names:
            continue
        pitch = 0.54 / len(names)
        for index, name in enumerate(names):
            box(name, 0.425 + index * pitch, y, pitch, row_height, 0.72)
    return result if set(result) == set(aspects) else None
