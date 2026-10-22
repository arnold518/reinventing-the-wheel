"""Render proven delay tables into the documentation.

A page writes ``<!-- timing-table: KEY -->`` where KEY is a stored table name
from ``part_delay_tables`` (for example
``word.adder.8.ripple.behavioral``). The hook replaces the marker
with one table: every path of the compact part, with its proven minimum and
maximum delay in each timing profile. The numbers are read from the generated
table files, so a page cannot drift from the code.
"""

from html import escape
from pathlib import Path
import json
import re

REPOSITORY = Path(__file__).resolve().parent.parent
TABLES = REPOSITORY / "src/components/selection"
PROFILES = (
    ("unit", "ProvenPartDelays.generated.inc", "ticks"),
    ("embedded", "ProvenPartDelays.embedded.generated.inc", "ps"),
    ("desktop", "ProvenPartDelays.desktop.generated.inc", "ps"),
)
MARKER = re.compile(r"<!-- timing-table: ([A-Za-z0-9_.@=-]+) -->")
LATENCY_MARKER = re.compile(r"<!-- latency-table: ([a-z,-]+) -->")
PROFILE_MARKER = re.compile(r"<!-- profile-table: (gates|flip-flop|sram-cell|clocks|memory-sram|memory-rom|memory-dram|memory-nor) -->")
LATENCIES = REPOSITORY / "tools/timing/documentation-timing.generated.json"
MEMORY_NAMES = {
    "ff": "Flip-flop", "sram": "SRAM (asynchronous chip)", "sram-clocked": "SRAM (clocked on-chip macro)",
    "rom": "ROM", "dram": "DRAM", "nor": "NOR flash",
}
ENTRY = re.compile(r'^\{"([^"]+)", \[\] \{$')
ARC = re.compile(r'\{"([^"]+)", "([^"]+)", \{(\d+), (\d+)\}\}')
_cache = {}


def parse(path):
    """Stored tables of one profile: name -> dict of fields."""
    tables, current = {}, None
    for line in path.read_text(encoding="utf-8").splitlines():
        if match := ENTRY.match(line):
            current = tables.setdefault(match.group(1), {
                "evidence": "", "arcs": [], "control": None, "by_control": {},
                "clock": None, "launches": {}, "setup": {}, "internal_period": 0,
                "fixed": {}, "self_timed": False, "clocked_interface": False})
            continue
        if current is None:
            continue
        text = line.strip()
        if text.startswith("}()}"):
            current = None
        elif m := re.match(r't\.evidence = "(.*)";', text):
            current["evidence"] = m.group(1)
        elif m := re.match(r't\.control = "(.*)";', text):
            current["control"] = m.group(1)
        elif m := re.match(r't\.clock = "(.*)";', text):
            current["clock"] = m.group(1)
        elif m := re.match(r't\.arcs_by_control\[(\d+)\] = (.*);', text):
            current["by_control"][int(m.group(1))] = [
                (a, b, int(lo), int(hi)) for a, b, lo, hi in ARC.findall(m.group(2))]
        elif m := re.match(r'Arc\{"([^"]+)", "([^"]+)", \{(\d+), (\d+)\}\},', text):
            current["arcs"].append((m.group(1), m.group(2), int(m.group(3)), int(m.group(4))))
        elif m := re.match(r't\.launches\["([^"]+)"\] = \{(\d+), (\d+)\};', text):
            current["launches"][m.group(1)] = (int(m.group(2)), int(m.group(3)))
        elif m := re.match(r't\.setup\["([^"]+)"\] = (\d+);', text):
            current["setup"][m.group(1)] = int(m.group(2))
        elif m := re.match(r't\.internal_period = (\d+);', text):
            current["internal_period"] = int(m.group(1))
        elif m := re.match(r't\.fixed_inputs\["([^"]+)"\] = (\d+);', text):
            current["fixed"][m.group(1)] = int(m.group(2))
        elif text == "t.self_timed = true;":
            current["self_timed"] = True
        elif text == "t.clocked_interface = true;":
            current["clocked_interface"] = True
    return tables


def profiles():
    if not _cache:
        for name, file, unit in PROFILES:
            _cache[name] = (parse(TABLES / file), unit)
    return _cache


def without_twin():
    text = (TABLES / "NoGateList.generated.inc").read_text(encoding="utf-8")
    return set(re.findall(r'^"([^"]+)"', text, re.MULTILINE))


