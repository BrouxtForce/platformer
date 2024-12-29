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
    src/font-atlas.cpp
    src/input.cpp
    src/lighting.cpp
    src/log.cpp
    src/main.cpp
    src/math.cpp
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
        --preload-file ${CMAKE_SOURCE_DIR}/assets/fonts/Roboto/Roboto-Regular.ttf@/assets/fonts/Roboto/Roboto-Regular.ttf
    )
    target_compile_options(game PRIVATE -O3)
else()
    target_compile_definitions(game PRIVATE IMGUI_IMPL_WEBGPU_BACKEND_WGPU)
endif()

if(APPLE)
    enable_language(OBJC)
endif()

# Disable Shared Libs
set(BUILD_SHARED_LIBS OFF)
set(SDL_SHARED OFF)
set(SDL_STATIC ON)

# Disable AVIF Support
set(SDL3IMAGE_AVIF OFF)

FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG preview-3.1.6
)

FetchContent_MakeAvailable(SDL3)

FetchContent_Declare(
    SDL3_image
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
    #GIT_TAG release-2.8.2
    GIT_TAG origin/main
)

set(SDL3IMAGE_INSTALL OFF)
set(SDL3IMAGE_BUILD_SHARED_LIBS OFF)

FetchContent_MakeAvailable(SDL3_image)

target_link_libraries(game PUBLIC SDL3::SDL3-static SDL3_image::SDL3_image-static)

add_subdirectory(WebGPU-distribution)
target_link_libraries(game PRIVATE webgpu)
target_copy_webgpu_binaries(game)

add_subdirectory(sdl3webgpu)
target_link_libraries(game PRIVATE sdl3webgpu)
