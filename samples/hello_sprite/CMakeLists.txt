add_executable(hello_sprite main.cpp)
target_link_libraries(hello_sprite PRIVATE coldx::core)
coldx_game(hello_sprite)
coldx_set_warnings(hello_sprite)

add_custom_command(TARGET hello_sprite POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/content $<TARGET_FILE_DIR:hello_sprite>/content)
