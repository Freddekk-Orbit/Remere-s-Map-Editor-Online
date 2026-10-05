include(FetchContent)

set(RME_IMGUI_TAG "v1.91.8" CACHE STRING "Dear ImGui git tag or commit")

if(EXISTS "${CMAKE_SOURCE_DIR}/third_party/imgui/imgui.h")
  set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/third_party/imgui")
  message(STATUS "Using local Dear ImGui at ${IMGUI_DIR}")
else()
  FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG ${RME_IMGUI_TAG}
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(imgui)
  set(IMGUI_DIR "${imgui_SOURCE_DIR}")
  message(STATUS "Fetched Dear ImGui ${RME_IMGUI_TAG} into ${IMGUI_DIR}")
endif()

add_library(rme_imgui STATIC
  ${IMGUI_DIR}/imgui.cpp
  ${IMGUI_DIR}/imgui_demo.cpp
  ${IMGUI_DIR}/imgui_draw.cpp
  ${IMGUI_DIR}/imgui_tables.cpp
  ${IMGUI_DIR}/imgui_widgets.cpp
  ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
  ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(rme_imgui PUBLIC
  ${IMGUI_DIR}
  ${IMGUI_DIR}/backends
)

if(EMSCRIPTEN)
  target_compile_definitions(rme_imgui PUBLIC IMGUI_IMPL_OPENGL_ES3)
endif()

set(IMGUI_DIR "${IMGUI_DIR}" PARENT_SCOPE)
