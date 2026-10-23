# coldx.hlsli goes into coldxc, so an effect always compiles against the include of the
# coldx it was built with. Line endings are dropped so every checkout embeds the same text.
set(hlsli_path ${PROJECT_SOURCE_DIR}/shaders/include/coldx.hlsli)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${hlsli_path})
file(READ ${hlsli_path} hlsli)
string(REPLACE "\r" "" hlsli "${hlsli}")
file(CONFIGURE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/generated/coldx_hlsli.h
    CONTENT "// Generated from shaders/include/coldx.hlsli by CMake.\n#pragma once\n\ninline constexpr char ColdxHlsli[] = R\"cxhlsli(@hlsli@)cxhlsli\";\n"
    @ONLY)

add_library(coldx_content STATIC
    build.cpp
    effect_compiler.cpp
    processors.cpp
    project.cpp
    protect_build.cpp
    tool.cpp
    watermark.cpp
    zlib_codec.cpp
)
add_library(coldx::content ALIAS coldx_content)

target_include_directories(coldx_content PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/generated
    ${PROJECT_SOURCE_DIR}/third_party/stb)
target_link_libraries(coldx_content PUBLIC coldx::core PRIVATE SDL3::SDL3-static)
coldx_set_warnings(coldx_content)
# stb's implementation: its warnings are not ours to fix.
set_source_files_properties(zlib_codec.cpp PROPERTIES COMPILE_OPTIONS "$<IF:$<CXX_COMPILER_ID:MSVC>,/W0,-w>")
