# Makes the demo's content key once: cmake -DTOOL=coldxc -DKEY=file -P make_key.cmake.
# Every configuration's build runs it, so it leaves an existing key alone (coldxc refuses
# to overwrite one, rightly: packages made with it would become unreadable).
if(NOT EXISTS "${KEY}")
    execute_process(COMMAND "${TOOL}" --new-key "${KEY}" RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "coldxc --new-key failed")
    endif()
endif()
