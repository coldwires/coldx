# The content tool: coldx::content's RunTool with the built-in processors. A game with
# processors of its own builds a tool like this one with them added.
add_executable(coldxc main.cpp)
target_link_libraries(coldxc PRIVATE coldx::content)
coldx_set_warnings(coldxc)
