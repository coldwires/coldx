add_executable(coldx_core_math_test math_test.cpp)
target_link_libraries(coldx_core_math_test PRIVATE coldx::core)
target_include_directories(coldx_core_math_test PRIVATE ${PROJECT_SOURCE_DIR}/tests)
coldx_set_warnings(coldx_core_math_test)
add_test(NAME core.math COMMAND coldx_core_math_test)

# Tests that draw run headless, once per backend in COLDX_TEST_GPU_DRIVERS.
function(coldx_core_render_test name source)
    add_executable(coldx_core_${name}_test ${source})
    target_link_libraries(coldx_core_${name}_test PRIVATE coldx::core SDL3::SDL3-static)
    target_include_directories(coldx_core_${name}_test PRIVATE ${PROJECT_SOURCE_DIR}/tests)
    coldx_set_warnings(coldx_core_${name}_test)
    foreach(driver IN LISTS COLDX_TEST_GPU_DRIVERS)
        add_test(NAME core.${name}.${driver} COMMAND coldx_core_${name}_test)
        set_tests_properties(core.${name}.${driver} PROPERTIES ENVIRONMENT "SDL_GPU_DRIVER=${driver}")
    endforeach()
endfunction()

coldx_core_render_test(render render_test.cpp)
coldx_core_render_test(sprite sprite_test.cpp)
coldx_core_render_test(font font_test.cpp)
target_include_directories(coldx_core_font_test PRIVATE ${PROJECT_SOURCE_DIR}/third_party/stb)
target_compile_definitions(coldx_core_font_test PRIVATE
    COLDX_TEST_FONT="${PROJECT_SOURCE_DIR}/third_party/fonts/liberation/LiberationSans-Regular.ttf")

# Input and Storage do not depend on the GPU backend; one run covers them.
add_executable(coldx_core_input_test input_test.cpp)
target_link_libraries(coldx_core_input_test PRIVATE coldx::core SDL3::SDL3-static)
target_include_directories(coldx_core_input_test PRIVATE ${PROJECT_SOURCE_DIR}/tests)
coldx_set_warnings(coldx_core_input_test)
add_test(NAME core.input COMMAND coldx_core_input_test)