def rows_of(table):
    """(group, path label, low, high) for every timed relationship."""
    rows = []
    if table["by_control"]:
        merged = {}
        for arcs in table["by_control"].values():
            for a, b, lo, hi in arcs:
                old = merged.get((a, b))
                merged[(a, b)] = (lo, hi) if old is None else (min(old[0], lo), max(old[1], hi))
        for (a, b), (lo, hi) in merged.items():
            rows.append(("path", f"{a} → {b}", lo, hi))
    for a, b, lo, hi in table["arcs"]:
        rows.append(("path", f"{a} → {b}", lo, hi))
    for out, (lo, hi) in sorted(table["launches"].items()):
        rows.append(("launch", f"{table['clock'] or 'clock'} ↑ → {out}", lo, hi))
    for pin, value in sorted(table["setup"].items()):
        rows.append(("setup", f"{pin} before {table['clock'] or 'clock'} ↑", value, value))
    if table["internal_period"]:
        rows.append(("period", "Shortest internal clock period", table["internal_period"], table["internal_period"]))
    return rows


def contract_of(key):
    return key.split("@", 1)[0].split(".behavioral", 1)[0]


def render(key):
    data = profiles()
    missing = [name for name, (tables, _) in data.items() if key not in tables]
    if missing:
        raise ValueError(f"No stored delay table {key!r} in profile(s) {', '.join(missing)}")
    first = data["unit"][0][key]
    order = [label for _, label, _, _ in rows_of(first)]
    values = {}
    for name, (tables, _) in data.items():
        table = tables[key]
        if [label for _, label, _, _ in rows_of(table)] != order:
            raise ValueError(f"Delay table {key!r} has different paths in profile {name}")
        values[name] = {label: (lo, hi) for _, label, lo, hi in rows_of(table)}

    def cell(lo, hi):
        return f"{lo:,}" if lo == hi else f"{lo:,}–{hi:,}"

    html = ['<div class="timing-table-block" data-table="' + escape(key) + '">']
    notes = [f"Proven delays of <code>{escape(key)}</code> "
             f"({escape(first['evidence'] or 'no evidence recorded')})."]
    if first["control"]:
        notes.append(f"Paths are given over every value of <code>{escape(first['control'])}</code>; "
                     "each value has its own table in the code.")
    if first["fixed"]:
        held = ", ".join(f"<code>{escape(p)}</code> = {v}" for p, v in sorted(first["fixed"].items()))
        notes.append(f"Assumes {held}.")
    if first["self_timed"]:
        notes.append("The part also changes outputs at times of its own"
                     + (", inside it only: its outputs still move only after a clock edge."
                        if first["clocked_interface"] else "; no clock can be certified around it."))
    notes.append("A delay twin times it from its current values within these bounds."
                 if contract_of(key) not in without_twin()
                 else "No delay twin: the compact part uses the maximum.")
    html.append('<p class="timing-table-description">' + " ".join(notes) + "</p>")
    html.append('<div class="example-table-scroll" role="region" tabindex="0" '
                f'aria-label="Proven delays of {escape(key)}"><table class="timing-table"><thead><tr>'
                '<th scope="col">Path</th>')
    for name, (_, unit) in data.items():
        html.append(f'<th scope="col"><code>{escape(name)}</code><small>{unit}, minimum–maximum</small></th>')
    html.append("</tr></thead><tbody>")
    for label in order:
        html.append(f"<tr><th scope=\"row\">{escape(label)}</th>")
        for name in data:
            html.append(f"<td>{cell(*values[name][label])}</td>")
        html.append("</tr>")
    html.append("</tbody></table></div></div>")
    return "".join(html)


def duration(ticks, tick_picoseconds):
    """A tick count as a readable time in a profile with a tick length."""
    if not tick_picoseconds:
        return f"{ticks:,} ticks"
    ps = ticks * tick_picoseconds
    for unit, size in (("ms", 10**9), ("µs", 10**6), ("ns", 10**3)):
        if ps >= size:
            value = ps / size
            text = f"{value:,.2f}".rstrip("0").rstrip(".") if value < 100 else f"{value:,.0f}"
            return f"{text} {unit}"
    return f"{ps:,} ps"


