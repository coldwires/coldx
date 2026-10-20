add_executable(hello_sprite main.cpp)
target_link_libraries(hello_sprite PRIVATE coldx::core)
coldx_game(hello_sprite)
coldx_set_warnings(hello_sprite)
