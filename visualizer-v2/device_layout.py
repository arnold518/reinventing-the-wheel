"""Readable device architecture and the computer-system floorplans: the
machine (core, buses, memory, devices), a bus (routers over arbiters) and the
memory subsystem (caches over the arbiter over the memory)."""

PUBLIC_PREFIXES = ("ClockedCounter<", "SynchronousFifo<", "DeviceRegisterPort<",
                   "BusRouter<", "BusFetchRouter<", "BusBridge<", "RegisteredBridge<")
PRIVATE_PREFIXES = ("ClockTick", "ClockDivider", "CounterIncrement<", "Fifo",
                    "DeviceRegister", "DeviceAddress", "DeviceRouter", "DeviceRoute",
                    "DeviceStatusMux", "FetchAddress", "FetchRequest", "RouterRequest",
                    "ConsoleCommand", "ConsoleRead", "ConsoleRegister", "TimerAccess",
                    "TimerRead", "TimerRegister", "ButtonInput", "ButtonLevel",
                    "ButtonEvent", "ButtonCommands", "ButtonRead", "ButtonRegister",
                    "PixelByte", "Rgb332", "DisplayRegister", "DisplayFrame",
                    "DisplayRead", "RegisterFrontEnd<", "LedBlink", "ConsoleTxByte",
                    "ButtonStatusBits", "DisplayFrameCounter")
import re

DEVICE_TYPES = ("ConsoleDevice", "TimerDevice", "ButtonDevice", "FramebufferDisplay", "LedPanel")


def defaults(kind):
    if kind == "ComputerSystem":
        return dict(aspect_ratio=1.10, min_aspect_ratio=.55, title_bar_ratio=.045,
                    pin_size_ratio=.018, boundary_area_ratio=.025)
    if kind == "ButtonInputUpdate":
        return dict(aspect_ratio=1.45, min_aspect_ratio=.5, title_bar_ratio=.055,
                    pin_size_ratio=.025, boundary_area_ratio=.025)
    if kind.startswith(("Bus<", "MemorySubsystem")):
        return dict(aspect_ratio=1.0, min_aspect_ratio=.5, title_bar_ratio=.05,
                    pin_size_ratio=.008, boundary_area_ratio=.025)
    if kind in DEVICE_TYPES or kind.startswith(PUBLIC_PREFIXES):
        return dict(aspect_ratio=.85, min_aspect_ratio=.4, title_bar_ratio=.065,
                    pin_size_ratio=.012 if not kind.startswith(("BusRouter<", "BusFetchRouter<")) else .007,
                    boundary_area_ratio=.025)
    if kind.startswith(PRIVATE_PREFIXES):
        return dict(aspect_ratio=.75, min_aspect_ratio=.4, title_bar_ratio=.06,
                    pin_size_ratio=.018, boundary_area_ratio=.025)
    return {}


# Small fixed floorplans, as data flows left to right: (child, x, y, width,
# height), with y and height as fractions of the content height.
_PART_GRIDS = {
    # Request held and checked, then the handshake control; the execution
    # (status, data, strobes), then the response buffer.
    "DeviceRegisterPort<": [("REQUEST", .07, 0, .40, .47), ("CONTROL", .07, .53, .40, .47),
                            ("EXECUTE", .53, 0, .40, .47), ("RESPONSE", .53, .53, .40, .47)],
    "SynchronousFifo<": [("CONTROL", .07, 0, .36, .47), ("POINTERS", .07, .53, .36, .47),
                         ("STORAGE", .50, 0, .43, 1)],
    "ClockedCounter<": [("TICK_CONTROL", .07, 0, .26, 1), ("COUNT_STATE", .37, 0, .26, 1),
                        ("INCREMENT", .67, 0, .26, 1)],
    # Both parts are wide: the RV32I decoder above its M-extension override.
    "RV32IMDecodeControlUnit": [("INTEGER", .07, 0, .86, .60), ("EXTENSION", .07, .64, .86, .36)],
}


def placements(kind, aspects, parent_aspect, title_fraction=0):
    if kind == "ComputerSystem":
        return _machine(aspects, parent_aspect, title_fraction)
    if kind in DEVICE_TYPES:
        return _device(aspects, parent_aspect, title_fraction)
    grid = _PART_GRIDS.get(next((p for p in _PART_GRIDS if kind.startswith(p)), None))
    if grid and set(aspects) >= {name for name, *_ in grid}:
        result, put = _placer(aspects, parent_aspect)
        top = max(.11, title_fraction + .035)
        for name, x, y, width, height in grid:
            put(name, x, top + y * (.94 - top), width, height * (.94 - top))
        return _rest(result, put, aspects)
    if kind.startswith("Bus<"):
        return _bus(aspects, parent_aspect, title_fraction)
    if kind.startswith("MemorySubsystem"):
        return _memory(aspects, parent_aspect, title_fraction)
    return None


def _placer(aspects, parent_aspect):
    result = {}

    def put(name, x, y, width, height):
        # Fit the part's proportions in the slot and centre it there.
        fitted = min(width, height * parent_aspect / aspects[name])
        fitted_height = fitted * aspects[name] / parent_aspect
        result[name] = dict(rel_pos=[x + (width - fitted) / 2, y + (height - fitted_height) / 2], rel_width=fitted)
    return result, put