def render_latency(selection):
    """Measured access latency per profile (tools/timing/DocumentationTimingData.cpp)."""
    data = json.loads(LATENCIES.read_text(encoding="utf-8"))
    profiles = data["profiles"]
    wanted = None if selection == "all" else selection.split(",")
    unknown = [name for name in (wanted or []) if name not in MEMORY_NAMES]
    if unknown:
        raise ValueError(f"Unknown memory {', '.join(unknown)} in latency-table marker")
    order = [(a["memory"], a["operation"]) for a in profiles[0]["accesses"]
             if wanted is None or a["memory"] in wanted]
    if not order:
        raise ValueError(f"No measured access for latency-table {selection!r}")
    cells = {}
    for profile in profiles:
        if [(a["memory"], a["operation"]) for a in profile["accesses"]
                if wanted is None or a["memory"] in wanted] != order:
            raise ValueError(f"Latency table rows differ in profile {profile['name']}")
        for a in profile["accesses"]:
            key = (a["memory"], a["operation"])
            if a["simulated"]:
                time = duration(a["cycles"] * profile["period_ticks"], profile["tick_picoseconds"])
                text = f"{a['cycles']:,}" + (f" <small>({time})</small>" if a["cycles"] else " <small>(same edge)</small>")
            else:
                text = (f"≈ {-(-a['interval_ticks'] // profile['period_ticks']):,} "
                        f"<small>({duration(a['interval_ticks'], profile['tick_picoseconds'])}, configured)</small>")
            cells[(profile["name"], key)] = text
    html = ['<div class="timing-table-block" data-latency="' + escape(selection) + '">']
    clocks = ", ".join(f"<code>{escape(p['name'])}</code> {duration(p['period_ticks'], p['tick_picoseconds'])}"
                       for p in profiles)
    html.append('<p class="timing-table-description">Measured latency in clock cycles, from the edge that '
                'accepts a request to the edge that takes its response, through the compact endpoint of a 256 KiB '
                f"memory at the advanced core's certified clock ({clocks}). Refresh is off. “Configured” rows are too long to "
                "simulate and show the device interval instead.</p>")
    html.append('<div class="example-table-scroll" role="region" tabindex="0" aria-label="Measured memory access '
                'latency"><table class="timing-table"><thead><tr><th scope="col">Memory</th>'
                '<th scope="col">Access</th>')
    for profile in profiles:
        html.append(f'<th scope="col"><code>{escape(profile["name"])}</code><small>cycles</small></th>')
    html.append("</tr></thead><tbody>")
    for memory, operation in order:
        html.append(f'<tr><th scope="row">{escape(MEMORY_NAMES[memory])}</th><td class="text">{escape(operation)}</td>')
        for profile in profiles:
            html.append(f"<td>{cells[(profile['name'], (memory, operation))]}</td>")
        html.append("</tr>")
    html.append("</tbody></table></div></div>")
    return "".join(html)


PROFILE_TABLES = {
    "gates": ("gates", "Leaf part", "Delay of each leaf part (the parts with no gate-level form)", None),
    "flip-flop": ("flip_flop", "Flip-flop contract", "Measured contract of the gate-level D flip-flop", {
        "setup": "Setup before the rising edge", "hold": "Hold after the rising edge",
        "clock_to_output_minimum": "Clock to Q, earliest", "clock_to_output_maximum": "Clock to Q, latest",
        "minimum_high": "Shortest clock high phase", "minimum_low": "Shortest clock low phase"}),
    "sram-cell": ("sram_cell", "SRAM cell contract", "Measured contract of the gate-level SRAM cell", {
        "earliest": "Earliest output change after a write", "latest": "Latest output change after a write",
        "minimum_spacing": "Shortest spacing between writes"}),
    "clocks": ("certified_periods", "Core", "Certified clock period of each core with flip-flop memory", {
        "single": "Single-cycle", "baseline": "Baseline pipeline", "advanced": "Advanced pipeline"}),
    "memory-sram": ("memory_intervals", "SRAM interval", "Analog intervals of the SRAM devices", {
        "sram_settle": "Address-change settle", "sram_precharge": "Bitline precharge",
        "sram_bitline_base": "Bitline development, base", "sram_bitline_per_row": "Bitline development, per row",
        "sram_wire_per_side": "Global wire, per subarray crossed"}),
    "memory-rom": ("memory_intervals", "ROM interval", "Analog intervals of the ROM devices", {
        "rom_settle": "Address-change settle", "rom_column_base": "Read column, base",
        "rom_column_per_row": "Read column, per row"}),
    "memory-dram": ("memory_intervals", "DRAM interval", "Analog intervals of the DRAM devices", {
        "dram_wordline": "ACTIVATE: wordline", "dram_sense_base": "ACTIVATE: sense, base",
        "dram_sense_per_row": "ACTIVATE: sense, per row", "dram_restore": "ACTIVATE: restore",
        "dram_read": "READ: column to data", "dram_write_on": "WRITE: drive on", "dram_write_off": "WRITE: drive off",
        "dram_write_recovery": "WRITE: recovery", "dram_close": "PRECHARGE: close row",
        "dram_precharge": "PRECHARGE: precharge", "dram_equalize": "PRECHARGE: equalize",
        "dram_refresh_restore": "REFRESH: full restore", "dram_refresh_extra": "REFRESH: rest of the refresh cycle",
        "dram_refresh_interval": "Refresh interval (0: the controller's own count)",
        "dram_device_retention": "Cell retention"}),
    "memory-nor": ("memory_intervals", "NOR-flash interval", "Analog intervals of the NOR-flash devices", {
        "nor_read": "Read column, base", "nor_read_per_row": "Read column, per row",
        "nor_program": "Program pulse", "nor_erase": "Erase pulse", "nor_pulse_end": "End of a pulse",
        "nor_recovery": "Recovery after a pulse"}),
}


