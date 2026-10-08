# Remere's Map Editor (Online)

WebAssembly port of [Remere's Map Editor](https://github.com/opentibiabr/remeres-map-editor/) — C++ map core in the browser via Emscripten, Dear ImGui, and WebGL2.

This repository is a phased WebAssembly port. The original desktop editor is a wxWidgets + OpenGL application. The browser target removes that windowing stack and boots an ImGui render loop instead.

## Phase 12 status

- Door, table, and carpet brushes from `materials.xml`. Auto on those items resolves to the matching tool. Shift+click removes the family
- Tables restitch like walls (single / h / v / join). Carpets restitch inner, 4 edges, and 4 corners. Doors pick horizontal vs vertical from nearby walls
- Sample `Tibia.spr` is 33 original 32×32 drawings (seamless grass/dirt/water, wood furniture, red carpet). It is not a client file; drop your own `.spr` / `.dat` to use real graphics
- Host test: `rme_core_test` covers door-on-wall, table restitch, 2×2 carpet corners, and invert undo

Phase 1–11 remain underneath.

## Repository layout

```
CMakeLists.txt            Dual-target build (emcmake → Wasm, cmake → native ImGui preview)
cmake/FetchImGui.cmake    Pins Dear ImGui (or uses third_party/imgui)
src/main_wasm.cpp         Browser / preview entry + ImGui loop
src/rme/core/             OTBM / DAT / SPR / brushes / minimap / houses / spawns / undo-redo (no ImGui, no GL)
src/rme/gfx/              WebGL2 sprite atlas (UI-side only)
src/platform/platform.h   RME_PLATFORM_WASM / RME_USE_WXWIDGETS switches
src/wasm/                 MEMFS / IDBFS / FETCH bridge
src/wx_stub/              Drop-in replacements for #include <wx/...>
web/shell.html            HTML5 canvas, file input, drag-and-drop
scripts/build_wasm.sh     Configure + build helper
```

Native core test (no Emscripten, no SDL):

```bash
g++ -std=c++20 -I src -I src/rme/core src/rme/core/*.cpp -o rme_core_test
./rme_core_test
```

## Building the browser target

1. Install the [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html) and source `emsdk_env.sh`.
2. From the repository root:

```bash
./scripts/build_wasm.sh RelWithDebInfo
```

Or manually:

```bash
mkdir -p build-wasm
emcmake cmake -S . -B build-wasm -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build-wasm --parallel
```

3. Serve the output (Wasm fetch is blocked from `file://`):

```bash
python3 -m http.server --directory build-wasm 8080
```

Open `http://localhost:8080/index.html`. Drop a `.otbm` / `.dat` / `.spr` onto the page or use **File → Upload**.

Linker flags for the Wasm target include `-s WASM=1 -s USE_WEBGL2=1 -s ALLOW_MEMORY_GROWTH=1`, plus SDL2, FETCH, and IDBFS.

## Native ImGui preview

A desktop preview of the same ImGui shell (no Emscripten) can be configured when SDL2 and OpenGL are available:

```bash
cmake -S . -B build-native -DCMAKE_BUILD_TYPE=Debug
cmake --build build-native --parallel
```

## wxWidgets decoupling

New UI is ImGui-only. Upstream files that still `#include <wx/...>` are expected to resolve through `src/wx_stub` (`-I src/wx_stub`) until those call sites are replaced. Do not add new wxWidgets widgets.

The stub is intentionally small: `wxString`, `wxFileName`, window/event macros, and enough types that RME headers can be compiled incrementally. It is not a wxWidgets implementation.

## Asset paths

| VFS path    | Backend | Role                                      |
|-------------|---------|-------------------------------------------|
| `/uploads`  | MEMFS   | Files chosen or dropped in the browser    |
| `/assets`   | MEMFS   | Fetched or preloaded client data          |
| `/persist`  | IDBFS   | Survives reloads after `FS.syncfs`        |

Once a file is in the VFS, existing `FILE*` / `fopen` readers (for example RME `FileReadHandle`) can use the virtual path without desktop disk I/O.

## Upstream

Desktop sources: https://github.com/opentibiabr/remeres-map-editor/

RME is licensed under the GNU GPL v3. This port follows the same terms.