def _rest(result, put, aspects):
    """Small leftover parts (constants, clock pass-throughs) in rows along the
    bottom edge, 16 to a row."""
    rest = sorted(set(aspects) - result.keys())
    rows = (len(rest) + 15) // 16
    for index, name in enumerate(rest):
        row, column = divmod(index, 16)
        put(name, .06 + column * .055, .97 - (rows - row) * .035, .03, .028)
    return result


def _machine(aspects, parent_aspect, title_fraction):
    """Core top left, memory bottom left; buses in the middle column with each
    bridge between the buses it joins; devices in the right column, top to
    bottom; port checkers in one row along the bottom of both columns. Loose
    gates (such as the checkers' combined flag) go in the leftover rows."""
    result, put = _placer(aspects, parent_aspect)
    top = max(.11, title_fraction + .035)
    names = set(aspects)
    devices = [n for n in ("CONSOLE", "TIMER", "BUTTONS", "DISPLAY", "LEDS") if n in names]
    checkers = sorted(n for n in names if n.startswith("CHECK_"))
    # The bus the core reaches first ("BUS", "SYSTEM_BUS", "CPU_BUS"), then
    # bridges, then the buses behind them.
    buses = sorted((n for n in names if n.endswith("BUS")),
                   key=lambda n: (n not in ("BUS", "SYSTEM_BUS", "CPU_BUS"), n))
    bridges = sorted(n for n in names if "BRIDGE" in n and n not in buses)
    middle = buses[:1] + bridges + buses[1:]
    if "CORE" in names:
        put("CORE", .04, top, .29, .36)
    if "MEMORY" in names:
        put("MEMORY", .04, .58, .29, .34)
    bottom = .76 if checkers else .94
    bridge_height = .09
    bus_height = (bottom - top - bridge_height * len(bridges) - .03 * max(0, len(middle) - 1)) / max(1, len(buses))
    y = top
    for name in middle:
        height = bridge_height if name in bridges else bus_height
        put(name, .37, y, .26, height)
        y += height + .03
    if checkers:
        step = .59 / len(checkers)
        for index, name in enumerate(checkers):
            put(name, .37 + index * step, .785, min(.05, step - .006), .07)
    others = [n for n in sorted(names - set(middle) - set(devices) - set(checkers) - {"CORE", "MEMORY"})
              if not n.startswith(("ZERO", "ONE", "PASS")) and not re.search(r"_\d+(_\d+)*$", n)]
    column = devices + others
    step = (bottom - top + .02) / max(1, len(column))
    for index, name in enumerate(column):
        put(name, .68, top + index * step, .28, step - .03)
    return _rest(result, put, aspects)


def _bus(aspects, parent_aspect, title_fraction):
    """Left to right, as requests flow: the routers stacked on the left (the
    masters' pins), the arbiters stacked on the right (toward the targets)."""
    result, put = _placer(aspects, parent_aspect)
    top = max(.11, title_fraction + .035)
    routers = sorted(n for n in aspects if n.endswith("_ROUTER"))
    arbiters = sorted(n for n in aspects if "_ARBITER" in n)
    columns = ((routers, .06, .46), (arbiters, .60, .32)) if arbiters else ((routers, .10, .80),)
    for group, x, width in columns:
        step = (.90 - top) / max(1, len(group))
        for index, name in enumerate(group):
            put(name, x, top + index * step, width, step - .04)
    return _rest(result, put, aspects)


def _device(aspects, parent_aspect, title_fraction):
    """Left to right, as an access flows: the bus PORT, the REGISTERS front
    end, then the device's own parts stacked toward its outside pins.
    One-bit constants go in the leftover row."""
    result, put = _placer(aspects, parent_aspect)
    top = max(.11, title_fraction + .035)
    height = .90 - top
    if "PORT" in aspects:
        put("PORT", .04, top + height * .15, .26, height * .70)
    if "REGISTERS" in aspects:
        put("REGISTERS", .36, top + height * .10, .26, height * .80)
    parts = [n for n in aspects if n not in ("PORT", "REGISTERS")
             and not re.match(r"(ZERO|ONE)_\d+$", n)]
    step = height / max(1, len(parts))
    for index, name in enumerate(parts):
        put(name, .68, top + index * step, .28, step - .03)
    return _rest(result, put, aspects)


def _memory(aspects, parent_aspect, title_fraction):
    """Top to bottom: the port caches side by side, the arbiter, the shared
    cache, the memory. Cache invalidation constants go in the leftover row."""
    result, put = _placer(aspects, parent_aspect)
    top = max(.11, title_fraction + .035)
    rows = []
    if "INSTRUCTION_CACHE" in aspects or "DATA_CACHE" in aspects:
        rows.append((("INSTRUCTION_CACHE", "DATA_CACHE"), .18))
    rows.append((("ARBITER",), .14))
    if "SHARED_CACHE" in aspects:
        rows.append((("SHARED_CACHE",), .14))
    rows.append((("RAM",), 0))
    fixed = sum(height for _, height in rows)
    gap = .04
    remaining = .90 - top - fixed - gap * (len(rows) - 1)
    y = top
    for names, height in rows:
        height = height or remaining
        present = [n for n in names if n in aspects]
        width = .84 / max(1, len(names))
        for index, name in enumerate(names):
            if name in present:
                put(name, .08 + index * width, y, width - .04 if len(names) > 1 else .60, height)
        if len(names) == 1 and names[0] in result:
            # Centre a single part.
            result[names[0]]["rel_pos"][0] = (1 - result[names[0]]["rel_width"]) / 2
        y += height + gap
    return _rest(result, put, aspects)
