add_executable(coldx_core_math_test math_test.cpp)
target_link_libraries(coldx_core_math_test PRIVATE coldx::core)
target_include_directories(coldx_core_math_test PRIVATE ${PROJECT_SOURCE_DIR}/tests)
coldx_set_warnings(coldx_core_math_test)
add_test(NAME core.math COMMAND coldx_core_math_test)

# Tests that draw run headless, once per backend in COLDX_TEST_GPU_DRIVERS, and once on the
# CPU renderer unless NO_CPU is given.
function(coldx_core_render_test name source)
    cmake_parse_arguments(PARSE_ARGV 2 arg "NO_CPU" "" "")
    add_executable(coldx_core_${name}_test ${source})
    target_link_libraries(coldx_core_${name}_test PRIVATE coldx::core SDL3::SDL3-static)
    target_include_directories(coldx_core_${name}_test PRIVATE ${PROJECT_SOURCE_DIR}/tests)
    coldx_set_warnings(coldx_core_${name}_test)
    foreach(driver IN LISTS COLDX_TEST_GPU_DRIVERS)
        add_test(NAME core.${name}.${driver} COMMAND coldx_core_${name}_test)
        set_tests_properties(core.${name}.${driver} PROPERTIES ENVIRONMENT "SDL_GPU_DRIVER=${driver}")
    endforeach()
    if(NOT arg_NO_CPU)
        add_test(NAME core.${name}.cpu COMMAND coldx_core_${name}_test)
        set_tests_properties(core.${name}.cpu PROPERTIES ENVIRONMENT "COLDX_RENDERER=cpu")
    endif()
endfunction()

