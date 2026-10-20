add_executable(stress main.cpp)
target_link_libraries(stress PRIVATE coldx::core)
coldx_game(stress)
coldx_set_warnings(stress)
