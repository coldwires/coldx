add_library(coldx_content STATIC
    build.cpp
    processors.cpp
    project.cpp
    protect_build.cpp
    tool.cpp
    watermark.cpp
    zlib_codec.cpp
)
add_library(coldx::content ALIAS coldx_content)

target_include_directories(coldx_content PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/third_party/stb)
target_link_libraries(coldx_content PUBLIC coldx::core PRIVATE SDL3::SDL3-static)
coldx_set_warnings(coldx_content)
# stb's implementation: its warnings are not ours to fix.
set_source_files_properties(zlib_codec.cpp PROPERTIES COMPILE_OPTIONS "$<IF:$<CXX_COMPILER_ID:MSVC>,/W0,-w>")
