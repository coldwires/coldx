add_executable(demo main.cpp sheet.h)
target_link_libraries(demo PRIVATE coldx::core)
coldx_game(demo)
coldx_set_warnings(demo)

# The sheet and sounds are made for coldx; the music is hello_audio's; the font ships
# with its license.
set(font_dir ${PROJECT_SOURCE_DIR}/third_party/fonts/liberation)
add_custom_command(TARGET demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:demo>/content
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/content/sheet.png
        ${CMAKE_CURRENT_SOURCE_DIR}/content/pickup.wav
        ${CMAKE_CURRENT_SOURCE_DIR}/content/hit.wav
        ${PROJECT_SOURCE_DIR}/samples/hello_audio/content/loop.ogg
        ${font_dir}/LiberationSans-Regular.ttf ${font_dir}/LICENSE
        $<TARGET_FILE_DIR:demo>/content/)
