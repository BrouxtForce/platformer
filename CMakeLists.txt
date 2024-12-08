cmake_minimum_required(VERSION 3.19)

project(Platformer)

add_executable(game
    src/application.cpp
    src/buffer.cpp
    src/camera.cpp
    src/log.cpp
    src/main.cpp
    src/math.cpp
    src/physics.cpp
    src/renderer.cpp
    src/scene.cpp
    src/transform.cpp
    src/utility.cpp
)
target_include_directories(game PUBLIC
    src
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
    	-sUSE_WEBGPU
        -sASYNCIFY
        -sENVIRONMENT=web
        --embed-file ${CMAKE_SOURCE_DIR}/shaders@/shaders)
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

include(FetchContent)

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
