add_executable(coldx_content_pipeline_test pipeline_test.cpp)
target_link_libraries(coldx_content_pipeline_test PRIVATE coldx::content SDL3::SDL3-static)
target_include_directories(coldx_content_pipeline_test PRIVATE ${PROJECT_SOURCE_DIR}/tests ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_definitions(coldx_content_pipeline_test PRIVATE
    COLDX_TEST_WAV="${PROJECT_SOURCE_DIR}/samples/demo/content/pickup.wav"
    COLDX_TEST_OGG="${PROJECT_SOURCE_DIR}/src/core/tests/data/tone.ogg"
    COLDX_TEST_FONT="${PROJECT_SOURCE_DIR}/third_party/fonts/liberation/LiberationSans-Regular.ttf")
coldx_set_warnings(coldx_content_pipeline_test)
add_test(NAME content.pipeline COMMAND coldx_content_pipeline_test)

add_executable(coldx_content_watermark_test watermark_test.cpp)
target_link_libraries(coldx_content_watermark_test PRIVATE coldx::content SDL3::SDL3-static)
target_include_directories(coldx_content_watermark_test PRIVATE ${PROJECT_SOURCE_DIR}/tests ${CMAKE_CURRENT_SOURCE_DIR}/..)
coldx_set_warnings(coldx_content_watermark_test)
add_test(NAME content.watermark COMMAND coldx_content_watermark_test)
