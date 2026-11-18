add_library(coldx_core STATIC
    audio.cpp
    audio_codecs.cpp
    audio_codecs_impl.cpp
    chacha20.cpp
    content_manager.cpp
    content_path.cpp
    cpu_backend.cpp
    cpu_effect.cpp
    cpu_math.cpp
    cpu_sprite_renderer.cpp
    debug_options.cpp
    effect.cpp
    effect_format.cpp
    game.cpp
    game_component.cpp
    gamepad.cpp
    gpu_backend.cpp
    gpu_download.cpp
    gpu_sprite_renderer.cpp
    graphics_device.cpp
    input.cpp
    input_script.cpp
    keyboard.cpp
    log.cpp
    matrix.cpp
    mixer.cpp
    mouse.cpp
    rime.cpp
    rime_package.cpp
    scaling.cpp
    music.cpp
    png_file.cpp
    shader_blob.cpp
    sound_effect.cpp
    sprite_batch.cpp
    sprite_font.cpp
    stb_truetype_impl.cpp
    storage.cpp
    text_input.cpp
    worker_pool.cpp
    texture2d.cpp
    ${PROJECT_SOURCE_DIR}/third_party/lz4/lz4/lz4.c
    ${PROJECT_SOURCE_DIR}/third_party/lz4/lz4/lz4hc.c
)
add_library(coldx::core ALIAS coldx_core)

target_compile_features(coldx_core PUBLIC cxx_std_20)
target_include_directories(coldx_core
    PUBLIC ${PROJECT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/shaders/compiled ${PROJECT_SOURCE_DIR}/third_party/stb
        ${PROJECT_SOURCE_DIR}/third_party/dr_libs ${PROJECT_SOURCE_DIR}/third_party/lz4
)
target_link_libraries(coldx_core PRIVATE SDL3::SDL3-static)
if(NOT COLDX_DEFAULT_RENDERER STREQUAL "gpu" AND NOT COLDX_DEFAULT_RENDERER STREQUAL "cpu")
    message(FATAL_ERROR "COLDX_DEFAULT_RENDERER is '${COLDX_DEFAULT_RENDERER}': use gpu or cpu")
endif()
target_compile_definitions(coldx_core PRIVATE COLDX_DEFAULT_RENDERER="${COLDX_DEFAULT_RENDERER}")
coldx_set_warnings(coldx_core)
# The CPU renderer and CpuMath give the same bits on every machine only if a * b + c is never
# fused into one rounding. MSVC does not fuse by default. GCC and Clang may.
if(NOT MSVC)
    target_compile_options(coldx_core PRIVATE -ffp-contract=off)
endif()
if(COLDX_HARDEN)
    coldx_harden(coldx_core)
endif()

# Third-party implementations: their warnings are not ours to fix.
set_source_files_properties(audio_codecs_impl.cpp stb_truetype_impl.cpp PROPERTIES
    COMPILE_OPTIONS "$<IF:$<CXX_COMPILER_ID:MSVC>,/W0,-w>")
set_source_files_properties(${PROJECT_SOURCE_DIR}/third_party/lz4/lz4/lz4.c ${PROJECT_SOURCE_DIR}/third_party/lz4/lz4/lz4hc.c PROPERTIES
    COMPILE_OPTIONS "$<IF:$<C_COMPILER_ID:MSVC>,/W0,-w>")
