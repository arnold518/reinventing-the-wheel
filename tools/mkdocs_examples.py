"""Render static reference tables from verified simulator scenario exports."""

from html import escape
import json
import os
from pathlib import Path
import re

REPOSITORY = Path(__file__).resolve().parent.parent
DEFAULT_DATA = REPOSITORY / "build-wasm/pages/simulator/example-rows.json"
MARKER = re.compile(r"<!-- simulator-examples: ([a-z0-9-]+) ([A-Za-z0-9_]+) -->")


def format_value(value, width):
    if value is None:
        return '<span title="This output is not asserted by this test row">not checked</span>'
    if not isinstance(value, str) or len(value) != width or re.fullmatch(r"[01XZ]+", value) is None:
        raise ValueError(f"Invalid {width}-bit example value: {value!r}")
    if width == 1:
        display = value
    elif set(value) <= {"0", "1"}:
        display = f"0x{int(value, 2):0{(width + 3) // 4}X}"
    else:
        display = "0b" + value
    return f'<code title="{escape(value)}">{display}</code>'


def render_table(data, scenario, test):
    if data["schemaVersion"] != 1 or data["scenario"] != scenario or data["registeredTest"] != test:
        raise ValueError(f"Example metadata mismatch for {scenario}/{test}")
    rows = data["rows"]
    if not rows:
        raise ValueError(f"No recorded rows for {scenario}")
    columns = [("inputs", pin) for pin in data["inputs"]] + [("expectedOutputs", pin) for pin in data["outputs"]]
    if len({pin["name"] for _, pin in columns}) != len(columns):
        raise ValueError("Duplicate example pin names")
    description = f"examples-{scenario}-description"
    html = [
        f'<div class="example-table-block" data-scenario="{escape(scenario)}" '
        f'data-profile="{escape(data["profileFingerprint"])}">',
        f'<p id="{description}" class="example-table-description">'
        f'All {len(rows)} recorded rows of <code>{escape(test)}</code>, in checkpoint order. '
        'Outputs are test expectations verified against the simulation. '
        'Time is the absolute settled checkpoint time in simulator units, not milliseconds or CPU cycles.</p>',
        '<p class="example-table-profile">Default scenario profile: '
        f'<code>{escape(data["profileName"])}</code> / {escape(data["fidelity"])} '
        f'(<code>{escape(data["profileFingerprint"])}</code>). '
        'Changing fidelity or the circuit can change these times.</p>',
        f'<div class="example-table-scroll" role="region" tabindex="0" '
        f'aria-label="{escape(data["rootType"])} recorded test rows" aria-describedby="{description}">',
        '<table class="example-table"><thead><tr>',
        '<th scope="col">Checkpoint<small>zero-based number · timeline label</small></th>',
        '<th scope="col">Time<small>simulator units</small></th>',
    ]
    for group, pin in columns:
        label = "Input" if group == "inputs" else "Expected output"
        html.append(f'<th scope="col"><code>{escape(pin["name"])}</code><small>{label} · {pin["width"]} bit</small></th>')
    html.append('</tr></thead><tbody>')
    previous_time = -1
    labels = set()
    for index, row in enumerate(rows):
        label = row["checkpoint"]
        if (row["rowIndex"] != index or not isinstance(label, str) or not label
                or label in labels or row["time"] <= previous_time):
            raise ValueError(f"Invalid checkpoint order for {scenario}")
        labels.add(label)
        previous_time = row["time"]
        if set(row["inputs"]) != {pin["name"] for pin in data["inputs"]}:
            raise ValueError(f"Missing/extra inputs for {scenario}/{index}")
        if set(row["expectedOutputs"]) - {pin["name"] for pin in data["outputs"]}:
            raise ValueError(f"Unexpected output for {scenario}/{index}")
        html.append(f'<tr data-checkpoint="{escape(row["checkpoint"])}" data-time="{row["time"]}" '
                    f'data-timeline-index="{row["timelineIndex"]}">'
                    f'<th scope="row"><small>#{index}</small><code>{escape(label)}</code></th><td>{row["time"]}</td>')
        for group, pin in columns:
            value = row[group].get(pin["name"])
            html.append(f'<td data-pin="{escape(pin["name"])}" data-direction="{group}" '
                        f'data-bits="{escape(value or "")}">{format_value(value, pin["width"])}</td>')
        html.append('</tr>')
    html.extend(['</tbody></table></div>',
                 '<p class="example-table-help">Scroll within the table for all rows and pins. '
                 'Buses use hexadecimal; values containing X/Z use MSB-first binary. '
                 'Inputs show the declared values in effect at each checkpoint, including retained inputs. '
                 'Sequential tests also replay preparation changes between checkpoints, such as lowering the clock. '
                 'Use the matching checkpoint label in the simulation below.</p>',
                 '</div>'])
    return "\n".join(html)


def on_page_markdown(markdown, *, page, config, files, **kwargs):
    from mkdocs.exceptions import PluginError

    if not MARKER.search(markdown):
        return markdown
    path = Path(os.environ.get("CIRCUITSIM_EXAMPLE_ROWS", DEFAULT_DATA))
    try:
        # Read each build, including live reload. Do not retain a stale cache.
        bundle = json.loads(path.read_text())
        if bundle["schemaVersion"] != 1:
            raise ValueError("Unsupported example export schema")
        return MARKER.sub(lambda match: render_table(bundle["scenarios"][match[1]], match[1], match[2]), markdown)
    except (OSError, ValueError, KeyError, TypeError) as error:
        raise PluginError(f"Cannot render simulator examples in {page.file.src_uri}: {error}. "
                          "Run tools/build-pages.sh to regenerate verified example data.") from error
