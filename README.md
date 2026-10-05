# Remere's Map Editor (Online)

WebAssembly port of [Remere's Map Editor](https://github.com/opentibiabr/remeres-map-editor/) — C++ map core in the browser via Emscripten, Dear ImGui, and WebGL2.

This repository starts at **Phase 1: Build Pipeline & UI Decoupling**. The original desktop editor is a wxWidgets + OpenGL application. The browser target removes that windowing stack and boots an ImGui render loop instead.

## Phase 1 status

- Emscripten CMake configuration (`emcmake cmake`)
- Dear ImGui with SDL2 + OpenGL ES 3.0 / WebGL2 backends
- `src/main_wasm.cpp` entry point using `emscripten_set_main_loop()` (not `wxApp::OnRun()`)
- Virtual filesystem: MEMFS (`/uploads`, `/assets`) and IDBFS (`/persist`)
- Browser file picker + drag-and-drop for `.otbm`, `.dat`, `.spr`, and XML
- Optional HTTP FETCH into `/assets`
- wxWidgets include stubs under `src/wx_stub` so later core imports do not pull the desktop toolkit

Map algorithms, item definitions, floor drawing, and undo/redo are not imported yet. They stay in C++ and will land against this interface layer.

## Repository layout

```
CMakeLists.txt            Dual-target build (emcmake → Wasm, cmake → native ImGui preview)
cmake/FetchImGui.cmake    Pins Dear ImGui (or uses third_party/imgui)
src/main_wasm.cpp         Browser / preview entry + ImGui loop
src/platform/platform.h   RME_PLATFORM_WASM / RME_USE_WXWIDGETS switches
src/wasm/                 MEMFS / IDBFS / FETCH bridge
src/wx_stub/              Drop-in replacements for #include <wx/...>
web/shell.html            HTML5 canvas, file input, drag-and-drop
scripts/build_wasm.sh     Configure + build helper
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
