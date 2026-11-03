"""Add the built WebAssembly visualizer to MkDocs development builds."""

from pathlib import Path
import shutil

from mkdocs.exceptions import PluginError


REPOSITORY_DIR = Path(__file__).resolve().parent.parent
SIMULATOR_SOURCE = REPOSITORY_DIR / "build-wasm" / "pages" / "simulator"


def on_serve(server, **_kwargs):
    """Let the parent page reload its embeds, without one long poll per iframe."""
    # MkDocs injects its poller into every HTML response, including the copied
    # simulator. Six long-lived iframe requests can exhaust Chrome's HTTP/1.1
    # connection pool and prevent later embeds from fetching their WASM/assets.
    # There is no public injection filter. Keep this small compatibility shim
    # confined to the preview server; production pages do not use this hook.
    inject = server._inject_js_into_html

    def inject_top_level_only(content, epoch):
        html = inject(content, epoch)
        invocation = b"\nlivereload("
        if invocation not in html:
            raise RuntimeError("MkDocs live-reload injection changed; review the iframe guard")
        return html.replace(invocation, b"\nif (window.self === window.top) livereload(", 1)

    server._inject_js_into_html = inject_top_level_only
    return server


def on_post_build(*, config, **_kwargs):
    """Copy the existing WASM build into MkDocs' temporary served site."""
    source = REPOSITORY_DIR / config["extra"].get("preview_simulator_dir", SIMULATOR_SOURCE)
    if not (source / "index.html").is_file():
        raise PluginError(
            f"The WebAssembly visualizer has not been built at {source}. "
            "Run ./tools/build-pages.sh with the configured build directory "
            "before starting the preview service."
        )

    destination = Path(config.site_dir) / "simulator"
    shutil.rmtree(destination, ignore_errors=True)
    shutil.copytree(source, destination)
    (Path(config.site_dir) / ".nojekyll").touch()
