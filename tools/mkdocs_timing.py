"""Render proven delay tables into the documentation.

A page writes ``<!-- timing-table: KEY -->`` where KEY is a stored table name
from ``part_delay_tables`` (for example
``arithmetic.adder.width8.behavioral.direct``). The hook replaces the marker
with one table: every path of the compact part, with its proven minimum and
maximum delay in each timing profile. The numbers are read from the generated
table files, so a page cannot drift from the code.
"""

from html import escape
from pathlib import Path
import re

REPOSITORY = Path(__file__).resolve().parent.parent
TABLES = REPOSITORY / "src/components/selection"
PROFILES = (
    ("unit", "ProvenPartDelays.generated.inc", "ticks"),
    ("embedded", "ProvenPartDelays.embedded.generated.inc", "ps"),
    ("desktop", "ProvenPartDelays.desktop.generated.inc", "ps"),
)
MARKER = re.compile(r"<!-- timing-table: ([A-Za-z0-9_.@=-]+) -->")
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
                f'aria-label="Proven delays of {escape(key)}"><table class="example-table"><thead><tr>'
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


def on_page_markdown(markdown, page, config, files):
    return MARKER.sub(lambda match: render(match.group(1)), markdown)
