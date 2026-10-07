# CircuitSim

Processor and memory performance campaigns share the data-only benchmark
envelope documented in [docs/benchmark-system.md](docs/benchmark-system.md).
Scenario organization and native/browser validation commands are documented in
[docs/test-organization.md](docs/test-organization.md).
Use `tools/benchmarks/run.py` as the serial campaign entry point; domain runners
retain their own correctness and completion rules.

CircuitSim is a C++20 event-driven digital-logic simulator with native Python
bindings and an interactive HTML/canvas visualizer.

The native visualizer remains available through `visualizer-v2/server.py`. The
same visualizer can also run entirely in a browser through WebAssembly for
static hosting under `/simulator/`; see
[`docs/webassembly-github-pages-report.md`](docs/webassembly-github-pages-report.md).

## Documentation site

Memory devices, the scalable memory container, transactions and controllers
are described in [chapter 3 of the site documentation](site-docs/memory/3.1/index.md).
Internal working notes are indexed in [docs/README.md](docs/README.md).

The public Material for MkDocs source is isolated in `site-docs/`; the existing
`docs/` directory remains an internal collection of implementation reports.
Install the pinned documentation dependency with:

```bash
python3 -m pip install -r requirements-docs.txt
```

To build the complete GitHub Pages artifact, including the WebAssembly
visualizer at `/simulator/`, activate Emscripten and run:

```bash
tools/build-pages.sh
python3 -m http.server 8877 --directory build-wasm/site
```

The deployable site is written to `build-wasm/site/`. The documentation occupies
the root and the unchanged browser visualizer occupies `/simulator/`.

After the first combined build, use `python3 -m mkdocs serve --config-file
mkdocs.preview.yml` for a live documentation preview with the built simulator.
Rebuild with `tools/build-pages.sh` after changing C++ circuits or test rows.

Chapter 2 example tables are generated from the recorded C++ scenarios, not
hand-maintained Markdown rows. Each `<!-- simulator-examples: SCENARIO TEST -->`
marker expands during MkDocs rendering. The Pages build exports declared inputs,
expected outputs, checkpoint labels, and settled times from the compiled WASM
engine to `build-wasm/pages/simulator/example-rows.json`, verifying every row
against recorded pin history. The preview reads that same generated data; no
native Python binding or backend server is needed for the published tables.
