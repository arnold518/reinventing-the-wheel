#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPOSITORY_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="${1:-$REPOSITORY_DIR/build-wasm}"
SITE_DIR="$BUILD_DIR/site"

if ! command -v emcmake >/dev/null 2>&1; then
    echo "emcmake was not found. Install and activate Emscripten first." >&2
    exit 1
fi

if ! python3 -m mkdocs --version >/dev/null 2>&1; then
    echo "MkDocs was not found. Run: python3 -m pip install -r requirements-docs.txt" >&2
    exit 1
fi

emcmake cmake \
    -S "$REPOSITORY_DIR" \
    -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" --target circuitsim_wasm --parallel 2

# Refresh the full package even after an interrupted post-link packaging step,
# or when only JavaScript, JSON, or documentation changed.
cmake -E copy_directory "$REPOSITORY_DIR/visualizer-v2/static" "$BUILD_DIR/pages/simulator"
for artifact in circuitsim_wasm.js circuitsim_wasm.wasm; do
    cmake -E copy "$BUILD_DIR/$artifact" "$BUILD_DIR/pages/simulator/$artifact"
done
for asset in layout.json palette.json; do
    cmake -E copy "$REPOSITORY_DIR/visualizer-v2/$asset" "$BUILD_DIR/pages/simulator/$asset"
done
# Runtime profiles are local state and are intentionally ignored by Git.
cmake -E copy "$REPOSITORY_DIR/visualizer-v2/profiles.defaults.json" "$BUILD_DIR/pages/simulator/profiles.json"
cmake -E copy "$REPOSITORY_DIR/visualizer-v2/static/runtime-config.pages.js" "$BUILD_DIR/pages/simulator/runtime-config.js"

node "$REPOSITORY_DIR/tools/generate-example-rows.mjs" \
    "$BUILD_DIR/pages/simulator/circuitsim_wasm.js" "$BUILD_DIR/pages/simulator/circuitsim_wasm.wasm" \
    "$REPOSITORY_DIR/site-docs" "$BUILD_DIR/pages/simulator/example-rows.json"

CIRCUITSIM_EXAMPLE_ROWS="$BUILD_DIR/pages/simulator/example-rows.json" python3 -m mkdocs build \
    --config-file "$REPOSITORY_DIR/mkdocs.yml" \
    --strict \
    --site-dir "$SITE_DIR"

cmake -E rm -rf "$SITE_DIR/simulator"
cmake -E copy_directory "$BUILD_DIR/pages/simulator" "$SITE_DIR/simulator"
cmake -E touch "$SITE_DIR/.nojekyll"

echo "GitHub Pages site built at $SITE_DIR"
