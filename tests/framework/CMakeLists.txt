# The framework stress tests (see the README's Tests section). A console program, so
# results print to the terminal; the windows it opens are its test scenes.
add_executable(coldx_framework_tests
    main.cpp
    framework_test.cpp
    framework_test.h
    graphics_tests.cpp
    graphics_tests2.cpp
    timing_tests.cpp
    audio_tests.cpp
    input_platform_tests.cpp
    human_tests.cpp
    feature_tests.cpp
)
target_link_libraries(coldx_framework_tests PRIVATE coldx::core SDL3::SDL3-static)
# Some tests read the mixer directly, the way core's own tests do.
target_include_directories(coldx_framework_tests PRIVATE ${PROJECT_SOURCE_DIR}/src/core)
if(WIN32)
    target_link_libraries(coldx_framework_tests PRIVATE psapi)
endif()
coldx_set_warnings(coldx_framework_tests)

set(font_dir ${PROJECT_SOURCE_DIR}/third_party/fonts/liberation)
add_custom_command(TARGET coldx_framework_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:coldx_framework_tests>/content
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${font_dir}/LiberationSans-Regular.ttf ${font_dir}/LICENSE
        ${PROJECT_SOURCE_DIR}/samples/hello_audio/content/loop.ogg
        ${PROJECT_SOURCE_DIR}/src/core/tests/data/stereo.ogg
        $<TARGET_FILE_DIR:coldx_framework_tests>/content/)
