cmake_minimum_required(VERSION 3.19)

project(Platformer)

include(FetchContent)
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG b4c96355c9b51b54c4deb52e7d7cdfc7bf79bc2f
)
FetchContent_MakeAvailable(imgui)

add_executable(game
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_wgpu.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp

    src/application.cpp
    src/buffer.cpp
    src/camera.cpp
    src/data-structures.cpp
    src/font-atlas.cpp
    src/input.cpp
    src/jump-flood.cpp
    src/lighting.cpp
    src/log.cpp
    src/main.cpp
    src/math.cpp
    src/memory-arena.cpp
    src/menu.cpp
    src/physics.cpp
    src/player.cpp
    src/renderer.cpp
    src/scene.cpp
    src/shader-library.cpp
    src/transform.cpp
    src/utility.cpp
)
target_include_directories(game PUBLIC
    src
    include
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${imgui_SOURCE_DIR}/misc/cpp
)

target_compile_definitions(game PRIVATE
    IMGUI_STB_TRUETYPE_FILENAME="stb/stb_truetype.h"
    IMGUI_STB_RECT_PACK_FILENAME="stb/stb_rect_pack.h"
    IMGUI_DISABLE_STB_TRUETYPE_IMPLEMENTATION
    IMGUI_DISABLE_STB_RECT_PACK_IMPLEMENTATION
)

set_target_properties(game PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

if (MSVC)
    target_compile_options(game PRIVATE /W4)
else()
    target_compile_options(game PRIVATE -Wall -Wextra -pedantic)
endif()

if (EMSCRIPTEN)
	target_link_options(game PRIVATE
	    -O3
    	-sUSE_WEBGPU
        -sASYNCIFY
        -sENVIRONMENT=web
        --embed-file ${CMAKE_SOURCE_DIR}/shaders@/shaders
        --preload-file ${CMAKE_SOURCE_DIR}/assets@/assets
    )
    target_compile_options(game PRIVATE -O3)
else()
    set(WEBGPU_BACKEND "DAWN")
    target_compile_definitions(game PRIVATE IMGUI_IMPL_WEBGPU_BACKEND_WGPU)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(game PRIVATE DEBUG=1)
endif()

if(APPLE)
    enable_language(OBJC)
endif()

# Disable Shared Libs
set(BUILD_SHARED_LIBS OFF)
set(SDL_SHARED OFF)
set(SDL_STATIC ON)

FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG preview-3.1.6
)

FetchContent_MakeAvailable(SDL3)

target_link_libraries(game PUBLIC SDL3::SDL3-static)

add_subdirectory(WebGPU-distribution)
target_link_libraries(game PRIVATE webgpu)
target_copy_webgpu_binaries(game)

add_subdirectory(sdl3webgpu)
target_link_libraries(game PRIVATE sdl3webgpu)

option(BUILD_TESTS "Build tests" OFF)
if (BUILD_TESTS)
    add_executable(test
        src/data-structures.cpp
        src/log.cpp
        src/memory-arena.cpp

        tests/array.test.cpp
        tests/memory-arena.test.cpp
        tests/string.test.cpp
        tests/test.cpp
    )
    target_include_directories(test PUBLIC
        src
        include/doctest
    )
    set_target_properties(test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )
endif()
