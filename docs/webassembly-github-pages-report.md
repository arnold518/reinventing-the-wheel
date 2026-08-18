# WebAssembly and GitHub Pages Implementation Report

## Scope

The WebAssembly runtime is an additional deployment path for Visualizer V2. It
does not remove or replace the Python server, pybind11 module, native tests, or
standalone C++ tools.

Both deployments use the same simulator sources and the same HTML, CSS,
component explorer, canvas renderer, timeline, inspectors, wire routing, and
layout editor.

## Runtime Boundary

The visualizer selects its transport from `runtime-config.js`.

| Deployment | Request path | Simulation path | Persistence |
|---|---|---|---|
| Native | `fetch('/api/...')` | `server.py` -> pybind11 -> C++ simulator | `layout.json` and `profiles.json` |
| GitHub Pages | in-page request adapter | Web Worker -> Embind -> C++ simulator in WASM | IndexedDB for the current browser origin |

`runtime-config.js` defaults to `http`, so the existing native startup path is
unchanged. The Pages packaging step replaces only that generated file with the
`wasm` configuration.

The worker keeps circuit construction and simulation off the browser's UI
thread. Large structural scenarios can therefore display the existing loading
overlay without freezing canvas and toolbar event handling.

## Exposed C++ Contract

`circuit::visualizer::VisualizerSession` is a portable, HTTP-independent facade
over the existing test registry and simulator. The Embind wrapper exposes only
these operations:

- enumerate every registered visualizable scenario;
- build a scenario with exact-path structural or behavioral overrides;
- return the progressive component topology and a requested topology scope;
- return an indexed pin/wire snapshot for a timeline index;
- return register-file and `Memory64Kx32` overlay state.

The facade uses the existing `SimulationTest`, `BuildProfile`, `Simulator`,
`Component`, `PinBase`, and `WireBase` objects. There is no JavaScript simulator
and no second implementation of circuit behavior.

The browser API adapter preserves the eight endpoints used by `app.js`:

```text
GET  /api/scenarios
GET  /api/circuit
GET  /api/state
GET  /api/component-state
POST /api/topology
POST /api/layout
POST /api/layout/defaults
POST /api/circuit/apply
```

## Visualizer Fidelity

The Pages build retains:

- all 154 scenarios currently marked visualizable in `TestRegistry`;
- complete component identity, hierarchy, implementation, fidelity, and
  profile-selection metadata;
- exact pin and wire IDs, widths, endpoints, and state indices;
- all four logic states (`0`, `1`, `X`, and `Z`) and multi-bit values;
- timeline timestamps, checkpoints, and performance metrics;
- progressive topology loading for large structural circuits;
- behavioral register-file and memory overlays;
- structural/behavioral profile editing with optimistic revision checks;
- component placement, type styles, colors, undo/redo, reset, and save.

The committed `layout.json` is shipped unchanged, including type, profile, and
root layouts. Resetting a current component uses its committed default
placement. A newly added topology without committed defaults receives a
deterministic grid until its layout is generated and committed; the Python
runtime can still run its connectivity-aware layered layout generator. This is
the only intentional presentation-policy difference.

## Browser Persistence

The Pages runtime stores two records in the `circuitsim-visualizer-v2`
IndexedDB database:

- `layout`: the same complete layout document sent to the native `/api/layout`
  endpoint;
- `profiles`: per-scenario revision and exact-path fidelity overrides.

The saved data survives page reloads and new workers. It is local to one
browser profile and one site origin; it is not written back to Git, shared with
other devices, or synchronized among researchers. The native server remains
the appropriate runtime when a shared or version-controlled layout/profile is
required.

## Building Locally

Install and activate Emscripten 6.0.7, then run:

```bash
emcmake cmake -S . -B build-wasm -DCMAKE_BUILD_TYPE=Release
cmake --build build-wasm --target circuitsim_wasm --parallel 2
python3 -m http.server 8877 --directory build-wasm/pages
```

Open `http://127.0.0.1:8877/simulator/`. The build produces one deployable
tree:

```text
build-wasm/pages/
├── .nojekyll
├── index.html
└── simulator/
    ├── index.html
    ├── circuitsim_wasm.js
    ├── circuitsim_wasm.wasm
    ├── layout.json
    └── existing visualizer JavaScript and CSS
```

The root redirect is deliberately small. A future MkDocs build can occupy the
Pages root while this artifact remains at `/simulator/`.

## GitHub Pages Deployment

`.github/workflows/github-pages.yml` builds on pushes to `github-pages` and on
manual dispatch. It pins Emscripten 6.0.7, tests the generated module, runs the
packaged site in headless Chrome, uploads `build-wasm/pages`, and deploys it
with the official Pages actions.

The repository's Pages source must be configured to **GitHub Actions**. One
repository still has one Pages deployment; documentation and the simulator are
combined as paths within that deployment rather than configured as separate
Pages sites.

## Validation

The implementation includes three new boundaries:

- `VisualizerSessionContractTest` compares the portable C++ facade with the
  current Python server for topology, indexed state, scenario inventory, and
  memory overlay data;
- `VisualizerWasmRuntimeTest.mjs` instantiates the generated module in Node and
  exercises scenario construction, state, topology, and overlays;
- `VisualizerWasmBrowserTest.py` loads the packaged site in Chrome and verifies
  rendering startup, scenario inventory, IndexedDB layout saving, fidelity
  profile saving, stale-revision rejection, and progressive topology requests.

## Measured Performance

Measurements were taken from the final `-O3` Emscripten 6.0.7 build on the
development host. Browser times use a clean headless-Chrome context and a local
static HTTP server. They are engineering checks, not cross-device guarantees.

| Artifact | Raw | gzip |
|---|---:|---:|
| Emscripten JavaScript loader | 60,666 bytes | 14,769 bytes |
| Simulator WebAssembly | 6,229,487 bytes | 1,217,664 bytes |
| Committed layout catalog | 979,901 bytes | 75,273 bytes |
| Combined | 7,270,054 bytes | 1,307,706 bytes |

| Scenario | Topology | Native facade | WASM build/sim/index | Peak RSS, native / WASM |
|---|---:|---:|---:|---:|
| Adder8 | 68 components, 256 pins, 169 wires | 0.01-0.04 s | 0.15-0.36 s, plus 0.15-0.22 s module initialization | 9 MB / 120 MB |
| Five-stage core | 13,237 components, 46,954 pins, 27,180 wires | 1.73 s | 5.47 s | 129 MB / 211 MB |
| RV32I Program 1 | 9,824 components, 34,395 pins, 19,460 wires, 2,319 timestamps | 4.17 s | 9.65 s | 141 MB / 219 MB |

The complete Adder8 Pages application reached a ready canvas in 2.65 seconds.
The Program 1 application reached a ready, interactive canvas in 12.20 seconds
with the full timeline and produced no browser errors. The native HTTP version
of the same Adder8 UI reached ready in 1.33 seconds.

For meaningful CPU scenarios, WebAssembly was about 2.3-3.2 times slower in
the simulator/build/index phase and used about 1.5-1.6 times the peak resident
memory of the optimized native facade. This is suitable for the current static
interactive visualizer, but it does not make browser execution the preferred
runtime for high-throughput research batches.

## Research-Platform Boundary

WebAssembly is a deployment target, not a replacement architecture. The C++
simulator remains the source of simulation truth and the Python server remains
available for shared files, automation, batch experiments, and future research
services. Browser-local execution is useful for reproducible demonstrations
and interactive single-user exploration, while server execution remains the
better boundary for large studies and centralized datasets.
