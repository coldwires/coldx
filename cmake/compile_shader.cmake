# Compiles one HLSL shader to SPIR-V, DXIL and MSL and writes them into a C++ header,
# along with the resource counts SDL_CreateGPUShader needs. Run with cmake -P:
#
#   cmake -DDXC=... -DSPIRV_CROSS=... -DSOURCE=x.vert.hlsl -DOUTPUT=x_vert.h
#         -DINCLUDE_DIR=... -DWORK_DIR=... -P compile_shader.cmake
#
# The stage comes from the file name: *.vert.hlsl or *.frag.hlsl. Entry point is main.

foreach(var DXC SPIRV_CROSS SOURCE OUTPUT INCLUDE_DIR WORK_DIR)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "compile_shader.cmake: ${var} is not set")
    endif()
endforeach()

get_filename_component(file_name "${SOURCE}" NAME)
if(file_name MATCHES "^(.+)\\.vert\\.hlsl$")
    set(profile vs_6_0)
    set(stage vert)
elseif(file_name MATCHES "^(.+)\\.frag\\.hlsl$")
    set(profile ps_6_0)
    set(stage frag)
else()
    message(FATAL_ERROR "${SOURCE}: name must end in .vert.hlsl or .frag.hlsl")
endif()
set(name "${CMAKE_MATCH_1}_${stage}")
string(MAKE_C_IDENTIFIER "${name}" name)

file(MAKE_DIRECTORY "${WORK_DIR}")
set(spirv "${WORK_DIR}/${name}.spv")
set(dxil "${WORK_DIR}/${name}.dxil")
set(msl "${WORK_DIR}/${name}.msl")

function(run)
    execute_process(COMMAND ${ARGN} RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "${SOURCE}: failed:\n${ARGN}\n${out}${err}")
    endif()
endfunction()

run("${DXC}" -T ${profile} -E main -spirv -fspv-target-env=vulkan1.0 -I "${INCLUDE_DIR}" "${SOURCE}" -Fo "${spirv}")
run("${DXC}" -T ${profile} -E main -I "${INCLUDE_DIR}" "${SOURCE}" -Fo "${dxil}")
run("${SPIRV_CROSS}" "${spirv}" --msl --output "${msl}")

execute_process(COMMAND "${SPIRV_CROSS}" "${spirv}" --reflect
                RESULT_VARIABLE result OUTPUT_VARIABLE reflection ERROR_VARIABLE err)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "${SOURCE}: reflection failed: ${err}")
endif()

# A missing key means none of that kind.
function(count_of key out)
    string(JSON n ERROR_VARIABLE missing LENGTH "${reflection}" ${key})
    if(missing)
        set(n 0)
    endif()
    set(${out} ${n} PARENT_SCOPE)
endfunction()
count_of(textures samplers)
count_of(images storage_textures)
count_of(ssbos storage_buffers)
count_of(ubos uniform_buffers)

function(byte_array path out)
    file(READ "${path}" hex HEX)
    string(LENGTH "${hex}" hex_length)
    math(EXPR size "${hex_length} / 2")
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${hex}")
    string(REGEX REPLACE "((0x..,){16})" "\\1\n    " bytes "${bytes}")
    set(${out} "${bytes}" PARENT_SCOPE)
    set(${out}_size ${size} PARENT_SCOPE)
endfunction()
byte_array("${spirv}" spirv_bytes)
byte_array("${dxil}" dxil_bytes)
byte_array("${msl}" msl_bytes)

set(header "// Generated from ${file_name} by cmake/compile_shader.cmake. Do not edit.
#pragma once

#include \"shader_blob.h\"

namespace coldx::shaders {

inline constexpr unsigned char ${name}_spirv[] = {
    ${spirv_bytes}
};

inline constexpr unsigned char ${name}_dxil[] = {
    ${dxil_bytes}
};

inline constexpr unsigned char ${name}_msl[] = {
    ${msl_bytes}
};

inline constexpr ShaderBlob ${name}{
    ${name}_spirv, ${spirv_bytes_size},
    ${name}_dxil, ${dxil_bytes_size},
    ${name}_msl, ${msl_bytes_size},
    ${samplers}, ${storage_textures}, ${storage_buffers}, ${uniform_buffers},
};

} // namespace coldx::shaders
")
string(REGEX REPLACE "[ ]+\n" "\n" header "${header}")

# Rewriting an unchanged header would rebuild everything that includes it.
if(EXISTS "${OUTPUT}")
    file(READ "${OUTPUT}" existing)
    if(existing STREQUAL header)
        return()
    endif()
endif()
file(WRITE "${OUTPUT}" "${header}")
message(STATUS "Wrote ${OUTPUT}")
