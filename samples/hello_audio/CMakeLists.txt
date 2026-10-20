add_executable(hello_audio main.cpp)
target_link_libraries(hello_audio PRIVATE coldx::core)
coldx_game(hello_audio)
coldx_set_warnings(hello_audio)

# loop.ogg is generated for coldx (see third_party/VERSIONS.md); the font ships with its license.
set(font_dir ${PROJECT_SOURCE_DIR}/third_party/fonts/liberation)
add_custom_command(TARGET hello_audio POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:hello_audio>/content
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${font_dir}/LiberationSans-Regular.ttf ${font_dir}/LICENSE
        ${CMAKE_CURRENT_SOURCE_DIR}/content/loop.ogg
        $<TARGET_FILE_DIR:hello_audio>/content/)
