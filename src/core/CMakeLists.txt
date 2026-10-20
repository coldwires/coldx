add_library(coldx_core STATIC
    content_path.cpp
    debug_options.cpp
    game.cpp
    graphics_device.cpp
    keyboard.cpp
    log.cpp
    matrix.cpp
    shader_blob.cpp
    sprite_batch.cpp
    sprite_font.cpp
    sprite_renderer.cpp
    stb_truetype_impl.cpp
    texture2d.cpp
)
add_library(coldx::core ALIAS coldx_core)

target_compile_features(coldx_core PUBLIC cxx_std_20)
target_include_directories(coldx_core
    PUBLIC ${PROJECT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/shaders/compiled ${PROJECT_SOURCE_DIR}/third_party/stb
)
target_link_libraries(coldx_core PRIVATE SDL3::SDL3-static)
coldx_set_warnings(coldx_core)
