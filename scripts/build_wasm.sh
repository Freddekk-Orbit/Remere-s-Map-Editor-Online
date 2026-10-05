#!/usr/bin/env bash
# Configure and build the Emscripten target.
# Prerequisites: emsdk sourced (emcmake / emcc on PATH).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${RME_BUILD_DIR:-$ROOT/build-wasm}"
BUILD_TYPE="${1:-RelWithDebInfo}"

if ! command -v emcmake >/dev/null 2>&1; then
  echo "emcmake not found. Install the Emscripten SDK and run:" >&2
  echo "  source /path/to/emsdk/emsdk_env.sh" >&2
  exit 1
fi

mkdir -p "$BUILD_DIR"
emcmake cmake -S "$ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
cmake --build "$BUILD_DIR" --parallel

echo
echo "Build artifacts:"
echo "  $BUILD_DIR/index.html"
echo "  $BUILD_DIR/index.js"
echo "  $BUILD_DIR/index.wasm"
echo
echo "Serve with:"
echo "  python3 -m http.server --directory $BUILD_DIR 8080"
