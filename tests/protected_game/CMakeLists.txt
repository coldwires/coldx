# An encrypted, fingerprinted package end to end: coldxc encrypts it with test.key and
# marks it with a build id, the generated source gives the game the key, the game reads
# its content, and coldxc --identify finds the fingerprint in both the game and the package.
add_executable(coldx_protected_game main.cpp)
target_link_libraries(coldx_protected_game PRIVATE coldx::core coldx::protect)
coldx_set_warnings(coldx_protected_game)
if(COLDX_HARDEN)
    coldx_harden(coldx_protected_game)
endif()
coldx_add_content(coldx_protected_game
    PROJECT content/content.coldx
    KEY_FILE test.key
    BUILD_ID "protected-test-build")

add_test(NAME protect.game COMMAND coldx_protected_game)
set_tests_properties(protect.game PROPERTIES ENVIRONMENT "COLDX_HEADLESS=1" TIMEOUT 60)
add_test(NAME protect.identify.game
    COMMAND coldxc --project ${CMAKE_CURRENT_SOURCE_DIR}/content/content.coldx --identify $<TARGET_FILE:coldx_protected_game>)
add_test(NAME protect.identify.package
    COMMAND coldxc --project ${CMAKE_CURRENT_SOURCE_DIR}/content/content.coldx
        --identify $<TARGET_FILE_DIR:coldx_protected_game>/content.rime)
set_tests_properties(protect.identify.game protect.identify.package PROPERTIES
    PASS_REGULAR_EXPRESSION "fingerprint 0x[0-9a-f]+" TIMEOUT 60)
