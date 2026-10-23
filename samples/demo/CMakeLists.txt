add_executable(demo main.cpp world.h play.h screens.h sheet.h)
target_link_libraries(demo PRIVATE coldx::core coldx::protect)
# The example effects' committed headers: compiled in, so no Vulkan SDK is needed here.
target_include_directories(demo PRIVATE ${PROJECT_SOURCE_DIR}/shaders/examples)
coldx_game(demo)
coldx_set_warnings(demo)
if(COLDX_HARDEN)
    coldx_harden(demo)
endif()

# A key for this build folder only, made once by coldxc. A real game keeps its key file
# out of its public repository and passes it the same way.
set(demo_key "${CMAKE_CURRENT_BINARY_DIR}/demo.key")
add_custom_command(OUTPUT "${demo_key}"
    COMMAND ${CMAKE_COMMAND} "-DTOOL=$<TARGET_FILE:coldxc>" "-DKEY=${demo_key}" -P "${CMAKE_CURRENT_SOURCE_DIR}/make_key.cmake"
    COMMENT "Making a content key for the demo"
    VERBATIM)

# content/content.coldx lists the content; coldxc packs it into content.rime beside
# demo.exe, which ContentManager mounts by itself. The key reaches the game through a
# source file coldx_add_content generates; coldxc --identify reads the fingerprint back.
coldx_add_content(demo
    PROJECT content/content.coldx
    KEY_FILE "${demo_key}"
    BUILD_ID "coldx-demo")

# A scripted run through every screen, nobody at the keyboard (tests/demo_run.txt).
if(COLDX_BUILD_TESTS)
    add_test(NAME samples.demo COMMAND demo --profile test)
    set_tests_properties(samples.demo PROPERTIES
        ENVIRONMENT "COLDX_HEADLESS=1;COLDX_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/demo_run.txt"
        PASS_REGULAR_EXPRESSION "demo: high score saved, Tester"
        FAIL_REGULAR_EXPRESSION "not found|failed|has no parameter|ignored"
        TIMEOUT 120)
endif()
