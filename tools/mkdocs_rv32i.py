"""Render source listings and reference-program checkpoints without a backend."""

from html import escape
import json
import os
from pathlib import Path
import re

REPOSITORY = Path(__file__).resolve().parent.parent
DEFAULT_DATA = REPOSITORY / "build-wasm/pages/simulator/example-rows.json"
SOURCE = re.compile(r"(?m)^([ \t]*)<!-- simulator-source: ([A-Za-z0-9_./-]+) -->[ \t]*$")
PROGRAM = re.compile(r"<!-- simulator-program: ([a-z0-9-]+) ([A-Za-z0-9_/-]+) -->")


def source_listing(match):
    indent, relative = match.groups()
    path = (REPOSITORY / relative).resolve()
    if not any(path.is_relative_to(REPOSITORY / root) for root in ("include", "src")):
        raise ValueError(f"Source listing must be inside include/ or src/: {relative}")
    if path.suffix not in (".hpp", ".cpp", ".tpp"):
        raise ValueError(f"Unsupported source listing: {relative}")
    source = path.read_text().rstrip()
    # Use a longer fence if the source itself happens to contain backticks.
    fence = "`" * max(3, 1 + max((len(s) for s in re.findall(r"`+", source)), default=0))
    block = f"`{relative}`\n\n{fence}cpp\n{source}\n{fence}"
    return "\n".join(indent + line if line else "" for line in block.splitlines())


def render_program(data, scenario, test):
    if data["scenario"] != scenario or data["registeredTest"] != test:
        raise ValueError("Program metadata mismatch")
    if data["coreFidelity"] != "behavioral" or not data["checkpoints"]:
        raise ValueError("Expected behavioral program checkpoints")
    html = [
        f'<div class="example-table-block" data-scenario="{escape(scenario)}">',
        f'<p>All {len(data["checkpoints"])} instruction-attempt checkpoints of '
        f'<code>{escape(test)}</code>, with a behavioral core. '
        'Times are absolute simulator units, not milliseconds or an ISA latency.</p>',
        '<div class="example-table-scroll" role="region" tabindex="0" '
        f'aria-label="{escape(scenario)} full instruction answer sheet">',
        '<table class="example-table"><thead><tr>'
        '<th scope="col">Checkpoint</th><th scope="col">Time</th>'
        '<th scope="col">Instruction / address</th>'
        '<th scope="col">Expected architectural effects</th></tr></thead><tbody>',
    ]
    previous_time = -1
    for index, checkpoint in enumerate(data["checkpoints"]):
        if checkpoint["rowIndex"] != index or checkpoint["time"] <= previous_time:
            raise ValueError("Unordered program checkpoints")
        if not checkpoint["label"].startswith(f"I{index} "):
            raise ValueError("Incorrect program checkpoint label")
        previous_time = checkpoint["time"]
        html.append(
            f'<tr data-checkpoint="I{index}" data-time="{checkpoint["time"]}" '
            f'data-timeline-index="{checkpoint["index"]}">'
            f'<th scope="row">I{index}</th><td>{checkpoint["time"]}</td>'
            f'<td><code>{escape(checkpoint["label"].split(" ", 1)[1])}</code></td>'
            f'<td><code>{escape(checkpoint["detail"])}</code></td></tr>')
    html.append('</tbody></table></div></div>')
    return "\n".join(html)


def on_page_markdown(markdown, *, page, config, files, **kwargs):
    from mkdocs.exceptions import PluginError

    try:
        # These long hand-authored contract tables remain Markdown (and are
        # parsed directly by tests), with the same accessible local scrolling
        # used by generated example tables.
        for marker in ("rv32i-encodings", "rv32i-decode-examples", "rv32i-step-examples", "rv32i-edge-examples"):
            pattern = rf"(<!-- {marker} -->\n)((?:\|[^\n]*\|\n)+)"
            markdown = re.sub(pattern, lambda m: m[1]
                + '<div class="reference-table-scroll" role="region" tabindex="0" '
                + f'aria-label="{marker}" markdown="1">\n\n' + m[2] + '\n</div>\n', markdown)
        markdown = SOURCE.sub(source_listing, markdown)
        if PROGRAM.search(markdown):
            path = Path(os.environ.get("CIRCUITSIM_EXAMPLE_ROWS", DEFAULT_DATA))
            bundle = json.loads(path.read_text())
            if bundle["schemaVersion"] != 1:
                raise ValueError("Unsupported example export schema")
            markdown = PROGRAM.sub(lambda m: render_program(bundle["programs"][m[1]], *m.groups()), markdown)
        return markdown
    except (OSError, ValueError, KeyError, TypeError) as error:
        raise PluginError(f"Cannot render RV32I reference in {page.file.src_uri}: {error}") from error


def on_serve(server, *, config, builder, **kwargs):
    # Source listings must refresh alongside their canonical implementation.
    for root in ("include/rv32i", "src/rv32i", "src/modules/rv32i"):
        server.watch(str(REPOSITORY / root), builder)
    return server
