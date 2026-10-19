"""Floorplan for the two-port memory arbiter: the request path in the upper
half, the response path in the lower half. Geometry only."""

import re


def placements(kind, aspects, parent_aspect):
    if not re.fullmatch(r"TwoPortMemoryArbiter<(8|16|32)>", kind) or parent_aspect <= 0:
        return None
    expected = {"RESET", "LOWER_CLOCK", "LOWER_RESET", "GRANT", "STATE", "RESPONSE_ROUTE",
                "I_RESPONSE", "D_RESPONSE", "SELECT_REQ_OP", "SELECT_REQ_ADDR",
                "SELECT_REQ_WRITE_DATA", "SELECT_REQ_WRITE_MASK"}
    if set(aspects) != expected:
        return None
    result = {}

    def box(name, x, y, width, height, fill=0.8):
        """Fit a child's aspect into a normalized cell (y in units of the parent height)."""
        cell_height = height * parent_aspect
        child_width = min(width * fill, cell_height * fill / max(aspects[name], 0.01))
        child_height = child_width * aspects[name]
        result[name] = {"rel_pos": [x + (width - child_width) / 2,
                                    (y * parent_aspect + (cell_height - child_height) / 2) / parent_aspect],
                        "rel_width": child_width}

    # Clock and reset helpers along the left edge.
    for row, name in enumerate(("RESET", "LOWER_CLOCK", "LOWER_RESET")):
        box(name, 0.04, 0.42 + row * 0.06, 0.16, 0.06, 0.75)
    # Request path: grant and state, then one multiplexer per request field.
    box("GRANT", 0.24, 0.06, 0.30, 0.14)
    box("STATE", 0.24, 0.22, 0.30, 0.16)
    for row, field in enumerate(("OP", "ADDR", "WRITE_DATA", "WRITE_MASK")):
        box(f"SELECT_REQ_{field}", 0.62, 0.05 + row * 0.095, 0.32, 0.095, 0.78)
    # Response path: routing, then one response buffer per port.
    box("RESPONSE_ROUTE", 0.24, 0.62, 0.30, 0.16)
    box("I_RESPONSE", 0.62, 0.58, 0.32, 0.15)
    box("D_RESPONSE", 0.62, 0.76, 0.32, 0.15)
    return result