def render_profile_table(kind):
    """One timing number per row, one column per profile."""
    field, heading, description, labels = PROFILE_TABLES[kind]
    profiles = json.loads(LATENCIES.read_text(encoding="utf-8"))["profiles"]
    keys = list(labels) if labels else list(profiles[0][field])
    timed = kind.startswith("memory-")
    html = ['<div class="timing-table-block" data-profile-table="' + kind + '">',
            f'<p class="timing-table-description">{description}, per timing profile.</p>',
            '<div class="example-table-scroll" role="region" tabindex="0" aria-label="' + escape(description) + '">'
            '<table class="timing-table"><thead><tr><th scope="col">' + escape(heading) + "</th>"]
    for profile in profiles:
        unit = "ps" if profile["tick_picoseconds"] == 1 else ("ticks" if not profile["tick_picoseconds"]
                                                               else f"× {profile['tick_picoseconds']} ps")
        if kind == "clocks" and profile["tick_picoseconds"]:
            unit = "ps (MHz)"
        if timed:
            unit = "time" if profile["tick_picoseconds"] else "ticks"
        html.append(f'<th scope="col"><code>{escape(profile["name"])}</code><small>{unit}</small></th>')
    html.append("</tr></thead><tbody>")
    for key in keys:
        name = labels[key] if labels else (f"<code>{escape(key)}</code>" if key in ("NOT", "NAND", "NOR", "AND", "OR", "XOR")
                                            else escape(key.capitalize()))
        html.append(f'<tr><th scope="row">{name}</th>')
        for profile in profiles:
            value = profile[field][key]
            text = (duration(value, profile["tick_picoseconds"]) if profile["tick_picoseconds"] else f"{value:,}") \
                if timed else f"{value:,}"
            if kind == "clocks" and profile["tick_picoseconds"]:
                text += f" <small>({1e6 / (value * profile['tick_picoseconds']):,.0f})</small>"
            html.append(f"<td>{text}</td>")
        html.append("</tr>")
    html.append("</tbody></table></div></div>")
    return "".join(html)


def render_rows(spec):
    """Chosen paths of one or more stored tables, latest change per profile.

    Each line of the marker is ``label | table key | path``, where path is a
    row label of that table as ``rows_of`` names it (``A → Cout``,
    ``CLK ↑ → Q``, ``D before CLK ↑``).
    """
    data = profiles()
    rows = []
    for line in filter(None, (line.strip() for line in spec.strip().splitlines())):
        parts = [part.strip() for part in line.split("|")]
        if len(parts) != 3:
            raise ValueError(f"timing-rows line needs 'label | key | path': {line!r}")
        label, key, path = parts
        cells = []
        for name, (tables, _) in data.items():
            if key not in tables:
                raise ValueError(f"No stored delay table {key!r} in profile {name}")
            found = [high for _, row, _, high in rows_of(tables[key]) if row == path]
            if not found:
                raise ValueError(f"Delay table {key!r} has no path {path!r} in profile {name}")
            cells.append(found[0])
        rows.append((label, path, cells))
    html = ['<div class="timing-table-block" data-timing-rows="' + escape(";".join(r[0] for r in rows)) + '">',
            '<p class="timing-table-description">Latest time each path can still change after its input '
            '(for a setup row: how long before the edge the input must be stable), proven on the gate-level form.</p>',
            '<div class="example-table-scroll" role="region" tabindex="0" aria-label="Selected proven delays">'
            '<table class="timing-table"><thead><tr><th scope="col">Path</th>']
    for name, (_, unit) in data.items():
        html.append(f'<th scope="col"><code>{escape(name)}</code><small>{unit}</small></th>')
    html.append("</tr></thead><tbody>")
    for label, path, cells in rows:
        html.append(f'<tr><th scope="row">{escape(label)} <small><code>{escape(path)}</code></small></th>')
        html.extend(f"<td>{value:,}</td>" for value in cells)
        html.append("</tr>")
    html.append("</tbody></table></div></div>")
    return "".join(html)


ROWS_MARKER = re.compile(r"<!-- timing-rows\n(.*?)\n-->", re.DOTALL)


def on_page_markdown(markdown, page, config, files):
    markdown = ROWS_MARKER.sub(lambda match: render_rows(match.group(1)), markdown)
    markdown = MARKER.sub(lambda match: render(match.group(1)), markdown)
    markdown = PROFILE_MARKER.sub(lambda match: render_profile_table(match.group(1)), markdown)
    return LATENCY_MARKER.sub(lambda match: render_latency(match.group(1)), markdown)
