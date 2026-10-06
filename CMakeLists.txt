cmake_minimum_required(VERSION 3.16)

project(rme_wasm LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# emcmake defines EMSCRIPTEN. Native CMake can still build a desktop ImGui preview.
if(EMSCRIPTEN)
  set(RME_WASM_BUILD ON)
  message(STATUS "Configuring Remere's Map Editor for Emscripten / WebAssembly")
else()
  set(RME_WASM_BUILD OFF)
  message(STATUS "Configuring native ImGui preview (use emcmake cmake ... for the browser target)")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(FetchImGui)

set(RME_CORE_SOURCES
  src/rme/core/filehandle.cpp
  src/rme/core/map.cpp
  src/rme/core/otbm.cpp
  src/rme/core/action.cpp
  src/rme/core/items_dat.cpp
  src/rme/core/sprites.cpp
  src/rme/core/brush.cpp
  src/rme/core/minimap.cpp
  src/rme/core/map_xml.cpp
  src/rme/core/session.cpp
)

set(RME_GFX_SOURCES
  src/rme/gfx/sprite_atlas.cpp
)

add_executable(rme_wasm
  src/main_wasm.cpp
  src/wasm/asset_bridge.cpp
  src/wasm/wx_compat_sanity.cpp
  ${RME_CORE_SOURCES}
  ${RME_GFX_SOURCES}
)

target_include_directories(rme_wasm PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/src/rme/core
  ${CMAKE_CURRENT_SOURCE_DIR}/src/wx_stub
)

target_compile_definitions(rme_wasm PRIVATE
  RME_USE_IMGUI=1
  RME_USE_WXWIDGETS=0
)

if(RME_WASM_BUILD)
  target_compile_definitions(rme_wasm PRIVATE RME_PLATFORM_WASM=1)
else()
  target_compile_definitions(rme_wasm PRIVATE RME_PLATFORM_WASM=0)
endif()

target_link_libraries(rme_wasm PRIVATE rme_imgui)

if(RME_WASM_BUILD)
  # Keep -s flags glued together. CMake de-duplicates bare "-s" tokens.
  set(RME_EMSCRIPTEN_COMMON
    -sUSE_SDL=2
  )

  target_compile_options(rme_imgui PUBLIC ${RME_EMSCRIPTEN_COMMON})
  target_compile_definitions(rme_imgui PUBLIC IMGUI_IMPL_OPENGL_ES3)

  target_compile_options(rme_wasm PRIVATE
    ${RME_EMSCRIPTEN_COMMON}
    -fexceptions
  )

  target_link_options(rme_wasm PRIVATE
    ${RME_EMSCRIPTEN_COMMON}
    -sWASM=1
    -sUSE_WEBGL2=1
    -sMIN_WEBGL_VERSION=2
    -sMAX_WEBGL_VERSION=2
    -sFULL_ES3=1
    -sALLOW_MEMORY_GROWTH=1
    -sFORCE_FILESYSTEM=1
    -sFETCH=1
    -sNO_EXIT_RUNTIME=1
    -sASSERTIONS=1
    -sDISABLE_EXCEPTION_CATCHING=0
    -lidbfs.js
    "--shell-file=${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
    "-sEXPORTED_FUNCTIONS=['_main','_rme_on_file_uploaded','_rme_on_idbfs_ready']"
    "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','FS','HEAPU8']"
  )

  set_target_properties(rme_wasm PROPERTIES
    OUTPUT_NAME index
    SUFFIX ".html"
  )
else()
  find_package(SDL2 REQUIRED)
  find_package(OpenGL REQUIRED)
  target_link_libraries(rme_wasm PRIVATE SDL2::SDL2 OpenGL::GL)

  add_executable(rme_core_test
    src/rme/core/core_test.cpp
    ${RME_CORE_SOURCES}
  )
  target_include_directories(rme_core_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rme/core
  )
endif()

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES
  src/main_wasm.cpp
  src/wasm/asset_bridge.cpp
  src/wasm/asset_bridge.h
  src/wasm/wx_compat_sanity.cpp
  src/platform/platform.h
)
