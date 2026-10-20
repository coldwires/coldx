cmake_minimum_required(VERSION 3.24)
project(coldx VERSION 0.1.0 LANGUAGES C CXX)

option(COLDX_BUILD_SAMPLES "Build the sample games" ${PROJECT_IS_TOP_LEVEL})
option(COLDX_BUILD_TESTS "Build the tests and register them with CTest" ${PROJECT_IS_TOP_LEVEL})

# Render tests run once per backend listed here.
if(WIN32)
    set(default_drivers direct3d12 vulkan)
elseif(APPLE)
    set(default_drivers metal)
else()
    set(default_drivers vulkan)
endif()
set(COLDX_TEST_GPU_DRIVERS "${default_drivers}" CACHE STRING "GPU backends the render tests run on")

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

if(COLDX_BUILD_TESTS)
    enable_testing()
    add_subdirectory(src/core/tests)
    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_FOUND)
        add_test(NAME check COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/check.py)
        set_tests_properties(check PROPERTIES TIMEOUT 60)
    endif()
endif()

if(COLDX_BUILD_SAMPLES)
    add_subdirectory(samples/demo)
    add_subdirectory(samples/hello_audio)
    add_subdirectory(samples/hello_sprite)
    add_subdirectory(samples/hello_text)
    add_subdirectory(samples/quickstart)
    add_subdirectory(samples/stress)
endif()