coldx_core_render_test(render render_test.cpp)
coldx_core_render_test(sprite sprite_test.cpp)
coldx_core_render_test(font font_test.cpp)
coldx_core_render_test(scaling scaling_test.cpp)
coldx_core_render_test(readback readback_test.cpp)
# Effects need their CPU versions, which are not written yet.
coldx_core_render_test(effect effect_test.cpp NO_CPU)
target_include_directories(coldx_core_effect_test PRIVATE ${PROJECT_SOURCE_DIR}/shaders/examples)
target_include_directories(coldx_core_scaling_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_include_directories(coldx_core_font_test PRIVATE ${PROJECT_SOURCE_DIR}/third_party/stb)
target_compile_definitions(coldx_core_font_test PRIVATE
    COLDX_TEST_FONT="${PROJECT_SOURCE_DIR}/third_party/fonts/liberation/LiberationSans-Regular.ttf")

# The CPU renderer against every GPU driver, byte for byte, in one process.
add_executable(coldx_core_backend_diff_test backend_diff_test.cpp)
target_link_libraries(coldx_core_backend_diff_test PRIVATE coldx::core SDL3::SDL3-static)
target_include_directories(coldx_core_backend_diff_test PRIVATE ${PROJECT_SOURCE_DIR}/tests)
coldx_set_warnings(coldx_core_backend_diff_test)
list(TRANSFORM COLDX_TEST_GPU_DRIVERS PREPEND "\"" OUTPUT_VARIABLE coldx_quoted_drivers)
list(TRANSFORM coldx_quoted_drivers APPEND "\"")
list(JOIN coldx_quoted_drivers "," coldx_quoted_drivers)
target_compile_definitions(coldx_core_backend_diff_test PRIVATE COLDX_TEST_DRIVERS=${coldx_quoted_drivers}
    COLDX_TEST_FONT="${PROJECT_SOURCE_DIR}/third_party/fonts/liberation/LiberationSans-Regular.ttf")
add_test(NAME core.backend_diff COMMAND coldx_core_backend_diff_test)

# Input and Storage do not depend on the GPU backend; one run covers them.
add_executable(coldx_core_input_test input_test.cpp)
target_link_libraries(coldx_core_input_test PRIVATE coldx::core SDL3::SDL3-static)
target_include_directories(coldx_core_input_test PRIVATE ${PROJECT_SOURCE_DIR}/tests)
coldx_set_warnings(coldx_core_input_test)
add_test(NAME core.input COMMAND coldx_core_input_test)

add_executable(coldx_core_loop_test loop_test.cpp)
target_link_libraries(coldx_core_loop_test PRIVATE coldx::core SDL3::SDL3-static)
target_include_directories(coldx_core_loop_test PRIVATE ${PROJECT_SOURCE_DIR}/tests)
coldx_set_warnings(coldx_core_loop_test)
add_test(NAME core.loop COMMAND coldx_core_loop_test)

add_executable(coldx_core_text_input_test text_input_test.cpp)
target_link_libraries(coldx_core_text_input_test PRIVATE coldx::core SDL3::SDL3-static)
target_include_directories(coldx_core_text_input_test PRIVATE ${PROJECT_SOURCE_DIR}/tests)
coldx_set_warnings(coldx_core_text_input_test)
add_test(NAME core.text_input COMMAND coldx_core_text_input_test)

# Audio does not depend on the GPU backend either. Headless runs use SDL's dummy audio
# driver, so the test is silent.
add_executable(coldx_core_audio_test audio_test.cpp)
target_link_libraries(coldx_core_audio_test PRIVATE coldx::core SDL3::SDL3-static)
target_include_directories(coldx_core_audio_test PRIVATE ${PROJECT_SOURCE_DIR}/tests ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_definitions(coldx_core_audio_test PRIVATE COLDX_TEST_DATA="${CMAKE_CURRENT_SOURCE_DIR}/data")
coldx_set_warnings(coldx_core_audio_test)
add_test(NAME core.audio COMMAND coldx_core_audio_test)

# A hang (a mixer that never finishes, a loop that never exits) fails instead of blocking.
get_property(core_tests DIRECTORY PROPERTY TESTS)
set_tests_properties(${core_tests} PROPERTIES TIMEOUT 120)

add_executable(coldx_core_components_test components_test.cpp)
target_link_libraries(coldx_core_components_test PRIVATE coldx::core SDL3::SDL3-static)
target_include_directories(coldx_core_components_test PRIVATE ${PROJECT_SOURCE_DIR}/tests)
coldx_set_warnings(coldx_core_components_test)
add_test(NAME core.components COMMAND coldx_core_components_test)
set_tests_properties(core.components PROPERTIES TIMEOUT 120)

add_executable(coldx_core_content_test content_test.cpp)
target_link_libraries(coldx_core_content_test PRIVATE coldx::core SDL3::SDL3-static)
target_include_directories(coldx_core_content_test PRIVATE ${PROJECT_SOURCE_DIR}/tests ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_definitions(coldx_core_content_test PRIVATE
    COLDX_TEST_DATA="${CMAKE_CURRENT_SOURCE_DIR}/data"
    COLDX_TEST_FONT="${PROJECT_SOURCE_DIR}/third_party/fonts/liberation/LiberationSans-Regular.ttf")
coldx_set_warnings(coldx_core_content_test)
add_test(NAME core.content COMMAND coldx_core_content_test)

add_executable(coldx_core_rime_test rime_test.cpp)
target_link_libraries(coldx_core_rime_test PRIVATE coldx::core SDL3::SDL3-static)
target_include_directories(coldx_core_rime_test PRIVATE ${PROJECT_SOURCE_DIR}/tests ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_definitions(coldx_core_rime_test PRIVATE
    COLDX_TEST_DATA="${CMAKE_CURRENT_SOURCE_DIR}/data"
    COLDX_TEST_FONT="${PROJECT_SOURCE_DIR}/third_party/fonts/liberation/LiberationSans-Regular.ttf")
coldx_set_warnings(coldx_core_rime_test)
add_test(NAME core.rime COMMAND coldx_core_rime_test)
set_tests_properties(core.content PROPERTIES TIMEOUT 120)

add_executable(coldx_core_window_test window_test.cpp)
target_link_libraries(coldx_core_window_test PRIVATE coldx::core SDL3::SDL3-static)
target_include_directories(coldx_core_window_test PRIVATE ${PROJECT_SOURCE_DIR}/tests)
coldx_set_warnings(coldx_core_window_test)
add_test(NAME core.window COMMAND coldx_core_window_test)
set_tests_properties(core.window PROPERTIES TIMEOUT 120)
