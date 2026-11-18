cmake_minimum_required(VERSION 3.24)
project(coldx VERSION 0.1.0 LANGUAGES C CXX)

option(COLDX_BUILD_SAMPLES "Build the sample games" ${PROJECT_IS_TOP_LEVEL})
option(COLDX_BUILD_TESTS "Build the tests and register them with CTest" ${PROJECT_IS_TOP_LEVEL})
# For cross-compiling to a target that ships its own SDL and runs no build tools.
option(COLDX_SYSTEM_SDL "Use an installed SDL 3.4 (find_package) instead of third_party/SDL" OFF)
option(COLDX_BUILD_TOOLS
       "Build the content module and coldxc, and what needs them at build time (the demo, packed-content tests)" ON)
set(COLDX_DEFAULT_RENDERER "gpu" CACHE STRING "Renderer when COLDX_RENDERER is not set: gpu or cpu")
set_property(CACHE COLDX_DEFAULT_RENDERER PROPERTY STRINGS gpu cpu)

# Render tests run once per backend listed here.
if(WIN32)
    set(default_drivers direct3d12 vulkan)
elseif(APPLE)
    set(default_drivers metal)
else()
    set(default_drivers vulkan)
endif()
set(COLDX_TEST_GPU_DRIVERS "${default_drivers}" CACHE STRING "GPU backends the render tests run on")

include(cmake/coldx_harden.cmake)
# Hardened release builds keep the build machine's folders out of source paths compiled
# into the code (SDL's error checks name their files), SDL's included.
if(COLDX_HARDEN AND MSVC)
    file(TO_NATIVE_PATH "${PROJECT_SOURCE_DIR}/" coldx_source_prefix)
    add_compile_options("$<$<CONFIG:Release>:/d1trimfile:${coldx_source_prefix}>")
elseif(COLDX_HARDEN AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # No RTTI for every C++ file, tests and samples included: GCC and Clang put a class's
    # type information in the file with its first virtual function, so code compiled with
    # RTTI that derives from a coldx class (a Game) needs coldx's, which -fno-rtti never made.
    add_compile_options("$<$<CONFIG:Release>:-ffile-prefix-map=${PROJECT_SOURCE_DIR}/=>"
                        "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:CXX>>:-fno-rtti>")
endif()

# SDL builds from the vendored source as a static library and nothing else.
set(SDL_SHARED OFF CACHE BOOL "")
set(SDL_STATIC ON CACHE BOOL "")
set(SDL_TEST_LIBRARY OFF CACHE BOOL "")
set(SDL_TESTS OFF CACHE BOOL "")
set(SDL_EXAMPLES OFF CACHE BOOL "")
set(SDL_INSTALL OFF CACHE BOOL "")
if(COLDX_SYSTEM_SDL)
    find_package(SDL3 3.4 REQUIRED CONFIG)
else()
    add_subdirectory(third_party/SDL EXCLUDE_FROM_ALL)
endif()

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
    if(COLDX_HARDEN)
        coldx_harden(${target})
    endif()
    if(WIN32)
        set_target_properties(${target} PROPERTIES WIN32_EXECUTABLE TRUE)
        if(MSVC)
            target_link_options(${target} PRIVATE /ENTRY:mainCRTStartup)
        endif()
    endif()
endfunction()

include(cmake/coldx_shaders.cmake)
include(cmake/coldx_content.cmake)

# Regenerates the committed headers in shaders/compiled after a built-in shader changes:
#   cmake --build build --target coldx_builtin_shaders
coldx_compile_shaders(coldx_builtin_shaders
    OUTPUT_DIR ${PROJECT_SOURCE_DIR}/shaders/compiled
    SOURCES
        shaders/src/sprite.vert.hlsl
        shaders/src/sprite.frag.hlsl
)

add_subdirectory(src/core)
if(COLDX_BUILD_TOOLS)
    add_subdirectory(src/content)
    add_subdirectory(tools/coldxc)
endif()
add_subdirectory(src/protect)

# Regenerates the committed headers of the example effects and the effects the tests use:
#   cmake --build build --target coldx_example_effects coldx_test_effects
coldx_compile_effects(coldx_example_effects
    OUTPUT_DIR ${PROJECT_SOURCE_DIR}/shaders/examples/compiled
    SOURCES
        shaders/examples/grayscale.hlsl
        shaders/examples/flash.hlsl
        shaders/examples/wave.hlsl
)
coldx_compile_effects(coldx_test_effects
    OUTPUT_DIR ${PROJECT_SOURCE_DIR}/tests/effects/compiled
    SOURCES tests/effects/params.hlsl
)

if(COLDX_BUILD_TESTS)
    enable_testing()
    add_subdirectory(src/core/tests)
    add_subdirectory(src/protect/tests)
    add_subdirectory(tests/framework)
    if(COLDX_BUILD_TOOLS)
        add_subdirectory(src/content/tests)
        add_subdirectory(tests/protected_game)
    endif()
    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_FOUND)
        add_test(NAME check COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/check.py)
        set_tests_properties(check PROPERTIES TIMEOUT 60)
    endif()
endif()

if(COLDX_BUILD_SAMPLES)
    if(COLDX_BUILD_TOOLS)
        add_subdirectory(samples/demo)  # packs its content with coldxc at build time
    endif()
    add_subdirectory(samples/hello_audio)
    add_subdirectory(samples/hello_sprite)
    add_subdirectory(samples/hello_text)
    add_subdirectory(samples/quickstart)
    add_subdirectory(samples/stress)
endif()
