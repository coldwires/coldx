add_library(coldx_core STATIC
    audio.cpp
    audio_codecs.cpp
    audio_codecs_impl.cpp
    content_manager.cpp
    content_path.cpp
    debug_options.cpp
    game.cpp
    game_component.cpp
    gamepad.cpp
    graphics_device.cpp
    input.cpp
    input_script.cpp
    keyboard.cpp
    log.cpp
    matrix.cpp
    mixer.cpp
    mouse.cpp
    music.cpp
    shader_blob.cpp
    sound_effect.cpp
    sprite_batch.cpp
    sprite_font.cpp
    sprite_renderer.cpp
    stb_truetype_impl.cpp
    storage.cpp
    texture2d.cpp
)
add_library(coldx::core ALIAS coldx_core)

target_compile_features(coldx_core PUBLIC cxx_std_20)
target_include_directories(coldx_core
    PUBLIC ${PROJECT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/shaders/compiled ${PROJECT_SOURCE_DIR}/third_party/stb
        ${PROJECT_SOURCE_DIR}/third_party/dr_libs
)
target_link_libraries(coldx_core PRIVATE SDL3::SDL3-static)
coldx_set_warnings(coldx_core)

# Third-party implementations: their warnings are not ours to fix.
set_source_files_properties(audio_codecs_impl.cpp stb_truetype_impl.cpp PROPERTIES
    COMPILE_OPTIONS "$<IF:$<CXX_COMPILER_ID:MSVC>,/W0,-w>")
