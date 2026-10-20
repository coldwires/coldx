# Shader compilation needs dxc and spirv-cross, both in the Vulkan SDK. Only regenerating
# shaders needs them; the compiled headers are committed, so a normal build does not.

find_program(COLDX_DXC dxc HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin")
find_program(COLDX_SPIRV_CROSS spirv-cross HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin")

# Adds a target that compiles each .vert.hlsl / .frag.hlsl in SOURCES into a header in
# OUTPUT_DIR, named after the shader (sprite.vert.hlsl -> sprite_vert.h). The target is not
# part of the default build; build it by name after editing a shader.
function(coldx_compile_shaders target)
    cmake_parse_arguments(ARG "" "OUTPUT_DIR" "SOURCES;INCLUDE_DIRS" ${ARGN})
    if(NOT COLDX_DXC OR NOT COLDX_SPIRV_CROSS)
        add_custom_target(${target}
            COMMAND ${CMAKE_COMMAND} -E echo "${target}: dxc and spirv-cross not found. Install the Vulkan SDK and reconfigure."
            COMMAND ${CMAKE_COMMAND} -E false)
        return()
    endif()

    set(include_dir "${COLDX_SHADER_INCLUDE_DIR}")
    set(outputs)
    foreach(source IN LISTS ARG_SOURCES)
        get_filename_component(source "${source}" ABSOLUTE)
        get_filename_component(file_name "${source}" NAME)
        string(REGEX REPLACE "\\.(vert|frag)\\.hlsl$" "_\\1.h" header_name "${file_name}")
        string(MAKE_C_IDENTIFIER "${header_name}" header_name)
        string(REGEX REPLACE "_h$" ".h" header_name "${header_name}")
        set(output "${ARG_OUTPUT_DIR}/${header_name}")
        add_custom_command(
            OUTPUT "${output}"
            COMMAND ${CMAKE_COMMAND}
                "-DDXC=${COLDX_DXC}"
                "-DSPIRV_CROSS=${COLDX_SPIRV_CROSS}"
                "-DSOURCE=${source}"
                "-DOUTPUT=${output}"
                "-DINCLUDE_DIR=${include_dir}"
                "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/${target}"
                -P "${COLDX_SHADER_SCRIPT}"
            DEPENDS "${source}" "${include_dir}/coldx.hlsli" "${COLDX_SHADER_SCRIPT}"
            COMMENT "Compiling ${file_name}"
            VERBATIM)
        list(APPEND outputs "${output}")
    endforeach()
    add_custom_target(${target} DEPENDS ${outputs})
endfunction()

get_filename_component(COLDX_SHADER_INCLUDE_DIR "${CMAKE_CURRENT_LIST_DIR}/../shaders/include" ABSOLUTE CACHE)
set(COLDX_SHADER_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/compile_shader.cmake" CACHE INTERNAL "")
