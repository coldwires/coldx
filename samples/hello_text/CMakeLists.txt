add_executable(hello_text main.cpp)
target_link_libraries(hello_text PRIVATE coldx::core)
coldx_game(hello_text)
coldx_set_warnings(hello_text)

# The font ships beside the game with its license, as the OFL requires.
set(font_dir ${PROJECT_SOURCE_DIR}/third_party/fonts/liberation)
add_custom_command(TARGET hello_text POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:hello_text>/content
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${font_dir}/LiberationSans-Regular.ttf ${font_dir}/LICENSE
        $<TARGET_FILE_DIR:hello_text>/content/)
