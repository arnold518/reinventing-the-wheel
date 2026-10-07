"""Inline reviewed static figures without introducing diagram runtime JS."""

from pathlib import Path
import re

MARKER = re.compile(r"<!--\s*diagram:\s*(.*?)\s*-->", re.S)
SLUG = re.compile(r"[a-z0-9]+(?:-[a-z0-9]+)*")
REGION = re.compile(r"<!-- diagram:start -->\s*(.*?)\s*<!-- diagram:end -->", re.S)


def inline_diagrams(markdown, docs_dir):
    seen = set()

    def inline(match):
        slug = match[1]
        if not SLUG.fullmatch(slug):
            raise ValueError(f"Invalid diagram name: {slug!r}")
        if slug in seen:
            raise ValueError(f"Duplicate diagram ID on one page: {slug}")
        seen.add(slug)
        path = Path(docs_dir) / "assets/diagrams" / f"{slug}.html"
        try:
            source = path.read_text(encoding="utf-8")
        except OSError as error:
            raise ValueError(f"Cannot load diagram {slug}: {error}") from error
        regions = REGION.findall(source)
        if len(regions) != 1 or f'data-diagram="{slug}"' not in regions[0]:
            raise ValueError(f"Invalid figure boundary or ID: {path}")
        # Keep SVG text as raw HTML, not Markdown (in particular underscores).
        return regions[0]

    return MARKER.sub(inline, markdown)


def on_page_markdown(markdown, *, page, config, files, **kwargs):
    from mkdocs.exceptions import PluginError

    try:
        return inline_diagrams(markdown, config["docs_dir"])
    except ValueError as error:
        raise PluginError(str(error)) from error
