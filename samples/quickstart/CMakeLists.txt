add_executable(quickstart main.cpp)
target_link_libraries(quickstart PRIVATE coldx::core)
coldx_game(quickstart)
coldx_set_warnings(quickstart)

set(font_dir ${PROJECT_SOURCE_DIR}/third_party/fonts/liberation)
add_custom_command(TARGET quickstart POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:quickstart>/content
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${font_dir}/LiberationSans-Regular.ttf ${font_dir}/LICENSE
        $<TARGET_FILE_DIR:quickstart>/content/)
