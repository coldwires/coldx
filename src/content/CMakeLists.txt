add_library(coldx_content STATIC
    build.cpp
    processors.cpp
    project.cpp
    protect_build.cpp
    tool.cpp
)
add_library(coldx::content ALIAS coldx_content)

target_include_directories(coldx_content PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(coldx_content PUBLIC coldx::core PRIVATE SDL3::SDL3-static)
coldx_set_warnings(coldx_content)
