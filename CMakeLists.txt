cmake_minimum_required(VERSION 3.24)
project(coldx VERSION 0.1.0 LANGUAGES C CXX)

option(COLDX_BUILD_SAMPLES "Build the sample games" ${PROJECT_IS_TOP_LEVEL})

# SDL builds from the vendored source as a static library and nothing else.
set(SDL_SHARED OFF CACHE BOOL "")
set(SDL_STATIC ON CACHE BOOL "")
set(SDL_TEST_LIBRARY OFF CACHE BOOL "")
set(SDL_TESTS OFF CACHE BOOL "")
set(SDL_EXAMPLES OFF CACHE BOOL "")
set(SDL_INSTALL OFF CACHE BOOL "")
add_subdirectory(third_party/SDL EXCLUDE_FROM_ALL)

# Warnings apply to coldx's own targets only, never to third_party.
function(coldx_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra)
    endif()
endfunction()

# Makes target a windowed game: no console on Windows, which coldx opens itself when
# asked (see GameSettings::console). The entry point stays main() on every platform.
function(coldx_game target)
    if(WIN32)
        set_target_properties(${target} PROPERTIES WIN32_EXECUTABLE TRUE)
        if(MSVC)
            target_link_options(${target} PRIVATE /ENTRY:mainCRTStartup)
        endif()
    endif()
endfunction()

include(cmake/coldx_shaders.cmake)

# Regenerates the committed headers in shaders/compiled after a built-in shader changes:
#   cmake --build build --target coldx_builtin_shaders
coldx_compile_shaders(coldx_builtin_shaders
    OUTPUT_DIR ${PROJECT_SOURCE_DIR}/shaders/compiled
    SOURCES
        shaders/src/sprite.vert.hlsl
        shaders/src/sprite.frag.hlsl
)

add_subdirectory(src/core)

if(COLDX_BUILD_SAMPLES)
    add_subdirectory(samples/hello_sprite)
endif()
