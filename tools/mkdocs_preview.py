"""Add the built WebAssembly visualizer to MkDocs development builds."""

from pathlib import Path
import shutil

from mkdocs.exceptions import PluginError


REPOSITORY_DIR = Path(__file__).resolve().parent.parent
SIMULATOR_SOURCE = REPOSITORY_DIR / "build-wasm" / "pages" / "simulator"


def on_post_build(*, config, **_kwargs):
    """Copy the existing WASM build into MkDocs' temporary served site."""
    if not (SIMULATOR_SOURCE / "index.html").is_file():
        raise PluginError(
            "The WebAssembly visualizer has not been built. "
            "Run ./tools/build-pages.sh once before starting the preview service."
        )

    destination = Path(config.site_dir) / "simulator"
    shutil.rmtree(destination, ignore_errors=True)
    shutil.copytree(SIMULATOR_SOURCE, destination)
    (Path(config.site_dir) / ".nojekyll").touch()
